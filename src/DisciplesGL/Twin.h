#pragma once

#include "StateBuffer.h"

#define TWIN_MAX 512
#define TWIN_NONE 0xFFFFFFFF
#define DIRTY_MAX 32
#define ART_MAX 64
#define MASK_SIZE 2048

namespace Twin
{
	BOOL Active();
	VOID OnCreate(StateBuffer* buffer, LONG width, LONG height);
	VOID OnRelease(StateBuffer* buffer);
	VOID OnCopyCaller(DWORD caller, DWORD outer);
	VOID OnCopy(StateBuffer* dst, LONG x, LONG y, StateBuffer* src, const RECT* srcRect, BOOL keyed, DWORD key);
	VOID OnFill(StateBuffer* dst, const RECT* rect, DWORD color);
	VOID OnWrite(const VOID* first, LONG pitch, LONG width, LONG height);
	VOID OnWriteAll(StateBuffer* buffer);
	enum PixelMode { PixelKeyed, PixelAverage, PixelAdd, PixelSub, PixelEmpty, PixelCopy, PixelBlend, PixelKeyedCopy, PixelTint, PixelModes };
	enum PixelFlag { PixelMirror = 0x10000 };
	VOID OnPixelCopy(PixelMode mode, const DWORD* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height, DWORD key, DWORD param);
	VOID OnFillRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color);
	VOID OnTintRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color, DWORD weight);
	VOID OnBlendRow(const DWORD* src, const DWORD* dst, LONG length, const BYTE* mask, BOOL keyed, DWORD key);
	VOID OnLighter(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD key, BOOL lighten);
	VOID OnMemory(const VOID* base, LONG pitch, LONG width, LONG height);
	VOID OnPresent(StateBuffer* screen, LONG left, LONG top);

	enum CmdType { CmdCreate, CmdDestroy, CmdReset, CmdCopy, CmdFill, CmdTint, CmdUpload, CmdPixel, CmdMask, CmdLighter, CmdSeed, CmdGround, CmdPresent };
	enum PresentFlags { PresentMapZoom = 1, PresentBack = 2, PresentZoomed = 4 };

	struct Cmd {
		DWORD type, dst, src;
		RECT rect, place;
		LONG x, y;
		DWORD value, flags, extra;
	};

	struct List {
		List* next;
		Cmd* cmds;
		DWORD count, capacity;
		BYTE* arena;
		DWORD size, arenaCapacity;
	};

	struct Shared {
		CRITICAL_SECTION cs;
		volatile LONG state, gen;
		volatile BOOL show, glFailed;
		List* head;
		List* tail;
		DWORD bytes;
		List* pool;
		DWORD poolCount;
	};

	extern Shared shared;
	VOID Lock();
	VOID Unlock();
	VOID RecycleList(List* list);
	VOID DropQueued();

	struct Record {
		StateBuffer* owner;
		BYTE* base;
		LONG pitch, width, height;
		BOOL keyed;
		DWORD key;
		BOOL memory, screen, told1x;
		DWORD lastUse, stamp, seeds;
		DWORD count;
		RECT dirty[DIRTY_MAX];
		DWORD artCount;
		RECT art[ART_MAX];
	};

	Record* At(DWORD id);
	Record* FindAddress(const VOID* ptr, DWORD* id);
	BOOL PixelPos(const Record* twin, const VOID* ptr, LONG pitch, LONG width, LONG height, LONG* x, LONG* y);
	Record* MemoryTwin(const VOID* base, LONG pitch, LONG width, LONG height, BOOL create, DWORD* id, RECT* rect);
	VOID DropMemory(const VOID* base);
	Cmd* Emit(DWORD type);
	DWORD ListBytes();
	VOID MarkDirty(Record* twin, RECT rect);
	VOID Cover(Record* twin, const RECT* rect);
	VOID CutRegion(Record* twin, const RECT* rect);
	VOID Upload(Record* twin, DWORD id, const RECT* rect, BOOL seed);
	VOID AddArt(Record* twin, const RECT* rect);
	BOOL Carries4K(const Record* twin, const RECT* rect);
	DWORD Stamp(const VOID* data);
	VOID GroundArt(const VOID* data, LONG pitch, const RECT* rect, const VOID* ops, DWORD bytes);
}
