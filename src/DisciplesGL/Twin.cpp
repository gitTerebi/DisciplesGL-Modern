#include "StdAfx.h"
#include "intrin.h"
#include "Twin.h"
#include "Config.h"
#include "GLib.h"
#include "ImageReplace.h"
#include "Hooks.h"
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
#define TARGET_CONFLICT 0x10002
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
#define ART_MAX 128
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
#define SEED_RECORDS 1024
#define SEED_PIECES 4096
#define SEED_CONFLICTS 256
#define SEED_LOGS 300
#define XFER_INSIDE 16
#define XFER_WATCH 16
#define XFER_FRAMES 600
#define XFER_UNBOUND_FRAMES 2
#define XFER_LOGS 200
#define XFER_FAIL_LOGS 64
#define XFER_DECODE_LOGS 24
#define XFER_LARGE_LOGS 8
#define XFER_LARGE 4
#define XFER_TOLERANT_PCT 90
#define XFER_TOLERANCE 8
#define XFER_INDEXED_INSIDE 16
#define TRAP_RING 4096
#define TRAP_WRITERS 256
#define TRAP_TOP 20
#define TRAP_PAGE 4096
#define TRAP_REARM_MS 1000
#define TRAP_REARM_MAX_MS 16000
#define TRAP_PAGES_SECOND 2048
#define TRAP_TOTAL 400000
#define TRAP_RETRIES 4
#define TRAP_STACK_SCAN 32
#define TRAP_QUICK 64
#define TRAP_REOPEN 64
#define ARRIVAL_TOP 12
#define TRAP_PRIORITY 8
#define TRAP_PRIORITY_FAULTS 4096
#define TRAP_PRIORITY_PAGES_SECOND 16384
#define TRAP_PRIORITY_LOGS 96
#define TRAP_PAIRS 256
#define TRAP_HINTS 8
#define COPYIN_LOGS 96
#define COPYIN_BIG (400 * 400)
#define WRITE_RING 8
#define INDEXED_OTHER_LOGS 32
#define INDEXED_CHAIN 6
#define UNNAMED_SIZES 32
#define UNNAMED_LOGS 96
#define DECODE_LOGS 240
#define ARTLESS_LOGS 32
#define CODE_LIMIT (64 * 1024)
#define CODE_REGIONS 64
#define SEED_PENDING_FRAMES 3600
#define SEED_WAIT_DEFAULT 0
#define SEED_CONFLICT_CELL 16
#define SEED_CONFLICT_BATCH 16
#define SEED_CONFLICT_DIFF 12
#define SEED_CONFLICT_LOGS 24
#define COVER_TOP 20
#define SEED_POLL_BYTES (24 * 1024 * 1024)
#define CMD_TYPES 14
#define REPLAY_SLOW_US 50000
#define REPLAY_SLOW_LOGS 64
#define SEED_INLINE_MS 4
#define SEED_INLINE_MIN (1024 * 1024)
#define SEED_INLINE_MAX (32 * 1024 * 1024)
#define SEED_LOAD_GAP_MS 100
#define SEED_LOAD_WAIT_DEFAULT 100
#define SEED_LOAD_TOTAL_MS 1000
#define TAG_GROUND -3
#define TAG_SEED_SHEET -4
#define TAG_MERGED_4K -5

typedef VOID(__stdcall* GLBLENDEQUATION)(GLenum mode);

namespace Twin
{
	enum CmdType { CmdCreate, CmdDestroy, CmdCopy, CmdFill, CmdUpload, CmdPresent, CmdReset, CmdArt, CmdGround, CmdPixel, CmdTint, CmdMask, CmdLighter, CmdSeed };

	const CHAR* const cmdNames[CMD_TYPES] = { "create", "destroy", "copy", "fill", "upload", "present", "reset", "art", "ground", "pixel", "tint", "mask", "lighter", "seed" };

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

	enum NoteKind { NoteWrite, NoteIndexed, NoteUntwinned, NoteFill, NoteCopy, NotePixel, NoteTrap, NoteWhole, NoteKinds };

	// one of the last writes into a twin: what kind, where, the game code above it and the source it read
	struct WriteNote {
		DWORD kind;
		RECT rect;
		DWORD caller;
		DWORD src;
		DWORD frameNo;
	};

	struct CopyIn {
		DWORD frameNo;
		DWORD srcId;
		StateBuffer* srcOwner;
		LONG srcW;
		LONG srcH;
		BOOL srcMemory;
		RECT from;
		LONG x;
		LONG y;
		BOOL keyed;
		BOOL is4K;
		DWORD srcArts;
		DWORD caller;
		DWORD outer;
		DWORD srcUploads;
		DWORD srcWrites;
		DWORD srcIn1x;
		DWORD srcInUntwinned;
		DWORD srcTraps;
		WriteNote note;
	};

	// how a twin's content arrived, since it was created; the coverage checklist names the path that fills each 1x-only surface
	struct Arrival {
		DWORD uploads;
		DWORD uploadPx;
		DWORD seedUploads;
		DWORD writes;
		DWORD fills;
		DWORD in4K;
		DWORD in1x;
		DWORD inUntwinned;
		DWORD pixelIn;
		DWORD locks;
		DWORD unlocks;
		DWORD lockCaller;
		DWORD lockOuter;
		RECT lockRect;
		BOOL lockRectSet;
		DWORD lockSeq;
		DWORD transfers;
		DWORD guardFails;
		DWORD xferSeq;
		DWORD failSeq;
		DWORD watchFailSeq;
		DWORD trapWrites;
		DWORD trapEip;
		DWORD inCount;
		CopyIn firstIn;
		CopyIn lastIn;
		WriteNote notes[WRITE_RING];
		DWORD noteHead;
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
		DWORD seedCount;
		DWORD coverPx;
		DWORD coverCopies;
		BOOL screen;
		Arrival arrive;
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
		DWORD copyCaller;
		DWORD copyOuter;
		DWORD copySrcData;
		DWORD copyInLogs;
		DWORD indexedOtherLogs;
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

	struct SeedRecord {
		StateBuffer* owner;
		DWORD id;
		RECT rect;
		DWORD kind;
		CHAR name[32];
		SeedPiece* pieces;
		BYTE* done;
		DWORD count;
		SeedConflict* conflicts;
		DWORD conflictCount;
		BOOL conflictsSent;
		BOOL pending;
		BOOL deferred;
		BOOL based;
		DWORD frameNo;
		DWORD tick;
	};

	enum SeedPieceState { PieceWaiting, PieceSent, PieceFailed, PieceReady, PieceWritten };

	struct SeedCounters {
		DWORD events;
		DWORD done;
		DWORD deferred;
		DWORD later;
		DWORD dropped;
		DWORD released;
		DWORD expired;
		DWORD overwritten;
		DWORD noTwin;
		DWORD noArt;
		DWORD pieces;
		DWORD emits;
		LONGLONG px;
		DWORD toMemory;
		DWORD toSurface;
		DWORD transfers;
	};

	struct {
		BOOL chosen;
		BOOL seed;
		DWORD waitMs;
		SeedRecord list[SEED_RECORDS];
		DWORD top;
		DWORD active;
		DWORD logs;
		SeedCounters kinds[SeedKinds];
		DWORD sheets;
		DWORD sheetFrames;
		DWORD sheetUnmapped;
		DWORD sheetAdditive;
		DWORD sheetOutside;
		DWORD sheetPieces;
		DWORD sheetShared;
		DWORD conflicts;
		DWORD conflictsUnchecked;
		DWORD repends;
		DWORD evicted;
		DWORD skippedPieces;
		LONGLONG waitUs;
		DWORD waits;
		DWORD waitMaxUs;
		DWORD coverSeeded;
		DWORD coverUnseeded;
		DWORD coverUntwinned;
		LONGLONG coverSeededPx;
		LONGLONG coverUnseededPx;
		LONGLONG coverUntwinnedPx;
		DWORD maskedCopies;
		DWORD maskedSkipped;
		DWORD repeats;
		BOOL conflictCheck;
		DWORD loadWaitMs;
		DWORD lastPresentTick;
		LONGLONG inlineBudget;
		LONGLONG inlineUsed;
		DWORD inlineDeferred;
		DWORD piecesWritten;
		DWORD late;
		DWORD latePieces;
		LONGLONG lateFrames;
		LONGLONG lateMs;
		DWORD lateMaxFrames;
		DWORD lateMaxMs;
		DWORD deferredInterval;
		BOOL deriving;
		BOOL deriveBased;
		DWORD derived;
		DWORD derivedPieces;
		DWORD waitDecode;
		DWORD loadWaits;
		DWORD loadWaitTotalUs;
	} seeds;

	enum XferVia { ViaWrite, ViaCreated, ViaTrap, ViaUnlock };

	struct XferWatch {
		StateBuffer* owner;
		DWORD id;
		LONG x;
		LONG y;
		DWORD seq;
		LONG rows;
		DWORD frameNo;
		DWORD via;
	};

	struct XferIndexed {
		const BYTE* src;
		LONG srcPitch;
		const DWORD* dst;
		LONG dstPitch;
		LONG width;
		LONG height;
		DWORD palette[256];
		DWORD chain[INDEXED_CHAIN];
		DWORD chainCount;
	};

	// indexed blits from memory that neither the latest store decode nor a record read names, by size and call chain
	struct UnnamedDecode {
		LONG w, h;
		DWORD count;
		DWORD chain[INDEXED_CHAIN];
		DWORD chainCount;
		DWORD frameNo;
	};

	struct {
		UnnamedDecode list[UNNAMED_SIZES];
		DWORD count;
		DWORD full;
		DWORD total;
		DWORD interval;
		DWORD logs;
	} unnamed;

	// the game's code around addresses the logs name, written to twin-code.bin
	struct {
		DWORD lo[CODE_REGIONS];
		DWORD hi[CODE_REGIONS];
		CHAR why[CODE_REGIONS][20];
		DWORD count;
		DWORD bytes;
		DWORD refused;
		BOOL dirty;
		DWORD writes;
	} code;

	enum XferLayout { Layout32 = 1, Layout16 = 2, Layout8 = 4 };

	// the last named decode, kept so a CPU transfer of its pixels into a surface can take its seed; only the latest decode is a candidate
	struct {
		DWORD seq;
		DWORD depth;
		BOOL valid;
		DWORD kind;
		CHAR name[32];
		LONG width;
		LONG height;
		const BYTE* first;
		LONG pitch;
		StateBuffer* target;
		DWORD frameNo;
		SeedPiece* pieces;
		DWORD count;
		DWORD pieceCap;
		SeedConflict* conflicts;
		DWORD conflictCount;
		SeedPiece* subPieces;
		DWORD subCap;
		// the decode's BlitObject as StoreDraw passed it; its format decides how the snapshot is read
		BOOL srcSet;
		const BYTE* srcData;
		LONG srcPitchField;
		BYTE srcTrue;
		RECT srcRect;
		// raw bytes of the decode, readable as 32, 16 or 8 bpp rows
		BYTE* raw;
		DWORD rawCap;
		const BYTE* rawData;
		LONG rawPitch;
		LONG rawLeft;
		LONG rawTop;
		LONG off32;
		LONG off16;
		LONG off8;
		DWORD layouts;
		BYTE trueColor;
		DWORD palette[256];
		BOOL paletteSet;
		XferIndexed indexedInside[XFER_INDEXED_INSIDE];
		DWORD indexedInsideCount;
		XferWatch inside[XFER_INSIDE];
		DWORD insideCount;
		XferWatch watch[XFER_WATCH];
		DWORD watchCount;
		DWORD decodes;
		DWORD named;
		DWORD snapshotFailed;
		DWORD formatBytes[4];
		DWORD locks;
		DWORD unlocks;
		DWORD unlocksUntwinned;
		DWORD unlocksMemory;
		DWORD unlocksInside;
		DWORD tries;
		DWORD noDecode;
		DWORD consumed;
		DWORD direct;
		DWORD stale;
		DWORD noFit;
		DWORD guardFails;
		DWORD partialExpired;
		DWORD watches;
		DWORD byDuring;
		DWORD byBefore;
		DWORD byInside;
		DWORD byWrite;
		DWORD byCreated;
		DWORD createdWatches;
		DWORD byTrap;
		DWORD trapWatches;
		DWORD trapLate;
		DWORD byIndexed;
		DWORD indexed;
		DWORD indexedFromDecode;
		DWORD indexedIntoDecode;
		DWORD indexedPartial;
		DWORD indexedNoTwin;
		DWORD indexedOther;
		DWORD indexedInsideDropped;
		DWORD unlockLarge;
		// the latest decode has a name but no pack art; its blits are not unnamed memory
		const BYTE* artSrc;
		LONG artSrcPitch;
		const CHAR* artReason;
		DWORD indexedArtless;
		DWORD artlessLogs;
		DWORD recordMatches;
		DWORD recordSeeded;
		DWORD recordDeferred;
		DWORD recordNoArt;
		DWORD recordNoFit;
		DWORD namedLogs;
		DWORD blank;
		DWORD exact[5];
		DWORD tolerant[5];
		DWORD weakTolerant;
		DWORD logs;
		DWORD failLogs;
		DWORD largeLogs;
		DWORD decodeLogs;
	} xfer;

	enum TrapFlags { TrapDecode = 1, TrapRender = 2, TrapHooked = 4, TrapOtherThread = 8 };

	// written by the exception handler on whatever thread faulted; drained on the game thread
	enum TrapReg { RegEax, RegEcx, RegEdx, RegEbx, RegEsi, RegEdi, TrapRegs };

	struct TrapEvent {
		volatile LONG ready;
		DWORD eip;
		DWORD addr;
		DWORD caller;
		DWORD ret[4];
		DWORD regs[TrapRegs];
		BYTE code[4];
		DWORD id;
		const BYTE* start;
		DWORD seq;
		DWORD frameNo;
		DWORD flags;
	};

	struct TrapRange {
		const BYTE* volatile start;
		volatile DWORD bytes;
		volatile LONG faults;
		volatile LONG own;
		// game thread only: armed pages that no announced write of ours opened yet
		DWORD* bits;
		DWORD bitWords;
		DWORD pages;
		DWORD armedBits;
		DWORD lastArm;
		DWORD interval;
		DWORD foreignSinceArm;
		BOOL fresh;
		DWORD quickArms;
		BOOL queued;
		DWORD reFirst;
		DWORD reLast;
		BOOL priority;
		DWORD priorityFaults;
	};

	struct TrapWriter {
		DWORD eip;
		DWORD caller;
		DWORD ret[4];
		DWORD count;
		DWORD twins[4];
		DWORD twinCount;
		DWORD sampleId;
		LONG sampleW;
		LONG sampleH;
		LONG sampleX;
		LONG sampleY;
		DWORD decode;
		DWORD render;
		DWORD hooked;
		DWORD other;
		DWORD fresh;
		CHAR module[24];
		DWORD moduleOffset;
	};

	struct {
		BOOL chosen;
		BOOL wanted;
		BOOL initialized;
		volatile BOOL on;
		DWORD selfLo;
		DWORD selfHi;
		DWORD exeLo;
		DWORD exeHi;
		DWORD gameThread;
		volatile LONG renderDepth;
		TrapRange ranges[TWIN_MAX];
		TrapEvent ring[TRAP_RING];
		volatile LONG head;
		DWORD tail;
		volatile LONG faults;
		volatile LONG own;
		volatile LONG foreign;
		volatile LONG dropped;
		volatile LONG retried;
		struct {
			volatile DWORD tid;
			volatile DWORD addr;
			volatile LONG count;
		} retry[64];
		TrapWriter writers[TRAP_WRITERS];
		DWORD writerCount;
		DWORD writersFull;
		DWORD lost;
		DWORD stale;
		DWORD attached;
		DWORD notPaged;
		DWORD armFails;
		DWORD createArms;
		DWORD rearms;
		DWORD budgetSkips;
		DWORD tooBig;
		DWORD opens;
		DWORD openPages;
		DWORD windowTick;
		DWORD windowPages;
		DWORD cursor;
		DWORD reopen[TRAP_REOPEN];
		DWORD reopenCount;
		DWORD quickArms;
		BOOL spentLogged;
		BOOL draining;
		struct {
			StateBuffer* owner;
			DWORD id;
		} priority[TRAP_PRIORITY];
		DWORD priorityCount;
		DWORD priorityArms;
		DWORD priorityHot;
		DWORD priorityWindow;
		DWORD priorityWrites;
		DWORD priorityLogs;
		struct {
			DWORD eip;
			DWORD id;
		} pairs[TRAP_PAIRS];
		DWORD pairCount;
		DWORD reopenSkips;
		DWORD marked;
		BOOL noOpen;
		BOOL copyChosen;
		BOOL copyOn;
		struct {
			StateBuffer* dst;
			DWORD dstId;
			StateBuffer* src;
			DWORD srcId;
			LONG ox;
			LONG oy;
			RECT rect;
			DWORD faults;
			DWORD frameNo;
		} hints[TRAP_HINTS];
		DWORD hintCount;
		DWORD copyFaults;
		DWORD copyReplayed;
		DWORD copyGuardFails;
		DWORD copyPx;
		DWORD copyLogs;
	} trap;

	BOOL GpuCheckWanted();
	const CHAR* TwinKind(const Record* twin);
	DWORD GameCaller();
	VOID RegText(DWORD value, CHAR* text);
	VOID TrapName(DWORD address, CHAR* text);
	VOID LogCopyIns(const DWORD* top, DWORD found);
	VOID CodeFlush();
	VOID TrapDrain();
	VOID TrapReopen();
	VOID TrapOpen(Record* twin, const RECT* rect);
	VOID TrapAttach(Record* twin, DWORD id);
	VOID TrapDetach(DWORD id);
	VOID TrapRearm(DWORD now);
	VOID TrapLog();

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

	struct SlowCmd {
		DWORD us;
		DWORD type;
		DWORD dst;
		DWORD src;
		RECT rect;
		LONG x;
		LONG y;
		DWORD flags;
		DWORD extra;
		DWORD cmds;
		DWORD uploads;
		DWORD uploadUs;
		DWORD frameNo;
	};

	struct ReplayTimes {
		LONGLONG us[CMD_TYPES];
		DWORD max[CMD_TYPES];
		DWORD count[CMD_TYPES];
		SlowCmd slow;
		DWORD slowReplays;
	};

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
		DWORD seedReplayed;
		DWORD seedPieces;
		DWORD seedMissing;
		DWORD conflictsChecked;
		DWORD conflictsDiffer;
		DWORD conflictsSkipped;
		DWORD conflictBlocks;
		ReplayTimes times;
		LONGLONG inlineBytes;
		LONGLONG inlineUs;
		DWORD inlineUploads;
		LONGLONG rateBytes;
		LONGLONG rateUs;
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
		DWORD seedReplayed;
		DWORD seedPieces;
		DWORD seedMissing;
		DWORD conflictsChecked;
		DWORD conflictsDiffer;
		DWORD conflictsSkipped;
		DWORD conflictBlocks;
		DWORD conflictLogs;
		GLuint conflict;
		GLuint conflictFbo;
		LONG conflictW;
		LONG conflictH;
		DWORD* conflictData;
		ReplayTimes times;
		SlowCmd drawSlow;
		DWORD inlineUploads;
		LONGLONG inlineUs;
		LONGLONG inlineBytes;
		DWORD slowLogs;
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

	VOID Upload(Record* twin, DWORD id, const RECT* rect, BOOL seed = FALSE);
	VOID DropRegions(Record* twin, const RECT* rect);
	VOID SeedWrite(Record* twin, const RECT* rect);
	VOID SeedDropTwin(Record* twin, DWORD id);
	VOID SeedRepend(const CHAR* reason);

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

		SeedWrite(twin, &rect);
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

	VOID Convert(const Record* twin, DWORD* dst, const DWORD* src, LONG width, LONG height, BOOL seed = FALSE)
	{
		BOOL keyed = twin->keyed;
		DWORD key = twin->key;
		for (LONG y = height; y; --y)
		{
			for (LONG x = 0; x < width; ++x)
			{
				DWORD pixel = src[x];
				// key pixels become transparent black so premultiplied art blends over them; alpha 254 marks a 1x alpha byte other than 255, so keyed copies keep the CPU's exact DWORD compare
				// a seeded rect always takes the decode's key, so its alpha is the 1x key mask the 4K art is cut by
				if ((keyed && pixel == key) || (seed && pixel == COLORKEY_CHECK))
					*dst++ = 0;
				else
					*dst++ = pixel >= ALPHA_COMPONENT ? pixel : (pixel & 0x00FFFFFF) | 0xFE000000;
			}
			src += twin->pitch;
		}
	}

	BOOL Is4K(const ArtTag* tag)
	{
		return tag->frame >= 0 || tag->frame == TAG_GROUND || tag->frame == TAG_SEED_SHEET || tag->frame == TAG_MERGED_4K;
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
		BOOL any4K = Is4K(tag);
		for (DWORD i = 0; i < twin->artCount; ++i)
		{
			UnionRect(&bound, &bound, &twin->art[i]);
			any4K = any4K || Is4K(&twin->artTag[i]);
		}
		twin->artCount = 1;
		twin->art[0] = bound;
		twin->artTag[0] = none;
		if (any4K)
			twin->artTag[0].frame = TAG_MERGED_4K;
	}

	BOOL Carries4K(const Record* twin, const RECT* rect)
	{
		RECT part;
		for (DWORD i = 0; i < twin->artCount; ++i)
			if (Is4K(&twin->artTag[i]) && IntersectRect(&part, &twin->art[i], rect))
				return TRUE;
		return FALSE;
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

	VOID Upload(Record* twin, DWORD id, const RECT* rect, BOOL seed)
	{
		LONG width = rect->right - rect->left;
		LONG height = rect->bottom - rect->top;

		DWORD offset;
		DWORD* dst = (DWORD*)Reserve(width * height * sizeof(DWORD), &offset);
		if (!dst)
			return;

		const DWORD* src = (const DWORD*)twin->base + rect->top * twin->pitch + rect->left;
		Convert(twin, dst, src, width, height, seed);

		Cmd* cmd = Emit(CmdUpload);
		if (!cmd)
			return;
		cmd->dst = id;
		cmd->rect = *rect;
		cmd->value = offset;

		++game.list->counters.uploads;
		game.list->counters.pixels += width * height;
		++twin->arrive.uploads;
		twin->arrive.uploadPx += width * height;
		if (seed)
			++twin->arrive.seedUploads;
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
		SeedWrite(twin, rect);
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
		SeedRepend(reset ? "queue overflow" : "GL release");
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

	VOID XferResolve(BOOL final);

	VOID Sync(BOOL drain = TRUE)
	{
		if (drain)
			DrainSync();

		LONG gen = shared.gen;
		if (gen != game.seenGen)
		{
			game.seenGen = gen;
			Rebuild(FALSE);
			DebugLog("twin rebuild after GL release, twins %u", game.alive);
		}

		if (trap.tail != (DWORD)trap.head)
			TrapDrain();
		if (drain && trap.reopenCount)
			TrapReopen();

		// a pixel hook that defers its syncs is still mid-write
		if (drain && xfer.watchCount)
			XferResolve(FALSE);
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

		if (!SeedMode())
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
		TrapDetach(id);
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

		SeedDropTwin(twin, id);
		twin->coverPx = twin->coverCopies = 0;
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
		twin->screen = FALSE;

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
		if (!Active() || !place || !parts || !count || SeedMode())
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
		if (!art.checked && GpuCheckWanted())
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
		if (!Active() || SeedMode())
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

		// in seed mode the art is written only at a decode; memory met later may have been rewritten since
		if (frame >= 0 && !SeedMode())
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

	VOID DropOverlapping(const VOID* base, LONG pitch, LONG width, LONG height);

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

		DropOverlapping(base, pitch, width, height);

		twin = FindAddress(first, &id);
		if (twin)
			return;

		CreateMemoryTwin(base, pitch, width, height, -1, FALSE, TRUE, &id);
	}

	// another memory twin over any part of this region describes stale geometry
	VOID DropOverlapping(const VOID* base, LONG pitch, LONG width, LONG height)
	{
		const DWORD* first = (const DWORD*)base;
		const DWORD* last = first + (height - 1) * pitch + width - 1;
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

#pragma region Seed
	const CHAR* const seedKindNames[SeedKinds] = { "store", "sheet", "face" };

	BOOL SeedMode()
	{
		if (!seeds.chosen)
		{
			seeds.chosen = TRUE;
			CHAR value[16], fallback[] = "seed";
			Config::Get("Twin", "Mode", fallback, value, sizeof(value));
			const CHAR* origin = "Twin.Mode";
			CHAR env[16];
			DWORD length = GetEnvironmentVariable("DGL_TWIN_MODE", env, sizeof(env));
			if (length && length < sizeof(env))
			{
				StrCopy(value, env);
				origin = "DGL_TWIN_MODE";
			}
			seeds.seed = StrCompareInsensitive(value, "match") != 0;
			INT wait = Config::Get("Twin", "SeedWait", SEED_WAIT_DEFAULT);
			seeds.waitMs = (DWORD)max(0, min(wait, 2000));
			wait = Config::Get("Twin", "SeedLoadWait", SEED_LOAD_WAIT_DEFAULT);
			seeds.loadWaitMs = (DWORD)max(0, min(wait, 2000));
			DebugLog("twin mode %s (%s=%s, seed wait %u ms): %s", seeds.seed ? "seed" : "match", origin, value, seeds.waitMs,
				seeds.seed ? "4K art is written where named 1x art is decoded; no expectations, regions, hash lookups or twin-index.bin" : "expectations, regions and the hash index identify art");
		}
		return seeds.seed;
	}

	// the GPU check and the screenshots read back on the render thread; they run only on F12 unless asked for
	BOOL GpuCheckWanted()
	{
		static INT wanted = -1;
		if (wanted < 0)
			wanted = Config::Get("Twin", "GpuCheck", 0) != 0;
		return wanted;
	}

	VOID SeedFree(SeedRecord* r)
	{
		if (r->pieces)
			MemoryFree(r->pieces);
		if (r->done)
			MemoryFree(r->done);
		if (r->conflicts)
			MemoryFree(r->conflicts);
		MemoryZero(r, sizeof(SeedRecord));
		--seeds.active;
		while (seeds.top && !seeds.list[seeds.top - 1].owner)
			--seeds.top;
	}

	VOID SeedForget(SeedRecord* r, BOOL written)
	{
		Record* twin = &game.twins[r->id];
		if (twin->owner == r->owner && twin->seedCount)
			--twin->seedCount;

		SeedCounters* c = &seeds.kinds[r->kind];
		if (r->pending)
		{
			if (written)
				++c->dropped;
			else
				++c->released;
		}
		else if (written)
			++c->overwritten;
		SeedFree(r);
	}

	VOID SeedWrite(Record* twin, const RECT* rect)
	{
		TrapOpen(twin, rect);
		if (!twin->seedCount)
			return;

		RECT part;
		for (DWORD i = 0; i < seeds.top; ++i)
		{
			SeedRecord* r = &seeds.list[i];
			if (!r->owner || r->owner != twin->owner || !IntersectRect(&part, &r->rect, rect))
				continue;
			// only the pieces the write touched lose their 4K; the rest of the decode is still intact
			DWORD left = 0;
			for (DWORD k = 0; k < r->count; ++k)
			{
				if (r->done[k] == PieceWritten || r->done[k] == PieceFailed)
					continue;
				RECT to = r->pieces[k].to;
				OffsetRect(&to, r->rect.left, r->rect.top);
				if (IntersectRect(&part, &to, rect))
				{
					r->done[k] = PieceWritten;
					++seeds.piecesWritten;
				}
				else
					++left;
			}
			if (!left || Contains(rect, &r->rect))
				SeedForget(r, TRUE);
		}
	}

	VOID SeedDropTwin(Record* twin, DWORD id)
	{
		if (!twin->seedCount)
			return;

		for (DWORD i = 0; i < seeds.top; ++i)
		{
			SeedRecord* r = &seeds.list[i];
			if (r->owner && r->owner == twin->owner && r->id == id)
				SeedForget(r, FALSE);
		}
		twin->seedCount = 0;
	}

	// the GPU copy was rebuilt from 1x; a seed whose 1x was not written since is still true and is drawn again
	VOID SeedRepend(const CHAR* reason)
	{
		DWORD count = 0;
		for (DWORD i = 0; i < seeds.top; ++i)
		{
			SeedRecord* r = &seeds.list[i];
			if (!r->owner)
				continue;
			for (DWORD k = 0; k < r->count; ++k)
				if (r->done[k] != PieceFailed && r->done[k] != PieceWritten)
					r->done[k] = PieceWaiting;
			r->based = FALSE;
			r->pending = TRUE;
			r->frameNo = game.frameNo;
			++count;
		}

		if (count)
		{
			seeds.repends += count;
			DebugLog("twin seed %u intact seeds queued for redraw after %s at frame %u", count, reason, game.frameNo);
		}
	}

	DWORD SeedEmit(SeedRecord* r)
	{
		Record* twin = &game.twins[r->id];
		if (twin->owner != r->owner)
		{
			SeedForget(r, FALSE);
			return 0;
		}

		DWORD fresh = 0, waiting = 0;
		LONG last = -2, state = 0;
		for (DWORD i = 0; i < r->count; ++i)
		{
			if (r->done[i] == PieceSent || r->done[i] == PieceFailed || r->done[i] == PieceWritten)
				continue;
			if (r->pieces[i].frame != last)
			{
				last = r->pieces[i].frame;
				state = ImageReplace::ArtState(last);
				// a decoded frame is uploaded by the replay that draws it; past this present's budget it waits for the next one or for the upload pump
				if (state == 1)
				{
					LONG bytes = ImageReplace::ArtBytes(last);
					if (seeds.inlineUsed + bytes > seeds.inlineBudget && seeds.inlineUsed)
					{
						state = 0;
						++seeds.inlineDeferred;
					}
					else
						seeds.inlineUsed += bytes;
				}
				else if (!state)
					++seeds.waitDecode;
			}
			if (state < 0)
				r->done[i] = PieceFailed;
			else if (state > 0)
			{
				r->done[i] = PieceReady;
				++fresh;
			}
			else
			{
				r->done[i] = PieceWaiting;
				++waiting;
			}
		}
		if (!fresh)
			return waiting;

		RECT rect = r->rect;
		SeedCounters* c = &seeds.kinds[r->kind];
		if (!r->based)
		{
			// the decode is still pending as dirty 1x; it is replaced by a key-alpha upload the 4K is cut by
			DrainSyncIn(twin, &rect);
			Cover(twin, &rect);
			FlushRegion(twin, r->id, &rect, TRUE);
			Upload(twin, r->id, &rect, TRUE);
			r->based = TRUE;
			c->px += Area(&rect);
		}

		DWORD conflictCount = 0;
		if (!waiting && !r->conflictsSent && seeds.conflictCheck)
		{
			r->conflictsSent = TRUE;
			for (DWORD i = 0; i < r->conflictCount; ++i)
				if (ImageReplace::ArtState(r->conflicts[i].frameA) > 0 && ImageReplace::ArtState(r->conflicts[i].frameB) > 0)
					++conflictCount;
			seeds.conflictsUnchecked += r->conflictCount - conflictCount;
		}

		DWORD offset;
		SeedPiece* out = (SeedPiece*)Reserve(fresh * sizeof(SeedPiece) + conflictCount * sizeof(SeedConflict), &offset);
		Cmd* cmd = out ? Emit(CmdSeed) : NULL;
		if (!cmd)
		{
			for (DWORD i = 0; i < r->count; ++i)
				if (r->done[i] == PieceReady)
					r->done[i] = PieceWaiting;
			return waiting + fresh;
		}

		for (DWORD i = 0; i < r->count; ++i)
			if (r->done[i] == PieceReady)
			{
				*out = r->pieces[i];
				OffsetRect(&out->to, rect.left, rect.top);
				OffsetRect(&out->place, rect.left, rect.top);
				++out;
				r->done[i] = PieceSent;
			}

		SeedConflict* conflict = (SeedConflict*)out;
		for (DWORD i = 0; i < r->conflictCount && conflictCount; ++i)
		{
			const SeedConflict* in = &r->conflicts[i];
			if (ImageReplace::ArtState(in->frameA) <= 0 || ImageReplace::ArtState(in->frameB) <= 0)
				continue;
			*conflict = *in;
			OffsetRect(&conflict->to, rect.left, rect.top);
			OffsetRect(&conflict->placeA, rect.left, rect.top);
			OffsetRect(&conflict->placeB, rect.left, rect.top);
			++conflict;
			--conflictCount;
			++seeds.conflicts;
		}
		conflictCount = DWORD(conflict - (SeedConflict*)out);

		cmd->dst = r->id;
		cmd->rect = rect;
		cmd->value = offset;
		cmd->x = (LONG)fresh;
		cmd->y = (LONG)conflictCount;
		cmd->flags = r->kind;

		DropRegions(twin, &rect);
		ArtTag tag = { r->kind != SeedSheet && r->count == 1 ? r->pieces[0].frame : TAG_SEED_SHEET, 0, 0, rect.right - rect.left, rect.bottom - rect.top, r->id };
		AddArt(twin, &rect, &tag);

		c->pieces += fresh;
		++c->emits;
		if (r->deferred)
		{
			DWORD frames = game.frameNo - r->frameNo, ms = GetTickCount() - r->tick;
			++seeds.late;
			seeds.latePieces += fresh;
			seeds.lateFrames += frames;
			seeds.lateMs += ms;
			seeds.lateMaxFrames = max(seeds.lateMaxFrames, frames);
			seeds.lateMaxMs = max(seeds.lateMaxMs, ms);
		}
		game.lastArtSource = r->id;
		if (!art.firstTick)
			art.firstTick = GetTickCount() | 1;
		if (!art.checked && GpuCheckWanted())
		{
			art.checked = TRUE;
			game.checkRequest = TRUE;
		}
		return waiting;
	}

	VOID SeedLog(const CHAR* what, DWORD kind, const CHAR* name, const Record* twin, DWORD id, const RECT* rect, LONG pitch, const SeedRecord* r, DWORD waitUs)
	{
		if (seeds.logs >= SEED_LOGS)
			return;
		++seeds.logs;

		DWORD sent = 0, waiting = 0, failed = 0;
		LONG low = 0x7FFFFFFF, high = -1;
		for (DWORD i = 0; r && i < r->count; ++i)
		{
			sent += r->done[i] == PieceSent;
			waiting += r->done[i] == PieceWaiting;
			failed += r->done[i] == PieceFailed;
			low = min(low, r->pieces[i].frame);
			high = max(high, r->pieces[i].frame);
		}

		DebugLog("twin seed #%u %s %s name=%.32s twin %u %s buf=%p base=%p rect %d,%d %dx%d pitch %d | pieces %u sent %u waiting %u no art %u | frames %d..%d pairs %u | wait %u us frame %u",
			seeds.logs, seedKindNames[kind], what, name, twin ? id : TWIN_NONE, twin ? TwinKind(twin) : "none", twin ? twin->owner : NULL, twin ? twin->base : NULL,
			rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top, pitch, r ? r->count : 0, sent, waiting, failed,
			r && r->count ? low : -1, high, r ? r->conflictCount : 0, waitUs, game.frameNo);
	}

	VOID SeedPoll()
	{
		for (DWORD i = 0; i < seeds.top; ++i)
		{
			SeedRecord* r = &seeds.list[i];
			if (!r->owner || !r->pending)
				continue;
			// after a rebuild every intact seed comes back; spreading them over frames keeps the queue under its limit
			if (game.list && game.list->size > SEED_POLL_BYTES)
				break;

			DWORD waiting = SeedEmit(r);
			if (!r->owner)
				continue;

			if (!waiting)
			{
				r->pending = FALSE;
				if (r->deferred)
				{
					r->deferred = FALSE;
					++seeds.kinds[r->kind].later;
					RECT local = r->rect;
					SeedLog("completed later", r->kind, r->name, &game.twins[r->id], r->id, &local, game.twins[r->id].pitch, r, 0);
				}
			}
			else if (game.frameNo - r->frameNo > SEED_PENDING_FRAMES)
			{
				r->pending = FALSE;
				r->deferred = FALSE;
				++seeds.kinds[r->kind].expired;
			}
		}
	}

	VOID XferCapture(const VOID* first, LONG pitch, LONG width, LONG height, SeedKind kind, const CHAR* label, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount, const SeedSheetStats* sheet);
	VOID XferResolve(BOOL final);

	// returns 0 when nothing was recorded, 1 done, 2 deferred
	DWORD SeedAt(Record* twin, DWORD id, const RECT* at, LONG pitch, SeedKind kind, const CHAR* label, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount, BOOL transfer)
	{
		SeedCounters* c = &seeds.kinds[kind];
		RECT rect = *at;
		LONG width = rect.right - rect.left, height = rect.bottom - rect.top;
		if (twin->seedCount)
			for (DWORD i = 0; i < seeds.top; ++i)
			{
				SeedRecord* other = &seeds.list[i];
				// no hook saw a write since the last seed of this image here, so the twin still holds it
				if (other->owner == twin->owner && EqualRect(&other->rect, &rect) && other->kind == (DWORD)kind && !StrCompareNum(other->name, label, sizeof(other->name) - 1))
				{
					BOOL written = FALSE;
					for (DWORD k = 0; k < other->count && !written; ++k)
						written = other->done[k] == PieceWritten;
					if (!written)
					{
						++seeds.repeats;
						return 0;
					}
					SeedForget(other, TRUE);
					break;
				}
			}
		SeedWrite(twin, &rect);

		SeedRecord* r = NULL;
		for (DWORD i = 0; i < SEED_RECORDS && !r; ++i)
			if (!seeds.list[i].owner)
				r = &seeds.list[i];
		if (!r)
		{
			SeedRecord* oldest = NULL;
			for (DWORD i = 0; i < SEED_RECORDS; ++i)
			{
				SeedRecord* other = &seeds.list[i];
				if (!oldest || (oldest->pending && !other->pending) || (!oldest->pending == !other->pending && other->frameNo < oldest->frameNo))
					oldest = other;
			}
			SeedForget(oldest, FALSE);
			++seeds.evicted;
			r = oldest;
		}

		count = min(count, (DWORD)SEED_PIECES);
		conflictCount = conflicts ? min(conflictCount, (DWORD)SEED_CONFLICTS) : 0;
		r->pieces = (SeedPiece*)MemoryAlloc(count * sizeof(SeedPiece));
		r->done = (BYTE*)MemoryAlloc(count);
		r->conflicts = conflictCount ? (SeedConflict*)MemoryAlloc(conflictCount * sizeof(SeedConflict)) : NULL;
		if (!r->pieces || !r->done || (conflictCount && !r->conflicts))
		{
			if (r->pieces)
				MemoryFree(r->pieces);
			if (r->done)
				MemoryFree(r->done);
			if (r->conflicts)
				MemoryFree(r->conflicts);
			MemoryZero(r, sizeof(SeedRecord));
			++c->noTwin;
			return 0;
		}

		for (DWORD i = 0; i < count; ++i)
		{
			const SeedPiece* p = &pieces[i];
			// a piece carried by a mirrored copy has its place mirrored too: left > right
			if (p->to.left < 0 || p->to.top < 0 || p->to.right > width || p->to.bottom > height || p->to.left >= p->to.right || p->to.top >= p->to.bottom ||
				p->place.left == p->place.right || p->place.top >= p->place.bottom || p->frame < 0)
			{
				++seeds.skippedPieces;
				continue;
			}
			r->pieces[r->count] = *p;
			r->done[r->count++] = PieceWaiting;
		}
		for (DWORD i = 0; i < conflictCount; ++i)
			r->conflicts[i] = conflicts[i];
		r->conflictCount = conflictCount;

		r->owner = twin->owner;
		r->id = id;
		r->rect = rect;
		r->kind = kind;
		DWORD length = min((DWORD)StrLength(label), (DWORD)sizeof(r->name) - 1);
		MemoryCopy(r->name, label, length);
		r->name[length] = 0;
		r->frameNo = game.frameNo;
		r->tick = GetTickCount();
		r->based = seeds.deriveBased;
		++seeds.active;
		if ((DWORD)(r - seeds.list) >= seeds.top)
			seeds.top = DWORD(r - seeds.list) + 1;
		++twin->seedCount;
		if (!r->count)
		{
			SeedForget(r, FALSE);
			++c->noArt;
			SeedLog(transfer ? "transfer skipped: every piece fell outside the image" : "skipped: every piece fell outside the image", kind, label, twin, id, &rect, pitch, NULL, 0);
			return 0;
		}
		if (seeds.deriving)
			;
		else if (transfer)
			++c->transfers;
		else if (twin->memory)
			++c->toMemory;
		else
			++c->toSurface;

		// the decode worker usually finishes within a frame; waiting here lets memory the game copies out and reuses at once get its 4K
		DWORD waitUs = 0;
		DWORD waitMs = seeds.deriving ? 0 : seeds.waitMs;
		// the game is not presenting, so it is loading and blocked anyway; only then does a decode wait for its 4K
		if (!seeds.deriving && seeds.lastPresentTick && GetTickCount() - seeds.lastPresentTick > SEED_LOAD_GAP_MS && seeds.loadWaitMs > waitMs && seeds.loadWaitTotalUs < SEED_LOAD_TOTAL_MS * 1000)
		{
			waitMs = seeds.loadWaitMs;
			++seeds.loadWaits;
		}
		if (waitMs)
		{
			LARGE_INTEGER start, now;
			QueryPerformanceCounter(&start);
			LONGLONG frequency = game.frequency.QuadPart ? game.frequency.QuadPart : 1000;
			for (;;)
			{
				BOOL all = TRUE;
				LONG last = -2;
				for (DWORD i = 0; i < r->count; ++i)
					if (r->pieces[i].frame != last)
					{
						last = r->pieces[i].frame;
						if (!ImageReplace::ArtState(last))
							all = FALSE;
					}

				QueryPerformanceCounter(&now);
				waitUs = DWORD((now.QuadPart - start.QuadPart) * 1000000 / frequency);
				if (all || waitUs >= waitMs * 1000)
					break;
				Sleep(1);
			}
			seeds.waitUs += waitUs;
			seeds.loadWaitTotalUs += waitUs;
			++seeds.waits;
			if (waitUs > seeds.waitMaxUs)
				seeds.waitMaxUs = waitUs;
		}

		// the copy that carries a pending seed is recorded before the CPU copies the 1x, so its base is uploaded at the next present
		DWORD waiting = seeds.deriving ? r->count : SeedEmit(r);
		if (!r->owner)
			return 0;
		if (waiting)
		{
			r->pending = TRUE;
			r->deferred = TRUE;
			++c->deferred;
			++seeds.deferredInterval;
		}
		else
			++c->done;
		if (!seeds.deriving)
			SeedLog(transfer ? (waiting ? "transfer deferred" : "transfer done") : (waiting ? "deferred" : "done"), kind, label, twin, id, &rect, pitch, r, waitUs);
		return waiting ? 2 : 1;
	}

	// an unkeyed copy out of memory whose seed still waits for its art takes the waiting pieces along, so the 4K lands where the pixels went even after the source is reused
	VOID SeedDerive(Record* target, DWORD dstId, const RECT* to, Record* source, const RECT* from, BOOL mirror)
	{
		if (!source->seedCount || target == source)
			return;
		// a buffer that was presented is redrawn every frame; an offscreen surface of the screen's size (a full-screen background) takes the pieces
		if (target->screen)
			return;

		LONG ox = to->left - from->left, oy = to->top - from->top;
		for (DWORD i = 0; i < seeds.top; ++i)
		{
			SeedRecord* r = &seeds.list[i];
			RECT part;
			if (!r->owner || r->owner != source->owner || !r->pending || !IntersectRect(&part, &r->rect, from))
				continue;

			static SeedPiece pieces[SEED_PIECES / 4];
			DWORD count = 0;
			RECT bound = { 0, 0, 0, 0 };
			for (DWORD k = 0; k < r->count && count < sizeof(pieces) / sizeof(*pieces); ++k)
			{
				if (r->done[k] != PieceWaiting)
					continue;
				RECT g = r->pieces[k].to;
				OffsetRect(&g, r->rect.left, r->rect.top);
				if (!Contains(from, &g))
					continue;
				SeedPiece* p = &pieces[count++];
				p->frame = r->pieces[k].frame;
				p->place = r->pieces[k].place;
				OffsetRect(&p->place, r->rect.left, r->rect.top);
				if (mirror)
				{
					// column x of the source lands at from.left + from.right - 1 - x; the place is mirrored with it, so its left edge ends up right
					LONG axis = from->left + from->right;
					SetRect(&g, axis - g.right, g.top, axis - g.left, g.bottom);
					p->place.left = axis - p->place.left;
					p->place.right = axis - p->place.right;
				}
				p->to = g;
				if (count == 1)
					bound = g;
				else
					UnionRect(&bound, &bound, &g);
			}
			if (!count)
				continue;

			RECT rect = bound;
			for (DWORD k = 0; k < count; ++k)
			{
				OffsetRect(&pieces[k].to, -rect.left, -rect.top);
				OffsetRect(&pieces[k].place, -rect.left, -rect.top);
			}
			RECT at = rect;
			OffsetRect(&at, ox, oy);
			DWORD kind = r->kind;
			CHAR name[sizeof(r->name)];
			MemoryCopy(name, r->name, sizeof(name));

			// the copy carried the key-alpha base the source's first pieces were cut by, so it is not uploaded again over them
			seeds.deriveBased = r->based;
			seeds.deriving = TRUE;
			SeedAt(target, dstId, &at, target->pitch, (SeedKind)kind, name, pieces, count, NULL, 0, TRUE);
			seeds.deriving = FALSE;
			seeds.deriveBased = FALSE;
			++seeds.derived;
			seeds.derivedPieces += count;
		}
	}

	VOID SeedArt(const VOID* first, LONG pitch, LONG width, LONG height, SeedKind kind, const CHAR* name, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount, const SeedSheetStats* sheet)
	{
		Timed timed("SeedArt");
		if (!Active() || !SeedMode() || !first || width <= 0 || height <= 0 || pitch < width || (DWORD)kind >= SeedKinds)
			return;

		Sync();
		const CHAR* label = name ? name : "?";
		SeedCounters* c = &seeds.kinds[kind];
		++c->events;
		if (sheet)
		{
			++seeds.sheets;
			seeds.sheetFrames += sheet->frames;
			seeds.sheetUnmapped += sheet->unmapped;
			seeds.sheetAdditive += sheet->additive;
			seeds.sheetOutside += sheet->outside;
			seeds.sheetShared += sheet->shared;
			seeds.sheetPieces += count;
		}

		// faces are copied out by hooked DrawFaces copies, which replay the memory twin's seed
		if (kind != SeedFace)
			XferCapture(first, pitch, width, height, kind, label, pieces, count, conflicts, conflictCount, sheet);

		RECT local = { 0, 0, width, height };
		if (!pieces || !count)
		{
			++c->noArt;
			SeedLog("skipped: no piece maps to a pack frame", kind, label, NULL, TWIN_NONE, &local, pitch, NULL, 0);
			return;
		}

		DWORD id = TWIN_NONE;
		LONG x = 0, y = 0;
		Record* twin = FindAddress(first, &id);
		BOOL fits = twin && PixelPos(twin, first, pitch, width, height, &x, &y);
		if (twin && !fits && twin->memory)
		{
			DropOverlapping(first, pitch, width, height);
			twin = NULL;
		}
		if (!twin)
		{
			twin = Adopt(first, &id);
			fits = twin && PixelPos(twin, first, pitch, width, height, &x, &y);
		}
		if (!twin || !fits)
		{
			++c->noTwin;
			SeedLog(twin ? "skipped: pitch or rect does not fit its twin" : "skipped: no twin for this memory", kind, label, twin, id, &local, pitch, NULL, 0);
			return;
		}
		if (kind != SeedFace)
			xfer.target = twin->owner;

		RECT rect = { x, y, x + width, y + height };
		SeedAt(twin, id, &rect, pitch, kind, label, pieces, count, conflicts, conflictCount, FALSE);
	}

#pragma region Transfer
	VOID XferBind();
	BOOL XferSnapshot(const VOID* first, LONG pitch, LONG width, LONG height, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount);
	VOID XferLogDecode();

	VOID DecodeSource(const VOID* data, LONG pitchField, BYTE trueColor, const RECT* rect)
	{
		if (!xfer.depth || !data || !rect)
			return;

		xfer.srcSet = TRUE;
		xfer.srcData = (const BYTE*)data;
		xfer.srcPitchField = pitchField;
		xfer.srcTrue = trueColor;
		xfer.srcRect = *rect;
	}

	const CHAR* ArtlessReason(SeedKind kind, const SeedSheetStats* sheet)
	{
		if (!sheet)
			return "no pack frame has its name and size";
		if (!sheet->frames)
			return "its sheet blocks list no frame";
		if (sheet->additive == sheet->frames)
			return "every frame is additive (aura or glow), which is not seeded";
		if (sheet->unmapped == sheet->frames)
			return "no frame name of the sheet is in the pack";
		if (sheet->unmapped + sheet->additive == sheet->frames)
			return "its frames are additive or not in the pack";
		return "every part falls outside the sheet";
	}

	VOID XferCapture(const VOID* first, LONG pitch, LONG width, LONG height, SeedKind kind, const CHAR* label, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount, const SeedSheetStats* sheet)
	{
		TrapDrain();
		// whatever was watched for the previous decode is complete by now
		XferResolve(TRUE);

		++xfer.seq;
		++xfer.decodes;
		++xfer.formatBytes[!xfer.srcSet ? 3 : xfer.srcTrue > 1 ? 2 : xfer.srcTrue];
		xfer.valid = FALSE;
		xfer.target = NULL;
		xfer.frameNo = game.frameNo;
		xfer.first = (const BYTE*)first;
		xfer.pitch = pitch;
		xfer.width = width;
		xfer.height = height;
		xfer.kind = kind;
		xfer.paletteSet = FALSE;
		xfer.layouts = 0;
		DWORD length = min((DWORD)StrLength(label), (DWORD)sizeof(xfer.name) - 1);
		MemoryCopy(xfer.name, label, length);
		xfer.name[length] = 0;
		// a named decode with no art still names its blits: they are logged with the reason instead of as unnamed memory
		xfer.artSrc = NULL;
		xfer.artReason = NULL;
		if ((!pieces || !count) && xfer.srcSet && xfer.srcTrue == 0 && xfer.srcPitchField > 0)
		{
			xfer.artSrc = xfer.srcData + xfer.srcRect.top * xfer.srcPitchField + xfer.srcRect.left;
			xfer.artSrcPitch = xfer.srcPitchField;
			xfer.artReason = ArtlessReason(kind, sheet);
		}
		if (pieces && count && XferSnapshot(first, pitch, width, height, pieces, count, conflicts, conflictCount))
		{
			xfer.valid = TRUE;
			++xfer.named;
		}
		XferLogDecode();
		xfer.srcSet = FALSE;
		XferBind();
	}

	BOOL XferSnapshot(const VOID* first, LONG pitch, LONG width, LONG height, const SeedPiece* pieces, DWORD count, const SeedConflict* conflicts, DWORD conflictCount)
	{
		// the range and the seed read the decode as 32 bpp at half the pitch field; the BlitObject also allows 16 bpp, or 8-bit indices at the pitch field itself
		const BYTE* data = (const BYTE*)first;
		LONG raw = pitch * 2, left = 0, top = 0;
		BOOL described = FALSE;
		xfer.trueColor = 0xFF;
		if (xfer.srcSet && xfer.srcPitchField > 0 && xfer.srcData + (xfer.srcRect.top * (xfer.srcPitchField >> 1) + xfer.srcRect.left) * (LONG)sizeof(DWORD) == (const BYTE*)first)
		{
			data = xfer.srcData;
			raw = xfer.srcPitchField;
			left = xfer.srcRect.left;
			top = xfer.srcRect.top;
			described = TRUE;
			xfer.trueColor = xfer.srcTrue;
		}

		LONG low = top * raw;
		LONG end32 = (top + height - 1) * raw * 2 + (left + width) * (LONG)sizeof(DWORD);
		LONG end16 = (top + height - 1) * raw + (left + width) * (LONG)sizeof(WORD);
		DWORD layouts = 0;
		LONG end = 0;
		if (!IsBadReadPtr(data + low, end32 - low))
		{
			layouts = described ? Layout32 | Layout16 | Layout8 : Layout32;
			end = end32;
		}
		else if (described && !IsBadReadPtr(data + low, end16 - low))
		{
			layouts = Layout16 | Layout8;
			end = end16;
		}

		DWORD bytes = layouts ? DWORD(end - low) : 0;
		if (bytes > xfer.rawCap)
		{
			if (xfer.raw)
				MemoryFree(xfer.raw);
			xfer.raw = (BYTE*)MemoryAlloc(bytes);
			xfer.rawCap = xfer.raw ? bytes : 0;
		}
		count = min(count, (DWORD)SEED_PIECES);
		if (count > xfer.pieceCap)
		{
			if (xfer.pieces)
				MemoryFree(xfer.pieces);
			xfer.pieces = (SeedPiece*)MemoryAlloc(count * sizeof(SeedPiece));
			xfer.pieceCap = xfer.pieces ? count : 0;
		}
		if (!xfer.conflicts)
			xfer.conflicts = (SeedConflict*)MemoryAlloc(SEED_CONFLICTS * sizeof(SeedConflict));
		if (!layouts || !xfer.raw || !xfer.pieces || !xfer.conflicts)
		{
			++xfer.snapshotFailed;
			return FALSE;
		}

		MemoryCopy(xfer.raw, data + low, bytes);
		xfer.rawData = data;
		xfer.rawPitch = raw;
		xfer.rawLeft = left;
		xfer.rawTop = top;
		xfer.off32 = top * raw * 2 + left * (LONG)sizeof(DWORD) - low;
		xfer.off16 = top * raw + left * (LONG)sizeof(WORD) - low;
		xfer.off8 = top * raw + left - low;
		xfer.layouts = layouts;
		MemoryCopy(xfer.pieces, pieces, count * sizeof(SeedPiece));
		xfer.count = count;
		xfer.conflictCount = conflicts ? min(conflictCount, (DWORD)SEED_CONFLICTS) : 0;
		if (xfer.conflictCount)
			MemoryCopy(xfer.conflicts, conflicts, xfer.conflictCount * sizeof(SeedConflict));
		return TRUE;
	}

	VOID XferLogDecode()
	{
		if (xfer.decodeLogs >= XFER_DECODE_LOGS)
			return;
		++xfer.decodeLogs;

		CHAR format[64], bytes[40];
		if (xfer.srcSet)
			StrPrint(format, "%u (%s)", xfer.srcTrue, xfer.srcTrue ? "true colour" : "8-bit indices");
		else
			StrCopy(format, "unknown (no StoreDraw source)");
		bytes[0] = 0;
		if (xfer.layouts)
		{
			const BYTE* b = xfer.raw + ((xfer.layouts & Layout8) ? xfer.off8 : xfer.off32);
			StrPrint(bytes, "%02X %02X %02X %02X %02X %02X %02X %02X", b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
		}
		DebugLog("twin seed decode #%u %.32s %dx%d: BlitObject format byte %s, pitch field %d, data %08X, clip %d,%d | snapshot %s%s%s%s | first bytes %s",
			xfer.seq, xfer.name, xfer.width, xfer.height, format, xfer.srcSet ? xfer.srcPitchField : xfer.pitch * 2, xfer.srcSet ? (DWORD)xfer.srcData : (DWORD)xfer.first,
			xfer.srcSet ? xfer.srcRect.left : 0, xfer.srcSet ? xfer.srcRect.top : 0, xfer.layouts ? "readable as" : (xfer.valid ? "none" : "not taken (no piece or unreadable)"),
			(xfer.layouts & Layout32) ? " 32" : "", (xfer.layouts & Layout16) ? " 16" : "", (xfer.layouts & Layout8) ? " 8 bpp" : "", bytes);
	}

	enum GuardFormat { GuardRgb, GuardBgr, Guard565, GuardIndexed, GuardPalette, GuardFormats };
	const CHAR* const guardNames[GuardFormats] = { "rgb32", "bgr32", "565", "8-bit indices, consistent colours", "8-bit indices through the blit's palette" };

	struct GuardResult {
		DWORD format;
		LONG lead;
		LONG rows;
		LONG badX;
		LONG badY;
		DWORD got;
		DWORD want;
		BOOL blank;
		LONG leads[GuardFormats];
		LONG matches[GuardFormats];
	};

	BOOL GuardKey(const Record* twin, DWORD color)
	{
		return !((color ^ COLORKEY_CHECK) & 0x00FFFFFF) || (twin->keyed && !((color ^ twin->key) & 0x00FFFFFF));
	}

	BOOL GuardNear(DWORD a, DWORD b)
	{
		for (DWORD shift = 0; shift < 24; shift += 8)
		{
			LONG d = LONG((a >> shift) & 0xFF) - LONG((b >> shift) & 0xFF);
			if (d > XFER_TOLERANCE || d < -XFER_TOLERANCE)
				return FALSE;
		}
		return TRUE;
	}

	// one decode row in one format against the surface row: the first column that differs, or width; within is the 565 compare to XFER_TOLERANCE
	LONG GuardRow(const Record* twin, DWORD format, const DWORD* have, LONG sx, LONG row, LONG width, BOOL final, BOOL* within, DWORD* map, DWORD* firstSeen, BOOL* varied, DWORD* want)
	{
		*within = TRUE;
		LONG bad = width;
		const BYTE* line32 = xfer.raw + xfer.off32 + row * xfer.rawPitch * 2;
		const BYTE* line16 = xfer.raw + xfer.off16 + row * xfer.rawPitch;
		const BYTE* line8 = xfer.raw + xfer.off8 + row * xfer.rawPitch;
		for (LONG i = 0; i < width; ++i)
		{
			DWORD got = have[i];
			BOOL key = GuardKey(twin, got);
			DWORD expect;
			BOOL same, close;
			if (format == GuardRgb || format == GuardBgr)
			{
				expect = ((const DWORD*)line32)[sx + i];
				if (format == GuardBgr)
					expect = (expect & 0xFF00FF00) | ((expect >> 16) & 0xFF) | ((expect & 0xFF) << 16);
				same = !((got ^ expect) & 0x00FFFFFF) || (key && (!((expect ^ COLORKEY_CHECK) & 0x00FFFFFF) || !(expect >> 24)));
				close = same;
			}
			else if (format == Guard565)
			{
				// the wrapper's own conversion, so its rounding and its key mapping
				expect = Hooks::Convert565toRGB(((const WORD*)line16)[sx + i]);
				same = key ? GuardKey(twin, expect) : !((got ^ expect) & 0x00FFFFFF);
				close = same || (!key && !GuardKey(twin, expect) && GuardNear(got, expect));
			}
			else if (format == GuardIndexed)
			{
				BYTE index = line8[sx + i];
				DWORD seen = key ? 0x02000000 : (got & 0x00FFFFFF) | 0x01000000;
				if (!map[index])
				{
					map[index] = seen;
					if (!*firstSeen)
						*firstSeen = seen;
					else if (seen != *firstSeen)
						*varied = TRUE;
				}
				expect = index;
				// every written pixel carries alpha; before the final check a zero is memory the game has not written yet
				same = close = map[index] == seen && (got || final);
			}
			else
			{
				expect = xfer.palette[line8[sx + i]] | ALPHA_COMPONENT;
				if ((expect & COLORKEY_AND) == 0xFFF800F8)
					expect = COLORKEY_CHECK;
				same = key ? GuardKey(twin, expect) : !((got ^ expect) & 0x00FFFFFF);
				close = same;
			}

			if (!same && bad == width)
			{
				bad = i;
				*want = expect;
			}
			if (!close)
			{
				*within = FALSE;
				return bad;
			}
		}
		return bad;
	}

	// how the surface at x,y holds the decode's sub rect in every format the snapshot can be read as; a check that is not final stops at the first bad row
	VOID XferGuard(const Record* twin, LONG x, LONG y, const RECT* sub, BOOL final, GuardResult* out)
	{
		LONG width = sub->right - sub->left, height = sub->bottom - sub->top;
		MemoryZero(out, sizeof(GuardResult));
		out->format = GuardFormats;
		out->badY = height;

		DWORD map[256];
		for (DWORD f = 0; f < GuardFormats; ++f)
		{
			out->leads[f] = out->matches[f] = -1;
			BOOL usable = f <= GuardBgr ? (xfer.layouts & Layout32) != 0 : f == Guard565 ? (xfer.layouts & Layout16) != 0 : f == GuardIndexed ? (xfer.layouts & Layout8) != 0 : (xfer.layouts & Layout8) && xfer.paletteSet;
			if (!usable)
				continue;

			MemoryZero(map, sizeof(map));
			DWORD firstSeen = 0;
			BOOL varied = FALSE;
			LONG lead = 0, rows = 0, badX = 0, badY = -1;
			DWORD got = 0, want = 0;
			for (LONG row = 0; row < height; ++row)
			{
				const DWORD* have = (const DWORD*)twin->base + (y + row) * twin->pitch + x;
				BOOL within;
				DWORD expect = 0;
				LONG bad = GuardRow(twin, f, have, sub->left, sub->top + row, width, final, &within, map, &firstSeen, &varied, &expect);
				if (bad == width)
				{
					if (badY < 0)
						++lead;
					++rows;
					continue;
				}
				if (badY < 0)
				{
					badY = row;
					badX = bad;
					got = have[bad];
					want = expect;
				}
				if (within)
					++rows;
				if (!final)
					break;
			}
			// one colour for every index proves nothing
			if (f == GuardIndexed && !varied)
				lead = rows = 0;

			out->leads[f] = lead;
			out->matches[f] = rows;
			if (out->format == GuardFormats || lead > out->lead || (lead == out->lead && rows > out->rows))
			{
				out->format = f;
				out->lead = lead;
				out->rows = rows;
				out->badX = badX;
				out->badY = badY < 0 ? height : badY;
				out->got = got;
				out->want = want;
			}
		}

		// zeroed memory matches zero bytes of a decode in some readings; a surface no one wrote yet confirms nothing
		if (!out->lead && !final)
			return;
		out->blank = TRUE;
		for (LONG row = 0; row < height && out->blank; ++row)
		{
			const DWORD* have = (const DWORD*)twin->base + (y + row) * twin->pitch + x;
			for (LONG i = 0; i < width; ++i)
				if (have[i])
				{
					out->blank = FALSE;
					break;
				}
		}
	}

	// 0 rejected, 1 exact in some format, 2 tolerant: a fresh surface of the decode's size where XFER_TOLERANT_PCT of the rows match within XFER_TOLERANCE
	DWORD GuardVerdict(const GuardResult* g, LONG height, BOOL strong, const CHAR** reason, DWORD* format)
	{
		*format = g->format;
		if (g->blank)
		{
			*reason = "the surface is still blank there";
			return 0;
		}
		if (g->format == GuardFormats)
		{
			*reason = "the decode snapshot has no readable layout";
			return 0;
		}
		if (g->lead == height)
			return 1;

		DWORD best = GuardFormats;
		for (DWORD f = Guard565; f < GuardFormats; ++f)
			if (g->matches[f] >= 0 && (best == GuardFormats || g->matches[f] > g->matches[best]))
				best = f;
		BOOL enough = best != GuardFormats && g->matches[best] * 100 >= height * XFER_TOLERANT_PCT;
		if (enough && strong)
		{
			*format = best;
			return 2;
		}
		if (enough)
			++xfer.weakTolerant;
		*reason = enough ? "most rows match, but only a fresh surface of the decode's size may pass the tolerant compare" : "no format holds the decode there";
		return 0;
	}

	VOID GuardText(CHAR* text, const GuardResult* g)
	{
		StrPrint(text, "rgb32 %d bgr32 %d 565 %d (near %d) idx %d pal %d", (INT)g->leads[GuardRgb], (INT)g->leads[GuardBgr], (INT)g->leads[Guard565], (INT)g->matches[Guard565],
			(INT)g->leads[GuardIndexed], (INT)g->leads[GuardPalette]);
	}

	VOID XferLogReject(const CHAR* via, const CHAR* reason, const Record* twin, DWORD id, LONG x, LONG y, const GuardResult* g, LONG height)
	{
		if (xfer.failLogs >= XFER_FAIL_LOGS)
			return;
		++xfer.failLogs;
		CHAR text[160];
		GuardText(text, g);
		DebugLog("twin seed transfer rejected via %s: %s | decode %.32s %dx%d (#%u, %u frames ago, format byte %u) -> twin %u %08X %dx%d at %d,%d | best %s: leading rows %d, matching rows %d of %d, px %d,%d holds %08X, decode %08X | leading rows per format: %s | lock caller %08X<%08X locks %u",
			via, reason, xfer.name, xfer.width, xfer.height, xfer.seq, game.frameNo - xfer.frameNo, xfer.trueColor, id, (DWORD)twin->owner, twin->width, twin->height, x, y,
			g->format < GuardFormats ? guardNames[g->format] : "none", g->lead, g->rows, height, g->badX, g->badY, g->got, g->want, text,
			twin->arrive.lockCaller, twin->arrive.lockOuter, twin->arrive.locks);
	}

	DWORD XferSeed(Record* twin, DWORD id, LONG x, LONG y, const RECT* sub)
	{
		RECT rect = { x, y, x + sub->right - sub->left, y + sub->bottom - sub->top };
		++seeds.kinds[xfer.kind].events;
		if (!sub->left && !sub->top && sub->right == xfer.width && sub->bottom == xfer.height)
			return SeedAt(twin, id, &rect, twin->pitch, (SeedKind)xfer.kind, xfer.name, xfer.pieces, xfer.count, xfer.conflicts, xfer.conflictCount, TRUE);

		// part of the decode: its pieces clipped to that part, in the part's coordinates
		if (xfer.count > xfer.subCap)
		{
			if (xfer.subPieces)
				MemoryFree(xfer.subPieces);
			xfer.subPieces = (SeedPiece*)MemoryAlloc(xfer.count * sizeof(SeedPiece));
			xfer.subCap = xfer.subPieces ? xfer.count : 0;
		}
		DWORD n = 0;
		for (DWORD i = 0; i < xfer.count && xfer.subPieces; ++i)
		{
			SeedPiece p = xfer.pieces[i];
			if (!IntersectRect(&p.to, &xfer.pieces[i].to, sub))
				continue;
			OffsetRect(&p.to, -sub->left, -sub->top);
			OffsetRect(&p.place, -sub->left, -sub->top);
			xfer.subPieces[n++] = p;
		}
		return n ? SeedAt(twin, id, &rect, twin->pitch, (SeedKind)xfer.kind, xfer.name, xfer.subPieces, n, NULL, 0, TRUE) : 0;
	}

	VOID XferLogAccept(const CHAR* via, const Record* twin, DWORD id, LONG x, LONG y, const RECT* sub, DWORD outcome, const GuardResult* g, DWORD verdict, DWORD format)
	{
		if (xfer.logs >= XFER_LOGS)
			return;
		++xfer.logs;
		DebugLog("twin seed transfer #%u via %s: decode %.32s %dx%d part %d,%d %dx%d (#%u, %u frames ago, seeded into %08X) -> twin %u %08X %dx%d at %d,%d | %s | guard %s %s (leading rows %d, matching rows %d of %d, format byte %u) | lock caller %08X<%08X",
			xfer.logs, via, xfer.name, xfer.width, xfer.height, sub->left, sub->top, sub->right - sub->left, sub->bottom - sub->top, xfer.seq, game.frameNo - xfer.frameNo, (DWORD)xfer.target,
			id, (DWORD)twin->owner, twin->width, twin->height, x, y, outcome == 1 ? "seeded" : outcome == 2 ? "seeded, art deferred" : "nothing recorded (repeat or no piece)",
			verdict == 2 ? "tolerant" : "exact", guardNames[format], g->lead, format < GuardFormats ? g->matches[format] : 0, sub->bottom - sub->top, xfer.trueColor,
			twin->arrive.lockCaller, twin->arrive.lockOuter);
	}

	VOID XferCount(DWORD verdict, DWORD format)
	{
		if (format >= GuardFormats)
			return;
		if (verdict == 2)
			++xfer.tolerant[format];
		else
			++xfer.exact[format];
	}

	enum XferResult { XferSkip, XferDone, XferPartial, XferFailed };

	// the provenance (the latest decode, the lock, write or first trapped write that followed it) picks the image and its place; the pixel compare only confirms it
	XferResult XferTry(Record* twin, DWORD id, const POINT* at, DWORD atCount, const CHAR* via, DWORD* by, BOOL final, BOOL fresh, LONG* doneRows, BOOL quiet = FALSE)
	{
		++xfer.tries;
		if (!xfer.valid)
		{
			++xfer.noDecode;
			return XferSkip;
		}
		if (twin->arrive.xferSeq == xfer.seq)
		{
			++xfer.consumed;
			return XferSkip;
		}
		if (xfer.target == twin->owner)
		{
			++xfer.direct;
			return XferSkip;
		}
		if (game.frameNo - xfer.frameNo > XFER_FRAMES)
		{
			++xfer.stale;
			return XferSkip;
		}

		RECT whole = { 0, 0, xfer.width, xfer.height };
		// a tolerant compare is only for a surface made for this decode, judged once it is written
		BOOL strong = fresh && final && twin->height == xfer.height && (twin->width == xfer.width || twin->width == xfer.width + 1);
		GuardResult best, g;
		MemoryZero(&best, sizeof(best));
		DWORD bestK = 0, verdict = 0, format = GuardFormats;
		const CHAR* reason = "no format holds the decode there";
		BOOL fitted = FALSE;
		for (DWORD k = 0; k < atCount && !verdict; ++k)
		{
			if (at[k].x < 0 || at[k].y < 0 || at[k].x + xfer.width > twin->width || at[k].y + xfer.height > twin->height)
				continue;
			XferGuard(twin, at[k].x, at[k].y, &whole, final, &g);
			const CHAR* why = NULL;
			DWORD f = GuardFormats;
			DWORD v = GuardVerdict(&g, xfer.height, strong, &why, &f);
			if (!fitted || v || g.lead > best.lead)
			{
				best = g;
				bestK = k;
				format = f;
				if (why)
					reason = why;
			}
			fitted = TRUE;
			verdict = v;
		}
		if (!fitted)
		{
			++xfer.noFit;
			return XferSkip;
		}

		if (!verdict)
		{
			// a watched surface may not be written yet; it is judged at the next decode or present
			if (!final)
			{
				if (doneRows)
					*doneRows = best.lead;
				return XferPartial;
			}
			if (doneRows && *doneRows)
				++xfer.partialExpired;
			if (best.blank)
				++xfer.blank;
			++twin->arrive.guardFails;
			if (twin->arrive.failSeq != xfer.seq)
			{
				twin->arrive.failSeq = xfer.seq;
				++xfer.guardFails;
				if (!quiet)
					XferLogReject(via, reason, twin, id, at[bestK].x, at[bestK].y, &best, xfer.height);
			}
			return XferFailed;
		}

		Sync();
		twin->arrive.xferSeq = xfer.seq;
		++twin->arrive.transfers;
		++*by;
		XferCount(verdict, format);
		DWORD outcome = XferSeed(twin, id, at[bestK].x, at[bestK].y, &whole);
		XferLogAccept(via, twin, id, at[bestK].x, at[bestK].y, &whole, outcome, &best, verdict, format);
		return XferDone;
	}

	// the ground atlas and other big buffers are locked constantly; such an unlock is only a candidate when its lock rect is the decode's size
	BOOL XferLarge(const Record* twin, DWORD id, const CHAR* via)
	{
		const Arrival* a = &twin->arrive;
		if ((LONGLONG)twin->width * twin->height <= (LONGLONG)XFER_LARGE * xfer.width * xfer.height)
			return FALSE;
		if (a->lockRectSet && a->lockRect.right - a->lockRect.left == xfer.width && a->lockRect.bottom - a->lockRect.top == xfer.height)
			return FALSE;

		++xfer.unlockLarge;
		if (xfer.largeLogs < XFER_LARGE_LOGS)
		{
			++xfer.largeLogs;
			DebugLog("twin seed transfer rejected via %s: twin %u %08X %dx%d is over %ux the decode %.32s %dx%d (#%u) and its lock rect %s | lock caller %08X<%08X locks %u",
				via, id, (DWORD)twin->owner, twin->width, twin->height, XFER_LARGE, xfer.name, xfer.width, xfer.height, xfer.seq,
				a->lockRectSet ? "is another size" : "covers the whole surface", a->lockCaller, a->lockOuter, a->locks);
		}
		return TRUE;
	}

	VOID XferAdd(XferWatch* list, DWORD* count, DWORD limit, Record* twin, DWORD id, LONG x, LONG y, DWORD seq, DWORD via)
	{
		for (DWORD i = 0; i < *count; ++i)
			if (list[i].owner == twin->owner)
			{
				// a trapped first write is the more exact provenance of the two
				if (via == ViaTrap && list[i].via != ViaTrap)
				{
					list[i].via = ViaTrap;
					list[i].x = x;
					list[i].y = y;
					if (seq)
						list[i].seq = seq;
				}
				return;
			}

		XferWatch* w = NULL;
		if (*count < limit)
			w = &list[(*count)++];
		else
		{
			// surfaces made for text and the like crowd the list; the newest unbound one is the likely texture
			for (DWORD i = 0; i < *count; ++i)
				if (!list[i].seq && (!w || list[i].frameNo < w->frameNo))
					w = &list[i];
			if (!w)
				return;
		}
		w->owner = twin->owner;
		w->id = id;
		w->x = x;
		w->y = y;
		w->seq = seq;
		w->rows = 0;
		w->frameNo = game.frameNo;
		w->via = via;
	}

	const CHAR* XferViaName(DWORD via, BOOL inside)
	{
		switch (via)
		{
		case ViaCreated:
			return "surface created before the decode";
		case ViaTrap:
			return inside ? "first write into a fresh surface inside the decode call (trap)" : "first write into a fresh surface after the decode (trap)";
		case ViaUnlock:
			return "unlock inside the decode call";
		default:
			return inside ? "hooked write inside the decode call" : "hooked write";
		}
	}

	DWORD* XferViaCounter(DWORD via, BOOL inside)
	{
		return via == ViaTrap ? &xfer.byTrap : inside ? &xfer.byInside : via == ViaCreated ? &xfer.byCreated : &xfer.byWrite;
	}

	// a hooked write announces itself before the CPU writes, so the surface is checked at the next twin call
	VOID XferWatchWrite(Record* twin, DWORD id, const RECT* rect)
	{
		if (twin->memory || !SeedMode())
			return;
		// inside the decode call the decode it belongs to is not captured yet
		if (xfer.depth)
		{
			XferAdd(xfer.inside, &xfer.insideCount, XFER_INSIDE, twin, id, rect->left, rect->top, 0, ViaWrite);
			return;
		}
		if (!xfer.valid || twin->arrive.xferSeq == xfer.seq || twin->arrive.watchFailSeq == xfer.seq || xfer.target == twin->owner || game.frameNo - xfer.frameNo > XFER_FRAMES)
			return;
		DWORD before = xfer.watchCount;
		XferAdd(xfer.watch, &xfer.watchCount, XFER_WATCH, twin, id, rect->left, rect->top, xfer.seq, ViaWrite);
		if (xfer.watchCount != before)
			++xfer.watches;
	}

	// the game creates the surface for a texture, then decodes the art into scratch memory and writes it into the surface itself; the next decode is that surface's content
	VOID XferWatchCreated(Record* twin, DWORD id)
	{
		if (twin->memory || !SeedMode())
			return;
		DWORD before = xfer.watchCount;
		XferAdd(xfer.watch, &xfer.watchCount, XFER_WATCH, twin, id, 0, 0, 0, ViaCreated);
		if (xfer.watchCount != before)
			++xfer.createdWatches;
	}

	VOID XferBind()
	{
		for (DWORD i = 0; i < xfer.watchCount;)
		{
			XferWatch* w = &xfer.watch[i];
			if (!w->seq && xfer.valid)
			{
				w->seq = xfer.seq;
				w->rows = 0;
				++i;
			}
			else if (w->seq)
				++i;
			else
				xfer.watch[i] = xfer.watch[--xfer.watchCount];
		}
	}

	VOID XferResolve(BOOL final)
	{
		static BOOL resolving;
		if (!xfer.watchCount || resolving)
			return;
		resolving = TRUE;
		for (DWORD i = 0; i < xfer.watchCount;)
		{
			XferWatch* w = &xfer.watch[i];
			Record* twin = &game.twins[w->id];
			XferResult result = XferSkip;
			if (twin->owner == w->owner && !w->seq && game.frameNo - w->frameNo <= XFER_UNBOUND_FRAMES)
				result = XferPartial;
			else if (twin->owner == w->owner && w->seq == xfer.seq)
			{
				POINT at = { w->x, w->y };
				// several surfaces can be made before one decode; only those of the decode's size (even width) are worth a log line when they miss
				BOOL sized = twin->height == xfer.height && (twin->width == xfer.width || twin->width == xfer.width + 1);
				result = XferTry(twin, w->id, &at, 1, XferViaName(w->via, FALSE), XferViaCounter(w->via, FALSE), final, w->via == ViaCreated || w->via == ViaTrap, &w->rows,
					w->via == ViaCreated && !sized && !w->rows);
				if (result == XferFailed)
					twin->arrive.watchFailSeq = xfer.seq;
			}
			if (result == XferPartial)
				++i;
			else
				xfer.watch[i] = xfer.watch[--xfer.watchCount];
		}
		resolving = FALSE;
	}

	DWORD DecodeLogNumber()
	{
		if (xfer.namedLogs >= DECODE_LOGS)
			return 0;
		return ++xfer.namedLogs;
	}

	VOID ChainText(const DWORD* chain, DWORD count, CHAR* text)
	{
		CHAR* at = text;
		*at = 0;
		for (DWORD i = 0; i < count; ++i)
		{
			CHAR name[48];
			TrapName(chain[i], name);
			// the address alone repeats in the module+offset, which is what the next round disassembles
			const CHAR* space = StrChar(name, ' ');
			at += StrPrint(at, "%s%s", i ? " < " : "", space ? space + 1 : name);
		}
		if (!count)
			StrCopy(text, "-");
	}

	// the latest store decode named the image but has no 4K for it: its blit is named, with the reason
	BOOL XferArtless(const BYTE* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height)
	{
		if (xfer.valid || !xfer.artSrc || srcPitch != xfer.artSrcPitch || src < xfer.artSrc || game.frameNo - xfer.frameNo > 2)
			return FALSE;
		LONG offset = LONG(src - xfer.artSrc);
		LONG sx = offset % srcPitch, sy = offset / srcPitch;
		if (sx + width > xfer.width || sy + height > xfer.height)
			return FALSE;

		++xfer.indexedArtless;
		if (xfer.artlessLogs < ARTLESS_LOGS)
		{
			++xfer.artlessLogs;
			CHAR to[96];
			RegText((DWORD)dst, to);
			DebugLog("twin decode without 4K #%u: %.32s %dx%d (decode #%u) blitted at %d,%d %dx%d -> %s | no 4K: %s", xfer.artlessLogs, xfer.name, xfer.width, xfer.height, xfer.seq,
				sx, sy, width, height, to, xfer.artReason ? xfer.artReason : "-");
		}
		return TRUE;
	}

	// a whole image converted from indices no store decode named, of exactly the size of a .ff record the game just read: that read names it
	BOOL XferRecordSeed(const DWORD* dst, LONG dstPitch, LONG width, LONG height, const DWORD* chain, DWORD chainCount)
	{
		ImageReplace::RecordName rn;
		if (!ImageReplace::RecordDecode(width, height, &rn))
			return FALSE;

		++xfer.recordMatches;
		// kept in the code dump as well, so the next round can check the loader the read named
		for (DWORD k = 0; k < chainCount; ++k)
			CodeDump(chain[k], 0x400, "named blit caller");
		DWORD id;
		LONG x = -1, y = -1;
		Record* twin = FindAddress(dst, &id);
		BOOL fits = twin && PixelPos(twin, dst, dstPitch, width, height, &x, &y);
		SeedKind kind = rn.sheet ? SeedSheet : SeedStore;
		DWORD outcome = 0;
		if (!rn.count)
			++xfer.recordNoArt;
		else if (!fits)
			++xfer.recordNoFit;
		else
		{
			Sync();
			RECT at = { x, y, x + width, y + height };
			++seeds.kinds[kind].events;
			outcome = SeedAt(twin, id, &at, twin->pitch, kind, rn.name, rn.pieces, rn.count, rn.conflicts, rn.conflictCount, TRUE);
			if (outcome)
				++xfer.recordSeeded;
			if (outcome == 2)
				++xfer.recordDeferred;
		}

		DWORD n = DecodeLogNumber();
		if (n)
		{
			CHAR reader[48], outer[48], blit[200];
			TrapName(rn.caller, reader);
			TrapName(rn.outer, outer);
			ChainText(chain, chainCount, blit);
			DebugLog("twin decode named #%u: %.40s %dx%d record %u (%s) %s | loader: read by %s < %s, converted by %s | found by the record read %u frames before its indexed blit (%u unread reads of this size) | -> twin %d %s at %d,%d | %s",
				n, rn.name, width, height, rn.record, rn.archive, rn.sheet ? "sheet" : "plain", reader, outer, blit, rn.framesAgo, rn.candidates, twin ? (INT)id : -1, twin ? TwinKind(twin) : "none", x, y,
				!rn.count ? (rn.reason ? rn.reason : "no piece") : !fits ? "no twin fits the blit" : outcome == 2 ? "seeded, art deferred" : outcome ? "seeded" : "nothing recorded (repeat)");
		}
		return TRUE;
	}

	// returns TRUE for a size and chain not seen before
	BOOL UnnamedNote(LONG width, LONG height, const DWORD* chain, DWORD chainCount)
	{
		++unnamed.total;
		++unnamed.interval;
		for (DWORD i = 0; i < unnamed.count; ++i)
		{
			UnnamedDecode* u = &unnamed.list[i];
			if (u->w == width && u->h == height && u->chainCount == chainCount && !MemoryCompare(u->chain, chain, chainCount * sizeof(DWORD)))
			{
				++u->count;
				u->frameNo = game.frameNo;
				return FALSE;
			}
		}
		if (unnamed.count == UNNAMED_SIZES)
		{
			++unnamed.full;
			return FALSE;
		}
		UnnamedDecode* u = &unnamed.list[unnamed.count++];
		u->w = width;
		u->h = height;
		u->count = 1;
		u->chainCount = chainCount;
		MemoryCopy(u->chain, chain, chainCount * sizeof(DWORD));
		u->frameNo = game.frameNo;
		for (DWORD k = 0; k < chainCount; ++k)
			CodeDump(chain[k], 0x400, "indexed blit caller");
		return TRUE;
	}

	VOID LogUnnamed()
	{
		DWORD order[UNNAMED_SIZES];
		for (DWORD i = 0; i < unnamed.count; ++i)
		{
			DWORD k = i;
			while (k && unnamed.list[order[k - 1]].count < unnamed.list[i].count)
			{
				order[k] = order[k - 1];
				--k;
			}
			order[k] = i;
		}
		CHAR text[720];
		CHAR* at = text;
		*at = 0;
		for (DWORD i = 0; i < unnamed.count && i < 5; ++i)
		{
			const UnnamedDecode* u = &unnamed.list[order[i]];
			CHAR chain[200];
			ChainText(u->chain, min(u->chainCount, (DWORD)3), chain);
			at += StrPrint(at, " %dx%d x%u %u frames ago: %.80s;", u->w, u->h, u->count, game.frameNo - u->frameNo, chain);
		}
		DebugLog("twin decode unnamed since start: %u indexed blits (%u in 5s) of %u sizes and chains (table full %u) | named by a record read %u (seeded %u, deferred %u, no 4K %u, no twin %u), blits of named decodes without 4K %u | left:%s",
			unnamed.total, unnamed.interval, unnamed.count, unnamed.full, xfer.recordMatches, xfer.recordSeeded, xfer.recordDeferred, xfer.recordNoArt, xfer.recordNoFit, xfer.indexedArtless, *text ? text : " none");
		unnamed.interval = 0;
	}

	VOID XferIndexedTry(const BYTE* src, LONG srcPitch, const DWORD* palette, const DWORD* dst, LONG dstPitch, LONG width, LONG height, const DWORD* chain, DWORD chainCount)
	{
		if (xfer.valid && (xfer.layouts & Layout32) && (const BYTE*)dst >= xfer.rawData && (const BYTE*)dst < xfer.rawData + (xfer.rawTop + xfer.height) * xfer.rawPitch * 2)
		{
			++xfer.indexedIntoDecode;
			return;
		}
		LONG offset = xfer.rawData ? LONG(src - xfer.rawData) : -1;
		LONG sy = offset >= 0 && srcPitch > 0 ? offset / srcPitch - xfer.rawTop : -1;
		LONG sx = offset >= 0 && srcPitch > 0 ? offset % srcPitch - xfer.rawLeft : -1;
		if (!xfer.valid || !(xfer.layouts & Layout8) || srcPitch != xfer.rawPitch || sx < 0 || sy < 0 || sx + width > xfer.width || sy + height > xfer.height)
		{
			++xfer.indexedOther;
			if (XferArtless(src, srcPitch, dst, dstPitch, width, height))
				return;
			// indices that are not the latest store decode: the big images come this way, from a loader that reads the .ff record itself
			if (XferRecordSeed(dst, dstPitch, width, height, chain, chainCount))
				return;
			BOOL fresh = UnnamedNote(width, height, chain, chainCount);
			if (game.indexedOtherLogs < INDEXED_OTHER_LOGS || (fresh && unnamed.logs < UNNAMED_LOGS))
			{
				if (game.indexedOtherLogs < INDEXED_OTHER_LOGS)
					++game.indexedOtherLogs;
				else
					++unnamed.logs;
				CHAR from[96], to[96], text[200];
				RegText((DWORD)src, from);
				RegText((DWORD)dst, to);
				ChainText(chain, chainCount, text);
				DebugLog("twin indexed blit #%u from other memory: src %s pitch %d %dx%d -> %s pitch %d | latest decode #%u %.32s %dx%d raw %08X pitch %d (at %d,%d of it) %u frames ago | no record read of this size | chain %s",
					game.indexedOtherLogs + unnamed.logs, from, srcPitch, width, height, to, dstPitch, xfer.seq, xfer.name[0] ? xfer.name : "-", xfer.width, xfer.height, xfer.rawData, xfer.rawPitch, sx, sy,
					game.frameNo - xfer.frameNo, text);
			}
			return;
		}

		++xfer.indexedFromDecode;
		MemoryCopy(xfer.palette, palette, sizeof(xfer.palette));
		xfer.paletteSet = TRUE;
		DWORD id;
		LONG x, y;
		Record* twin = FindAddress(dst, &id);
		if (!twin || twin->memory || !PixelPos(twin, dst, dstPitch, width, height, &x, &y))
		{
			++xfer.indexedNoTwin;
			return;
		}

		RECT sub = { sx, sy, sx + width, sy + height };
		BOOL whole = !sx && !sy && width == xfer.width && height == xfer.height;
		if (!whole)
			++xfer.indexedPartial;
		if (whole && twin->arrive.xferSeq == xfer.seq)
		{
			++xfer.consumed;
			return;
		}
		if (xfer.target == twin->owner)
		{
			++xfer.direct;
			return;
		}

		const CHAR* via = "indexed blit from the decode";
		GuardResult g;
		XferGuard(twin, x, y, &sub, TRUE, &g);
		const CHAR* reason = NULL;
		DWORD format = GuardFormats;
		DWORD verdict = GuardVerdict(&g, height, TRUE, &reason, &format);
		if (!verdict)
		{
			++twin->arrive.guardFails;
			++xfer.guardFails;
			XferLogReject(via, reason, twin, id, x, y, &g, height);
			return;
		}

		Sync();
		if (whole)
			twin->arrive.xferSeq = xfer.seq;
		++twin->arrive.transfers;
		++xfer.byIndexed;
		XferCount(verdict, format);
		DWORD outcome = XferSeed(twin, id, x, y, &sub);
		XferLogAccept(via, twin, id, x, y, &sub, outcome, &g, verdict, format);
	}

	VOID OnIndexedBlit(const BYTE* src, LONG srcPitch, const DWORD* palette, const DWORD* dst, LONG dstPitch, LONG width, LONG height, const DWORD* slot)
	{
		Timed timed("OnIndexedBlit");
		if (!src || !dst || !palette || width <= 0 || height <= 0 || !Active())
			return;
		DWORD noteId;
		Record* noted = FindAddress(dst, &noteId);
		if (noted && noted->arrive.noteHead)
		{
			WriteNote* n = &noted->arrive.notes[(noted->arrive.noteHead - 1) % WRITE_RING];
			if (n->kind == NoteWrite)
			{
				n->kind = NoteIndexed;
				n->src = (DWORD)src;
			}
		}
		if (!SeedMode())
			return;

		++xfer.indexed;
		// inside the decode call its snapshot is not taken yet; the blit is judged when the call returns
		if (xfer.depth)
		{
			if (xfer.indexedInsideCount == XFER_INDEXED_INSIDE)
			{
				++xfer.indexedInsideDropped;
				return;
			}
			XferIndexed* b = &xfer.indexedInside[xfer.indexedInsideCount++];
			b->src = src;
			b->srcPitch = srcPitch;
			b->dst = dst;
			b->dstPitch = dstPitch;
			b->width = width;
			b->height = height;
			MemoryCopy(b->palette, palette, sizeof(b->palette));
			b->chainCount = WalkFrames(slot, b->chain, INDEXED_CHAIN);
			return;
		}
		DWORD chain[INDEXED_CHAIN];
		DWORD chainCount = WalkFrames(slot, chain, INDEXED_CHAIN);
		XferIndexedTry(src, srcPitch, palette, dst, dstPitch, width, height, chain, chainCount);
	}

	VOID DecodeBegin()
	{
		++xfer.depth;
	}

	VOID DecodeEnd()
	{
		if (xfer.depth)
			--xfer.depth;
		if (xfer.depth)
			return;
		xfer.srcSet = FALSE;
		DWORD count = xfer.insideCount, blits = xfer.indexedInsideCount;
		xfer.insideCount = 0;
		xfer.indexedInsideCount = 0;
		if ((!count && !blits) || !Active())
			return;

		Sync();
		for (DWORD i = 0; i < count; ++i)
		{
			const XferWatch* w = &xfer.inside[i];
			Record* twin = &game.twins[w->id];
			if (twin->owner != w->owner)
				continue;
			if (w->via == ViaUnlock && xfer.valid && XferLarge(twin, w->id, XferViaName(w->via, TRUE)))
				continue;
			POINT at[2] = { { w->x, w->y }, { 0, 0 } };
			XferTry(twin, w->id, at, w->x || w->y ? 2 : 1, XferViaName(w->via, TRUE), XferViaCounter(w->via, TRUE), TRUE, w->via == ViaTrap, NULL);
		}
		for (DWORD i = 0; i < blits; ++i)
		{
			const XferIndexed* b = &xfer.indexedInside[i];
			XferIndexedTry(b->src, b->srcPitch, b->palette, b->dst, b->dstPitch, b->width, b->height, b->chain, b->chainCount);
		}
	}

	VOID OnLock(StateBuffer* buffer, const RECT* rect, DWORD caller, DWORD outer)
	{
		if (!buffer || !Active())
			return;

		++xfer.locks;
		DWORD id;
		Record* twin = Find(buffer, &id);
		if (!twin)
			return;

		Arrival* a = &twin->arrive;
		++a->locks;
		a->lockCaller = caller;
		a->lockOuter = outer;
		a->lockSeq = xfer.seq;
		a->lockRectSet = rect != NULL;
		if (rect)
			a->lockRect = *rect;
	}

	VOID OnUnlock(StateBuffer* buffer)
	{
		Timed timed("OnUnlock");
		if (!buffer || !Active())
			return;

		++xfer.unlocks;
		DWORD id;
		Record* twin = Find(buffer, &id);
		if (!twin)
		{
			++xfer.unlocksUntwinned;
			return;
		}

		++twin->arrive.unlocks;
		if (twin->memory)
		{
			++xfer.unlocksMemory;
			return;
		}
		// the decode that fills it has not been seen yet; it is checked when the decode call returns
		if (xfer.depth)
		{
			++xfer.unlocksInside;
			const Arrival* a = &twin->arrive;
			XferAdd(xfer.inside, &xfer.insideCount, XFER_INSIDE, twin, id, a->lockRectSet ? a->lockRect.left : 0, a->lockRectSet ? a->lockRect.top : 0, 0, ViaUnlock);
			return;
		}
		if (!xfer.valid || twin->arrive.xferSeq == xfer.seq)
			return;

		const Arrival* a = &twin->arrive;
		BOOL during = a->lockSeq != xfer.seq;
		const CHAR* via = during ? "unlock, decoded while locked" : "unlock, decoded before the lock";
		if (XferLarge(twin, id, via))
			return;
		POINT at[2] = { { a->lockRectSet ? a->lockRect.left : 0, a->lockRectSet ? a->lockRect.top : 0 }, { 0, 0 } };
		XferTry(twin, id, at, at[0].x || at[0].y ? 2 : 1, via, during ? &xfer.byDuring : &xfer.byBefore, TRUE, FALSE, NULL);
	}

	VOID LogTransfers()
	{
		DebugLog("twin seed transfer since start: decodes %u (named %u, snapshot failed %u; BlitObject format byte 0: %u, 1: %u, other %u, no StoreDraw source %u) | locks %u unlocks %u (untwinned %u, memory twins %u, inside a decode %u) | tries %u: no decode %u, already taken %u, decoded there %u, stale %u, does not fit %u",
			xfer.decodes, xfer.named, xfer.snapshotFailed, xfer.formatBytes[0], xfer.formatBytes[1], xfer.formatBytes[2], xfer.formatBytes[3],
			xfer.locks, xfer.unlocks, xfer.unlocksUntwinned, xfer.unlocksMemory, xfer.unlocksInside, xfer.tries, xfer.noDecode, xfer.consumed, xfer.direct, xfer.stale, xfer.noFit);
		DebugLog("twin seed transfer done: unlock with the decode during the lock %u, before the lock %u | inside the decode call %u | hooked write %u (watched %u) | surface created before the decode %u (watched %u) | first write trapped %u (watched %u, late %u) | indexed blit from the decode %u | rejected %u surfaces (blank %u, partial at the end %u, mostly matching but not a fresh surface %u), unlocks of much larger surfaces %u",
			xfer.byDuring, xfer.byBefore, xfer.byInside, xfer.byWrite, xfer.watches, xfer.byCreated, xfer.createdWatches, xfer.byTrap, xfer.trapWatches, xfer.trapLate, xfer.byIndexed,
			xfer.guardFails, xfer.blank, xfer.partialExpired, xfer.weakTolerant, xfer.unlockLarge);
		DebugLog("twin seed transfer guard since start: exact rgb32 %u bgr32 %u 565 %u indices %u palette %u | tolerant 565 %u indices %u palette %u | indexed blits %u: from the decode %u (part of it %u, no surface twin %u), into the decode %u, other sources %u, dropped inside a decode %u",
			xfer.exact[GuardRgb], xfer.exact[GuardBgr], xfer.exact[Guard565], xfer.exact[GuardIndexed], xfer.exact[GuardPalette], xfer.tolerant[Guard565], xfer.tolerant[GuardIndexed], xfer.tolerant[GuardPalette],
			xfer.indexed, xfer.indexedFromDecode, xfer.indexedPartial, xfer.indexedNoTwin, xfer.indexedIntoDecode, xfer.indexedOther, xfer.indexedInsideDropped);
		LogUnnamed();
		CodeFlush();
	}
#pragma endregion

#pragma region Trap
	BOOL TrapWanted()
	{
		if (!trap.chosen)
		{
			trap.chosen = TRUE;
			trap.wanted = Config::Get("Twin", "WriteTrap", 1) != 0;
		}
		return trap.wanted;
	}

	VOID RenderBegin()
	{
		++trap.renderDepth;
	}

	VOID RenderEnd()
	{
		if (trap.renderDepth)
			--trap.renderDepth;
	}

	BOOL TrapSelf(DWORD address)
	{
		return address - trap.selfLo < trap.selfHi - trap.selfLo;
	}

	BOOL TrapExe(DWORD address)
	{
		return address - trap.exeLo < trap.exeHi - trap.exeLo;
	}

	// a stack value is only taken for a return address when it follows a call; saved registers often point into the modules' data
	BOOL TrapReturn(DWORD address)
	{
		BOOL self = TrapSelf(address);
		if (!self && !TrapExe(address))
			return FALSE;
		if (address - 6 < (self ? trap.selfLo : trap.exeLo))
			return FALSE;
		const BYTE* code = (const BYTE*)address;
		return code[-5] == 0xE8 || code[-6] == 0xFF || code[-3] == 0xFF || code[-2] == 0xFF;
	}

	VOID TrapModule(HMODULE module, DWORD* low, DWORD* high);

	VOID TrapModules()
	{
		if (!trap.exeHi)
		{
			TrapModule(GetModuleHandle(NULL), &trap.exeLo, &trap.exeHi);
			if (!trap.selfHi)
				TrapModule(hDllModule, &trap.selfLo, &trap.selfHi);
		}
	}

	DWORD WalkFrames(const DWORD* slot, DWORD* frames, DWORD count)
	{
		for (DWORD k = 0; k < count; ++k)
			frames[k] = 0;
		if (!slot || !count)
			return 0;
		TrapModules();
		const NT_TIB* tib = (const NT_TIB*)__readfsdword(0x18);
		DWORD high = (DWORD)tib->StackBase;
		if ((DWORD)slot < (DWORD)tib->StackLimit || (DWORD)slot + sizeof(DWORD) > high || ((DWORD)slot & 3))
			return 0;

		DWORD n = 0;
		if (TrapExe(slot[0]))
			frames[n++] = slot[0];
		// the game is not built with frame pointers everywhere: the chain starts at the first saved EBP above the slot that points further up and is followed by a return address into the game
		const DWORD* frame = NULL;
		for (DWORD i = 1; i < 256 && (DWORD)(slot + i + 2) <= high && !frame; ++i)
		{
			const DWORD* f = slot + i;
			if (f[0] > (DWORD)f && f[0] + 2 * sizeof(DWORD) <= high && !(f[0] & 3) && TrapExe(f[1]) && TrapReturn(f[1]))
				frame = f;
		}
		while (frame && n < count)
		{
			frames[n++] = frame[1];
			const DWORD* next = (const DWORD*)frame[0];
			if ((DWORD)next <= (DWORD)frame || (DWORD)next + 2 * sizeof(DWORD) > high || ((DWORD)next & 3) || !TrapExe(next[1]) || !TrapReturn(next[1]))
				break;
			frame = next;
		}
		return n;
	}

	VOID CodeDump(DWORD address, DWORD radius, const CHAR* why)
	{
		TrapModules();
		if (!TrapExe(address))
			return;
		DWORD lo = (address - min(radius, address - trap.exeLo)) & ~15u;
		DWORD hi = (min(address + radius, trap.exeHi) + 15) & ~15u;
		if (hi > trap.exeHi)
			hi = trap.exeHi;
		if (hi <= lo)
			return;
		for (DWORD i = 0; i < code.count; ++i)
		{
			if (lo >= code.lo[i] && hi <= code.hi[i])
				return;
			if (lo <= code.hi[i] && hi >= code.lo[i])
			{
				DWORD newLo = min(lo, code.lo[i]), newHi = max(hi, code.hi[i]);
				DWORD added = (newHi - newLo) - (code.hi[i] - code.lo[i]);
				if (code.bytes + added > CODE_LIMIT)
				{
					++code.refused;
					return;
				}
				code.bytes += added;
				code.lo[i] = newLo;
				code.hi[i] = newHi;
				code.dirty = TRUE;
				return;
			}
		}
		if (code.count == CODE_REGIONS || code.bytes + (hi - lo) > CODE_LIMIT)
		{
			++code.refused;
			return;
		}
		DWORD k = code.count++;
		code.lo[k] = lo;
		code.hi[k] = hi;
		lstrcpyn(code.why[k], why, sizeof(code.why[k]));
		code.bytes += hi - lo;
		code.dirty = TRUE;
	}

	// twin-code.bin: "DGLCODE1", region count, exe base, SizeOfImage, TimeDateStamp, then per region its address, size, file offset and a 20-byte reason, then the bytes
	VOID CodeFlush()
	{
		if (!code.dirty)
			return;
		code.dirty = FALSE;

		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, sizeof(path));
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, "twin-code.bin");
		HANDLE file = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file == INVALID_HANDLE_VALUE)
		{
			DebugLog("twin code dump: %s could not be written", path);
			return;
		}

		const BYTE* exe = (const BYTE*)GetModuleHandle(NULL);
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(exe + ((const IMAGE_DOS_HEADER*)exe)->e_lfanew);
		DWORD head[5] = { 0, code.count, (DWORD)exe, nt->OptionalHeader.SizeOfImage, nt->FileHeader.TimeDateStamp };
		DWORD written;
		WriteFile(file, "DGLCODE1", 8, &written, NULL);
		WriteFile(file, head + 1, 4 * sizeof(DWORD), &written, NULL);
		DWORD offset = 8 + 4 * sizeof(DWORD) + code.count * (3 * sizeof(DWORD) + sizeof(code.why[0]));
		for (DWORD i = 0; i < code.count; ++i)
		{
			DWORD entry[3] = { code.lo[i], code.hi[i] - code.lo[i], offset };
			WriteFile(file, entry, sizeof(entry), &written, NULL);
			WriteFile(file, code.why[i], sizeof(code.why[i]), &written, NULL);
			offset += entry[1];
		}
		CHAR text[CODE_REGIONS * 24 + 1];
		CHAR* at = text;
		*at = 0;
		for (DWORD i = 0; i < code.count; ++i)
		{
			DWORD size = code.hi[i] - code.lo[i];
			if (IsBadReadPtr((const VOID*)code.lo[i], size))
			{
				static BYTE zero[4096];
				for (DWORD left = size; left; left -= min(left, (DWORD)sizeof(zero)))
					WriteFile(file, zero, min(left, (DWORD)sizeof(zero)), &written, NULL);
			}
			else
				WriteFile(file, (const VOID*)code.lo[i], size, &written, NULL);
			if (at < text + sizeof(text) - 24 && i < 40)
				at += StrPrint(at, " %X+%X", code.lo[i] - (DWORD)exe, size);
		}
		CloseHandle(file);
		++code.writes;
		DebugLog("twin code dump #%u: twin-code.bin, %u regions, %u bytes (limit %u, refused %u), Discipl2.exe base %08X stamp %08X | exe offset+size:%s",
			code.writes, code.count, code.bytes, CODE_LIMIT, code.refused, (DWORD)exe, head[4], text);
	}

	// the first return address into the game above the wrapper's frames: the game code that asked for this write
	__declspec(noinline) DWORD GameCaller()
	{
		TrapModules();
		const DWORD* slot = (const DWORD*)_AddressOfReturnAddress();
		const NT_TIB* tib = (const NT_TIB*)__readfsdword(0x18);
		for (DWORD i = 0; i < 96 && (DWORD)(slot + i + 1) <= (DWORD)tib->StackBase; ++i)
		{
			DWORD value = slot[i];
			if (TrapExe(value) && TrapReturn(value))
				return value;
		}
		return 0;
	}

	VOID TrapName(DWORD address, CHAR* text);
	VOID RegText(DWORD value, CHAR* text);

	const CHAR* const noteNames[NoteKinds] = { "hooked write", "indexed blit", "copy from untwinned", "fill", "copy", "pixel op", "trapped game write", "whole surface" };

	VOID Note(Record* twin, DWORD kind, const RECT* rect, DWORD src)
	{
		Arrival* a = &twin->arrive;
		WriteNote* n = &a->notes[a->noteHead++ % WRITE_RING];
		n->kind = kind;
		n->rect = *rect;
		n->caller = kind == NoteTrap ? src : GameCaller();
		n->src = src;
		n->frameNo = game.frameNo;
	}

	// the latest write that reached the rect, newest first
	const WriteNote* LastNote(const Record* twin, const RECT* rect)
	{
		const Arrival* a = &twin->arrive;
		for (DWORD k = 0; k < WRITE_RING && k < a->noteHead; ++k)
		{
			const WriteNote* n = &a->notes[(a->noteHead - 1 - k) % WRITE_RING];
			RECT part;
			if (IntersectRect(&part, &n->rect, rect))
				return n;
		}
		return NULL;
	}

	VOID NoteText(CHAR* text, const WriteNote* n)
	{
		if (!n || n->kind >= NoteKinds)
		{
			StrCopy(text, "no write recorded");
			return;
		}
		CHAR caller[48];
		TrapName(n->caller, caller);
		StrPrint(text, "%s %d,%d %dx%d src %08X caller %s frame %u", noteNames[n->kind < NoteKinds ? n->kind : 0], n->rect.left, n->rect.top, n->rect.right - n->rect.left,
			n->rect.bottom - n->rect.top, n->src, caller, n->frameNo);
	}

	// first in the vectored chain, so before any frame handler and the crash filter; only a write into a trapped surface page is handled, every other exception goes on to them.
	// It may run on any thread: no allocation, no lock, no logging, only VirtualProtect and the preallocated ring
	LONG __stdcall TrapHandler(EXCEPTION_POINTERS* info)
	{
		const EXCEPTION_RECORD* record = info->ExceptionRecord;
		if (!trap.on || record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record->NumberParameters < 2 || record->ExceptionInformation[0] != 1)
			return EXCEPTION_CONTINUE_SEARCH;

		DWORD addr = (DWORD)record->ExceptionInformation[1];
		DWORD id = TWIN_NONE;
		const BYTE* start = NULL;
		for (DWORD i = 0; i < TWIN_MAX; ++i)
		{
			const BYTE* s = trap.ranges[i].start;
			if (s && addr - (DWORD)s < trap.ranges[i].bytes)
			{
				id = i;
				start = s;
				break;
			}
		}
		if (id == TWIN_NONE)
			return EXCEPTION_CONTINUE_SEARCH;

		DWORD old = 0;
		if (!VirtualProtect((VOID*)(addr & ~(TRAP_PAGE - 1)), TRAP_PAGE, PAGE_READWRITE, &old))
			return EXCEPTION_CONTINUE_SEARCH;

		DWORD tid = GetCurrentThreadId();
		if (old != PAGE_READONLY)
		{
			// another thread opened the page first; a write that keeps faulting on an open page is a real crash
			InterlockedIncrement(&trap.retried);
			DWORD slot = tid & 63;
			if (trap.retry[slot].tid == tid && trap.retry[slot].addr == addr)
			{
				if (InterlockedIncrement(&trap.retry[slot].count) > TRAP_RETRIES)
					return EXCEPTION_CONTINUE_SEARCH;
			}
			else
			{
				trap.retry[slot].tid = tid;
				trap.retry[slot].addr = addr;
				trap.retry[slot].count = 1;
			}
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		InterlockedIncrement(&trap.faults);
		InterlockedIncrement(&trap.ranges[id].faults);

		const CONTEXT* context = info->ContextRecord;
		DWORD eip = context->Eip;
		const NT_TIB* tib = (const NT_TIB*)__readfsdword(0x18);
		DWORD low = context->Esp, high = (DWORD)tib->StackBase;
		BOOL stack = low >= (DWORD)tib->StackLimit && low < high && !(low & 3);
		DWORD caller = 0;
		for (DWORD i = 0; stack && i < TRAP_STACK_SCAN && low + i * sizeof(DWORD) + sizeof(DWORD) <= high; ++i)
		{
			DWORD value = ((const DWORD*)low)[i];
			if (TrapReturn(value))
			{
				caller = value;
				break;
			}
		}

		// the wrapper writing its own surfaces: its own code, or a CRT copy it called
		if (TrapSelf(eip) || (!TrapExe(eip) && TrapSelf(caller)))
		{
			InterlockedIncrement(&trap.own);
			InterlockedIncrement(&trap.ranges[id].own);
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		InterlockedIncrement(&trap.foreign);
		LONG slot = InterlockedIncrement(&trap.head) - 1;
		if ((DWORD)slot - trap.tail >= TRAP_RING)
		{
			InterlockedIncrement(&trap.dropped);
			return EXCEPTION_CONTINUE_EXECUTION;
		}

		TrapEvent* e = &trap.ring[(DWORD)slot % TRAP_RING];
		e->eip = eip;
		e->addr = addr;
		e->caller = caller;
		e->regs[RegEax] = context->Eax;
		e->regs[RegEcx] = context->Ecx;
		e->regs[RegEdx] = context->Edx;
		e->regs[RegEbx] = context->Ebx;
		e->regs[RegEsi] = context->Esi;
		e->regs[RegEdi] = context->Edi;
		// the first bytes of the faulting instruction tell a string copy (rep movs) apart, whose source is then in ESI
		MEMORY_BASIC_INFORMATION mbi;
		BOOL readable = VirtualQuery((LPCVOID)eip, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
			(mbi.Protect & (PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY | PAGE_READONLY | PAGE_READWRITE)) &&
			eip + 4 <= (DWORD)mbi.BaseAddress + mbi.RegionSize;
		const BYTE* code = (const BYTE*)eip;
		for (DWORD k = 0; k < 4; ++k)
			e->code[k] = readable ? code[k] : 0;
		e->id = id;
		e->start = start;
		DWORD flags = 0;
		if (tid != trap.gameThread)
			flags |= TrapOtherThread;
		else
		{
			if (xfer.depth)
				flags |= TrapDecode;
			if (trap.renderDepth)
				flags |= TrapRender;
		}
		for (DWORD k = 0; k < 4; ++k)
			e->ret[k] = 0;
		DWORD frame = context->Ebp;
		for (DWORD k = 0; stack && k < 4; ++k)
		{
			if (frame < low || frame + 2 * sizeof(DWORD) > high || (frame & 3))
				break;
			const DWORD* f = (const DWORD*)frame;
			e->ret[k] = f[1];
			if (TrapSelf(f[1]) && !(flags & (TrapDecode | TrapRender | TrapOtherThread)))
				flags |= TrapHooked;
			if (f[0] <= frame)
				break;
			frame = f[0];
		}
		e->flags = flags;
		e->seq = xfer.seq;
		e->frameNo = game.frameNo;
		InterlockedExchange(&e->ready, slot + 1);
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	// the code of the module, so that pointers into its data never pass for code addresses
	VOID TrapModule(HMODULE module, DWORD* low, DWORD* high)
	{
		*low = *high = 0;
		if (!module)
			return;
		const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)module;
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)((const BYTE*)module + dos->e_lfanew);
		const IMAGE_OPTIONAL_HEADER* header = &nt->OptionalHeader;
		if (header->SizeOfCode && header->BaseOfCode + header->SizeOfCode <= header->SizeOfImage)
		{
			*low = (DWORD)module + header->BaseOfCode;
			*high = *low + header->SizeOfCode;
		}
		else
		{
			*low = (DWORD)module;
			*high = *low + header->SizeOfImage;
		}
	}

	BOOL TrapExport(DWORD address, CHAR* text);

	// the 16:07 run saw binkw32.dll+21A13 and +21B29 write the 1152x1152 surface at frame 0; this names the exports they are in and what the game imports from Bink
	VOID TrapLogBink()
	{
		HMODULE bink = GetModuleHandle("binkw32.dll");
		if (!bink)
		{
			DebugLog("twin trap bink: binkw32.dll is not loaded");
			return;
		}
		CHAR first[64], second[64];
		if (!TrapExport((DWORD)bink + 0x21A13, first))
			StrCopy(first, "-");
		if (!TrapExport((DWORD)bink + 0x21B29, second))
			StrCopy(second, "-");

		CHAR list[700];
		CHAR* at = list;
		*at = 0;
		const BYTE* exe = (const BYTE*)GetModuleHandle(NULL);
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(exe + ((const IMAGE_DOS_HEADER*)exe)->e_lfanew);
		const IMAGE_DATA_DIRECTORY* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
		for (const IMAGE_IMPORT_DESCRIPTOR* d = dir->VirtualAddress ? (const IMAGE_IMPORT_DESCRIPTOR*)(exe + dir->VirtualAddress) : NULL; d && d->Name; ++d)
		{
			if (StrCompareInsensitive((const CHAR*)(exe + d->Name), "binkw32.dll") || !d->OriginalFirstThunk)
				continue;
			for (const IMAGE_THUNK_DATA* t = (const IMAGE_THUNK_DATA*)(exe + d->OriginalFirstThunk); t->u1.AddressOfData && at < list + sizeof(list) - 48; ++t)
			{
				if (IMAGE_SNAP_BY_ORDINAL(t->u1.Ordinal))
					at += StrPrint(at, " #%u", (DWORD)IMAGE_ORDINAL(t->u1.Ordinal));
				else
					at += StrPrint(at, " %.40s", (const CHAR*)((const IMAGE_IMPORT_BY_NAME*)(exe + t->u1.AddressOfData))->Name);
			}
		}
		DebugLog("twin trap bink: binkw32.dll at %08X, +21A13 is in export %s, +21B29 in %s | the game imports:%s", bink, first, second, *list ? list : " none");
	}

	BOOL TrapInit()
	{
		if (trap.initialized)
			return trap.on;
		trap.initialized = TRUE;
		if (!TrapWanted())
		{
			DebugLog("twin trap off (Twin.WriteTrap=0)");
			return FALSE;
		}

		TrapModule(hDllModule, &trap.selfLo, &trap.selfHi);
		TrapModule(GetModuleHandle(NULL), &trap.exeLo, &trap.exeHi);
		trap.gameThread = GetCurrentThreadId();
		trap.windowTick = GetTickCount();
		trap.on = TRUE;
		if (!AddVectoredExceptionHandler(1, TrapHandler))
		{
			trap.on = FALSE;
			DebugLog("twin trap off: the vectored exception handler could not be added");
			return FALSE;
		}
		DebugLog("twin trap on (Twin.WriteTrap=1): surface pages stay read-only until the game's first write; wrapper %08X-%08X, game %08X-%08X | re-armed every %u to %u ms, at most %u pages a second, %u faults in all",
			trap.selfLo, trap.selfHi, trap.exeLo, trap.exeHi, TRAP_REARM_MS, TRAP_REARM_MAX_MS, TRAP_PAGES_SECOND, TRAP_TOTAL);
		TrapLogBink();
		return TRUE;
	}

	VOID TrapSetBits(TrapRange* r, DWORD first, DWORD last)
	{
		for (DWORD p = first; p <= last; ++p)
		{
			DWORD bit = 1u << (p & 31);
			if (!(r->bits[p >> 5] & bit))
			{
				r->bits[p >> 5] |= bit;
				++r->armedBits;
			}
		}
	}

	BOOL TrapProtect(TrapRange* r, DWORD first, DWORD last)
	{
		DWORD old;
		if (!VirtualProtect((VOID*)(r->start + first * TRAP_PAGE), (last - first + 1) * TRAP_PAGE, PAGE_READONLY, &old))
		{
			++trap.armFails;
			return FALSE;
		}
		TrapSetBits(r, first, last);
		trap.windowPages += last - first + 1;
		return TRUE;
	}

	BOOL TrapArm(TrapRange* r, DWORD now)
	{
		if (!TrapProtect(r, 0, r->pages - 1))
			return FALSE;
		r->lastArm = now;
		InterlockedExchange(&r->faults, 0);
		InterlockedExchange(&r->own, 0);
		r->foreignSinceArm = 0;
		return TRUE;
	}

	VOID TrapAttach(Record* twin, DWORD id)
	{
		if (!twin->owner->isPaged || twin->memory || ((DWORD)twin->base & (TRAP_PAGE - 1)))
		{
			++trap.notPaged;
			return;
		}
		if (!TrapInit() || trap.faults >= TRAP_TOTAL)
			return;

		TrapRange* r = &trap.ranges[id];
		DWORD pages = ((DWORD)twin->pitch * twin->height * sizeof(DWORD) + TRAP_PAGE - 1) / TRAP_PAGE;
		DWORD words = (pages + 31) / 32;
		if (!pages)
			return;
		if (words > r->bitWords)
		{
			if (r->bits)
				MemoryFree(r->bits);
			r->bits = (DWORD*)MemoryAlloc(words * sizeof(DWORD));
			r->bitWords = r->bits ? words : 0;
			if (!r->bits)
				return;
		}
		MemoryZero(r->bits, r->bitWords * sizeof(DWORD));
		r->armedBits = 0;
		r->pages = pages;
		r->bytes = pages * TRAP_PAGE;
		r->interval = TRAP_REARM_MS;
		r->fresh = TRUE;
		r->quickArms = 0;
		r->queued = FALSE;
		r->reFirst = MAXDWORD;
		r->reLast = 0;
		r->priority = FALSE;
		r->priorityFaults = 0;
		r->start = twin->base;
		++trap.attached;
		if (TrapArm(r, GetTickCount()))
			++trap.createArms;
	}

	VOID TrapDetach(DWORD id)
	{
		TrapRange* r = &trap.ranges[id];
		const BYTE* start = r->start;
		if (!start)
			return;
		r->start = NULL;
		DWORD old;
		VirtualProtect((VOID*)start, r->bytes, PAGE_READWRITE, &old);
		r->armedBits = 0;
		r->pages = 0;
	}

	// a write of ours is about to happen there: those pages open, so it costs one call instead of a fault per page
	VOID TrapOpen(Record* twin, const RECT* rect)
	{
		DWORD id = DWORD(twin - game.twins);
		if (id >= TWIN_MAX || trap.noOpen)
			return;
		TrapRange* r = &trap.ranges[id];
		if (!r->armedBits || r->start != twin->base)
			return;

		LONG top = max(rect->top, 0L), bottom = min(rect->bottom, twin->height);
		LONG left = max(rect->left, 0L), right = min(rect->right, twin->width);
		if (top >= bottom || left >= right)
			return;
		DWORD first = DWORD(top * twin->pitch + left) * sizeof(DWORD) / TRAP_PAGE;
		DWORD last = (DWORD((bottom - 1) * twin->pitch + right) * sizeof(DWORD) - 1) / TRAP_PAGE;
		if (last >= r->pages)
			last = r->pages - 1;

		DWORD cleared = 0;
		for (DWORD p = first; p <= last; ++p)
		{
			DWORD bit = 1u << (p & 31);
			if (r->bits[p >> 5] & bit)
			{
				r->bits[p >> 5] &= ~bit;
				++cleared;
			}
		}
		if (!cleared)
			return;

		DWORD old;
		VirtualProtect((VOID*)(r->start + first * TRAP_PAGE), (last - first + 1) * TRAP_PAGE, PAGE_READWRITE, &old);
		r->armedBits -= min(cleared, r->armedBits);
		++trap.opens;
		trap.openPages += cleared;

		// a fresh surface is armed again right after our write, so the game's first write still traps
		if (r->fresh && r->quickArms < TRAP_QUICK)
		{
			r->reFirst = min(r->reFirst, first);
			r->reLast = max(r->reLast, last);
			if (!r->queued && trap.reopenCount < TRAP_REOPEN)
			{
				r->queued = TRUE;
				trap.reopen[trap.reopenCount++] = id;
			}
		}
	}

	// at the next twin call that drains, so after the hooked write that opened the pages has finished
	VOID TrapReopen()
	{
		DWORD count = trap.reopenCount;
		trap.reopenCount = 0;
		for (DWORD i = 0; i < count; ++i)
		{
			TrapRange* r = &trap.ranges[trap.reopen[i]];
			r->queued = FALSE;
			DWORD first = r->reFirst, last = r->reLast;
			r->reFirst = MAXDWORD;
			r->reLast = 0;
			if (!r->start || !r->fresh || first > last || last >= r->pages || trap.faults >= TRAP_TOTAL)
				continue;
			if (trap.windowPages >= TRAP_PAGES_SECOND && !r->priority)
			{
				++trap.reopenSkips;
				continue;
			}
			if (TrapProtect(r, first, last))
			{
				++r->quickArms;
				++trap.quickArms;
			}
		}
	}

	VOID TrapRearm(DWORD now)
	{
		if (!trap.on)
			return;
		if (now - trap.windowTick >= 1000)
		{
			trap.windowTick = now;
			trap.windowPages = 0;
			trap.priorityWindow = 0;
		}
		if (trap.faults >= TRAP_TOTAL)
		{
			if (!trap.spentLogged)
			{
				trap.spentLogged = TRUE;
				DebugLog("twin trap budget of %u faults spent: no more arming (writers %u)", TRAP_TOTAL, trap.writerCount);
			}
			return;
		}

		// the top 1x-only surfaces are re-armed at every present, outside the page budget, until they have shown their writers; one the game writes every frame then drops to once a second
		for (DWORD k = 0; k < trap.priorityCount; ++k)
		{
			DWORD id = trap.priority[k].id;
			TrapRange* r = &trap.ranges[id];
			if (game.twins[id].owner != trap.priority[k].owner || !r->start || !r->priority || (r->armedBits >= r->pages && !r->faults))
				continue;
			BOOL hot = r->priorityFaults >= TRAP_PRIORITY_FAULTS;
			if ((hot && now - r->lastArm < TRAP_REARM_MS) || trap.priorityWindow + r->pages > TRAP_PRIORITY_PAGES_SECOND)
				continue;
			DWORD before = trap.windowPages;
			if (TrapArm(r, now))
			{
				++trap.priorityArms;
				if (hot)
					++trap.priorityHot;
				trap.priorityWindow += r->pages;
			}
			trap.windowPages = before;
		}

		DWORD top = game.top;
		for (DWORD n = 0; n < top; ++n)
		{
			DWORD id = (trap.cursor + n) % top;
			TrapRange* r = &trap.ranges[id];
			if (!r->start || r->priority || (r->armedBits >= r->pages && !r->faults) || now - r->lastArm < r->interval)
				continue;
			if (r->pages > TRAP_PAGES_SECOND)
			{
				++trap.tooBig;
				continue;
			}
			if (trap.windowPages + r->pages > TRAP_PAGES_SECOND)
			{
				++trap.budgetSkips;
				trap.cursor = id;
				return;
			}
			// a surface only we write backs off; one the game writes stays at the base rate
			r->interval = r->foreignSinceArm ? TRAP_REARM_MS : min(r->interval * 2, (DWORD)TRAP_REARM_MAX_MS);
			if (TrapArm(r, now))
				++trap.rearms;
		}
		trap.cursor = 0;
	}

	VOID TrapName(DWORD address, CHAR* text)
	{
		if (!address)
		{
			StrCopy(text, "-");
			return;
		}
		CHAR name[24];
		DWORD base = CrashModule(address, name, sizeof(name));
		StrPrint(text, "%08X %s+%X", address, name, address - base);
	}

	// the nearest export at or below the address, for code in a module that is neither the game nor the wrapper
	BOOL TrapExport(DWORD address, CHAR* text)
	{
		*text = 0;
		CHAR module[24];
		DWORD base = address ? CrashModule(address, module, sizeof(module)) : 0;
		if (!base || TrapExe(address) || TrapSelf(address))
			return FALSE;
		const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
		if (dos->e_magic != IMAGE_DOS_SIGNATURE)
			return FALSE;
		const IMAGE_NT_HEADERS* nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return FALSE;
		const IMAGE_DATA_DIRECTORY* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		if (!dir->VirtualAddress || !dir->Size)
			return FALSE;
		const IMAGE_EXPORT_DIRECTORY* exports = (const IMAGE_EXPORT_DIRECTORY*)(base + dir->VirtualAddress);
		const DWORD* functions = (const DWORD*)(base + exports->AddressOfFunctions);
		const DWORD* names = (const DWORD*)(base + exports->AddressOfNames);
		const WORD* ordinals = (const WORD*)(base + exports->AddressOfNameOrdinals);
		DWORD offset = address - base, best = MAXDWORD, bestRva = 0;
		for (DWORD i = 0; i < exports->NumberOfFunctions; ++i)
		{
			DWORD rva = functions[i];
			// a forwarder points into the export directory, not at code
			if (!rva || rva > offset || rva - dir->VirtualAddress < dir->Size || (best != MAXDWORD && rva <= bestRva))
				continue;
			best = i;
			bestRva = rva;
		}
		if (best == MAXDWORD)
			return FALSE;
		const CHAR* name = NULL;
		for (DWORD j = 0; j < exports->NumberOfNames && !name; ++j)
			if (ordinals[j] == best)
				name = (const CHAR*)(base + names[j]);
		if (name)
			StrPrint(text, "%.48s+%X", name, offset - bestRva);
		else
			StrPrint(text, "#%u+%X", best + exports->Base, offset - bestRva);
		return TRUE;
	}

	// where a register points: a twin and its pixel, a store image, or nothing known
	VOID RegText(DWORD value, CHAR* text)
	{
		DWORD id;
		Record* twin = value ? FindAddress((const VOID*)value, &id) : NULL;
		if (twin)
		{
			LONG start = LONG((value - (DWORD)twin->base) / sizeof(DWORD));
			StrPrint(text, "%08X (twin %u %s %dx%d at %d,%d)", value, id, TwinKind(twin), twin->width, twin->height, start % twin->pitch, start / twin->pitch);
			return;
		}
		const CHAR* store = value ? ImageReplace::MemoryName((const VOID*)value) : NULL;
		if (store)
			StrPrint(text, "%08X (store %.24s)", value, store);
		else
			StrPrint(text, "%08X", value);
	}

	BOOL TrapCopyWanted()
	{
		if (!trap.copyChosen)
		{
			trap.copyChosen = TRUE;
			trap.copyOn = Config::Get("Twin", "TrapCopy", 1) != 0;
		}
		return trap.copyOn;
	}

	// a rep movs the trap caught is a CPU copy whose source is ESI; the rows it wrote become a hint, replayed twin to twin once the 1x rows are equal
	VOID TrapHint(Record* twin, DWORD id, const TrapEvent* e, LONG x, LONG y)
	{
		BOOL dwords = e->code[0] == 0xF3 && e->code[1] == 0xA5;
		BOOL bytes = e->code[0] == 0xF3 && e->code[1] == 0xA4;
		if ((!dwords && !bytes) || twin->memory || !TrapCopyWanted() || e->addr != e->regs[RegEdi])
			return;
		DWORD srcId;
		Record* source = FindAddress((const VOID*)e->regs[RegEsi], &srcId);
		if (!source || source == twin || ((e->regs[RegEsi] - (DWORD)source->base) & 3) || (e->addr & 3))
			return;
		++trap.copyFaults;
		LONG start = LONG((e->regs[RegEsi] - (DWORD)source->base) / sizeof(DWORD));
		LONG sx = start % source->pitch, sy = start / source->pitch;
		LONG ox = x - sx, oy = y - sy;
		LONG n = LONG(bytes ? e->regs[RegEcx] / 4 : e->regs[RegEcx]);
		if (n <= 0)
			return;
		RECT seg;
		if (x + n <= twin->width)
			SetRect(&seg, x, y, x + n, y + 1);
		else if (!ox && source->pitch == twin->pitch)
			SetRect(&seg, 0, y, twin->width, min(y + (x + n + twin->width - 1) / twin->width, twin->height));
		else
			SetRect(&seg, x, y, twin->width, y + 1);

		DWORD k = 0;
		while (k < trap.hintCount && !(trap.hints[k].dst == twin->owner && trap.hints[k].src == source->owner && trap.hints[k].ox == ox && trap.hints[k].oy == oy))
			++k;
		if (k == trap.hintCount)
		{
			if (k == TRAP_HINTS)
				return;
			++trap.hintCount;
			trap.hints[k].dst = twin->owner;
			trap.hints[k].dstId = id;
			trap.hints[k].src = source->owner;
			trap.hints[k].srcId = srcId;
			trap.hints[k].ox = ox;
			trap.hints[k].oy = oy;
			trap.hints[k].rect = seg;
			trap.hints[k].faults = 0;
		}
		else
			UnionRect(&trap.hints[k].rect, &trap.hints[k].rect, &seg);
		++trap.hints[k].faults;
		trap.hints[k].frameNo = game.frameNo;
	}

	VOID CopyTwins(Record* target, DWORD dstId, LONG x, LONG y, Record* source, DWORD srcId, const RECT* srcRect, BOOL keyed, DWORD key);

	VOID TrapCopyFlush()
	{
		if (!trap.hintCount || trap.draining)
			return;
		DWORD count = trap.hintCount;
		trap.hintCount = 0;
		for (DWORD k = 0; k < count; ++k)
		{
			const RECT* hr = &trap.hints[k].rect;
			Record* target = &game.twins[trap.hints[k].dstId];
			Record* source = &game.twins[trap.hints[k].srcId];
			if (target->owner != trap.hints[k].dst || source->owner != trap.hints[k].src)
				continue;
			LONG ox = trap.hints[k].ox, oy = trap.hints[k].oy;
			RECT to = *hr;
			if (!Clip(target, &to))
				continue;
			RECT from = to;
			OffsetRect(&from, -ox, -oy);
			if (!Clip(source, &from))
				continue;
			to = from;
			OffsetRect(&to, ox, oy);

			// the guard: the CPU copy left exactly the source's 1x in every row; anything else stays the 1x the trapped write marked
			LONG width = to.right - to.left, bad = -1;
			for (LONG row = to.top; row < to.bottom && bad < 0; ++row)
				if (MemoryCompare((const DWORD*)target->base + row * target->pitch + to.left, (const DWORD*)source->base + (row - oy) * source->pitch + from.left, width * sizeof(DWORD)))
					bad = row;
			BOOL carries = Carries4K(source, &from);
			if (bad < 0)
			{
				CopyTwins(target, trap.hints[k].dstId, to.left, to.top, source, trap.hints[k].srcId, &from, FALSE, 0);
				++trap.copyReplayed;
				trap.copyPx += Area(&to);
			}
			else
				++trap.copyGuardFails;
			if (trap.copyLogs < 24)
			{
				++trap.copyLogs;
				CHAR outcome[96];
				if (bad < 0)
					StrPrint(outcome, "replayed as a twin copy, source %s", carries ? "carries 4K" : "is 1x");
				else
					StrPrint(outcome, "not replayed: row %d differs from the source's 1x", bad);
				DebugLog("twin trap copy #%u: rep movs from twin %u %dx%d at %d,%d -> twin %u %dx%d rect %d,%d %dx%d (%u faults, frame %u) | %s",
					trap.copyLogs, trap.hints[k].srcId, source->width, source->height, from.left, from.top, trap.hints[k].dstId, target->width, target->height,
					to.left, to.top, width, to.bottom - to.top, trap.hints[k].faults, trap.hints[k].frameNo, outcome);
			}
		}
	}

	// the first write into a fresh surface during or right after a decode is where that decode's pixels went
	VOID TrapProvenance(Record* twin, DWORD id, const TrapEvent* e, LONG x, LONG y)
	{
		if (twin->memory || !SeedMode())
			return;
		if (e->flags & TrapDecode)
		{
			if (xfer.depth)
			{
				XferAdd(xfer.inside, &xfer.insideCount, XFER_INSIDE, twin, id, x, y, 0, ViaTrap);
				++xfer.trapWatches;
				return;
			}
			// the decode call it happened in has returned; what that call decoded is the latest decode
			if (e->seq + 1 != xfer.seq)
			{
				++xfer.trapLate;
				return;
			}
		}
		else if (e->seq != xfer.seq)
		{
			++xfer.trapLate;
			return;
		}
		if (!xfer.valid || game.frameNo - xfer.frameNo > XFER_UNBOUND_FRAMES || twin->arrive.xferSeq == xfer.seq)
		{
			++xfer.trapLate;
			return;
		}

		BOOL sized = twin->height == xfer.height && (twin->width == xfer.width || twin->width == xfer.width + 1);
		XferAdd(xfer.watch, &xfer.watchCount, XFER_WATCH, twin, id, sized ? 0 : x, sized ? 0 : y, xfer.seq, ViaTrap);
		++xfer.trapWatches;
	}

	VOID TrapRecord(const TrapEvent* e)
	{
		Record* twin = e->id < TWIN_MAX ? &game.twins[e->id] : NULL;
		if (twin && (!twin->owner || twin->base != e->start))
		{
			++trap.stale;
			twin = NULL;
		}
		LONG x = 0, y = 0;
		DWORD page = 0;
		if (twin)
		{
			DWORD offset = e->addr - (DWORD)twin->base;
			DWORD row = (DWORD)twin->pitch * sizeof(DWORD);
			y = min(LONG(offset / row), twin->height - 1);
			x = LONG(offset % row / sizeof(DWORD));
			page = offset / TRAP_PAGE;
		}
		TrapRange* r = twin ? &trap.ranges[e->id] : NULL;
		BOOL fresh = r && r->fresh && !(e->flags & TrapOtherThread);

		TrapWriter* w = NULL;
		for (DWORD i = 0; i < trap.writerCount && !w; ++i)
			if (trap.writers[i].eip == e->eip)
				w = &trap.writers[i];
		BOOL added = FALSE;
		if (!w)
		{
			if (trap.writerCount < TRAP_WRITERS)
			{
				w = &trap.writers[trap.writerCount++];
				MemoryZero(w, sizeof(TrapWriter));
				w->eip = e->eip;
				w->caller = e->caller;
				MemoryCopy(w->ret, e->ret, sizeof(w->ret));
				w->sampleId = TWIN_NONE;
				CHAR name[24];
				w->moduleOffset = e->eip - CrashModule(e->eip, name, sizeof(name));
				MemoryCopy(w->module, name, sizeof(w->module));
				w->module[sizeof(w->module) - 1] = 0;
				added = TRUE;
			}
			else
				++trap.writersFull;
		}
		if (w)
		{
			++w->count;
			if (twin)
			{
				BOOL known = FALSE;
				for (DWORD i = 0; i < min(w->twinCount, (DWORD)4) && !known; ++i)
					known = w->twins[i] == e->id;
				if (!known)
				{
					if (w->twinCount < 4)
						w->twins[w->twinCount] = e->id;
					++w->twinCount;
				}
				if (w->sampleId == TWIN_NONE)
				{
					w->sampleId = e->id;
					w->sampleW = twin->width;
					w->sampleH = twin->height;
					w->sampleX = x;
					w->sampleY = y;
				}
			}
			if (e->flags & TrapDecode)
				++w->decode;
			if (e->flags & TrapRender)
				++w->render;
			if (e->flags & TrapHooked)
				++w->hooked;
			if (e->flags & TrapOtherThread)
				++w->other;
			if (fresh)
				++w->fresh;
		}

		const CHAR* where = (e->flags & TrapOtherThread) ? "another thread" : (e->flags & TrapDecode) ? "inside a decode (StoreDraw)" : (e->flags & TrapRender) ? "inside Render" :
			(e->flags & TrapHooked) ? "under a hooked op" : "outside decode, Render and hooked ops";
		if (added)
		{
			CHAR eip[48], caller[48], ret[4][48], exported[64];
			TrapName(e->eip, eip);
			TrapName(e->caller, caller);
			for (DWORD k = 0; k < 4; ++k)
				TrapName(e->ret[k], ret[k]);
			TrapExport(e->eip, exported);
			DebugLog("twin trap writer #%u new: eip %s%s%s | caller %s | ret %s < %s < %s < %s | code %02X %02X %02X %02X esi %08X edi %08X ecx %u | twin %u %dx%d at %d,%d (page %u)%s | %s | frame %u, latest decode #%u %.32s %dx%d %u frames before",
				trap.writerCount, eip, *exported ? " in export " : "", exported, caller, ret[0], ret[1], ret[2], ret[3], e->code[0], e->code[1], e->code[2], e->code[3],
				e->regs[RegEsi], e->regs[RegEdi], e->regs[RegEcx], twin ? e->id : TWIN_NONE, twin ? twin->width : 0, twin ? twin->height : 0, x, y, page,
				fresh ? ", first write into the fresh surface" : "", where, e->frameNo, e->seq, xfer.seq == e->seq ? xfer.name : "-", xfer.seq == e->seq ? xfer.width : 0,
				xfer.seq == e->seq ? xfer.height : 0, e->frameNo - xfer.frameNo);
		}

		if (!twin)
			return;
		++twin->arrive.trapWrites;
		twin->arrive.trapEip = e->eip;
		++r->foreignSinceArm;
		if (fresh)
		{
			r->fresh = FALSE;
			TrapProvenance(twin, e->id, e, x, y);
		}

		// the game's own write changed the 1x on that page; its rows get the 1x as a hooked write's would, without opening the pages around it
		{
			DWORD rowBytes = (DWORD)twin->pitch * sizeof(DWORD);
			DWORD pageStart = (e->addr - (DWORD)twin->base) & ~(TRAP_PAGE - 1);
			RECT rows = { 0, LONG(pageStart / rowBytes), twin->width, LONG(min((pageStart + TRAP_PAGE + rowBytes - 1) / rowBytes, (DWORD)twin->height)) };
			trap.noOpen = TRUE;
			MarkDirty(twin, rows);
			trap.noOpen = FALSE;
			++trap.marked;
			Note(twin, NoteTrap, &rows, e->eip);
		}
		TrapHint(twin, e->id, e, x, y);

		if (r->priority)
		{
			++r->priorityFaults;
			++trap.priorityWrites;
			BOOL known = FALSE;
			for (DWORD i = 0; i < trap.pairCount && !known; ++i)
				known = trap.pairs[i].eip == e->eip && trap.pairs[i].id == e->id;
			if (!known && trap.pairCount < TRAP_PAIRS)
			{
				trap.pairs[trap.pairCount].eip = e->eip;
				trap.pairs[trap.pairCount++].id = e->id;
			}
			if (!known && trap.priorityLogs < TRAP_PRIORITY_LOGS)
			{
				++trap.priorityLogs;
				CHAR eip[48], caller[48], ret[4][48], exported[64], esi[64], edi[64];
				TrapName(e->eip, eip);
				TrapName(e->caller, caller);
				for (DWORD k = 0; k < 4; ++k)
					TrapName(e->ret[k], ret[k]);
				TrapExport(e->eip, exported);
				RegText(e->regs[RegEsi], esi);
				RegText(e->regs[RegEdi], edi);
				DebugLog("twin trap priority write #%u: twin %u %dx%d at %d,%d | eip %s%s%s | caller %s | ret %s < %s < %s < %s | code %02X %02X %02X %02X | esi %s edi %s ecx %u eax %08X ebx %08X edx %08X | %s | frame %u",
					trap.priorityLogs, e->id, twin->width, twin->height, x, y, eip, *exported ? " in export " : "", exported, caller, ret[0], ret[1], ret[2], ret[3],
					e->code[0], e->code[1], e->code[2], e->code[3], esi, edi, e->regs[RegEcx], e->regs[RegEax], e->regs[RegEbx], e->regs[RegEdx], where, e->frameNo);
			}
		}
	}

	VOID TrapDrain()
	{
		if (trap.draining)
			return;
		trap.draining = TRUE;
		DWORD head = (DWORD)trap.head;
		while (trap.tail != head)
		{
			DWORD slot = trap.tail;
			TrapEvent* e = &trap.ring[slot % TRAP_RING];
			if (e->ready == (LONG)(slot + 1))
				TrapRecord(e);
			else
				++trap.lost;
			trap.tail = slot + 1;
		}
		trap.draining = FALSE;
	}

	VOID TrapLog()
	{
		if (!trap.initialized)
			return;
		DebugLog("twin trap since start: %s | twins attached %u (not page-aligned %u) | armed at create %u, re-armed at present %u, after our own write %u (skipped for the page budget %u, too big %u, failed %u) | opened by our announced writes %u (%u pages) | faults %u: wrapper's own %u, game %u (ring dropped %u, lost %u, stale %u, retried %u) | writers %u (table full %u) | pages armed this second %u of %u",
			trap.on ? (trap.faults >= TRAP_TOTAL ? "budget spent" : "on") : "off", trap.attached, trap.notPaged, trap.createArms, trap.rearms, trap.quickArms, trap.budgetSkips, trap.tooBig, trap.armFails,
			trap.opens, trap.openPages, trap.faults, trap.own, trap.foreign, trap.dropped, trap.lost, trap.stale, trap.retried, trap.writerCount, trap.writersFull, trap.windowPages, TRAP_PAGES_SECOND);
		CHAR list[TRAP_PRIORITY * 40 + 1];
		CHAR* at = list;
		*at = 0;
		for (DWORD k = 0; k < trap.priorityCount; ++k)
		{
			const TrapRange* r = &trap.ranges[trap.priority[k].id];
			const Record* twin = &game.twins[trap.priority[k].id];
			at += StrPrint(at, " %u %dx%d tr %u%s;", trap.priority[k].id, twin->width, twin->height, r->priorityFaults, r->priorityFaults >= TRAP_PRIORITY_FAULTS ? " hot" : "");
		}
		DebugLog("twin trap priority (the top 1x-only surfaces, re-armed every present outside the budget):%s | arms %u (at the base rate once hot %u), writes %u, writer+twin pairs %u | quick re-arms skipped for the budget %u | trapped writes marked dirty %u | rep movs copies %u: hints replayed %u (%u px), guard failed %u (TrapCopy=%u)",
			list, trap.priorityArms, trap.priorityHot, trap.priorityWrites, trap.pairCount, trap.reopenSkips, trap.marked, trap.copyFaults, trap.copyReplayed, trap.copyPx, trap.copyGuardFails, TrapCopyWanted());

		DWORD top[TRAP_TOP];
		DWORD found = 0;
		for (DWORD i = 0; i < trap.writerCount; ++i)
		{
			DWORD slot = found < TRAP_TOP ? found++ : TRAP_TOP;
			while (slot && trap.writers[top[slot - 1]].count < trap.writers[i].count)
			{
				if (slot < TRAP_TOP)
					top[slot] = top[slot - 1];
				--slot;
			}
			if (slot < TRAP_TOP)
				top[slot] = i;
		}
		for (DWORD line = 0; line * 4 < found; ++line)
		{
			CHAR text[4 * 240 + 1];
			CHAR* ptr = text;
			*ptr = 0;
			for (DWORD k = line * 4; k < found && k < line * 4 + 4; ++k)
			{
				const TrapWriter* w = &trap.writers[top[k]];
				ptr += StrPrint(ptr, " [%u] %08X %.20s+%X n %u tw %u (#%d %dx%d at %d,%d) dec %u ren %u hk %u thr %u fresh %u;", k + 1, w->eip, w->module, w->moduleOffset, w->count, w->twinCount,
					w->sampleId == TWIN_NONE ? -1 : (INT)w->sampleId, w->sampleW, w->sampleH, w->sampleX, w->sampleY, w->decode, w->render, w->hooked, w->other, w->fresh);
			}
			DebugLog("twin trap top writers (rank eip module+offset, n pages opened, tw twins (sample twin size at x,y), inside decode/Render/hooked op/other thread, first writes into fresh surfaces):%s", text);
		}
	}
#pragma endregion

	// the surfaces copied most as 1x are re-armed at every present until their writers show
	VOID TrapPriority(const DWORD* top, DWORD found)
	{
		DWORD ids[TRAP_PRIORITY];
		DWORD count = 0;
		for (DWORD k = 0; k < found && count < TRAP_PRIORITY; ++k)
		{
			const Record* twin = &game.twins[top[k]];
			if (!twin->memory && trap.ranges[top[k]].start == twin->base && twin->base)
				ids[count++] = top[k];
		}
		for (DWORD k = 0; k < trap.priorityCount; ++k)
		{
			DWORD id = trap.priority[k].id;
			BOOL kept = FALSE;
			for (DWORD n = 0; n < count && !kept; ++n)
				kept = ids[n] == id && game.twins[id].owner == trap.priority[k].owner;
			if (!kept)
				trap.ranges[id].priority = FALSE;
		}
		for (DWORD n = 0; n < count; ++n)
		{
			TrapRange* r = &trap.ranges[ids[n]];
			if (!r->priority)
			{
				r->priority = TRUE;
				r->priorityFaults = 0;
			}
			trap.priority[n].id = ids[n];
			trap.priority[n].owner = game.twins[ids[n]].owner;
		}
		trap.priorityCount = count;
	}

	VOID CountCoverage(Record* source, const RECT* from)
	{
		LONG px = Area(from);
		if (!source)
		{
			++seeds.coverUntwinned;
			seeds.coverUntwinnedPx += px;
		}
		else if (Carries4K(source, from))
		{
			++seeds.coverSeeded;
			seeds.coverSeededPx += px;
		}
		else
		{
			++seeds.coverUnseeded;
			seeds.coverUnseededPx += px;
			source->coverPx += px;
			++source->coverCopies;
		}
	}

	VOID LogSeeds()
	{
		DWORD mbScale = game.scale * game.scale * sizeof(DWORD);
		for (DWORD k = 0; k < SeedKinds; ++k)
		{
			const SeedCounters* c = &seeds.kinds[k];
			DWORD mb10 = DWORD(c->px * mbScale * 10 / (1024 * 1024));
			DebugLog("twin seed %s since start: events %u done %u deferred %u (completed later %u, expired %u) dropped %u (memory written %u, freed %u) | overwritten after seeding %u | no twin %u no art %u | pieces %u emits %u px %u (%u.%u MB at N) | into memory twins %u, surface twins %u, by transfer %u",
				seedKindNames[k], c->events, c->done, c->deferred, c->later, c->expired, c->dropped + c->released, c->dropped, c->released, c->overwritten, c->noTwin, c->noArt,
				c->pieces, c->emits, (DWORD)c->px, mb10 / 10, mb10 % 10, c->toMemory, c->toSurface, c->transfers);
		}

		DWORD pending = 0;
		for (DWORD i = 0; i < seeds.top; ++i)
			pending += seeds.list[i].owner && seeds.list[i].pending;

		Lock();
		DWORD replayed = shared.seedReplayed, replayedPieces = shared.seedPieces, missing = shared.seedMissing;
		DWORD checked = shared.conflictsChecked, differ = shared.conflictsDiffer, skipped = shared.conflictsSkipped, blocks = shared.conflictBlocks;
		Unlock();

		DWORD waitAvg = seeds.waits ? DWORD(seeds.waitUs / seeds.waits) : 0;
		DebugLog("twin seed sheets since start %u: frames %u (unmapped %u, additive skipped %u) parts outside the sheet %u | pieces %u | blocks shared by several frames %u, of those with 4K differing by mean>%u: %u | frame pairs sent %u checked %u differ %u (gpu skipped %u, art not ready %u)",
			seeds.sheets, seeds.sheetFrames, seeds.sheetUnmapped, seeds.sheetAdditive, seeds.sheetOutside, seeds.sheetPieces, seeds.sheetShared, SEED_CONFLICT_DIFF, blocks,
			seeds.conflicts, checked, differ, skipped, seeds.conflictsUnchecked);
		DebugLog("twin seed records %u (pending %u, evicted %u, redraws queued %u, repeat decodes still intact %u, invalid pieces %u) | wait avg %u us max %u us (%u waits) | replayed %u commands %u pieces, art missing at replay %u | keyed copies on another key from 4K masked by 1x %u (too big %u)",
			seeds.active, pending, seeds.evicted, seeds.repends, seeds.repeats, seeds.skippedPieces, waitAvg, seeds.waitMaxUs, seeds.waits, replayed, replayedPieces, missing, seeds.maskedCopies, seeds.maskedSkipped);

		DWORD lateAvgFrames = seeds.late ? DWORD(seeds.lateFrames / seeds.late) : 0, lateAvgMs = seeds.late ? DWORD(seeds.lateMs / seeds.late) : 0;
		DebugLog("twin seed streaming 5s: deferred %u (now pending %u), carried by copies out of pending seeds %u (pieces %u) | landed late %u (pieces %u) after avg %u frames / %u ms, max %u frames / %u ms | frame checks still decoding %u, over the inline upload budget %u (budget %u KB per present) | pieces dropped by a write since the decode %u | waits while loading %u (SeedWait %u ms, SeedLoadWait %u ms)",
			seeds.deferredInterval, pending, seeds.derived, seeds.derivedPieces, seeds.late, seeds.latePieces, lateAvgFrames, lateAvgMs, seeds.lateMaxFrames, seeds.lateMaxMs, seeds.waitDecode, seeds.inlineDeferred,
			DWORD(seeds.inlineBudget / 1024), seeds.piecesWritten, seeds.loadWaits, seeds.waitMs, seeds.loadWaitMs);
		seeds.deferredInterval = seeds.derived = seeds.derivedPieces = seeds.late = seeds.latePieces = seeds.waitDecode = seeds.inlineDeferred = seeds.piecesWritten = seeds.loadWaits = 0;
		seeds.lateFrames = seeds.lateMs = 0;
		seeds.lateMaxFrames = seeds.lateMaxMs = 0;

		ImageReplace::LogSeedSources();
		DebugLog("twin coverage 5s: copies from sources carrying 4K %u (px %u) | from 1x-only twins %u (px %u) | from untwinned memory %u (px %u)",
			seeds.coverSeeded, (DWORD)seeds.coverSeededPx, seeds.coverUnseeded, (DWORD)seeds.coverUnseededPx, seeds.coverUntwinned, (DWORD)seeds.coverUntwinnedPx);
		seeds.coverSeeded = seeds.coverUnseeded = seeds.coverUntwinned = 0;
		seeds.coverSeededPx = seeds.coverUnseededPx = seeds.coverUntwinnedPx = 0;

		DWORD top[COVER_TOP];
		DWORD found = 0;
		for (DWORD i = 0; i < game.top; ++i)
		{
			const Record* twin = &game.twins[i];
			if (!twin->owner || !twin->coverPx)
				continue;
			DWORD slot = found < COVER_TOP ? found++ : COVER_TOP;
			while (slot && game.twins[top[slot - 1]].coverPx < twin->coverPx)
			{
				if (slot < COVER_TOP)
					top[slot] = top[slot - 1];
				--slot;
			}
			if (slot < COVER_TOP)
				top[slot] = i;
		}

		for (DWORD line = 0; line * 5 < found; ++line)
		{
			CHAR text[5 * 80 + 1];
			CHAR* ptr = text;
			*ptr = 0;
			for (DWORD k = line * 5; k < found && k < line * 5 + 5; ++k)
			{
				const Record* twin = &game.twins[top[k]];
				const CHAR* store = ImageReplace::MemoryName(twin->base);
				ptr += StrPrint(ptr, " [%u] %u %s %08X %dx%d px %u n %u %.20s", k + 1, top[k], twin->anonymous ? "anon" : twin->memory ? "mem" : "surf", (DWORD)twin->owner,
					twin->width, twin->height, twin->coverPx, twin->coverCopies, store ? store : "-");
			}
			DebugLog("twin coverage top 1x-only copy sources (rank twin kind buffer size px copies store):%s", text);
		}

		TrapPriority(top, found);
		LogTransfers();
		TrapLog();
		LogCopyIns(top, found);
		for (DWORD line = 0; line * 3 < found && line * 3 < ARRIVAL_TOP; ++line)
		{
			CHAR text[3 * 240 + 1];
			CHAR* ptr = text;
			*ptr = 0;
			for (DWORD k = line * 3; k < found && k < line * 3 + 3 && k < ARRIVAL_TOP; ++k)
			{
				const Record* twin = &game.twins[top[k]];
				const Arrival* a = &twin->arrive;
				ptr += StrPrint(ptr, " [%u] %u %dx%d up %u/%u s%u wr %u fi %u in %u/%u/%u px %u lk %u/%u %08X<%08X x%u f%u tr %u %08X;",
					k + 1, top[k], twin->width, twin->height, a->uploads, a->uploadPx, a->seedUploads, a->writes, a->fills, a->in4K, a->in1x, a->inUntwinned, a->pixelIn,
					a->locks, a->unlocks, a->lockCaller, a->lockOuter, a->transfers, a->guardFails, a->trapWrites, a->trapEip);
			}
			DebugLog("twin arrival top 1x-only (up n/px, s seed ups, wr hooked writes, fi fills, in 4K/1x/untwinned, px pixel ops, lk lock/unlock caller<outer, x transfers, f guard fails, tr trapped game writes and the last writer):%s", text);
		}

		for (DWORD i = 0; i < game.top; ++i)
			game.twins[i].coverPx = game.twins[i].coverCopies = 0;
	}
#pragma endregion

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
		DWORD id;
		Record* twin = Find(buffer, &id);
		if (twin)
		{
			XferWatchCreated(twin, id);
			TrapAttach(twin, id);
		}
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
		twin->seedCount = 0;
		twin->coverPx = twin->coverCopies = 0;
		MemoryZero(&twin->arrive, sizeof(twin->arrive));
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

		SeedWrite(twin, &area);
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

	VOID OnCopyCaller(DWORD caller, DWORD outer)
	{
		game.copyCaller = caller;
		game.copyOuter = outer;
	}

	VOID CopyInText(CHAR* text, const CopyIn* c)
	{
		CHAR caller[48], outer[48], note[160];
		TrapName(c->caller, caller);
		TrapName(c->outer, outer);
		NoteText(note, &c->note);
		StrPrint(text, "from twin %u %s %dx%d (%08X) %d,%d %dx%d to %d,%d%s %s (src arts %u) caller %s < %s frame %u | src up %u wr %u in1x %u untw %u tr %u, its last write there: %s",
			c->srcId, c->srcMemory ? "mem" : "surf", c->srcW, c->srcH, (DWORD)c->srcOwner, c->from.left, c->from.top, c->from.right - c->from.left, c->from.bottom - c->from.top,
			c->x, c->y, c->keyed ? " keyed" : "", c->is4K ? "4K" : "1x", c->srcArts, caller, outer, c->frameNo, c->srcUploads, c->srcWrites, c->srcIn1x, c->srcInUntwinned, c->srcTraps, note);
	}

	BOOL TrapIsPriority(DWORD id)
	{
		return id < TWIN_MAX && trap.ranges[id].priority;
	}

	// which twin, rect and game code filled a surface, and how that source itself got those pixels
	VOID CopyInRecord(Record* target, DWORD dstId, const RECT* to, Record* source, DWORD srcId, const RECT* from, BOOL keyed)
	{
		Arrival* a = &target->arrive;
		CopyIn* c = a->inCount ? &a->lastIn : &a->firstIn;
		++a->inCount;
		c->frameNo = game.frameNo;
		c->srcId = srcId;
		c->srcOwner = source->owner;
		c->srcW = source->width;
		c->srcH = source->height;
		c->srcMemory = source->memory;
		c->from = *from;
		c->x = to->left;
		c->y = to->top;
		c->keyed = keyed;
		c->is4K = Carries4K(source, from);
		c->srcArts = source->artCount;
		c->caller = game.copyCaller ? game.copyCaller : GameCaller();
		c->outer = game.copyOuter;
		c->srcUploads = source->arrive.uploads;
		c->srcWrites = source->arrive.writes;
		c->srcIn1x = source->arrive.in1x;
		c->srcInUntwinned = source->arrive.inUntwinned;
		c->srcTraps = source->arrive.trapWrites;
		const WriteNote* n = LastNote(source, from);
		if (n)
			c->note = *n;
		else
		{
			MemoryZero(&c->note, sizeof(c->note));
			c->note.kind = NoteKinds;
		}

		if ((Area(to) >= COPYIN_BIG || TrapIsPriority(dstId)) && game.copyInLogs < COPYIN_LOGS)
		{
			++game.copyInLogs;
			CHAR text[700];
			CopyInText(text, c);
			DebugLog("twin copy in #%u: twin %u %dx%d (%08X) <- %s", game.copyInLogs, dstId, target->width, target->height, (DWORD)target->owner, text);
		}
	}

	VOID LogCopyIns(const DWORD* top, DWORD found)
	{
		for (DWORD k = 0; k < found && k < 4; ++k)
		{
			const Record* twin = &game.twins[top[k]];
			const Arrival* a = &twin->arrive;
			if (!a->inCount)
				continue;
			CHAR first[700], last[700];
			CopyInText(first, &a->firstIn);
			if (a->inCount > 1)
				CopyInText(last, &a->lastIn);
			DebugLog("twin arrival copies in [%u] twin %u %dx%d: %u, first %s", k + 1, top[k], twin->width, twin->height, a->inCount, first);
			if (a->inCount > 1)
				DebugLog("twin arrival copies in [%u] twin %u: last %s", k + 1, top[k], last);
		}
	}

	VOID OnCopy(StateBuffer* dst, LONG x, LONG y, StateBuffer* src, const RECT* srcRect, BOOL keyed, DWORD key)
	{
		Timed timed("OnCopy");
		DWORD caller = game.copyCaller, outer = game.copyOuter;
		game.copyCaller = game.copyOuter = 0;
		if (!Active() || !srcRect)
			return;

		Sync();
		TrapCopyFlush();

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
		game.copySrcData = src ? (DWORD)src->data : 0;
		game.copyCaller = caller;
		game.copyOuter = outer;
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
		game.copyCaller = game.copyOuter = game.copySrcData = 0;
	}

	VOID CopyTwins(Record* target, DWORD dstId, LONG x, LONG y, Record* source, DWORD srcId, const RECT* srcRect, BOOL keyed, DWORD key)
	{
		RECT whole = { x, y, x + srcRect->right - srcRect->left, y + srcRect->bottom - srcRect->top };

		if (!source)
		{
			if (game.list || CurrentList())
				++game.list->counters.unknown;
			CountCoverage(NULL, &whole);
			++target->arrive.inUntwinned;
			Note(target, NoteUntwinned, &whole, game.copySrcData);
			if (Area(&whole) >= COPYIN_BIG && game.copyInLogs < COPYIN_LOGS)
			{
				++game.copyInLogs;
				CHAR src[96], caller[48], outer[48];
				RegText(game.copySrcData, src);
				TrapName(game.copyCaller, caller);
				TrapName(game.copyOuter, outer);
				DebugLog("twin copy in #%u: twin %u %dx%d (%08X) at %d,%d %dx%d <- untwinned surface data %s | caller %s < %s | frame %u", game.copyInLogs, dstId, target->width, target->height,
					(DWORD)target->owner, whole.left, whole.top, whole.right - whole.left, whole.bottom - whole.top, src, caller, outer, game.frameNo);
			}
			XferWatchWrite(target, dstId, &whole);
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

		SeedWrite(target, &to);

		// a ground block still being built must get its 4K command recorded before anything copies out of it
		ImageReplace::GroundFlush((const DWORD*)source->base + from.top * source->pitch + from.left);

		FlushRegion(source, srcId, &from);
		Verify(source, srcId, &from, NULL, 0, FALSE);
		CountCoverage(source, &from);
		if (source != target)
		{
			if (Carries4K(source, &from))
				++target->arrive.in4K;
			else
				++target->arrive.in1x;
			CopyInRecord(target, dstId, &to, source, srcId, &from, keyed);
			Note(target, NoteCopy, &to, srcId);
		}

		FlushRegion(target, dstId, &to, TRUE);
		ArtPlan plan;
		BOOL hasArt = !SeedMode() && IdentifyArt(target, dstId, &to, source, srcId, &from, x, y, srcRect, keyed, key, FALSE, FALSE, &plan);
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

		// 4K texels never equal another key exactly, so a copy keyed on anything but the decode key takes its coverage from the 1x pixels, as the CPU does
		Cmd* cmd = NULL;
		DWORD maskOffset = 0;
		BOOL masked = keyed && source != target && (key | ALPHA_COMPONENT) != COLORKEY_CHECK && SeedMode() && Carries4K(source, &from);
		if (masked)
		{
			LONG cw = to.right - to.left, ch = to.bottom - to.top;
			BYTE* mask = cw <= MASK_SIZE && ch <= MASK_SIZE ? Reserve(cw * ch, &maskOffset) : NULL;
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
			if (cmd)
				++seeds.maskedCopies;
			else
			{
				++seeds.maskedSkipped;
				masked = FALSE;
			}
		}
		if (!cmd)
			cmd = Emit(CmdCopy);
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
		cmd->flags = masked ? 2 : keyed ? 1 : 0;
		if (masked)
			cmd->extra = maskOffset;

		++game.list->counters.copies;
		if (keyed)
			++game.list->counters.keyed;
		else if (SeedMode() && source->seedCount)
			SeedDerive(target, dstId, &to, source, &from, FALSE);

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

		SeedWrite(twin, &area);
		++twin->arrive.fills;
		Note(twin, NoteFill, &area, color);
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

		++twin->arrive.writes;
		Note(twin, NoteWrite, &rect, 0);
		XferWatchWrite(twin, id, &rect);
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
		{
			RECT whole = { 0, 0, twin->width, twin->height };
			++twin->arrive.writes;
			Note(twin, NoteWhole, &whole, 0);
			SeedWrite(twin, &whole);
			MarkAll(twin);
		}
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
		SeedWrite(target, &to);

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
			CountCoverage(NULL, &to);
			++target->arrive.inUntwinned;
			XferWatchWrite(target, dstId, &to);
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
		CountCoverage(source, &from);
		++target->arrive.pixelIn;
		Note(target, NotePixel, &to, srcId);
		FlushRegion(target, dstId, &to, TRUE);
		DropRegions(target, &to);

		if (mode == PixelKeyed && (param & 0xFF) >= 255)
		{
			source->keyed = TRUE;
			source->key = key | ALPHA_COMPONENT;
			if (!SeedMode() && IdentifyArt(target, dstId, &to, source, srcId, &from, dx, dy, &from, TRUE, key, TRUE, FALSE))
			{
				++game.pixelArt;
				AddArt(target, &to, &art.lastTag);
				DeferSync(target, dstId, &to);
				return;
			}
		}

		ArtPlan plan;
		BOOL hasArt = mode == PixelCopy && !SeedMode() && IdentifyArt(target, dstId, &to, source, srcId, &from, dx, dy, &from, FALSE, 0, FALSE, TRUE, &plan);

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

		// the face copy (DrawFaces) is mirrored for units facing the other way; it replaces its rect like any unkeyed copy
		if (mode == PixelCopy && mirror && SeedMode() && source->seedCount)
			SeedDerive(target, dstId, &to, source, &from, TRUE);

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

		SeedWrite(twin, &rect);
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
		SeedWrite(target, &to);

		DWORD srcId;
		Record* source = Adopt(src, &srcId);
		LONG sx, sy;
		if (!source || source == target || !PixelPos(source, src, source->pitch, length, 1, &sx, &sy))
		{
			++game.pixelUntwinned;
			if (!source)
				CountCoverage(NULL, &to);
			MarkDirty(target, to);
			return;
		}
		RECT from = { sx, sy, sx + length, sy + 1 };

		++game.blendRows;
		DrainSyncIn(source, &from);
		FlushRegion(source, srcId, &from);
		Verify(source, srcId, &from, NULL, 0, FALSE);
		CountCoverage(source, &from);
		++target->arrive.pixelIn;
		Note(target, NotePixel, &to, srcId);
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

		SeedWrite(twin, &rect);
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

	VOID SlowText(CHAR* text, const SlowCmd* w);

	VOID LogReplayTimes()
	{
		Lock();
		ReplayTimes t = shared.times;
		MemoryZero(&shared.times, sizeof(shared.times));
		LONGLONG inlineBytes = shared.inlineBytes, inlineUs = shared.inlineUs;
		DWORD inlineUploads = shared.inlineUploads;
		shared.inlineBytes = shared.inlineUs = 0;
		shared.inlineUploads = 0;
		Unlock();

		CHAR text[1000];
		CHAR* ptr = text;
		*ptr = 0;
		for (DWORD k = 0; k < CMD_TYPES; ++k)
			if (t.count[k])
			{
				DWORD total = DWORD(t.us[k] / 1000);
				ptr += StrPrint(ptr, " %s %u %u ms max %u.%03u;", cmdNames[k], t.count[k], total, t.max[k] / 1000, t.max[k] % 1000);
			}
		DWORD inlineMs = DWORD(inlineUs / 1000);
		DebugLog("twin replay per command 5s (type, dispatches, total, max):%s | replays over %u ms %u | inline 4K uploads %u (%u KB, %u ms)", text, REPLAY_SLOW_US / 1000, t.slowReplays,
			inlineUploads, DWORD(inlineBytes / 1024), inlineMs);
		if (t.slow.us)
		{
			SlowText(text, &t.slow);
			DebugLog("twin replay slowest command 5s: %s", text);
		}
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
		LogReplayTimes();

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
		LogSeeds();
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
		DWORD screenId;
		Record* shown = Find(screen, &screenId);
		if (shown)
			shown->screen = TRUE;

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
			{
				for (DWORD i = 0; i < game.top; ++i)
					if (game.twins[i].owner)
						MarkAll(&game.twins[i]);
				SeedRepend("tile check on");
			}
		}
		game.tileKeyDown = down;

		down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) && (GetAsyncKeyState(VK_F10) & 0x8000);
		if (down && !game.tintKeyDown)
		{
			shared.tint = !shared.tint;
			DebugLog("twin art tint %s", shared.tint ? "magenta" : "off");
			// seeds are drawn once at the decode; redrawing the intact ones lets the tint show what came from seeds
			if (SeedMode())
				SeedRepend("tint toggle");
		}
		game.tintKeyDown = down;

		BOOL shot = game.shotRequest;
		if (GpuCheckWanted() && art.firstTick && !art.shotTaken && now - art.firstTick >= SHOT_DELAY)
		{
			art.shotTaken = TRUE;
			shot = TRUE;
		}
		if (shot)
		{
			game.shotRequest = FALSE;
			game.checkRequest = TRUE;
		}

		TrapCopyFlush();
		XferResolve(TRUE);
		TrapRearm(now);
		if (seeds.active)
			SeedPoll();

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
		if (twin && (game.checkRequest || (GpuCheckWanted() && now - game.checkTick >= CHECK_PERIOD)))
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

		// the next list's inline 4K uploads are bounded by the time the render thread measured for them
		Lock();
		LONGLONG rateBytes = shared.rateBytes, rateUs = shared.rateUs;
		if (rateUs > 2000000)
		{
			shared.rateBytes /= 2;
			shared.rateUs /= 2;
		}
		Unlock();
		LONGLONG budget = rateUs > 1000 ? rateBytes * SEED_INLINE_MS * 1000 / rateUs : SEED_INLINE_MS * 1024 * 1024;
		seeds.inlineBudget = max(min(budget, (LONGLONG)SEED_INLINE_MAX), (LONGLONG)SEED_INLINE_MIN);
		seeds.inlineUsed = 0;
		seeds.loadWaitTotalUs = 0;
		seeds.lastPresentTick = GetTickCount() | 1;

		if (now - game.logTick >= LOG_PERIOD)
			LogInterval(now);

		if (!SeedMode() && index.dirty && now - index.saveTick >= INDEX_SAVE_PERIOD && !InterlockedCompareExchange(&learn.saving, 1, 0))
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
		// the shared-block check reads back from the GPU, so it runs only once asked for
		if (SeedMode() && !seeds.conflictCheck)
		{
			seeds.conflictCheck = TRUE;
			SeedRepend("F12 conflict check");
		}
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
		else if (tag->frame == TAG_GROUND)
			StrPrint(text, "ground twin %u", tag->src);
		else if (tag->frame == TAG_SEED_SHEET)
			StrPrint(text, "seeded sheet twin %u at %d,%d of %dx%d", tag->src, tag->cx, tag->cy, tag->canvasW, tag->canvasH);
		else if (tag->frame == TAG_MERGED_4K)
			StrPrint(text, "%s", "merged art rects, some 4K");
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

		// flag 1: the destination key test; flag 2: a keyed copy whose coverage is the source's exact 1x key mask
		BOOL dstKeyed = (cmd->flags & 1) != 0;
		BOOL exact = (cmd->flags & 2) != 0;
		FLOAT ew = 0.0f, eh = 0.0f;
		if (dstKeyed && !ToScratch(dst, &to, &ew, &eh))
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
		if (exact)
			SetNearest(GL_NEAREST);
		if (dstKeyed)
		{
			GLActiveTexture(GL_TEXTURE2);
			GLBindTexture(GL_TEXTURE_2D, gl.scratch);
		}
		GLActiveTexture(GL_TEXTURE0);
		GLBindTexture(GL_TEXTURE_2D, src->texture);

		TargetTwin(cmd->dst);
		gl.program = gl.maskProgram;
		GLUseProgram(gl.maskProgram);
		GLUniform1i(gl.maskKeyedLoc, dstKeyed ? 1 : 0);
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
		if (exact)
		{
			GLActiveTexture(GL_TEXTURE1);
			SetNearest(GL_LINEAR);
			GLActiveTexture(GL_TEXTURE0);
		}
	}

	DWORD GlUs(LONGLONG ticks);

	BOOL SeedTexture(LONG frame, GLuint* texture, LONG* width, LONG* height)
	{
		if (ImageReplace::ArtTexture(frame, texture, width, height) && *texture)
			return TRUE;
		LONG bytes = ImageReplace::ArtBytes(frame);
		LARGE_INTEGER start, end;
		QueryPerformanceCounter(&start);
		if (!ImageReplace::ArtUpload(frame))
			return FALSE;
		QueryPerformanceCounter(&end);
		++gl.inlineUploads;
		gl.inlineUs += GlUs(end.QuadPart - start.QuadPart);
		gl.inlineBytes += bytes;

		// the upload ran ImageReplace's GL; restore what the twin draws cache
		gl.program = 0;
		gl.target = TWIN_NONE;
		GLBindVertexArray(gl.vao);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLActiveTexture(GL_TEXTURE0);
		return ImageReplace::ArtTexture(frame, texture, width, height) && *texture;
	}

	VOID PieceUv(const RECT* to, const RECT* place, FLOAT* u0, FLOAT* v0, FLOAT* u1, FLOAT* v1)
	{
		FLOAT pw = (FLOAT)(place->right - place->left), ph = (FLOAT)(place->bottom - place->top);
		*u0 = (to->left - place->left) / pw;
		*u1 = (to->right - place->left) / pw;
		*v0 = (to->top - place->top) / ph;
		*v1 = (to->bottom - place->top) / ph;
	}

	// a block the packer shares between frames carries the first frame's 4K; this measures how far the other frames' 4K for it is from that
	VOID CheckConflicts(DWORD dst, const SeedConflict* conflicts, DWORD count)
	{
		LONG cell = SEED_CONFLICT_CELL;
		LONG width = cell * 2 * SEED_CONFLICT_BATCH, height = cell;
		if (!gl.conflictData)
			gl.conflictData = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
		if (!gl.conflictData || !EnsureTarget(&gl.conflict, &gl.conflictFbo, &gl.conflictW, &gl.conflictH, width, height))
		{
			gl.conflictsSkipped += count;
			return;
		}

		RECT differing[SEED_CONFLICTS];
		DWORD differingCount = 0;

		for (DWORD base = 0; base < count; base += SEED_CONFLICT_BATCH)
		{
			DWORD n = min(count - base, (DWORD)SEED_CONFLICT_BATCH);
			GLuint textures[SEED_CONFLICT_BATCH][2];
			BOOL ok[SEED_CONFLICT_BATCH];
			for (DWORD k = 0; k < n; ++k)
			{
				LONG w, h;
				const SeedConflict* c = &conflicts[base + k];
				ok[k] = SeedTexture(c->frameA, &textures[k][0], &w, &h) && SeedTexture(c->frameB, &textures[k][1], &w, &h);
			}

			Target(TARGET_CONFLICT, gl.conflictFbo, gl.conflictW, gl.conflictH);
			GLClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			GLClear(GL_COLOR_BUFFER_BIT);
			UseCopy(FALSE, 0);
			GLUniform1i(gl.artLoc, 1);
			GLUniform1i(gl.tintLoc, 0);
			for (DWORD k = 0; k < n; ++k)
			{
				if (!ok[k])
					continue;
				const SeedConflict* c = &conflicts[base + k];
				for (DWORD side = 0; side < 2; ++side)
				{
					FLOAT u0, v0, u1, v1;
					PieceUv(&c->to, side ? &c->placeB : &c->placeA, &u0, &v0, &u1, &v1);
					FLOAT x0 = (FLOAT)((k * 2 + side) * cell);
					GLBindTexture(GL_TEXTURE_2D, textures[k][side]);
					gl.vertCount = 0;
					AddQuad((FLOAT)gl.conflictW, (FLOAT)gl.conflictH, x0, 0.0f, x0 + cell, (FLOAT)cell, u0, v0, u1, v1);
					DrawVerts();
				}
			}
			GLUniform1i(gl.artLoc, 0);

			GLBindFramebuffer(GL_READ_FRAMEBUFFER, gl.conflictFbo);
			GLPixelStorei(GL_PACK_ALIGNMENT, 4);
			GLReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, gl.conflictData);
			GLBindFramebuffer(GL_READ_FRAMEBUFFER, gl.readFbo);

			for (DWORD k = 0; k < n; ++k)
			{
				if (!ok[k])
				{
					++gl.conflictsSkipped;
					continue;
				}

				DWORD diff = 0;
				for (LONG y = 0; y < cell; ++y)
				{
					const BYTE* a = (const BYTE*)(gl.conflictData + y * width + k * 2 * cell);
					const BYTE* b = a + cell * sizeof(DWORD);
					for (LONG i = 0; i < cell * 4; ++i)
						diff += a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];
				}
				diff /= cell * cell * 4;
				++gl.conflictsChecked;
				if (diff <= SEED_CONFLICT_DIFF)
					continue;

				++gl.conflictsDiffer;
				const SeedConflict* c = &conflicts[base + k];
				BOOL known = FALSE;
				for (DWORD d = 0; d < differingCount && !known; ++d)
					known = EqualRect(&differing[d], &c->to);
				if (!known && differingCount < SEED_CONFLICTS)
				{
					differing[differingCount++] = c->to;
					++gl.conflictBlocks;
				}
				if (gl.conflictLogs < SEED_CONFLICT_LOGS)
				{
					++gl.conflictLogs;
					DebugLog("twin seed conflict twin %u block %d,%d %dx%d holds frame %d, frame %d's 4K for it differs by mean %u (limit %u)", dst, c->to.left, c->to.top,
						c->to.right - c->to.left, c->to.bottom - c->to.top, c->frameA, c->frameB, diff, SEED_CONFLICT_DIFF);
				}
			}
		}
	}

	VOID ReplaySeed(const Cmd* cmd, const List* list)
	{
		if (!Valid(cmd->dst))
			return;

		const SeedPiece* pieces = (const SeedPiece*)(list->arena + cmd->value);
		DWORD count = (DWORD)cmd->x;
		const SeedConflict* conflicts = (const SeedConflict*)(pieces + count);
		DWORD conflictCount = (DWORD)cmd->y;

		Gpu* gpu = &gl.twins[cmd->dst];
		LONG scale = gpu->scale;
		FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);
		++gl.seedReplayed;

		for (DWORD i = 0; i < count;)
		{
			LONG frame = pieces[i].frame;
			DWORD end = i + 1;
			while (end < count && pieces[end].frame == frame)
				++end;

			GLuint texture = 0;
			LONG w, h;
			if (!SeedTexture(frame, &texture, &w, &h))
			{
				gl.seedMissing += end - i;
				i = end;
				continue;
			}

			TargetTwin(cmd->dst);
			UseCopy(FALSE, 0);
			GLUniform1i(gl.artLoc, 1);
			GLUniform1i(gl.tintLoc, shared.tint ? 1 : 0);
			GLBindTexture(GL_TEXTURE_2D, texture);
			// the key-alpha upload recorded just before is the cut: key texels stay transparent black, the rest take the 4K texel and its alpha
			GLEnable(GL_BLEND);
			GLBlendFunc(GL_DST_ALPHA, GL_ZERO);
			gl.vertCount = 0;
			for (DWORD k = i; k < end; ++k)
			{
				const SeedPiece* p = &pieces[k];
				FLOAT u0, v0, u1, v1;
				PieceUv(&p->to, &p->place, &u0, &v0, &u1, &v1);
				AddQuad(tw, th, (FLOAT)(p->to.left * scale), (FLOAT)(p->to.top * scale), (FLOAT)(p->to.right * scale), (FLOAT)(p->to.bottom * scale), u0, v0, u1, v1);
			}
			DrawVerts();
			GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			GLDisable(GL_BLEND);
			gl.seedPieces += end - i;
			i = end;
		}
		GLUniform1i(gl.artLoc, 0);
		GLUniform1i(gl.tintLoc, 0);

		if (conflictCount)
			CheckConflicts(cmd->dst, conflicts, conflictCount);
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

	struct ShotJob {
		const CHAR* name;
		BYTE* pixels;
		LONG width;
		LONG height;
		LONG stride;
	};

	DWORD __stdcall ShotWorker(LPVOID param)
	{
		ShotJob* job = (ShotJob*)param;
		WritePng(job->name, job->pixels, job->width, job->height, job->stride);
		MemoryFree(job->pixels);
		MemoryFree(job);
		return 0;
	}

	// PNG compression took over a second on the render thread; it runs on its own thread and owns the pixels
	VOID WritePngAsync(const CHAR* name, BYTE* pixels, LONG width, LONG height, LONG stride)
	{
		ShotJob* job = (ShotJob*)MemoryAlloc(sizeof(ShotJob));
		if (job)
		{
			job->name = name;
			job->pixels = pixels;
			job->width = width;
			job->height = height;
			job->stride = stride;
			HANDLE thread = CreateThread(NULL, 0, ShotWorker, job, 0, NULL);
			if (thread)
			{
				CloseHandle(thread);
				return;
			}
			MemoryFree(job);
		}
		WritePng(name, pixels, width, height, stride);
		MemoryFree(pixels);
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
		WritePngAsync(name, pixels, width, height, width * 4);
	}

	VOID Shots(const Cmd* cmd, const List* list)
	{
		if (!pnglib_create_write_struct)
		{
			DebugLog("twin shot skipped: png library not loaded");
			return;
		}

		if (cmd->flags & PresentCheck)
		{
			BYTE* copy = (BYTE*)MemoryAlloc(cmd->x * cmd->y * 4);
			if (copy)
			{
				MemoryCopy(copy, list->arena + cmd->value, cmd->x * cmd->y * 4);
				WritePngAsync("twin-1x.png", copy, cmd->x, cmd->y, cmd->x * 4);
			}
		}
		ShotTwin("twin-screen.png", cmd->dst, SHOT_SCREEN_W, SHOT_SCREEN_H, TRUE);
		ShotTwin("twin-sheet.png", (DWORD)cmd->clip.left, SHOT_SHEET, SHOT_SHEET, FALSE);
	}

	DWORD GlUs(LONGLONG ticks)
	{
		return gl.frequency.QuadPart ? DWORD(ticks * 1000000 / gl.frequency.QuadPart) : 0;
	}

	DWORD ReplayOne(const List* list, DWORD i)
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
			return ReplayFills(list, i, FALSE);

		case CmdTint:
			return ReplayFills(list, i, TRUE);

		case CmdMask:
			ReplayMask(cmd, list);
			break;

		case CmdLighter:
			ReplayLighter(cmd);
			break;

		case CmdUpload:
			return ReplayUploads(list, i);

		case CmdArt:
			return ReplayArt(list, i);

		case CmdGround:
			ReplayGround(cmd, list);
			break;

		case CmdSeed:
			ReplaySeed(cmd, list);
			break;

		case CmdPixel:
			return ReplayPixels(list, i);

		case CmdCopy:
			return ReplayCopies(list->cmds, list->count, i);

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
		return i + 1;
	}

	// every dispatch is timed, so a stall names the command that caused it
	VOID Replay(const List* list)
	{
		DWORD i = 0;
		LARGE_INTEGER start, end;
		QueryPerformanceCounter(&start);
		while (i < list->count)
		{
			const Cmd* cmd = &list->cmds[i];
			DWORD uploads = gl.inlineUploads;
			LONGLONG uploadUs = gl.inlineUs;
			DWORD next = ReplayOne(list, i);
			if (next <= i)
				next = i + 1;
			QueryPerformanceCounter(&end);
			DWORD us = GlUs(end.QuadPart - start.QuadPart);
			start = end;

			DWORD type = cmd->type < CMD_TYPES ? cmd->type : 0;
			gl.times.us[type] += us;
			++gl.times.count[type];
			if (us > gl.times.max[type])
				gl.times.max[type] = us;
			if (us > gl.drawSlow.us)
			{
				SlowCmd* w = &gl.drawSlow;
				w->us = us;
				w->type = type;
				w->dst = cmd->dst;
				w->src = cmd->src;
				w->rect = cmd->rect;
				w->x = cmd->x;
				w->y = cmd->y;
				w->flags = cmd->flags;
				w->extra = cmd->extra;
				w->cmds = next - i;
				w->uploads = gl.inlineUploads - uploads;
				w->uploadUs = DWORD(gl.inlineUs - uploadUs);
			}
			i = next;
		}
	}

	VOID SlowText(CHAR* text, const SlowCmd* w)
	{
		StrPrint(text, "%s dst %u src %u rect %d,%d %dx%d x %d y %d flags %X extra %u (%u commands in the run, inline 4K uploads %u in %u.%03u ms) %u.%03u ms",
			cmdNames[w->type], w->dst, w->src, w->rect.left, w->rect.top, w->rect.right - w->rect.left, w->rect.bottom - w->rect.top, w->x, w->y, w->flags, w->extra,
			w->cmds, w->uploads, w->uploadUs / 1000, w->uploadUs % 1000, w->us / 1000, w->us % 1000);
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
			MemoryZero(&gl.drawSlow, sizeof(gl.drawSlow));
			DWORD commands = 0;
			QueryPerformanceCounter(&start);
			for (List* list = lists; list; list = list->next)
			{
				Replay(list);
				commands += list->count;
				++replayed;
			}
			QueryPerformanceCounter(&end);
			if (gl.frequency.QuadPart)
				us = DWORD((end.QuadPart - start.QuadPart) * 1000000 / gl.frequency.QuadPart);

			if (gl.drawSlow.us > gl.times.slow.us)
				gl.times.slow = gl.drawSlow;
			if (us > REPLAY_SLOW_US)
			{
				++gl.times.slowReplays;
				if (gl.slowLogs < REPLAY_SLOW_LOGS)
				{
					++gl.slowLogs;
					CHAR text[400];
					SlowText(text, &gl.drawSlow);
					DebugLog("twin slow replay %u.%03u ms (%u lists, %u commands): slowest %s", us / 1000, us % 1000, replayed, commands, text);
				}
			}

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
		shared.seedReplayed += gl.seedReplayed;
		shared.seedPieces += gl.seedPieces;
		shared.seedMissing += gl.seedMissing;
		shared.conflictsChecked += gl.conflictsChecked;
		shared.conflictsDiffer += gl.conflictsDiffer;
		shared.conflictsSkipped += gl.conflictsSkipped;
		shared.conflictBlocks += gl.conflictBlocks;
		gl.seedReplayed = gl.seedPieces = gl.seedMissing = gl.conflictsChecked = gl.conflictsDiffer = gl.conflictsSkipped = gl.conflictBlocks = 0;
		for (DWORD t = 0; t < CMD_TYPES; ++t)
		{
			shared.times.us[t] += gl.times.us[t];
			shared.times.count[t] += gl.times.count[t];
			if (gl.times.max[t] > shared.times.max[t])
				shared.times.max[t] = gl.times.max[t];
		}
		if (gl.times.slow.us > shared.times.slow.us)
			shared.times.slow = gl.times.slow;
		shared.times.slowReplays += gl.times.slowReplays;
		MemoryZero(&gl.times, sizeof(gl.times));
		shared.inlineBytes += gl.inlineBytes;
		shared.inlineUs += gl.inlineUs;
		shared.inlineUploads += gl.inlineUploads;
		shared.rateBytes += gl.inlineBytes;
		shared.rateUs += gl.inlineUs;
		gl.inlineBytes = gl.inlineUs = 0;
		gl.inlineUploads = 0;
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
			if (gl.conflict)
				GLDeleteTextures(1, &gl.conflict);
			if (gl.conflictFbo)
				GLDeleteFramebuffers(1, &gl.conflictFbo);
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
		if (gl.conflictData)
			MemoryFree(gl.conflictData);
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
