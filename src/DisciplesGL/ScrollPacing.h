#pragma once

#include "windows.h"

namespace ScrollPacing
{
	VOID Reset();
	LONGLONG NextFlush(LONGLONG now, LONGLONG step, BOOL capped);
	BOOL Step(LONG tickX, LONG tickY, DWORD ticksPerSecond, POINT* offset);
}
