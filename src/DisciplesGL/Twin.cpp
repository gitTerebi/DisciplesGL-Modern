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
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_RED
#define GL_RED 0x1903
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
#define REGION_MAX 256
#define INDEX_MAGIC 0x58495754
#define INDEX_VERSION 4
#define IDENTITIES 4
#define SOURCE_WEIGHT 100
#define TRACE_LOGS 24
#define SEEN_SLOTS 65536
#define LEARN_LOGS 16
#define INDEX_MIN_SIZE 8
#define INDEX_SAVE_PERIOD 60000
#define INDEX_LOGS 32
#define MEMORY_BUDGET (256LL * 1024 * 1024)
#define MEMORY_LOGS 16
#define MEMORY_ART_LOGS 16
#define FRAME_SLOTS 65536
#define PIECE_SLOTS 131072
#define LEARN_PASS 600
#define HASH_LOGS 32
#define GROUND_LOGS 8
#define SCREEN_LOGS 64
#define SHOW_LOGS 32
#define SYNC_MAX 64
#define PIXEL_MODES 9
#define MASK_SIZE 2048
#define LOGS_FRAME 4
#define SHOT_DELAY 15000
#define SHOT_SCREEN_W 1600
#define SHOT_SCREEN_H 1200
#define SHOT_SHEET 2048
#define KNOWN_SLOTS 65536
#define REGION_HASHED -2
#define EXPECT_LOGS 32
#define EXPECT_ATTEMPTS 4
#define REHASH_AREA (128 * 128)
#define REHASH_FRAMES 30
#define ART_CHECK_LOGS 32
#define ART_CHECK_MAD 48
#define ART_CHECK_MEAN 32

typedef VOID(__stdcall* GLBLENDEQUATION)(GLenum mode);

namespace Twin
{
	enum CmdType { CmdCreate, CmdDestroy, CmdCopy, CmdFill, CmdUpload, CmdPresent, CmdReset, CmdArt, CmdGround, CmdPixel, CmdTint, CmdMask, CmdLighter };

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

	struct Region {
		RECT rect;
		ULONGLONG hash;
		BOOL learned;
		LONG frame;
		BOOL flipped;
		LONG placeW;
		LONG placeH;
		LONG cx;
		LONG cy;
		DWORD lastUse;
		BOOL content;
		DWORD checked;
	};

	struct ArtGross {
		LONG x;
		LONG y;
		DWORD mad;
		DWORD mean;
		DWORD cover;
		DWORD px;
		LONG art;
	};

	struct ArtTag {
		LONG frame;
		LONG cx;
		LONG cy;
		LONG canvasW;
		LONG canvasH;
		DWORD src;
	};

	struct ArtPlan {
		LONG frame;
		BOOL flipped;
		RECT place;
		BOOL cached;
	};

	struct Identity {
		LONG frame;
		LONG cx;
		LONG cy;
		LONG canvasW;
		LONG canvasH;
		LONG flipped;
		DWORD count;
		DWORD source;
	};

	// the packer dedupes identical 1x blocks across animation frames, so one block can legitimately be several pieces; each is kept with its evidence
	struct IndexEntry {
		ULONGLONG hash;
		LONG width;
		LONG height;
		DWORD idCount;
		LONG pad;
		Identity ids[IDENTITIES];
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
		DWORD artGross;
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
		ArtTag artTag[ART_MAX];
		Region* regions;
		BOOL memory;
		BOOL anonymous;
		DWORD lastUse;
		DWORD regionCount;
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
		DWORD fillRects;
		DWORD fillMerged;
		DWORD tints;
		DWORD blendRows;
		DWORD blendMerged;
		DWORD lighters;
		DWORD pixelFallbacks;
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
		IndexEntry* items;
		DWORD capacity;
		DWORD count;
		BOOL dirty;
		BOOL loaded;
		DWORD saveTick;
		DWORD regionHits;
		DWORD lookups;
		DWORD hits;
		DWORD learnedExpect;
		DWORD learnedSource;
		DWORD conflicts;
		DWORD misses;
		DWORD rejected;
		DWORD logs;
		DWORD multi;
		DWORD contextDraws;
		DWORD contentDraws;
		DWORD evidence;
		DWORD traceLogs;
		DWORD untwinnedLogs;
		DWORD clipLogs;
		DWORD notReadyLogs;
		DWORD learnLogs;
		DWORD learnedLogs;
		DWORD learnCalls;
		BOOL learning;
	} index;

	struct {
		LONG frame[FRAME_SLOTS];
		DWORD learned[FRAME_SLOTS];
		DWORD pass[FRAME_SLOTS];
		BYTE multi[FRAME_SLOTS];
		struct {
			LONG frame;
			LONG cx;
			LONG cy;
			LONG w;
			LONG h;
		} pieces[PIECE_SLOTS];
		DWORD pieceCount;
		struct {
			LONG frame;
			LONG cx;
			LONG cy;
			LONG canvasW;
			LONG canvasH;
			LONG w;
			LONG h;
		} known[KNOWN_SLOTS];
		DWORD knownCount;
		DWORD skipped;
		DWORD slowHashes;
		DWORD hashLogs;
		volatile LONG saving;
	} learn;

	struct {
		ULONGLONG cycles;
		const CHAR* name;
		ULONGLONG tsc;
		LONGLONG qpc;
	} slow;

	struct Timed {
		const CHAR* name;
		ULONGLONG start;
		Timed(const CHAR* label) : name(label), start(__rdtsc()) {}
		~Timed()
		{
			ULONGLONG cycles = __rdtsc() - start;
			if (cycles > slow.cycles)
			{
				slow.cycles = cycles;
				slow.name = name;
			}
		}
	};

	struct {
		DWORD count;
		LONGLONG bytes;
		DWORD created;
		DWORD evictions;
		DWORD logs;
		DWORD artLogs;
		DWORD refused;
		DWORD anonymous;
		DWORD anonymousLogs;
		DWORD replaced;
		DWORD addrHits;
		DWORD artDraws;
	} memory;

	struct {
		const CHAR* name;
		DWORD mode;
		BOOL mirror;
		Record* source;
		DWORD srcId;
		RECT from;
		Record* target;
		DWORD dstId;
		RECT to;
		const CHAR* outcome;
	} trace;

	struct Seen {
		DWORD stamp;
		LONG frame;
		LONG flipped;
		LONG dx;
		LONG dy;
		LONG sx;
		LONG sy;
		LONG w;
		LONG h;
		DWORD expect;
	};

	struct {
		Seen slots[SEEN_SLOTS];
		DWORD stamp;
		ULONGLONG frameCycles;
		ULONGLONG sumCycles;
		ULONGLONG maxCycles;
		DWORD frames;
	} seen;

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
		DWORD cachedDraws;
		DWORD useClock;
		DWORD groundArts;
		DWORD groundNoTwin;
		DWORD groundLogs;
		DWORD px;
		DWORD artLogs;
		DWORD rejectLogs;
		DWORD frameLogs;
		const CHAR* reason;
		BOOL drew;
		BOOL checked;
		DWORD firstTick;
		BOOL shotTaken;
		ArtTag lastTag;
		DWORD expectVerified;
		DWORD expectUnverified;
		DWORD expectProvenance;
		DWORD expectRejectSize;
		DWORD expectRejectSource;
		DWORD expectRejectContent;
		DWORD expectStale;
		DWORD expectLogs;
		DWORD expectOkLogs;
		struct {
			LONG frame;
			DWORD src;
			const CHAR* reason;
		} expectSeen[16];
		DWORD expectSeenNext;
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
		DWORD artCalls;
		DWORD artMasked;
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
		GLint fillLoc;
		GLint tintedLoc;
		GLint lighterLoc;
		GLint solidLoc;
		GLint srcMaskLoc;
		GLint srcMapLoc;
		GLint srcKeyLoc;
		GLint boxLoc;
		DWORD* checkBox;
		DWORD artMasked;
		DWORD artCheckLogs;
		DWORD artCheckOk;
		LONG artBadFrames[32];
		DWORD artBadNext;
		GLuint maskProgram;
		GLint maskKeyedLoc;
		GLint maskKeyLoc;
		GLint maskFrameLoc;
		GLint maskExtentLoc;
		GLuint maskTexture;
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
		twin->regionCount = 0;
		twin->count = 1;
		SetRect(&twin->dirty[0], 0, 0, twin->width, twin->height);
	}

	VOID Upload(Record* twin, DWORD id, const RECT* rect);
	VOID DropRegions(Record* twin, const RECT* rect);

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

		if (twin->regionCount)
			DropRegions(twin, &rect);

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

	BOOL SameTag(const ArtTag* a, const ArtTag* b)
	{
		return a->frame == b->frame && a->cx == b->cx && a->cy == b->cy && a->canvasW == b->canvasW && a->canvasH == b->canvasH;
	}

	// the tag says which identity put 4K art there, so the art check can name the culprit
	VOID AddArt(Record* twin, const RECT* rect, const ArtTag* tag = NULL)
	{
		if (rect->left >= rect->right || rect->top >= rect->bottom)
			return;

		ArtTag none = { -1, 0, 0, 0, 0, TWIN_NONE };
		if (!tag)
			tag = &none;

		for (DWORD i = 0; i < twin->artCount; ++i)
		{
			if (EqualRect(&twin->art[i], rect))
			{
				twin->artTag[i] = *tag;
				return;
			}
			// a different identity inside a bigger art rect is kept separately while there is room, so the check can name it
			if (Contains(&twin->art[i], rect) && (tag->frame < 0 || SameTag(&twin->artTag[i], tag) || twin->artCount >= ART_MAX / 2))
				return;
		}

		if (twin->artCount < ART_MAX)
		{
			twin->artTag[twin->artCount] = *tag;
			twin->art[twin->artCount++] = *rect;
			return;
		}

		RECT bound = *rect;
		for (DWORD i = 0; i < twin->artCount; ++i)
			UnionRect(&bound, &bound, &twin->art[i]);
		twin->artCount = 1;
		twin->art[0] = bound;
		twin->artTag[0] = none;
	}

	VOID CoverArt(Record* twin, const RECT* rect)
	{
		for (DWORD i = twin->artCount; i; --i)
			if (Contains(rect, &twin->art[i - 1]))
			{
				--twin->artCount;
				twin->art[i - 1] = twin->art[twin->artCount];
				twin->artTag[i - 1] = twin->artTag[twin->artCount];
			}
	}

	VOID DropRegions(Record* twin, const RECT* rect)
	{
		RECT part;
		for (DWORD i = twin->regionCount; i; --i)
			if (IntersectRect(&part, rect, &twin->regions[i - 1].rect))
				twin->regions[i - 1] = twin->regions[--twin->regionCount];
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
		// an upload only brings unchanged 1x onto the twin; real writes already dropped their regions when they were marked
		CoverArt(twin, rect);

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
		DropRegions(twin, rect);
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
		// memory twins can interleave rows with other images in one buffer, so containment is 2D
		return twin->owner && ptr >= twin->base && ptr < twin->base + twin->pitch * twin->height * sizeof(DWORD) &&
			(DWORD)((ptr - twin->base) % (twin->pitch * sizeof(DWORD))) / sizeof(DWORD) < (DWORD)twin->width;
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
			RECT clipped = game.syncs[i].rect;
			const RECT* rect = &clipped;
			if (twin->owner != game.syncs[i].owner || !twin->mirror || !Clip(twin, &clipped))
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

	VOID DrainSyncIn(Record* twin, const RECT* rect)
	{
		RECT part;
		for (DWORD i = 0; i < game.syncCount; ++i)
			if (game.syncs[i].owner == twin->owner && IntersectRect(&part, &game.syncs[i].rect, rect))
			{
				DrainSync();
				return;
			}
	}

	VOID DeferSync(Record* twin, DWORD id, const RECT* rect)
	{
		// the hook writes 1x right after its Twin calls, and some hooks send several calls before writing, so pixel calls leave the queue alone and other entry points drain it
		if (game.syncCount)
		{
			RECT* last = &game.syncs[game.syncCount - 1].rect;
			if (game.syncs[game.syncCount - 1].owner == twin->owner && (Contains(last, rect) ||
				(last->left == rect->left && last->right == rect->right && last->bottom == rect->top)))
			{
				UnionRect(last, last, rect);
				return;
			}
		}

		if (game.syncCount == SYNC_MAX)
			DrainSync();
		game.syncs[game.syncCount].owner = twin->owner;
		game.syncs[game.syncCount].id = id;
		game.syncs[game.syncCount].rect = *rect;
		++game.syncCount;
	}

	VOID Sync(BOOL drain = TRUE)
	{
		if (drain)
			DrainSync();

		LONG gen = shared.gen;
		if (gen == game.seenGen)
			return;

		game.seenGen = gen;
		Rebuild(FALSE);
		DebugLog("twin rebuild after GL release, twins %u", game.alive);
	}

	VOID LoadIndex();

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

		LoadIndex();

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
		if (twin->memory)
		{
			--memory.count;
			memory.bytes -= (LONGLONG)twin->width * twin->height * sizeof(DWORD) * game.scale * game.scale;
			twin->memory = FALSE;
		}
		if (twin->mirror)
			MemoryFree(twin->mirror);
		twin->mirror = NULL;
		if (twin->regions)
			MemoryFree(twin->regions);
		twin->regions = NULL;
		twin->regionCount = 0;
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

	DWORD* FrameLearned(LONG frame, BOOL create);

	Seen* SeenSlot(LONG frame, BOOL flipped, const ArtPart* part, BOOL* found)
	{
		DWORD mask = SEEN_SLOTS - 1;
		DWORD slot = ((DWORD)frame * 2654435761u ^ (DWORD)part->dx * 73856093u ^ (DWORD)part->dy * 19349663u ^ (DWORD)part->sx * 83492791u ^
			(DWORD)part->sy * 2246822519u ^ (DWORD)part->w * 3266489917u ^ (DWORD)part->h ^ (flipped ? 0x9E3779B9u : 0)) & mask;
		for (DWORD probes = 0; probes < SEEN_SLOTS; ++probes, slot = (slot + 1) & mask)
		{
			Seen* entry = &seen.slots[slot];
			if (entry->stamp != seen.stamp)
			{
				*found = FALSE;
				return entry;
			}
			if (entry->frame == frame && !entry->flipped == !flipped && entry->dx == part->dx && entry->dy == part->dy && entry->sx == part->sx && entry->sy == part->sy && entry->w == part->w && entry->h == part->h)
			{
				*found = TRUE;
				return entry;
			}
		}
		*found = FALSE;
		return NULL;
	}

	VOID ExpectParts(LONG frame, BOOL flipped, const RECT* place, const RECT* clip, const ArtPart* parts, DWORD count)
	{
		BOOL found;
		Seen* first = SeenSlot(frame, flipped, &parts[0], &found);
		// the game renders the same image at the same spot many times a frame; while that registration is still pending the whole call is a repeat
		if (found && first && !art.expect[first->expect].used)
		{
			++art.duplicates;
			return;
		}

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
		image->clip = clip ? *clip : *place;

		for (DWORD i = 0; i < count; ++i)
		{
			const ArtPart* part = &parts[i];
			if (part->w <= 0 || part->h <= 0)
				continue;

			Seen* slot = SeenSlot(frame, flipped, part, &found);
			if (!slot)
			{
				++art.overflow;
				continue;
			}
			if (found)
			{
				// a second copy of a repeated image still needs its context
				art.expect[slot->expect].used = FALSE;
				if (slot->expect < art.head)
					art.head = slot->expect;
				++art.duplicates;
				continue;
			}

			if (art.count >= MAX_EXPECT)
			{
				art.overflow += count - i;
				break;
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
			DWORD* bucket = &art.bucket[ExpectBucket(e->dx, e->dy)];
			e->next = *bucket;
			*bucket = index + 1;

			slot->stamp = seen.stamp;
			slot->frame = frame;
			slot->flipped = flipped ? 1 : 0;
			slot->dx = part->dx;
			slot->dy = part->dy;
			slot->sx = part->sx;
			slot->sy = part->sy;
			slot->w = part->w;
			slot->h = part->h;
			slot->expect = index;

			++art.expected;
			++added;
		}

		if (!added && imageIndex == art.imageCount - 1)
			--art.imageCount;
	}

	VOID ExpectArt(LONG frame, BOOL flipped, const RECT* place, const RECT* clip, const ArtPart* parts, DWORD count)
	{
		if (!Active() || !place || !parts || !count)
			return;

		Timed timed("ExpectArt");
		ULONGLONG start = __rdtsc();
		ExpectParts(frame, flipped, place, clip, parts, count);
		seen.frameCycles += __rdtsc() - start;
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

	const CHAR* TwinKind(const Record* twin);

	BOOL RecordArt(Record* target, DWORD dstId, const RECT* to, DWORD srcId, const RECT* from, LONG frame, BOOL flipped, const RECT* place, BOOL keyed, DWORD key, BOOL cached, BOOL half)
	{
		if (!ImageReplace::ArtReady(frame))
		{
			++art.notReady;
			art.reason = "art texture not ready (frame never requested or still decoding)";
			if (index.notReadyLogs < TRACE_LOGS)
			{
				++index.notReadyLogs;
				DebugLog("twin art not ready frame=%d src %u %s dst %u at frame %u", frame, srcId, TwinKind(&game.twins[srcId]), dstId, game.frameNo);
			}
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
		art.drew = TRUE;
		art.lastTag.frame = frame;
		art.lastTag.cx = to->left - place->left;
		art.lastTag.cy = to->top - place->top;
		art.lastTag.canvasW = place->right - place->left;
		art.lastTag.canvasH = place->bottom - place->top;
		art.lastTag.src = srcId;
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

		if (game.twins[srcId].memory)
			++memory.artDraws;
		if (game.twins[srcId].memory && memory.artLogs < MEMORY_ART_LOGS)
		{
			++memory.artLogs;
			DebugLog("twin art from memory twin %u base=%p frame=%d sheet=%d,%d %dx%d dst=%p %d,%d canvas=%d,%d of %dx%d%s%s at frame %u", srcId, game.twins[srcId].base, frame,
				from->left, from->top, to->right - to->left, to->bottom - to->top, target->owner, to->left, to->top, to->left - place->left, to->top - place->top,
				place->right - place->left, place->bottom - place->top, flipped ? " flipped" : "", keyed ? " keyed" : "", game.frameNo);
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

	BOOL PixelPos(const Record* twin, const VOID* ptr, LONG pitch, LONG width, LONG height, LONG* x, LONG* y);
	VOID CreateTwin(StateBuffer* buffer, LONG width, LONG height);

	DWORD* FrameLearned(LONG frame, BOOL create)
	{
		DWORD mask = FRAME_SLOTS - 1;
		for (DWORD slot = ((DWORD)frame * 2654435761u) & mask, probes = 0; probes < FRAME_SLOTS; slot = (slot + 1) & mask, ++probes)
		{
			if (learn.learned[slot] && learn.frame[slot] == frame)
				return &learn.learned[slot];
			if (!learn.learned[slot])
			{
				if (!create)
					return NULL;
				learn.frame[slot] = frame;
				return &learn.learned[slot];
			}
		}
		return NULL;
	}

	// a piece is a part's place in its frame's canvas; counting unique pieces, not learned positions, keeps the fast path from closing before every part is known
	VOID CountLearned(LONG frame, LONG cx, LONG cy, LONG w, LONG h)
	{
		if (learn.pieceCount * 10 >= PIECE_SLOTS * 7)
			return;

		DWORD mask = PIECE_SLOTS - 1;
		DWORD slot = ((DWORD)frame * 2654435761u ^ (DWORD)cx * 73856093u ^ (DWORD)cy * 19349663u ^ (DWORD)w * 83492791u ^ (DWORD)h) & mask;
		for (;; slot = (slot + 1) & mask)
		{
			if (!learn.pieces[slot].w)
				break;
			if (learn.pieces[slot].frame == frame && learn.pieces[slot].cx == cx && learn.pieces[slot].cy == cy && learn.pieces[slot].w == w && learn.pieces[slot].h == h)
				return;
		}

		learn.pieces[slot].frame = frame;
		learn.pieces[slot].cx = cx;
		learn.pieces[slot].cy = cy;
		learn.pieces[slot].w = w;
		learn.pieces[slot].h = h;
		++learn.pieceCount;

		DWORD* count = FrameLearned(frame, TRUE);
		if (count)
			++*count;
	}

	// pieces whose 1x pixels were seen at the decode; an expectation naming one of them must show those pixels
	DWORD KnownSlot(LONG frame, LONG cx, LONG cy, LONG canvasW, LONG canvasH, LONG w, LONG h, BOOL* found)
	{
		DWORD mask = KNOWN_SLOTS - 1;
		DWORD slot = ((DWORD)frame * 2654435761u ^ (DWORD)cx * 73856093u ^ (DWORD)cy * 19349663u ^ (DWORD)canvasW * 83492791u ^ (DWORD)canvasH * 2246822519u ^
			(DWORD)w * 3266489917u ^ (DWORD)h) & mask;
		for (;; slot = (slot + 1) & mask)
		{
			if (!learn.known[slot].w)
			{
				*found = FALSE;
				return slot;
			}
			if (learn.known[slot].frame == frame && learn.known[slot].cx == cx && learn.known[slot].cy == cy && learn.known[slot].canvasW == canvasW &&
				learn.known[slot].canvasH == canvasH && learn.known[slot].w == w && learn.known[slot].h == h)
			{
				*found = TRUE;
				return slot;
			}
		}
	}

	VOID KnownAdd(LONG frame, LONG cx, LONG cy, LONG canvasW, LONG canvasH, LONG w, LONG h)
	{
		if (learn.knownCount * 10 >= KNOWN_SLOTS * 7 || w <= 0)
			return;

		BOOL found;
		DWORD slot = KnownSlot(frame, cx, cy, canvasW, canvasH, w, h, &found);
		if (found)
			return;
		learn.known[slot].frame = frame;
		learn.known[slot].cx = cx;
		learn.known[slot].cy = cy;
		learn.known[slot].canvasW = canvasW;
		learn.known[slot].canvasH = canvasH;
		learn.known[slot].w = w;
		learn.known[slot].h = h;
		++learn.knownCount;
	}

	BOOL KnownPiece(LONG frame, LONG cx, LONG cy, LONG canvasW, LONG canvasH, LONG w, LONG h)
	{
		BOOL found;
		KnownSlot(frame, cx, cy, canvasW, canvasH, w, h, &found);
		return found;
	}

	ULONGLONG HashBlock(const DWORD* pixels, LONG pitch, LONG width, LONG height, DWORD key, BOOL* solid)
	{
		// a caller's geometry can come from a guessed pitch; never let a hash walk off the end of an allocation
		if (width <= 0 || height <= 0 || pitch < width || IsBadReadPtr(pixels, ((height - 1) * pitch + width) * sizeof(DWORD)))
		{
			*solid = TRUE;
			return 0;
		}

		LARGE_INTEGER start;
		BOOL timed = width * height >= 64 * 64;
		if (timed)
			QueryPerformanceCounter(&start);

		DWORD h1 = 2166136261u, h2 = 0x9747B28Cu ^ (DWORD)(width * 31 + height);
		DWORD first = ((pixels[0] & 0x00FFFFFF) == 0x00FF00FF || (pixels[0] & 0x00FFFFFF) == key) ? 0 : pixels[0] & 0x00FFFFFF;
		BOOL same = TRUE;
		for (LONG y = 0; y < height; ++y)
		{
			const DWORD* row = pixels + y * pitch;
			for (LONG x = 0; x < width; ++x)
			{
				DWORD c = row[x] & 0x00FFFFFF;
				if (c == 0x00FF00FF || c == key)
					c = 0;
				same = same && c == first;
				h1 = (h1 ^ c) * 16777619u;
				h2 = (h2 ^ c) * 0x5BD1E995u;
				h2 ^= h2 >> 15;
			}
		}

		if (timed && game.frequency.QuadPart)
		{
			LARGE_INTEGER end;
			QueryPerformanceCounter(&end);
			DWORD us = DWORD((end.QuadPart - start.QuadPart) * 1000000 / game.frequency.QuadPart);
			if (us > 500)
			{
				++learn.slowHashes;
				if (learn.hashLogs < HASH_LOGS)
				{
					++learn.hashLogs;
					DebugLog("twin slow hash %dx%d took %u us at frame %u", width, height, us, game.frameNo);
				}
			}
		}

		*solid = same;
		return ((ULONGLONG)h1 << 32) | h2;
	}

	DWORD HashKey(const Record* twin)
	{
		return twin && twin->keyed ? twin->key & 0x00FFFFFF : 0xFFFFFFFF;
	}

	IndexEntry* IndexFind(ULONGLONG hash, LONG width, LONG height)
	{
		if (!index.capacity)
			return NULL;

		DWORD mask = index.capacity - 1;
		for (DWORD slot = DWORD(hash ^ (hash >> 32)) & mask;; slot = (slot + 1) & mask)
		{
			IndexEntry* entry = &index.items[slot];
			if (!entry->width)
				return NULL;
			if (entry->hash == hash && entry->width == width && entry->height == height)
				return entry;
		}
	}

	VOID IndexPlace(IndexEntry* items, DWORD capacity, const IndexEntry* entry)
	{
		DWORD mask = capacity - 1;
		DWORD slot = DWORD(entry->hash ^ (entry->hash >> 32)) & mask;
		while (items[slot].width)
			slot = (slot + 1) & mask;
		items[slot] = *entry;
	}

	IndexEntry* IndexCreate(ULONGLONG hash, LONG width, LONG height)
	{
		IndexEntry* found = IndexFind(hash, width, height);
		if (found)
			return found;

		if ((index.count + 1) * 10 >= index.capacity * 7)
		{
			DWORD capacity = index.capacity ? index.capacity * 2 : 4096;
			IndexEntry* items = (IndexEntry*)MemoryAlloc(capacity * sizeof(IndexEntry));
			if (!items)
				return NULL;
			MemoryZero(items, capacity * sizeof(IndexEntry));
			for (DWORD i = 0; i < index.capacity; ++i)
				if (index.items[i].width)
					IndexPlace(items, capacity, &index.items[i]);

			Lock();
			IndexEntry* old = index.items;
			index.items = items;
			index.capacity = capacity;
			Unlock();
			if (old)
				MemoryFree(old);
		}

		IndexEntry entry;
		MemoryZero(&entry, sizeof(entry));
		entry.hash = hash;
		entry.width = width;
		entry.height = height;

		Lock();
		IndexPlace(index.items, index.capacity, &entry);
		++index.count;
		Unlock();
		return IndexFind(hash, width, height);
	}

	BOOL SameIdentity(const Identity* a, const Identity* b)
	{
		return a->frame == b->frame && a->cx == b->cx && a->cy == b->cy && a->canvasW == b->canvasW && a->canvasH == b->canvasH && !a->flipped == !b->flipped;
	}

	// the identity used when only the pixels are known: the best supported one with at least two sightings, earliest on a tie
	const Identity* Dominant(const IndexEntry* entry)
	{
		const Identity* best = NULL;
		for (DWORD i = 0; i < entry->idCount; ++i)
			if (entry->ids[i].count >= 2 && (!best || entry->ids[i].count > best->count))
				best = &entry->ids[i];
		return best;
	}

	VOID MarkMulti(const IndexEntry* entry)
	{
		for (DWORD i = 0; i < entry->idCount; ++i)
		{
			DWORD* learned = FrameLearned(entry->ids[i].frame, TRUE);
			if (learned)
				learn.multi[learned - learn.learned] = TRUE;
		}
	}

	VOID AddEvidence(IndexEntry* entry, const Identity* seen, DWORD weight)
	{
		Lock();
		Identity* slot = NULL;
		for (DWORD i = 0; i < entry->idCount && !slot; ++i)
			if (SameIdentity(&entry->ids[i], seen))
				slot = &entry->ids[i];

		BOOL added = FALSE;
		if (slot)
			slot->count = min(slot->count + weight, 0x7FFFFFFFu);
		else
		{
			if (entry->idCount < IDENTITIES)
				slot = &entry->ids[entry->idCount++];
			else
			{
				slot = &entry->ids[0];
				for (DWORD i = 1; i < IDENTITIES; ++i)
					if (entry->ids[i].count < slot->count)
						slot = &entry->ids[i];
			}
			*slot = *seen;
			slot->count = weight;
			slot->source = 0;
			added = TRUE;
		}
		BOOL source = weight >= SOURCE_WEIGHT && !slot->source;
		if (source)
			slot->source = 1;
		index.dirty = TRUE;
		Unlock();

		if (source)
			KnownAdd(slot->frame, slot->cx, slot->cy, slot->canvasW, slot->canvasH, entry->width, entry->height);

		++index.evidence;
		if (added && entry->idCount == 2)
			++index.multi;
		if (entry->idCount > 1)
			MarkMulti(entry);
		if (slot->count >= 2 && Dominant(entry) == slot)
			CountLearned(slot->frame, slot->cx, slot->cy, entry->width, entry->height);
	}

	VOID IndexPath(CHAR* path, const CHAR* name)
	{
		DWORD length = GetModuleFileName(NULL, path, MAX_PATH);
		while (length && path[length - 1] != '\\')
			--length;
		path[length] = 0;
		StrCat(path, name);
	}

	VOID LoadIndex()
	{
		if (index.loaded)
			return;
		index.loaded = TRUE;

		CHAR path[MAX_PATH];
		IndexPath(path, "twin-index.bin");
		HANDLE file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			DebugLog("twin index none at start");
			return;
		}

		DWORD header[4] = {}, read = 0;
		if (!ReadFile(file, header, sizeof(header), &read, NULL) || read != sizeof(header) || header[0] != INDEX_MAGIC || header[1] != INDEX_VERSION || header[2] != sizeof(IndexEntry))
		{
			CloseHandle(file);
			DebugLog("twin index rejected: header %08X version %u entry %u, expected version %u entry %u; starting empty", header[0], header[1], header[2], INDEX_VERSION, sizeof(IndexEntry));
			return;
		}

		DWORD count = header[3], loaded = 0;
		IndexEntry entry;
		while (loaded < count && ReadFile(file, &entry, sizeof(entry), &read, NULL) && read == sizeof(entry))
		{
			++loaded;
			if (entry.width < INDEX_MIN_SIZE || entry.height < INDEX_MIN_SIZE || !entry.idCount || entry.idCount > IDENTITIES)
				continue;

			IndexEntry* slot = IndexCreate(entry.hash, entry.width, entry.height);
			if (!slot)
				continue;
			Lock();
			slot->idCount = entry.idCount;
			MemoryCopy(slot->ids, entry.ids, sizeof(entry.ids));
			Unlock();

			if (slot->idCount > 1)
			{
				++index.multi;
				MarkMulti(slot);
			}
			for (DWORD i = 0; i < slot->idCount; ++i)
				if (slot->ids[i].source)
					KnownAdd(slot->ids[i].frame, slot->ids[i].cx, slot->ids[i].cy, slot->ids[i].canvasW, slot->ids[i].canvasH, slot->width, slot->height);
			const Identity* dominant = Dominant(slot);
			if (dominant)
				CountLearned(dominant->frame, dominant->cx, dominant->cy, slot->width, slot->height);
		}
		CloseHandle(file);

		index.dirty = FALSE;
		DebugLog("twin index loaded %u of %u entries, %u pieces known at the decode", index.count, count, learn.knownCount);
	}

	VOID SaveIndex()
	{
		if (shared.state != 2)
			return;

		Lock();
		DWORD count = 0;
		IndexEntry* copy = index.count ? (IndexEntry*)MemoryAlloc(index.count * sizeof(IndexEntry)) : NULL;
		if (copy)
			for (DWORD i = 0; i < index.capacity && count < index.count; ++i)
				if (index.items[i].width && index.items[i].idCount)
					copy[count++] = index.items[i];
		BOOL dirty = index.dirty;
		index.dirty = FALSE;
		Unlock();

		if (!copy)
			return;
		if (!dirty)
		{
			MemoryFree(copy);
			return;
		}

		CHAR path[MAX_PATH], temp[MAX_PATH];
		IndexPath(path, "twin-index.bin");
		IndexPath(temp, "twin-index.bin.tmp");

		BOOL done = FALSE;
		HANDLE file = CreateFile(temp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			DWORD header[4] = { INDEX_MAGIC, INDEX_VERSION, sizeof(IndexEntry), count };
			DWORD written = 0, body = 0;
			done = WriteFile(file, header, sizeof(header), &written, NULL) && written == sizeof(header) &&
				WriteFile(file, copy, count * sizeof(IndexEntry), &body, NULL) && body == count * sizeof(IndexEntry);
			CloseHandle(file);
			done = done && MoveFileEx(temp, path, MOVEFILE_REPLACE_EXISTING);
		}
		MemoryFree(copy);

		if (!done)
		{
			Lock();
			index.dirty = TRUE;
			Unlock();
		}
		DebugLog("twin index %s %u entries", done ? "saved" : "save failed for", count);
	}

	DWORD __stdcall SaveWorker(LPVOID)
	{
		SaveIndex();
		InterlockedExchange(&learn.saving, 0);
		return 0;
	}

	BOOL EnsureRegions(Record* source)
	{
		if (!source->regions)
		{
			source->regions = (Region*)MemoryAlloc(REGION_MAX * sizeof(Region));
			source->regionCount = 0;
		}
		return source->regions != NULL;
	}

	Region* AddRegion(Record* source, const RECT* rect, ULONGLONG hash, LONG frame, BOOL flipped, LONG placeW, LONG placeH, LONG cx, LONG cy)
	{
		if (!EnsureRegions(source))
			return NULL;

		// no pixels changed, so a hash cached over a bigger rect is still true
		RECT part;
		for (DWORD i = source->regionCount; i; --i)
			if (source->regions[i - 1].frame != REGION_HASHED && IntersectRect(&part, rect, &source->regions[i - 1].rect))
				source->regions[i - 1] = source->regions[--source->regionCount];

		Region* region;
		if (source->regionCount < REGION_MAX)
			region = &source->regions[source->regionCount++];
		else
		{
			region = &source->regions[0];
			for (DWORD i = 1; i < REGION_MAX; ++i)
				if (source->regions[i].lastUse < region->lastUse)
					region = &source->regions[i];
		}

		region->rect = *rect;
		region->hash = hash;
		region->learned = FALSE;
		region->frame = frame;
		region->flipped = flipped;
		region->placeW = placeW;
		region->placeH = placeH;
		region->cx = cx;
		region->cy = cy;
		region->lastUse = ++art.useClock;
		region->content = FALSE;
		region->checked = game.frameNo;
		return region;
	}

	// remembers the hash of an expectation's whole part without naming it, so a rejected claim repeated every frame is not rehashed
	VOID CacheHash(Record* source, const RECT* rect, ULONGLONG hash)
	{
		if (!EnsureRegions(source))
			return;

		for (DWORD i = 0; i < source->regionCount; ++i)
			if (EqualRect(&source->regions[i].rect, rect))
			{
				if (source->regions[i].frame == REGION_HASHED || !source->regions[i].hash)
					source->regions[i].hash = hash;
				return;
			}
		if (source->regionCount >= REGION_MAX)
			return;

		Region* region = &source->regions[source->regionCount++];
		MemoryZero(region, sizeof(Region));
		region->rect = *rect;
		region->hash = hash;
		region->frame = REGION_HASHED;
		region->lastUse = ++art.useClock;
		region->checked = game.frameNo;
	}

	BOOL CachedHash(const Record* source, const RECT* rect, ULONGLONG* hash)
	{
		for (DWORD i = 0; i < source->regionCount; ++i)
			if (source->regions[i].hash && EqualRect(&source->regions[i].rect, rect))
			{
				*hash = source->regions[i].hash;
				return TRUE;
			}
		return FALSE;
	}

	Region* FindRegion(Record* source, const RECT* from)
	{
		for (DWORD i = 0; i < source->regionCount; ++i)
			if (source->regions[i].frame != REGION_HASHED && Contains(&source->regions[i].rect, from))
				return &source->regions[i];
		return NULL;
	}

	BOOL RegionAgrees(const Region* region, const Image* image, const RECT* from, const RECT* to)
	{
		return region->frame == image->frame && !region->flipped == !image->flipped && region->placeW == image->place.right - image->place.left &&
			region->placeH == image->place.bottom - image->place.top && region->cx + from->left - region->rect.left == to->left - image->place.left &&
			region->cy + from->top - region->rect.top == to->top - image->place.top;
	}

	BOOL HasIdentity(const IndexEntry* entry, LONG frame, LONG cx, LONG cy, LONG canvasW, LONG canvasH)
	{
		for (DWORD i = 0; i < entry->idCount; ++i)
			if (entry->ids[i].frame == frame && entry->ids[i].cx == cx && entry->ids[i].cy == cy && entry->ids[i].canvasW == canvasW && entry->ids[i].canvasH == canvasH)
				return TRUE;
		return FALSE;
	}

	// the source already carries 4K art of this frame over the whole part and nothing else: the copy repeats a render that was itself accepted
	BOOL Provenance(const Record* source, const RECT* part, LONG frame)
	{
		BOOL covered = FALSE;
		RECT overlap;
		for (DWORD i = 0; i < source->artCount; ++i)
			if (IntersectRect(&overlap, &source->art[i], part))
			{
				if (source->artTag[i].frame != frame)
					return FALSE;
				if (Contains(&source->art[i], part))
					covered = TRUE;
			}
		return covered;
	}

	VOID ExpectLog(const CHAR* reason, const Expect* e, DWORD srcId, const Record* source, const RECT* from, const RECT* to, const CHAR* detail, BOOL normal = FALSE)
	{
		const Image* image = &art.images[e->image];
		for (DWORD i = 0; i < sizeof(art.expectSeen) / sizeof(*art.expectSeen); ++i)
			if (art.expectSeen[i].reason == reason && art.expectSeen[i].frame == image->frame && art.expectSeen[i].src == srcId)
				return;
		DWORD slot = art.expectSeenNext++ % (sizeof(art.expectSeen) / sizeof(*art.expectSeen));
		art.expectSeen[slot].reason = reason;
		art.expectSeen[slot].frame = image->frame;
		art.expectSeen[slot].src = srcId;

		// the normal outcomes get a small budget of their own so they never starve the rejections
		DWORD* budget = normal ? &art.expectOkLogs : &art.expectLogs;
		if (*budget >= (normal ? EXPECT_LOGS / 4 : EXPECT_LOGS))
			return;
		++*budget;
		DebugLog("twin expect %s frame=%d part sheet=%d,%d %dx%d canvas=%d,%d of %dx%d | copy src %u %s %dx%d %d,%d %dx%d -> %d,%d | %s at frame %u", reason, image->frame,
			e->sx, e->sy, e->w, e->h, e->dx - image->place.left, e->dy - image->place.top, image->place.right - image->place.left, image->place.bottom - image->place.top,
			srcId, TwinKind(source), source->width, source->height, from->left, from->top, from->right - from->left, from->bottom - from->top, to->left, to->top, detail, game.frameNo);
	}

	// an expectation names an image by where the game drew it, not by what the source holds, so it may claim a copy only when the source can be that render
	BOOL ExpectAllowed(const Expect* e, Record* source, DWORD srcId, const RECT* from, const RECT* to, Region** found, BOOL* verified)
	{
		*verified = FALSE;
		const Image* image = &art.images[e->image];
		LONG placeW = image->place.right - image->place.left, placeH = image->place.bottom - image->place.top;
		LONG cx = e->dx - image->place.left, cy = e->dy - image->place.top;
		RECT part = { e->sx, e->sy, e->sx + e->w, e->sy + e->h };
		CHAR detail[160];

		// the part lives whole in the sheet the game copies from; a smaller source is some other buffer at the same coordinates
		if (part.left < 0 || part.top < 0 || part.right > source->width || part.bottom > source->height)
		{
			++art.expectRejectSize;
			ExpectLog("rejected size", e, srcId, source, from, to, "the part does not fit in the source");
			return FALSE;
		}

		Region* region = *found;
		if (region && RegionAgrees(region, image, from, to))
			return TRUE;

		BOOL trusted = FALSE;
		if (region && region->content && region->frame >= 0)
		{
			trusted = TRUE;
			// an untracked rewrite (hover state drawn over the old one) leaves a region behind; a fresh hash tells it apart from a wrong render call
			if (Area(&region->rect) <= REHASH_AREA || game.frameNo - region->checked >= REHASH_FRAMES)
			{
				BOOL solid;
				ULONGLONG hash = HashBlock((const DWORD*)source->base + region->rect.top * source->pitch + region->rect.left, source->pitch,
					region->rect.right - region->rect.left, region->rect.bottom - region->rect.top, HashKey(source), &solid);
				++index.lookups;
				if (solid || hash != region->hash)
				{
					++art.expectStale;
					StrPrint(detail, "source region frame=%d %d,%d %dx%d changed since it was identified, dropped", region->frame, region->rect.left, region->rect.top,
						region->rect.right - region->rect.left, region->rect.bottom - region->rect.top);
					ExpectLog("stale region", e, srcId, source, from, to, detail);
					RECT stale = region->rect;
					DropRegions(source, &stale);
					region = *found = FindRegion(source, from);
					if (region && RegionAgrees(region, image, from, to))
						return TRUE;
					trusted = region && region->content && region->frame >= 0;
				}
				else
					region->checked = game.frameNo;
			}
		}

		if (!KnownPiece(image->frame, cx, cy, placeW, placeH, e->w, e->h))
		{
			if (trusted)
			{
				++art.expectRejectSource;
				StrPrint(detail, "source already identified as frame=%d canvas=%d,%d of %dx%d%s, content unchanged, part never seen at a decode", region->frame,
					region->cx, region->cy, region->placeW, region->placeH, region->learned ? " (learned at the decode)" : "");
				ExpectLog("rejected source", e, srcId, source, from, to, detail);
				return FALSE;
			}
			++art.expectUnverified;
			ExpectLog("allowed unverified", e, srcId, source, from, to, "part never seen at a decode, nothing to compare", TRUE);
			return TRUE;
		}

		ULONGLONG hash = 0;
		BOOL have = CachedHash(source, &part, &hash);
		if (!have)
		{
			BOOL solid;
			hash = HashBlock((const DWORD*)source->base + part.top * source->pitch + part.left, source->pitch, e->w, e->h, HashKey(source), &solid);
			++index.lookups;
			have = !solid;
			if (have)
				CacheHash(source, &part, hash);
			region = *found = FindRegion(source, from);
		}

		const IndexEntry* entry = have ? IndexFind(hash, e->w, e->h) : NULL;
		if (entry && HasIdentity(entry, image->frame, cx, cy, placeW, placeH))
		{
			++art.expectVerified;
			*verified = TRUE;
			ExpectLog("verified content", e, srcId, source, from, to, "source pixels match the part seen at the decode", TRUE);
			return TRUE;
		}

		if (!trusted && Provenance(source, &part, image->frame))
		{
			++art.expectProvenance;
			ExpectLog("allowed by provenance", e, srcId, source, from, to, "pixels differ from the decode but the source holds only this frame's 4K art there");
			return TRUE;
		}

		++art.expectRejectContent;
		const Identity* other = entry ? Dominant(entry) : NULL;
		if (other)
			StrPrint(detail, "source pixels are frame=%d canvas=%d,%d of %dx%d, not this part", other->frame, other->cx, other->cy, other->canvasW, other->canvasH);
		else
			StrPrint(detail, "source pixels %s the part seen at the decode", have ? "are unknown, not" : "are solid, not");
		ExpectLog("rejected content", e, srcId, source, from, to, detail);
		return FALSE;
	}

	BOOL PlaceSane(const Expect* e, LONG x, LONG y)
	{
		const Image* image = &art.images[e->image];
		LONG placeW = image->place.right - image->place.left;
		LONG placeH = image->place.bottom - image->place.top;
		LONG cx = e->dx - image->place.left, cy = e->dy - image->place.top;
		if (placeW >= 2 && placeH >= 2 && cx >= -1 && cy >= -1 && cx + e->w <= placeW + 1 && cy + e->h <= placeH + 1)
			return TRUE;

		++index.rejected;
		if (art.rejectLogs < REJECT_LOGS && art.frameLogs < LOGS_FRAME)
		{
			++art.rejectLogs;
			++art.frameLogs;
			DebugLog("twin expectation rejected place frame=%d sheet=%d,%d %dx%d dst=%d,%d canvas=%d,%d of %dx%d at frame %u", image->frame,
				e->sx, e->sy, e->w, e->h, x, y, cx, cy, placeW, placeH, game.frameNo);
		}
		return FALSE;
	}

	// context first: an expectation is the game's own statement of what this copy draws, once ExpectAllowed finds the source can be that render; the pixels decide otherwise
	BOOL IdentifyArt(Record* target, DWORD dstId, const RECT* to, Record* source, DWORD srcId, const RECT* from, LONG x, LONG y, const RECT* srcRect, BOOL keyed, DWORD key, BOOL half, BOOL mirror, ArtPlan* plan = NULL)
	{
		LONG width = from->right - from->left;
		LONG height = from->bottom - from->top;
		if (width < INDEX_MIN_SIZE || height < INDEX_MIN_SIZE)
		{
			art.reason = "block under 8x8";
			return FALSE;
		}

		Region* region = FindRegion(source, from);

		if (index.learnedLogs < LEARN_LOGS)
		{
			const Region* learnedRegion = NULL;
			RECT part;
			for (DWORD i = 0; i < source->regionCount && !learnedRegion; ++i)
				if (source->regions[i].learned && IntersectRect(&part, &source->regions[i].rect, from))
					learnedRegion = &source->regions[i];
			if (learnedRegion)
			{
				++index.learnedLogs;
				DebugLog("twin copy from source-learned region frame=%d region %d,%d %dx%d copy %d,%d %dx%d: %s at frame %u", learnedRegion->frame,
					learnedRegion->rect.left, learnedRegion->rect.top, learnedRegion->rect.right - learnedRegion->rect.left, learnedRegion->rect.bottom - learnedRegion->rect.top,
					from->left, from->top, width, height, region == learnedRegion ? "inside, region hit" : "only overlaps it, not inside", game.frameNo);
			}
		}

		Expect* e = NULL;
		Expect* refused[EXPECT_ATTEMPTS];
		DWORD refusedCount = 0;
		BOOL verified = FALSE;
		while (art.count && !mirror && refusedCount < EXPECT_ATTEMPTS)
		{
			e = Match(x, y, srcRect->right - srcRect->left, srcRect->bottom - srcRect->top, srcRect->left, srcRect->top);
			if (!e || (PlaceSane(e, x, y) && ExpectAllowed(e, source, srcId, from, to, &region, &verified)))
				break;
			// hidden for the next Match only; another render at the same spot may be the real one
			e->used = TRUE;
			refused[refusedCount++] = e;
			e = NULL;
		}
		for (DWORD i = 0; i < refusedCount; ++i)
			refused[i]->used = FALSE;

		if (e)
		{
			e->used = TRUE;
			while (art.head < art.count && art.expect[art.head].used)
				++art.head;
			++art.matched;

			const Image* image = &art.images[e->image];
			BOOL whole = EqualRect(from, srcRect) && x == e->dx && y == e->dy && width == e->w && height == e->h;
			ULONGLONG blockHash = region && EqualRect(&region->rect, from) ? region->hash : 0;
			if (whole)
			{
				ULONGLONG hash = 0;
				BOOL have = FALSE;
				if (region && EqualRect(&region->rect, from) && region->hash)
				{
					hash = region->hash;
					have = TRUE;
				}
				else if (CachedHash(source, from, &hash))
					have = TRUE;
				else
				{
					BOOL solid;
					hash = HashBlock((const DWORD*)source->base + from->top * source->pitch + from->left, source->pitch, width, height, HashKey(source), &solid);
					++index.lookups;
					have = !solid;
				}

				if (have)
					blockHash = hash;
				IndexEntry* entry = have ? IndexCreate(hash, width, height) : NULL;
				if (entry)
				{
					Identity seen;
					seen.frame = image->frame;
					seen.cx = e->dx - image->place.left;
					seen.cy = e->dy - image->place.top;
					seen.canvasW = image->place.right - image->place.left;
					seen.canvasH = image->place.bottom - image->place.top;
					seen.flipped = image->flipped ? 1 : 0;
					seen.count = 0;
					seen.source = 0;
					AddEvidence(entry, &seen, 1);
				}
			}

			// the game says what this is right now; cached copies that follow without a render call must keep showing it
			if (!region || !RegionAgrees(region, image, from, to))
			{
				Region* added = AddRegion(source, from, blockHash, image->frame, image->flipped,
					image->place.right - image->place.left, image->place.bottom - image->place.top, to->left - image->place.left, to->top - image->place.top);
				if (added)
					added->content = verified;
			}
			else if (verified)
				region->content = TRUE;

			++index.contextDraws;
			art.reason = "context";
			if (plan)
			{
				plan->frame = image->frame;
				plan->flipped = image->flipped;
				plan->place = image->place;
				plan->cached = FALSE;
				return TRUE;
			}
			return RecordArt(target, dstId, to, srcId, from, image->frame, image->flipped, &image->place, keyed, key, FALSE, half);
		}

		Region found;
		if (region)
		{
			region->lastUse = ++art.useClock;
			if (region->frame < 0)
			{
				art.reason = "known miss region";
				return FALSE;
			}

			++index.regionHits;
			found = *region;
			found.cx += from->left - region->rect.left;
			found.cy += from->top - region->rect.top;
		}
		else
		{
			BOOL solid;
			ULONGLONG hash = HashBlock((const DWORD*)source->base + from->top * source->pitch + from->left, source->pitch, width, height, HashKey(source), &solid);
			++index.lookups;
			if (solid)
			{
				++index.misses;
				art.reason = "solid or unreadable block";
				return FALSE;
			}

			IndexEntry* entry = IndexFind(hash, width, height);
			const Identity* dominant = entry ? Dominant(entry) : NULL;
			if (!dominant)
			{
				++index.misses;
				AddRegion(source, from, hash, -1, FALSE, 0, 0, 0, 0);
				art.reason = entry ? "hash known but no identity seen twice" : "hash miss";
				return FALSE;
			}

			++index.hits;
			found.frame = dominant->frame;
			found.flipped = dominant->flipped;
			found.placeW = dominant->canvasW;
			found.placeH = dominant->canvasH;
			found.cx = dominant->cx;
			found.cy = dominant->cy;
			Region* added = AddRegion(source, from, hash, found.frame, found.flipped, found.placeW, found.placeH, found.cx, found.cy);
			if (added)
				added->content = TRUE;
			region = NULL;
		}

		RECT place;
		BOOL flipped = found.flipped;
		if (mirror)
		{
			place.left = to->right + found.cx - found.placeW;
			flipped = !flipped;
		}
		else
			place.left = to->left - found.cx;
		place.top = to->top - found.cy;
		place.right = place.left + found.placeW;
		place.bottom = place.top + found.placeH;

		++index.contentDraws;
		art.reason = region ? "region" : "hash";
		if (plan)
		{
			plan->frame = found.frame;
			plan->flipped = flipped;
			plan->place = place;
			plan->cached = region != NULL;
			return TRUE;
		}
		return RecordArt(target, dstId, to, srcId, from, found.frame, flipped, &place, keyed, key, region != NULL, half);
	}

	VOID LearnBlock(const VOID* data, LONG pitch, const RECT* rect, LONG frame, const RECT* canvas, LONG canvasWidth, LONG canvasHeight, BOOL flipped);
	Record* Adopt(const VOID* ptr, DWORD* id);

	VOID LearnArt(const VOID* data, LONG pitch, const RECT* rect, LONG frame, const RECT* canvas, LONG canvasWidth, LONG canvasHeight, BOOL flipped)
	{
		Timed timed("LearnArt");
		if (!Active())
			return;

		++index.learnCalls;
		if (!data || !rect || !canvas || frame < 0 || pitch <= 0)
		{
			if (index.learnLogs < LEARN_LOGS)
			{
				++index.learnLogs;
				DebugLog("twin learn at source refused: data=%p pitch %d frame %d rect %s canvas %s", data, pitch, frame, rect ? "set" : "missing", canvas ? "set" : "missing");
			}
			return;
		}

		Sync();
		LearnBlock(data, pitch, rect, frame, canvas, canvasWidth, canvasHeight, flipped);
	}

	VOID LearnBlock(const VOID* data, LONG pitch, const RECT* rect, LONG frame, const RECT* canvas, LONG canvasWidth, LONG canvasHeight, BOOL flipped)
	{
		LONG width = rect->right - rect->left;
		LONG height = rect->bottom - rect->top;
		if (width < INDEX_MIN_SIZE || height < INDEX_MIN_SIZE)
			return;

		if (canvasWidth < 2 || canvasHeight < 2 || canvas->left < -1 || canvas->top < -1 || canvas->left + width > canvasWidth + 1 || canvas->top + height > canvasHeight + 1)
		{
			++index.rejected;
			if (index.logs < INDEX_LOGS)
			{
				++index.logs;
				DebugLog("twin learn rejected at source frame=%d block %dx%d canvas %d,%d of %dx%d", frame, width, height, canvas->left, canvas->top, canvasWidth, canvasHeight);
			}
			return;
		}

		const DWORD* first = (const DWORD*)data + rect->top * pitch + rect->left;
		DWORD id;
		Record* twin = FindAddress(first, &id);
		BOOL adopted = FALSE;
		if (!twin && !index.learning)
		{
			index.learning = TRUE;
			twin = Adopt(first, &id);
			index.learning = FALSE;
			adopted = twin != NULL;
		}

		// the decode that just wrote these pixels is still pending as dirty; take it now so the region made below survives the first copy
		LONG x = 0, y = 0;
		BOOL placed = twin && PixelPos(twin, first, pitch, width, height, &x, &y);
		RECT block = { x, y, x + width, y + height };
		DWORD flushed = 0;
		if (placed)
		{
			DWORD before = game.list ? game.list->counters.pixels : 0;
			DrainSyncIn(twin, &block);
			FlushRegion(twin, id, &block, TRUE);
			flushed = game.list ? game.list->counters.pixels - before : 0;
		}

		BOOL solid;
		ULONGLONG hash = HashBlock(first, pitch, width, height, HashKey(twin), &solid);
		if (solid)
		{
			if (index.learnLogs < LEARN_LOGS)
			{
				++index.learnLogs;
				DebugLog("twin learn at source frame=%d data=%p pitch %d %dx%d: block solid or unreadable, twin %s", frame, data, pitch, width, height, twin ? TwinKind(twin) : "none");
			}
			return;
		}

		IndexEntry* kept = IndexCreate(hash, width, height);
		if (!kept)
			return;

		Identity seen;
		seen.frame = frame;
		seen.cx = canvas->left;
		seen.cy = canvas->top;
		seen.canvasW = canvasWidth;
		seen.canvasH = canvasHeight;
		seen.flipped = flipped ? 1 : 0;
		seen.count = 0;
		seen.source = 0;
		AddEvidence(kept, &seen, SOURCE_WEIGHT);
		++index.learnedSource;

		const Identity* dominant = Dominant(kept);
		BOOL region = FALSE;
		if (dominant && placed)
		{
			Region* added = AddRegion(twin, &block, hash, dominant->frame, dominant->flipped, dominant->canvasW, dominant->canvasH, dominant->cx, dominant->cy);
			if (added)
			{
				added->learned = TRUE;
				added->content = TRUE;
			}
			region = added != NULL;
		}

		if (index.learnLogs < LEARN_LOGS)
		{
			++index.learnLogs;
			DebugLog("twin learn at source frame=%d data=%p pitch %d %dx%d canvas %d,%d of %dx%d | twin %s%s %u at %d,%d | region %s | flushed %u px | identity frame %d at frame %u",
				frame, data, pitch, width, height, canvas->left, canvas->top, canvasWidth, canvasHeight, twin ? TwinKind(twin) : "none", adopted ? " (created now)" : "",
				twin ? id : TWIN_NONE, x, y, region ? "created" : placed ? "not created (no identity)" : twin ? "not created (pitch or rect does not fit the twin)" : "not created (no twin)",
				flushed, dominant ? dominant->frame : -1, game.frameNo);
		}
	}

	VOID EvictMemory(LONGLONG needed)
	{
		while (memory.bytes + needed > MEMORY_BUDGET)
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
				return;

			StateBuffer* owner = oldest->owner;
			Release(oldest, oldestId);
			delete owner;
			++memory.evictions;
		}
	}

	Record* CreateMemoryTwin(const VOID* base, LONG pitch, LONG width, LONG height, LONG frame, BOOL flipped, BOOL anonymous, DWORD* id)
	{
		if (!base || width <= 0 || height <= 0 || pitch < width)
			return NULL;

		if (IsBadReadPtr(base, ((height - 1) * pitch + width) * sizeof(DWORD)))
		{
			++memory.refused;
			if (memory.logs < MEMORY_LOGS)
			{
				++memory.logs;
				DebugLog("twin memory twin refused base=%p pitch %d %dx%d: not readable", base, pitch, width, height);
			}
			return NULL;
		}

		LONGLONG bytes = (LONGLONG)width * height * sizeof(DWORD) * game.scale * game.scale;
		EvictMemory(bytes);

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
		twin->lastUse = game.frameNo;
		if (twin->mirror)
			for (LONG y = 0; y < height; ++y)
				MemoryCopy(twin->mirror + y * width, (const DWORD*)base + y * pitch, width * sizeof(DWORD));
		++memory.count;
		++memory.created;
		memory.bytes += bytes;
		twin->anonymous = anonymous;
		if (anonymous)
			++memory.anonymous;

		// upload now, before the operation that created it records anything that reads the twin
		RECT whole = { 0, 0, width, height };
		DWORD uploads = game.list ? game.list->counters.uploads : 0;
		FlushRegion(twin, *id, &whole, TRUE);
		BOOL uploaded = game.list && game.list->counters.uploads > uploads;

		if (anonymous ? memory.anonymousLogs < MEMORY_LOGS : memory.logs < MEMORY_LOGS)
		{
			if (anonymous)
				++memory.anonymousLogs;
			else
				++memory.logs;
			DebugLog("twin memory twin %u%s base=%p pitch %d %dx%d frame %d%s upload %s", *id, anonymous ? " anonymous" : "", base, pitch, width, height, frame,
				flipped ? " flipped" : "", uploaded ? "recorded" : "MISSING");
		}

		if (frame >= 0)
		{
			RECT canvas = whole;
			LearnBlock(base, pitch, &whole, frame, &canvas, width, height, flipped);
		}

		return twin;
	}

	Record* Adopt(const VOID* ptr, DWORD* id)
	{
		Record* twin = FindAddress(ptr, id);
		if (twin)
		{
			if (twin->memory)
				twin->lastUse = game.frameNo;
			return twin;
		}

		const VOID* base = NULL;
		LONG pitch = 0, width = 0, height = 0, frame = -1;
		BOOL flipped = FALSE;
		if (!ImageReplace::FindMemory(ptr, &base, &pitch, &width, &height, &frame, &flipped))
			return NULL;

		if (!CreateMemoryTwin(base, pitch, width, height, frame, flipped, FALSE, id))
			return NULL;
		return FindAddress(ptr, id);
	}

	VOID DestroyMemoryTwin(Record* twin, DWORD id)
	{
		StateBuffer* owner = twin->owner;
		Release(twin, id);
		delete owner;
	}

	VOID OnMemory(const VOID* base, LONG pitch, LONG width, LONG height)
	{
		Timed timed("OnMemory");
		if (!base || width <= 0 || height <= 0 || pitch < width || !Active())
			return;

		Sync(FALSE);

		const DWORD* first = (const DWORD*)base;
		const DWORD* last = first + (height - 1) * pitch + width - 1;
		DWORD id, lastId;
		Record* twin = FindAddress(first, &id);
		if (twin && twin->pitch == pitch && FindAddress(last, &lastId) == twin)
		{
			if (twin->memory)
				twin->lastUse = game.frameNo;
			return;
		}

		// another memory twin over any part of this region describes stale geometry
		const BYTE* begin = (const BYTE*)first;
		const BYTE* end = (const BYTE*)(last + 1);
		for (DWORD i = 0; i < game.top; ++i)
		{
			Record* other = &game.twins[i];
			if (!other->owner || !other->memory)
				continue;

			const BYTE* otherEnd = other->base + ((other->height - 1) * other->pitch + other->width) * sizeof(DWORD);
			if (other->base >= end || otherEnd <= begin)
				continue;

			BOOL overlap = FALSE;
			for (LONG y = 0; y < height && !overlap; ++y)
			{
				const BYTE* row = begin + y * pitch * sizeof(DWORD);
				overlap = InTwin(other, row) || InTwin(other, row + (width - 1) * sizeof(DWORD));
			}
			overlap = overlap || (other->base >= begin && other->base < end && (DWORD)((other->base - begin) % (pitch * sizeof(DWORD))) / sizeof(DWORD) < (DWORD)width);
			if (overlap)
			{
				++memory.replaced;
				DestroyMemoryTwin(other, i);
			}
		}

		twin = FindAddress(first, &id);
		if (twin)
			return;

		CreateMemoryTwin(base, pitch, width, height, -1, FALSE, TRUE, &id);
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
		twin->lastUse = game.frameNo;
		++memory.addrHits;
		return twin;
	}

	VOID OnMemoryFree(const VOID* data)
	{
		Timed timed("OnMemoryFree");
		if (!data)
			return;

		DrainSync();
		for (DWORD i = 0; i < game.top; ++i)
		{
			Record* twin = &game.twins[i];
			if (twin->owner && twin->memory && twin->base == (const BYTE*)data)
			{
				DestroyMemoryTwin(twin, i);
				return;
			}
		}
	}

	VOID ExpireArt()
	{
		if (art.count)
			MemoryZero(art.bucket, sizeof(art.bucket));
		art.count = art.imageCount = art.head = 0;

		if (!++seen.stamp)
		{
			MemoryZero(seen.slots, sizeof(seen.slots));
			seen.stamp = 1;
		}

		seen.sumCycles += seen.frameCycles;
		if (seen.frameCycles > seen.maxCycles)
			seen.maxCycles = seen.frameCycles;
		seen.frameCycles = 0;
		++seen.frames;
	}

	VOID CreateTwin(StateBuffer* buffer, LONG width, LONG height);

	VOID OnCreate(StateBuffer* buffer, LONG width, LONG height)
	{
		Timed timed("OnCreate");
		if (!Active() || !buffer || !buffer->data || width <= 0 || height <= 0)
			return;

		Sync();
		CreateTwin(buffer, width, height);
	}

	VOID CreateTwin(StateBuffer* buffer, LONG width, LONG height)
	{
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
		twin->regions = NULL;
		twin->regionCount = 0;
		twin->memory = FALSE;
		twin->anonymous = FALSE;
		twin->lastUse = game.frameNo;
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
		Timed timed("GroundArt");
		if (!Active() || !data || !rect || !ops || !bytes)
			return;

		// GroundFlush can call this from inside a pixel hook's sequence, where draining every pending mirror sync would read pixels before their write
		Sync(FALSE);

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

		DrainSyncIn(twin, &area);
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

		DropRegions(twin, &area);
		CoverArt(twin, &area);
		ArtTag ground = { -3, 0, 0, 0, 0, id };
		AddArt(twin, &area, &ground);
		++art.groundArts;
	}

	VOID OnRelease(StateBuffer* buffer)
	{
		Timed timed("OnRelease");
		DrainSync();

		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
			Release(twin, id);
	}

	const CHAR* TwinKind(const Record* twin)
	{
		return !twin ? "none" : twin->anonymous ? "anonymous" : twin->memory ? "memory" : "surface";
	}

	VOID TraceBegin(const CHAR* name, DWORD mode, BOOL mirror)
	{
		MemoryZero(&trace, sizeof(trace));
		trace.name = name;
		trace.mode = mode;
		trace.mirror = mirror;
		trace.outcome = "not reached";
		art.reason = "no art lookup";
		art.drew = FALSE;
	}

	VOID TraceEnd()
	{
		if (index.traceLogs >= TRACE_LOGS || !((trace.source && trace.source->memory) || (trace.target && trace.target->memory)))
			return;

		++index.traceLogs;
		DebugLog("twin trace %s mode %u%s src %u %s %d,%d %dx%d dst %u %s %d,%d %dx%d | %s | art %s (%s) at frame %u", trace.name, trace.mode, trace.mirror ? " mirror" : "",
			trace.srcId, TwinKind(trace.source), trace.from.left, trace.from.top, trace.from.right - trace.from.left, trace.from.bottom - trace.from.top,
			trace.dstId, TwinKind(trace.target), trace.to.left, trace.to.top, trace.to.right - trace.to.left, trace.to.bottom - trace.to.top,
			trace.outcome, art.drew ? "drawn" : "none", art.reason, game.frameNo);
	}

	VOID CopyTwins(Record* target, DWORD dstId, LONG x, LONG y, Record* source, DWORD srcId, const RECT* srcRect, BOOL keyed, DWORD key);

	VOID OnCopy(StateBuffer* dst, LONG x, LONG y, StateBuffer* src, const RECT* srcRect, BOOL keyed, DWORD key)
	{
		Timed timed("OnCopy");
		if (!Active() || !srcRect)
			return;

		Sync();

		DWORD dstId;
		LONG dox = 0, doy = 0;
		Record* target = Find(dst, &dstId);
		if (!target)
			target = FindByData(dst, &dstId, &dox, &doy);
		if (!target)
			return;

		DWORD srcId;
		LONG sox = 0, soy = 0;
		Record* source = Find(src, &srcId);
		if (!source)
			source = FindByData(src, &srcId, &sox, &soy);

		RECT rect = *srcRect;
		OffsetRect(&rect, sox, soy);
		TraceBegin("blt", keyed, FALSE);
		trace.source = source;
		trace.srcId = srcId;
		trace.from = rect;
		trace.target = target;
		trace.dstId = dstId;
		SetRect(&trace.to, x + dox, y + doy, x + dox + rect.right - rect.left, y + doy + rect.bottom - rect.top);
		trace.outcome = source ? "recorded" : "src untwinned, dst marked dirty";
		CopyTwins(target, dstId, x + dox, y + doy, source, srcId, &rect, keyed, key);
		TraceEnd();
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

		// a ground block still being built must get its 4K command recorded before anything copies out of it
		ImageReplace::GroundFlush((const DWORD*)source->base + from.top * source->pitch + from.left);

		FlushRegion(source, srcId, &from);
		Verify(source, srcId, &from, NULL, 0, FALSE);

		FlushRegion(target, dstId, &to, TRUE);
		ArtPlan plan;
		BOOL hasArt = IdentifyArt(target, dstId, &to, source, srcId, &from, x, y, srcRect, keyed, key, FALSE, FALSE, &plan);
		if (hasArt && keyed)
		{
			if (RecordArt(target, dstId, &to, srcId, &from, plan.frame, plan.flipped, &plan.place, keyed, key, plan.cached, FALSE))
			{
				MirrorCopy(target, &to, source, &from, keyed, key);
				if (target != source)
					DropRegions(target, &to);
				AddArt(target, &to, &art.lastTag);
				return;
			}
			hasArt = FALSE;
		}

		if (!keyed)
			Cover(target, &to);
		FlushRegion(target, dstId, &to, TRUE);

		RECT arts[ART_MAX];
		ArtTag tags[ART_MAX];
		DWORD artCount = 0;
		for (DWORD i = 0; i < source->artCount; ++i)
			if (IntersectRect(&arts[artCount], &source->art[i], &from))
			{
				tags[artCount] = source->artTag[i];
				tags[artCount].cx += arts[artCount].left - source->art[i].left;
				tags[artCount].cy += arts[artCount].top - source->art[i].top;
				OffsetRect(&arts[artCount++], ox, oy);
			}
		if (!keyed)
			CoverArt(target, &to);
		DropRegions(target, &to);
		for (DWORD i = 0; i < artCount; ++i)
			AddArt(target, &arts[i], &tags[i]);

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

		// an unkeyed copy replaces the whole rect, so the 1x copy goes first and the art lands on top; transparent art then shows the source, not stale destination
		if (hasArt && RecordArt(target, dstId, &to, srcId, &from, plan.frame, plan.flipped, &plan.place, keyed, key, plan.cached, FALSE))
			AddArt(target, &to, &art.lastTag);

		if (!EqualRect(&from, srcRect))
		{
			if (keyed && index.clipLogs < 16)
			{
				++index.clipLogs;
				DebugLog("twin clipped keyed copy src %u %s %d,%d %dx%d (asked %d,%d %dx%d) -> dst %u %d,%d key %08X src key %s %08X art %s (%s) at frame %u", srcId, TwinKind(source),
					from.left, from.top, from.right - from.left, from.bottom - from.top, srcRect->left, srcRect->top, srcRect->right - srcRect->left, srcRect->bottom - srcRect->top,
					dstId, to.left, to.top, key, source->keyed ? "learned" : "unknown", source->key, art.drew ? "drawn" : "none", art.reason, game.frameNo);
			}
			MarkDirty(target, whole);
		}
	}

	VOID FillTwin(Record* twin, DWORD id, const RECT* rect, DWORD color);

	VOID OnFill(StateBuffer* dst, const RECT* rect, DWORD color)
	{
		Timed timed("OnFill");
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

	VOID FillTwin(Record* twin, DWORD id, const RECT* rect, DWORD color)
	{
		RECT area = *rect;
		if (!Clip(twin, &area))
			return;

		Cover(twin, &area);
		FlushRegion(twin, id, &area, TRUE);
		CoverArt(twin, &area);
		DropRegions(twin, &area);
		MirrorFill(twin, &area, color);

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
				++game.fillMerged;
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

		++game.list->counters.fills;
	}

	VOID OnWrite(const VOID* first, LONG pitch, LONG width, LONG height)
	{
		Timed timed("OnWrite");
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
		Timed timed("OnWriteAll");
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

	VOID PixelCopyInner(PixelMode mode, const DWORD* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height, DWORD key, DWORD param);

	VOID OnPixelCopy(PixelMode mode, const DWORD* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height, DWORD key, DWORD param)
	{
		Timed timed("OnPixelCopy");
		if (!dst || !src || width <= 0 || height <= 0 || (DWORD)mode >= PIXEL_MODES || !Active())
			return;

		ImageReplace::GroundFlush(src);
		TraceBegin("pixel", mode, (param & PixelMirror) != 0);
		PixelCopyInner(mode, src, srcPitch, dst, dstPitch, width, height, key, param);
		TraceEnd();
	}

	VOID PixelCopyInner(PixelMode mode, const DWORD* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height, DWORD key, DWORD param)
	{
		Sync(FALSE);

		DWORD dstId;
		Record* target = Adopt(dst, &dstId);
		trace.target = target;
		trace.dstId = dstId;
		if (!target)
		{
			trace.outcome = "dst untwinned";
			return;
		}

		++game.pixelOps[mode];

		LONG dx, dy;
		if (!PixelPos(target, dst, dstPitch, width, height, &dx, &dy))
		{
			trace.outcome = dstPitch != target->pitch ? "dst pitch differs from its twin, marked dirty" : "dst rect outside its twin, marked dirty";
			OnWrite(dst, dstPitch, width, height);
			return;
		}
		RECT to = { dx, dy, dx + width, dy + height };
		trace.to = to;

		DWORD srcId;
		Record* source = Adopt(src, &srcId);
		trace.source = source;
		trace.srcId = srcId;
		LONG sx, sy;
		if (!source || !PixelPos(source, src, srcPitch, width, height, &sx, &sy))
		{
			++game.pixelUntwinned;
			trace.outcome = !source ? "src untwinned, dst marked dirty" : srcPitch != source->pitch ? "src pitch differs from its twin, dst marked dirty" : "src rect outside its twin, dst marked dirty";
			if (index.untwinnedLogs < TRACE_LOGS)
			{
				++index.untwinnedLogs;
				const VOID* base = NULL;
				LONG pitch = 0, w = 0, h = 0, frame = -1;
				BOOL flipped = FALSE;
				BOOL known = ImageReplace::FindMemory(src, &base, &pitch, &w, &h, &frame, &flipped);
				DebugLog("twin untwinned src mode %u src=%p pitch %d %dx%d -> dst %u %s | %s | FindMemory %s base=%p pitch %d %dx%d frame %d at frame %u", mode, src, srcPitch, width, height,
					dstId, TwinKind(target), trace.outcome, known ? "knows it" : "doesn't know it", base, pitch, w, h, frame, game.frameNo);
			}
			MarkDirty(target, to);
			return;
		}
		RECT from = { sx, sy, sx + width, sy + height };
		trace.from = from;
		trace.outcome = "recorded";
		DrainSyncIn(source, &from);

		BOOL mirror = (param & PixelMirror) != 0;
		if ((mode == PixelAverage && (param & 0xFF) >= 255) || mode == PixelKeyedCopy || (mode == PixelCopy && !mirror))
		{
			CopyTwins(target, dstId, dx, dy, source, srcId, &from, mode != PixelCopy, key);
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
		DropRegions(target, &to);

		if (mode == PixelKeyed && (param & 0xFF) >= 255)
		{
			source->keyed = TRUE;
			source->key = key | ALPHA_COMPONENT;
			if (IdentifyArt(target, dstId, &to, source, srcId, &from, dx, dy, &from, TRUE, key, TRUE, FALSE))
			{
				++game.pixelArt;
				AddArt(target, &to, &art.lastTag);
				DeferSync(target, dstId, &to);
				return;
			}
		}

		ArtPlan plan;
		BOOL hasArt = mode == PixelCopy && IdentifyArt(target, dstId, &to, source, srcId, &from, dx, dy, &from, FALSE, 0, FALSE, TRUE, &plan);

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

		if (mode != PixelAdd && mode != PixelSub && mode != PixelBlend)
		{
			RECT part;
			for (DWORD i = 0; i < source->artCount; ++i)
				if (IntersectRect(&part, &source->art[i], &from))
				{
					ArtTag tag = source->artTag[i];
					tag.cx += part.left - source->art[i].left;
					tag.cy += part.top - source->art[i].top;
					OffsetRect(&part, dx - sx, dy - sy);
					AddArt(target, &part, &tag);
				}

		}

		// the smooth half coverage and rounded blends differ from the 1x dither and floor on purpose, so the GPU check treats them like art
		if (mode != PixelAdd && mode != PixelSub && mode != PixelCopy)
			AddArt(target, &to);

		if (hasArt && RecordArt(target, dstId, &to, srcId, &from, plan.frame, plan.flipped, &plan.place, FALSE, 0, plan.cached, FALSE))
		{
			++game.pixelArt;
			AddArt(target, &to, &art.lastTag);
		}

		DeferSync(target, dstId, &to);
	}

	Record* PixelTarget(const VOID* first, LONG pitch, LONG width, LONG height, DWORD* id, RECT* rect)
	{
		Record* twin = FindAddress(first, id);
		if (!twin)
			return NULL;

		LONG x, y;
		if (!PixelPos(twin, first, pitch, width, height, &x, &y))
		{
			++game.pixelFallbacks;
			OnWrite(first, pitch, width, height);
			return NULL;
		}

		SetRect(rect, x, y, x + width, y + height);
		return twin;
	}

	VOID OnFillRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color)
	{
		Timed timed("OnFillRect");
		if (!first || width <= 0 || height <= 0 || !Active())
			return;

		Sync();

		DWORD id;
		RECT rect;
		Record* twin = PixelTarget(first, pitch, width, height, &id, &rect);
		if (!twin)
			return;

		++game.fillRects;
		FillTwin(twin, id, &rect, color);
	}

	VOID OnTintRect(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD color, DWORD weight)
	{
		Timed timed("OnTintRect");
		if (!first || width <= 0 || height <= 0 || !weight || !Active())
			return;

		Sync(FALSE);

		DWORD id;
		RECT rect;
		Record* twin = PixelTarget(first, pitch, width, height, &id, &rect);
		if (!twin)
			return;

		FlushRegion(twin, id, &rect, TRUE);
		DropRegions(twin, &rect);

		Cmd* cmd = Emit(CmdTint);
		if (!cmd)
		{
			MarkDirty(twin, rect);
			return;
		}
		cmd->dst = id;
		cmd->rect = rect;
		cmd->value = color;
		cmd->extra = weight;

		++game.tints;
		AddArt(twin, &rect);
		DeferSync(twin, id, &rect);
	}

	VOID OnBlendRow(const DWORD* src, const DWORD* dst, LONG length, const BYTE* mask, BOOL keyed, DWORD key)
	{
		Timed timed("OnBlendRow");
		if (!src || !dst || !mask || length <= 0 || !Active())
			return;

		ImageReplace::GroundFlush(src);
		Sync(FALSE);

		DWORD dstId;
		Record* target = Adopt(dst, &dstId);
		if (!target)
			return;

		LONG dx, dy;
		if (!PixelPos(target, dst, target->pitch, length, 1, &dx, &dy))
		{
			++game.pixelFallbacks;
			OnWrite(dst, target->pitch, length, 1);
			return;
		}
		RECT to = { dx, dy, dx + length, dy + 1 };

		DWORD srcId;
		Record* source = Adopt(src, &srcId);
		LONG sx, sy;
		if (!source || source == target || !PixelPos(source, src, source->pitch, length, 1, &sx, &sy))
		{
			++game.pixelUntwinned;
			MarkDirty(target, to);
			return;
		}
		RECT from = { sx, sy, sx + length, sy + 1 };

		++game.blendRows;
		DrainSyncIn(source, &from);
		FlushRegion(source, srcId, &from);
		Verify(source, srcId, &from, NULL, 0, FALSE);
		FlushRegion(target, dstId, &to, TRUE);
		DropRegions(target, &to);

		List* list = game.list;
		if (list && list->count)
		{
			Cmd* last = &list->cmds[list->count - 1];
			LONG lastH = last->rect.bottom - last->rect.top;
			if (last->type == CmdMask && last->dst == dstId && last->src == srcId && last->flags == (DWORD)(keyed ? 1 : 0) && (!keyed || last->value == key) &&
				last->x == dx && last->y + lastH == dy && last->rect.left == sx && last->rect.right == sx + length && last->rect.bottom == sy &&
				lastH < MASK_SIZE && length <= MASK_SIZE && last->extra + (DWORD)(length * lastH) == list->size)
			{
				DWORD offset;
				BYTE* bytes = Reserve(length, &offset);
				if (bytes)
				{
					MemoryCopy(bytes, mask, length);
					++last->rect.bottom;
					++game.blendMerged;
					AddArt(target, &to);
					DeferSync(target, dstId, &to);
					return;
				}
			}
		}

		DWORD offset;
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

		AddArt(target, &to);
		DeferSync(target, dstId, &to);
	}

	VOID OnLighter(const DWORD* first, LONG pitch, LONG width, LONG height, DWORD key, BOOL lighten)
	{
		Timed timed("OnLighter");
		if (!first || width <= 0 || height <= 0 || !Active())
			return;

		Sync(FALSE);

		DWORD id;
		RECT rect;
		Record* twin = PixelTarget(first, pitch, width, height, &id, &rect);
		if (!twin)
			return;

		FlushRegion(twin, id, &rect, TRUE);
		DropRegions(twin, &rect);

		Cmd* cmd = Emit(CmdLighter);
		if (!cmd)
		{
			MarkDirty(twin, rect);
			return;
		}
		cmd->dst = id;
		cmd->rect = rect;
		cmd->value = key;
		cmd->flags = lighten ? 1 : 0;

		++game.lighters;
		DeferSync(twin, id, &rect);
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
			while (slot && (!top[slot - 1] || top[slot - 1]->repairs < twin->repairs))
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
		DWORD artCalls = shared.artCalls;
		DWORD artMasked = shared.artMasked;
		shared.artMasked = 0;
		DWORD groundReplayed = shared.groundReplayed;
		DWORD groundFallbacks = shared.groundFallbacks;
		DWORD pixelReplayed = shared.pixelReplayed;
		DWORD pixelCalls = shared.pixelCalls;
		shared.pixelReplayed = shared.pixelCalls = 0;
		shared.artDraws = shared.artFallbacks = shared.artCalls = shared.groundReplayed = shared.groundFallbacks = 0;
		CheckResult check = shared.check;
		DWORD showOpaque = shared.showOpaque, showBack = shared.showBack, showInvalid = shared.showInvalid, showGone = shared.showGone, showOffCount = shared.showOff, showOther = shared.showOther;
		DWORD showUs = shared.showSamples ? shared.showUs / shared.showSamples : 0, showMaxUs = shared.showMaxUs, showSamples = shared.showSamples;
		shared.showOpaque = shared.showBack = shared.showInvalid = shared.showGone = shared.showOff = shared.showOther = 0;
		shared.showUs = shared.showSamples = shared.showMaxUs = 0;
		Unlock();

		DebugLog("twin art draws %u px %u notready %u | region hits %u hash lookups %u hits %u misses %u learned expect %u source %u conflicts %u rejected %u | index %u entries | expectations %u (dup %u, used %u, overflow %u) | replayed %u calls %u fallbacks %u keyed masked %u | expect rejected %u",
			art.draws, art.px, art.notReady, index.regionHits, index.lookups, index.hits, index.misses, index.learnedExpect, index.learnedSource, index.conflicts, index.rejected, index.count,
			art.expected, art.duplicates, art.matched, art.overflow, artReplayed, artCalls, artFallbacks, artMasked,
			art.expectRejectSize + art.expectRejectSource + art.expectRejectContent);
		// kept on its own line: DebugLog formats into 512 bytes
		DebugLog("twin art expect checks: expect rejected size %u source %u content %u | stale regions dropped %u | expect verified %u unverified %u provenance %u | pieces known at decode %u",
			art.expectRejectSize, art.expectRejectSource, art.expectRejectContent, art.expectStale, art.expectVerified, art.expectUnverified, art.expectProvenance, learn.knownCount);
		art.expectVerified = art.expectUnverified = art.expectProvenance = art.expectRejectSize = art.expectRejectSource = art.expectRejectContent = art.expectStale = 0;
		art.expected = art.duplicates = art.matched = art.draws = art.px = art.notReady = art.overflow = 0;
		index.regionHits = index.lookups = index.hits = index.misses = index.learnedExpect = index.learnedSource = index.conflicts = index.rejected = 0;

		DebugLog("twin index identities: multi-identity entries %u | context draws %u content draws %u | evidence %u | LearnArt calls %u", index.multi, index.contextDraws, index.contentDraws, index.evidence, index.learnCalls);
		index.contextDraws = index.contentDraws = index.evidence = index.learnCalls = 0;

		LARGE_INTEGER qpc;
		QueryPerformanceCounter(&qpc);
		ULONGLONG tsc = __rdtsc();
		DWORD slowUs = 0;
		if (slow.qpc && qpc.QuadPart > slow.qpc && game.frequency.QuadPart)
		{
			ULONGLONG elapsedUs = (ULONGLONG)(qpc.QuadPart - slow.qpc) * 1000000 / game.frequency.QuadPart;
			if (elapsedUs)
				slowUs = DWORD(slow.cycles * elapsedUs / max(tsc - slow.tsc, 1ULL));
		}
		ULONGLONG tscPerUs = 0;
		if (slow.qpc && qpc.QuadPart > slow.qpc && game.frequency.QuadPart)
		{
			ULONGLONG elapsedUs = (ULONGLONG)(qpc.QuadPart - slow.qpc) * 1000000 / game.frequency.QuadPart;
			if (elapsedUs)
				tscPerUs = (tsc - slow.tsc) / elapsedUs;
		}
		DWORD expectAvgUs = tscPerUs && seen.frames ? DWORD(seen.sumCycles / seen.frames / tscPerUs) : 0;
		DWORD expectMaxUs = tscPerUs ? DWORD(seen.maxCycles / tscPerUs) : 0;
		DebugLog("twin slowest game-thread call %s %u us | ExpectArt per frame avg %u us max %u us | slow hashes %u", slow.name ? slow.name : "none", slowUs, expectAvgUs, expectMaxUs, learn.slowHashes);
		seen.sumCycles = seen.maxCycles = 0;
		seen.frames = 0;
		slow.cycles = 0;
		slow.name = NULL;
		slow.tsc = tsc;
		slow.qpc = qpc.QuadPart;
		learn.skipped = learn.slowHashes = 0;

		DWORD memoryMb10 = DWORD(memory.bytes * 10 / (1024 * 1024));
		DebugLog("twin memory twins %u (%u.%u MB, created %u, anonymous %u, replaced %u, evicted %u, refused %u) | addr fallback hits %u | art from memory twins %u",
			memory.count, memoryMb10 / 10, memoryMb10 % 10, memory.created, memory.anonymous, memory.replaced, memory.evictions, memory.refused, memory.addrHits, memory.artDraws);
		memory.created = memory.anonymous = memory.replaced = memory.evictions = memory.refused = memory.addrHits = memory.artDraws = 0;

		DebugLog("twin pixel ops keyed %u average %u add %u sub %u empty %u copy %u blend %u keyedcopy %u tint %u | fill rects %u (merged %u) tint rects %u blend rows %u (merged %u) lighter %u | art %u untwinned src %u self %u fallbacks %u | replayed %u calls %u",
			game.pixelOps[PixelKeyed], game.pixelOps[PixelAverage], game.pixelOps[PixelAdd], game.pixelOps[PixelSub], game.pixelOps[PixelEmpty],
			game.pixelOps[PixelCopy], game.pixelOps[PixelBlend], game.pixelOps[PixelKeyedCopy], game.pixelOps[PixelTint],
			game.fillRects, game.fillMerged, game.tints, game.blendRows, game.blendMerged, game.lighters,
			game.pixelArt, game.pixelUntwinned, game.pixelSelf, game.pixelFallbacks, pixelReplayed, pixelCalls);
		MemoryZero(game.pixelOps, sizeof(game.pixelOps));
		game.pixelArt = game.pixelUntwinned = game.pixelSelf = game.pixelFallbacks = 0;
		game.fillRects = game.fillMerged = game.tints = game.blendRows = game.blendMerged = game.lighters = 0;

		DebugLog("twin ground arts %u replayed %u fallbacks %u no twin %u", art.groundArts, groundReplayed, groundFallbacks, art.groundNoTwin);
		art.cachedDraws = art.groundArts = art.groundNoTwin = 0;

		DebugLog("twin show shown %u (opaque %u, over background %u) hidden: 1x toggle %u, invalid %u (no twin at present %u, twin gone %u), other %u | show gpu avg %u us max %u us (%u samples)",
			showOpaque + showBack, showOpaque, showBack, showOffCount, showInvalid + showGone, showInvalid, showGone, showOther, showUs, showMaxUs, showSamples);
		game.presentNoTwin = 0;

		if (check.valid)
			DebugLog("twin gpu check %ums ago twin %u bad %u of %u non-art tiles, px %u | art tiles %u (differ from 1x %u, grossly %u)", now - check.tick, check.twin, check.bad, check.tiles, check.px, check.art, check.artDiffer, check.artGross);
		else
			DebugLog("twin gpu check none yet");

		game.frames = 0;
		game.queuedMax = 0;
		game.resets = 0;
		MemoryZero(&game.interval, sizeof(Counters));
	}

	VOID OnPresent(StateBuffer* screen, LONG left, LONG top)
	{
		Timed timed("OnPresent");
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
			RECT* arts = (RECT*)Reserve(twin->artCount * (sizeof(RECT) + sizeof(ArtTag)), &artOffset);
			if (arts)
			{
				MemoryCopy(arts, twin->art, twin->artCount * sizeof(RECT));
				MemoryCopy(arts + twin->artCount, twin->artTag, twin->artCount * sizeof(ArtTag));
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

		if (index.dirty && now - index.saveTick >= INDEX_SAVE_PERIOD && !InterlockedCompareExchange(&learn.saving, 1, 0))
		{
			index.saveTick = now;
			HANDLE thread = CreateThread(NULL, 0, SaveWorker, NULL, 0, NULL);
			if (thread)
				CloseHandle(thread);
			else
				InterlockedExchange(&learn.saving, 0);
		}
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
			"uniform int fill;\n"
			"uniform int tinted;\n"
			"uniform int lighter;\n"
			"uniform vec2 weight;\n"
			"uniform vec4 key;\n"
			"uniform vec4 solid;\n"
			"uniform sampler2D tex02;\n"
			"uniform int srcMask;\n"
			"uniform vec4 srcMap;\n"
			"uniform vec4 srcKey;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"bool isKey(vec4 c)\n"
			"{\n"
			"	return c.a < 0.5 / 255.0 || (c.a > 254.5 / 255.0 && all(lessThan(abs(c.rgb - key.rgb), vec3(0.5 / 255.0))));\n"
			"}\n"
			"bool srcOpaque(ivec2 p, int s)\n"
			"{\n"
			"	ivec2 size = textureSize(tex02, 0) / s;\n"
			"	vec4 c = texelFetch(tex02, clamp(p, ivec2(0), size - ivec2(1)) * s + ivec2(s / 2), 0);\n"
			"	return !(c.a < 0.5 / 255.0 || (c.a > 254.5 / 255.0 && all(lessThan(abs(c.rgb - srcKey.rgb), vec3(0.5 / 255.0)))));\n"
			"}\n"
			"void main()\n"
			"{\n"
			"	if (fill != 0)\n"
			"	{\n"
			"		FRAG_COLOR = solid;\n"
			"		return;\n"
			"	}\n"
			"	if (art != 0 && (fTex.x < 0.0 || fTex.y < 0.0 || fTex.x > 1.0 || fTex.y > 1.0))\n"
			"		discard;\n"
			"	if (srcMask != 0)\n"
			"	{\n"
			"		int s = int(srcMap.w);\n"
			"		ivec2 i = ivec2(floor(gl_FragCoord.xy / srcMap.z + srcMap.xy - vec2(0.5)));\n"
			"		if (!srcOpaque(i, s) && !srcOpaque(i + ivec2(1, 0), s) && !srcOpaque(i + ivec2(0, 1), s) && !srcOpaque(i + ivec2(1, 1), s))\n"
			"			discard;\n"
			"	}\n"
			"	vec4 c = texture(tex01, fTex);\n"
			"	if (art != 0 && tint != 0)\n"
			"		c.rgb *= vec3(1.0, 0.15, 1.0);\n"
			"	if (keyed != 0)\n"
			"	{\n"
			"		if (isKey(c))\n"
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
			"	if (tinted != 0)\n"
			"		c = vec4(mix(c.rgb, solid.rgb, 0.5), 1.0);\n"
			"	if (lighter != 0)\n"
			"	{\n"
			"		if (isKey(c))\n"
			"			c = vec4(0.0);\n"
			"		else if (lighter == 2)\n"
			"			c = (c.r + c.g + c.b) * 255.0 > 56.5 ? vec4(min(c.rgb * 2.0, vec3(1.0)), 1.0) : vec4(0.0);\n"
			"	}\n"
			"	FRAG_COLOR = c * weight.x;\n"
			"}\n";

		// dst copy on unit 2 for the destination key test; the 1x mask on unit 1 is sampled linearly so 4K edges are smooth
		const CHAR* maskFragment =
			"uniform sampler2D tex01;\n"
			"uniform sampler2D tex02;\n"
			"uniform sampler2D tex03;\n"
			"uniform int keyed;\n"
			"uniform vec4 key;\n"
			"uniform vec4 frame;\n"
			"uniform vec4 extent;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	vec2 local = (gl_FragCoord.xy - frame.xy) / frame.zw;\n"
			"	float m = texture(tex02, clamp(local * extent.xy, vec2(0.5 / 2048.0), extent.xy - vec2(0.5 / 2048.0))).r * 255.0;\n"
			"	if (m < 0.5)\n"
			"		discard;\n"
			"	if (keyed != 0)\n"
			"	{\n"
			"		vec4 d = texture(tex03, local * extent.zw);\n"
			"		if (d.a < 0.5 / 255.0 || (d.a > 254.5 / 255.0 && all(lessThan(abs(d.rgb - key.rgb), vec3(0.5 / 255.0)))))\n"
			"			discard;\n"
			"	}\n"
			"	vec4 c = texture(tex01, fTex);\n"
			"	FRAG_COLOR = c * (m > 254.5 ? 1.0 : (m + 1.0) / 256.0);\n"
			"}\n";

		const CHAR* checkFragment =
			"uniform sampler2D tex01;\n"
			"uniform int scale;\n"
			"uniform int box;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	ivec2 base = ivec2(gl_FragCoord.xy) * scale;\n"
			"	if (box == 0)\n"
			"	{\n"
			"		FRAG_COLOR = texelFetch(tex01, base + ivec2(scale / 2), 0);\n"
			"		return;\n"
			"	}\n"
			"	vec4 sum = vec4(0.0);\n"
			"	for (int y = 0; y < scale; ++y)\n"
			"		for (int x = 0; x < scale; ++x)\n"
			"			sum += texelFetch(tex01, base + ivec2(x, y), 0);\n"
			"	FRAG_COLOR = sum / float(scale * scale);\n"
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
			gl.fillLoc = GLGetUniformLocation(gl.copyProgram, "fill");
			gl.tintedLoc = GLGetUniformLocation(gl.copyProgram, "tinted");
			gl.lighterLoc = GLGetUniformLocation(gl.copyProgram, "lighter");
			gl.solidLoc = GLGetUniformLocation(gl.copyProgram, "solid");
			GLUniform1i(GLGetUniformLocation(gl.copyProgram, "tex02"), 1);
			gl.srcMaskLoc = GLGetUniformLocation(gl.copyProgram, "srcMask");
			gl.srcMapLoc = GLGetUniformLocation(gl.copyProgram, "srcMap");
			gl.srcKeyLoc = GLGetUniformLocation(gl.copyProgram, "srcKey");
			GLUniform1i(gl.srcMaskLoc, 0);
			GLUniform1i(gl.fillLoc, 0);
			GLUniform1i(gl.tintedLoc, 0);
			GLUniform1i(gl.lighterLoc, 0);
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

		gl.maskProgram = Link(vertex, maskFragment);
		if (gl.maskProgram)
		{
			GLUseProgram(gl.maskProgram);
			GLUniform1i(GLGetUniformLocation(gl.maskProgram, "tex01"), 0);
			GLUniform1i(GLGetUniformLocation(gl.maskProgram, "tex02"), 1);
			GLUniform1i(GLGetUniformLocation(gl.maskProgram, "tex03"), 2);
			gl.maskKeyedLoc = GLGetUniformLocation(gl.maskProgram, "keyed");
			gl.maskKeyLoc = GLGetUniformLocation(gl.maskProgram, "key");
			gl.maskFrameLoc = GLGetUniformLocation(gl.maskProgram, "frame");
			gl.maskExtentLoc = GLGetUniformLocation(gl.maskProgram, "extent");
		}

		gl.checkProgram = Link(vertex, checkFragment);
		if (gl.checkProgram)
		{
			GLUseProgram(gl.checkProgram);
			GLUniform1i(GLGetUniformLocation(gl.checkProgram, "tex01"), 0);
			gl.scaleLoc = GLGetUniformLocation(gl.checkProgram, "scale");
			gl.boxLoc = GLGetUniformLocation(gl.checkProgram, "box");
			GLUniform1i(gl.boxLoc, 0);
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

		gl.ready = gl.copyProgram && gl.checkProgram && gl.showProgram && gl.maskProgram && gl.vao && gl.vbo && gl.stage;
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

	VOID KeyUniform(GLint location, DWORD key);

	DWORD ReplayArt(const List* list, DWORD index)
	{
		const Cmd* first = &list->cmds[index];
		DWORD end = index + 1;
		while (end < list->count && list->cmds[end].type == CmdArt && list->cmds[end].dst == first->dst && list->cmds[end].value == first->value && (list->cmds[end].flags & 4) == (first->flags & 4))
			++end;

		GLuint texture = 0;
		LONG width = 0, height = 0;
		BOOL ready = Valid(first->dst) && ImageReplace::ArtTexture((LONG)first->value, &texture, &width, &height) && texture;
		if (!ready)
		{
			for (DWORD i = index; i < end; ++i)
				ArtFallback(&list->cmds[i]);
			return end;
		}

		Gpu* gpu = &gl.twins[first->dst];
		LONG scale = gpu->scale;
		FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);

		TargetTwin(first->dst);
		UseCopy(FALSE, 0);
		GLUniform1i(gl.artLoc, 1);
		GLUniform1i(gl.tintLoc, shared.tint ? 1 : 0);
		if (first->flags & 4)
			GLUniform2f(gl.weightLoc, 0.5f, 0.0f);
		GLBindTexture(GL_TEXTURE_2D, texture);
		Blend(TRUE);

		gl.vertCount = 0;
		DWORD drawn = 0;
		BOOL masking = FALSE, masked = FALSE;
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

			// the 1x keyed copy leaves key pixels untouched, so the 4K piece may only cover the source's opaque pixels plus a 1x pixel of smooth edge
			BOOL mask = (cmd->flags & 2) && cmd->src != cmd->dst && Valid(cmd->src);
			if (mask || masking)
				DrawVerts();
			if (mask)
			{
				Gpu* src = &gl.twins[cmd->src];
				GLActiveTexture(GL_TEXTURE1);
				GLBindTexture(GL_TEXTURE_2D, src->texture);
				GLActiveTexture(GL_TEXTURE0);
				GLUniform1i(gl.srcMaskLoc, 1);
				GLUniform4f(gl.srcMapLoc, (FLOAT)(cmd->clip.left - cmd->rect.left), (FLOAT)(cmd->clip.top - cmd->rect.top), (FLOAT)scale, (FLOAT)src->scale);
				KeyUniform(gl.srcKeyLoc, cmd->extra);
				++gl.artMasked;
				masked = TRUE;
			}
			else if (masking)
				GLUniform1i(gl.srcMaskLoc, 0);
			masking = mask;

			AddQuad(tw, th, (FLOAT)(cmd->rect.left * scale), (FLOAT)(cmd->rect.top * scale), (FLOAT)(cmd->rect.right * scale), (FLOAT)(cmd->rect.bottom * scale), u0, v0, u1, v1);
			++drawn;
		}
		DrawVerts();

		if (masked)
		{
			GLUniform1i(gl.srcMaskLoc, 0);
			GLActiveTexture(GL_TEXTURE1);
			GLBindTexture(GL_TEXTURE_2D, 0);
			GLActiveTexture(GL_TEXTURE0);
		}
		Blend(FALSE);
		GLUniform1i(gl.artLoc, 0);
		GLUniform1i(gl.tintLoc, 0);
		if (first->flags & 4)
			GLUniform2f(gl.weightLoc, 1.0f, 0.0f);
		gl.artDraws += drawn;
		++gl.artCalls;
		gl.vertCount = 0;

		return end;
	}

	// mean abs diff and tile mean colour of the box-downscaled 4K against 1x, over the tile's art pixels; a right 4K piece stays close, a wrong one or a hole does not
	BOOL ArtTileGross(const DWORD* ref, LONG width, LONG x0, LONG y0, LONG x1, LONG y1, DWORD* mad, DWORD* mean, DWORD* cover)
	{
		LONG sum1[3] = { 0, 0, 0 }, sum4[3] = { 0, 0, 0 };
		DWORD diff = 0, px = 0, holes = 0;
		for (LONG y = y0; y < y1; ++y)
		{
			const DWORD* a = ref + y * width;
			const DWORD* b = gl.checkBox + y * width;
			const BYTE* m = gl.checkMask + y * width;
			for (LONG x = x0; x < x1; ++x)
			{
				if (!m[x])
					continue;
				++px;
				for (DWORD c = 0; c < 3; ++c)
				{
					LONG ca = (a[x] >> (c * 8)) & 0xFF, cb = (b[x] >> (c * 8)) & 0xFF;
					sum1[c] += ca;
					sum4[c] += cb;
					diff += ca > cb ? ca - cb : cb - ca;
				}
				if ((b[x] >> 24) < 128)
					++holes;
			}
		}
		if (!px)
			return FALSE;

		*mad = diff / (3 * px);
		DWORD worst = 0;
		for (DWORD c = 0; c < 3; ++c)
		{
			DWORD d = DWORD(sum1[c] > sum4[c] ? sum1[c] - sum4[c] : sum4[c] - sum1[c]) / px;
			if (d > worst)
				worst = d;
		}
		*mean = worst;
		*cover = holes * 100 / px;
		return *mad >= ART_CHECK_MAD || *mean >= ART_CHECK_MEAN || *cover >= 25;
	}

	LONG ArtAt(const RECT* arts, DWORD count, LONG x, LONG y)
	{
		LONG best = -1;
		LONG bestArea = 0;
		for (DWORD k = 0; k < count; ++k)
		{
			const RECT* r = &arts[k];
			if (x < r->left || x >= r->right || y < r->top || y >= r->bottom)
				continue;
			LONG area = Area(r);
			if (best < 0 || area < bestArea)
			{
				best = (LONG)k;
				bestArea = area;
			}
		}
		return best;
	}

	VOID TagText(CHAR* text, const ArtTag* tag)
	{
		if (!tag)
			StrPrint(text, "%s", "no art rect");
		else if (tag->frame == -3)
			StrPrint(text, "ground twin %u", tag->src);
		else if (tag->frame < 0)
			StrPrint(text, "%s", "effect or merged art");
		else
			StrPrint(text, "frame=%d canvas %d,%d of %dx%d src twin %u", tag->frame, tag->cx, tag->cy, tag->canvasW, tag->canvasH, tag->src);
	}

	VOID LogArtCheck(DWORD dst, const RECT* arts, const ArtTag* tags, DWORD checked, DWORD grossCount, const ArtGross* gross)
	{
		if (!grossCount)
		{
			if (checked && gl.artCheckOk < ART_CHECK_LOGS / 8)
			{
				++gl.artCheckOk;
				DebugLog("twin art check twin %u: %u art tiles compared with 4K box-downscaled, none grossly off", dst, checked);
			}
			return;
		}

		// three at most: DebugLog formats into 512 bytes
		CHAR text[3 * 160 + 1];
		CHAR* ptr = text;
		*ptr = 0;
		for (DWORD k = 0; k < 3 && gross[k].px; ++k)
		{
			CHAR tag[96];
			TagText(tag, gross[k].art >= 0 ? &tags[gross[k].art] : NULL);
			ptr += StrPrint(ptr, " (%d,%d mad %u mean %u holes %u%% %s)", gross[k].x, gross[k].y, gross[k].mad, gross[k].mean, gross[k].cover, tag);
		}
		if (gl.artCheckLogs < ART_CHECK_LOGS)
		{
			++gl.artCheckLogs;
			DebugLog("twin art check twin %u: %u of %u art tiles grossly off 1x (mad>=%u mean>=%u holes>=25%%) worst%s", dst, grossCount, checked, ART_CHECK_MAD, ART_CHECK_MEAN, text);
		}

		// one line per identity, so a wrong piece is named once however many tiles or checks it spoils
		for (DWORD k = 0; k < WORST && gross[k].px; ++k)
		{
			if (gross[k].art < 0)
				continue;
			const ArtTag* tag = &tags[gross[k].art];
			BOOL seen = FALSE;
			for (DWORD i = 0; i < sizeof(gl.artBadFrames) / sizeof(*gl.artBadFrames) && !seen; ++i)
				seen = gl.artBadFrames[i] == tag->frame + 1;
			if (seen)
				continue;
			gl.artBadFrames[gl.artBadNext++ % (sizeof(gl.artBadFrames) / sizeof(*gl.artBadFrames))] = tag->frame + 1;

			const RECT* r = &arts[gross[k].art];
			CHAR name[96];
			TagText(name, tag);
			DebugLog("twin art check bad identity %s | art rect %d,%d %dx%d on twin %u | tile %d,%d mad %u mean %u holes %u%%", name, r->left, r->top, r->right - r->left, r->bottom - r->top,
				dst, gross[k].x, gross[k].y, gross[k].mad, gross[k].mean, gross[k].cover);
		}
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
			if (gl.checkBox)
				MemoryFree(gl.checkBox);
			if (gl.checkMask)
				MemoryFree(gl.checkMask);
			gl.checkData = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
			gl.checkBox = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
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

		const RECT* arts = (const RECT*)(list->arena + cmd->src);
		const ArtTag* tags = (const ArtTag*)(arts + cmd->extra);
		DWORD artCount = cmd->extra;

		// the centre texel is exact for 1x content; art needs the whole footprint averaged to be comparable with 1x at all
		BOOL boxed = artCount && gl.checkBox;
		if (boxed)
		{
			GLUniform1i(gl.boxLoc, 1);
			AddQuad(1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
			DrawVerts();
			GLUniform1i(gl.boxLoc, 0);
			GLReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, gl.checkBox);
		}
		GLBindFramebuffer(GL_READ_FRAMEBUFFER, gl.readFbo);

		const DWORD* ref = (const DWORD*)(list->arena + cmd->value);
		LONG tilesX = (width + TILE - 1) / TILE;
		LONG tilesY = (height + TILE - 1) / TILE;

		struct { LONG x, y; DWORD count; } worst[WORST] = {};
		ArtGross gross[WORST];
		MemoryZero(gross, sizeof(gross));
		DWORD badTiles = 0, badPixels = 0, artTiles = 0, artBad = 0, artGross = 0, artChecked = 0;

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

				if (boxed && artDiffer && artCountPx * 2 >= DWORD((x1 - x0) * (y1 - y0)))
				{
					++artChecked;
					DWORD mad, mean, cover;
					if (ArtTileGross(ref, width, x0, y0, x1, y1, &mad, &mean, &cover))
					{
						++artGross;
						DWORD score = mad + mean + cover;
						DWORD slot = WORST;
						while (slot && gross[slot - 1].mad + gross[slot - 1].mean + gross[slot - 1].cover < score)
							--slot;
						if (slot < WORST)
						{
							for (DWORD k = WORST - 1; k > slot; --k)
								gross[k] = gross[k - 1];
							gross[slot].x = x0;
							gross[slot].y = y0;
							gross[slot].mad = mad;
							gross[slot].mean = mean;
							gross[slot].cover = cover;
							gross[slot].px = artCountPx;
							gross[slot].art = ArtAt(arts, artCount, x0 + (x1 - x0) / 2, y0 + (y1 - y0) / 2);
						}
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
		if (boxed)
			LogArtCheck(cmd->dst, arts, tags, artChecked, artGross, gross);

		Lock();
		shared.check.valid = TRUE;
		shared.check.tick = GetTickCount();
		shared.check.twin = cmd->dst;
		shared.check.bad = badTiles;
		shared.check.tiles = tilesX * tilesY - artTiles;
		shared.check.px = badPixels;
		shared.check.art = artTiles;
		shared.check.artDiffer = artBad;
		shared.check.artGross = artGross;
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
			FLOAT u0 = (FLOAT)cmd->rect.left / src->width, u1 = (FLOAT)cmd->rect.right / src->width;
			if ((mode == PixelCopy || mode == PixelBlend) && (cmd->extra & PixelMirror))
			{
				FLOAT u = u0;
				u0 = u1;
				u1 = u;
			}
			AddQuad(tw, th, (FLOAT)(cmd->x * scale), (FLOAT)(cmd->y * scale), (FLOAT)((cmd->x + w) * scale), (FLOAT)((cmd->y + h) * scale),
				u0, (FLOAT)cmd->rect.top / src->height, u1, (FLOAT)cmd->rect.bottom / src->height);
		}

		DWORD param = first->extra & 0xFF;
		FLOAT weight = 1.0f;
		BOOL keyed = FALSE, empty = FALSE, tinted = FALSE, blend = TRUE;
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
		case PixelCopy:
			blend = FALSE;
			break;
		case PixelTint:
			keyed = TRUE;
			tinted = TRUE;
			blend = FALSE;
			break;
		case PixelBlend:
			weight = (first->extra & 0xFFFF) / 256.0f;
			break;
		default:
			break;
		}

		TargetTwin(first->dst);
		UseCopy(keyed, first->value);
		if (empty)
			GLUniform1i(gl.emptyLoc, 1);
		if (tinted)
		{
			DWORD color = first->extra;
			GLUniform1i(gl.tintedLoc, 1);
			GLUniform4f(gl.solidLoc, (color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, 1.0f);
		}
		if (weight != 1.0f)
			GLUniform2f(gl.weightLoc, weight, 0.0f);
		GLBindTexture(GL_TEXTURE_2D, src->texture);

		if (blend)
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
		if (tinted)
			GLUniform1i(gl.tintedLoc, 0);
		if (weight != 1.0f)
			GLUniform2f(gl.weightLoc, 1.0f, 0.0f);
		gl.vertCount = 0;
		return end;
	}

	VOID SolidColor(DWORD color, FLOAT alpha, BOOL premultiply)
	{
		FLOAT k = premultiply ? alpha : 1.0f;
		GLUniform4f(gl.solidLoc, (color & 0xFF) / 255.0f * k, ((color >> 8) & 0xFF) / 255.0f * k, ((color >> 16) & 0xFF) / 255.0f * k, alpha);
	}

	DWORD ReplayFills(const List* list, DWORD index, BOOL tint)
	{
		const Cmd* first = &list->cmds[index];
		DWORD end = index + 1;
		while (end < list->count)
		{
			const Cmd* cmd = &list->cmds[end];
			if (cmd->type != first->type || cmd->dst != first->dst || cmd->value != first->value || cmd->flags != first->flags || cmd->extra != first->extra)
				break;
			++end;
		}

		if (!Valid(first->dst))
			return end;

		Gpu* gpu = &gl.twins[first->dst];
		LONG scale = gpu->scale;
		FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);
		gl.vertCount = 0;
		for (DWORD i = index; i < end; ++i)
		{
			const RECT* r = &list->cmds[i].rect;
			AddQuad(tw, th, (FLOAT)(r->left * scale), (FLOAT)(r->top * scale), (FLOAT)(r->right * scale), (FLOAT)(r->bottom * scale), 0.0f, 0.0f, 0.0f, 0.0f);
		}

		TargetTwin(first->dst);
		UseCopy(FALSE, 0);
		GLUniform1i(gl.fillLoc, 1);
		DWORD color = first->value;
		if (tint)
		{
			SolidColor(color, first->extra / 256.0f, TRUE);
			Blend(TRUE);
		}
		else if (first->flags)
			GLUniform4f(gl.solidLoc, 0.0f, 0.0f, 0.0f, 0.0f);
		else
			SolidColor(color, color >= ALPHA_COMPONENT ? 1.0f : 254.0f / 255.0f, FALSE);
		DrawVerts();
		if (tint)
			Blend(FALSE);
		GLUniform1i(gl.fillLoc, 0);
		gl.vertCount = 0;
		return end;
	}

	BOOL ToScratch(Gpu* gpu, const RECT* rect, FLOAT* extentW, FLOAT* extentH)
	{
		LONG scale = gpu->scale;
		LONG width = (rect->right - rect->left) * scale;
		LONG height = (rect->bottom - rect->top) * scale;
		if (!EnsureTarget(&gl.scratch, &gl.scratchFbo, &gl.scratchW, &gl.scratchH, max(gl.scratchW, width), max(gl.scratchH, height)))
			return FALSE;

		Target(TARGET_SCRATCH, gl.scratchFbo, gl.scratchW, gl.scratchH);
		UseCopy(FALSE, 0);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		gl.vertCount = 0;
		AddQuad((FLOAT)gl.scratchW, (FLOAT)gl.scratchH, 0.0f, 0.0f, (FLOAT)width, (FLOAT)height,
			(FLOAT)rect->left / gpu->width, (FLOAT)rect->top / gpu->height, (FLOAT)rect->right / gpu->width, (FLOAT)rect->bottom / gpu->height);
		DrawVerts();

		*extentW = (FLOAT)width / gl.scratchW;
		*extentH = (FLOAT)height / gl.scratchH;
		return TRUE;
	}

	VOID KeyUniform(GLint location, DWORD key)
	{
		GLUniform4f(location, (key & 0xFF) / 255.0f, ((key >> 8) & 0xFF) / 255.0f, ((key >> 16) & 0xFF) / 255.0f, 1.0f);
	}

	VOID ReplayLighter(const Cmd* cmd)
	{
		if (!Valid(cmd->dst))
			return;

		Gpu* gpu = &gl.twins[cmd->dst];
		FLOAT ew, eh;
		if (!ToScratch(gpu, &cmd->rect, &ew, &eh))
			return;

		LONG scale = gpu->scale;
		TargetTwin(cmd->dst);
		UseCopy(FALSE, 0);
		KeyUniform(gl.keyLoc, cmd->value);
		gl.key = cmd->value;
		GLUniform1i(gl.lighterLoc, cmd->flags ? 2 : 1);
		GLBindTexture(GL_TEXTURE_2D, gl.scratch);
		gl.vertCount = 0;
		AddQuad((FLOAT)(gpu->width * scale), (FLOAT)(gpu->height * scale), (FLOAT)(cmd->rect.left * scale), (FLOAT)(cmd->rect.top * scale), (FLOAT)(cmd->rect.right * scale), (FLOAT)(cmd->rect.bottom * scale),
			0.0f, 0.0f, ew, eh);
		DrawVerts();
		GLUniform1i(gl.lighterLoc, 0);
		++gl.pixelCalls;
	}

	VOID ReplayMask(const Cmd* cmd, const List* list)
	{
		if (!Valid(cmd->src) || !Valid(cmd->dst))
			return;

		Gpu* src = &gl.twins[cmd->src];
		Gpu* dst = &gl.twins[cmd->dst];
		LONG scale = dst->scale;
		LONG width = cmd->rect.right - cmd->rect.left;
		LONG height = cmd->rect.bottom - cmd->rect.top;
		RECT to = { cmd->x, cmd->y, cmd->x + width, cmd->y + height };

		FLOAT ew = 0.0f, eh = 0.0f;
		if (cmd->flags && !ToScratch(dst, &to, &ew, &eh))
			return;

		if (!gl.maskTexture)
		{
			GLGenTextures(1, &gl.maskTexture);
			GLActiveTexture(GL_TEXTURE1);
			GLBindTexture(GL_TEXTURE_2D, gl.maskTexture);
			SetNearest(GL_LINEAR);
			GLTexImage2D(GL_TEXTURE_2D, 0, GL_R8, MASK_SIZE, MASK_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		}

		GLActiveTexture(GL_TEXTURE1);
		GLBindTexture(GL_TEXTURE_2D, gl.maskTexture);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, width);
		GLTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, list->arena + cmd->extra);
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		if (cmd->flags)
		{
			GLActiveTexture(GL_TEXTURE2);
			GLBindTexture(GL_TEXTURE_2D, gl.scratch);
		}
		GLActiveTexture(GL_TEXTURE0);
		GLBindTexture(GL_TEXTURE_2D, src->texture);

		TargetTwin(cmd->dst);
		gl.program = gl.maskProgram;
		GLUseProgram(gl.maskProgram);
		GLUniform1i(gl.maskKeyedLoc, cmd->flags ? 1 : 0);
		KeyUniform(gl.maskKeyLoc, cmd->value);
		GLUniform4f(gl.maskFrameLoc, (FLOAT)(to.left * scale), (FLOAT)(to.top * scale), (FLOAT)(width * scale), (FLOAT)(height * scale));
		GLUniform4f(gl.maskExtentLoc, (FLOAT)width / MASK_SIZE, (FLOAT)height / MASK_SIZE, ew, eh);

		gl.vertCount = 0;
		AddQuad((FLOAT)(dst->width * scale), (FLOAT)(dst->height * scale), (FLOAT)(to.left * scale), (FLOAT)(to.top * scale), (FLOAT)(to.right * scale), (FLOAT)(to.bottom * scale),
			(FLOAT)cmd->rect.left / src->width, (FLOAT)cmd->rect.top / src->height, (FLOAT)cmd->rect.right / src->width, (FLOAT)cmd->rect.bottom / src->height);
		Blend(TRUE);
		DrawVerts();
		Blend(FALSE);
		++gl.pixelCalls;
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
			if (shared.tint)
			{
				TargetTwin(cmd->dst);
				UseCopy(FALSE, 0);
				GLUniform1i(gl.fillLoc, 1);
				GLUniform4f(gl.solidLoc, 0.3f, 1.0f, 0.3f, 1.0f);
				GLEnable(GL_BLEND);
				GLBlendFunc(GL_DST_COLOR, GL_ZERO);
				gl.vertCount = 0;
				AddQuad((FLOAT)(gpu->width * scale), (FLOAT)(gpu->height * scale), (FLOAT)(cmd->rect.left * scale), (FLOAT)(cmd->rect.top * scale),
					(FLOAT)(cmd->rect.right * scale), (FLOAT)(cmd->rect.bottom * scale), 0.0f, 0.0f, 0.0f, 0.0f);
				DrawVerts();
				GLDisable(GL_BLEND);
				GLUniform1i(gl.fillLoc, 0);
			}
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
				i = ReplayFills(list, i, FALSE);
				continue;

			case CmdTint:
				i = ReplayFills(list, i, TRUE);
				continue;

			case CmdMask:
				ReplayMask(cmd, list);
				break;

			case CmdLighter:
				ReplayLighter(cmd);
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
			gl.artCalls = 0;
			gl.artMasked = 0;
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
			shared.artCalls += gl.artCalls;
			shared.artMasked += gl.artMasked;
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

	VOID SaveIndex();

	VOID ReleaseGL()
	{
		SaveIndex();

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
			GLDeleteProgram(gl.maskProgram);
			if (gl.maskTexture)
				GLDeleteTextures(1, &gl.maskTexture);
		}

		if (gl.checkData)
			MemoryFree(gl.checkData);
		if (gl.checkBox)
			MemoryFree(gl.checkBox);
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
