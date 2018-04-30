#include "Maths.h"

real32 remap_n11(const real32 low, const real32 high, const real32 value)
{
	return (-1.0f + (value - low) * 2.0f / (real32)(high - low));
}