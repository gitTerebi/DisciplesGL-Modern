#pragma once

#include "Hooker.h"
#include "Hooks.h"
#include "Twin.h"

namespace Seed
{
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);
	VOID OnFileOpen(const CHAR* path, HANDLE handle);
	VOID OnFaceCopy(const DWORD* src, LONG srcPitch);
	VOID OnIndexedBlit(const BYTE* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height);

	VOID Write(Twin::Record* twin, const RECT* rect);
	VOID DropTwin(Twin::Record* twin);
	VOID Repend();
	VOID Poll();
	VOID Derive(Twin::Record* target, DWORD dstId, const RECT* to, Twin::Record* source, const RECT* from, BOOL mirror);
	VOID Tell1x(const Twin::Record* target, Twin::Record* source, const RECT* from, DWORD caller, const CHAR* how);
	const CHAR* DescribeMemory(const VOID* p);
}
