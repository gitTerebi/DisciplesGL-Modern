#include "StdAfx.h"
#include "Hooks.h"
#include "BatchTextures.h"

#define f(a) (a + baseOffset)

namespace BatchTextures
{
	#define ENTRY_SIZE 0x14
	#define TABLE_BITS 14
	#define TABLE_SIZE (1 << TABLE_BITS)
	#define TABLE_LIMIT (TABLE_SIZE / 2)
	#define APPEND_OFFSET 0x48
	#define SKIP_OFFSET 0x74

	const BYTE searchCode[APPEND_OFFSET] = {
		0x8B, 0x4D, 0x04, 0x8B, 0x45, 0x00, 0x8D, 0x14, 0x89, 0x8D, 0x3C, 0x90, 0x3B, 0xC7, 0x74, 0x38,
		0x8B, 0x4C, 0x24, 0x1C, 0x8B, 0x49, 0x08, 0x8B, 0x50, 0x08, 0x85, 0xD2, 0x74, 0x04, 0x8B, 0x12,
		0xEB, 0x02, 0x33, 0xD2, 0x85, 0xC9, 0x74, 0x04, 0x8B, 0x31, 0xEB, 0x02, 0x33, 0xF6, 0x33, 0xDB,
		0x3B, 0xD6, 0x0F, 0x94, 0xC3, 0x84, 0xDB, 0x75, 0x07, 0x83, 0xC0, 0x14, 0x3B, 0xC7, 0x75, 0xD7,
		0x8B, 0x5C, 0x24, 0x10, 0x3B, 0xC7, 0x75, 0x2C
	};

	struct TextureList {
		BYTE* data;
		DWORD count;
		DWORD capacity;
	};

	struct Slot {
		DWORD key;
		DWORD index;
	};

	Slot table[TABLE_SIZE];
	DWORD used[TABLE_LIMIT];
	DWORD usedCount, indexedCount;
	const BYTE* indexedData;
	DWORD appendAddress, skipAddress;

	DWORD Key(const BYTE* handleField)
	{
		const DWORD* handle = *(const DWORD* const*)handleField;
		return handle ? *handle : 0;
	}

	DWORD Hash(DWORD key)
	{
		return (key * 2654435761u) >> (32 - TABLE_BITS);
	}

	BYTE* Linear(const TextureList* list, DWORD key)
	{
		for (DWORD i = 0; i < list->count; ++i)
		{
			BYTE* entry = list->data + i * ENTRY_SIZE;
			if (Key(entry + 8) == key)
				return entry;
		}
		return NULL;
	}

	VOID Reset(const BYTE* data)
	{
		for (DWORD i = 0; i < usedCount; ++i)
			table[used[i]].index = 0;
		usedCount = indexedCount = 0;
		indexedData = data;
	}

	BYTE* Lookup(const TextureList* list, DWORD key)
	{
		if (list->data != indexedData || list->count < indexedCount || !list->count)
			Reset(list->data);

		if (list->count > TABLE_LIMIT)
			return Linear(list, key);

		for (; indexedCount < list->count; ++indexedCount)
		{
			DWORD entryKey = Key(list->data + indexedCount * ENTRY_SIZE + 8);
			DWORD slot = Hash(entryKey);
			while (table[slot].index && table[slot].key != entryKey)
				slot = (slot + 1) & (TABLE_SIZE - 1);
			if (!table[slot].index)
			{
				table[slot].key = entryKey;
				table[slot].index = indexedCount + 1;
				used[usedCount++] = slot;
			}
		}

		for (DWORD slot = Hash(key); table[slot].index; slot = (slot + 1) & (TABLE_SIZE - 1))
			if (table[slot].key == key)
				return list->data + (table[slot].index - 1) * ENTRY_SIZE;
		return NULL;
	}

	BYTE* __fastcall Find(const TextureList* list, const BYTE* item)
	{
		return Lookup(list, Key(item + 8));
	}

	// Discipl2.exe v2.02b 0x00514633: ebp = texture list, [esp+0x1C] = sprite handle block
	VOID __declspec(naked) SearchHook()
	{
		__asm {
			mov ecx, ebp
			mov edx, [esp+0x1C]
			call Find
			mov ecx, [ebp+4]
			mov edi, [ebp]
			lea ecx, [ecx+ecx*4]
			lea edi, [edi+ecx*4]
			test eax, eax
			jz lbl_append
			jmp skipAddress

			lbl_append:
			jmp appendAddress
		}
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!hookSpace->batch_texture_search)
			return;

		BYTE code[APPEND_OFFSET];
		if (!ReadBlock(hooker, hookSpace->batch_texture_search, code, sizeof(code)) || MemoryCompare(code, searchCode, sizeof(code)))
		{
			DebugLog("BatchTextures: search code at %08X differs, not patched", hookSpace->batch_texture_search);
			return;
		}

		appendAddress = f(hookSpace->batch_texture_search + APPEND_OFFSET);
		skipAddress = f(hookSpace->batch_texture_search + SKIP_OFFSET);
		PatchHook(hooker, hookSpace->batch_texture_search, SearchHook);
		DebugLog("BatchTextures: patched search at %08X", hookSpace->batch_texture_search);
	}
}
