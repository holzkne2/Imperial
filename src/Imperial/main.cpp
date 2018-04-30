#include "main.h"

#include <math.h>

static void game_output_sound(game_sound_output_buffer *sound_buffer_p, int32 tone_hz)
{
	static real32 t_sine = 0;
	const static int16 tone_volume = 16000;
	int32 wave_period = sound_buffer_p->samples_per_second / tone_hz;
	
	int16 *sample_out = sound_buffer_p->samples;
	for (int32 sample_index = 0; sample_index < sound_buffer_p->sample_count; ++sample_index)
	{
		real32 sine_value = sinf(t_sine);
		int16 sample_value = (int16)(sine_value * tone_volume);
		*sample_out++ = sample_value;
		*sample_out++ = sample_value;

		t_sine += 2.0f * PI32 * 1.0f / (real32)wave_period;
	}
}

static void render_weird_grandent(game_offscreen_buffer *buffer_p, int32 x_offset, int32 y_offset)
{
	uint8 *row = (uint8 *)buffer_p->memory;
	for (int32 y = 0; y < buffer_p->height; ++y)
	{
		uint32 *pixel = (uint32 *)row;
		for (int32 x = 0; x < buffer_p->width; ++x)
		{
			/*
			Pixel in memory: BB GG RR xx
			Little Endian Architecture 0x xxRRGGBB
			*/

			uint8 b = (uint8)(x + x_offset);
			uint8 g = (uint8)(y + y_offset);

			*pixel++ = ((g << 8) | b);
		}
		row += buffer_p->pitch;
	}
}

void game_update_and_render(game_input *input_p,
	game_offscreen_buffer *buffer_p, game_sound_output_buffer *sound_buffer_p)
{
	static int32 tone_hz	 = 256;
	static int32 x_offset = 0;
	static int32 y_offset = 0;

	game_controller_input *input_0 = &input_p->controllers[0];
	if (input_0->is_analog) {
		tone_hz = 256 + (int32)(128.0f * input_0->left_stick.end_y);
		x_offset += (int32)(4.8f * input_0->left_stick.end_x);
	}
	else {

	}

	// Input.AButtonHalfTransitionCount;
	if (input_0->a.ended_down)
	{
		y_offset += 1;
	}
	

	game_output_sound(sound_buffer_p, tone_hz);
	render_weird_grandent(buffer_p, x_offset, y_offset);
}