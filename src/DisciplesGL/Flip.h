#pragma once

#include "Twin.h"

namespace Flip
{
	VOID NoteSource(Twin::Record* twin);
	VOID OnCopy(Twin::Record* target, DWORD dstId, const RECT* to, Twin::Record* source, DWORD srcId, const RECT* from, const CHAR* how, DWORD param);
	VOID OnPixel(DWORD mode, const DWORD* src, const DWORD* dst, LONG width, LONG height, DWORD param);
	VOID OnBlt(const CHAR* how, StateBuffer* dst, StateBuffer* src, const RECT* dstRect, LONG x, LONG y, const RECT* srcRect, DWORD flags, const DDBLTFX* fx);
	VOID OnRelease(Twin::Record* twin);
	VOID Settle();
}
