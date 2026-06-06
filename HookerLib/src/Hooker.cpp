/*
	MIT License

	Copyright (c) 2020 Oleksiy Ryabchun

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of hooker software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and hooker permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "windows.h"
#include "Hooker.h"

#pragma optimize("t", on)
extern "C"
{
	VOID __declspec(naked) MemoryCopy(VOID* dst, const VOID* src, DWORD len)
	{
		__asm {
			push ebp
			mov ebp, esp
			push esi
			push edi

			mov esi, src
			mov edi, dst
			mov ecx, len
			rep movsb
			
			pop edi
			pop esi
			mov esp, ebp
			pop ebp
			retn 12
		}
	}

	VOID __declspec(naked) MemorySet(VOID* dst, DWORD val, DWORD len)
	{
		__asm {
			push ebp
			mov ebp, esp
			push edi

			mov edi, dst
			mov eax, val
			mov ecx, len
			rep stosb
			
			pop edi
			mov esp, ebp
			pop ebp
			retn 12
		}
	}
}

#pragma optimize("s", on)
DWORD MapFile(HOOKER hooker)
{
	if (!hooker->mapAddress)
	{
		if (hooker->hFile == INVALID_HANDLE_VALUE)
		{
			CHAR filePath[MAX_PATH];
			GetModuleFileName(hooker->hModule, filePath, MAX_PATH);
			hooker->hFile = CreateFile(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hooker->hFile == INVALID_HANDLE_VALUE)
				return NULL;
		}

		if (!hooker->hMap)
		{
			hooker->hMap = CreateFileMapping(hooker->hFile, NULL, PAGE_READONLY, 0, 0, NULL);
			if (!hooker->hMap)
				return NULL;
		}

		hooker->mapAddress = (DWORD)MapViewOfFile(hooker->hMap, FILE_MAP_READ, 0, 0, 0);
	}

	return hooker->mapAddress;
}

VOID UnmapFile(HOOKER hooker)
{
	if (hooker->mapAddress && UnmapViewOfFile((VOID*)hooker->mapAddress))
		hooker->mapAddress = NULL;

	if (hooker->hMap && CloseHandle(hooker->hMap))
		hooker->hMap = NULL;

	if (hooker->hFile != INVALID_HANDLE_VALUE && CloseHandle(hooker->hFile))
		hooker->hFile = INVALID_HANDLE_VALUE;
}

VOID CreateInner(HOOKER hooker, HANDLE hHeap, HMODULE hModule)
{
	hooker->hHeap = hHeap;
	hooker->hModule = hModule;
	hooker->headNT = (PIMAGE_NT_HEADERS)((DWORD)hooker->hModule + ((PIMAGE_DOS_HEADER)hooker->hModule)->e_lfanew);
	
	hooker->hFile = INVALID_HANDLE_VALUE;
	hooker->hMap = NULL;
	hooker->mapAddress = NULL;

	if (MapFile(hooker))
	{
		PIMAGE_NT_HEADERS headNT = (PIMAGE_NT_HEADERS)(hooker->mapAddress + ((PIMAGE_DOS_HEADER)hooker->mapAddress)->e_lfanew);
		hooker->baseOffset = *(INT*)&hooker->hModule - *(INT*)&headNT->OptionalHeader.ImageBase;
		UnmapFile(hooker);
	}
	else
		hooker->baseOffset = *(INT*)&hooker->hModule - *(INT*)&hooker->headNT->OptionalHeader.ImageBase;
}

VOID ReleaseInner(HOOKER hooker)
{
	UnmapFile(hooker);
}

HOOKER CreateHooker(HMODULE hModule)
{
	if (hModule)
	{
		HANDLE hHeap = GetProcessHeap();
		if (hHeap)
		{
			HOOKER hooker = (HOOKER)HeapAlloc(hHeap, NULL, sizeof(Hooker));
			if (hooker)
			{
				CreateInner(hooker, hHeap, hModule);
				return hooker;
			}
		}
	}

	return NULL;
}

VOID ReleaseHooker(HOOKER hooker)
{
	ReleaseInner(hooker);
	HeapFree(hooker->hHeap, NULL, hooker);
}

DWORD GetBaseOffset(HOOKER hooker)
{
	return hooker ? hooker->baseOffset : 0;
}

HMODULE GetHookerModule(HOOKER hooker)
{
	return hooker->hModule;
}

BOOL ReadBlock(HOOKER hooker, DWORD addr, VOID* block, DWORD size)
{
	DWORD address = addr + hooker->baseOffset;

	DWORD old_prot;
	if (VirtualProtect((VOID*)address, size, PAGE_READONLY, &old_prot))
	{
		switch (size)
		{
		case 8:
			*(QWORD*)block = *(QWORD*)address;
			break;
		case 4:
			*(DWORD*)block = *(DWORD*)address;
			break;
		case 2:
			*(WORD*)block = *(WORD*)address;
			break;
		case 1:
			*(BYTE*)block = *(BYTE*)address;
			break;
		default:
			MemoryCopy(block, (VOID*)address, size);
			break;
		}

		VirtualProtect((VOID*)address, size, old_prot, &old_prot);

		return TRUE;
	}

	return FALSE;
}

BOOL ReadPtr(HOOKER hooker, DWORD addr, VOID** value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadByte(HOOKER hooker, DWORD addr, BYTE* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadWord(HOOKER hooker, DWORD addr, WORD* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadDWord(HOOKER hooker, DWORD addr, DWORD* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadQWord(HOOKER hooker, DWORD addr, QWORD* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadShort(HOOKER hooker, DWORD addr, SHORT* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadLong(HOOKER hooker, DWORD addr, LONG* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadLongLong(HOOKER hooker, DWORD addr, LONGLONG* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadFloat(HOOKER hooker, DWORD addr, FLOAT* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

BOOL ReadDouble(HOOKER hooker, DWORD addr, DOUBLE* value)
{
	return ReadBlock(hooker, addr, value, sizeof(*value));
}

DWORD FindBlock(HOOKER hooker, const VOID* block, DWORD size, DWORD flags, DWORD start)
{
	DWORD res = 0;

	IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(hooker->headNT);
	for (DWORD idx = 0; idx < hooker->headNT->FileHeader.NumberOfSections; ++idx, ++section)
	{
		if (!flags || (section->Characteristics & flags) == flags)
		{
			DWORD old_prot;
			DWORD startAddress = (DWORD)hooker->hModule + section->VirtualAddress;
			DWORD vsize = section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData;
			if (startAddress + vsize > start && VirtualProtect((VOID*)(startAddress), vsize, PAGE_EXECUTE_READWRITE, &old_prot))
			{
				DWORD entry = max(start, startAddress);
				DWORD total = vsize - size - entry + startAddress;
				do
				{
					BYTE* ptr1 = (BYTE*)entry;
					BYTE* ptr2 = (BYTE*)block;

					DWORD count = size;
					while (*ptr1++ == *ptr2++ && --count);

					if (!count)
					{
						res = (DWORD)entry - hooker->baseOffset;
						break;
					}

					++entry;
				} while (--total);

				VirtualProtect((VOID*)(startAddress), vsize, old_prot, &old_prot);
				if (res)
					break;
			}
		}
	}

	return res;
}

DWORD FindBlockByMask(HOOKER hooker, const VOID* block, const VOID* mask, DWORD size, DWORD flags, DWORD start)
{
	DWORD res = 0;

	IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(hooker->headNT);
	for (DWORD idx = 0; idx < hooker->headNT->FileHeader.NumberOfSections; ++idx, ++section)
	{
		if (!flags || (section->Characteristics & flags) == flags)
		{
			DWORD old_prot;
			DWORD startAddress = (DWORD)hooker->hModule + section->VirtualAddress;
			DWORD vsize = section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData;
			if (startAddress + vsize > start && VirtualProtect((VOID*)(startAddress), vsize, PAGE_EXECUTE_READWRITE, &old_prot))
			{
				DWORD entry = max(start, startAddress);
				DWORD total = vsize - size - entry + startAddress;
				do
				{
					BYTE* ptr1 = (BYTE*)entry;
					BYTE* ptr2 = (BYTE*)block;
					BYTE* msk = (BYTE*)mask;

					BYTE m;
					DWORD idx = 0;
					DWORD count = size;
					do
					{
						if (!(idx % 8))
							m = *msk++;
						else
							m >>= 1;

						if ((m & 1) && *ptr1 != *ptr2)
							break;

						++ptr1;
						++ptr2;
						++idx;
					} while (--count);

					if (!count)
					{
						res = (DWORD)entry - hooker->baseOffset;
						break;
					}

					++entry;
				} while (--total);

				VirtualProtect((VOID*)(startAddress), vsize, old_prot, &old_prot);
				if (res)
					break;
			}
		}
	}

	return res;
}

DWORD FindPtr(HOOKER hooker, const VOID* value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindByte(HOOKER hooker, BYTE value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindWord(HOOKER hooker, WORD value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindDWord(HOOKER hooker, DWORD value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindQWord(HOOKER hooker, QWORD value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindShort(HOOKER hooker, SHORT value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindLong(HOOKER hooker, LONG value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindLongLong(HOOKER hooker, LONGLONG value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindFloat(HOOKER hooker, FLOAT value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindDouble(HOOKER hooker, DOUBLE value, DWORD flags, DWORD start)
{
	return FindBlock(hooker, &value, sizeof(value), flags, start);
}

DWORD FindCall(HOOKER hooker, DWORD addr, DWORD flags, DWORD start)
{
	DWORD res = 0;
	BYTE block[5] = { 0xE8 };
	LONG* ptr = (LONG*)&block[1];
	addr += hooker->baseOffset;

	IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(hooker->headNT);
	for (DWORD idx = 0; idx < hooker->headNT->FileHeader.NumberOfSections; ++idx, ++section)
	{
		if (!flags || (section->Characteristics & flags) == flags)
		{
			DWORD old_prot;
			DWORD startAddress = (DWORD)hooker->hModule + section->VirtualAddress;
			DWORD vsize = section->Misc.VirtualSize ? section->Misc.VirtualSize : section->SizeOfRawData;
			if (startAddress + vsize > start && VirtualProtect((VOID*)(startAddress), vsize, PAGE_EXECUTE_READWRITE, &old_prot))
			{
				DWORD entry = max(start, startAddress);
				*ptr = *(LONG*)&addr - *(LONG*)&entry - sizeof(block);
				DWORD total = vsize - sizeof(block) - entry + startAddress;
				do
				{
					BYTE* ptr1 = (BYTE*)entry;
					BYTE* ptr2 = (BYTE*)block;

					DWORD count = sizeof(block);
					while (*ptr1++ == *ptr2++ && --count);

					if (!count)
					{
						res = (DWORD)entry - hooker->baseOffset;
						break;
					}

					++entry;
					--*ptr;
				} while (--total);

				VirtualProtect((VOID*)(startAddress), vsize, old_prot, &old_prot);
				if (res)
					break;
			}
		}
	}

	return res;
}

BOOL PatchRedirect(HOOKER hooker, DWORD addr, DWORD dest, RedirectType type, DWORD nop)
{
	DWORD address = addr + hooker->baseOffset;

	DWORD size = type == REDIRECT_JUMP_SHORT ? 2 : 5;

	DWORD old_prot;
	if (VirtualProtect((VOID*)address, size + nop, PAGE_EXECUTE_READWRITE, &old_prot))
	{
		BYTE* jump = (BYTE*)address;
		*jump = LOBYTE(type);
		++jump;

		LONG val = dest - address - size;
		if (type == REDIRECT_JUMP_SHORT)
			*(BYTE*)jump = *(BYTE*)&val;
		else
			*(DWORD*)jump = *(DWORD*)&val;

		if (nop)
			MemorySet((VOID*)(address + size), 0x90, nop);

		VirtualProtect((VOID*)address, size + nop, old_prot, &old_prot);

		return TRUE;
	}
	return FALSE;
}

BOOL PatchJump(HOOKER hooker, DWORD addr, DWORD dest)
{
	INT relative = dest - addr - hooker->baseOffset - 2;
	return PatchRedirect(hooker, addr, dest, relative >= -128 && relative <= 127 ? REDIRECT_JUMP_SHORT : REDIRECT_JUMP, 0);
}

BOOL PatchHook(HOOKER hooker, DWORD addr, const VOID* hook, DWORD nop)
{
	return PatchRedirect(hooker, addr, (DWORD)hook, REDIRECT_JUMP, nop);
}

BOOL PatchCall(HOOKER hooker, DWORD addr, const VOID* hook, DWORD nop)
{
	return PatchRedirect(hooker, addr, (DWORD)hook, REDIRECT_CALL, nop);
}

BOOL PatchSet(HOOKER hooker, DWORD addr, BYTE byte, DWORD size)
{
	DWORD address = addr + hooker->baseOffset;

	DWORD old_prot;
	if (VirtualProtect((VOID*)address, size, PAGE_EXECUTE_READWRITE, &old_prot))
	{
		MemorySet((VOID*)address, byte, size);
		VirtualProtect((VOID*)address, size, old_prot, &old_prot);

		return TRUE;
	}
	return FALSE;
}

BOOL PatchNop(HOOKER hooker, DWORD addr, DWORD size)
{
	return PatchSet(hooker, addr, 0x90, size);
}

BOOL PatchHex(HOOKER hooker, DWORD addr, const CHAR* block)
{
	const CHAR* ch = block;
	DWORD bt = 0;
	BOOL b = FALSE;
	DWORD size = 0;
	while (*ch)
	{
		if (*ch == ' ')
		{
			if (b)
			{
				bt = 0;
				++size;
				b = FALSE;
			}
		}
		else
		{
			if (bt > 0xF)
				return FALSE;

			bt <<= 4;
			if (*ch >= '0' && *ch <= '9')
				bt += *ch - '0';
			else if (*ch >= 'A' && *ch <= 'F')
				bt += *ch - 'A' + 10;
			else if (*ch >= 'a' && *ch <= 'f')
				bt += *ch - 'a' + 10;
			else
				return FALSE;

			b = TRUE;
		}

		++ch;
	}

	if (b)
		++size;
	else if (!size)
		return FALSE;

	DWORD address = addr + hooker->baseOffset;

	DWORD old_prot;
	if (VirtualProtect((VOID*)address, size, PAGE_EXECUTE_READWRITE, &old_prot))
	{
		BYTE* dst = (BYTE*)address;

		ch = block;
		bt = 0;
		b = FALSE;

		while (*ch)
		{
			if (*ch == ' ')
			{
				if (b)
				{
					*dst++ = LOBYTE(bt);
					bt = 0;
					b = FALSE;
				}
			}
			else
			{
				bt <<= 4;
				if (*ch >= '0' && *ch <= '9')
					bt += *ch - '0';
				else if (*ch >= 'A' && *ch <= 'F')
					bt += *ch - 'A' + 10;
				else
					bt += *ch - 'a' + 10;

				b = TRUE;
			}

			++ch;
		}

		if (b)
			*dst++ = LOBYTE(bt);

		VirtualProtect((VOID*)address, size, old_prot, &old_prot);

		return TRUE;
	}
	return FALSE;
}

BOOL PatchBlock(HOOKER hooker, DWORD addr, const VOID* block, DWORD size)
{
	DWORD address = addr + hooker->baseOffset;

	DWORD old_prot;
	if (VirtualProtect((VOID*)address, size, PAGE_EXECUTE_READWRITE, &old_prot))
	{
		switch (size)
		{
		case 8:
			*(QWORD*)address = *(QWORD*)block;
			break;
		case 4:
			*(DWORD*)address = *(DWORD*)block;
			break;
		case 2:
			*(WORD*)address = *(WORD*)block;
			break;
		case 1:
			*(BYTE*)address = *(BYTE*)block;
			break;
		default:
			MemoryCopy((VOID*)address, block, size);
			break;
		}

		VirtualProtect((VOID*)address, size, old_prot, &old_prot);

		return TRUE;
	}
	return FALSE;
}

BOOL PatchBlockByMask(HOOKER hooker, DWORD addr, const VOID* block, const VOID* mask, DWORD size)
{
	DWORD address = addr + hooker->baseOffset;

	DWORD old_prot;
	if (VirtualProtect((VOID*)address, size, PAGE_EXECUTE_READWRITE, &old_prot))
	{
		BYTE* src = (BYTE*)block;
		BYTE* dst = (BYTE*)address;
		BYTE* msk = (BYTE*)mask;

		BYTE m;
		DWORD idx = 0;
		DWORD count = size;
		do
		{
			if (!(idx % 8))
				m = *msk++;
			else
				m >>= 1;

			if (m & 1)
				*dst = *src;

			++src;
			++dst;
			++idx;
		} while (--count);

		VirtualProtect((VOID*)address, size, old_prot, &old_prot);

		return TRUE;
	}
	return FALSE;
}

BOOL PatchPtr(HOOKER hooker, DWORD addr, const VOID* value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchByte(HOOKER hooker, DWORD addr, BYTE value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchWord(HOOKER hooker, DWORD addr, WORD value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchDWord(HOOKER hooker, DWORD addr, DWORD value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchQWord(HOOKER hooker, DWORD addr, QWORD value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchShort(HOOKER hooker, DWORD addr, SHORT value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchLong(HOOKER hooker, DWORD addr, LONG value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchLongLong(HOOKER hooker, DWORD addr, LONGLONG value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchFloat(HOOKER hooker, DWORD addr, FLOAT value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

BOOL PatchDouble(HOOKER hooker, DWORD addr, DOUBLE value)
{
	return PatchBlock(hooker, addr, &value, sizeof(value));
}

DWORD PatchAllBlocks(HOOKER hooker, const VOID* old_block, const VOID* new_block, DWORD size, DWORD flags)
{
	DWORD count = 0;
	for (DWORD found = FindBlock(hooker, old_block, size, flags); found; found = FindBlock(hooker, old_block, size, flags, found + size), ++count)
		PatchBlock(hooker, found, new_block, size);

	return count;
}

DWORD PatchAllBlocksByMask(HOOKER hooker, const VOID* old_block, const VOID* old_mask, const VOID* new_block, const VOID* new_mask, DWORD size, DWORD flags)
{
	DWORD count = 0;
	for (DWORD found = FindBlockByMask(hooker, old_block, old_mask, size, flags); found; found = FindBlockByMask(hooker, old_block, old_mask, size, flags, found + size), ++count)
		PatchBlockByMask(hooker, found, new_block, new_mask, size);

	return count;
}

DWORD PatchAllPtrs(HOOKER hooker, const VOID* old_value, const VOID* new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllBytes(HOOKER hooker, BYTE old_value, BYTE new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllWords(HOOKER hooker, WORD old_value, WORD new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllDWords(HOOKER hooker, DWORD old_value, DWORD new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllQWords(HOOKER hooker, QWORD old_value, QWORD new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllShorts(HOOKER hooker, SHORT old_value, SHORT new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllLongs(HOOKER hooker, LONG old_value, LONG new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllLongLongs(HOOKER hooker, LONGLONG old_value, LONGLONG new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllFloats(HOOKER hooker, FLOAT old_value, FLOAT new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD PatchAllDoubles(HOOKER hooker, DOUBLE old_value, DOUBLE new_value, DWORD flags)
{
	return PatchAllBlocks(hooker, &old_value, &new_value, sizeof(new_value), flags);
}

DWORD RedirectCall(HOOKER hooker, DWORD addr, const VOID* hook)
{
	BYTE block[5];
	if (ReadBlock(hooker, addr, block, sizeof(block)) && block[0] == 0xE8 && PatchCall(hooker, addr, hook))
		return addr + 5 + *(DWORD*)&block[1] + hooker->baseOffset;

	return NULL;
}

BOOL PatchVirtual(const VOID* obj, DWORD index, const VOID* funct, VOID* old_value)
{
	BOOL res = FALSE;

	DWORD* address = &((DWORD**)obj)[0][index];
	DWORD old_prot;
	if (VirtualProtect(address, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &old_prot))
	{
		if (*address != (DWORD)funct)
		{
			if (old_value)
				*(DWORD*)old_value = *address;
			*address = (DWORD)funct;
		}

		res = TRUE;
		VirtualProtect(address, sizeof(DWORD), old_prot, &old_prot);
	}

	return res;
}

DWORD RedirectAllCalls(HOOKER hooker, DWORD addr, const VOID* hook, DWORD flags)
{
	DWORD count = 0;
	for (DWORD found = FindCall(hooker, addr, flags); found; found = FindCall(hooker, addr, flags, found + 5), ++count)
		PatchCall(hooker, found, hook);

	return count;
}

DWORD PatchImport(HOOKER hooker, DWORD function, const VOID* addr, DWORD* old_val, BOOL erace)
{
	if (old_val)
		*old_val = NULL;

	PIMAGE_DATA_DIRECTORY dataDir = &hooker->headNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (dataDir->Size)
	{
		for (PIMAGE_IMPORT_DESCRIPTOR imports = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)hooker->hModule + dataDir->VirtualAddress); imports->Name; ++imports)
		{
			PIMAGE_THUNK_DATA addressThunk = (PIMAGE_THUNK_DATA)((DWORD)hooker->hModule + imports->FirstThunk);
			PIMAGE_THUNK_DATA nameThunk;

			DWORD nameInternal = imports->OriginalFirstThunk;
			if (nameInternal)
				nameThunk = (PIMAGE_THUNK_DATA)((DWORD)hooker->hModule + nameInternal);
			else if (MapFile(hooker))
			{
				PIMAGE_NT_HEADERS headNT = (PIMAGE_NT_HEADERS)(hooker->mapAddress + ((PIMAGE_DOS_HEADER)hooker->mapAddress)->e_lfanew);
				PIMAGE_SECTION_HEADER sh = (PIMAGE_SECTION_HEADER)((DWORD)&headNT->OptionalHeader + headNT->FileHeader.SizeOfOptionalHeader);

				nameThunk = NULL;
				DWORD sCount = headNT->FileHeader.NumberOfSections;
				while (sCount--)
				{
					if (imports->FirstThunk >= sh->VirtualAddress && imports->FirstThunk < sh->VirtualAddress + (sh->Misc.VirtualSize ? sh->Misc.VirtualSize : sh->SizeOfRawData))
					{
						nameThunk = PIMAGE_THUNK_DATA((DWORD)hooker->mapAddress + sh->PointerToRawData + imports->FirstThunk - sh->VirtualAddress);
						break;
					}

					++sh;
				}

				if (!nameThunk)
					return NULL;
			}
			else
				return NULL;

			for (; nameThunk->u1.AddressOfData; ++nameThunk, ++addressThunk)
			{
				if ((nameThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32) == (function & IMAGE_ORDINAL_FLAG32))
				{
					PIMAGE_IMPORT_BY_NAME name = PIMAGE_IMPORT_BY_NAME((DWORD)hooker->hModule + (DWORD)nameThunk->u1.AddressOfData);
					
					WORD hint;
					BOOL isOrdianl = nameThunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32;
					if (isOrdianl && nameThunk->u1.Ordinal == function ||
						!isOrdianl && ReadWord(hooker, (DWORD)&name->Hint - hooker->baseOffset, &hint) && !lstrcmp((CHAR*)&name->Name, (const CHAR*)function))
					{
						DWORD res;
						DWORD address = (DWORD)&addressThunk->u1.AddressOfData - hooker->baseOffset;
						if (ReadDWord(hooker, address, &res))
						{
							if (addr)
							{
								if (PatchPtr(hooker, address, addr))
								{
									if (nameInternal && erace)
										PatchSet(hooker, (DWORD)name->Name - hooker->baseOffset, NULL, 1);

									goto lbl_sucess;
								}
							}
							else
							{
								lbl_sucess:;
								if (old_val)
									*old_val = res;
								return address;
							}
						}
						
						return NULL;
					}
				}
			}
		}
	}

	return NULL;
}

DWORD PatchImportByName(HOOKER hooker, const CHAR* function, const VOID* addr, DWORD* old_val, BOOL erace)
{
	return PatchImport(hooker, (DWORD)function, addr, old_val, erace);
}

DWORD PatchImportByOrdinal(HOOKER hooker, DWORD ordinal, const VOID* addr, DWORD* old_val, BOOL erace)
{
	return PatchImport(hooker, ordinal | IMAGE_ORDINAL_FLAG32, addr, old_val, erace);
}

DWORD PatchExport(HOOKER hooker, const CHAR* function, const VOID* addr, DWORD* old_val)
{
	if (old_val)
		*old_val = NULL;

	DWORD func = (DWORD)GetProcAddress(hooker->hModule, function);
	if (func)
	{
		PIMAGE_DATA_DIRECTORY dataDir = &hooker->headNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		if (dataDir->Size)
		{
			PIMAGE_EXPORT_DIRECTORY exports = (PIMAGE_EXPORT_DIRECTORY)((DWORD)hooker->hModule + dataDir->VirtualAddress);
			{
				DWORD* functions = (DWORD*)((DWORD)hooker->hModule + exports->AddressOfFunctions);

				DWORD count = exports->NumberOfFunctions;
				while (count--)
				{
					DWORD res;
					if (func == (DWORD)hooker->hModule + *functions &&
						ReadDWord(hooker, (DWORD)functions - hooker->baseOffset, &res) &&
						(!addr || PatchDWord(hooker, (DWORD)functions - hooker->baseOffset, (DWORD)addr - (DWORD)hooker->hModule)))
					{
						if (old_val)
							*old_val = (DWORD)hooker->hModule + res;

						return (DWORD)functions - hooker->baseOffset;
					}

					++functions;
				}
			}
		}
	}

	return NULL;
}

DWORD PatchEntry(HOOKER hooker, const VOID* entryPoint)
{
	DWORD res = (DWORD)hooker->hModule + hooker->headNT->OptionalHeader.AddressOfEntryPoint - hooker->baseOffset;
	if (!entryPoint || PatchHook(hooker, res, entryPoint))
		return res;

	return NULL;
}

BOOL RedirectImports(HOOKER hooker, const CHAR* libName, HMODULE hLib)
{
	BOOL res = FALSE;

	PIMAGE_DATA_DIRECTORY dataDir = &hooker->headNT->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (dataDir->Size)
	{
		for (PIMAGE_IMPORT_DESCRIPTOR imports = (PIMAGE_IMPORT_DESCRIPTOR)((DWORD)hooker->hModule + dataDir->VirtualAddress); imports->Name; ++imports)
		{
			CHAR* libraryName = (CHAR*)((DWORD)hooker->hModule + imports->Name);
			if (!lstrcmpi(libraryName, libName))
			{
				if (hLib)
				{
					PIMAGE_THUNK_DATA addressThunk = (PIMAGE_THUNK_DATA)((DWORD)hooker->hModule + imports->FirstThunk);
					PIMAGE_THUNK_DATA nameThunk;

					DWORD nameInternal = imports->OriginalFirstThunk;
					if (nameInternal)
						nameThunk = (PIMAGE_THUNK_DATA)((DWORD)hooker->hModule + nameInternal);
					else if (MapFile(hooker))
					{
						PIMAGE_NT_HEADERS headNT = (PIMAGE_NT_HEADERS)((DWORD)hooker->mapAddress + ((PIMAGE_DOS_HEADER)hooker->mapAddress)->e_lfanew);
						PIMAGE_SECTION_HEADER sh = (PIMAGE_SECTION_HEADER)((DWORD)&headNT->OptionalHeader + headNT->FileHeader.SizeOfOptionalHeader);

						nameThunk = NULL;
						DWORD sCount = headNT->FileHeader.NumberOfSections;
						while (sCount--)
						{
							if (imports->FirstThunk >= sh->VirtualAddress && imports->FirstThunk < sh->VirtualAddress + (sh->Misc.VirtualSize ? sh->Misc.VirtualSize : sh->SizeOfRawData))
							{
								nameThunk = PIMAGE_THUNK_DATA((DWORD)hooker->mapAddress + sh->PointerToRawData + imports->FirstThunk - sh->VirtualAddress);
								break;
							}

							++sh;
						}

						if (!nameThunk)
							return FALSE;
					}
					else
						return FALSE;

					for (; nameThunk->u1.AddressOfData; ++nameThunk, ++addressThunk)
					{
						PIMAGE_IMPORT_BY_NAME name = PIMAGE_IMPORT_BY_NAME((DWORD)hooker->hModule + (DWORD)nameThunk->u1.AddressOfData);

						WORD hint;
						if (ReadWord(hooker, (DWORD)&name->Hint - hooker->baseOffset, &hint))
						{
							DWORD old;
							DWORD address = (DWORD)&addressThunk->u1.AddressOfData - hooker->baseOffset;
							if (ReadDWord(hooker, address, &old))
							{
								FARPROC addr = GetProcAddress(hLib, (CHAR*)name->Name);
								if (addr && PatchPtr(hooker, address, addr))
									res = TRUE;
							}
						}
					}
				}
				else
					res = TRUE;
			}
		}
	}
	
	return res;
}
#pragma optimize("", on)
