#pragma once

#include "OpenDrawSurface.h"
#include "StateBuffer.h"
#include "Hooks.h"

namespace TextReplace
{
	VOID SetRenderer(BOOL ready);
	VOID OnBuffer(OpenDrawSurface* surface);
	VOID OnRelease(OpenDrawSurface* surface);
	VOID OnLock(OpenDrawSurface* surface);
	VOID OnIsLost(OpenDrawSurface* surface);
	VOID TraceFrame();
	VOID TraceLock(OpenDrawSurface* surface, const RECT* rect, const VOID* ret);
	VOID TraceBlt(const CHAR* op, OpenDrawSurface* dst, const RECT* dstRect, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect, DWORD flags, const VOID* ret);
	VOID TracePixel(const CHAR* op, const VOID* dst, const VOID* src, LONG width, LONG height, DWORD alpha, const VOID* ret);
	VOID OnFill(OpenDrawSurface* surface);
	VOID OnBlit(OpenDrawSurface* dst, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect, LONG srcStride, BOOL keyed, DWORD key);
	VOID OnFillRect(const VOID* data, const RECT* rect);
	VOID OnDiamond(const VOID* data, const POINT* loc, const RECT* rect);
	VOID OnPixelCopy(const DWORD* srcData, LONG srcPitch, const POINT* srcPos, const DWORD* dstData, LONG dstPitch, const POINT* dstPos, const SIZE* size, BOOL keyed, DWORD key, DWORD alpha);
	VOID OnPixelRows(const Hooks::BlendData* items, DWORD count);
	BOOL OnGlyph(const DWORD* font, const VOID* data, POINT pt, const RECT* clipper, DWORD fill, DWORD line, BOOL outline, BYTE symbol);
	VOID OnPresent(OpenDrawSurface* back, StateBuffer* frame, LONG left, LONG top);
	VOID OnFlushEnd(OpenDrawSurface* back);

	BOOL Prepare(StateBufferAligned* frame, BOOL active);
	VOID Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
