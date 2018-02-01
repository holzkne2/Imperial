#include <Windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

typedef uint8_t uint8;
typedef uint32_t uint32;

#define Pi32 3.14159265359f

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
};

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

static void
Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");

	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
	else
	{
		// TODO: Logging Error
	}
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

static void
Win32InitDSound(HWND Window, int32_t SamplersPerSecond, int32_t BufferSize)
{
	// Load the library
	HMODULE DSoundLibray = LoadLibraryA("dsound.dll");

	if (DSoundLibray)
	{
		// Get a DirectSound object -- cooperative
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
			GetProcAddress(DSoundLibray, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
		{
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplersPerSecond;
			WaveFormat.wBitsPerSample = 16;
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
			WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC BufferDescription = {};
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// Create a primary buffer
				LPDIRECTSOUNDBUFFER PrimaryBuffer;
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
					{
						OutputDebugStringA("Primary Buffer Format Set.\n");
					}
					else
					{
						// TODO: Logging Error
					}
				}
			}
			else
			{
				// TODO: Logging Error
			}

			DSBUFFERDESC BufferDescription = {};
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;

			// Create a secondary buffer			
			if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
			{
				OutputDebugStringA("Secondary Buffer Created.\n");
			}

			// Start playing
		}
		else
		{
			// TODO: Logging Error
		}
	}
}

static bool Running;
static win32_offscreen_buffer GlobalBackBuffer;

struct win32_window_dimension
{
	int Width;
	int Height;
};

static win32_window_dimension
GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

static void
RenderWeirdGrandent(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; ++X)
		{
			/*
			Pixel in memory: BB GG RR xx
			Little Endian Architecture 0x xxRRGGBB
			*/

			uint8 B = (uint8)(X + XOffset);
			uint8 G = (uint8)(Y + YOffset);

			*Pixel++ = ((G << 8) | B);
		}
		Row += Buffer->Pitch;
	}
}

static void
ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
	if (Buffer->Memory)
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	Buffer->BytesPerPixel = 4;

	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

static void
Win32CopyBufferToWindow(HDC DeviceContext, int WindowWidth, int WindowHeight,
win32_offscreen_buffer *Buffer,
int X, int Y, int Width, int Height)
{
	StretchDIBits(
		DeviceContext,
		/*
		X, Y, Width, Height,
		X, Y, Width, Height,
		*/
		0, 0, WindowWidth, WindowHeight,
		0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory,
		&Buffer->Info,
		DIB_RGB_COLORS,
		SRCCOPY
		);
}

LRESULT CALLBACK MainWindowCallback(
	HWND   Window,
	UINT   Message,
	WPARAM WParam,
	LPARAM LParam
	)
{
	LRESULT Result = 0;

	switch (Message)
	{
	case WM_SIZE:
	{
	} break;

	case WM_DESTROY:
	{
		// TODO: Handle as error - recreate window?
		Running = false;
	} break;

	case WM_CLOSE:
	{
		// TODO: Handle with message to user
		Running = false;
	} break;

	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	} break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		uint32_t VKCode = WParam;
		bool WasDown = (LParam & (1 << 30) != 0);
		bool IsDown = ((LParam & (1 << 31)) == 0);

		if (VKCode == 'W')
		{

		}
		else if (VKCode == 'A')
		{

		}
		else if (VKCode == 'S')
		{

		}
		else if (VKCode == 'D')
		{

		}
		else if (VKCode == 'Q')
		{

		}
		else if (VKCode == VK_UP)
		{

		}
		else if (VKCode == VK_LEFT)
		{

		}
		else if (VKCode == VK_DOWN)
		{

		}
		else if (VKCode == VK_RIGHT)
		{

		}
		else if (VKCode == VK_ESCAPE)
		{
			if (IsDown)
				Running = false;
		}
		else if (VKCode == VK_SPACE)
		{

		}

		if (VKCode == VK_F4 && (LParam & (1 << 29)))
		{
			Running = false;
		}

	} break;

	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint);
		int X = Paint.rcPaint.left;
		int Y = Paint.rcPaint.top;
		int Width = Paint.rcPaint.right - Paint.rcPaint.left;
		int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

		win32_window_dimension Dimension = GetWindowDimension(Window);

		Win32CopyBufferToWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer, X, Y, Width, Height);
		EndPaint(Window, &Paint);
	} break;

	default:
	{
		Result = DefWindowProc(Window, Message, WParam, LParam);
	} break;
	}

	return Result;
}

struct win32_sound_output
{
	int SamplesPerSecond;
	int ToneHz;
	int ToneVolume;
	uint32 RunningSampleIndex;
	int WavePeriod;
	int BytesPerSample;
	int SecondaryBufferSize;
};

void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
	VOID *Region1;
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;


	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(
		ByteToLock,
		BytesToWrite,
		&Region1, &Region1Size,
		&Region2, &Region2Size,
		0)))
	{

		int16_t *SampleOut = (int16_t *)Region1;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			float t = 2.0f * Pi32 * (float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
			float SineValue = sinf(t);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}
		SampleOut = (int16_t *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			float t = 2.0f * Pi32 * (float)SoundOutput->RunningSampleIndex / (float)SoundOutput->WavePeriod;
			float SineValue = sinf(t);
			int16_t SampleValue = (int16_t)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

int CALLBACK
WinMain(HINSTANCE Instance,
HINSTANCE PrevInstance,
LPSTR CommandLine,
int ShowCode)
{
	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = MainWindowCallback;
	WindowClass.hInstance = Instance;
//	WindowClass.hIcon;
	WindowClass.lpszClassName = "WildWestWindowClass";

	if (RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(
			0,
			WindowClass.lpszClassName,
			"Wild West",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0
			);
		if (Window)
		{
			// Graphics Test
			int XOffset = 0;
			int YOffset = 0;

			// Sound Test
			win32_sound_output SoundOutput = {};
			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.ToneHz = 256;
			SoundOutput.ToneVolume = 16000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
			SoundOutput.BytesPerSample = sizeof(int16_t) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			Running = true;
			while (Running)
			{
				MSG Message;
				while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
				{
					if (Message.message == WM_QUIT)
						Running = false;

					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				}

				for (DWORD ControllerIndex = 0;
					ControllerIndex < XUSER_MAX_COUNT;
					++ControllerIndex)
				{
					XINPUT_STATE ControllerState;
					if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
					{
						XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

						bool Up = Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
						bool Down = Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
						bool Left = Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
						bool Right = Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
						bool Start = Pad->wButtons & XINPUT_GAMEPAD_START;
						bool Back = Pad->wButtons & XINPUT_GAMEPAD_BACK;
						bool LeftShoulder = Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
						bool RightShoulder = Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
						bool AButton = Pad->wButtons & XINPUT_GAMEPAD_A;
						bool BButton = Pad->wButtons & XINPUT_GAMEPAD_B;
						bool XButton = Pad->wButtons & XINPUT_GAMEPAD_X;
						bool YButton = Pad->wButtons & XINPUT_GAMEPAD_Y;

						int16_t StickX = Pad->sThumbLX;
						int16_t StickY = Pad->sThumbLY;
					}
					else
					{
						// Controller not available
					}
				}

				RenderWeirdGrandent(&GlobalBackBuffer, XOffset, YOffset);

				DWORD PlayCursor;
				DWORD WriteCursor;
				if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(
					&PlayCursor, &WriteCursor)))
				{
					DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
					DWORD BytesToWrite;
					if (ByteToLock > PlayCursor)
					{
						BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
						BytesToWrite += PlayCursor;
					}
					else
					{
						BytesToWrite = PlayCursor - ByteToLock;
					}

					Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
				}

				HDC DeviceContext = GetDC(Window);
				win32_window_dimension Dimension = GetWindowDimension(Window);
				Win32CopyBufferToWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer, 0, 0, Dimension.Width, Dimension.Height);
				ReleaseDC(Window, DeviceContext);

				XOffset++;
				YOffset++;
			}
		}
		else
		{
			// TODO: Logging Error
		}
	}
	else
	{
		// TODO: Logging Error
	}

	return 0;
}