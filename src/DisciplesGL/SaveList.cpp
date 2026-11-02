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

#include "StdAfx.h"
#include "Hooks.h"
#include "SaveList.h"

#define f(a) (a + baseOffset)

namespace SaveList
{
	#define SAVE_ENTRY_SIZE 0x80
	#define LOAD_ENTRY_SIZE 0x64

	const CHAR* SaveName(const CHAR* path)
	{
		const CHAR* name = StrLastChar(path, '\\');
		return name ? name + 1 : path;
	}

	BOOL IsPinnedSave(const CHAR* name)
	{
		return !StrCompareInsensitive(name, "AutoSave.sg") || !StrCompareInsensitive(name, "BattleSave.sg") || !StrCompareInsensitive(name, "QuickSave.sg");
	}

	// Matches the "<scenario> T0000.sg" names that Hooks CopyAutoSave writes
	BOOL IsCurrentGameSave(const CHAR* name, const CHAR* scenario)
	{
		DWORD length = StrLength(scenario);
		if (StrLength(name) != length + 9 || StrCompareNum(name, scenario, length) || name[length] != ' ' || name[length + 1] != 'T')
			return FALSE;

		for (DWORD i = length + 2; i < length + 6; ++i)
			if (name[i] < '0' || name[i] > '9')
				return FALSE;

		return !StrCompareInsensitive(name + length + 6, ".sg");
	}

	// Entries from Discipl2.exe 0x005FD786 and 0x0050326E start with the path string, text pointer per c_str at 0x00403AFE
	BYTE* PinMatches(BYTE* front, BYTE* end, DWORD size, const CHAR* scenario)
	{
		for (BYTE* item = front; item < end; item += size)
		{
			const CHAR* path = *(const CHAR**)(item + 8);
			if (path && (scenario ? IsCurrentGameSave(SaveName(path), scenario) : IsPinnedSave(SaveName(path))))
			{
				if (item != front)
				{
					BYTE entry[SAVE_ENTRY_SIZE];
					MemoryCopy(entry, item, size);
					memmove(front + size, front, item - front);
					MemoryCopy(front, entry, size);
				}

				front += size;
			}
		}

		return front;
	}

	VOID PinSaves(BYTE* front, BYTE* end, DWORD size)
	{
		if (front >= end)
			return;

		const CHAR* first = *(const CHAR**)(front + 8);
		front = PinMatches(front, end, size, NULL);
		if (!first)
			return;

		CHAR path[MAX_PATH];
		DWORD dirLen = SaveName(first) - first;
		if (dirLen >= sizeof(path) - 16)
			return;

		MemoryCopy(path, first, dirLen);
		StrCopy(path + dirLen, "AutoSave.sg");

		Hooks::AutoSaveInfo info;
		if (Hooks::ReadAutoSaveInfo(path, &info))
			PinMatches(front, end, size, info.scenario[0] ? info.scenario : "AutoSave");
	}

	VOID __fastcall PinSaveList(BYTE* list)
	{
		PinSaves(*(BYTE**)(list + 4), *(BYTE**)(list + 8), SAVE_ENTRY_SIZE);
	}

	VOID __fastcall PinLoadList(BYTE** list)
	{
		if (list)
			PinSaves(list[0], list[1], LOAD_ENTRY_SIZE);
	}

	DWORD back_005FD0D7;
	VOID __declspec(naked) hook_005FD0D2()
	{
		__asm {
			cmp byte ptr [esi], 0
			je skip
			mov ecx, esi
			call PinSaveList

			skip:
			mov ecx, [ebp-0xC]
			mov eax, esi
			jmp back_005FD0D7
		}
	}

	DWORD back_00503260;
	VOID __declspec(naked) hook_0050325B()
	{
		__asm {
			mov ecx, [esi+0x4]
			call PinLoadList

			mov ecx, [ebp-0xC]
			mov eax, esi
			jmp back_00503260
		}
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (hookSpace->save_list_sort)
		{
			PatchHook(hooker, hookSpace->save_list_sort, hook_005FD0D2);
			back_005FD0D7 = f(hookSpace->save_list_sort + 5);
		}

		if (hookSpace->load_list_sort)
		{
			PatchHook(hooker, hookSpace->load_list_sort, hook_0050325B);
			back_00503260 = f(hookSpace->load_list_sort + 5);
		}
	}
}
