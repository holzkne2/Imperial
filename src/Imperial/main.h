#pragma once

#include "Debug.h"

#include <stdint.h>

#define PI32 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef float real32;
typedef double real64;

//
// Services that the game provides to the platform
//

struct game_offscreen_buffer
{
	void *Memory;
	int32 Width;
	int32 Height;
	int32 Pitch;
};

struct game_sound_output_buffer
{
	int32 SamplesPerSecond;
	int32 SampleCount;
	int16 *Samples;
};

// timing, input, bitmap buffer, sound buffer
static void GameUpdateAndRender(game_offscreen_buffer *buffer, game_sound_output_buffer *SoundBuffer);

//
// Services that the platform provides to the game
//