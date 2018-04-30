#pragma once

#include "main.h"

#include <Windows.h>

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