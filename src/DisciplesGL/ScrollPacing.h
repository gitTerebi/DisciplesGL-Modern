#pragma once

#include "windows.h"

namespace ScrollPacing
{
	VOID Reset();
	LONGLONG NextFlush(LONGLONG now, LONGLONG fallbackStep);
	BOOL Step(HWND hWnd, LONG tickX, LONG tickY, DWORD ticksPerSecond, POINT* offset);
}
