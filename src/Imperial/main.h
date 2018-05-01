#pragma once

#include "Maths.h"
#include "Debug.h"

#define UNREFERENCE(x) x

#define ARRAY_COUNT(_array) (sizeof(_array) / sizeof((_array)[0]))

#define KB * 1024
#define MB KB KB
#define GB MB KB
#define TB GB KB

//
// Services that the game provides to the platform
//

struct game_offscreen_buffer
{
	void *		memory;
	int32		width;
	int32		height;
	int32		pitch;
};

struct game_sound_output_buffer
{
	int32		samples_per_second;
	int32		sample_count;
	int16 *		samples;
};

struct game_button_state
{
	int32 half_transition_count;
	bool ended_down;
};

struct game_analog_stick
{
	real32 start_x;
	real32 start_y;
	real32 min_x;
	real32 min_y;
	real32 max_x;
	real32 max_y;
	real32 end_x;
	real32 end_y;
};

struct game_controller_input
{
	bool is_analog;

	game_analog_stick left_stick;
	game_analog_stick right_stick;

	game_button_state up;
	game_button_state down;
	game_button_state left;
	game_button_state right;
	game_button_state left_shoulder;
	game_button_state right_shoulder;
	game_button_state a;
	game_button_state b;
	game_button_state x;
	game_button_state y;
	game_button_state start;
	game_button_state back;
};

struct game_input
{
	game_controller_input controllers[4];
};

struct game_memory
{
	bool is_initialized;
	uint64 permanent_storage_size;
	void * permanent_storage_p;
};

struct game_clocks
{
	real32 delta_time;
};

void game_update_and_render(game_memory * const memory_p, game_input const * const input_p,
	game_offscreen_buffer * const buffer_p, game_sound_output_buffer * const sound_buffer_p);

struct game_state
{
	int32 tone_hz;
	int32 x_offset;
	int32 y_offset;
};

//
// Services that the platform provides to the game
//