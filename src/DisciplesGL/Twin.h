#pragma once

#include "StateBuffer.h"

class OpenDrawSurface;

namespace Twin
{
	// game thread
	BOOL Active();
	VOID OnCreate(StateBuffer* buffer, LONG width, LONG height);
	VOID OnRelease(StateBuffer* buffer);
	VOID OnCopy(StateBuffer* dst, LONG x, LONG y, StateBuffer* src, const RECT* srcRect, BOOL keyed, DWORD key);
	VOID OnFill(StateBuffer* dst, const RECT* rect, DWORD color);
	VOID OnWrite(const VOID* first, LONG pitch, LONG width, LONG height);
	VOID OnWriteAll(StateBuffer* buffer);
	VOID OnPresent(StateBuffer* screen, LONG left, LONG top);
	VOID BeginArt(LONG frame, const RECT* place, const RECT* clip, BOOL flipped);
	VOID EndArt();
	VOID LogStats();

	// render thread
	BOOL Prepare();
	BOOL Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
