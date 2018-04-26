#if _WIN64 || _WIN32

#include "main.cpp"

#include <Windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <math.h>

#define Pi32 3.14159265359f

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *Memory;
	int32 Width;
	int32 Height;
	int32 Pitch;
	int32 BytesPerPixel;
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

static void Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");

	IASSERT_RETURN(XInputLibrary, "Failed to create Win32 XInput");
	XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
	XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, \
LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

static LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

static void Win32InitDSound(HWND Window, int32 SamplersPerSecond, int32 BufferSize)
{
	// Load the library
	HMODULE DSoundLibray = LoadLibraryA("dsound.dll");

	if (DSoundLibray)
	{
		// Get a DirectSound object -- cooperative
		direct_sound_create *DirectSoundCreate = (direct_sound_create *)
			GetProcAddress(DSoundLibray, "DirectSoundCreate");
		LPDIRECTSOUND DirectSound;
		IASSERT_RETURN(DirectSoundCreate, "Failed to create Sound Buffer");
		IASSERT_RETURN(SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)),
			"Failed to create Sound Buffer");
		WAVEFORMATEX WaveFormat = {};
		WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
		WaveFormat.nChannels = 2;
		WaveFormat.nSamplesPerSec = SamplersPerSecond;
		WaveFormat.wBitsPerSample = 16;
		WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
		WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
		WaveFormat.cbSize = 0;

		IASSERT_RETURN(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)),
			"Failed to Set Cooperative Level");
			
		DSBUFFERDESC BufferDescription = {};
		BufferDescription.dwSize = sizeof(BufferDescription);
		BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

		// Create a primary buffer
		LPDIRECTSOUNDBUFFER PrimaryBuffer;
		if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
		{
			IASSERT_RETURN(SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)),
				"Failed to Set Format for Primary Sound Buffer");
			OutputDebugStringA("Primary Buffer Format Set.\n");
		}			

		BufferDescription = {};
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
}

static bool Running;
static win32_offscreen_buffer GlobalBackBuffer;

struct win32_window_dimension
{
	int32 Width;
	int32 Height;
};

static win32_window_dimension GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return Result;
}

static void ResizeDIBSection(win32_offscreen_buffer *Buffer, int32 Width, int32 Height)
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

	int32 BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

static void Win32CopyBufferToWindow(HDC DeviceContext, int32 WindowWidth, int32 WindowHeight,
	win32_offscreen_buffer *Buffer,
	int32 X, int32 Y, int32 Width, int32 Height)
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
		uint32 VKCode = (uint32)WParam;
		bool WasDown = ((LParam & (1 << 30)) != 0);
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
		int32 X = Paint.rcPaint.left;
		int32 Y = Paint.rcPaint.top;
		int32 Width = Paint.rcPaint.right - Paint.rcPaint.left;
		int32 Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

		win32_window_dimension Dimension = GetWindowDimension(Window);

		Win32CopyBufferToWindow(DeviceContext,
			Dimension.Width, Dimension.Height,
			&GlobalBackBuffer,
			X, Y,
			Width, Height);
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
	int32 SamplesPerSecond;
	int32 ToneHz;
	int32 ToneVolume;
	uint32 RunningSampleIndex;
	int32 WavePeriod;
	int32 BytesPerSample;
	int32 SecondaryBufferSize;
};

void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
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

		int16 *SampleOut = (int16 *)Region1;
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
		{
			real32 t = 2.0f * Pi32 * (real32)SoundOutput->RunningSampleIndex
				/ (real32)SoundOutput->WavePeriod;
			real32 SineValue = sinf(t);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}
		SampleOut = (int16 *)Region2;
		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
		{
			real32 t = 2.0f * Pi32 * (real32)SoundOutput->RunningSampleIndex
				/ (real32)SoundOutput->WavePeriod;
			real32 SineValue = sinf(t);
			int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
			*SampleOut++ = SampleValue;
			*SampleOut++ = SampleValue;
			++SoundOutput->RunningSampleIndex;
		}

		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

int32 CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int32 ShowCode)
{
	LARGE_INTEGER PerfCountFrequency;
	QueryPerformanceFrequency(&PerfCountFrequency);

	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = MainWindowCallback;
	WindowClass.hInstance = Instance;
//	WindowClass.hIcon;
	WindowClass.lpszClassName = "ImperialWindowClass";

	IASSERT_RETURN_VALUE(RegisterClass(&WindowClass), 1, "Failed to register class");
	HWND Window = CreateWindowEx(
		0,
		WindowClass.lpszClassName,
		"Imperial",
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
	IASSERT_RETURN_VALUE(Window, 1, "Failed to create window");

	// Graphics Test
	int32 XOffset = 0;
	int32 YOffset = 0;

	// Sound Test
	win32_sound_output SoundOutput = {};
	SoundOutput.SamplesPerSecond = 48000;
	SoundOutput.ToneHz = 256;
	SoundOutput.ToneVolume = 16000;
	SoundOutput.RunningSampleIndex = 0;
	SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
	SoundOutput.BytesPerSample = sizeof(int16) * 2;
	SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;

	Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
	Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.SecondaryBufferSize);
	GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

	Running = true;

	LARGE_INTEGER LastCounter;
	QueryPerformanceCounter(&LastCounter);
	int64 LastCycleCount = __rdtsc();
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

				bool Up					= (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP) > 0;
				bool Down				= (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) > 0;
				bool Left				= (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) > 0;
				bool Right				= (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) > 0;
				bool Start				= (Pad->wButtons & XINPUT_GAMEPAD_START) > 0;
				bool Back				= (Pad->wButtons & XINPUT_GAMEPAD_BACK) > 0;
				bool LeftShoulder		= (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) > 0;
				bool RightShoulder		= (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) > 0;
				bool AButton			= (Pad->wButtons & XINPUT_GAMEPAD_A) > 0;
				bool BButton			= (Pad->wButtons & XINPUT_GAMEPAD_B) > 0;
				bool XButton			= (Pad->wButtons & XINPUT_GAMEPAD_X) > 0;
				bool YButton			= (Pad->wButtons & XINPUT_GAMEPAD_Y) > 0;

				int16 StickLX = Pad->sThumbLX;
				int16 StickLY = Pad->sThumbLY;

				int16 StickRX = Pad->sThumbRX;
				int16 StickRY = Pad->sThumbRY;

				uint8 RightTrigger = Pad->bRightTrigger;
				uint8 LeftTrigger = Pad->bLeftTrigger;
			}
			else
			{
				// Controller not available
			}
		}

		game_offscreen_buffer Buffer = {};
		Buffer.Memory = GlobalBackBuffer.Memory;
		Buffer.Width = GlobalBackBuffer.Width;
		Buffer.Height = GlobalBackBuffer.Height;
		Buffer.Pitch = GlobalBackBuffer.Pitch;
		GameUpdateAndRender(&Buffer);

		DWORD PlayCursor;
		DWORD WriteCursor;
		if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(
			&PlayCursor, &WriteCursor)))
		{
			DWORD ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample)
				% SoundOutput.SecondaryBufferSize;
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
		Win32CopyBufferToWindow(DeviceContext,
			Dimension.Width, Dimension.Height,
			&GlobalBackBuffer,
			0, 0,
			Dimension.Width, Dimension.Height);
		ReleaseDC(Window, DeviceContext);

		XOffset++;
		YOffset++;

		int64 EndCycleCount = __rdtsc();

		LARGE_INTEGER EndCounter;
		QueryPerformanceCounter(&EndCounter);

		int64 CycleElapsed = EndCycleCount - LastCycleCount;
		int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
		real32 MSPerFrame = (1000.0f * (real32)CounterElapsed) / (real32)PerfCountFrequency.QuadPart;
		real32 FPS = (real32)PerfCountFrequency.QuadPart / (real32)CounterElapsed;
		real32 MCPF = (real32)CycleElapsed / (1000.0f * 1000.0f);

		ILOG("%.02fms, %.02fFPS, %.02fmc/f", MSPerFrame, FPS, MCPF);

		LastCounter = EndCounter;
		LastCycleCount = EndCycleCount;
	}

	return 0;
}

#endif // #if _WIN64 || _WIN32