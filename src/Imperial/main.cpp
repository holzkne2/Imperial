#include "main.h"

#include <math.h>

static void GameOutputSound(game_sound_output_buffer *SoundBuffer)
{
	static real32 tSine = 0;
	const static int16 ToneVolume = 16000;
	const static int32 ToneHz = 256;
	int32 WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;
	
	int16 *SampleOut = SoundBuffer->Samples;
	for (int32 SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
	{
		real32 SineValue = sinf(tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;

		tSine += 2.0f * PI32 * 1.0f / (real32)WavePeriod;
	}
}

static void RenderWeirdGrandent(game_offscreen_buffer *Buffer, int32 XOffset, int32 YOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;
	for (int32 Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int32 X = 0; X < Buffer->Width; ++X)
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

static void GameUpdateAndRender(game_offscreen_buffer *buffer, game_sound_output_buffer *SoundBuffer)
{
	GameOutputSound(SoundBuffer);

	int32 XOffset = 0;
	int32 YOffset = 0;
	RenderWeirdGrandent(buffer, XOffset, YOffset);
}