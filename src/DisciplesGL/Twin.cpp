#include "StdAfx.h"
#include "intrin.h"
#include "Config.h"
#include "Ground.h"
#include "Pack.h"
#include "Seed.h"
#include "Twin.h"

#define ARENA_LIMIT (64 * 1024 * 1024)
#define TWIN_BUDGET (512LL * 1024 * 1024)
#define TWIN_BUDGET_SCREENS 12
#define TWIN_MAX_SIZE 8192
#define MEMORY_BUDGET (256LL * 1024 * 1024)
#define POOL_MAX 4
#define POOL_KEEP (8 * 1024 * 1024)
#define FIND_CACHE 256
#define ERROR_LOGS 16

namespace Twin
{
	Shared shared = { {}, 0, 0, TRUE, FALSE, FALSE };

	struct {
		BOOL chosen;
		LONG scale;
		Record twins[TWIN_MAX];
		DWORD top, lastOwner, lastAddress;
		DWORD findCache[FIND_CACHE];
		List* list;
		LONG seenGen;
		DWORD frameNo, stamp;
		LONGLONG memoryBytes;
		DWORD copyCaller;
		BOOL showKey;
		DWORD errors;
	} game;

	VOID Lock()
	{
		if (shared.state != 2)
		{
			if (!InterlockedCompareExchange(&shared.state, 1, 0))
			{
				InitializeCriticalSection(&shared.cs);
				InterlockedExchange(&shared.state, 2);
			}
			else
				while (shared.state != 2)
					Sleep(0);
		}

		EnterCriticalSection(&shared.cs);
	}

	VOID Unlock()
	{
		LeaveCriticalSection(&shared.cs);
	}

	VOID RecycleList(List* list)
	{
		if (shared.poolCount < POOL_MAX && list->arenaCapacity <= POOL_KEEP)
		{
			list->next = shared.pool;
			shared.pool = list;
			++shared.poolCount;
			return;
		}
		if (list->cmds)
			MemoryFree(list->cmds);
		if (list->arena)
			MemoryFree(list->arena);
		MemoryFree(list);
	}

	VOID DropQueued()
	{
		while (shared.head)
		{
			List* list = shared.head;
			shared.head = list->next;
			RecycleList(list);
		}
		shared.tail = NULL;
		shared.bytes = 0;
	}

	BOOL Active()
	{
		return config.renderer != RendererGDI && config.gl.version.value >= GL_VER_3_0 && config.bpp32Hooked && !config.type.editor && Pack::Present();
	}

	List* CurrentList()
	{
		if (game.list)
			return game.list;

		Lock();
		List* list = shared.pool;
		if (list)
		{
			shared.pool = list->next;
			--shared.poolCount;
		}
		Unlock();

		if (!list)
		{
			list = (List*)MemoryAlloc(sizeof(List));
			if (!list)
				return NULL;
			MemoryZero(list, sizeof(List));
		}

		list->next = NULL;
		list->count = 0;
		list->size = 0;
		game.list = list;
		return list;
	}

	Cmd* Emit(DWORD type)
	{
		List* list = CurrentList();
		if (!list)
			return NULL;

		if (list->count == list->capacity)
		{
			DWORD capacity = list->capacity ? list->capacity * 2 : 1024;
			Cmd* cmds = (Cmd*)MemoryAlloc(capacity * sizeof(Cmd));
			if (!cmds)
				return NULL;
			if (list->cmds)
			{
				MemoryCopy(cmds, list->cmds, list->count * sizeof(Cmd));
				MemoryFree(list->cmds);
			}
			list->cmds = cmds;
			list->capacity = capacity;
		}

		Cmd* cmd = &list->cmds[list->count++];
		MemoryZero(cmd, sizeof(Cmd));
		cmd->type = type;
		return cmd;
	}

	BYTE* Reserve(DWORD bytes, DWORD* offset)
	{
		List* list = CurrentList();
		if (!list)
			return NULL;

		if (list->size + bytes > list->arenaCapacity)
		{
			DWORD capacity = list->arenaCapacity ? list->arenaCapacity : 1024 * 1024;
			while (capacity < list->size + bytes)
				capacity *= 2;
			BYTE* arena = (BYTE*)MemoryAlloc(capacity);
			if (!arena)
				return NULL;
			if (list->arena)
			{
				MemoryCopy(arena, list->arena, list->size);
				MemoryFree(list->arena);
			}
			list->arena = arena;
			list->arenaCapacity = capacity;
		}

		*offset = list->size;
		list->size += bytes;
		return list->arena + *offset;
	}

	DWORD ListBytes()
	{
		return game.list ? game.list->size : 0;
	}

	LONG Area(const RECT* rect)
	{
		return (rect->right - rect->left) * (rect->bottom - rect->top);
	}

	BOOL Contains(const RECT* outer, const RECT* inner)
	{
		return inner->left >= outer->left && inner->top >= outer->top && inner->right <= outer->right && inner->bottom <= outer->bottom;
	}

	BOOL Clip(const Record* twin, RECT* rect)
	{
		if (rect->left < 0)
			rect->left = 0;
		if (rect->top < 0)
			rect->top = 0;
		if (rect->right > twin->width)
			rect->right = twin->width;
		if (rect->bottom > twin->height)
			rect->bottom = twin->height;
		return rect->left < rect->right && rect->top < rect->bottom;
	}

	Record* At(DWORD id)
	{
		return &game.twins[id];
	}

	VOID Touched(Record* twin, const RECT* rect)
	{
		twin->stamp = ++game.stamp;
		if (twin->seeds)
			Seed::Write(twin, rect);
	}

	DWORD Stamp(const VOID* data)
	{
		DWORD id;
		Record* twin = FindAddress(data, &id);
		return twin ? twin->stamp : 0;
	}

	VOID MarkAll(Record* twin)
	{
		twin->count = 1;
		SetRect(&twin->dirty[0], 0, 0, twin->width, twin->height);
	}

	VOID Append(Record* twin, const RECT* rect)
	{
		if (twin->count == DIRTY_MAX)
		{
			RECT pending[DIRTY_MAX];
			DWORD count = twin->count;
			MemoryCopy(pending, twin->dirty, count * sizeof(RECT));
			twin->count = 0;
			for (DWORD i = 0; i < count; ++i)
				Upload(twin, DWORD(twin - game.twins), &pending[i], FALSE);
		}

		twin->dirty[twin->count++] = *rect;
	}

	VOID Split(Record* twin, const RECT* dirty, const RECT* part)
	{
		RECT pieces[4] = {
			{ dirty->left, dirty->top, dirty->right, part->top },
			{ dirty->left, part->bottom, dirty->right, dirty->bottom },
			{ dirty->left, part->top, part->left, part->bottom },
			{ part->right, part->top, dirty->right, part->bottom }
		};
		for (DWORD k = 0; k < 4; ++k)
			if (pieces[k].left < pieces[k].right && pieces[k].top < pieces[k].bottom)
				Append(twin, &pieces[k]);
	}

	VOID CutRegion(Record* twin, const RECT* rect)
	{
		DWORD i = 0;
		while (i < twin->count)
		{
			RECT part;
			if (!IntersectRect(&part, &twin->dirty[i], rect))
			{
				++i;
				continue;
			}

			RECT dirty = twin->dirty[i];
			twin->dirty[i] = twin->dirty[--twin->count];
			Split(twin, &dirty, &part);
			i = 0;
		}
	}

	VOID MarkDirty(Record* twin, RECT rect)
	{
		if (!Clip(twin, &rect))
			return;

		Touched(twin, &rect);

		BOOL merged;
		do
		{
			merged = FALSE;
			for (DWORD i = 0; i < twin->count; ++i)
			{
				RECT* dirty = &twin->dirty[i];
				if (Contains(dirty, &rect))
					return;

				RECT bound;
				UnionRect(&bound, dirty, &rect);
				if ((LONGLONG)Area(&bound) * 4 <= ((LONGLONG)Area(dirty) + Area(&rect)) * 5)
				{
					rect = bound;
					twin->dirty[i] = twin->dirty[--twin->count];
					merged = TRUE;
					break;
				}
			}
		} while (merged);

		Append(twin, &rect);
	}

	VOID Cover(Record* twin, const RECT* rect)
	{
		for (DWORD i = twin->count; i; --i)
			if (Contains(rect, &twin->dirty[i - 1]))
				twin->dirty[i - 1] = twin->dirty[--twin->count];
	}

	VOID Convert(const Record* twin, DWORD* dst, const DWORD* src, LONG width, LONG height, BOOL seed)
	{
		BOOL keyed = twin->keyed;
		DWORD key = twin->key;
		BOOL wide = config.cpu.sse2 && width >= 4;
		__m128i vKey = _mm_set1_epi32((INT)key);
		__m128i vCheck = _mm_set1_epi32((INT)COLORKEY_CHECK);
		__m128i vLow = _mm_set1_epi32(0x00FFFFFF);
		__m128i vMark = _mm_set1_epi32((INT)0xFE000000);
		__m128i vTop = _mm_set1_epi32(0xFF);
		for (LONG y = height; y; --y)
		{
			LONG x = 0;
			if (wide)
			{
				for (; x + 4 <= width; x += 4)
				{
					__m128i p = _mm_loadu_si128((const __m128i*)(src + x));
					__m128i full = _mm_cmpeq_epi32(_mm_srli_epi32(p, 24), vTop);
					__m128i marked = _mm_or_si128(_mm_and_si128(p, vLow), vMark);
					__m128i out = _mm_or_si128(_mm_and_si128(full, p), _mm_andnot_si128(full, marked));
					__m128i drop = _mm_setzero_si128();
					if (keyed)
						drop = _mm_cmpeq_epi32(p, vKey);
					if (seed)
						drop = _mm_or_si128(drop, _mm_cmpeq_epi32(p, vCheck));
					_mm_storeu_si128((__m128i*)(dst + x), _mm_andnot_si128(drop, out));
				}
				dst += x;
			}
			for (; x < width; ++x)
			{
				DWORD pixel = src[x];
				if ((keyed && pixel == key) || (seed && pixel == COLORKEY_CHECK))
					*dst++ = 0;
				else
					*dst++ = pixel >= ALPHA_COMPONENT ? pixel : (pixel & 0x00FFFFFF) | 0xFE000000;
			}
			src += twin->pitch;
		}
	}

	VOID AddArt(Record* twin, const RECT* rect)
	{
		if (rect->left >= rect->right || rect->top >= rect->bottom)
			return;

		for (DWORD i = 0; i < twin->artCount; ++i)
			if (Contains(&twin->art[i], rect))
				return;

		RECT bound = *rect;
		for (DWORD i = twin->artCount; i; --i)
			if (Contains(rect, &twin->art[i - 1]) || twin->artCount == ART_MAX)
			{
				UnionRect(&bound, &bound, &twin->art[i - 1]);
				twin->art[i - 1] = twin->art[--twin->artCount];
			}
		twin->art[twin->artCount++] = bound;
	}

	BOOL Carries4K(const Record* twin, const RECT* rect)
	{
		RECT part;
		for (DWORD i = 0; i < twin->artCount; ++i)
			if (IntersectRect(&part, &twin->art[i], rect))
				return TRUE;
		return FALSE;
	}

	VOID CoverArt(Record* twin, const RECT* rect)
	{
		for (DWORD i = twin->artCount; i; --i)
			if (Contains(rect, &twin->art[i - 1]))
				twin->art[i - 1] = twin->art[--twin->artCount];
	}

	VOID CarryArt(Record* target, const RECT* to, const Record* source, const RECT* from, BOOL mirror)
	{
		RECT part;
		for (DWORD i = 0; i < source->artCount; ++i)
			if (IntersectRect(&part, &source->art[i], from))
			{
				if (mirror)
					SetRect(&part, to->left + from->right - part.right, part.top, to->left + from->right - part.left, part.bottom);
				else
					OffsetRect(&part, to->left - from->left, 0);
				OffsetRect(&part, 0, to->top - from->top);
				AddArt(target, &part);
			}
	}

	VOID Upload(Record* twin, DWORD id, const RECT* rect, BOOL seed)
	{
		LONG width = rect->right - rect->left;
		LONG height = rect->bottom - rect->top;

		DWORD offset;
		DWORD* dst = (DWORD*)Reserve(width * height * sizeof(DWORD), &offset);
		Cmd* cmd = dst ? Emit(CmdUpload) : NULL;
		if (!cmd)
			return;
		Convert(twin, dst, (const DWORD*)twin->base + rect->top * twin->pitch + rect->left, width, height, seed);
		cmd->dst = id;
		cmd->rect = *rect;
		cmd->value = offset;
		CoverArt(twin, rect);
	}

	VOID FlushRegion(Record* twin, DWORD id, const RECT* rect, BOOL whole)
	{
		DWORD i = 0;
		while (i < twin->count)
		{
			RECT part;
			if (!IntersectRect(&part, &twin->dirty[i], rect))
			{
				++i;
				continue;
			}

			RECT dirty = twin->dirty[i];
			twin->dirty[i] = twin->dirty[--twin->count];
			if (whole || Area(&part) * 2 >= Area(&dirty))
				Upload(twin, id, &dirty, FALSE);
			else
			{
				Upload(twin, id, &part, FALSE);
				Split(twin, &dirty, &part);
			}
			i = 0;
		}
	}

	Record* Find(StateBuffer* buffer, DWORD* id)
	{
		if (!buffer)
			return NULL;

		if (game.lastOwner < game.top && game.twins[game.lastOwner].owner == buffer)
		{
			*id = game.lastOwner;
			return &game.twins[game.lastOwner];
		}

		DWORD slot = ((DWORD)buffer >> 4) & (FIND_CACHE - 1);
		DWORD cached = game.findCache[slot];
		if (cached < game.top && game.twins[cached].owner == buffer)
		{
			game.lastOwner = cached;
			*id = cached;
			return &game.twins[cached];
		}

		for (DWORD i = 0; i < game.top; ++i)
			if (game.twins[i].owner == buffer)
			{
				game.lastOwner = i;
				game.findCache[slot] = i;
				*id = i;
				return &game.twins[i];
			}

		return NULL;
	}

	BOOL InTwin(const Record* twin, const BYTE* ptr)
	{
		return twin->owner && ptr >= twin->base && ptr < twin->base + twin->pitch * twin->height * sizeof(DWORD) &&
			(DWORD)((ptr - twin->base) % (twin->pitch * sizeof(DWORD))) / sizeof(DWORD) < (DWORD)twin->width;
	}

	Record* FindAddress(const VOID* ptr, DWORD* id)
	{
		Record* twin = NULL;
		if (game.lastAddress < game.top && InTwin(&game.twins[game.lastAddress], (const BYTE*)ptr))
			twin = &game.twins[game.lastAddress];
		for (DWORD i = 0; i < game.top && !twin; ++i)
			if (InTwin(&game.twins[i], (const BYTE*)ptr))
			{
				game.lastAddress = i;
				twin = &game.twins[i];
			}
		if (!twin)
			return NULL;

		*id = game.lastAddress;
		twin->lastUse = game.frameNo;
		return twin;
	}

	BOOL PixelPos(const Record* twin, const VOID* ptr, LONG pitch, LONG width, LONG height, LONG* x, LONG* y)
	{
		LONG start = LONG(((const BYTE*)ptr - twin->base) / sizeof(DWORD));
		*x = start % twin->pitch;
		*y = start / twin->pitch;
		return pitch == twin->pitch && *x + width <= twin->width && *y + height <= twin->height;
	}

	VOID EmitCreate(Record* twin, DWORD id)
	{
		Cmd* cmd = Emit(CmdCreate);
		if (cmd)
		{
			cmd->dst = id;
			SetRect(&cmd->rect, 0, 0, twin->width, twin->height);
			cmd->value = game.scale;
		}
		twin->artCount = 0;
		MarkAll(twin);
	}

	VOID Rebuild(BOOL reset)
	{
		if (reset)
			Emit(CmdReset);
		for (DWORD i = 0; i < game.top; ++i)
			if (game.twins[i].owner)
				EmitCreate(&game.twins[i], i);
		Seed::Repend();
	}

	VOID Sync()
	{
		LONG gen = shared.gen;
		if (gen != game.seenGen)
		{
			game.seenGen = gen;
			Rebuild(FALSE);
		}
	}

	VOID ChooseScale()
	{
		if (game.chosen)
			return;
		game.chosen = TRUE;

		LONG width = *(LONG*)&config.mode->width;
		LONG height = *(LONG*)&config.mode->height;
		LONG screenW = GetSystemMetrics(SM_CXSCREEN);
		LONG screenH = GetSystemMetrics(SM_CYSCREEN);

		LONG scale = 1;
		if (width > 0 && height > 0)
			scale = max((screenW + width - 1) / width, (screenH + height - 1) / height);
		scale = max(1, min(PACK_SCALE, scale));
		while (scale > 1 && ((LONGLONG)width * height * sizeof(DWORD) * scale * scale * TWIN_BUDGET_SCREENS > TWIN_BUDGET || width * scale > TWIN_MAX_SIZE || height * scale > TWIN_MAX_SIZE))
			--scale;
		game.scale = scale;
	}

	VOID Release(Record* twin, DWORD id)
	{
		Cmd* cmd = Emit(CmdDestroy);
		if (cmd)
			cmd->dst = id;

		Seed::DropTwin(twin);
		if (twin->memory)
			game.memoryBytes -= (LONGLONG)twin->width * twin->height * sizeof(DWORD) * game.scale * game.scale;
		MemoryZero(twin, sizeof(Record));
		while (game.top && !game.twins[game.top - 1].owner)
			--game.top;
	}

	VOID CreateTwin(StateBuffer* buffer, LONG width, LONG height)
	{
		ChooseScale();

		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
			Release(twin, id);

		for (id = 0; id < TWIN_MAX && game.twins[id].owner; ++id)
			;
		if (id == TWIN_MAX || width * game.scale > TWIN_MAX_SIZE || height * game.scale > TWIN_MAX_SIZE)
		{
			if (game.errors++ < ERROR_LOGS)
				DebugLog("twin skipped: %dx%d at N=%d %s", width, height, game.scale, id == TWIN_MAX ? "finds no free twin slot" : "is too large");
			return;
		}

		twin = &game.twins[id];
		MemoryZero(twin, sizeof(Record));
		twin->owner = buffer;
		twin->base = (BYTE*)buffer->data;
		twin->pitch = width;
		twin->width = width;
		twin->height = height;
		twin->lastUse = game.frameNo;
		twin->stamp = ++game.stamp;
		if (id >= game.top)
			game.top = id + 1;
		EmitCreate(twin, id);
	}

	VOID DestroyMemoryTwin(Record* twin, DWORD id)
	{
		StateBuffer* owner = twin->owner;
		Release(twin, id);
		delete owner;
	}

	Record* CreateMemoryTwin(const VOID* base, LONG pitch, LONG width, LONG height, DWORD* id)
	{
		if (IsBadReadPtr(base, ((height - 1) * pitch + width) * sizeof(DWORD)))
			return NULL;

		ChooseScale();
		LONGLONG bytes = (LONGLONG)width * height * sizeof(DWORD) * game.scale * game.scale;
		while (game.memoryBytes + bytes > MEMORY_BUDGET)
		{
			Record* oldest = NULL;
			DWORD oldestId = 0;
			for (DWORD i = 0; i < game.top; ++i)
			{
				Record* twin = &game.twins[i];
				if (twin->owner && twin->memory && twin->lastUse != game.frameNo && (!oldest || twin->lastUse < oldest->lastUse))
				{
					oldest = twin;
					oldestId = i;
				}
			}
			if (!oldest)
				break;
			DestroyMemoryTwin(oldest, oldestId);
		}

		StateBuffer* owner = new StateBuffer((VOID*)base);
		CreateTwin(owner, width, height);
		Record* twin = Find(owner, id);
		if (!twin)
		{
			delete owner;
			return NULL;
		}

		twin->pitch = pitch;
		twin->memory = TRUE;
		game.memoryBytes += bytes;
		RECT whole = { 0, 0, width, height };
		FlushRegion(twin, *id, &whole, TRUE);
		return twin;
	}

	VOID DropOverlapping(const VOID* base, LONG pitch, LONG width, LONG height)
	{
		const BYTE* begin = (const BYTE*)base;
		const BYTE* end = begin + ((height - 1) * pitch + width) * sizeof(DWORD);
		for (DWORD i = 0; i < game.top; ++i)
		{
			Record* other = &game.twins[i];
			if (!other->owner || !other->memory)
				continue;

			const BYTE* otherEnd = other->base + ((other->height - 1) * other->pitch + other->width) * sizeof(DWORD);
			if (other->base >= end || otherEnd <= begin)
				continue;

			BOOL overlap = other->base >= begin && (DWORD)((other->base - begin) % (pitch * sizeof(DWORD))) / sizeof(DWORD) < (DWORD)width;
			for (LONG y = 0; y < height && !overlap; ++y)
			{
				const BYTE* row = begin + y * pitch * sizeof(DWORD);
				overlap = InTwin(other, row) || InTwin(other, row + (width - 1) * sizeof(DWORD));
			}
			if (overlap)
				DestroyMemoryTwin(other, i);
		}
	}

	Record* MemoryTwin(const VOID* base, LONG pitch, LONG width, LONG height, BOOL create, DWORD* id, RECT* rect)
	{
		if (!base || width <= 0 || height <= 0 || pitch < width || !Active())
			return NULL;

		Sync();
		LONG x, y;
		Record* twin = FindAddress(base, id);
		if (twin && PixelPos(twin, base, pitch, width, height, &x, &y))
		{
			SetRect(rect, x, y, x + width, y + height);
			return twin;
		}

		DropOverlapping(base, pitch, width, height);
		twin = create ? CreateMemoryTwin(base, pitch, width, height, id) : NULL;
		if (twin)
			SetRect(rect, 0, 0, width, height);
		return twin;
	}

	VOID DropMemory(const VOID* base)
	{
		for (DWORD i = 0; base && i < game.top; ++i)
		{
			Record* twin = &game.twins[i];
			if (twin->owner && twin->memory && twin->base == (const BYTE*)base)
			{
				DestroyMemoryTwin(twin, i);
				return;
			}
		}
	}

	VOID OnMemory(const VOID* base, LONG pitch, LONG width, LONG height)
	{
		if (!base || width <= 0 || height <= 0 || pitch < width || !Active())
			return;

		Sync();
		const DWORD* first = (const DWORD*)base;
		const DWORD* last = first + (height - 1) * pitch + width - 1;
		DWORD id, lastId;
		Record* twin = FindAddress(first, &id);
		if (twin && twin->pitch == pitch && FindAddress(last, &lastId) == twin)
			return;

		DropOverlapping(base, pitch, width, height);
		if (!FindAddress(first, &id))
			CreateMemoryTwin(base, pitch, width, height, &id);
	}

	Record* FindByData(StateBuffer* buffer, DWORD* id, LONG* ox, LONG* oy)
	{
		if (!buffer || !buffer->data)
			return NULL;

		Record* twin = FindAddress(buffer->data, id);
		if (!twin || !twin->memory)
			return NULL;

		LONG start = LONG(((const BYTE*)buffer->data - twin->base) / sizeof(DWORD));
		*ox = start % twin->pitch;
		*oy = start / twin->pitch;
		return twin;
	}

	VOID OnCreate(StateBuffer* buffer, LONG width, LONG height)
	{
		if (!Active() || !buffer || !buffer->data || width <= 0 || height <= 0)
			return;

		Sync();
		CreateTwin(buffer, width, height);
	}

	VOID OnRelease(StateBuffer* buffer)
	{
		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
			Release(twin, id);
	}

	VOID OnCopyCaller(DWORD caller, DWORD outer)
	{
		game.copyCaller = outer ? outer : caller;
	}

	VOID CopyTwins(Record* target, DWORD dstId, LONG x, LONG y, Record* source, DWORD srcId, const RECT* srcRect, BOOL keyed, DWORD key, DWORD caller)
	{
		RECT whole = { x, y, x + srcRect->right - srcRect->left, y + srcRect->bottom - srcRect->top };
		if (!source)
		{
			Seed::Tell1x(target, NULL, &whole, caller, "copy");
			MarkDirty(target, whole);
			return;
		}

		LONG ox = x - srcRect->left;
		LONG oy = y - srcRect->top;
		RECT from = *srcRect;
		if (!Clip(source, &from))
		{
			MarkDirty(target, whole);
			return;
		}

		RECT to = from;
		OffsetRect(&to, ox, oy);
		if (!Clip(target, &to))
			return;
		from = to;
		OffsetRect(&from, -ox, -oy);

		if (keyed)
		{
			source->keyed = TRUE;
			source->key = key | ALPHA_COMPONENT;
		}

		Touched(target, &to);
		Ground::Flush((const DWORD*)source->base + from.top * source->pitch + from.left);
		FlushRegion(source, srcId, &from, FALSE);
		Seed::Tell1x(target, source, &from, caller, keyed ? "keyed copy" : "copy");
		if (!keyed)
			Cover(target, &to);
		FlushRegion(target, dstId, &to, TRUE);

		BOOL masked = keyed && source != target && (key | ALPHA_COMPONENT) != COLORKEY_CHECK && Carries4K(source, &from);
		if (!keyed)
			CoverArt(target, &to);
		CarryArt(target, &to, source, &from, FALSE);

		Cmd* cmd = NULL;
		DWORD maskOffset = 0;
		LONG cw = to.right - to.left, ch = to.bottom - to.top;
		BYTE* mask = masked && cw <= MASK_SIZE && ch <= MASK_SIZE ? Reserve(cw * ch, &maskOffset) : NULL;
		if (mask)
		{
			DWORD skip = key | ALPHA_COMPONENT;
			for (LONG row = 0; row < ch; ++row)
			{
				const DWORD* line = (const DWORD*)source->base + (from.top + row) * source->pitch + from.left;
				for (LONG col = 0; col < cw; ++col)
					*mask++ = line[col] != skip ? 255 : 0;
			}
			cmd = Emit(CmdMask);
		}
		masked = cmd != NULL;
		if (!cmd)
			cmd = Emit(CmdCopy);
		if (!cmd)
		{
			MarkDirty(target, to);
			return;
		}
		cmd->dst = dstId;
		cmd->src = srcId;
		cmd->rect = from;
		cmd->x = to.left;
		cmd->y = to.top;
		cmd->value = key;
		cmd->flags = masked ? 2 : keyed ? 1 : 0;
		cmd->extra = maskOffset;

		if (!keyed)
			Seed::Derive(target, dstId, &to, source, &from, FALSE);
		if (!EqualRect(&from, srcRect))
			MarkDirty(target, whole);
	}

	VOID OnCopy(StateBuffer* dst, LONG x, LONG y, StateBuffer* src, const RECT* srcRect, BOOL keyed, DWORD key)
	{
		DWORD caller = game.copyCaller;
		game.copyCaller = 0;
		if (!Active() || !srcRect)
			return;

		Sync();
		DWORD dstId, srcId;
		LONG dox = 0, doy = 0, sox = 0, soy = 0;
		Record* target = Find(dst, &dstId);
		if (!target)
			target = FindByData(dst, &dstId, &dox, &doy);
		if (!target)
			return;

		Record* source = Find(src, &srcId);
		if (!source)
			source = FindByData(src, &srcId, &sox, &soy);

		RECT rect = *srcRect;
		OffsetRect(&rect, sox, soy);
		CopyTwins(target, dstId, x + dox, y + doy, source, srcId, &rect, keyed, key, caller);
	}

	VOID FillTwin(Record* twin, DWORD id, const RECT* rect, DWORD color)
	{
		RECT area = *rect;
		if (!Clip(twin, &area))
			return;

		Touched(twin, &area);
		Cover(twin, &area);
		FlushRegion(twin, id, &area, TRUE);
		CoverArt(twin, &area);

		DWORD flags = twin->keyed && color == twin->key ? 1 : 0;
		List* list = game.list;
		if (list && list->count)
		{
			Cmd* last = &list->cmds[list->count - 1];
			if (last->type == CmdFill && last->dst == id && last->value == color && last->flags == flags &&
				((last->rect.top == area.top && last->rect.bottom == area.bottom && (last->rect.right == area.left || last->rect.left == area.right)) ||
				(last->rect.left == area.left && last->rect.right == area.right && (last->rect.bottom == area.top || last->rect.top == area.bottom)) ||
				Contains(&last->rect, &area)))
			{
				UnionRect(&last->rect, &last->rect, &area);
				return;
			}
		}

		Cmd* cmd = Emit(CmdFill);
		if (!cmd)
		{
			MarkDirty(twin, area);
			return;
		}
		cmd->dst = id;
		cmd->rect = area;
		cmd->value = color;
		cmd->flags = flags;
	}

	VOID OnFill(StateBuffer* dst, const RECT* rect, DWORD color)
	{
		if (!Active())
			return;

		Sync();
		DWORD id;
		Record* twin = Find(dst, &id);
		if (!twin)
			return;

		RECT area;
		if (rect)
			area = *rect;
		else
			SetRect(&area, 0, 0, twin->width, twin->height);
		FillTwin(twin, id, &area, color);
	}

	VOID OnWrite(const VOID* first, LONG pitch, LONG width, LONG height)
	{
		if (!first || width <= 0 || height <= 0 || !Active())
			return;

		Sync();
		DWORD id;
		Record* twin = FindAddress(first, &id);
		if (!twin)
			return;

		LONG start = LONG(((const BYTE*)first - twin->base) / sizeof(DWORD));
		LONG x = start % twin->pitch;
		LONG y = start / twin->pitch;

		RECT rect;
		if (pitch == twin->pitch && x + width <= twin->pitch)
			SetRect(&rect, x, y, x + width, y + height);
		else
		{
			LONGLONG span = (LONGLONG)(height - 1) * pitch;
			LONGLONG low = start + min(span, 0LL);
			LONGLONG high = start + max(span, 0LL) + width;
			if (low < 0)
				low = 0;
			if (high <= low)
				return;
			SetRect(&rect, 0, LONG(low / twin->pitch), twin->width, LONG(min((high - 1) / twin->pitch + 1, (LONGLONG)twin->height)));
		}

		if (!Clip(twin, &rect))
			return;
		Seed::Tell1x(twin, NULL, &rect, 0, "hooked CPU write");
		MarkDirty(twin, rect);
	}

	VOID OnWriteAll(StateBuffer* buffer)
	{
		if (!Active())
			return;

		Sync();
		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
		{
			RECT whole = { 0, 0, twin->width, twin->height };
			Touched(twin, &whole);
			MarkAll(twin);
		}
	}

	VOID DumpUncovered(const Record* target, PixelMode mode, DWORD param, const DWORD* src, LONG srcPitch, LONG width, LONG height, const RECT* to)
	{
		static LONG dumped;
		static DWORD seen[64];
		DWORD key = (DWORD)src ^ ((DWORD)mode << 28) ^ ((DWORD)width << 16) ^ (DWORD)height;
		if (!target->screen || dumped >= 64 || width > 1024 || height > 1024)
			return;
		for (LONG i = 0; i < dumped; ++i)
			if (seen[i] == key)
				return;
		seen[dumped] = key;
		LONG n = ++dumped;

		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, MAX_PATH);
		while (length && path[length - 1] != '\\')
			--length;
		StrPrint(path + length, "uncovered-%02d-mode%d-p%X-%dx%d.raw", n, mode, param, width, height);
		HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD written;
			LONG size[2] = { width, height };
			WriteFile(file, size, sizeof(size), &written, NULL);
			for (LONG y = 0; y < height; ++y)
				WriteFile(file, src + y * srcPitch, width * sizeof(DWORD), &written, NULL);
			CloseHandle(file);
		}
		DebugLog("pixel op %d from memory with no twin: mode %d param %X %dx%d to %d,%d src %p in %s", n, mode, param, width, height, to->left, to->top, src, Seed::DescribeMemory(src));
	}

	VOID OnPixelCopy(PixelMode mode, const DWORD* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height, DWORD key, DWORD param)
	{
		if (!dst || !src || width <= 0 || height <= 0 || (DWORD)mode >= PixelModes || !Active())
			return;

		Ground::Flush(src);
		Sync();

		DWORD dstId, srcId;
		LONG dx, dy, sx, sy;
		Record* target = FindAddress(dst, &dstId);
		if (!target)
			return;
		if (!PixelPos(target, dst, dstPitch, width, height, &dx, &dy))
		{
			OnWrite(dst, dstPitch, width, height);
			return;
		}
		RECT to = { dx, dy, dx + width, dy + height };

		Record* source = FindAddress(src, &srcId);
		if (!source || !PixelPos(source, src, srcPitch, width, height, &sx, &sy))
		{
			DumpUncovered(target, mode, param, src, srcPitch, width, height, &to);
			Seed::Tell1x(target, NULL, &to, 0, "pixel op");
			MarkDirty(target, to);
			return;
		}
		RECT from = { sx, sy, sx + width, sy + height };

		BOOL mirror = (param & PixelMirror) != 0;
		if ((mode == PixelAverage && (param & 0xFF) >= 255) || mode == PixelKeyedCopy || (mode == PixelCopy && !mirror))
		{
			CopyTwins(target, dstId, dx, dy, source, srcId, &from, mode != PixelCopy, key, 0);
			return;
		}
		if (source == target)
		{
			MarkDirty(target, to);
			return;
		}

		Touched(target, &to);
		FlushRegion(source, srcId, &from, FALSE);
		Seed::Tell1x(target, source, &from, 0, "pixel op");
		FlushRegion(target, dstId, &to, TRUE);
		if (mode == PixelKeyed && (param & 0xFF) >= 255)
		{
			source->keyed = TRUE;
			source->key = key | ALPHA_COMPONENT;
		}

		Cmd* cmd = Emit(CmdPixel);
		if (!cmd)
		{
			MarkDirty(target, to);
			return;
		}
		cmd->dst = dstId;
		cmd->src = srcId;
		cmd->rect = from;
		cmd->x = dx;
		cmd->y = dy;
		cmd->value = key;
		cmd->flags = mode;
		cmd->extra = param;

		if (mode == PixelCopy)
			CoverArt(target, &to);
		if (mode != PixelAdd && mode != PixelSub && mode != PixelBlend)
			CarryArt(target, &to, source, &from, mirror);
		if (mode == PixelCopy)
			Seed::Derive(target, dstId, &to, source, &from, TRUE);
	}

	Record* PixelTarget(const VOID* first, LONG pitch, LONG width, LONG height, DWORD* id, RECT* rect)
	{
		Record* twin = FindAddress(first, id);
		if (!twin)
			return NULL;

		LONG x, y;
		if (!PixelPos(twin, first, pitch, width, height, &x, &y))
		{
			OnWrite(first, pitch, width, height);
			return NULL;
		}
		SetRect(rect, x, y, x + width, y + height);
		return twin;
	}

	VOID OnFillRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color)
	{
		if (!first || width <= 0 || height <= 0 || !Active())
			return;

		Sync();
		DWORD id;
		RECT rect;
		Record* twin = PixelTarget(first, pitch, width, height, &id, &rect);
		if (twin)
			FillTwin(twin, id, &rect, color);
	}

	Cmd* PixelCmd(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD type)
	{
		Sync();
		DWORD id;
		RECT rect;
		Record* twin = PixelTarget(first, pitch, width, height, &id, &rect);
		if (!twin)
			return NULL;

		Touched(twin, &rect);
		FlushRegion(twin, id, &rect, TRUE);
		Cmd* cmd = Emit(type);
		if (!cmd)
		{
			MarkDirty(twin, rect);
			return NULL;
		}
		cmd->dst = id;
		cmd->rect = rect;
		return cmd;
	}

	VOID OnTintRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color, DWORD weight)
	{
		Cmd* cmd = first && width > 0 && height > 0 && weight && Active() ? PixelCmd(first, pitch, width, height, CmdTint) : NULL;
		if (cmd)
		{
			cmd->value = color;
			cmd->extra = weight;
		}
	}

	VOID OnLighter(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD key, BOOL lighten)
	{
		Cmd* cmd = first && width > 0 && height > 0 && Active() ? PixelCmd(first, pitch, width, height, CmdLighter) : NULL;
		if (cmd)
		{
			cmd->value = key;
			cmd->flags = lighten ? 1 : 0;
		}
	}

	VOID OnBlendRow(const DWORD* src, const DWORD* dst, LONG length, const BYTE* mask, BOOL keyed, DWORD key)
	{
		if (!src || !dst || !mask || length <= 0 || !Active())
			return;

		Ground::Flush(src);
		Sync();

		DWORD dstId, srcId;
		LONG dx, dy, sx, sy;
		Record* target = FindAddress(dst, &dstId);
		if (!target)
			return;
		if (!PixelPos(target, dst, target->pitch, length, 1, &dx, &dy))
		{
			OnWrite(dst, target->pitch, length, 1);
			return;
		}
		RECT to = { dx, dy, dx + length, dy + 1 };

		Record* source = FindAddress(src, &srcId);
		if (!source || source == target || !PixelPos(source, src, source->pitch, length, 1, &sx, &sy))
		{
			if (!source)
				Seed::Tell1x(target, NULL, &to, 0, "blend row");
			MarkDirty(target, to);
			return;
		}
		RECT from = { sx, sy, sx + length, sy + 1 };

		Touched(target, &to);
		FlushRegion(source, srcId, &from, FALSE);
		Seed::Tell1x(target, source, &from, 0, "blend row");
		FlushRegion(target, dstId, &to, TRUE);

		DWORD offset;
		List* list = game.list;
		Cmd* last = list && list->count ? &list->cmds[list->count - 1] : NULL;
		LONG lastH = last ? last->rect.bottom - last->rect.top : 0;
		if (last && last->type == CmdMask && last->dst == dstId && last->src == srcId && last->flags == (DWORD)(keyed ? 1 : 0) && (!keyed || last->value == key) &&
			last->x == dx && last->y + lastH == dy && last->rect.left == sx && last->rect.right == sx + length && last->rect.bottom == sy &&
			lastH < MASK_SIZE && length <= MASK_SIZE && last->extra + (DWORD)(length * lastH) == list->size)
		{
			BYTE* bytes = Reserve(length, &offset);
			if (bytes)
			{
				MemoryCopy(bytes, mask, length);
				++last->rect.bottom;
				return;
			}
		}

		BYTE* bytes = length <= MASK_SIZE ? Reserve(length, &offset) : NULL;
		Cmd* cmd = bytes ? Emit(CmdMask) : NULL;
		if (!cmd)
		{
			MarkDirty(target, to);
			return;
		}
		MemoryCopy(bytes, mask, length);
		cmd->dst = dstId;
		cmd->src = srcId;
		cmd->rect = from;
		cmd->x = dx;
		cmd->y = dy;
		cmd->value = key;
		cmd->flags = keyed ? 1 : 0;
		cmd->extra = offset;
	}

	VOID GroundArt(const VOID* data, LONG pitch, const RECT* rect, const VOID* ops, DWORD bytes)
	{
		if (!Active() || !data || !rect || !ops || !bytes)
			return;

		Sync();
		DWORD id;
		Record* twin = FindAddress(data, &id);
		if (!twin || pitch != twin->pitch)
			return;

		LONG start = LONG(((const BYTE*)data - twin->base) / sizeof(DWORD));
		RECT area = *rect;
		OffsetRect(&area, start % twin->pitch, start / twin->pitch);
		if (!Clip(twin, &area))
			return;

		Touched(twin, &area);
		Cover(twin, &area);
		FlushRegion(twin, id, &area, TRUE);

		LONG width = area.right - area.left;
		LONG height = area.bottom - area.top;
		DWORD opsOffset, offset;
		BYTE* opsCopy = Reserve(bytes, &opsOffset);
		if (opsCopy)
			MemoryCopy(opsCopy, ops, bytes);
		DWORD* pixels = opsCopy ? (DWORD*)Reserve(width * height * sizeof(DWORD), &offset) : NULL;
		Cmd* cmd = pixels ? Emit(CmdGround) : NULL;
		if (!cmd)
		{
			MarkDirty(twin, area);
			return;
		}
		Convert(twin, pixels, (const DWORD*)twin->base + area.top * twin->pitch + area.left, width, height, FALSE);
		cmd->dst = id;
		cmd->src = offset;
		cmd->rect = area;
		cmd->value = opsOffset;
		cmd->extra = bytes;

		CoverArt(twin, &area);
		AddArt(twin, &area);
	}

	BOOL KeyPressed(INT key, BOOL* down)
	{
		BOOL now = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(key) & 0x8000);
		BOOL pressed = now && !*down;
		*down = now;
		return pressed;
	}

	VOID OnPresent(StateBuffer* screen, LONG /*left*/, LONG /*top*/)
	{
		if (!Active())
			return;

		Sync();
		if (KeyPressed(VK_F12, &game.showKey))
			shared.show = !shared.show;

		Ground::Retry();
		Seed::Poll();

		DWORD id;
		Record* twin = Find(screen, &id);
		if (twin)
		{
			twin->screen = TRUE;
			RECT whole = { 0, 0, twin->width, twin->height };
			FlushRegion(twin, id, &whole, FALSE);
		}
		else
			id = TWIN_NONE;

		Cmd* cmd = Emit(CmdPresent);
		if (cmd)
		{
			StateBufferAligned* aligned = (StateBufferAligned*)screen;
			cmd->dst = id;
			Config::GetMapZoomRect(&cmd->rect);
			cmd->flags = (Config::IsMapZoomed() ? PresentMapZoom : 0) | (aligned->isBack ? PresentBack : 0) | (aligned->isZoomed ? PresentZoomed : 0);
		}
		++game.frameNo;

		List* list = game.list;
		game.list = NULL;
		if (!list)
			return;

		DWORD bytes = list->size + list->count * sizeof(Cmd);
		BOOL overflow = FALSE;
		Lock();
		if (shared.glFailed)
			RecycleList(list);
		else if (shared.head && shared.bytes + bytes > ARENA_LIMIT)
		{
			DropQueued();
			RecycleList(list);
			overflow = TRUE;
		}
		else
		{
			if (shared.tail)
				shared.tail->next = list;
			else
				shared.head = list;
			shared.tail = list;
			shared.bytes += bytes;
		}
		Unlock();

		if (overflow)
		{
			if (game.errors++ < ERROR_LOGS)
				DebugLog("twin queue overflow: the queued frames were dropped and every twin is rebuilt from 1x");
			Rebuild(TRUE);
		}
	}
}
