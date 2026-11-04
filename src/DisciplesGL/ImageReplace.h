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
	BOOL OnPixelBlit(const POINT* srcPos, const DWORD* dstData, LONG dstPitch, const POINT* dstPos, const SIZE* size);
	VOID OnPixelRows(const Hooks::BlendData* items, DWORD count);

	enum GroundKind { GroundCopy, GroundBlend, GroundBorder, GroundClear };
	VOID OnGround(GroundKind kind, const VOID* tile, const Hooks::BlitObject* obj, const POINT* src, const POINT* dst, const RECT* rect, const BYTE* blendMask, const DWORD* alphaMask);
	VOID OnGroundFill(const Hooks::BlitObject* obj, DWORD color, const RECT* rect);
	struct StripLook {
		RECT part;
		COLORREF tint;
		COLORREF rim;
		BOOL gem;
	};

	struct StripFace {
		LONG frame;
		BOOL flipped;
		RECT rect;
		RECT clip;
		StripLook look;
	};

	const CHAR* ImageName(const VOID* image);
	LONG TakeFace(const VOID* pixels, LONG width, LONG height, BOOL* flipped);
	LONG PartFrame(const CHAR* name, const RECT* canvas, RECT* part);
	VOID OnStrip(const VOID* origin, LONG pitch, LONG width, LONG height, const StripFace* faces, DWORD count);
	VOID OnFaceCopy(const DWORD* src, LONG srcPitch, const DWORD* dst, LONG top, LONG bottom, LONG width, BOOL mirror, BOOL plain);
	VOID OnGroundCopy(const VOID* data, const RECT* rect, LONG pitch, const Hooks::BlitObject* obj, const POINT* loc);
	VOID OnPresent(OpenDrawSurface* back, StateBuffer* frame, LONG left, LONG top);

	BOOL Prepare(StateBufferAligned* frame, BOOL active);
	VOID Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
