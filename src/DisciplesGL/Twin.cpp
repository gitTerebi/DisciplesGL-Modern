#include "StdAfx.h"
#include "intrin.h"
#include "Twin.h"
#include "Config.h"
#include "GLib.h"
#include "ImageReplace.h"
#include "PngLib.h"

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
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif
#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#endif
#ifndef GL_FUNC_REVERSE_SUBTRACT
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
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
#define CHECK_PERIOD 5000
#define LOG_TWINS 64
#define LOG_FAILS 16
#define TILE 16
#define WORST 10
#define REPAIR_LOGS 32
#define REPAIR_LOGS_FRAME 4
#define REPAIR_TOP 3
#define ART_MAX 64
#define MAX_EXPECT 32768
#define MAX_IMAGES 8192
#define EXPECT_BUCKETS 8192
#define REJECT_LOGS 16
#define EXPECT_SCAN 256
#define ART_LOGS 32
#define LINK_MAX 256
#define GROUND_LOGS 8
#define SCREEN_LOGS 64
#define SHOW_LOGS 32
#define SYNC_MAX 8
#define PIXEL_MODES 5
#define UNMATCHED_LOGS 16
#define LOGS_FRAME 4
#define SHOT_DELAY 15000
#define SHOT_SCREEN_W 1600
#define SHOT_SCREEN_H 1200
#define SHOT_SHEET 2048

typedef VOID(__stdcall* GLBLENDEQUATION)(GLenum mode);

namespace Twin
{
	enum CmdType { CmdCreate, CmdDestroy, CmdCopy, CmdFill, CmdUpload, CmdPresent, CmdReset, CmdArt, CmdGround, CmdPixel };

	enum PresentFlags { PresentMapZoom = 1, PresentBack = 2, PresentZoomed = 4, PresentCheck = 8, PresentShot = 16 };

	struct Cmd {
		DWORD type;
		DWORD dst;
		DWORD src;
		RECT rect;
		RECT clip;
		RECT place;
		LONG x;
		LONG y;
		DWORD value;
		DWORD flags;
		DWORD extra;
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

	struct ArtLink {
		RECT rect;
		LONG frame;
		BOOL flipped;
		LONG placeW;
		LONG placeH;
		LONG cx;
		LONG cy;
		DWORD lastUse;
	};

	struct Expect {
		LONG dx;
		LONG dy;
		LONG sx;
		LONG sy;
		LONG w;
		LONG h;
		DWORD image;
		DWORD next;
		BOOL used;
	};

	struct Image {
		LONG frame;
		BOOL flipped;
		RECT place;
		RECT clip;
	};

	struct CheckResult {
		BOOL valid;
		DWORD tick;
		DWORD twin;
		DWORD bad;
		DWORD tiles;
		DWORD px;
		DWORD art;
		DWORD artDiffer;
	};

	struct Record {
		StateBuffer* owner;
		BYTE* base;
		LONG pitch;
		LONG width;
		LONG height;
		DWORD* mirror;
		DWORD repairs;
		DWORD repairPx;
		BOOL keyed;
		DWORD key;
		DWORD artCount;
		RECT art[ART_MAX];
		ArtLink* links;
		DWORD linkCount;
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
		BOOL tileCheck;
		BOOL tileKeyDown;
		BOOL tintKeyDown;
		struct {
			StateBuffer* owner;
			DWORD id;
			RECT rect;
		} syncs[SYNC_MAX];
		DWORD syncCount;
		DWORD pixelOps[PIXEL_MODES];
		DWORD pixelArt;
		DWORD pixelUntwinned;
		DWORD pixelSelf;
		DWORD presentNoTwin;
		DWORD presentLogTick;
		DWORD screenLogs;
		BOOL shotRequest;
		DWORD lastArtSource;
		DWORD frameNo;
		DWORD repairsLogged;
		DWORD frameRepairLogs;
		DWORD repairTiles;
		DWORD repairRects;
		DWORD repairPx;
		LARGE_INTEGER frequency;
		LONGLONG verifyFrame;
		LONGLONG verifySum;
		LONGLONG verifyMax;
		BYTE bad[TWIN_MAX_SIZE / TILE];
	} game;

	struct {
		Expect expect[MAX_EXPECT];
		Image images[MAX_IMAGES];
		DWORD bucket[EXPECT_BUCKETS];
		DWORD count;
		DWORD imageCount;
		DWORD head;
		DWORD expected;
		DWORD matched;
		DWORD notReady;
		DWORD unmatched;
		DWORD overflow;
		DWORD duplicates;
		DWORD rejected;
		DWORD draws;
		DWORD cacheHits;
		DWORD cacheMisses;
		DWORD cachedDraws;
		DWORD useClock;
		DWORD groundArts;
		DWORD groundNoTwin;
		DWORD groundLogs;
		DWORD px;
		DWORD artLogs;
		DWORD unmatchedLogs;
		DWORD rejectLogs;
		DWORD frameLogs;
		BOOL checked;
		DWORD firstTick;
		BOOL shotTaken;
	} art;

	struct {
		CRITICAL_SECTION cs;
		volatile LONG state;
		volatile LONG gen;
		volatile BOOL show;
		volatile BOOL tint;
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
		DWORD artDraws;
		DWORD artFallbacks;
		DWORD artRejected;
		DWORD artCalls;
		DWORD groundReplayed;
		DWORD groundFallbacks;
		DWORD pixelReplayed;
		DWORD pixelCalls;
		DWORD showOpaque;
		DWORD showBack;
		DWORD showInvalid;
		DWORD showGone;
		DWORD showUs;
		DWORD showSamples;
		DWORD showMaxUs;
		DWORD showOff;
		DWORD showOther;
		CheckResult check;
	} shared = { {}, 0, 0, TRUE, FALSE };

	struct {
		BOOL ready;
		BOOL failed;
		GLuint copyProgram;
		GLuint checkProgram;
		GLuint showProgram;
		GLint ratioLoc;
		GLint keyedLoc;
		GLint keyLoc;
		GLint artLoc;
		GLint tintLoc;
		GLint emptyLoc;
		GLint weightLoc;
		GLBLENDEQUATION blendEquation;
		GLint opaqueLoc;
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
		DWORD artDraws;
		DWORD artFallbacks;
		DWORD artRejected;
		DWORD artCalls;
		DWORD rejectLogs;
		DWORD showOpaque;
		DWORD showBack;
		DWORD showInvalid;
		DWORD showGone;
		DWORD showOther;
		DWORD groundReplayed;
		DWORD groundFallbacks;
		DWORD pixelReplayed;
		DWORD pixelCalls;
		BYTE* checkMask;
		DWORD showView;
		DWORD showTwin;
		DWORD showFrame;
		DWORD showLogs;
		DWORD showCount;
		DWORD showUs;
		DWORD showSamples;
		DWORD showMaxUs;
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
		twin->linkCount = 0;
		twin->count = 1;
		SetRect(&twin->dirty[0], 0, 0, twin->width, twin->height);
	}

	VOID Upload(Record* twin, DWORD id, const RECT* rect);
	VOID DropLinks(Record* twin, const RECT* rect);

	VOID Append(Record* twin, const RECT* rect)
	{
		if (twin->count == DIRTY_MAX)
		{
			// every earlier dirty write is complete, so flushing them now is exact; a bounding box would later wipe art copied in between
			RECT pending[DIRTY_MAX];
			DWORD count = twin->count;
			MemoryCopy(pending, twin->dirty, count * sizeof(RECT));
			twin->count = 0;
			for (DWORD i = 0; i < count; ++i)
				Upload(twin, DWORD(twin - game.twins), &pending[i]);
		}

		twin->dirty[twin->count++] = *rect;
	}

	VOID MarkDirty(Record* twin, RECT rect)
	{
		if (!Clip(twin, &rect))
			return;

		if (twin->linkCount)
			DropLinks(twin, &rect);

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

	VOID Convert(const Record* twin, DWORD* dst, const DWORD* src, LONG width, LONG height)
	{
		BOOL keyed = twin->keyed;
		DWORD key = twin->key;
		for (LONG y = height; y; --y)
		{
			for (LONG x = 0; x < width; ++x)
			{
				DWORD pixel = src[x];
				// key pixels become transparent black so premultiplied art blends over them; alpha 254 marks a 1x alpha byte other than 255, so keyed copies keep the CPU's exact DWORD compare
				if (keyed && pixel == key)
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

		if (twin->artCount < ART_MAX)
		{
			twin->art[twin->artCount++] = *rect;
			return;
		}

		RECT bound = *rect;
		for (DWORD i = 0; i < twin->artCount; ++i)
			UnionRect(&bound, &bound, &twin->art[i]);
		twin->artCount = 1;
		twin->art[0] = bound;
	}

	VOID CoverArt(Record* twin, const RECT* rect)
	{
		for (DWORD i = twin->artCount; i; --i)
			if (Contains(rect, &twin->art[i - 1]))
				twin->art[i - 1] = twin->art[--twin->artCount];
	}

	VOID DropLinks(Record* twin, const RECT* rect)
	{
		RECT part;
		for (DWORD i = twin->linkCount; i; --i)
			if (IntersectRect(&part, rect, &twin->links[i - 1].rect))
				twin->links[i - 1] = twin->links[--twin->linkCount];
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
		Convert(twin, dst, src, width, height);

		Cmd* cmd = Emit(CmdUpload);
		if (!cmd)
			return;
		cmd->dst = id;
		cmd->rect = *rect;
		cmd->value = offset;

		++game.list->counters.uploads;
		game.list->counters.pixels += width * height;
		CoverArt(twin, rect);
		DropLinks(twin, rect);

		if (twin->mirror)
		{
			src = (const DWORD*)twin->base + rect->top * twin->pitch + rect->left;
			DWORD* mirror = twin->mirror + rect->top * twin->width + rect->left;
			for (LONG y = height; y; --y)
			{
				MemoryCopy(mirror, src, width * sizeof(DWORD));
				src += twin->pitch;
				mirror += twin->width;
			}
		}
	}

	BOOL RowEqual(const DWORD* a, const DWORD* b, LONG count)
	{
		if (config.cpu.sse2 && count >= 4)
		{
			__m128i acc = _mm_set1_epi32(-1);
			LONG blocks = count >> 2;
			const __m128i* pa = (const __m128i*)a;
			const __m128i* pb = (const __m128i*)b;
			for (LONG i = 0; i < blocks; ++i)
				acc = _mm_and_si128(acc, _mm_cmpeq_epi32(_mm_loadu_si128(pa + i), _mm_loadu_si128(pb + i)));
			if (_mm_movemask_epi8(acc) != 0xFFFF)
				return FALSE;

			LONG done = blocks << 2;
			a += done;
			b += done;
			count -= done;
		}

		for (LONG i = 0; i < count; ++i)
			if (a[i] != b[i])
				return FALSE;
		return TRUE;
	}

	VOID Repair(Record* twin, DWORD id, const RECT* rect, DWORD tiles, BOOL present)
	{
		Upload(twin, id, rect);

		DWORD px = (rect->right - rect->left) * (rect->bottom - rect->top);
		twin->repairs += tiles;
		twin->repairPx += px;
		game.repairTiles += tiles;
		game.repairPx += px;
		++game.repairRects;

		if (game.repairsLogged < REPAIR_LOGS && game.frameRepairLogs < REPAIR_LOGS_FRAME)
		{
			++game.repairsLogged;
			++game.frameRepairLogs;
			DebugLog("twin repair buf=%p %d,%d %dx%d at frame %u (%s, twin %u %dx%d)", twin->owner, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
				game.frameNo, present ? "present" : "copy source", id, twin->width, twin->height);
		}
	}

	BOOL RowSkipped(const RECT* skip, DWORD skipCount, LONG y, LONG left, LONG right)
	{
		for (DWORD i = 0; i < skipCount; ++i)
			if (y >= skip[i].top && y < skip[i].bottom && skip[i].left <= left && skip[i].right >= right)
				return TRUE;
		return FALSE;
	}

	VOID Verify(Record* twin, DWORD id, const RECT* rect, const RECT* skip, DWORD skipCount, BOOL present)
	{
		if (!game.tileCheck || !twin->mirror)
			return;

		LARGE_INTEGER start, end;
		QueryPerformanceCounter(&start);

		LONG width = rect->right - rect->left;
		LONG cols = (width + TILE - 1) / TILE;
		for (LONG band = rect->top; band < rect->bottom; band += TILE)
		{
			LONG bottom = min(band + TILE, rect->bottom);
			BOOL any = FALSE;
			for (LONG y = band; y < bottom; ++y)
			{
				if (skipCount && RowSkipped(skip, skipCount, y, rect->left, rect->right))
					continue;

				const DWORD* real = (const DWORD*)twin->base + y * twin->pitch + rect->left;
				const DWORD* mirror = twin->mirror + y * twin->width + rect->left;
				if (RowEqual(real, mirror, width))
					continue;

				if (!any)
				{
					any = TRUE;
					MemoryZero(game.bad, cols);
				}

				for (LONG c = 0; c < cols; ++c)
					if (!game.bad[c] && !RowEqual(real + c * TILE, mirror + c * TILE, min(TILE, width - c * TILE)))
						game.bad[c] = TRUE;
			}

			if (!any)
				continue;

			for (LONG c = 0; c < cols;)
			{
				if (!game.bad[c])
				{
					++c;
					continue;
				}

				LONG first = c;
				while (c < cols && game.bad[c])
					++c;

				RECT bad = { rect->left + first * TILE, band, min(rect->left + c * TILE, rect->right), bottom };
				Repair(twin, id, &bad, c - first, present);
			}
		}

		QueryPerformanceCounter(&end);
		game.verifyFrame += end.QuadPart - start.QuadPart;
	}

	VOID MirrorCopy(Record* target, const RECT* to, const Record* source, const RECT* from, BOOL keyed, DWORD key)
	{
		if (!target->mirror)
			return;

		LONG width = to->right - to->left;
		LONG height = to->bottom - to->top;
		const DWORD* src = (const DWORD*)source->base + from->top * source->pitch + from->left;
		DWORD* dst = target->mirror + to->top * target->width + to->left;

		if (!keyed)
		{
			for (LONG y = height; y; --y)
			{
				MemoryCopy(dst, src, width * sizeof(DWORD));
				src += source->pitch;
				dst += target->width;
			}
			return;
		}

		DWORD skip = key | ALPHA_COMPONENT;
		LONG blocks = config.cpu.sse2 ? width >> 2 : 0;
		__m128i k = _mm_set1_epi32(skip);
		for (LONG y = height; y; --y)
		{
			for (LONG i = 0; i < blocks; ++i)
			{
				__m128i a = _mm_loadu_si128((const __m128i*)src + i);
				__m128i mask = _mm_cmpeq_epi32(a, k);
				__m128i* d = (__m128i*)dst + i;
				_mm_storeu_si128(d, _mm_or_si128(_mm_andnot_si128(mask, a), _mm_and_si128(mask, _mm_loadu_si128(d))));
			}

			for (LONG x = blocks << 2; x < width; ++x)
				if (src[x] != skip)
					dst[x] = src[x];

			src += source->pitch;
			dst += target->width;
		}
	}

	VOID MirrorFill(Record* twin, const RECT* rect, DWORD color)
	{
		if (!twin->mirror)
			return;

		LONG width = rect->right - rect->left;
		DWORD* dst = twin->mirror + rect->top * twin->width + rect->left;
		for (LONG y = rect->bottom - rect->top; y; --y)
		{
			for (LONG x = 0; x < width; ++x)
				dst[x] = color;
			dst += twin->width;
		}
	}

	VOID FlushRegion(Record* twin, DWORD id, const RECT* rect, BOOL whole = FALSE)
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
	}

	VOID DrainSync()
	{
		for (DWORD i = 0; i < game.syncCount; ++i)
		{
			Record* twin = &game.twins[game.syncs[i].id];
			const RECT* rect = &game.syncs[i].rect;
			if (twin->owner != game.syncs[i].owner || !twin->mirror)
				continue;

			LONG width = rect->right - rect->left;
			const DWORD* src = (const DWORD*)twin->base + rect->top * twin->pitch + rect->left;
			DWORD* mirror = twin->mirror + rect->top * twin->width + rect->left;
			for (LONG y = rect->bottom - rect->top; y; --y)
			{
				MemoryCopy(mirror, src, width * sizeof(DWORD));
				src += twin->pitch;
				mirror += twin->width;
			}
		}
		game.syncCount = 0;
	}

	VOID DeferSync(Record* twin, DWORD id, const RECT* rect)
	{
		// the hook writes 1x right after this call, so the mirror can only take the real pixels at the next entry point
		if (game.syncCount == SYNC_MAX)
			DrainSync();
		game.syncs[game.syncCount].owner = twin->owner;
		game.syncs[game.syncCount].id = id;
		game.syncs[game.syncCount].rect = *rect;
		++game.syncCount;
	}

	VOID Sync()
	{
		DrainSync();

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
		game.lastArtSource = TWIN_NONE;

		CHAR env[8];
		DWORD length = GetEnvironmentVariable("DGL_TWIN_TILECHECK", env, sizeof(env));
		game.tileCheck = !(length && length < sizeof(env) && env[0] == '0');
		QueryPerformanceFrequency(&game.frequency);

		DebugLog("twin scale N=%d screen %dx%d mode %dx%d tile check %s", scale, screenW, screenH, width, height, game.tileCheck ? "on" : "off");
		DebugLog("twin display config borders allowed %u active %u type %u | background allowed %u enabled %u | interpolation %u upscaling %u x%u | zoomed %u renderer %u gl %08X",
			config.borders.allowed, config.borders.active, config.borders.type, config.background.allowed, config.background.enabled,
			config.image.interpolation, config.image.upscaling, config.image.value, Config::IsZoomed(), config.renderer, config.gl.version.value);
	}

	VOID Release(Record* twin, DWORD id)
	{
		if (Active())
		{
			Cmd* cmd = Emit(CmdDestroy);
			if (cmd)
				cmd->dst = id;
		}

		BOOL screenSized = twin->width == *(LONG*)&config.mode->width && twin->height == *(LONG*)&config.mode->height;
		if (game.logged < LOG_TWINS || (screenSized && game.screenLogs < SCREEN_LOGS))
		{
			if (game.logged < LOG_TWINS)
				++game.logged;
			else
				++game.screenLogs;
			DebugLog("twin destroy %u %dx%d buffer %08X at frame %u", id, twin->width, twin->height, twin->owner, game.frameNo);
		}

		--game.alive;
		game.bytes -= (LONGLONG)twin->width * twin->height * sizeof(DWORD) * game.scale * game.scale;
		if (twin->mirror)
			MemoryFree(twin->mirror);
		twin->mirror = NULL;
		if (twin->links)
			MemoryFree(twin->links);
		twin->links = NULL;
		twin->linkCount = 0;
		twin->owner = NULL;
		twin->base = NULL;
		twin->count = 0;
		twin->repairs = 0;
		twin->repairPx = 0;
		twin->keyed = FALSE;
		twin->artCount = 0;

		while (game.top && !game.twins[game.top - 1].owner)
			--game.top;
	}

	DWORD ExpectBucket(LONG x, LONG y)
	{
		return ((DWORD)x * 73856093u ^ (DWORD)y * 19349663u) & (EXPECT_BUCKETS - 1);
	}

	VOID ExpectArt(LONG frame, BOOL flipped, const RECT* place, const RECT* clip, const ArtPart* parts, DWORD count)
	{
		if (!Active() || !place || !parts || !count)
			return;

		if (art.imageCount >= MAX_IMAGES || art.count >= MAX_EXPECT)
		{
			art.overflow += count;
			return;
		}

		DWORD imageIndex = art.imageCount++;
		DWORD added = 0;
		Image* image = &art.images[imageIndex];
		image->frame = frame;
		image->flipped = flipped;
		image->place = *place;
		if (clip)
			image->clip = *clip;
		else
			image->clip = *place;

		for (DWORD i = 0; i < count; ++i)
		{
			const ArtPart* part = &parts[i];
			if (part->w <= 0 || part->h <= 0)
				continue;

			if (art.count >= MAX_EXPECT)
			{
				art.overflow += count - i;
				break;
			}

			DWORD* bucket = &art.bucket[ExpectBucket(part->dx, part->dy)];
			BOOL duplicate = FALSE;
			for (DWORD next = *bucket; next && !duplicate; next = art.expect[next - 1].next)
			{
				const Expect* old = &art.expect[next - 1];
				const Image* oldImage = &art.images[old->image];
				duplicate = !old->used && old->dx == part->dx && old->dy == part->dy && old->sx == part->sx && old->sy == part->sy && old->w == part->w && old->h == part->h
					&& oldImage->frame == frame && !oldImage->flipped == !flipped;
			}
			if (duplicate)
			{
				++art.duplicates;
				continue;
			}

			DWORD index = art.count++;
			Expect* e = &art.expect[index];
			e->dx = part->dx;
			e->dy = part->dy;
			e->sx = part->sx;
			e->sy = part->sy;
			e->w = part->w;
			e->h = part->h;
			e->image = imageIndex;
			e->used = FALSE;
			e->next = *bucket;
			*bucket = index + 1;
			++art.expected;
			++added;
		}

		if (!added && imageIndex == art.imageCount - 1)
			--art.imageCount;
	}

	BOOL Inside(const Expect* e, LONG x, LONG y, LONG w, LONG h, LONG sx, LONG sy)
	{
		return x >= e->dx && y >= e->dy && x + w <= e->dx + e->w && y + h <= e->dy + e->h && sx - e->sx == x - e->dx && sy - e->sy == y - e->dy;
	}

	Expect* Match(LONG x, LONG y, LONG w, LONG h, LONG sx, LONG sy)
	{
		Expect* best = NULL;
		for (DWORD next = art.bucket[ExpectBucket(x, y)]; next; next = art.expect[next - 1].next)
		{
			Expect* e = &art.expect[next - 1];
			if (e->used || e->dx != x || e->dy != y || e->sx != sx || e->sy != sy || w > e->w || h > e->h)
				continue;
			best = e;
			if (w == e->w && h == e->h)
				break;
		}
		if (best)
			return best;

		DWORD end = min(art.count, art.head + EXPECT_SCAN);
		for (DWORD i = art.head; i < end; ++i)
		{
			Expect* e = &art.expect[i];
			if (!e->used && Inside(e, x, y, w, h, sx, sy))
				return e;
		}
		return NULL;
	}

	BOOL RecordArt(Record* target, DWORD dstId, const RECT* to, DWORD srcId, const RECT* from, LONG frame, BOOL flipped, const RECT* place, BOOL keyed, DWORD key, BOOL cached, BOOL half)
	{
		if (!ImageReplace::ArtReady(frame))
		{
			++art.notReady;
			return FALSE;
		}

		Cmd* cmd = Emit(CmdArt);
		if (!cmd)
			return FALSE;
		cmd->dst = dstId;
		cmd->src = srcId;
		cmd->rect = *to;
		cmd->clip = *from;
		cmd->place = *place;
		cmd->value = (DWORD)frame;
		cmd->flags = (flipped ? 1 : 0) | (keyed ? 2 : 0) | (half ? 4 : 0);
		cmd->extra = key;

		++art.draws;
		if (cached)
			++art.cachedDraws;
		art.px += (to->right - to->left) * (to->bottom - to->top);
		game.lastArtSource = srcId;
		if (!art.firstTick)
			art.firstTick = GetTickCount() | 1;
		if (!art.checked)
		{
			art.checked = TRUE;
			game.checkRequest = TRUE;
		}

		if (art.artLogs < ART_LOGS && art.frameLogs < LOGS_FRAME)
		{
			++art.artLogs;
			++art.frameLogs;
			DebugLog("twin art%s frame=%d src=%p sheet=%d,%d dst=%p %d,%d %dx%d canvas=%d,%d of %dx%d%s%s at frame %u", cached ? " cached" : "", frame, game.twins[srcId].owner, from->left, from->top,
				target->owner, to->left, to->top, to->right - to->left, to->bottom - to->top, to->left - place->left, to->top - place->top,
				place->right - place->left, place->bottom - place->top, flipped ? " flipped" : "", keyed ? " keyed" : "", game.frameNo);
		}
		return TRUE;
	}

	VOID Remember(Record* source, const RECT* from, LONG frame, BOOL flipped, const RECT* place, const RECT* to)
	{
		if (!source->links)
		{
			source->links = (ArtLink*)MemoryAlloc(LINK_MAX * sizeof(ArtLink));
			source->linkCount = 0;
			if (!source->links)
				return;
		}

		DropLinks(source, from);

		ArtLink* link;
		if (source->linkCount < LINK_MAX)
			link = &source->links[source->linkCount++];
		else
		{
			link = &source->links[0];
			for (DWORD i = 1; i < LINK_MAX; ++i)
				if (source->links[i].lastUse < link->lastUse)
					link = &source->links[i];
		}

		link->rect = *from;
		link->frame = frame;
		link->flipped = flipped;
		link->placeW = place->right - place->left;
		link->placeH = place->bottom - place->top;
		link->cx = to->left - place->left;
		link->cy = to->top - place->top;
		link->lastUse = ++art.useClock;
	}

	BOOL ArtCopy(Record* target, DWORD dstId, const RECT* to, Record* source, DWORD srcId, const RECT* from, Expect* e, LONG x, LONG y, BOOL keyed, DWORD key, BOOL half)
	{
		e->used = TRUE;
		while (art.head < art.count && art.expect[art.head].used)
			++art.head;
		++art.matched;

		const Image* image = &art.images[e->image];
		LONG placeW = image->place.right - image->place.left;
		LONG placeH = image->place.bottom - image->place.top;
		LONG cx = e->dx - image->place.left, cy = e->dy - image->place.top;
		if (placeW < 2 || placeH < 2 || cx < -1 || cy < -1 || cx + e->w > placeW + 1 || cy + e->h > placeH + 1)
		{
			++art.rejected;
			if (art.rejectLogs < REJECT_LOGS && art.frameLogs < LOGS_FRAME)
			{
				++art.rejectLogs;
				++art.frameLogs;
				DebugLog("twin rejected place frame=%d sheet=%d,%d %dx%d dst=%d,%d canvas=%d,%d of %dx%d at frame %u", image->frame,
					e->sx, e->sy, e->w, e->h, x, y, cx, cy, placeW, placeH, game.frameNo);
			}
			return FALSE;
		}

		if (!RecordArt(target, dstId, to, srcId, from, image->frame, image->flipped, &image->place, keyed, key, FALSE, half))
			return FALSE;

		Remember(source, from, image->frame, image->flipped, &image->place, to);
		return TRUE;
	}

	BOOL CachedArt(Record* target, DWORD dstId, const RECT* to, Record* source, DWORD srcId, const RECT* from, BOOL keyed, DWORD key, BOOL half)
	{
		if (!source->linkCount)
			return FALSE;

		ArtLink* link = NULL;
		for (DWORD i = 0; i < source->linkCount && !link; ++i)
			if (Contains(&source->links[i].rect, from))
				link = &source->links[i];
		if (!link)
		{
			++art.cacheMisses;
			return FALSE;
		}

		++art.cacheHits;
		link->lastUse = ++art.useClock;

		LONG cx = link->cx + from->left - link->rect.left;
		LONG cy = link->cy + from->top - link->rect.top;
		RECT place = { to->left - cx, to->top - cy, to->left - cx + link->placeW, to->top - cy + link->placeH };
		return RecordArt(target, dstId, to, srcId, from, link->frame, link->flipped, &place, keyed, key, TRUE, half);
	}

	VOID ExpireArt()
	{
		for (DWORD i = 0; i < art.count; ++i)
		{
			const Expect* e = &art.expect[i];
			if (e->used)
				continue;

			++art.unmatched;
			if (art.unmatchedLogs < UNMATCHED_LOGS && art.frameLogs < LOGS_FRAME)
			{
				++art.unmatchedLogs;
				++art.frameLogs;
				DebugLog("twin unmatched frame=%d dst=%d,%d sheet=%d,%d %dx%d at frame %u", art.images[e->image].frame, e->dx, e->dy, e->sx, e->sy, e->w, e->h, game.frameNo);
			}
		}

		if (art.count)
			MemoryZero(art.bucket, sizeof(art.bucket));
		art.count = art.imageCount = art.head = 0;
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
		twin->repairs = 0;
		twin->repairPx = 0;
		twin->keyed = FALSE;
		twin->key = 0;
		twin->artCount = 0;
		twin->links = NULL;
		twin->linkCount = 0;
		twin->mirror = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
		if (twin->mirror)
			MemoryCopy(twin->mirror, twin->base, width * height * sizeof(DWORD));
		else if (game.fails < LOG_FAILS)
		{
			++game.fails;
			DebugLog("twin mirror %dx%d buffer %08X: out of memory, tile check off for it", width, height, buffer);
		}
		if (id >= game.top)
			game.top = id + 1;

		++game.alive;
		game.bytes += (LONGLONG)width * height * sizeof(DWORD) * game.scale * game.scale;
		EmitCreate(twin, id);

		BOOL screenSized = width == *(LONG*)&config.mode->width && height == *(LONG*)&config.mode->height;
		if (game.logged < LOG_TWINS || (screenSized && game.screenLogs < SCREEN_LOGS))
		{
			if (game.logged < LOG_TWINS)
				++game.logged;
			else
				++game.screenLogs;
			DebugLog("twin create %u %dx%d buffer %08X data %08X at frame %u", id, width, height, buffer, buffer->data, game.frameNo);
		}
	}

	VOID GroundArt(const VOID* data, LONG pitch, const RECT* rect, const VOID* ops, DWORD bytes)
	{
		if (!Active() || !data || !rect || !ops || !bytes)
			return;

		Sync();

		DWORD id;
		Record* twin = FindAddress(data, &id);
		if (!twin || pitch != twin->pitch)
		{
			++art.groundNoTwin;
			if (art.groundLogs < GROUND_LOGS)
			{
				++art.groundLogs;
				DebugLog("twin ground no twin data=%p pitch %d rect %d,%d %dx%d%s", data, pitch, rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
					twin ? " (pitch differs from its twin)" : "");
			}
			return;
		}

		LONG start = LONG(((const BYTE*)data - twin->base) / sizeof(DWORD));
		RECT area = *rect;
		OffsetRect(&area, start % twin->pitch, start / twin->pitch);
		if (!Clip(twin, &area))
			return;

		Cover(twin, &area);
		FlushRegion(twin, id, &area, TRUE);

		LONG width = area.right - area.left;
		LONG height = area.bottom - area.top;
		const DWORD* real = (const DWORD*)twin->base + area.top * twin->pitch + area.left;
		if (twin->mirror)
		{
			const DWORD* src = real;
			DWORD* mirror = twin->mirror + area.top * twin->width + area.left;
			for (LONG y = height; y; --y)
			{
				MemoryCopy(mirror, src, width * sizeof(DWORD));
				src += twin->pitch;
				mirror += twin->width;
			}
		}

		DWORD opsOffset;
		BYTE* opsCopy = Reserve(bytes, &opsOffset);
		if (opsCopy)
			MemoryCopy(opsCopy, ops, bytes);

		DWORD offset;
		DWORD* pixels = opsCopy ? (DWORD*)Reserve(width * height * sizeof(DWORD), &offset) : NULL;
		Cmd* cmd = pixels ? Emit(CmdGround) : NULL;
		if (!cmd)
		{
			MarkDirty(twin, area);
			return;
		}
		Convert(twin, pixels, real, width, height);

		cmd->dst = id;
		cmd->src = offset;
		cmd->rect = area;
		cmd->value = opsOffset;
		cmd->extra = bytes;

		DropLinks(twin, &area);
		CoverArt(twin, &area);
		AddArt(twin, &area);
		++art.groundArts;
	}

	VOID OnRelease(StateBuffer* buffer)
	{
		DrainSync();

		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
			Release(twin, id);
	}

	VOID CopyTwins(Record* target, DWORD dstId, LONG x, LONG y, Record* source, DWORD srcId, const RECT* srcRect, BOOL keyed, DWORD key);

	VOID OnCopy(StateBuffer* dst, LONG x, LONG y, StateBuffer* src, const RECT* srcRect, BOOL keyed, DWORD key)
	{
		if (!Active() || !srcRect)
			return;

		Sync();

		DWORD dstId;
		Record* target = Find(dst, &dstId);
		if (!target)
			return;

		DWORD srcId;
		Record* source = Find(src, &srcId);
		CopyTwins(target, dstId, x, y, source, srcId, srcRect, keyed, key);
	}

	VOID CopyTwins(Record* target, DWORD dstId, LONG x, LONG y, Record* source, DWORD srcId, const RECT* srcRect, BOOL keyed, DWORD key)
	{
		RECT whole = { x, y, x + srcRect->right - srcRect->left, y + srcRect->bottom - srcRect->top };

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

		if (keyed)
		{
			source->keyed = TRUE;
			source->key = key | ALPHA_COMPONENT;
		}

		FlushRegion(source, srcId, &from);
		Verify(source, srcId, &from, NULL, 0, FALSE);

		Expect* e = art.count ? Match(x, y, srcRect->right - srcRect->left, srcRect->bottom - srcRect->top, srcRect->left, srcRect->top) : NULL;
		if (e || source->linkCount)
		{
			FlushRegion(target, dstId, &to, TRUE);
			if (e ? ArtCopy(target, dstId, &to, source, srcId, &from, e, x, y, keyed, key, FALSE) : CachedArt(target, dstId, &to, source, srcId, &from, keyed, key, FALSE))
			{
				MirrorCopy(target, &to, source, &from, keyed, key);
				if (target != source)
					DropLinks(target, &to);
				AddArt(target, &to);
				return;
			}
		}

		if (!keyed)
			Cover(target, &to);
		FlushRegion(target, dstId, &to, TRUE);

		RECT arts[ART_MAX];
		DWORD artCount = 0;
		for (DWORD i = 0; i < source->artCount; ++i)
			if (IntersectRect(&arts[artCount], &source->art[i], &from))
				OffsetRect(&arts[artCount++], ox, oy);
		if (!keyed)
			CoverArt(target, &to);
		DropLinks(target, &to);
		for (DWORD i = 0; i < artCount; ++i)
			AddArt(target, &arts[i]);

		Cmd* cmd = Emit(CmdCopy);
		if (!cmd)
		{
			MarkDirty(target, to);
			return;
		}
		MirrorCopy(target, &to, source, &from, keyed, key);
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
		FlushRegion(twin, id, &area, TRUE);
		CoverArt(twin, &area);
		DropLinks(twin, &area);

		Cmd* cmd = Emit(CmdFill);
		if (!cmd)
		{
			MarkDirty(twin, area);
			return;
		}
		MirrorFill(twin, &area, color);
		cmd->dst = id;
		cmd->rect = area;
		cmd->value = color;
		cmd->flags = twin->keyed && color == twin->key ? 1 : 0;

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

		if (!Clip(twin, &rect))
			return;

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

	BOOL PixelPos(const Record* twin, const VOID* ptr, LONG pitch, LONG width, LONG height, LONG* x, LONG* y)
	{
		LONG start = LONG(((const BYTE*)ptr - twin->base) / sizeof(DWORD));
		*x = start % twin->pitch;
		*y = start / twin->pitch;
		return pitch == twin->pitch && *x + width <= twin->width && *y + height <= twin->height;
	}

	VOID OnPixelCopy(PixelMode mode, const DWORD* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height, DWORD key, DWORD param)
	{
		if (!dst || !src || width <= 0 || height <= 0 || (DWORD)mode >= PIXEL_MODES || !Active())
			return;

		Sync();

		DWORD dstId;
		Record* target = FindAddress(dst, &dstId);
		if (!target)
			return;

		++game.pixelOps[mode];

		LONG dx, dy;
		if (!PixelPos(target, dst, dstPitch, width, height, &dx, &dy))
		{
			OnWrite(dst, dstPitch, width, height);
			return;
		}
		RECT to = { dx, dy, dx + width, dy + height };

		DWORD srcId;
		Record* source = FindAddress(src, &srcId);
		LONG sx, sy;
		if (!source || !PixelPos(source, src, srcPitch, width, height, &sx, &sy))
		{
			++game.pixelUntwinned;
			MarkDirty(target, to);
			return;
		}
		RECT from = { sx, sy, sx + width, sy + height };

		if (mode == PixelAverage && (param & 0xFF) >= 255)
		{
			CopyTwins(target, dstId, dx, dy, source, srcId, &from, TRUE, key);
			return;
		}

		if (source == target)
		{
			++game.pixelSelf;
			MarkDirty(target, to);
			return;
		}

		FlushRegion(source, srcId, &from);
		Verify(source, srcId, &from, NULL, 0, FALSE);
		FlushRegion(target, dstId, &to, TRUE);
		DropLinks(target, &to);

		if (mode == PixelKeyed && (param & 0xFF) >= 255 && (art.count || source->linkCount))
		{
			source->keyed = TRUE;
			source->key = key | ALPHA_COMPONENT;
			Expect* e = art.count ? Match(dx, dy, width, height, sx, sy) : NULL;
			if (e ? ArtCopy(target, dstId, &to, source, srcId, &from, e, dx, dy, TRUE, key, TRUE) : CachedArt(target, dstId, &to, source, srcId, &from, TRUE, key, TRUE))
			{
				++game.pixelArt;
				AddArt(target, &to);
				DeferSync(target, dstId, &to);
				return;
			}
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

		if (mode != PixelAdd && mode != PixelSub)
		{
			RECT part;
			for (DWORD i = 0; i < source->artCount; ++i)
				if (IntersectRect(&part, &source->art[i], &from))
				{
					OffsetRect(&part, dx - sx, dy - sy);
					AddArt(target, &part);
				}

			// the smooth half-coverage and rounded blends differ from the 1x dither and floor on purpose, so the GPU check treats them like art
			AddArt(target, &to);
		}

		DeferSync(target, dstId, &to);
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

	VOID LogTiles()
	{
		LONGLONG frequency = game.frequency.QuadPart ? game.frequency.QuadPart : 1;
		DWORD avgUs = game.frames ? DWORD(game.verifySum * 1000000 / frequency / game.frames) : 0;
		DWORD maxUs = DWORD(game.verifyMax * 1000000 / frequency);

		Record* top[REPAIR_TOP] = {};
		for (DWORD i = 0; i < game.top; ++i)
		{
			Record* twin = &game.twins[i];
			if (!twin->owner || !twin->repairs)
				continue;

			DWORD slot = REPAIR_TOP;
			while (slot && top[slot - 1]->repairs < twin->repairs)
				--slot;
			if (slot < REPAIR_TOP)
			{
				for (DWORD k = REPAIR_TOP - 1; k > slot; --k)
					top[k] = top[k - 1];
				top[slot] = twin;
			}
		}

		CHAR text[REPAIR_TOP * 64 + 1];
		CHAR* ptr = text;
		*ptr = 0;
		for (DWORD k = 0; k < REPAIR_TOP && top[k]; ++k)
			ptr += StrPrint(ptr, " buf=%08X %dx%d tiles %u px %u", (DWORD)top[k]->owner, top[k]->width, top[k]->height, top[k]->repairs, top[k]->repairPx);

		DebugLog("twin tiles check %s verify avg %u.%03u max %u.%03u ms/frame | repaired tiles %u rects %u px %u | top%s",
			game.tileCheck ? "on" : "off", avgUs / 1000, avgUs % 1000, maxUs / 1000, maxUs % 1000,
			game.repairTiles, game.repairRects, game.repairPx, top[0] ? text : " none");

		for (DWORD i = 0; i < game.top; ++i)
		{
			game.twins[i].repairs = 0;
			game.twins[i].repairPx = 0;
		}
		game.repairTiles = game.repairRects = game.repairPx = 0;
		game.verifySum = game.verifyMax = 0;
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

		LogTiles();

		Lock();
		DWORD artReplayed = shared.artDraws;
		DWORD artFallbacks = shared.artFallbacks;
		DWORD artRejected = shared.artRejected;
		DWORD artCalls = shared.artCalls;
		DWORD groundReplayed = shared.groundReplayed;
		DWORD groundFallbacks = shared.groundFallbacks;
		DWORD pixelReplayed = shared.pixelReplayed;
		DWORD pixelCalls = shared.pixelCalls;
		shared.pixelReplayed = shared.pixelCalls = 0;
		shared.artDraws = shared.artFallbacks = shared.artRejected = shared.artCalls = shared.groundReplayed = shared.groundFallbacks = 0;
		CheckResult check = shared.check;
		DWORD showOpaque = shared.showOpaque, showBack = shared.showBack, showInvalid = shared.showInvalid, showGone = shared.showGone, showOffCount = shared.showOff, showOther = shared.showOther;
		DWORD showUs = shared.showSamples ? shared.showUs / shared.showSamples : 0, showMaxUs = shared.showMaxUs, showSamples = shared.showSamples;
		shared.showOpaque = shared.showBack = shared.showInvalid = shared.showGone = shared.showOff = shared.showOther = 0;
		shared.showUs = shared.showSamples = shared.showMaxUs = 0;
		Unlock();

		DebugLog("twin art expected %u dup %u matched %u draws %u px %u notready %u rejected place %u aspect %u unmatched %u overflow %u | replayed %u calls %u fallbacks %u",
			art.expected, art.duplicates, art.matched, art.draws, art.px, art.notReady, art.rejected, artRejected, art.unmatched, art.overflow, artReplayed, artCalls, artFallbacks);
		art.expected = art.duplicates = art.matched = art.draws = art.px = art.notReady = art.rejected = art.unmatched = art.overflow = 0;

		DebugLog("twin pixel ops keyed %u average %u add %u sub %u empty %u | art %u untwinned src %u self %u | replayed %u calls %u",
			game.pixelOps[PixelKeyed], game.pixelOps[PixelAverage], game.pixelOps[PixelAdd], game.pixelOps[PixelSub], game.pixelOps[PixelEmpty],
			game.pixelArt, game.pixelUntwinned, game.pixelSelf, pixelReplayed, pixelCalls);
		MemoryZero(game.pixelOps, sizeof(game.pixelOps));
		game.pixelArt = game.pixelUntwinned = game.pixelSelf = 0;

		DebugLog("twin art cache hits %u misses %u cached draws %u | ground arts %u replayed %u fallbacks %u no twin %u",
			art.cacheHits, art.cacheMisses, art.cachedDraws, art.groundArts, groundReplayed, groundFallbacks, art.groundNoTwin);
		art.cacheHits = art.cacheMisses = art.cachedDraws = art.groundArts = art.groundNoTwin = 0;

		DebugLog("twin show shown %u (opaque %u, over background %u) hidden: 1x toggle %u, invalid %u (no twin at present %u, twin gone %u), other %u | show gpu avg %u us max %u us (%u samples)",
			showOpaque + showBack, showOpaque, showBack, showOffCount, showInvalid + showGone, showInvalid, showGone, showOther, showUs, showMaxUs, showSamples);
		game.presentNoTwin = 0;

		if (check.valid)
			DebugLog("twin gpu check %ums ago twin %u bad %u of %u non-art tiles, px %u | art tiles %u (differ from 1x %u)", now - check.tick, check.twin, check.bad, check.tiles, check.px, check.art, check.artDiffer);
		else
			DebugLog("twin gpu check none yet");

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
			StateBufferAligned* first = (StateBufferAligned*)screen;
			DebugLog("twin first present buffer %08X isBack %u isZoomed %u borders %u size %ux%u", screen, first->isBack, first->isZoomed, first->borders, first->size.width, first->size.height);
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

		down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_F11) & 0x8000);
		if (down && !game.tileKeyDown)
		{
			game.tileCheck = !game.tileCheck;
			DebugLog("twin tile check %s", game.tileCheck ? "on" : "off");
			if (game.tileCheck)
				for (DWORD i = 0; i < game.top; ++i)
					if (game.twins[i].owner)
						MarkAll(&game.twins[i]);
		}
		game.tileKeyDown = down;

		down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_F10) & 0x8000);
		if (down && !game.tintKeyDown)
		{
			shared.tint = !shared.tint;
			DebugLog("twin art tint %s", shared.tint ? "magenta" : "off");
		}
		game.tintKeyDown = down;

		BOOL shot = game.shotRequest;
		if (art.firstTick && !art.shotTaken && now - art.firstTick >= SHOT_DELAY)
		{
			art.shotTaken = TRUE;
			shot = TRUE;
		}
		if (shot)
		{
			game.shotRequest = FALSE;
			game.checkRequest = TRUE;
		}

		DWORD id = TWIN_NONE;
		Record* twin = Find(screen, &id);
		if (!twin)
		{
			++game.presentNoTwin;
			if (!game.presentLogTick || now - game.presentLogTick >= 1000)
			{
				game.presentLogTick = now | 1;
				DebugLog("twin present without twin buffer=%p data=%p at frame %u (%u so far this interval)", screen, screen ? screen->data : NULL, game.frameNo, game.presentNoTwin);
			}
		}
		if (twin)
		{
			RECT whole = { 0, 0, twin->width, twin->height };
			RECT flushed[DIRTY_MAX];
			DWORD flushedCount = twin->count;
			MemoryCopy(flushed, twin->dirty, flushedCount * sizeof(RECT));
			FlushRegion(twin, id, &whole);
			Verify(twin, id, &whole, flushed, flushedCount, TRUE);
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

		DWORD artOffset = 0;
		DWORD artCount = 0;
		if (check && twin->artCount)
		{
			RECT* arts = (RECT*)Reserve(twin->artCount * sizeof(RECT), &artOffset);
			if (arts)
			{
				MemoryCopy(arts, twin->art, twin->artCount * sizeof(RECT));
				artCount = twin->artCount;
			}
		}

		Cmd* cmd = Emit(CmdPresent);
		if (cmd)
		{
			cmd->dst = id;
			Config::GetMapZoomRect(&cmd->rect);
			StateBufferAligned* aligned = (StateBufferAligned*)screen;
			cmd->flags = (Config::IsMapZoomed() ? PresentMapZoom : 0) | (aligned->isBack ? PresentBack : 0) | (aligned->isZoomed ? PresentZoomed : 0) | (check ? PresentCheck : 0);
			cmd->value = checkOffset;
			cmd->src = artOffset;
			cmd->extra = artCount;
			if (shot)
			{
				cmd->flags |= PresentShot;
				cmd->clip.left = game.lastArtSource;
			}
			if (twin)
			{
				cmd->x = twin->width;
				cmd->y = twin->height;
			}
		}

		++game.frameNo;
		game.frameRepairLogs = 0;
		ExpireArt();
		art.frameLogs = 0;
		game.verifySum += game.verifyFrame;
		if (game.verifyFrame > game.verifyMax)
			game.verifyMax = game.verifyFrame;
		game.verifyFrame = 0;

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
		game.shotRequest = TRUE;
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
			"uniform int art;\n"
			"uniform int tint;\n"
			"uniform int empty;\n"
			"uniform vec2 weight;\n"
			"uniform vec4 key;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	if (art != 0 && (fTex.x < 0.0 || fTex.y < 0.0 || fTex.x > 1.0 || fTex.y > 1.0))\n"
			"		discard;\n"
			"	vec4 c = texture(tex01, fTex);\n"
			"	if (art != 0 && tint != 0)\n"
			"		c.rgb *= vec3(1.0, 0.15, 1.0);\n"
			"	if (keyed != 0)\n"
			"	{\n"
			"		if (c.a < 0.5 / 255.0 || (c.a > 254.5 / 255.0 && all(lessThan(abs(c.rgb - key.rgb), vec3(0.5 / 255.0)))))\n"
			"			discard;\n"
			"		if (c.a > 253.5 / 255.0)\n"
			"			c.a = 1.0;\n"
			"	}\n"
			"	if (empty != 0)\n"
			"	{\n"
			"		if (c.a < 254.5 / 255.0 && all(lessThan(c.rgb, vec3(0.5 / 255.0))))\n"
			"			discard;\n"
			"		if (c.a > 253.5 / 255.0)\n"
			"			c.a = 1.0;\n"
			"	}\n"
			"	FRAG_COLOR = c * weight.x;\n"
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
		gl.blendEquation = (GLBLENDEQUATION)wglGetProcAddress("glBlendEquation");

		gl.copyProgram = Link(vertex, copyFragment);
		if (gl.copyProgram)
		{
			GLUseProgram(gl.copyProgram);
			GLUniform1i(GLGetUniformLocation(gl.copyProgram, "tex01"), 0);
			gl.keyedLoc = GLGetUniformLocation(gl.copyProgram, "keyed");
			gl.keyLoc = GLGetUniformLocation(gl.copyProgram, "key");
			gl.artLoc = GLGetUniformLocation(gl.copyProgram, "art");
			gl.tintLoc = GLGetUniformLocation(gl.copyProgram, "tint");
			gl.emptyLoc = GLGetUniformLocation(gl.copyProgram, "empty");
			gl.weightLoc = GLGetUniformLocation(gl.copyProgram, "weight");
			GLUniform1i(gl.tintLoc, 0);
			GLUniform1i(gl.emptyLoc, 0);
			GLUniform2f(gl.weightLoc, 1.0f, 0.0f);
			GLUniform1i(gl.keyedLoc, 0);
			GLUniform1i(gl.artLoc, 0);
		}

		// box filter: each output pixel averages the twin texels its footprint covers, weighted by overlap, so minifying 4K detail doesn't shimmer while scrolling; a footprint of 1 texel is plain bilinear
		const CHAR* showFragment =
			"uniform sampler2D tex01;\n"
			"uniform vec2 ratio;\n"
			"uniform int opaque;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	ivec2 size = textureSize(tex01, 0);\n"
			"	vec2 r = max(ratio, vec2(1.0));\n"
			"	vec2 centre = fTex * vec2(size) - vec2(0.5);\n"
			"	vec2 lo = centre - r * 0.5 + vec2(0.5);\n"
			"	vec2 hi = centre + r * 0.5 + vec2(0.5);\n"
			"	ivec2 i0 = ivec2(floor(lo));\n"
			"	ivec2 i1 = min(ivec2(ceil(hi)) - ivec2(1), i0 + ivec2(7));\n"
			"	vec4 sum = vec4(0.0);\n"
			"	float total = 0.0;\n"
			"	for (int y = i0.y; y <= i1.y; ++y)\n"
			"	{\n"
			"		float wy = min(hi.y, float(y + 1)) - max(lo.y, float(y));\n"
			"		int cy = clamp(y, 0, size.y - 1);\n"
			"		for (int x = i0.x; x <= i1.x; ++x)\n"
			"		{\n"
			"			float w = (min(hi.x, float(x + 1)) - max(lo.x, float(x))) * wy;\n"
			"			if (w > 0.0)\n"
			"			{\n"
			"				sum += texelFetch(tex01, ivec2(clamp(x, 0, size.x - 1), cy), 0) * w;\n"
			"				total += w;\n"
			"			}\n"
			"		}\n"
			"	}\n"
			"	vec4 c = sum / max(total, 1e-6);\n"
			"	if (opaque != 0 && c.a < 254.5 / 255.0)\n"
			"		discard;\n"
			"	FRAG_COLOR = c;\n"
			"}\n";

		gl.showProgram = Link(vertex, showFragment);
		if (gl.showProgram)
		{
			GLUseProgram(gl.showProgram);
			GLUniform1i(GLGetUniformLocation(gl.showProgram, "tex01"), 0);
			gl.ratioLoc = GLGetUniformLocation(gl.showProgram, "ratio");
			gl.opaqueLoc = GLGetUniformLocation(gl.showProgram, "opaque");
			GLUniform1i(gl.opaqueLoc, 0);
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

		gl.ready = gl.copyProgram && gl.checkProgram && gl.showProgram && gl.vao && gl.vbo && gl.stage;
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
		if (cmd->flags)
			GLClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		else
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

	VOID Blend(BOOL enable)
	{
		if (enable)
		{
			GLEnable(GL_BLEND);
			GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
			GLDisable(GL_BLEND);
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
		if (cmd->flags)
			Blend(TRUE);
		DrawVerts();
		if (cmd->flags)
			Blend(FALSE);
	}

	DWORD ReplayCopies(const Cmd* cmds, DWORD count, DWORD index)
	{
		const Cmd* first = &cmds[index];
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
		for (; i < count; ++i)
		{
			const Cmd* cmd = &cmds[i];
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
			if (first->flags)
				Blend(TRUE);
			DrawVerts();
			if (first->flags)
				Blend(FALSE);
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

	VOID StageUpload(DWORD dst, const RECT* rect, const DWORD* pixels, LONG* shelf)
	{
		LONG width = rect->right - rect->left;
		LONG height = rect->bottom - rect->top;
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, width);

		for (LONG ty = 0; ty < height; ty += STAGE_SIZE)
		{
			LONG th = min(height - ty, STAGE_SIZE);
			for (LONG tx = 0; tx < width; tx += STAGE_SIZE)
			{
				LONG tw = min(width - tx, STAGE_SIZE);

				if (shelf[0] + tw > STAGE_SIZE)
				{
					shelf[1] += shelf[2];
					shelf[0] = 0;
					shelf[2] = 0;
				}

				if (shelf[1] + th > STAGE_SIZE)
				{
					FlushPlaced();
					shelf[0] = shelf[1] = shelf[2] = 0;
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

				GLTexSubImage2D(GL_TEXTURE_2D, 0, shelf[0], shelf[1], tw, th, GL_RGBA, GL_UNSIGNED_BYTE, pixels + ty * width + tx);

				Placed* p = &gl.placed[gl.placedCount++];
				p->dst = dst;
				p->sx = shelf[0];
				p->sy = shelf[1];
				SetRect(&p->rect, rect->left + tx, rect->top + ty, rect->left + tx + tw, rect->top + ty + th);

				shelf[0] += tw;
				if (th > shelf[2])
					shelf[2] = th;
			}
		}
	}

	DWORD ReplayUploads(const List* list, DWORD index)
	{
		GLBindTexture(GL_TEXTURE_2D, gl.stage);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		LONG shelf[3] = { 0, 0, 0 };
		DWORD i = index;
		for (; i < list->count && list->cmds[i].type == CmdUpload; ++i)
		{
			const Cmd* cmd = &list->cmds[i];
			if (Valid(cmd->dst))
				StageUpload(cmd->dst, &cmd->rect, (const DWORD*)(list->arena + cmd->value), shelf);
		}

		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		FlushPlaced();
		return i;
	}

	BOOL ArtAspect(const Cmd* cmd, LONG width, LONG height)
	{
		FLOAT placeW = (FLOAT)(cmd->place.right - cmd->place.left), placeH = (FLOAT)(cmd->place.bottom - cmd->place.top);
		FLOAT aspect = width > 0 && height > 0 && placeH > 0.0f ? (placeW * height) / (placeH * width) : 0.0f;
		if (aspect >= 0.98f && aspect <= 1.02f)
			return TRUE;

		++gl.artRejected;
		if (gl.rejectLogs < REJECT_LOGS)
		{
			++gl.rejectLogs;
			DebugLog("twin rejected aspect frame=%d place %dx%d texture %dx%d", (LONG)cmd->value, cmd->place.right - cmd->place.left, cmd->place.bottom - cmd->place.top, width, height);
		}
		return FALSE;
	}

	VOID ArtFallback(const Cmd* cmd)
	{
		++gl.artFallbacks;

		Cmd copy;
		MemoryZero(&copy, sizeof(Cmd));
		copy.type = CmdCopy;
		copy.dst = cmd->dst;
		copy.src = cmd->src;
		copy.rect = cmd->clip;
		copy.x = cmd->rect.left;
		copy.y = cmd->rect.top;
		copy.value = cmd->extra;
		copy.flags = (cmd->flags & 2) ? 1 : 0;
		if (Valid(copy.src) && Valid(copy.dst))
			ReplayCopies(&copy, 1, 0);
	}

	DWORD ReplayArt(const List* list, DWORD index)
	{
		const Cmd* first = &list->cmds[index];
		DWORD end = index + 1;
		while (end < list->count && list->cmds[end].type == CmdArt && list->cmds[end].dst == first->dst && list->cmds[end].value == first->value && (list->cmds[end].flags & 4) == (first->flags & 4))
			++end;

		GLuint texture = 0;
		LONG width = 0, height = 0;
		BOOL ready = Valid(first->dst) && ImageReplace::ArtTexture((LONG)first->value, &texture, &width, &height) && texture && ArtAspect(first, width, height);
		if (!ready)
		{
			for (DWORD i = index; i < end; ++i)
				ArtFallback(&list->cmds[i]);
			return end;
		}

		Gpu* gpu = &gl.twins[first->dst];
		LONG scale = gpu->scale;
		FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);

		gl.vertCount = 0;
		DWORD drawn = 0;
		for (DWORD i = index; i < end; ++i)
		{
			const Cmd* cmd = &list->cmds[i];

			FLOAT placeW = (FLOAT)(cmd->place.right - cmd->place.left), placeH = (FLOAT)(cmd->place.bottom - cmd->place.top);
			FLOAT u0 = (cmd->rect.left - cmd->place.left) / placeW, u1 = (cmd->rect.right - cmd->place.left) / placeW;
			FLOAT v0 = (cmd->rect.top - cmd->place.top) / placeH, v1 = (cmd->rect.bottom - cmd->place.top) / placeH;
			if (cmd->flags & 1)
			{
				u0 = 1.0f - u0;
				u1 = 1.0f - u1;
			}

			AddQuad(tw, th, (FLOAT)(cmd->rect.left * scale), (FLOAT)(cmd->rect.top * scale), (FLOAT)(cmd->rect.right * scale), (FLOAT)(cmd->rect.bottom * scale), u0, v0, u1, v1);
			++drawn;
		}

		if (drawn)
		{
			TargetTwin(first->dst);
			UseCopy(FALSE, 0);
			GLUniform1i(gl.artLoc, 1);
			GLUniform1i(gl.tintLoc, shared.tint ? 1 : 0);
			if (first->flags & 4)
				GLUniform2f(gl.weightLoc, 0.5f, 0.0f);
			GLBindTexture(GL_TEXTURE_2D, texture);
			Blend(TRUE);
			DrawVerts();
			Blend(FALSE);
			GLUniform1i(gl.artLoc, 0);
			GLUniform1i(gl.tintLoc, 0);
			if (first->flags & 4)
				GLUniform2f(gl.weightLoc, 1.0f, 0.0f);
			gl.artDraws += drawn;
			++gl.artCalls;
		}
		gl.vertCount = 0;

		return end;
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
			if (gl.checkMask)
				MemoryFree(gl.checkMask);
			gl.checkData = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
			gl.checkMask = (BYTE*)MemoryAlloc(width * height);
			if (gl.check)
				GLDeleteTextures(1, &gl.check);
			if (gl.checkFbo)
				GLDeleteFramebuffers(1, &gl.checkFbo);
			gl.check = gl.checkFbo = 0;
			gl.checkW = gl.checkH = 0;
		}

		if (!gl.checkData || !gl.checkMask || !EnsureTarget(&gl.check, &gl.checkFbo, &gl.checkW, &gl.checkH, width, height))
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
		DWORD badTiles = 0, badPixels = 0, artTiles = 0, artBad = 0;
		const RECT* arts = (const RECT*)(list->arena + cmd->src);
		DWORD artCount = cmd->extra;

		// art regions often meet inside a tile, so classify per pixel against their union instead of per tile against single rects
		MemoryZero(gl.checkMask, width * height);
		for (DWORD k = 0; k < artCount; ++k)
		{
			RECT r = arts[k];
			r.left = max(r.left, 0L);
			r.top = max(r.top, 0L);
			r.right = min(r.right, width);
			r.bottom = min(r.bottom, height);
			for (LONG y = r.top; y < r.bottom; ++y)
				if (r.right > r.left)
					MemorySet(gl.checkMask + y * width + r.left, 1, r.right - r.left);
		}

		for (LONG ty = 0; ty < tilesY; ++ty)
		{
			for (LONG tx = 0; tx < tilesX; ++tx)
			{
				DWORD count = 0, artCountPx = 0, artDiffer = 0;
				LONG x0 = tx * TILE, y0 = ty * TILE;
				LONG x1 = min(x0 + TILE, width), y1 = min(y0 + TILE, height);
				for (LONG y = y0; y < y1; ++y)
				{
					const DWORD* a = ref + y * width;
					const DWORD* b = gl.checkData + y * width;
					const BYTE* m = gl.checkMask + y * width;
					for (LONG x = x0; x < x1; ++x)
					{
						BOOL differ = ((a[x] ^ b[x]) & 0x00FFFFFF) != 0;
						if (m[x])
						{
							++artCountPx;
							if (differ)
								++artDiffer;
						}
						else if (differ)
							++count;
					}
				}

				if (artCountPx == DWORD((x1 - x0) * (y1 - y0)))
				{
					++artTiles;
					if (artDiffer)
						++artBad;
					continue;
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

		DebugLog("twin check twin %u %dx%d N %d bad %u of %u non-art tiles, px %u | art tiles %u (differ from 1x %u) worst%s", cmd->dst, width, height, gpu->scale, badTiles, tilesX * tilesY - artTiles, badPixels, artTiles, artBad, badTiles ? text : " none");

		Lock();
		shared.check.valid = TRUE;
		shared.check.tick = GetTickCount();
		shared.check.twin = cmd->dst;
		shared.check.bad = badTiles;
		shared.check.tiles = tilesX * tilesY - artTiles;
		shared.check.px = badPixels;
		shared.check.art = artTiles;
		shared.check.artDiffer = artBad;
		Unlock();
	}

	DWORD ReplayPixels(const List* list, DWORD index)
	{
		const Cmd* first = &list->cmds[index];
		DWORD end = index + 1;
		while (end < list->count)
		{
			const Cmd* cmd = &list->cmds[end];
			if (cmd->type != CmdPixel || cmd->src != first->src || cmd->dst != first->dst || cmd->flags != first->flags || cmd->value != first->value || cmd->extra != first->extra)
				break;
			++end;
		}

		DWORD mode = first->flags;
		if (!Valid(first->src) || !Valid(first->dst) || (mode == PixelSub && !gl.blendEquation))
			return end;

		Gpu* src = &gl.twins[first->src];
		Gpu* dst = &gl.twins[first->dst];
		LONG scale = dst->scale;
		FLOAT tw = (FLOAT)(dst->width * scale), th = (FLOAT)(dst->height * scale);

		gl.vertCount = 0;
		for (DWORD i = index; i < end; ++i)
		{
			const Cmd* cmd = &list->cmds[i];
			LONG w = cmd->rect.right - cmd->rect.left;
			LONG h = cmd->rect.bottom - cmd->rect.top;
			AddQuad(tw, th, (FLOAT)(cmd->x * scale), (FLOAT)(cmd->y * scale), (FLOAT)((cmd->x + w) * scale), (FLOAT)((cmd->y + h) * scale),
				(FLOAT)cmd->rect.left / src->width, (FLOAT)cmd->rect.top / src->height, (FLOAT)cmd->rect.right / src->width, (FLOAT)cmd->rect.bottom / src->height);
		}

		DWORD param = first->extra & 0xFF;
		FLOAT weight = 1.0f;
		BOOL keyed = FALSE, empty = FALSE;
		switch (mode)
		{
		case PixelAverage:
			keyed = TRUE;
			weight = (param + 1) / 256.0f;
			break;
		case PixelKeyed:
			// the CPU dithers every other pixel; at N× the smooth equivalent is half coverage
			keyed = TRUE;
			weight = 0.5f * (param >= 255 ? 1.0f : (param + 1) / 256.0f);
			break;
		case PixelEmpty:
			empty = TRUE;
			weight = 0.5f;
			break;
		default:
			break;
		}

		TargetTwin(first->dst);
		UseCopy(keyed, first->value);
		if (empty)
			GLUniform1i(gl.emptyLoc, 1);
		if (weight != 1.0f)
			GLUniform2f(gl.weightLoc, weight, 0.0f);
		GLBindTexture(GL_TEXTURE_2D, src->texture);

		GLEnable(GL_BLEND);
		if (mode == PixelAdd || mode == PixelSub)
		{
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
			GLBlendFunc(GL_ONE, GL_ONE);
			if (mode == PixelSub)
				gl.blendEquation(GL_FUNC_REVERSE_SUBTRACT);
		}
		else
			GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		DrawVerts();
		++gl.pixelCalls;
		gl.pixelReplayed += end - index;

		if (mode == PixelSub)
			gl.blendEquation(GL_FUNC_ADD);
		if (mode == PixelAdd || mode == PixelSub)
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		GLDisable(GL_BLEND);
		if (empty)
			GLUniform1i(gl.emptyLoc, 0);
		if (weight != 1.0f)
			GLUniform2f(gl.weightLoc, 1.0f, 0.0f);
		gl.vertCount = 0;
		return end;
	}

	VOID ReplayGround(const Cmd* cmd, const List* list)
	{
		if (!Valid(cmd->dst))
			return;

		Gpu* gpu = &gl.twins[cmd->dst];
		LONG scale = gpu->scale;
		LONG width = cmd->rect.right - cmd->rect.left;
		LONG height = cmd->rect.bottom - cmd->rect.top;

		TargetTwin(cmd->dst);
		GLViewport(cmd->rect.left * scale, cmd->rect.top * scale, width * scale, height * scale);
		GLEnable(GL_SCISSOR_TEST);
		glScissor(cmd->rect.left * scale, cmd->rect.top * scale, width * scale, height * scale);

		BOOL drawn = ImageReplace::DrawGround(list->arena + cmd->value, cmd->extra, width, height);

		gl.target = TWIN_NONE;
		gl.program = 0;
		GLUseProgram(0);
		GLBindVertexArray(gl.vao);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLDisable(GL_BLEND);
		GLDisable(GL_SCISSOR_TEST);
		GLActiveTexture(GL_TEXTURE0);

		if (drawn)
		{
			++gl.groundReplayed;
			return;
		}

		++gl.groundFallbacks;
		GLBindTexture(GL_TEXTURE_2D, gl.stage);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		LONG shelf[3] = { 0, 0, 0 };
		StageUpload(cmd->dst, &cmd->rect, (const DWORD*)(list->arena + cmd->src), shelf);
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		FlushPlaced();
	}

	VOID WritePng(const CHAR* name, const BYTE* pixels, LONG width, LONG height, LONG stride)
	{
		if (!pnglib_create_write_struct || width <= 0 || height <= 0)
			return;

		Stream stream = { NULL, 0, 0 };
		png_structp png_ptr = pnglib_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		if (!png_ptr)
			return;

		png_infop info_ptr = pnglib_create_info_struct(png_ptr);
		if (info_ptr)
		{
			BYTE* rgb = (BYTE*)MemoryAlloc(width * height * 3);
			BYTE** rows = (BYTE**)MemoryAlloc(height * sizeof(BYTE*));
			if (rgb && rows)
			{
				for (LONG y = 0; y < height; ++y)
				{
					const BYTE* src = pixels + y * stride;
					BYTE* dst = rgb + y * width * 3;
					rows[y] = dst;
					for (LONG x = 0; x < width; ++x)
					{
						*dst++ = src[0];
						*dst++ = src[1];
						*dst++ = src[2];
						src += 4;
					}
				}

				pnglib_set_write_fn(png_ptr, &stream, PngLib::WriteDataIntoOutputStream, PngLib::FlushData);
				pnglib_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
				pnglib_write_info(png_ptr, info_ptr);
				pnglib_write_image(png_ptr, rows);
				pnglib_write_end(png_ptr, NULL);
			}
			if (rgb)
				MemoryFree(rgb);
			if (rows)
				MemoryFree(rows);
		}
		pnglib_destroy_write_struct(&png_ptr, &info_ptr);

		if (!stream.data)
			return;

		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, MAX_PATH);
		while (length && path[length - 1] != '\\')
			--length;
		path[length] = 0;
		StrCat(path, name);

		BOOL done = FALSE;
		HANDLE file = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			done = WriteFile(file, stream.data, stream.position, &written, NULL) && written == stream.position;
			CloseHandle(file);
		}
		MemoryFree(stream.data);

		if (done)
			DebugLog("twin shot written %s %dx%d", name, width, height);
		else
			DebugLog("twin shot failed %s", name);
	}

	VOID ShotTwin(const CHAR* name, DWORD id, LONG maxW, LONG maxH, BOOL centre)
	{
		if (!Valid(id))
		{
			DebugLog("twin shot skipped %s: no twin %u", name, id);
			return;
		}

		Gpu* gpu = &gl.twins[id];
		LONG fullW = gpu->width * gpu->scale, fullH = gpu->height * gpu->scale;
		LONG width = min(fullW, maxW), height = min(fullH, maxH);
		LONG x = centre ? (fullW - width) / 2 : 0;
		LONG y = centre ? (fullH - height) / 2 : 0;

		BYTE* pixels = (BYTE*)MemoryAlloc(width * height * 4);
		if (!pixels)
			return;

		GLBindFramebuffer(GL_READ_FRAMEBUFFER, gpu->fbo);
		GLPixelStorei(GL_PACK_ALIGNMENT, 4);
		GLReadPixels(x, y, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
		GLBindFramebuffer(GL_READ_FRAMEBUFFER, gl.readFbo);

		// twin textures keep 1x row 0 in texel row 0, so the read rows are already top-down
		WritePng(name, pixels, width, height, width * 4);
		MemoryFree(pixels);
	}

	VOID Shots(const Cmd* cmd, const List* list)
	{
		if (!pnglib_create_write_struct)
		{
			DebugLog("twin shot skipped: png library not loaded");
			return;
		}

		if (cmd->flags & PresentCheck)
			WritePng("twin-1x.png", list->arena + cmd->value, cmd->x, cmd->y, cmd->x * 4);
		ShotTwin("twin-screen.png", cmd->dst, SHOT_SCREEN_W, SHOT_SCREEN_H, TRUE);
		ShotTwin("twin-sheet.png", (DWORD)cmd->clip.left, SHOT_SHEET, SHOT_SHEET, FALSE);
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

			case CmdArt:
				i = ReplayArt(list, i);
				continue;

			case CmdGround:
				ReplayGround(cmd, list);
				break;

			case CmdPixel:
				i = ReplayPixels(list, i);
				continue;

			case CmdCopy:
				i = ReplayCopies(list->cmds, list->count, i);
				continue;

			case CmdPresent:
				gl.shown.valid = TRUE;
				gl.shown.id = cmd->dst;
				gl.shown.hole = cmd->rect;
				gl.shown.flags = cmd->flags;
				if (cmd->flags & PresentCheck)
					Check(cmd, list);
				if (cmd->flags & PresentShot)
					Shots(cmd, list);
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
		if (!gl.shown.valid || gl.shown.id >= TWIN_MAX)
		{
			++gl.showInvalid;
			return FALSE;
		}

		if (!Valid(gl.shown.id))
		{
			++gl.showGone;
			return FALSE;
		}

		if (rect->width <= 0 || rect->height <= 0 || !frameWidth || !frameHeight)
		{
			++gl.showOther;
			return FALSE;
		}

		BOOL back = (gl.shown.flags & PresentBack) != 0;
		if (back)
			++gl.showBack;
		else
			++gl.showOpaque;

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

		FLOAT ratioX = (FLOAT)(frameWidth * gpu->scale) / rect->width;
		FLOAT ratioY = (FLOAT)(frameHeight * gpu->scale) / rect->height;
		DWORD view = MAKELONG(rect->width, rect->height);
		DWORD twinSize = MAKELONG(gpu->width * gpu->scale, gpu->height * gpu->scale);
		DWORD frameSize = MAKELONG(frameWidth, frameHeight);
		if ((view != gl.showView || twinSize != gl.showTwin || frameSize != gl.showFrame) && gl.showLogs < SHOW_LOGS)
		{
			gl.showView = view;
			gl.showTwin = twinSize;
			gl.showFrame = frameSize;
			++gl.showLogs;
			DebugLog("twin show viewport %dx%d twin %dx%d frame %ux%u ratio %d.%03d x %d.%03d %s", rect->width, rect->height, gpu->width * gpu->scale, gpu->height * gpu->scale,
				frameWidth, frameHeight, (LONG)ratioX, (LONG)(ratioX * 1000) % 1000, (LONG)ratioY, (LONG)(ratioY * 1000) % 1000, ratioX > 1.0f || ratioY > 1.0f ? "box minify" : "bilinear");
		}

		gl.program = gl.showProgram;
		GLUseProgram(gl.showProgram);
		GLUniform2f(gl.ratioLoc, ratioX, ratioY);
		// with a background image the renderer already blended the 1x frame over it; keep those pixels and only cover what was opaque in 1x
		GLUniform1i(gl.opaqueLoc, back ? 1 : 0);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);

		BOOL timed = !(++gl.showCount & 255);
		LARGE_INTEGER start, end;
		if (timed)
		{
			GLFinish();
			QueryPerformanceCounter(&start);
		}
		DrawVerts();
		if (timed)
		{
			GLFinish();
			QueryPerformanceCounter(&end);
			DWORD us = gl.frequency.QuadPart ? DWORD((end.QuadPart - start.QuadPart) * 1000000 / gl.frequency.QuadPart) : 0;
			gl.showUs += us;
			++gl.showSamples;
			if (us > gl.showMaxUs)
				gl.showMaxUs = us;
		}
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
		GLint blendSrc, blendDst;
		GLGetIntegerv(GL_BLEND_SRC, &blendSrc);
		GLGetIntegerv(GL_BLEND_DST, &blendDst);
		GLboolean scissorTest = glIsEnabled(GL_SCISSOR_TEST);
		GLint texture2;
		GLActiveTexture(GL_TEXTURE2);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2);
		GLActiveTexture(GL_TEXTURE1);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture1);
		GLActiveTexture(GL_TEXTURE0);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);

		BOOL shown = FALSE;
		DWORD replayed = 0;
		DWORD showOff = 0;
		DWORD us = 0;
		if (InitGL())
		{
			gl.target = TWIN_NONE;
			gl.program = 0;
			gl.drawCalls = 0;
			gl.artDraws = 0;
			gl.artFallbacks = 0;
			gl.artRejected = 0;
			gl.artCalls = 0;
			gl.showOpaque = gl.showBack = gl.showInvalid = gl.showGone = gl.showOther = 0;
			gl.groundReplayed = gl.groundFallbacks = 0;
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
			else
				++showOff;
		}

		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
		GLBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
		GLViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
		GLClearColor(clear[0], clear[1], clear[2], clear[3]);
		GLBlendFunc(blendSrc, blendDst);
		if (blend)
			GLEnable(GL_BLEND);
		else
			GLDisable(GL_BLEND);
		if (scissorTest)
			GLEnable(GL_SCISSOR_TEST);
		else
			GLDisable(GL_SCISSOR_TEST);
		GLActiveTexture(GL_TEXTURE2);
		GLBindTexture(GL_TEXTURE_2D, texture2);
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

		shared.showOpaque += gl.showOpaque;
		shared.showBack += gl.showBack;
		shared.showInvalid += gl.showInvalid;
		shared.showGone += gl.showGone;
		shared.showUs += gl.showUs;
		shared.showSamples += gl.showSamples;
		if (gl.showMaxUs > shared.showMaxUs)
			shared.showMaxUs = gl.showMaxUs;
		gl.showUs = gl.showSamples = gl.showMaxUs = 0;
		shared.groundReplayed += gl.groundReplayed;
		shared.groundFallbacks += gl.groundFallbacks;
		shared.pixelReplayed += gl.pixelReplayed;
		shared.pixelCalls += gl.pixelCalls;
		shared.showOther += gl.showOther;
		shared.showOff += showOff;
		gl.showOpaque = gl.showBack = gl.showInvalid = gl.showGone = gl.showOther = 0;
		gl.groundReplayed = gl.groundFallbacks = 0;
		gl.pixelReplayed = gl.pixelCalls = 0;

		if (replayed)
		{
			shared.replays += replayed;
			shared.artDraws += gl.artDraws;
			shared.artFallbacks += gl.artFallbacks;
			shared.artRejected += gl.artRejected;
			shared.artCalls += gl.artCalls;
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
			GLDeleteProgram(gl.showProgram);
		}

		if (gl.checkData)
			MemoryFree(gl.checkData);
		if (gl.checkMask)
			MemoryFree(gl.checkMask);
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
