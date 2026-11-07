#pragma once

#include "ExtraTypes.h"

namespace TwinGL
{
	BOOL Prepare();
	BOOL Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
