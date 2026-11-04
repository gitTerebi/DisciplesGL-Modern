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
	// the game code behind the next OnCopy (Blt/BltFast return address and the caller above the game's wrapper)
	VOID OnCopyCaller(DWORD caller, DWORD outer);
	VOID OnFill(StateBuffer* dst, const RECT* rect, DWORD color);
	VOID OnWrite(const VOID* first, LONG pitch, LONG width, LONG height);
	VOID OnWriteAll(StateBuffer* buffer);
	enum PixelMode { PixelKeyed, PixelAverage, PixelAdd, PixelSub, PixelEmpty, PixelCopy, PixelBlend, PixelKeyedCopy, PixelTint };
	enum PixelFlag { PixelMirror = 0x10000 };
	VOID OnPixelCopy(PixelMode mode, const DWORD* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height, DWORD key, DWORD param);
	VOID OnFillRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color);
	VOID OnTintRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color, DWORD weight);
	VOID OnBlendRow(const DWORD* src, const DWORD* dst, LONG length, const BYTE* mask, BOOL keyed, DWORD key);
	VOID OnLighter(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD key, BOOL lighten);
	VOID OnPresent(StateBuffer* screen, LONG left, LONG top);
	struct ArtPart {
		LONG dx, dy;
		LONG sx, sy;
		LONG w, h;
	};
	VOID ExpectArt(LONG frame, BOOL flipped, const RECT* place, const RECT* clip, const ArtPart* parts, DWORD count);
	VOID GroundArt(const VOID* data, LONG pitch, const RECT* rect, const VOID* ops, DWORD bytes);
	VOID OnMemory(const VOID* base, LONG pitch, LONG width, LONG height);
	VOID OnMemoryFree(const VOID* data);
	VOID LearnArt(const VOID* data, LONG pitch, const RECT* rect, LONG frame, const RECT* canvas, LONG canvasWidth, LONG canvasHeight, BOOL flipped);
	VOID LogStats();

	// seed mode: 4K art is written into a twin where the game decodes 1x art whose name is known; every later operation is a plain replay
	BOOL SeedMode();
	enum SeedKind { SeedStore, SeedSheet, SeedFace, SeedKinds };
	// rects are local to the decoded image; place is where the whole pack frame lands in those coordinates
	struct SeedPiece {
		RECT to;
		RECT place;
		LONG frame;
	};
	struct SeedConflict {
		RECT to;
		RECT placeA;
		RECT placeB;
		LONG frameA;
		LONG frameB;
	};
	struct SeedSheetStats {
		DWORD frames;
		DWORD unmapped;
		DWORD additive;
		DWORD outside;
		DWORD shared;
	};
	VOID SeedArt(const VOID* first, LONG pitch, LONG width, LONG height, SeedKind kind, const CHAR* name, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount, const SeedSheetStats* sheet);
	// the game decodes store art into a scratch buffer and then writes it into a surface by CPU; these follow it there
	VOID DecodeBegin();
	VOID DecodeEnd();
	VOID OnLock(StateBuffer* buffer, const RECT* rect, DWORD caller, DWORD outer);
	VOID OnUnlock(StateBuffer* buffer);
	// the decode's BlitObject as the game passed it: data, raw pitch field, format byte (0 = 8-bit indices) and clip rect
	VOID DecodeSource(const VOID* data, LONG pitchField, BYTE trueColor, const RECT* rect);
	VOID OnIndexedBlit(const BYTE* src, LONG srcPitch, const DWORD* palette, const DWORD* dst, LONG dstPitch, LONG width, LONG height);
	VOID RenderBegin();
	VOID RenderEnd();
	// first-write trap: surface pages are read-only until the first write, which names the code that writes it ([Twin] WriteTrap)
	BOOL TrapWanted();

	// render thread
	BOOL Prepare();
	BOOL Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
