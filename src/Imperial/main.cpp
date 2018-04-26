#include "main.h"

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

static void GameUpdateAndRender(game_offscreen_buffer *buffer)
{
	int32 XOffset = 0;
	int32 YOffset = 0;
	RenderWeirdGrandent(buffer, XOffset, YOffset);
}