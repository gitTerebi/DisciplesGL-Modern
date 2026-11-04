#pragma once

#include "StateBuffer.h"

class OpenDrawSurface;

namespace Twin
{
	// game thread
	BOOL Active();
	// game-thread time inside twin and image code, per present and category; a scope charges its own time, not its callees' scopes
	enum CostKind { CostNone, CostRecord, CostMirror, CostTile, CostSeed, CostTrap, CostLookup, CostDiag, CostImage, CostKinds };
	struct Cost {
		DWORD prev;
		explicit Cost(DWORD kind);
		~Cost();
	};
	// sub-steps of the seed work on the game thread, for the hitch line; exclusive like Cost: an inner step charges its own time, not the outer one's
	enum SeedStepKind { StepNone, StepWait, StepPieces, StepCapture, StepGuard, StepBase, StepStack, StepResolve, SeedStepKinds };
	struct SeedStep {
		DWORD prev;
		explicit SeedStep(DWORD kind);
		~SeedStep();
	};
	// [Twin] Diagnostics=1: per-copy write notes and caller scans, copy-in and arrival records, coverage tables, copy traces
	BOOL Diagnostics();
	// a ground tile or border blit: the map view is being drawn, which scrolling does every frame
	VOID CountGround();
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
		// blocks whose sheet size is not the decode's, frames whose pack entry is named like the sheet file (seeded in canvas layout like the rest), pieces dropped as uncertain
		DWORD wrongSize;
		DWORD fileNamed;
		// frames whose pack entry reaches outside the union of their parts in the canvas: another image of that name, left 1x
		DWORD notFrame;
		DWORD uncertain;
	};
	// memoryTwin FALSE: an 8-bit decode whose 32 bpp reading runs past the scratch; it is only captured for the transfer, no memory twin is made over it
	VOID SeedArt(const VOID* first, LONG pitch, LONG width, LONG height, SeedKind kind, const CHAR* name, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount, const SeedSheetStats* sheet, BOOL memoryTwin = TRUE);
	// the latest transfer capture's sequence, and whether it holds a snapshot with pieces
	DWORD CaptureSeq(BOOL* valid);
	// the image the coverage is counted under: the latest large plain image a loader decoded (the screen's background)
	VOID ScreenLabel(const CHAR* name);
	// the game decodes store art into a scratch buffer and then writes it into a surface by CPU; these follow it there
	VOID DecodeBegin();
	VOID DecodeEnd();
	VOID OnLock(StateBuffer* buffer, const RECT* rect, DWORD caller, DWORD outer);
	VOID OnUnlock(StateBuffer* buffer);
	// the decode's BlitObject as the game passed it: data, raw pitch field, format byte (0 = 8-bit indices) and clip rect
	VOID DecodeSource(const VOID* data, LONG pitchField, BYTE trueColor, const RECT* rect);
	// slot is the stack slot of the game's return address into the blit, the start of the call chain
	VOID OnIndexedBlit(const BYTE* src, LONG srcPitch, const DWORD* palette, const DWORD* dst, LONG dstPitch, LONG width, LONG height, const DWORD* slot);
	// return addresses into the game, from the slot of the innermost one outwards (frame pointer chain above it)
	DWORD WalkFrames(const DWORD* slot, DWORD* frames, DWORD count);
	// the game's code around an address goes into twin-code.bin, for disassembly in the next round
	VOID CodeDump(DWORD address, DWORD radius, const CHAR* why);
	// the next number of a twin decode named line, 0 once the log limit is reached
	DWORD DecodeLogNumber();
	VOID RenderBegin();
	VOID RenderEnd();
	// first-write trap: surface pages are read-only until the first write, which names the code that writes it ([Twin] WriteTrap)
	BOOL TrapWanted();

	// render thread
	BOOL Prepare();
	BOOL Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
}
