#if _WIN64 || _WIN32

#include "win32_main.h"

#include <malloc.h>
#include <Xinput.h>
#include <dsound.h>

//
// Defines
//

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	UNREFERENCE(dwUserIndex);
	UNREFERENCE(pState);
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	UNREFERENCE(dwUserIndex);
	UNREFERENCE(pVibration);
	return ERROR_DEVICE_NOT_CONNECTED;
}
static x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// TODO: function???
#define PROCESS_XINPUT_BUTTON(controller, game_button, xinput_button) \
controller.##game_button.half_transition_count = (controller.##game_button.ended_down == xinput_button) ? \
0 : 1; \
controller.##game_button.ended_down = xinput_button;

static void win32_load_xinput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary) {
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
		UNREFERENCE(XInputLibrary);
	}

	IASSERT_RETURN(XInputLibrary, "Failed to create Win32 XInput");
	XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
	XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, \
LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

//
// Structs
//

struct win32_offscreen_buffer
{
	BITMAPINFO		info;
	void *			memory;
	int32			width;
	int32			height;
	int32			pitch;
	int32			bytes_per_pixel;
};

struct win32_window_dimension
{
	int32			width;
	int32			height;
};

struct win32_sound_output
{
	int32			samples_per_second;
	uint32			running_sample_index;
	int32			bytes_per_sample;
	int32			secondary_buffer_size;
	int32			latency_sample_count;
};

//
// Globals
//

static LPDIRECTSOUNDBUFFER Global_secondary_buffer;

static bool Running;
static win32_offscreen_buffer Global_back_buffer;

static void win32_init_dsound(HWND window, int32 samplers_per_second, int32 BufferSize)
{
	// Load the library
	HMODULE dsound_libray = LoadLibraryA("dsound.dll");

	if (dsound_libray)
	{
		// Get a DirectSound object -- cooperative
		direct_sound_create *direct_sound_create_p = (direct_sound_create *)
			GetProcAddress(dsound_libray, "DirectSoundCreate");
		LPDIRECTSOUND direct_sound;
		IASSERT_RETURN(direct_sound_create_p, "Failed to create Sound Buffer");
		IASSERT_RETURN(SUCCEEDED(direct_sound_create_p(0, &direct_sound, 0)),
			"Failed to create Sound Buffer");
		WAVEFORMATEX wave_format = {};
		wave_format.wFormatTag = WAVE_FORMAT_PCM;
		wave_format.nChannels = 2;
		wave_format.nSamplesPerSec = samplers_per_second;
		wave_format.wBitsPerSample = 16;
		wave_format.nBlockAlign = (wave_format.nChannels * wave_format.wBitsPerSample) / 8;
		wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec * wave_format.nBlockAlign;
		wave_format.cbSize = 0;

		IASSERT_RETURN(SUCCEEDED(direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY)),
			"Failed to Set Cooperative Level");
			
		DSBUFFERDESC buffer_description = {};
		buffer_description.dwSize = sizeof(buffer_description);
		buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;

		// Create a primary buffer
		LPDIRECTSOUNDBUFFER PrimaryBuffer;
		if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &PrimaryBuffer, 0)))
		{
			IASSERT_RETURN(SUCCEEDED(PrimaryBuffer->SetFormat(&wave_format)),
				"Failed to Set Format for Primary Sound Buffer");
			ILOG("Primary Buffer Format Set.");
		}			

		buffer_description = {};
		buffer_description.dwSize = sizeof(buffer_description);
		buffer_description.dwFlags = 0;
		buffer_description.dwBufferBytes = BufferSize;
		buffer_description.lpwfxFormat = &wave_format;

		// Create a secondary buffer			
		if (SUCCEEDED(direct_sound->CreateSoundBuffer(&buffer_description, &Global_secondary_buffer, 0)))
		{
			ILOG("Secondary Buffer Created.");
		}

		// Start playing
	}
}

static win32_window_dimension win32_get_window_dimension(HWND window)
{
	win32_window_dimension result;

	RECT client_rect;
	GetClientRect(window, &client_rect);
	result.width = client_rect.right - client_rect.left;
	result.height = client_rect.bottom - client_rect.top;

	return result;
}

static void ResizeDIBSection(win32_offscreen_buffer *buffer_p, int32 width, int32 height)
{
	if (buffer_p->memory)
	{
		VirtualFree(buffer_p->memory, 0, MEM_RELEASE);
	}

	buffer_p->width = width;
	buffer_p->height = height;

	buffer_p->info.bmiHeader.biSize = sizeof(buffer_p->info.bmiHeader);
	buffer_p->info.bmiHeader.biWidth = buffer_p->width;
	buffer_p->info.bmiHeader.biHeight = -buffer_p->height;
	buffer_p->info.bmiHeader.biPlanes = 1;
	buffer_p->info.bmiHeader.biBitCount = 32;
	buffer_p->info.bmiHeader.biCompression = BI_RGB;

	buffer_p->bytes_per_pixel = 4;

	int32 bitmap_memory_size = (buffer_p->width * buffer_p->height) * buffer_p->bytes_per_pixel;
	buffer_p->memory = VirtualAlloc(0, bitmap_memory_size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	IASSERT_RETURN(buffer_p != nullptr, "Failed to allocate memory of size %llu", bitmap_memory_size);

	buffer_p->pitch = buffer_p->width * buffer_p->bytes_per_pixel;
}

static void win32_copy_buffer_to_window(HDC device_context, int32 window_width, int32 window_height,
	win32_offscreen_buffer *buffer_p)
{
	StretchDIBits(
		device_context,
		/*
		X, Y, Width, Height,
		X, Y, Width, Height,
		*/
		0, 0, window_width, window_height,
		0, 0, buffer_p->width, buffer_p->height,
		buffer_p->memory,
		&buffer_p->info,
		DIB_RGB_COLORS,
		SRCCOPY
		);
}

LRESULT CALLBACK MainWindowCallback(
	HWND   window,
	UINT   message,
	WPARAM wparam,
	LPARAM lparam
	)
{
	LRESULT result = 0;

	switch (message)
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
		ILOG("WM_ACTIVATEAPP");
	} break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		uint32 vk_code = (uint32)wparam;
		bool was_down = ((lparam & (1 << 30)) != 0);
		UNREFERENCE(was_down);
		bool IsDown = ((lparam & (1 << 31)) == 0);

		if (vk_code == 'W')
		{

		}
		else if (vk_code == 'A')
		{

		}
		else if (vk_code == 'S')
		{

		}
		else if (vk_code == 'D')
		{

		}
		else if (vk_code == 'Q')
		{

		}
		else if (vk_code == VK_UP)
		{

		}
		else if (vk_code == VK_LEFT)
		{

		}
		else if (vk_code == VK_DOWN)
		{

		}
		else if (vk_code == VK_RIGHT)
		{

		}
		else if (vk_code == VK_ESCAPE)
		{
			if (IsDown)
				Running = false;
		}
		else if (vk_code == VK_SPACE)
		{

		}

		if (vk_code == VK_F4 && (lparam & (1 << 29)))
		{
			Running = false;
		}

	} break;

	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		HDC device_context = BeginPaint(window, &paint);

		win32_window_dimension dimension = win32_get_window_dimension(window);

		win32_copy_buffer_to_window(device_context,
			dimension.width, dimension.height,
			&Global_back_buffer);
		EndPaint(window, &paint);
	} break;

	default:
	{
		result = DefWindowProc(window, message, wparam, lparam);
	} break;
	}

	return result;
}

static void win32_clear_sound_buffer(win32_sound_output *sound_output_p)
{
	VOID *region_1_p;
	DWORD region_1_size;
	VOID *region_2_p;
	DWORD region_2_size;

	if (SUCCEEDED(Global_secondary_buffer->Lock(
		0,
		sound_output_p->secondary_buffer_size,
		&region_1_p, &region_1_size,
		&region_2_p, &region_2_size,
		0)))
	{
		uint8 *dest_sample_p = (uint8 *)region_1_p;
		for (DWORD byte_index = 0; byte_index < region_1_size; ++byte_index)
		{
			*dest_sample_p++ = 0;
		}
		dest_sample_p = (uint8 *)region_2_p;
		for (DWORD ByteIndex = 0; ByteIndex < region_2_size; ++ByteIndex)
		{
			*dest_sample_p++ = 0;
		}

		Global_secondary_buffer->Unlock(region_1_p, region_1_size, region_2_p, region_2_size);
	}
}

static void win32_fill_sound_buffer(win32_sound_output *sound_output_p,
	DWORD byte_to_lock, DWORD bytes_to_write,
	game_sound_output_buffer *source_buffer_p)
{
	VOID *region_1_p;
	DWORD region_1_size;
	VOID *region_2_p;
	DWORD region_2_size;

	if (SUCCEEDED(Global_secondary_buffer->Lock(
		byte_to_lock,
		bytes_to_write,
		&region_1_p, &region_1_size,
		&region_2_p, &region_2_size,
		0)))
	{

		DWORD region_1_sample_count = region_1_size / sound_output_p->bytes_per_sample;
		int16 *dest_sample_p = (int16 *)region_1_p;
		int16 *source_sample_p = source_buffer_p->samples;
		for (DWORD sample_index = 0; sample_index < region_1_sample_count; ++sample_index)
		{
			*dest_sample_p++ = *source_sample_p++;
			*dest_sample_p++ = *source_sample_p++;
			++sound_output_p->running_sample_index;
		}
		dest_sample_p = (int16 *)region_2_p;
		DWORD region_2_sample_count = region_2_size / sound_output_p->bytes_per_sample;
		for (DWORD sample_index = 0; sample_index < region_2_sample_count; ++sample_index)
		{
			*dest_sample_p++ = *source_sample_p++;
			*dest_sample_p++ = *source_sample_p++;
			++sound_output_p->running_sample_index;
		}

		Global_secondary_buffer->Unlock(region_1_p, region_1_size, region_2_p, region_2_size);
	}
}

int32 CALLBACK WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR command_line, int32 show_code)
{
	UNREFERENCE(prev_instance);
	UNREFERENCE(command_line);
	UNREFERENCE(show_code);

	LARGE_INTEGER perf_count_frequency;
	QueryPerformanceFrequency(&perf_count_frequency);

	win32_load_xinput();

	WNDCLASSA window_class = {};

	ResizeDIBSection(&Global_back_buffer, 1280, 720);

	window_class.style = CS_HREDRAW | CS_VREDRAW;
	window_class.lpfnWndProc = MainWindowCallback;
	window_class.hInstance = instance;
//	WindowClass.hIcon;
	window_class.lpszClassName = "ImperialWindowClass";

	IASSERT_RETURN_VALUE(RegisterClass(&window_class), 1, "Failed to register class");
	HWND window = CreateWindowEx(
		0,
		window_class.lpszClassName,
		"Imperial",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		0,
		0,
		instance,
		0
		);
	IASSERT_RETURN_VALUE(window, 1, "Failed to create window");

	// Sound Test
	win32_sound_output sound_output = {};
	sound_output.samples_per_second = 48000;
	sound_output.running_sample_index = 0;
	sound_output.bytes_per_sample = sizeof(int16) * 2;
	sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
	sound_output.latency_sample_count = sound_output.samples_per_second / 15;

	win32_init_dsound(window, sound_output.samples_per_second, sound_output.secondary_buffer_size);
	win32_clear_sound_buffer(&sound_output);
	Global_secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

	int16 *samples = (int16 *)VirtualAlloc(0,sound_output.secondary_buffer_size,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	IASSERT_RETURN_VALUE(samples != nullptr, 1,
		"Failed to allocate memory of size %llu", sound_output.secondary_buffer_size);

	game_memory game_memory_o = {};
	game_memory_o.permanent_storage_size = 64 MB;
	game_memory_o.transient_storage_size = ((uint64)2) GB;
#if DEBUG
	LPVOID base_address = (LPVOID)(((uint64)2) TB);
#else // #if DEBUG
	LPVOID base_address = 0;
#endif // #if DEBUG
	game_memory_o.permanent_storage_p = VirtualAlloc(base_address,
		game_memory_o.permanent_storage_size + game_memory_o.transient_storage_size,
		MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	IASSERT_RETURN_VALUE(game_memory_o.permanent_storage_p != nullptr, 1,
		"Failed to allocate memory of size %llu", game_memory_o.permanent_storage_size);
	game_memory_o.transient_storage_p = ((uint8 *)game_memory_o.permanent_storage_p +
		game_memory_o.permanent_storage_size);

	Running = true;

	LARGE_INTEGER last_counter;
	QueryPerformanceCounter(&last_counter);
	int64 last_cycle_count = __rdtsc();
	while (Running)
	{
		game_input input = {};

		MSG message;
		while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				Running = false;

			TranslateMessage(&message);
			DispatchMessageA(&message);
		}

		int max_controller_count = XUSER_MAX_COUNT;
		if (max_controller_count > ARRAY_COUNT(input.controllers)) {
			IASSERT_ONCE(false, "Too many controllers plugged in.");
			max_controller_count = ARRAY_COUNT(input.controllers);
		}
		for (DWORD controller_index = 0;
			controller_index < XUSER_MAX_COUNT;
			++controller_index)
		{
			XINPUT_STATE controller_state;
			if (XInputGetState(controller_index, &controller_state) == ERROR_SUCCESS)
			{
				XINPUT_GAMEPAD *pad_p = &controller_state.Gamepad;
				game_controller_input& controller_input = input.controllers[controller_index];

				const bool up				= (pad_p->wButtons & XINPUT_GAMEPAD_DPAD_UP) > 0;
				const bool down				= (pad_p->wButtons & XINPUT_GAMEPAD_DPAD_DOWN) > 0;
				const bool left				= (pad_p->wButtons & XINPUT_GAMEPAD_DPAD_LEFT) > 0;
				const bool right			= (pad_p->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) > 0;
				const bool start			= (pad_p->wButtons & XINPUT_GAMEPAD_START) > 0;
				const bool back				= (pad_p->wButtons & XINPUT_GAMEPAD_BACK) > 0;
				const bool left_shoulder	= (pad_p->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) > 0;
				const bool right_shoulder	= (pad_p->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) > 0;
				const bool a_button			= (pad_p->wButtons & XINPUT_GAMEPAD_A) > 0;
				const bool b_button			= (pad_p->wButtons & XINPUT_GAMEPAD_B) > 0;
				const bool x_button			= (pad_p->wButtons & XINPUT_GAMEPAD_X) > 0;
				const bool y_button			= (pad_p->wButtons & XINPUT_GAMEPAD_Y) > 0;

				PROCESS_XINPUT_BUTTON(controller_input, a, a_button);
				PROCESS_XINPUT_BUTTON(controller_input, b, b_button);
				PROCESS_XINPUT_BUTTON(controller_input, x, x_button);
				PROCESS_XINPUT_BUTTON(controller_input, y, y_button);

				PROCESS_XINPUT_BUTTON(controller_input, up, up);
				PROCESS_XINPUT_BUTTON(controller_input, down, down);
				PROCESS_XINPUT_BUTTON(controller_input, left, left);
				PROCESS_XINPUT_BUTTON(controller_input, right, right);

				PROCESS_XINPUT_BUTTON(controller_input, start, start);
				PROCESS_XINPUT_BUTTON(controller_input, back, back);
				
				PROCESS_XINPUT_BUTTON(controller_input, left_shoulder, left_shoulder);
				PROCESS_XINPUT_BUTTON(controller_input, right_shoulder, right_shoulder);

				const int16 xinput_stick_left_x = pad_p->sThumbLX;
				const int16 xinput_stick_left_y = pad_p->sThumbLY;
				real32 stick_left_x = remap_n11(SHRT_MIN, SHRT_MAX, xinput_stick_left_x);
				real32 stick_left_y = remap_n11(SHRT_MIN, SHRT_MAX, xinput_stick_left_y);
				controller_input.left_stick.start_x = controller_input.left_stick.end_x;
				controller_input.left_stick.start_y = controller_input.left_stick.end_y;

				// Not Correct
				controller_input.left_stick.min_x = controller_input.left_stick.max_x
					= controller_input.left_stick.end_x = stick_left_x;
				controller_input.left_stick.min_y = controller_input.left_stick.max_y
					= controller_input.left_stick.end_y = stick_left_y;

				const int16 xinput_stick_right_x = pad_p->sThumbRX;
				const int16 xinput_stick_right_y = pad_p->sThumbRY;
				real32 stick_right_x = remap_n11(SHRT_MIN, SHRT_MAX, xinput_stick_right_x);
				real32 stick_right_y = remap_n11(SHRT_MIN, SHRT_MAX, xinput_stick_right_y);
				controller_input.right_stick.start_x = controller_input.right_stick.end_x;
				controller_input.right_stick.start_y = controller_input.right_stick.end_y;

				// Not Correct
				controller_input.right_stick.min_x = controller_input.right_stick.max_x
					= controller_input.right_stick.end_x = stick_right_x;
				controller_input.right_stick.min_y = controller_input.right_stick.max_y
					= controller_input.right_stick.end_y = stick_right_y;

				controller_input.is_analog = true;

				const uint8 xinput_right_trigger = pad_p->bRightTrigger;
				const uint8 xinput_left_trigger = pad_p->bLeftTrigger;
				UNREFERENCE(xinput_right_trigger);
				UNREFERENCE(xinput_left_trigger);
			}
			else
			{
				// Controller not available
			}
		}

		DWORD byte_to_lock;
		DWORD target_cursor;
		DWORD bytes_to_write;
		DWORD play_cursor;
		DWORD write_cursor;
		bool sound_is_valid = false;
		IASSERT_RETURN_VALUE(SUCCEEDED(Global_secondary_buffer->GetCurrentPosition(
			&play_cursor, &write_cursor)), 1, "Failed to get position of sound cursors.");
		byte_to_lock = (sound_output.running_sample_index * sound_output.bytes_per_sample)
			% sound_output.secondary_buffer_size;
		target_cursor = ((play_cursor +
			(sound_output.latency_sample_count * sound_output.bytes_per_sample)) %
			sound_output.secondary_buffer_size);
		if (byte_to_lock > target_cursor)
		{
			bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock;
			bytes_to_write += target_cursor;
		}
		else
		{
			bytes_to_write = target_cursor - byte_to_lock;
		}

		sound_is_valid = true;

		game_sound_output_buffer sound_buffer = {};
		sound_buffer.samples_per_second = sound_output.samples_per_second;
		sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
		sound_buffer.samples = samples;

		game_offscreen_buffer buffer = {};
		buffer.memory = Global_back_buffer.memory;
		buffer.width = Global_back_buffer.width;
		buffer.height = Global_back_buffer.height;
		buffer.pitch = Global_back_buffer.pitch;
		game_update_and_render(game_memory_o, input, buffer, sound_buffer);

		if (sound_is_valid)
		{
			win32_fill_sound_buffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
		}

		HDC device_context = GetDC(window);
		win32_window_dimension dimension = win32_get_window_dimension(window);
		win32_copy_buffer_to_window(device_context,
			dimension.width, dimension.height,
			&Global_back_buffer);
		ReleaseDC(window, device_context);

		// Performace Check
		int64 end_cycle_count = __rdtsc();

		LARGE_INTEGER end_counter;
		QueryPerformanceCounter(&end_counter);

		int64 cycle_elapsed = end_cycle_count - last_cycle_count;
		int64 counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
		real32 ms_per_frame = (1000.0f * (real32)counter_elapsed) / (real32)perf_count_frequency.QuadPart;
		real32 fps = (real32)perf_count_frequency.QuadPart / (real32)counter_elapsed;
		real32 mcpf = (real32)cycle_elapsed / (1000.0f * 1000.0f);

#if !defined(DEBUG)
		UNREFERENCE(ms_per_frame);
		UNREFERENCE(fps);
		UNREFERENCE(mcpf);
#endif

		if (fps < 60.0f) {
			ILOG("%.02fms, %.02fFPS, %.02fmc/f", ms_per_frame, fps, mcpf);
		}

		last_counter = end_counter;
		last_cycle_count = end_cycle_count;
	}

	return 0;
}

#if defined(DEBUG)
read_file_DEBUG platform_read_entire_file_DEBUG(const char * const file_name)
{
	read_file_DEBUG file = {};

	HANDLE file_handle = CreateFileA(file_name, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, 0);
	IASSERT_RETURN_VALUE(file_handle != INVALID_HANDLE_VALUE, file, "Failed to get file '%s'", file_name);

	LARGE_INTEGER file_size;
	BOOL results = GetFileSizeEx(file_handle, &file_size);
	if (!results) {
		CloseHandle(file_handle);
		IASSERT_RETURN_VALUE(false, file, "Failed to get file size of file '%s'", file_name);
	}

	IASSERT(file_size.QuadPart <= UINT32_MAX, "File '%s' is too large. Size: %lli Max: %u",
		file_name, file_size, UINT32_MAX);
	file.size = (uint32)file_size.QuadPart;

	file.memory = VirtualAlloc(0, file.size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (file.memory == nullptr) {
		CloseHandle(file_handle);
		file.size = 0;
		IASSERT_RETURN_VALUE(false, file, "Failed to allocate memory of size %ll");
	}

	DWORD bytes_read;
	results = ReadFile(file_handle, file.memory, file.size, &bytes_read, nullptr);
	if (!results || bytes_read != file.size) {
		CloseHandle(file_handle);
		platform_free_file_memory_DEBUG(file);
		IASSERT_RETURN_VALUE(false, file, "Failed to read file '%s'", file_name);
	}

	CloseHandle(file_handle);
	return file;
}

void platform_free_file_memory_DEBUG(read_file_DEBUG & file)
{
	IASSERT_RETURN(file.memory != nullptr, "");

	VirtualFree(file.memory, 0, MEM_RELEASE);

	file.memory = nullptr;
	file.size = 0;
}

bool platform_write_entire_file_DEBUG(const char * const file_name,
	const uint32 memory_size, void const * const file_memory_p)
{
	HANDLE file_handle = CreateFileA(file_name, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, 0);
	IASSERT_RETURN_VALUE(file_handle != INVALID_HANDLE_VALUE, false, "Failed to get file '%s'", file_name);

	DWORD bytes_written;
	BOOL results = WriteFile(file_handle, file_memory_p, memory_size, &bytes_written, nullptr);
	if (!results || bytes_written != memory_size) {
		CloseHandle(file_handle);
		IASSERT_RETURN_VALUE(false, false, "Failed to write to file '%s'", file_name);
	}

	CloseHandle(file_handle);
	return true;
}
#endif //#if defined(DEBUG)

#endif // #if _WIN64 || _WIN32