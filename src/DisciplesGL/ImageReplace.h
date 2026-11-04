#pragma once

#include "Hooker.h"
#include "Hooks.h"
#include "OpenDrawSurface.h"
#include "StateBuffer.h"
#include "Twin.h"

namespace ImageReplace
{
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);

	VOID OnRelease(OpenDrawSurface* surface);

	enum GroundKind { GroundCopy, GroundBlend, GroundBorder, GroundClear };
	VOID OnGround(GroundKind kind, const VOID* tile, const Hooks::BlitObject* obj, const POINT* src, const POINT* dst, const RECT* rect, const BYTE* blendMask, const DWORD* alphaMask);
	VOID OnGroundFill(const Hooks::BlitObject* obj, DWORD color, const RECT* rect);

	const CHAR* ImageName(const VOID* image);
	LONG TakeFace(const VOID* pixels, LONG width, LONG height, BOOL* flipped);
	LONG PartFrame(const CHAR* name, const RECT* canvas, RECT* part);
	VOID OnFaceCopy(const DWORD* src, LONG srcPitch, const DWORD* dst, LONG top, LONG bottom, LONG width, BOOL mirror, BOOL plain);

	BOOL Prepare(StateBufferAligned* frame, BOOL active);
	VOID Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight);
	VOID ReleaseGL();
	BOOL ArtTexture(LONG frame, GLuint* id, LONG* width, LONG* height);
	BOOL ArtReady(LONG frame);
	LONG ArtState(LONG frame);
	BOOL ArtUpload(LONG frame);
	LONG ArtBytes(LONG frame);
	// .ff record reads by the game (CreateFileA, ReadFile, CloseHandle from Discipl2.exe's imports): the record names the image that is decoded next
	VOID OnFileOpen(const CHAR* path, HANDLE handle);
	VOID OnFileClose(HANDLE handle);
	BOOL RecordFile(HANDLE handle);
	VOID OnFileRead(HANDLE handle, LONGLONG position, const VOID* buffer, DWORD size, DWORD caller, DWORD outer);
	struct RecordName {
		CHAR name[40];
		CHAR archive[24];
		DWORD record;
		DWORD caller;
		DWORD outer;
		DWORD framesAgo;
		DWORD candidates;
		BOOL sheet;
		const Twin::SeedPiece* pieces;
		DWORD count;
		const Twin::SeedConflict* conflicts;
		DWORD conflictCount;
		const CHAR* reason;
	};
	// the latest record read of an image of exactly this size that no decode took yet; the pieces stay valid until the next call
	BOOL RecordDecode(LONG width, LONG height, RecordName* out);
	const CHAR* MemoryName(const VOID* ptr);
	VOID LogSeedSources();
	BOOL FindMemory(const VOID* ptr, const VOID** base, LONG* pitch, LONG* width, LONG* height, LONG* frame, BOOL* flipped);
	VOID GroundFlush(const VOID* ptr);
	BOOL GroundReady(DWORD chunk);
	BOOL DrawGround(const VOID* ops, DWORD bytes, LONG width, LONG height);
}
