#pragma once

#include "Hooker.h"
#include "Hooks.h"

class OpenDrawSurface;

namespace Ground
{
	enum Kind { OpCopy, OpBlend, OpBorder, OpClear };

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);
	VOID OnGround(Kind kind, const VOID* tile, const Hooks::BlitObject* obj, const POINT* src, const POINT* dst, const RECT* rect, const BYTE* blendMask, const DWORD* alphaMask);
	VOID OnFill(const Hooks::BlitObject* obj, DWORD color, const RECT* rect);
	VOID OnRelease(OpenDrawSurface* surface);
	VOID Flush(const VOID* ptr);
	VOID Retry();

	BOOL Draw(const VOID* ops, DWORD bytes, LONG width, LONG height);
	VOID ReleaseGL();
}
