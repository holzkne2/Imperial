#pragma once

#include "Debug.h"

#include <stdint.h>

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

// timing, input, bitmap buffer, sound buffer
static void GameUpdateAndRender(game_offscreen_buffer *buffer);

//
// Services that the platform provides to the game
//