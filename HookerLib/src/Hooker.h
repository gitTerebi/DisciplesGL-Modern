/*
	MIT License

	Copyright (c) 2020 Oleksiy Ryabchun

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#pragma once

#if !defined(DOUBLE)
typedef double DOUBLE;
#endif

#if !defined(QWORD)
typedef unsigned __int64 QWORD;
#endif

enum RedirectType
{
	REDIRECT_CALL = 0xE8,
	REDIRECT_JUMP = 0xE9,
	REDIRECT_JUMP_SHORT = 0xEB
};

typedef struct Hooker {
	HANDLE hHeap;
	HMODULE hModule;
	PIMAGE_NT_HEADERS headNT;
	DWORD baseOffset;
	DWORD mapAddress;
	HANDLE hFile;
	HANDLE hMap;
} * HOOKER;

extern "C"
{
	DWORD MapFile(HOOKER);
	VOID UnmapFile(HOOKER);
	VOID CreateInner(HOOKER, HANDLE, HMODULE);
	VOID ReleaseInner(HOOKER);

	HOOKER CreateHooker(HMODULE);
	VOID ReleaseHooker(HOOKER);
	DWORD GetBaseOffset(HOOKER);
	HMODULE GetHookerModule(HOOKER);
	BOOL ReadBlock(HOOKER, DWORD, VOID*, DWORD);
	BOOL ReadPtr(HOOKER, DWORD, VOID**);
	BOOL ReadByte(HOOKER, DWORD, BYTE*);
	BOOL ReadWord(HOOKER, DWORD, WORD*);
	BOOL ReadDWord(HOOKER, DWORD, DWORD*);
	BOOL ReadQWord(HOOKER, DWORD, QWORD*);
	BOOL ReadShort(HOOKER, DWORD, SHORT*);
	BOOL ReadLong(HOOKER, DWORD, LONG*);
	BOOL ReadLongLong(HOOKER, DWORD, LONGLONG*);
	BOOL ReadFloat(HOOKER, DWORD, FLOAT*);
	BOOL ReadDouble(HOOKER, DWORD, DOUBLE*);
	DWORD FindBlock(HOOKER, const VOID*, DWORD, DWORD = 0, DWORD = 0);
	DWORD FindBlockByMask(HOOKER, const VOID*, const VOID*, DWORD, DWORD = 0, DWORD = 0);
	DWORD FindPtr(HOOKER, const VOID*, DWORD = 0, DWORD = 0);
	DWORD FindByte(HOOKER, BYTE, DWORD = 0, DWORD = 0);
	DWORD FindWord(HOOKER, WORD, DWORD = 0, DWORD = 0);
	DWORD FindDWord(HOOKER, DWORD, DWORD = 0, DWORD = 0);
	DWORD FindQWord(HOOKER, QWORD, DWORD = 0, DWORD = 0);
	DWORD FindShort(HOOKER, SHORT, DWORD = 0, DWORD = 0);
	DWORD FindLong(HOOKER, LONG, DWORD = 0, DWORD = 0);
	DWORD FindLongLong(HOOKER, LONGLONG, DWORD = 0, DWORD = 0);
	DWORD FindFloat(HOOKER, FLOAT, DWORD = 0, DWORD = 0);
	DWORD FindDouble(HOOKER, DOUBLE, DWORD = 0, DWORD = 0);
	DWORD FindCall(HOOKER, DWORD, DWORD = 0, DWORD = 0);
	BOOL PatchRedirect(HOOKER, DWORD, DWORD, RedirectType, DWORD = 0);
	BOOL PatchJump(HOOKER, DWORD, DWORD);
	BOOL PatchHex(HOOKER, DWORD, const CHAR*);
	BOOL PatchBlock(HOOKER, DWORD, const VOID*, DWORD);
	BOOL PatchBlockByMask(HOOKER, DWORD, const VOID*, const VOID*, DWORD);
	BOOL PatchHook(HOOKER, DWORD, const VOID*, DWORD = 0);
	BOOL PatchCall(HOOKER, DWORD, const VOID*, DWORD = 0);
	BOOL PatchSet(HOOKER, DWORD, BYTE, DWORD);
	BOOL PatchNop(HOOKER, DWORD, DWORD);
	BOOL PatchPtr(HOOKER, DWORD, const VOID*);
	BOOL PatchByte(HOOKER, DWORD, BYTE);
	BOOL PatchWord(HOOKER, DWORD, WORD);
	BOOL PatchDWord(HOOKER, DWORD, DWORD);
	BOOL PatchQWord(HOOKER, DWORD, QWORD);
	BOOL PatchShort(HOOKER, DWORD, SHORT);
	BOOL PatchLong(HOOKER, DWORD, LONG);
	BOOL PatchLongLong(HOOKER, DWORD, LONGLONG);
	BOOL PatchFloat(HOOKER, DWORD, FLOAT);
	BOOL PatchDouble(HOOKER, DWORD, DOUBLE);
	DWORD PatchAllBlocks(HOOKER, const VOID*, const VOID*, DWORD, DWORD = 0);
	DWORD PatchAllBlocksByMask(HOOKER, const VOID*, const VOID*, const VOID*, const VOID*, DWORD, DWORD = 0);
	DWORD PatchAllPtrs(HOOKER, const VOID*, const VOID*, DWORD = 0);
	DWORD PatchAllBytes(HOOKER, BYTE, BYTE, DWORD = 0);
	DWORD PatchAllWords(HOOKER, WORD, WORD, DWORD = 0);
	DWORD PatchAllDWords(HOOKER, DWORD, DWORD, DWORD = 0);
	DWORD PatchAllQWords(HOOKER, QWORD, QWORD, DWORD = 0);
	DWORD PatchAllShorts(HOOKER, SHORT, SHORT, DWORD = 0);
	DWORD PatchAllLongs(HOOKER, LONG, LONG, DWORD = 0);
	DWORD PatchAllLongLongs(HOOKER, LONGLONG, LONGLONG, DWORD = 0);
	DWORD PatchAllFloats(HOOKER, FLOAT, FLOAT, DWORD = 0);
	DWORD PatchAllDoubles(HOOKER, DOUBLE, DOUBLE, DWORD = 0);
	BOOL PatchVirtual(const VOID*, DWORD, const VOID*, VOID* = NULL);
	DWORD RedirectCall(HOOKER, DWORD, const VOID*);
	DWORD RedirectAllCalls(HOOKER, DWORD, const VOID*, DWORD = 0);
	DWORD PatchImport(HOOKER, DWORD, const VOID*, DWORD*, BOOL = FALSE);
	DWORD PatchImportByName(HOOKER, const CHAR*, const VOID* = NULL, DWORD* = NULL, BOOL = FALSE);
	DWORD PatchImportByOrdinal(HOOKER, DWORD, const VOID* = NULL, DWORD* = NULL, BOOL = FALSE);
	DWORD PatchExport(HOOKER, const CHAR*, const VOID*, DWORD* = NULL);
	DWORD PatchEntry(HOOKER, const VOID*);
	BOOL RedirectImports(HOOKER, const CHAR*, HMODULE = NULL);
}