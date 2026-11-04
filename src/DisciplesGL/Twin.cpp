#include "StdAfx.h"
#include "Twin.h"
#include "Config.h"
#include "GLib.h"

#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif
#ifndef GL_VERTEX_ARRAY_BINDING
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#endif
#ifndef GL_ARRAY_BUFFER_BINDING
#define GL_ARRAY_BUFFER_BINDING 0x8894
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT 0x0CF5
#endif
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT 0x0D05
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif
#ifndef GL_READ_FRAMEBUFFER_BINDING
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#endif

#define TWIN_MAX 512
#define TWIN_NONE 0xFFFFFFFF
#define TARGET_SCRATCH 0x10000
#define TARGET_CHECK 0x10001
#define DIRTY_MAX 32
#define ARENA_LIMIT (64 * 1024 * 1024)
#define TWIN_BUDGET (512LL * 1024 * 1024)
#define TWIN_BUDGET_SCREENS 12
#define TWIN_MAX_SIZE 8192
#define STAGE_SIZE 2048
#define POOL_MAX 4
#define POOL_KEEP (8 * 1024 * 1024)
#define LOG_PERIOD 5000
#define CHECK_FIRST 3000
#define CHECK_PERIOD 10000
#define LOG_TWINS 64
#define LOG_FAILS 16
#define TILE 16
#define WORST 10

namespace Twin
{
	enum CmdType { CmdCreate, CmdDestroy, CmdCopy, CmdFill, CmdUpload, CmdPresent, CmdReset };
	enum PresentFlags { PresentMapZoom = 1, PresentBack = 2, PresentZoomed = 4, PresentCheck = 8 };

	struct Cmd {
		DWORD type;
		DWORD dst;
		DWORD src;
		RECT rect;
		LONG x;
		LONG y;
		DWORD value;
		DWORD flags;
	};

	struct Counters {
		DWORD copies;
		DWORD keyed;
		DWORD fills;
		DWORD uploads;
		DWORD pixels;
		DWORD writes;
		DWORD unknown;
	};

	struct List {
		List* next;
		Cmd* cmds;
		DWORD count;
		DWORD capacity;
		BYTE* arena;
		DWORD size;
		DWORD arenaCapacity;
		Counters counters;
	};

	struct Record {
		StateBuffer* owner;
		BYTE* base;
		LONG pitch;
		LONG width;
		LONG height;
		DWORD count;
		RECT dirty[DIRTY_MAX];
	};

	struct Vertex {
		FLOAT x;
		FLOAT y;
		FLOAT u;
		FLOAT v;
	};

	struct Gpu {
		GLuint texture;
		GLuint fbo;
		LONG width;
		LONG height;
		LONG scale;
	};

	struct Placed {
		DWORD dst;
		LONG sx;
		LONG sy;
		RECT rect;
	};

	struct {
		BOOL chosen;
		LONG scale;
		Record twins[TWIN_MAX];
		DWORD top;
		DWORD alive;
		LONGLONG bytes;
		DWORD lastOwner;
		DWORD lastAddress;
		List* list;
		LONG seenGen;
		DWORD logged;
		DWORD fails;
		BOOL keyDown;
		BOOL checkRequest;
		BOOL started;
		DWORD logTick;
		DWORD checkTick;
		DWORD frames;
		Counters interval;
		Counters frame;
		DWORD queuedMax;
		DWORD resets;
		DWORD resetsLogged;
		DWORD resetTick;
	} game;

	struct {
		CRITICAL_SECTION cs;
		volatile LONG state;
		volatile LONG gen;
		volatile BOOL show;
		volatile BOOL glFailed;
		List* head;
		List* tail;
		DWORD queued;
		DWORD bytes;
		List* pool;
		DWORD poolCount;
		DWORD replays;
		DWORD drawCalls;
		DWORD lastDrawCalls;
		DWORD draws;
		LONGLONG replayUs;
		DWORD replayMaxUs;
		DWORD lastReplayUs;
	} shared = { {}, 0, 0, TRUE, FALSE };

	struct {
		BOOL ready;
		BOOL failed;
		GLuint copyProgram;
		GLuint checkProgram;
		GLint keyedLoc;
		GLint keyLoc;
		GLint scaleLoc;
		GLuint vao;
		GLuint vbo;
		GLuint stage;
		GLuint scratch;
		GLuint scratchFbo;
		LONG scratchW;
		LONG scratchH;
		GLuint check;
		GLuint checkFbo;
		LONG checkW;
		LONG checkH;
		DWORD* checkData;
		Gpu twins[TWIN_MAX];
		Vertex* verts;
		DWORD vertCount;
		DWORD vertCapacity;
		Placed* placed;
		DWORD placedCount;
		DWORD placedCapacity;
		DWORD target;
		DWORD program;
		BOOL keyed;
		DWORD key;
		DWORD drawCalls;
		DWORD readFbo;
		LARGE_INTEGER frequency;
		struct {
			BOOL valid;
			DWORD id;
			RECT hole;
			DWORD flags;
		} shown;
	} gl;

#pragma region Shared
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

	VOID DeleteList(List* list)
	{
		if (list->cmds)
			MemoryFree(list->cmds);
		if (list->arena)
			MemoryFree(list->arena);
		MemoryFree(list);
	}

	VOID RecycleList(List* list)
	{
		if (shared.poolCount < POOL_MAX && list->arenaCapacity <= POOL_KEEP)
		{
			list->next = shared.pool;
			shared.pool = list;
			++shared.poolCount;
		}
		else
			DeleteList(list);
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
		shared.queued = 0;
		shared.bytes = 0;
	}
#pragma endregion

#pragma region Record
	BOOL Active()
	{
		return config.renderer != RendererGDI && config.gl.version.value >= GL_VER_3_0 && config.bpp32Hooked && !config.type.editor;
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
		MemoryZero(&list->counters, sizeof(Counters));
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

	VOID MarkAll(Record* twin)
	{
		twin->count = 1;
		SetRect(&twin->dirty[0], 0, 0, twin->width, twin->height);
	}

	VOID Append(Record* twin, const RECT* rect)
	{
		if (twin->count < DIRTY_MAX)
		{
			twin->dirty[twin->count++] = *rect;
			return;
		}

		RECT bound = *rect;
		for (DWORD i = 0; i < twin->count; ++i)
			UnionRect(&bound, &bound, &twin->dirty[i]);
		twin->count = 1;
		twin->dirty[0] = bound;
	}

	VOID MarkDirty(Record* twin, RECT rect)
	{
		if (!Clip(twin, &rect))
			return;

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

	VOID Upload(Record* twin, DWORD id, const RECT* rect)
	{
		LONG width = rect->right - rect->left;
		LONG height = rect->bottom - rect->top;

		DWORD offset;
		DWORD* dst = (DWORD*)Reserve(width * height * sizeof(DWORD), &offset);
		if (!dst)
			return;

		const DWORD* src = (const DWORD*)twin->base + rect->top * twin->pitch + rect->left;
		for (LONG y = height; y; --y)
		{
			for (LONG x = 0; x < width; ++x)
			{
				DWORD pixel = src[x];
				// alpha 254 marks a 1x alpha byte other than 255, so keyed copies keep the CPU's exact DWORD compare
				*dst++ = pixel >= ALPHA_COMPONENT ? pixel : (pixel & 0x00FFFFFF) | 0xFE000000;
			}
			src += twin->pitch;
		}

		Cmd* cmd = Emit(CmdUpload);
		if (!cmd)
			return;
		cmd->dst = id;
		cmd->rect = *rect;
		cmd->value = offset;

		++game.list->counters.uploads;
		game.list->counters.pixels += width * height;
	}

	VOID FlushRegion(Record* twin, DWORD id, const RECT* rect)
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

			if (Area(&part) * 2 >= Area(&dirty))
				Upload(twin, id, &dirty);
			else
			{
				Upload(twin, id, &part);

				RECT pieces[4] = {
					{ dirty.left, dirty.top, dirty.right, part.top },
					{ dirty.left, part.bottom, dirty.right, dirty.bottom },
					{ dirty.left, part.top, part.left, part.bottom },
					{ part.right, part.top, dirty.right, part.bottom }
				};
				for (DWORD k = 0; k < 4; ++k)
					if (pieces[k].left < pieces[k].right && pieces[k].top < pieces[k].bottom)
						Append(twin, &pieces[k]);
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

		for (DWORD i = 0; i < game.top; ++i)
			if (game.twins[i].owner == buffer)
			{
				game.lastOwner = i;
				*id = i;
				return &game.twins[i];
			}

		return NULL;
	}

	BOOL InTwin(const Record* twin, const BYTE* ptr)
	{
		return twin->owner && ptr >= twin->base && ptr < twin->base + twin->pitch * twin->height * sizeof(DWORD);
	}

	Record* FindAddress(const VOID* ptr, DWORD* id)
	{
		if (game.lastAddress < game.top && InTwin(&game.twins[game.lastAddress], (const BYTE*)ptr))
		{
			*id = game.lastAddress;
			return &game.twins[game.lastAddress];
		}

		for (DWORD i = 0; i < game.top; ++i)
			if (InTwin(&game.twins[i], (const BYTE*)ptr))
			{
				game.lastAddress = i;
				*id = i;
				return &game.twins[i];
			}

		return NULL;
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
		MarkAll(twin);
	}

	VOID Rebuild(BOOL reset)
	{
		if (reset)
			Emit(CmdReset);

		for (DWORD i = 0; i < game.top; ++i)
			if (game.twins[i].owner)
				EmitCreate(&game.twins[i], i);
	}

	VOID Sync()
	{
		LONG gen = shared.gen;
		if (gen == game.seenGen)
			return;

		game.seenGen = gen;
		Rebuild(FALSE);
		DebugLog("twin rebuild after GL release, twins %u", game.alive);
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
		scale = max(1, min(4, scale));

		while (scale > 1 && ((LONGLONG)width * height * sizeof(DWORD) * scale * scale * TWIN_BUDGET_SCREENS > TWIN_BUDGET || width * scale > TWIN_MAX_SIZE || height * scale > TWIN_MAX_SIZE))
			--scale;

		game.scale = scale;
		DebugLog("twin scale N=%d screen %dx%d mode %dx%d", scale, screenW, screenH, width, height);
	}

	VOID Release(Record* twin, DWORD id)
	{
		if (Active())
		{
			Cmd* cmd = Emit(CmdDestroy);
			if (cmd)
				cmd->dst = id;
		}

		if (game.logged < LOG_TWINS)
		{
			++game.logged;
			DebugLog("twin destroy %u %dx%d buffer %08X", id, twin->width, twin->height, twin->owner);
		}

		--game.alive;
		game.bytes -= (LONGLONG)twin->width * twin->height * sizeof(DWORD) * game.scale * game.scale;
		twin->owner = NULL;
		twin->base = NULL;
		twin->count = 0;

		while (game.top && !game.twins[game.top - 1].owner)
			--game.top;
	}

	VOID OnCreate(StateBuffer* buffer, LONG width, LONG height)
	{
		if (!Active() || !buffer || !buffer->data || width <= 0 || height <= 0)
			return;

		Sync();
		ChooseScale();

		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
			Release(twin, id);

		if (width * game.scale > TWIN_MAX_SIZE || height * game.scale > TWIN_MAX_SIZE)
		{
			if (game.fails < LOG_FAILS)
			{
				++game.fails;
				DebugLog("twin skip %dx%d buffer %08X: too large for N=%d", width, height, buffer, game.scale);
			}
			return;
		}

		for (id = 0; id < TWIN_MAX && game.twins[id].owner; ++id)
			;
		if (id == TWIN_MAX)
		{
			if (game.fails < LOG_FAILS)
			{
				++game.fails;
				DebugLog("twin skip %dx%d buffer %08X: no free slot", width, height, buffer);
			}
			return;
		}

		twin = &game.twins[id];
		twin->owner = buffer;
		twin->base = (BYTE*)buffer->data;
		twin->pitch = width;
		twin->width = width;
		twin->height = height;
		if (id >= game.top)
			game.top = id + 1;

		++game.alive;
		game.bytes += (LONGLONG)width * height * sizeof(DWORD) * game.scale * game.scale;
		EmitCreate(twin, id);

		if (game.logged < LOG_TWINS)
		{
			++game.logged;
			DebugLog("twin create %u %dx%d buffer %08X data %08X", id, width, height, buffer, buffer->data);
		}
	}

	VOID OnRelease(StateBuffer* buffer)
	{
		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
			Release(twin, id);
	}

	VOID OnCopy(StateBuffer* dst, LONG x, LONG y, StateBuffer* src, const RECT* srcRect, BOOL keyed, DWORD key)
	{
		if (!Active() || !srcRect)
			return;

		Sync();

		DWORD dstId;
		Record* target = Find(dst, &dstId);
		if (!target)
			return;

		RECT whole = { x, y, x + srcRect->right - srcRect->left, y + srcRect->bottom - srcRect->top };

		DWORD srcId;
		Record* source = Find(src, &srcId);
		if (!source)
		{
			if (game.list || CurrentList())
				++game.list->counters.unknown;
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

		FlushRegion(source, srcId, &from);
		if (!keyed)
			Cover(target, &to);

		Cmd* cmd = Emit(CmdCopy);
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
		cmd->flags = keyed ? 1 : 0;

		++game.list->counters.copies;
		if (keyed)
			++game.list->counters.keyed;

		if (!EqualRect(&from, srcRect))
			MarkDirty(target, whole);
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
		if (!Clip(twin, &area))
			return;

		Cover(twin, &area);

		Cmd* cmd = Emit(CmdFill);
		if (!cmd)
		{
			MarkDirty(twin, area);
			return;
		}
		cmd->dst = id;
		cmd->rect = area;
		cmd->value = color;

		++game.list->counters.fills;
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

		if (game.list || CurrentList())
			++game.list->counters.writes;

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
			MarkAll(twin);
	}

	VOID AddCounters(Counters* dst, const Counters* src)
	{
		dst->copies += src->copies;
		dst->keyed += src->keyed;
		dst->fills += src->fills;
		dst->uploads += src->uploads;
		dst->pixels += src->pixels;
		dst->writes += src->writes;
		dst->unknown += src->unknown;
	}

	VOID LogInterval(DWORD now)
	{
		DWORD elapsed = now - game.logTick;
		game.logTick = now;

		Lock();
		DWORD replays = shared.replays;
		DWORD drawCalls = shared.drawCalls;
		DWORD draws = shared.draws;
		LONGLONG replayUs = shared.replayUs;
		DWORD replayMaxUs = shared.replayMaxUs;
		shared.replays = shared.drawCalls = shared.draws = shared.replayMaxUs = 0;
		shared.replayUs = 0;
		Unlock();

		DWORD avgUs = draws ? DWORD(replayUs / draws) : 0;
		DWORD mb10 = DWORD(game.bytes * 10 / (1024 * 1024));
		const Counters* c = &game.interval;
		DebugLog("twin stats %ums frames %u copy %u keyed %u fill %u upload %u px %u writes %u unknown %u | replays %u draws %u calls %u queuedMax %u replay avg %u.%03u max %u.%03u ms | twins %u %u.%uMB N %d resets %u",
			elapsed, game.frames, c->copies, c->keyed, c->fills, c->uploads, c->pixels, c->writes, c->unknown,
			replays, draws, drawCalls, game.queuedMax, avgUs / 1000, avgUs % 1000, replayMaxUs / 1000, replayMaxUs % 1000,
			game.alive, mb10 / 10, mb10 % 10, game.scale, game.resets);

		game.frames = 0;
		game.queuedMax = 0;
		game.resets = 0;
		MemoryZero(&game.interval, sizeof(Counters));
	}

	VOID OnPresent(StateBuffer* screen, LONG left, LONG top)
	{
		if (!Active())
			return;

		Sync();

		DWORD now = GetTickCount();
		if (!game.started)
		{
			game.started = TRUE;
			game.logTick = now;
			game.checkTick = now - CHECK_PERIOD + CHECK_FIRST;
		}

		BOOL down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_F12) & 0x8000);
		if (down && !game.keyDown)
		{
			shared.show = !shared.show;
			DebugLog("twin display %s", shared.show ? "twin" : "1x");
		}
		game.keyDown = down;

		DWORD id = TWIN_NONE;
		Record* twin = Find(screen, &id);
		if (twin)
		{
			RECT whole = { 0, 0, twin->width, twin->height };
			FlushRegion(twin, id, &whole);
		}
		else
			id = TWIN_NONE;

		BOOL check = FALSE;
		DWORD checkOffset = 0;
		if (twin && (game.checkRequest || now - game.checkTick >= CHECK_PERIOD))
		{
			DWORD* copy = (DWORD*)Reserve(twin->width * twin->height * sizeof(DWORD), &checkOffset);
			if (copy)
			{
				MemoryCopy(copy, twin->base, twin->width * twin->height * sizeof(DWORD));
				check = TRUE;
			}
			game.checkRequest = FALSE;
			game.checkTick = now;
		}

		Cmd* cmd = Emit(CmdPresent);
		if (cmd)
		{
			cmd->dst = id;
			Config::GetMapZoomRect(&cmd->rect);
			StateBufferAligned* aligned = (StateBufferAligned*)screen;
			cmd->flags = (Config::IsMapZoomed() ? PresentMapZoom : 0) | (aligned->isBack ? PresentBack : 0) | (aligned->isZoomed ? PresentZoomed : 0) | (check ? PresentCheck : 0);
			cmd->value = checkOffset;
			if (twin)
			{
				cmd->x = twin->width;
				cmd->y = twin->height;
			}
		}

		List* list = game.list;
		game.list = NULL;
		if (list)
		{
			++game.frames;
			AddCounters(&game.interval, &list->counters);
			game.frame = list->counters;

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
				++shared.queued;
				shared.bytes += bytes;
				if (shared.queued > game.queuedMax)
					game.queuedMax = shared.queued;
			}
			Unlock();

			if (overflow)
			{
				++game.resets;
				if (game.resetsLogged < 4 || now - game.resetTick >= LOG_PERIOD)
				{
					++game.resetsLogged;
					game.resetTick = now;
					DebugLog("twin queue overflow, dropped pending frames and rebuilding %u twins", game.alive);
				}
				Rebuild(TRUE);
			}
		}

		if (now - game.logTick >= LOG_PERIOD)
			LogInterval(now);
	}

	VOID LogStats()
	{
		if (!Active())
		{
			DebugLog("twin F12 inactive renderer %u gl %08X bpp32 %u editor %u", config.renderer, config.gl.version.value, config.bpp32Hooked, config.type.editor);
			return;
		}

		Lock();
		DWORD queued = shared.queued;
		DWORD queuedBytes = shared.bytes;
		DWORD lastUs = shared.lastReplayUs;
		DWORD lastCalls = shared.lastDrawCalls;
		BOOL failed = shared.glFailed;
		Unlock();

		DWORD mb10 = DWORD(game.bytes * 10 / (1024 * 1024));
		const Counters* c = &game.frame;
		DebugLog("twin F12 display %s gl %s twins %u %u.%uMB N %d | last frame copy %u keyed %u fill %u upload %u px %u writes %u unknown %u | queued %u (%u KB) | last replay %u.%03u ms calls %u",
			shared.show ? "twin" : "1x", failed ? "failed" : "ok", game.alive, mb10 / 10, mb10 % 10, game.scale,
			c->copies, c->keyed, c->fills, c->uploads, c->pixels, c->writes, c->unknown,
			queued, queuedBytes / 1024, lastUs / 1000, lastUs % 1000, lastCalls);

		game.checkRequest = TRUE;
	}
#pragma endregion

#pragma region Render
	GLuint Compile(const CHAR* source, GLenum type)
	{
		CHAR* text = (CHAR*)MemoryAlloc(StrLength(GLSL_VER_1_30) + StrLength(source) + 1);
		StrCopy(text, GLSL_VER_1_30);
		StrCat(text, source);

		GLuint shader = GLCreateShader(type);
		GLShaderSource(shader, 1, (const GLchar**)&text, NULL);
		GLCompileShader(shader);
		MemoryFree(text);

		GLint status = 0;
		GLGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (!status)
			DebugLog("twin shader compile failed type %X", type);
		return status ? shader : 0;
	}

	GLuint Link(const CHAR* vertex, const CHAR* fragment)
	{
		GLuint vShader = Compile(vertex, GL_VERTEX_SHADER);
		GLuint fShader = Compile(fragment, GL_FRAGMENT_SHADER);
		if (!vShader || !fShader)
		{
			if (vShader)
				GLDeleteShader(vShader);
			if (fShader)
				GLDeleteShader(fShader);
			return 0;
		}

		GLuint program = GLCreateProgram();
		GLBindAttribLocation(program, 0, "vCoord");
		GLBindAttribLocation(program, 1, "vTex");
		GLAttachShader(program, vShader);
		GLAttachShader(program, fShader);
		GLLinkProgram(program);
		GLDetachShader(program, fShader);
		GLDetachShader(program, vShader);
		GLDeleteShader(fShader);
		GLDeleteShader(vShader);
		return program;
	}

	VOID SetNearest(GLint filter)
	{
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	BOOL InitGL()
	{
		if (gl.ready)
			return TRUE;
		if (gl.failed)
			return FALSE;

		const CHAR* vertex =
			"in vec2 vCoord;\n"
			"in vec2 vTex;\n"
			"out vec2 fTex;\n"
			"void main()\n"
			"{\n"
			"	gl_Position = vec4(vCoord, 0.0, 1.0);\n"
			"	fTex = vTex;\n"
			"}\n";

		const CHAR* copyFragment =
			"uniform sampler2D tex01;\n"
			"uniform int keyed;\n"
			"uniform vec4 key;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	vec4 c = texture(tex01, fTex);\n"
			"	if (keyed != 0 && (c.a < 0.5 / 255.0 || (c.a > 254.5 / 255.0 && all(lessThan(abs(c.rgb - key.rgb), vec3(0.5 / 255.0))))))\n"
			"		discard;\n"
			"	FRAG_COLOR = c;\n"
			"}\n";

		const CHAR* checkFragment =
			"uniform sampler2D tex01;\n"
			"uniform int scale;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	FRAG_COLOR = texelFetch(tex01, ivec2(gl_FragCoord.xy) * scale + ivec2(scale / 2), 0);\n"
			"}\n";

		QueryPerformanceFrequency(&gl.frequency);

		gl.copyProgram = Link(vertex, copyFragment);
		if (gl.copyProgram)
		{
			GLUseProgram(gl.copyProgram);
			GLUniform1i(GLGetUniformLocation(gl.copyProgram, "tex01"), 0);
			gl.keyedLoc = GLGetUniformLocation(gl.copyProgram, "keyed");
			gl.keyLoc = GLGetUniformLocation(gl.copyProgram, "key");
			GLUniform1i(gl.keyedLoc, 0);
		}

		gl.checkProgram = Link(vertex, checkFragment);
		if (gl.checkProgram)
		{
			GLUseProgram(gl.checkProgram);
			GLUniform1i(GLGetUniformLocation(gl.checkProgram, "tex01"), 0);
			gl.scaleLoc = GLGetUniformLocation(gl.checkProgram, "scale");
		}

		GLGenVertexArrays(1, &gl.vao);
		GLBindVertexArray(gl.vao);
		GLGenBuffers(1, &gl.vbo);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLEnableVertexAttribArray(0);
		GLVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		GLEnableVertexAttribArray(1);
		GLVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)8);

		GLGenTextures(1, &gl.stage);
		GLBindTexture(GL_TEXTURE_2D, gl.stage);
		SetNearest(GL_NEAREST);
		GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, STAGE_SIZE, STAGE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		gl.ready = gl.copyProgram && gl.checkProgram && gl.vao && gl.vbo && gl.stage;
		gl.failed = !gl.ready;
		shared.glFailed = gl.failed;
		gl.keyed = FALSE;
		gl.key = 0;
		if (gl.failed)
			DebugLog("twin GL init failed, twins off");
		else
			DebugLog("twin GL ready");
		return gl.ready;
	}

	VOID Target(DWORD id, GLuint fbo, LONG width, LONG height)
	{
		if (gl.target == id)
			return;

		gl.target = id;
		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
		GLViewport(0, 0, width, height);
	}

	VOID TargetTwin(DWORD id)
	{
		Gpu* gpu = &gl.twins[id];
		Target(id, gpu->fbo, gpu->width * gpu->scale, gpu->height * gpu->scale);
	}

	VOID UseCopy(BOOL keyed, DWORD key)
	{
		if (gl.program != gl.copyProgram)
		{
			gl.program = gl.copyProgram;
			GLUseProgram(gl.copyProgram);
		}

		if (keyed != gl.keyed)
		{
			gl.keyed = keyed;
			GLUniform1i(gl.keyedLoc, keyed);
		}

		if (keyed && key != gl.key)
		{
			gl.key = key;
			GLUniform4f(gl.keyLoc, (key & 0xFF) / 255.0f, ((key >> 8) & 0xFF) / 255.0f, ((key >> 16) & 0xFF) / 255.0f, 1.0f);
		}
	}

	BOOL AddNdc(FLOAT nx0, FLOAT ny0, FLOAT nx1, FLOAT ny1, FLOAT u0, FLOAT v0, FLOAT u1, FLOAT v1)
	{
		if (gl.vertCount + 6 > gl.vertCapacity)
		{
			DWORD capacity = gl.vertCapacity ? gl.vertCapacity * 2 : 6 * 256;
			Vertex* verts = (Vertex*)MemoryAlloc(capacity * sizeof(Vertex));
			if (!verts)
				return FALSE;
			if (gl.verts)
			{
				MemoryCopy(verts, gl.verts, gl.vertCount * sizeof(Vertex));
				MemoryFree(gl.verts);
			}
			gl.verts = verts;
			gl.vertCapacity = capacity;
		}

		Vertex* v = &gl.verts[gl.vertCount];
		v[0] = { nx0, ny0, u0, v0 };
		v[1] = { nx1, ny0, u1, v0 };
		v[2] = { nx1, ny1, u1, v1 };
		v[3] = { nx0, ny0, u0, v0 };
		v[4] = { nx1, ny1, u1, v1 };
		v[5] = { nx0, ny1, u0, v1 };
		gl.vertCount += 6;
		return TRUE;
	}

	BOOL AddQuad(FLOAT width, FLOAT height, FLOAT x0, FLOAT y0, FLOAT x1, FLOAT y1, FLOAT u0, FLOAT v0, FLOAT u1, FLOAT v1)
	{
		return AddNdc(x0 / width * 2.0f - 1.0f, y0 / height * 2.0f - 1.0f, x1 / width * 2.0f - 1.0f, y1 / height * 2.0f - 1.0f, u0, v0, u1, v1);
	}

	VOID DrawVerts()
	{
		if (!gl.vertCount)
			return;

		GLBufferData(GL_ARRAY_BUFFER, gl.vertCount * sizeof(Vertex), gl.verts, GL_STREAM_DRAW);
		GLDrawArrays(GL_TRIANGLES, 0, gl.vertCount);
		gl.vertCount = 0;
		++gl.drawCalls;
	}

	BOOL Valid(DWORD id)
	{
		return id < TWIN_MAX && gl.twins[id].texture;
	}

	VOID DestroyGpu(DWORD id)
	{
		Gpu* gpu = &gl.twins[id];
		if (gpu->texture)
			GLDeleteTextures(1, &gpu->texture);
		if (gpu->fbo)
			GLDeleteFramebuffers(1, &gpu->fbo);
		MemoryZero(gpu, sizeof(Gpu));

		if (gl.target == id)
			gl.target = TWIN_NONE;
	}

	VOID CreateGpu(DWORD id, LONG width, LONG height, LONG scale)
	{
		if (id >= TWIN_MAX)
			return;

		DestroyGpu(id);
		if (width <= 0 || height <= 0 || scale <= 0)
			return;

		Gpu* gpu = &gl.twins[id];
		while (glGetError() != GL_NO_ERROR)
			;

		GLGenTextures(1, &gpu->texture);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		SetNearest(GL_NEAREST);
		GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width * scale, height * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
		{
			static DWORD logged;
			if (logged < LOG_FAILS)
			{
				++logged;
				DebugLog("twin texture %u %dx%d failed error %X", id, width * scale, height * scale, error);
			}
			DestroyGpu(id);
			return;
		}

		GLGenFramebuffers(1, &gpu->fbo);
		gpu->width = width;
		gpu->height = height;
		gpu->scale = scale;

		gl.target = TWIN_NONE;
		TargetTwin(id);
		GLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gpu->texture, 0);
		GLClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		GLClear(GL_COLOR_BUFFER_BIT);
	}

	VOID ReplayFill(const Cmd* cmd)
	{
		if (!Valid(cmd->dst))
			return;

		Gpu* gpu = &gl.twins[cmd->dst];
		TargetTwin(cmd->dst);

		DWORD color = cmd->value;
		GLEnable(GL_SCISSOR_TEST);
		glScissor(cmd->rect.left * gpu->scale, cmd->rect.top * gpu->scale, (cmd->rect.right - cmd->rect.left) * gpu->scale, (cmd->rect.bottom - cmd->rect.top) * gpu->scale);
		GLClearColor((color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, color >= ALPHA_COMPONENT ? 1.0f : 254.0f / 255.0f);
		GLClear(GL_COLOR_BUFFER_BIT);
		GLDisable(GL_SCISSOR_TEST);
		++gl.drawCalls;
	}

	BOOL EnsureTarget(GLuint* texture, GLuint* fbo, LONG* curW, LONG* curH, LONG width, LONG height)
	{
		if (*texture && *curW >= width && *curH >= height)
			return TRUE;

		if (*texture)
			GLDeleteTextures(1, texture);
		if (*fbo)
			GLDeleteFramebuffers(1, fbo);
		*texture = 0;
		*fbo = 0;
		*curW = *curH = 0;

		while (glGetError() != GL_NO_ERROR)
			;

		GLGenTextures(1, texture);
		GLBindTexture(GL_TEXTURE_2D, *texture);
		SetNearest(GL_NEAREST);
		GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		if (glGetError() != GL_NO_ERROR)
		{
			GLDeleteTextures(1, texture);
			*texture = 0;
			return FALSE;
		}

		GLGenFramebuffers(1, fbo);
		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, *fbo);
		GLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);
		gl.target = TWIN_NONE;

		*curW = width;
		*curH = height;
		return TRUE;
	}

	VOID ReplaySelfCopy(const Cmd* cmd)
	{
		Gpu* gpu = &gl.twins[cmd->dst];
		LONG scale = gpu->scale;
		LONG width = (cmd->rect.right - cmd->rect.left) * scale;
		LONG height = (cmd->rect.bottom - cmd->rect.top) * scale;

		LONG sw = max(gl.scratchW, width), sh = max(gl.scratchH, height);
		if (!EnsureTarget(&gl.scratch, &gl.scratchFbo, &gl.scratchW, &gl.scratchH, sw, sh))
			return;

		Target(TARGET_SCRATCH, gl.scratchFbo, gl.scratchW, gl.scratchH);
		UseCopy(FALSE, 0);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		AddQuad((FLOAT)gl.scratchW, (FLOAT)gl.scratchH, 0.0f, 0.0f, (FLOAT)width, (FLOAT)height,
			(FLOAT)cmd->rect.left / gpu->width, (FLOAT)cmd->rect.top / gpu->height, (FLOAT)cmd->rect.right / gpu->width, (FLOAT)cmd->rect.bottom / gpu->height);
		DrawVerts();

		TargetTwin(cmd->dst);
		UseCopy(cmd->flags, cmd->value);
		GLBindTexture(GL_TEXTURE_2D, gl.scratch);
		AddQuad((FLOAT)(gpu->width * scale), (FLOAT)(gpu->height * scale), (FLOAT)(cmd->x * scale), (FLOAT)(cmd->y * scale), (FLOAT)(cmd->x * scale + width), (FLOAT)(cmd->y * scale + height),
			0.0f, 0.0f, (FLOAT)width / gl.scratchW, (FLOAT)height / gl.scratchH);
		DrawVerts();
	}

	DWORD ReplayCopies(const List* list, DWORD index)
	{
		const Cmd* first = &list->cmds[index];
		if (first->src == first->dst)
		{
			if (Valid(first->dst))
				ReplaySelfCopy(first);
			return index + 1;
		}

		BOOL valid = Valid(first->src) && Valid(first->dst);
		Gpu* src = &gl.twins[first->src < TWIN_MAX ? first->src : 0];
		Gpu* dst = &gl.twins[first->dst < TWIN_MAX ? first->dst : 0];
		FLOAT tw = (FLOAT)(dst->width * dst->scale), th = (FLOAT)(dst->height * dst->scale);
		LONG scale = dst->scale;

		DWORD i = index;
		gl.vertCount = 0;
		for (; i < list->count; ++i)
		{
			const Cmd* cmd = &list->cmds[i];
			if (cmd->type != CmdCopy || cmd->src != first->src || cmd->dst != first->dst || cmd->flags != first->flags || (cmd->flags && cmd->value != first->value))
				break;

			if (valid)
			{
				LONG w = cmd->rect.right - cmd->rect.left;
				LONG h = cmd->rect.bottom - cmd->rect.top;
				AddQuad(tw, th, (FLOAT)(cmd->x * scale), (FLOAT)(cmd->y * scale), (FLOAT)((cmd->x + w) * scale), (FLOAT)((cmd->y + h) * scale),
					(FLOAT)cmd->rect.left / src->width, (FLOAT)cmd->rect.top / src->height, (FLOAT)cmd->rect.right / src->width, (FLOAT)cmd->rect.bottom / src->height);
			}
		}

		if (valid)
		{
			TargetTwin(first->dst);
			UseCopy(first->flags, first->value);
			GLBindTexture(GL_TEXTURE_2D, src->texture);
			DrawVerts();
		}
		gl.vertCount = 0;

		return i;
	}

	VOID FlushPlaced()
	{
		for (DWORD i = 0; i < gl.placedCount; ++i)
		{
			DWORD dst = gl.placed[i].dst;
			BOOL seen = FALSE;
			for (DWORD k = 0; k < i && !seen; ++k)
				seen = gl.placed[k].dst == dst;
			if (seen)
				continue;

			Gpu* gpu = &gl.twins[dst];
			LONG scale = gpu->scale;
			FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);

			gl.vertCount = 0;
			for (DWORD k = i; k < gl.placedCount; ++k)
			{
				const Placed* p = &gl.placed[k];
				if (p->dst != dst)
					continue;

				LONG w = p->rect.right - p->rect.left;
				LONG h = p->rect.bottom - p->rect.top;
				AddQuad(tw, th, (FLOAT)(p->rect.left * scale), (FLOAT)(p->rect.top * scale), (FLOAT)(p->rect.right * scale), (FLOAT)(p->rect.bottom * scale),
					(FLOAT)p->sx / STAGE_SIZE, (FLOAT)p->sy / STAGE_SIZE, (FLOAT)(p->sx + w) / STAGE_SIZE, (FLOAT)(p->sy + h) / STAGE_SIZE);
			}

			TargetTwin(dst);
			UseCopy(FALSE, 0);
			GLBindTexture(GL_TEXTURE_2D, gl.stage);
			DrawVerts();
		}

		gl.placedCount = 0;
	}

	DWORD ReplayUploads(const List* list, DWORD index)
	{
		GLBindTexture(GL_TEXTURE_2D, gl.stage);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		LONG shelfX = 0, shelfY = 0, shelfH = 0;
		DWORD i = index;
		for (; i < list->count && list->cmds[i].type == CmdUpload; ++i)
		{
			const Cmd* cmd = &list->cmds[i];
			if (!Valid(cmd->dst))
				continue;

			LONG width = cmd->rect.right - cmd->rect.left;
			LONG height = cmd->rect.bottom - cmd->rect.top;
			const DWORD* pixels = (const DWORD*)(list->arena + cmd->value);
			GLPixelStorei(GL_UNPACK_ROW_LENGTH, width);

			for (LONG ty = 0; ty < height; ty += STAGE_SIZE)
			{
				LONG th = min(height - ty, STAGE_SIZE);
				for (LONG tx = 0; tx < width; tx += STAGE_SIZE)
				{
					LONG tw = min(width - tx, STAGE_SIZE);

					if (shelfX + tw > STAGE_SIZE)
					{
						shelfY += shelfH;
						shelfX = 0;
						shelfH = 0;
					}

					if (shelfY + th > STAGE_SIZE)
					{
						FlushPlaced();
						shelfX = shelfY = shelfH = 0;
					}

					if (gl.placedCount == gl.placedCapacity)
					{
						DWORD capacity = gl.placedCapacity ? gl.placedCapacity * 2 : 256;
						Placed* placed = (Placed*)MemoryAlloc(capacity * sizeof(Placed));
						if (!placed)
							continue;
						if (gl.placed)
						{
							MemoryCopy(placed, gl.placed, gl.placedCount * sizeof(Placed));
							MemoryFree(gl.placed);
						}
						gl.placed = placed;
						gl.placedCapacity = capacity;
					}

					GLTexSubImage2D(GL_TEXTURE_2D, 0, shelfX, shelfY, tw, th, GL_RGBA, GL_UNSIGNED_BYTE, pixels + ty * width + tx);

					Placed* p = &gl.placed[gl.placedCount++];
					p->dst = cmd->dst;
					p->sx = shelfX;
					p->sy = shelfY;
					SetRect(&p->rect, cmd->rect.left + tx, cmd->rect.top + ty, cmd->rect.left + tx + tw, cmd->rect.top + ty + th);

					shelfX += tw;
					if (th > shelfH)
						shelfH = th;
				}
			}
		}

		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		FlushPlaced();
		return i;
	}

	VOID Check(const Cmd* cmd, const List* list)
	{
		if (!Valid(cmd->dst))
			return;

		Gpu* gpu = &gl.twins[cmd->dst];
		LONG width = gpu->width, height = gpu->height;
		if (width != cmd->x || height != cmd->y)
			return;

		if (gl.checkW != width || gl.checkH != height)
		{
			if (gl.checkData)
				MemoryFree(gl.checkData);
			gl.checkData = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
			if (gl.check)
				GLDeleteTextures(1, &gl.check);
			if (gl.checkFbo)
				GLDeleteFramebuffers(1, &gl.checkFbo);
			gl.check = gl.checkFbo = 0;
			gl.checkW = gl.checkH = 0;
		}

		if (!gl.checkData || !EnsureTarget(&gl.check, &gl.checkFbo, &gl.checkW, &gl.checkH, width, height))
			return;

		Target(TARGET_CHECK, gl.checkFbo, width, height);
		gl.program = gl.checkProgram;
		GLUseProgram(gl.checkProgram);
		GLUniform1i(gl.scaleLoc, gpu->scale);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		AddQuad(1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
		DrawVerts();

		GLBindFramebuffer(GL_READ_FRAMEBUFFER, gl.checkFbo);
		GLPixelStorei(GL_PACK_ALIGNMENT, 4);
		GLReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, gl.checkData);
		GLBindFramebuffer(GL_READ_FRAMEBUFFER, gl.readFbo);

		const DWORD* ref = (const DWORD*)(list->arena + cmd->value);
		LONG tilesX = (width + TILE - 1) / TILE;
		LONG tilesY = (height + TILE - 1) / TILE;

		struct { LONG x, y; DWORD count; } worst[WORST] = {};
		DWORD badTiles = 0, badPixels = 0;
		for (LONG ty = 0; ty < tilesY; ++ty)
		{
			for (LONG tx = 0; tx < tilesX; ++tx)
			{
				DWORD count = 0;
				LONG x0 = tx * TILE, y0 = ty * TILE;
				LONG x1 = min(x0 + TILE, width), y1 = min(y0 + TILE, height);
				for (LONG y = y0; y < y1; ++y)
				{
					const DWORD* a = ref + y * width;
					const DWORD* b = gl.checkData + y * width;
					for (LONG x = x0; x < x1; ++x)
						if ((a[x] ^ b[x]) & 0x00FFFFFF)
							++count;
				}

				if (!count)
					continue;

				++badTiles;
				badPixels += count;

				DWORD slot = WORST;
				while (slot && worst[slot - 1].count < count)
					--slot;
				if (slot < WORST)
				{
					for (DWORD k = WORST - 1; k > slot; --k)
						worst[k] = worst[k - 1];
					worst[slot].x = x0;
					worst[slot].y = y0;
					worst[slot].count = count;
				}
			}
		}

		CHAR text[WORST * 32 + 1];
		CHAR* ptr = text;
		*ptr = 0;
		for (DWORD k = 0; k < WORST && worst[k].count; ++k)
			ptr += StrPrint(ptr, " (%d,%d,%u)", worst[k].x, worst[k].y, worst[k].count);

		DebugLog("twin check twin %u %dx%d N %d bad tiles %u/%u px %u worst%s", cmd->dst, width, height, gpu->scale, badTiles, tilesX * tilesY, badPixels, badTiles ? text : " none");
	}

	VOID Replay(const List* list)
	{
		DWORD i = 0;
		while (i < list->count)
		{
			const Cmd* cmd = &list->cmds[i];
			switch (cmd->type)
			{
			case CmdCreate:
				CreateGpu(cmd->dst, cmd->rect.right, cmd->rect.bottom, (LONG)cmd->value);
				break;

			case CmdDestroy:
				if (cmd->dst < TWIN_MAX)
					DestroyGpu(cmd->dst);
				break;

			case CmdReset:
				for (DWORD k = 0; k < TWIN_MAX; ++k)
					if (gl.twins[k].texture || gl.twins[k].fbo)
						DestroyGpu(k);
				break;

			case CmdFill:
				ReplayFill(cmd);
				break;

			case CmdUpload:
				i = ReplayUploads(list, i);
				continue;

			case CmdCopy:
				i = ReplayCopies(list, i);
				continue;

			case CmdPresent:
				gl.shown.valid = TRUE;
				gl.shown.id = cmd->dst;
				gl.shown.hole = cmd->rect;
				gl.shown.flags = cmd->flags;
				if (cmd->flags & PresentCheck)
					Check(cmd, list);
				break;

			default:
				break;
			}

			++i;
		}
	}

	VOID AddShowQuad(const RECT* part, const Gpu* gpu, LONG baseX, LONG baseY, DWORD frameWidth, DWORD frameHeight)
	{
		if (part->left >= part->right || part->top >= part->bottom)
			return;

		FLOAT fw = (FLOAT)frameWidth, fh = (FLOAT)frameHeight;
		AddNdc((part->left - baseX) / fw * 2.0f - 1.0f, 1.0f - (part->top - baseY) / fh * 2.0f, (part->right - baseX) / fw * 2.0f - 1.0f, 1.0f - (part->bottom - baseY) / fh * 2.0f,
			(FLOAT)part->left / gpu->width, (FLOAT)part->top / gpu->height, (FLOAT)part->right / gpu->width, (FLOAT)part->bottom / gpu->height);
	}

	BOOL Show(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight, GLuint drawFbo)
	{
		if (!gl.shown.valid || !Valid(gl.shown.id) || (gl.shown.flags & PresentBack) || rect->width <= 0 || rect->height <= 0 || !frameWidth || !frameHeight)
			return FALSE;

		Gpu* gpu = &gl.twins[gl.shown.id];
		gl.target = TWIN_NONE;
		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
		GLViewport(rect->x, rect->y + offset, rect->width, rect->height);

		LONG baseX = (LONG)(config.mode->width - frameWidth) >> 1;
		LONG baseY = (LONG)(config.mode->height - frameHeight) >> 1;
		RECT frame = { baseX, baseY, baseX + (LONG)frameWidth, baseY + (LONG)frameHeight };

		gl.vertCount = 0;
		RECT hole;
		BOOL zoomed = ((gl.shown.flags & PresentMapZoom) || Config::IsMapZoomed()) && !(gl.shown.flags & PresentZoomed);
		if (zoomed && IntersectRect(&hole, &gl.shown.hole, &frame))
		{
			RECT parts[4] = {
				{ frame.left, frame.top, frame.right, hole.top },
				{ frame.left, hole.bottom, frame.right, frame.bottom },
				{ frame.left, hole.top, hole.left, hole.bottom },
				{ hole.right, hole.top, frame.right, hole.bottom }
			};
			for (DWORD k = 0; k < 4; ++k)
				AddShowQuad(&parts[k], gpu, baseX, baseY, frameWidth, frameHeight);
		}
		else
			AddShowQuad(&frame, gpu, baseX, baseY, frameWidth, frameHeight);

		UseCopy(FALSE, 0);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		DrawVerts();
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		return TRUE;
	}

	BOOL Prepare()
	{
		if (shared.state != 2)
			return FALSE;

		Lock();
		BOOL pending = shared.head != NULL;
		Unlock();
		return pending;
	}

	BOOL Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight)
	{
		if (shared.state != 2)
			return FALSE;

		Lock();
		List* lists = shared.head;
		shared.head = shared.tail = NULL;
		shared.queued = 0;
		shared.bytes = 0;
		Unlock();

		if (!lists && !gl.shown.valid)
			return FALSE;

		GLint program, vao, buffer, active, texture0, texture1, drawFbo, readFbo, viewport[4], scissor[4];
		GLfloat clear[4];
		GLGetIntegerv(GL_CURRENT_PROGRAM, &program);
		GLGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		GLGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		GLGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		GLGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
		GLGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
		GLGetIntegerv(GL_VIEWPORT, viewport);
		GLGetIntegerv(GL_SCISSOR_BOX, scissor);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
		GLboolean blend = glIsEnabled(GL_BLEND);
		GLboolean scissorTest = glIsEnabled(GL_SCISSOR_TEST);
		GLActiveTexture(GL_TEXTURE1);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture1);
		GLActiveTexture(GL_TEXTURE0);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);

		BOOL shown = FALSE;
		DWORD replayed = 0;
		DWORD us = 0;
		if (InitGL())
		{
			gl.target = TWIN_NONE;
			gl.program = 0;
			gl.drawCalls = 0;
			gl.readFbo = readFbo;
			GLUseProgram(0);
			GLBindVertexArray(gl.vao);
			GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
			GLDisable(GL_BLEND);
			GLDisable(GL_SCISSOR_TEST);

			LARGE_INTEGER start, end;
			QueryPerformanceCounter(&start);
			for (List* list = lists; list; list = list->next)
			{
				Replay(list);
				++replayed;
			}
			QueryPerformanceCounter(&end);
			if (gl.frequency.QuadPart)
				us = DWORD((end.QuadPart - start.QuadPart) * 1000000 / gl.frequency.QuadPart);

			if (shared.show)
				shown = Show(rect, offset, frameWidth, frameHeight, drawFbo);
		}

		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
		GLBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
		GLViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
		GLClearColor(clear[0], clear[1], clear[2], clear[3]);
		if (blend)
			GLEnable(GL_BLEND);
		else
			GLDisable(GL_BLEND);
		if (scissorTest)
			GLEnable(GL_SCISSOR_TEST);
		else
			GLDisable(GL_SCISSOR_TEST);
		GLActiveTexture(GL_TEXTURE1);
		GLBindTexture(GL_TEXTURE_2D, texture1);
		GLActiveTexture(GL_TEXTURE0);
		GLBindTexture(GL_TEXTURE_2D, texture0);
		GLActiveTexture(active);
		GLBindBuffer(GL_ARRAY_BUFFER, buffer);
		GLBindVertexArray(vao);
		GLUseProgram(program);

		Lock();
		while (lists)
		{
			List* next = lists->next;
			RecycleList(lists);
			lists = next;
		}

		if (replayed)
		{
			shared.replays += replayed;
			shared.drawCalls += gl.drawCalls;
			shared.lastDrawCalls = gl.drawCalls;
			++shared.draws;
			shared.replayUs += us;
			shared.lastReplayUs = us;
			if (us > shared.replayMaxUs)
				shared.replayMaxUs = us;
		}
		Unlock();

		return shown;
	}

	VOID ReleaseGL()
	{
		BOOL wasReady = gl.ready;
		if (gl.ready)
		{
			for (DWORD i = 0; i < TWIN_MAX; ++i)
				if (gl.twins[i].texture || gl.twins[i].fbo)
					DestroyGpu(i);

			GLDeleteTextures(1, &gl.stage);
			if (gl.scratch)
				GLDeleteTextures(1, &gl.scratch);
			if (gl.scratchFbo)
				GLDeleteFramebuffers(1, &gl.scratchFbo);
			if (gl.check)
				GLDeleteTextures(1, &gl.check);
			if (gl.checkFbo)
				GLDeleteFramebuffers(1, &gl.checkFbo);
			GLDeleteBuffers(1, &gl.vbo);
			GLDeleteVertexArrays(1, &gl.vao);
			GLDeleteProgram(gl.copyProgram);
			GLDeleteProgram(gl.checkProgram);
		}

		if (gl.checkData)
			MemoryFree(gl.checkData);
		if (gl.verts)
			MemoryFree(gl.verts);
		if (gl.placed)
			MemoryFree(gl.placed);
		MemoryZero(&gl, sizeof(gl));

		if (shared.state == 2)
		{
			Lock();
			DropQueued();
			shared.glFailed = FALSE;
			Unlock();
		}
		InterlockedIncrement(&shared.gen);

		if (wasReady)
			DebugLog("twin GL released");
	}
#pragma endregion
}
