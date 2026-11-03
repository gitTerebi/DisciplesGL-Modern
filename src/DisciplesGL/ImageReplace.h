#pragma once

#include "Hooker.h"
#include "Hooks.h"
#include "OpenDrawSurface.h"
#include "StateBuffer.h"

namespace ImageReplace
{
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);

	BOOL OnBlit(OpenDrawSurface* dst, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect, LONG srcStride, BOOL keyed, DWORD key);
	VOID OnFill(OpenDrawSurface* surface);
	VOID OnRelease(OpenDrawSurface* surface);
	VOID OnFillRect(const VOID* data, const RECT* rect);
	VOID OnDiamond(const VOID* data, const POINT* loc, const RECT* rect);
	VOID OnPixelCopy(const DWORD* srcData, LONG srcPitch, const POINT* srcPos, const DWORD* dstData, LONG dstPitch, const POINT* dstPos, const SIZE* size, BOOL keyed, DWORD key, DWORD alpha);
	VOID OnPixelRows(const Hooks::BlendData* items, DWORD count);
	VOID OnPresent(OpenDrawSurface* back, StateBuffer* frame, LONG left, LONG top);

	BOOL Prepare(StateBufferAligned* frame, BOOL active);
	VOID Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
