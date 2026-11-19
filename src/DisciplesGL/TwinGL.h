#pragma once

#include "ExtraTypes.h"

namespace TwinGL
{
	struct Source {
		UINT fbo;
		LONG width, height, scale;
	};

	BOOL Prepare(DWORD frame);
	VOID Replay(DWORD frame);
	BOOL GetSource(Source* source);
	BOOL Show(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	BOOL Draw(DWORD frame, const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
