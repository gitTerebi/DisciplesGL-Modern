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

#ifdef _HOOKER_LIB
#pragma comment(lib, "hooker.lib")
#pragma comment(linker, "/DLL /ENTRY:HookMain@12")
#endif  // _HOOKER_LIB

#include "windows.h"

typedef VOID* HOOKER;

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

extern "C"
{
	/// <summary>
	///	Creates hooker object
	/// </summary>
	/// <param name="hModule"></param>
	/// <returns></returns>
	HOOKER __stdcall CreateHooker(HMODULE hModule);

	/// <summary>
	/// Deletes Hooker object
	/// </summary>
	/// <param name="hooker"></param>
	VOID __stdcall ReleaseHooker(HOOKER hooker);

	/// <summary>
	/// Retrives module base address offset
	/// </summary>
	/// <param name="hooker"></param>
	/// <returns></returns>
	DWORD __stdcall GetBaseOffset(HOOKER hooker);

	/// <summary>
	/// Retrives module handle
	/// </summary>
	/// <param name="hooker"></param>
	/// <returns></returns>
	HMODULE __stdcall GetHookerModule(HOOKER hooker);

	/// <summary>
	/// Reads data block
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="block"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	BOOL __stdcall ReadBlock(HOOKER hooker, DWORD address, VOID* block, DWORD size);

	/// <summary>
	/// Reads pointer value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadPtr(HOOKER hooker, DWORD address, VOID** lpValue);

	/// <summary>
	/// Reads byte value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadByte(HOOKER hooker, DWORD address, BYTE* lpValue);

	/// <summary>
	/// Reads word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadWord(HOOKER hooker, DWORD address, WORD* lpValue);

	/// <summary>
	/// Reads double word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadDWord(HOOKER hooker, DWORD address, DWORD* lpValue);

	/// <summary>
	/// Reads quad word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadQWord(HOOKER hooker, DWORD address, QWORD* lpValue);

	/// <summary>
	/// Reads short value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadShort(HOOKER hooker, DWORD address, SHORT* lpValue);

	/// <summary>
	/// Reads long value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadLong(HOOKER hooker, DWORD address, LONG* lpValue);

	/// <summary>
	/// Reads long long value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadLongLong(HOOKER hooker, DWORD address, LONGLONG* lpValue);

	/// <summary>
	/// Reads float value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadFloat(HOOKER hooker, DWORD address, FLOAT* lpValue);

	/// <summary>
	/// Reads double float value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="lpValue"></param>
	/// <returns></returns>
	BOOL __stdcall ReadDouble(HOOKER hooker, DWORD address, DOUBLE* lpValue);

	/// <summary>
	/// Find data block address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="block"></param>
	/// <param name="size"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindBlock(HOOKER hooker, const VOID* block, DWORD size, DWORD flags = 0, DWORD start = 0);
	
	/// <summary>
	/// Find data block address by bit mask
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="block"></param>
	/// <param name="mask"></param>
	/// <param name="size"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindBlockByMask(HOOKER hooker, const VOID* block, const VOID* mask, DWORD size, DWORD flags = 0, DWORD start = 0);
	
	/// <summary>
	/// Find pointer value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindPtr(HOOKER hooker, const VOID* value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find byte value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindByte(HOOKER hooker, BYTE value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindWord(HOOKER hooker, WORD value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find double word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindDWord(HOOKER hooker, DWORD value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find quad word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindQWord(HOOKER hooker, QWORD value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find short value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindShort(HOOKER hooker, SHORT value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find long value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindLong(HOOKER hooker, LONG value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find long long value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindLongLong(HOOKER hooker, LONGLONG value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find float value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindFloat(HOOKER hooker, FLOAT value, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Find double float value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="value"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindDouble(HOOKER hooker, DOUBLE value, DWORD flags = 0, DWORD start = 0);
	
	/// <summary>
	/// Find relative function call
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="flags"></param>
	/// <param name="start"></param>
	/// <returns></returns>
	DWORD __stdcall FindCall(HOOKER hooker, DWORD address, DWORD flags = 0, DWORD start = 0);

	/// <summary>
	/// Write redirect to new address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="newAddress"></param>
	/// <param name="type"></param>
	/// <param name="nopCount"></param>
	/// <returns></returns>
	BOOL __stdcall PatchRedirect(HOOKER hooker, DWORD address, DWORD newAddress, RedirectType type, DWORD nopCount = 0);

	/// <summary>
	/// Writes jump to new address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="jumpAddress"></param>
	/// <returns></returns>
	BOOL __stdcall PatchJump(HOOKER hooker, DWORD address, DWORD jumpAddress);

	/// <summary>
	/// Writes jump to new function
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="hookAddress"></param>
	/// <param name="nopCount"></param>
	/// <returns></returns>
	BOOL __stdcall PatchHook(HOOKER hooker, DWORD address, const VOID* hookAddress, DWORD nopCount = 0);

	/// <summary>
	/// Writes call to new function
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="funcAddress"></param>
	/// <param name="nopCount"></param>
	/// <returns></returns>
	BOOL __stdcall PatchCall(HOOKER hooker, DWORD address, const VOID* funcAddress, DWORD nopCount = 0);

	/// <summary>
	/// Fill bytes by specified value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	BOOL __stdcall PatchSet(HOOKER hooker, DWORD address, BYTE value, DWORD size);

	/// <summary>
	/// Fills bytes by no operation (nop) instruction
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	BOOL __stdcall PatchNop(HOOKER hooker, DWORD address, DWORD size);

	/// <summary>
	/// Writes new hex string
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="block"></param>
	/// <returns></returns>
	BOOL __stdcall PatchHex(HOOKER hooker, DWORD address, const CHAR* hex);

	/// <summary>
	/// Writes new data block
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="block"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	BOOL __stdcall PatchBlock(HOOKER hooker, DWORD address, const VOID* block, DWORD size);
	
	/// <summary>
	/// Writes new data block by bit mask
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="block"></param>
	/// <param name="mask"></param>
	/// <param name="size"></param>
	/// <returns></returns>
	BOOL __stdcall PatchBlockByMask(HOOKER hooker, DWORD address, const VOID* block, const VOID* mask, DWORD size);

	/// <summary>
	/// Writes new pointer value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchPtr(HOOKER hooker, DWORD address, const VOID* value);

	/// <summary>
	/// Writes new byte value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchByte(HOOKER hooker, DWORD address, BYTE value);

	/// <summary>
	/// Writes new word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchWord(HOOKER hooker, DWORD address, WORD value);

	/// <summary>
	/// Writes new double word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchDWord(HOOKER hooker, DWORD address, DWORD value);

	/// <summary>
	/// Writes new quad word value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchQWord(HOOKER hooker, DWORD address, QWORD value);

	/// <summary>
	/// Writes new short value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchShort(HOOKER hooker, DWORD address, SHORT value);

	/// <summary>
	/// Writes new long value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchLong(HOOKER hooker, DWORD address, LONG value);

	/// <summary>
	/// Writes new long long value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchLongLong(HOOKER hooker, DWORD address, LONGLONG value);

	/// <summary>
	/// Writes new float value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchFloat(HOOKER hooker, DWORD address, FLOAT value);

	/// <summary>
	/// Writes new double float value
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchDouble(HOOKER hooker, DWORD address, DOUBLE value);

	/// <summary>
	/// Writes all new data blocks
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_block"></param>
	/// <param name="new_block"></param>
	/// <param name="size"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllBlocks(HOOKER hooker, const VOID* old_block, const VOID* new_block, DWORD size, DWORD flags = 0);

	/// <summary>
	/// Writes all new data blocks by bit mask
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_block"></param>
	/// <param name="old_mask"></param>
	/// <param name="new_block"></param>
	/// <param name="new_mask"></param>
	/// <param name="size"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllBlocksByMask(HOOKER hooker, const VOID* old_block, const VOID* old_mask, const VOID* new_block, const VOID* new_mask, DWORD size, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new pointer values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllPtrs(HOOKER hooker, const VOID* old_value, const VOID* new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new byte values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllBytes(HOOKER hooker, BYTE old_value, BYTE new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new word values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllWords(HOOKER hooker, WORD old_value, WORD new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new double word values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllDWords(HOOKER hooker, DWORD old_value, DWORD new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new quad word values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllQWords(HOOKER hooker, QWORD old_value, QWORD new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new short values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllShorts(HOOKER hooker, SHORT old_value, SHORT new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new long values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllLongs(HOOKER hooker, LONG old_value, LONG new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new long long values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllLongLongs(HOOKER hooker, LONGLONG old_value, LONGLONG new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new float values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllFloats(HOOKER hooker, FLOAT old_value, FLOAT new_value, DWORD flags = 0);
	
	/// <summary>
	/// Writes all new double float values
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="old_value"></param>
	/// <param name="new_value"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall PatchAllDoubles(HOOKER hooker, DOUBLE old_value, DOUBLE new_value, DWORD flags = 0);

	/// <summary>
	/// Patch virtual function of object by its index
	/// </summary>
	/// <param name="obj"></param>
	/// <param name="index"></param>
	/// <param name="funct"></param>
	/// <param name="old_value"></param>
	/// <returns></returns>
	BOOL __stdcall PatchVirtual(const VOID* obj, DWORD index, const VOID* funct, VOID* old_value = NULL);

	/// <summary>
	/// Redirect relative function call to new address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="funcAddress"></param>
	/// <returns></returns>
	DWORD __stdcall RedirectCall(HOOKER hooker, DWORD address, const VOID* funcAddress);
	
	/// <summary>
	/// Redirect all relative function calls to new address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="address"></param>
	/// <param name="funcAddress"></param>
	/// <param name="flags"></param>
	/// <returns></returns>
	DWORD __stdcall RedirectAllCalls(HOOKER hooker, DWORD address, const VOID* funcAddress, DWORD flags = 0);

	/// <summary>
	/// Redirects module imported function and retrives old address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="name"></param>
	/// <param name="funcAddress"></param>
	/// <param name="old_value"></param>
	/// <param name="erace"></param>
	/// <returns></returns>
	DWORD __stdcall PatchImportByName(HOOKER hooker, const CHAR* name, const VOID* funcAddress = NULL, DWORD* old_value = NULL, BOOL erace = FALSE);
	
	/// <summary>
	/// Redirects module imported function and retrives old address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="ordinal"></param>
	/// <param name="funcAddress"></param>
	/// <param name="old_value"></param>
	/// <param name="erace"></param>
	/// <returns></returns>
	DWORD __stdcall PatchImportByOrdinal(HOOKER hooker, DWORD ordinal, const VOID* funcAddress = NULL, DWORD* old_value = NULL, BOOL erace = FALSE);

	/// <summary>
	/// Redirects module exported function and retrives old address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="name"></param>
	/// <param name="funcAddress"></param>
	/// <param name="old_value"></param>
	/// <returns></returns>
	DWORD __stdcall PatchExport(HOOKER hooker, const CHAR* name, const VOID* funcAddress, DWORD* old_value = NULL);

	/// <summary>
	/// Redirects module entry point and retrives old address
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="funcAddress"></param>
	/// <returns></returns>
	DWORD __stdcall PatchEntry(HOOKER hooker, const VOID* funcAddress);
	
	/// <summary>
	/// Redirect all imports to other module
	/// </summary>
	/// <param name="hooker"></param>
	/// <param name="libName"></param>
	/// <param name="hLib"></param>
	/// <returns></returns>
	BOOL __stdcall RedirectImports(HOOKER hooker, const CHAR* libName, HMODULE hLib = NULL);
	
	/// <summary>
	/// Map module file into memory
	/// </summary>
	/// <param name="hooker"></param>
	/// <returns></returns>
	DWORD __stdcall MapFile(HOOKER hooker);
	
	/// <summary>
	/// Unmap module file from memory
	/// </summary>
	/// <param name="hooker"></param>
	/// <returns></returns>
	VOID __stdcall UnmapFile(HOOKER hooker);
}