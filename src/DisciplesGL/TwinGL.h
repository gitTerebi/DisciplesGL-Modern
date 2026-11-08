#pragma once

#include "ExtraTypes.h"

namespace TwinGL
{
	struct Source {
		UINT fbo;
		LONG width, height, scale;
	};

	BOOL Prepare();
	VOID Replay();
	BOOL GetSource(Source* source);
	BOOL Show(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	BOOL Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
