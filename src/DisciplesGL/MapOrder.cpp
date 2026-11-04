#include "StdAfx.h"
#include "Hooks.h"
#include "ImageReplace.h"
#include "MapOrder.h"

#define f(a) (a + baseOffset)

namespace MapOrder
{
	DWORD sub_MapKey;
	const DWORD* treesLayer;
	const DWORD* stacksLayer;
	const DWORD* mapOwner;

	// Discipl2.exe v3.01b 0x00545A58: elements draw in ascending layer + (x + y) * 512; a forest tile's back trees stand level with a stack one diagonal behind, so trees sort a diagonal earlier
	DWORD __stdcall MapKey(DWORD layer, const POINT* pos)
	{
		if (layer != *treesLayer || pos->x + pos->y <= 0)
			return ((DWORD(__stdcall*)(DWORD, const POINT*))sub_MapKey)(layer, pos);

		POINT behind = { pos->x, pos->y - 1 };
		return ((DWORD(__stdcall*)(DWORD, const POINT*))sub_MapKey)(layer, &behind);
	}

	// Discipl2.exe v2.02b 0x0053C5F3: the add caller keeps its this in ebx, and the engine map is [[[[this]] + 4]]
	DWORD __stdcall MapKeyAdd(DWORD layer, const POINT* pos)
	{
		const DWORD* owner;
		__asm mov owner, ebx
		if (stacksLayer && layer == *stacksLayer)
			mapOwner = owner;
		return MapKey(layer, pos);
	}

	const BYTE* ReadPtr(const VOID* at)
	{
		return at && !IsBadReadPtr(at, 4) ? *(const BYTE* const*)at : NULL;
	}

	VOID Dump()
	{
		if (!mapOwner || !stacksLayer)
		{
			DebugLog("map owner not captured");
			return;
		}

		const BYTE* holder = ReadPtr(ReadPtr(mapOwner));
		const BYTE* map = holder ? ReadPtr(ReadPtr(holder + 4)) : NULL;
		const BYTE* data = map ? ReadPtr(map + 4) : NULL;
		const BYTE* begin = data ? ReadPtr(data + 0x34) : NULL;
		const BYTE* end = data ? ReadPtr(data + 0x38) : NULL;
		if (!begin || end < begin || IsBadReadPtr(begin, end - begin))
			return;

		DWORD stacks = 0, counts[2] = {};
		for (const BYTE* s = begin; s < end; s += 0x2C)
		{
			DWORD layer = *(const DWORD*)(s + 0x14) & 0x1FF;
			counts[0] += layer == *stacksLayer;
			counts[1] += layer == *treesLayer;
		}
		DebugLog("map elements=%d stacksLayer=%d treesLayer=%d stacks=%d trees=%d", (end - begin) / 0x2C, *stacksLayer, *treesLayer, counts[0], counts[1]);

		for (const BYTE* s = begin; s < end; s += 0x2C)
		{
			DWORD hash = *(const DWORD*)(s + 0x14);
			if ((hash & 0x1FF) != *stacksLayer)
				continue;

			const LONG* box = (const LONG*)(s + 0x18);
			const BYTE* first = ReadPtr(s);
			DebugLog("map stack %d row=%d box=%d,%d,%d,%d img=%s", stacks++, hash >> 9, box[0], box[1], box[2], box[3], first ? ImageReplace::ImageName(ReadPtr(first)) : "?");
			for (const BYTE* o = s + 0x2C; o < end; o += 0x2C)
			{
				const LONG* other = (const LONG*)(o + 0x18);
				if (other[0] >= box[2] || other[2] <= box[0] || other[1] >= box[3] || other[3] <= box[1])
					continue;

				DWORD key = *(const DWORD*)(o + 0x14);
				const BYTE* item = ReadPtr(o);
				const BYTE* last = ReadPtr(o + 4);
				for (; item && item < last && !IsBadReadPtr(item, 24); item += 24)
				{
					const LONG* at = (const LONG*)(item + 4);
					const VOID* image = ReadPtr(item);
					DebugLog("  over layer=%d row=%d box=%d,%d,%d,%d pos=%d,%d size=%d,%d img=%s vtbl=%08X", key & 0x1FF, key >> 9, other[0], other[1], other[2], other[3], at[0], at[1], at[2], at[3], ImageReplace::ImageName(image), image ? (DWORD)ReadPtr(image) : 0);
				}
			}
		}
		DebugLog("map stacks dumped %d", stacks);
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!hookSpace->map_key_add || !hookSpace->map_trees_layer)
			return;

		treesLayer = (const DWORD*)f(hookSpace->map_trees_layer);
		stacksLayer = hookSpace->map_stacks_layer ? (const DWORD*)f(hookSpace->map_stacks_layer) : NULL;
		sub_MapKey = RedirectCall(hooker, hookSpace->map_key_add, MapKeyAdd);
		if (!sub_MapKey || !RedirectCall(hooker, hookSpace->map_key_change, MapKey))
			DebugLog("map order patch failed at %08X %08X", hookSpace->map_key_add, hookSpace->map_key_change);
	}
}
