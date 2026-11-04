#include "StdAfx.h"
#include "Config.h"
#include "GLib.h"
#include "Hooks.h"
#include "ImageReplace.h"
#include "MapOrder.h"
#include "Twin.h"
#include "intrin.h"
#include "../libwebp/src/webp/decode.h"

#define f(a) (a + baseOffset)

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
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS 0x8B81
#endif
#ifndef GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#endif
#ifndef GL_TEXTURE_FREE_MEMORY_ATI
#define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC
#endif

namespace TextReplace
{
	extern DWORD traceLeft;
}

namespace ImageReplace
{
	#define TABLE_BITS 16
	#define TABLE_SIZE (1 << TABLE_BITS)
	#define NAME_LENGTH 40
	#define FLAG_CHECK_MS 1000

	#define ANIM_BITS 15
	#define ANIM_SIZE (1 << ANIM_BITS)
	#define REQUEST_SIZE 4096
	#define ENTRY_LIMIT (TABLE_SIZE * 3 / 4)
	#define DECODE_LIMIT (128 * 1024 * 1024)
	#define DECODE_PREFETCH_LIMIT (DECODE_LIMIT * 3 / 4)
	#define DECODE_STALE 120
	#define GPU_FALLBACK (1536 * 1024 * 1024)
	#define GPU_MIN (256 * 1024 * 1024)
	#define UPLOAD_US 4000
	#define UPLOAD_BATCH 64
	#define WORKER_WAIT_MS 100

	enum TexState { TexNone, TexQueued, TexDecoded, TexUploaded, TexFailed };

	enum TexList { ListNone, ListDecoded, ListGpu };

	enum PackFormat { PackWebP };

	enum Origin { OriginCanvas, OriginCentre, OriginSprite };

	struct Pair {
		const CHAR* key;
		const CHAR* value;
	};

	const Pair races[] = { { "human", "HU" }, { "undead", "UN" }, { "heretic", "HE" }, { "dwarf", "DW" }, { "neutral", "NE" }, { "elf", "EL" } };
	const Pair raceIds[] = { { "human", "0000" }, { "undead", "0003" }, { "heretic", "0002" }, { "dwarf", "0001" }, { "neutral", "8888" }, { "elf", "0005" } };
	const Pair boatVariants[] = { { "human", "0" }, { "undead", "1" }, { "heretic", "2" }, { "dwarf", "3" }, { "neutral", "4" }, { "elf", "5" } };
	const Pair capitalLetters[] = { { "human", "H" }, { "undead", "U" }, { "heretic", "E" }, { "dwarf", "D" }, { "elf", "F" } };
	const Pair sites[] = { { "mage", "MAGE" }, { "merchant", "MERH" }, { "mercenary", "MERC" }, { "trainer", "TRAI" } };
	const Pair crystals[] = { { "gold", "GL" }, { "death-mana", "RG" }, { "rune-mana", "WH" }, { "life-mana", "YE" }, { "infernal-mana", "RD" }, { "grove-mana", "GR" } };
	const Pair poses[] = { { "idle", "IDLE" }, { "attack", "HMOV" }, { "hit", "HHIT" } };
	const Pair sides[] = { { "attacker", "A" }, { "defender", "D" } };
	const Pair layers[] = { { "shadow", "S1" }, { "body", "A1" }, { "aura", "A2" } };
	const Pair attackSides[] = { { "attack", "A" }, { "defend", "D" }, { "both", "B" } };
	const Pair attackTypes[] = { { "hit", "HEFF" }, { "touch", "TUCH" } };
	const Pair items[] = { { "potion", "G000IG1111" }, { "orb", "G000IG2222" }, { "talisman", "G000IG3333" } };
	const Pair spellLayers[] = { { "ground", "G" }, { "under", "U" }, { "front", "F" }, { "over", "O" }, { "top", "T" } };
	const Pair statuses[] = {
		{ "poison", "POISONANIM" }, { "frostbite", "FROSTBITEANIM" }, { "blister", "BLISTERANIM" }, { "revive", "REVIVEANIM" },
		{ "reduce-level", "DRAINLVL" }, { "transform", "TRANSF" }, { "doppelganger", "DOPPEL" }, { "unsummon", "USUMMONANIM" },
		{ "restore-level", "UDRAINLVL" }, { "untransform", "UTRANSF" }, { "undoppelganger", "UDOPPEL" }
	};
	const Pair deaths[] = {
		{ "human/small", "DEATH_HUMAN_S13" }, { "heretic/small", "DEATH_HERETIC_S13" }, { "dwarf/small", "DEATH_DWARF_S15" },
		{ "undead/small", "DEATH_UNDEAD_S15" }, { "neutral/small", "DEATH_NEUTRAL_S10" }, { "dragon/small", "DEATH_DRAGON_S15" },
		{ "ghost/small", "DEATH_GHOST_S15" }, { "elf/small", "DEATH_ELF_S15" }, { "elf/large", "DEATH_ELF_L15" }
	};
	const Pair cursors[] = {
		{ "default", "DEFAULT" }, { "wait", "WAIT" }, { "alliance", "ALLIANCE" }, { "attack", "BATTLE" }, { "negotiate", "NEGO" },
		{ "enter-city", "ENTERCITY" }, { "move", "MOVE" }, { "select-unit", "SELECTUNIT" }, { "select-city", "SELECTCITY" },
		{ "view-city", "VIEWCITY" }, { "manage-stack", "MANAGESTK" }, { "use-scroll", "SCROLLVAL" }, { "use-scroll-invalid", "SCROLLINV" },
		{ "cast-spell", "SPELLVAL" }, { "cast-spell-invalid", "SPELLINV" }, { "take-bag", "TAKEBAG" }, { "exchange", "EXCHANGE" },
		{ "visit", "VISIT" }, { "drop-bag", "DROPBAG" }, { "create-rod", "DROPRODVAL" }, { "create-rod-invalid", "DROPRODINV" },
		{ "destroy-rod", "BREAKROD" }, { "drag-invalid", "NODRAGDROP" }, { "dismiss", "DISMISSVAL" }, { "dismiss-invalid", "DISMISSINV" },
		{ "heal", "HEALVAL" }, { "heal-invalid", "HEALINV" }, { "revive", "REVIVEVAL" }, { "revive-invalid", "REVIVEINV" },
		{ "dynamic-level", "DYNLEVELVAL" }, { "dynamic-level-invalid", "DYNLEVELINV" }, { "battle-default", "CUDEFAULTA" },
		{ "battle-attack", "CUATTACKA" }, { "battle-attack-invalid", "CUATTACKNA" }, { "battle-heal", "CUHEALA" },
		{ "battle-heal-invalid", "CUHEALNA" }, { "battle-transform", "CUTRANSFA" }, { "battle-transform-invalid", "CUTRANSFNA" }
	};
	const Pair constants[] = {
		{ "battle-effect-heal", "HEALTUCHA1B00" }, { "battle-effect-blister", "BLISTUCHA1B00" },
		{ "stack-create-effect", "G000UU9999CREA8" }, { "stack-destroy-effect", "G000UU9999DYIN8" },
		{ "stack-summon-destroy-effect", "G000UU9998DYIN8" }, { "stack-owner-change-effect", "G000UU9999COWNE" },
		{ "stack-battle-effect", "ABATTLE" }, { "water", "WATERBKGND1" }
	};

	#define SPELL_PREFIX 12

	struct PackAnim;

	struct Entry {
		DWORD rec;
		BYTE flipped;
		WORD frame;
		LONG packFrame;
		BOOL resolved;
		Origin origin;
		const PackAnim* anim;
		DWORD draws;
		DWORD refs;
		DWORD nextFree;
		CHAR name[NAME_LENGTH];
		CHAR owner[NAME_LENGTH];
	};

	struct PackFrame {
		LONGLONG offset;
		DWORD length;
		LONG w, h, ox, oy;
		BOOL additive;
		PackFormat format;
	};

	struct PackAnim {
		DWORD hash;
		DWORD name;
		DWORD id;
		DWORD first;
		DWORD count;
		Origin origin;
	};

	struct Tex {
		volatile LONG state;
		BYTE* pixels;
		LONG w, h;
		DWORD bytes;
		DWORD gpuBytes;
		GLenum compressed;
		GLuint id;
		volatile LONG lastUse;
		volatile LONG prefetch;
		DWORD prev, next;
		TexList list;
	};

	struct TexChain {
		DWORD head, tail, count;
	};

	struct Vertex {
		FLOAT x, y, u, v, pu, pv, layer;
	};

	struct {
		Entry list[ENTRY_LIMIT];
		DWORD index[TABLE_SIZE];
		DWORD used;
		DWORD free;
		DWORD count;
	} entries;

	DWORD sub_Build, sub_Render, sub_Names, sub_StoreLoad, sub_StoreDraw, sub_StoreFree, sub_ImageFree, sub_FaceLoad;

	#define STORE_BITS 12
	#define STORE_SIZE (1 << STORE_BITS)
	#define ARCHIVE_LIMIT 64
	#define ARCHIVE_ENTRY_SIZE 260

	struct ArchiveFile {
		DWORD id;
		DWORD dims[4];
		CHAR name[NAME_LENGTH];
	};

	struct SheetItem {
		DWORD id;
		DWORD offset;
		DWORD next;
	};

	struct SheetLink {
		DWORD hash;
		const CHAR* frame;
		const CHAR* anim;
		DWORD index;
	};

	struct Archive {
		CHAR path[MAX_PATH];
		ArchiveFile* files;
		DWORD mask;
		BOOL sheetsLoaded;
		BYTE* indexData;
		BYTE* images;
		DWORD imagesSize;
		BYTE* animsData;
		SheetItem* items;
		DWORD itemCount;
		DWORD* itemHeads;
		DWORD itemMask;
		SheetLink* links;
		DWORD linkMask;
		DWORD linkCount;
		HANDLE reader;
	};

	struct {
		Archive list[ARCHIVE_LIMIT];
		DWORD count;
	} archives;

	#define FACE_PRINT 8
	#define FACE_PRINT_MIN 6

	struct StoreImage {
		const VOID* image;
		DWORD dims[4];
		const BYTE* data;
		LONG left, top;
		const BYTE* first;
		LONG pitch;
		LONG rangeW, rangeH;
		Entry* entry;
		ArchiveFile* file;
		DWORD nextFree;
		BOOL decodeLogged;
		BOOL printed;
		ULONGLONG printMask;
		DWORD print[FACE_PRINT * FACE_PRINT];
	};

	struct {
		StoreImage list[STORE_SIZE * 3 / 4];
		DWORD byImage[STORE_SIZE];
		DWORD byData[STORE_SIZE];
		DWORD used;
		DWORD free;
		DWORD count;
	} stores;

	#define RANGE_SCAN 256
	#define RANGE_PITCH 4096

	struct Range {
		const BYTE* start;
		const BYTE* end;
		DWORD store;
	};

	struct {
		Range list[STORE_SIZE * 3 / 4];
		DWORD count;
		DWORD span;
		DWORD hits, misses, learnStore, learnFace, dropped, rejected, captureOnly;
	} memory;

	struct {
		CHAR name[NAME_LENGTH];
		DWORD frame;
	} current;

	struct {
		DWORD builds;
		DWORD full;
		DWORD draws;
		DWORD unknown;
	} stats;

	struct {
		BOOL key;
		DWORD flagCheck;
	} trace;

	#define ART_LOG 32
	#define ART_PARTS 64

	struct {
		DWORD depth;
		DWORD begins, notReady, noFrame, zoomed, truncated;
	} art;

	struct {
		HANDLE file;
		BOOL gameNames;
		HANDLE thread;
		HANDLE wake;
		volatile LONG ready;
		BOOL failed;
		PackFrame* frames;
		DWORD frameCount;
		DWORD frameCapacity;
		CHAR* names;
		DWORD namesSize;
		DWORD namesCapacity;
		PackAnim anims[ANIM_SIZE];
		DWORD animCount;
		Tex* texs;
		HANDLE space;
		volatile LONG decoded;
		LONG gpu;
		LONG gpuBudget;
		TexChain decodedList;
		TexChain gpuList;
		volatile LONG evictGen;
		DWORD animScan[ANIM_SIZE];
	} pack;

	struct Ring {
		DWORD queue[REQUEST_SIZE];
		DWORD head, tail;
	};

	struct {
		CRITICAL_SECTION cs;
		Ring demand;
		Ring prefetch;
	} requests;

	struct {
		BOOL initialized;
		volatile LONG frameNo;
	} game;

	typedef VOID(__stdcall* GLGENERATEMIPMAP)(GLenum target);
	typedef VOID(__stdcall* GLCOMPRESSEDTEXIMAGE2D)(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLint border, GLsizei size, const GLvoid* data);

	struct {
		BOOL ready;
		BOOL failed;
		GLuint vao;
		GLuint vbo;
		GLGENERATEMIPMAP generateMipmap;
		GLCOMPRESSEDTEXIMAGE2D compressedTexImage2D;
		LARGE_INTEGER frequency;
	} gl;

	#define GROUND_OBJECTS 4096
	#define GROUND_CHUNKS 64
	#define GROUND_OPS 512
	#define GROUND_LOG 16
	#define GROUND_UNRESOLVED -2

	struct GroundObject {
		const VOID* obj;
		LONG frame;
		CHAR name[16];
	};

	struct GroundOp {
		GroundKind kind;
		LONG tex, mask1, mask2;
		SHORT dx, dy, sx, sy;
		SHORT left, top, right, bottom;
	};

	struct GroundChunk {
		const VOID* data;
		LONG left, top, cw, ch;
		DWORD hash;
		BOOL valid;
		DWORD color, version, count;
		GroundOp ops[GROUND_OPS];
		LONG pitch;
		BOOL pending;
		DWORD sentVersion;
	};

	struct GroundBlob {
		DWORD count, color;
		GroundOp ops[GROUND_OPS];
	};

	struct GroundMask {
		const VOID* obj;
		const VOID* data;
	};

	struct {
		GroundObject objects[GROUND_OBJECTS];
		GroundChunk chunks[GROUND_CHUNKS];
		DWORD chunkCount;
		GroundMask masks[2];
		DWORD builds, ops, unnamed, unmasked, rejected;
		DWORD published, arts, notReady, retried, mismatched;
		LONG retryFrame;
		GroundChunk* flush;
		const BYTE* flushStart;
		const BYTE* flushEnd;
		DWORD flushes;
	} ground;

	DWORD sub_TileFactory, sub_BorderFactory, sub_MaskData, sub_RaceCode, sub_GroundUpdate;

	struct {
		GLuint program;
		GLint mode, two;
	} groundGL;

	BOOL TakeFlag()
	{
		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, sizeof(path));
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, "imagetrace.flag");
		return DeleteFile(path);
	}

	typedef DWORD(*KEYOF)(DWORD value);

	DWORD Home(DWORD key, DWORD bits)
	{
		return (key * 2654435761u) >> (32 - bits);
	}

	DWORD IndexFind(const DWORD* index, DWORD bits, DWORD key, KEYOF keyOf, BOOL* found)
	{
		DWORD mask = (1u << bits) - 1;
		DWORD slot = Home(key, bits);
		for (DWORD i = 0; i <= mask; ++i, slot = (slot + 1) & mask)
		{
			if (!index[slot])
			{
				*found = FALSE;
				return slot;
			}
			if (keyOf(index[slot]) == key)
			{
				*found = TRUE;
				return slot;
			}
		}
		*found = FALSE;
		return mask + 1;
	}

	VOID IndexRemove(DWORD* index, DWORD bits, DWORD slot, KEYOF keyOf)
	{
		DWORD mask = (1u << bits) - 1;
		DWORD hole = slot;
		for (DWORD next = (slot + 1) & mask; index[next]; next = (next + 1) & mask)
		{
			DWORD home = Home(keyOf(index[next]), bits);
			if (((next - home) & mask) >= ((next - hole) & mask))
			{
				index[hole] = index[next];
				hole = next;
			}
		}
		index[hole] = 0;
	}

	DWORD EntryKey(DWORD value)
	{
		return entries.list[value - 1].rec;
	}

	Entry* Find(DWORD rec, BOOL insert)
	{
		BOOL found;
		DWORD slot = IndexFind(entries.index, TABLE_BITS, rec, EntryKey, &found);
		if (found)
			return &entries.list[entries.index[slot] - 1];
		if (!insert || slot >= TABLE_SIZE)
			return NULL;

		DWORD value;
		if (entries.free)
		{
			value = entries.free;
			entries.free = entries.list[value - 1].nextFree;
		}
		else if (entries.used < ENTRY_LIMIT)
			value = ++entries.used;
		else
		{
			++stats.full;
			return NULL;
		}

		Entry* entry = &entries.list[value - 1];
		MemoryZero(entry, sizeof(Entry));
		entry->rec = rec;
		entries.index[slot] = value;
		++entries.count;
		return entry;
	}

	VOID Release(Entry* entry)
	{
		if (!entry || !entry->refs || --entry->refs)
			return;

		BOOL found;
		DWORD slot = IndexFind(entries.index, TABLE_BITS, entry->rec, EntryKey, &found);
		if (!found)
			return;

		DWORD value = entries.index[slot];
		IndexRemove(entries.index, TABLE_BITS, slot, EntryKey);
		MemoryZero(entry, sizeof(Entry));
		entry->nextFree = entries.free;
		entries.free = value;
		--entries.count;
	}

	// Discipl2.exe v3.01b 0x0052F741: the packer loader caches this record per (name, flipped)
	DWORD GetRecord(const VOID* image)
	{
		return *(const DWORD*)((const BYTE*)image + 0x1C);
	}

	VOID CopyName(CHAR* dst, const CHAR* src)
	{
		DWORD length = StrLength(src);
		if (length > NAME_LENGTH - 1)
			length = NAME_LENGTH - 1;
		MemoryCopy(dst, src, length);
		dst[length] = '\0';
	}

	DWORD HashText(const CHAR* text, DWORD length)
	{
		DWORD hash = 2166136261u;
		for (DWORD i = 0; i < length; ++i)
			hash = (hash ^ (BYTE)text[i]) * 16777619u;
		return hash ? hash : 1;
	}

#pragma region Manifest
	struct Json {
		const CHAR* p;
		const CHAR* end;
		BOOL fail;
	};

	VOID Ws(Json* j)
	{
		while (j->p < j->end && (*j->p == ' ' || *j->p == '\n' || *j->p == '\r' || *j->p == '\t'))
			++j->p;
	}

	BOOL Eat(Json* j, CHAR c)
	{
		Ws(j);
		if (j->p < j->end && *j->p == c)
		{
			++j->p;
			return TRUE;
		}
		return FALSE;
	}

	BOOL Str(Json* j, const CHAR** text, DWORD* length)
	{
		Ws(j);
		if (j->p >= j->end || *j->p != '"')
		{
			j->fail = TRUE;
			return FALSE;
		}

		const CHAR* start = ++j->p;
		while (j->p < j->end && *j->p != '"')
			j->p += *j->p == '\\' ? 2 : 1;
		if (j->p >= j->end)
		{
			j->fail = TRUE;
			return FALSE;
		}

		*text = start;
		*length = j->p - start;
		++j->p;
		return TRUE;
	}

	BOOL Key(Json* j, const CHAR** text, DWORD* length)
	{
		return Str(j, text, length) && Eat(j, ':');
	}

	BOOL Is(const CHAR* text, DWORD length, const CHAR* word)
	{
		DWORD n = StrLength(word);
		return n == length && !MemoryCompare(text, word, n);
	}

	LONGLONG Num(Json* j)
	{
		Ws(j);
		BOOL negative = FALSE;
		if (j->p < j->end && *j->p == '-')
		{
			negative = TRUE;
			++j->p;
		}

		LONGLONG value = 0;
		while (j->p < j->end && *j->p >= '0' && *j->p <= '9')
			value = value * 10 + (*j->p++ - '0');
		while (j->p < j->end && (*j->p == '.' || *j->p == 'e' || *j->p == 'E' || *j->p == '+' || *j->p == '-' || (*j->p >= '0' && *j->p <= '9')))
			++j->p;

		return negative ? -value : value;
	}

	VOID Skip(Json* j)
	{
		Ws(j);
		if (j->p >= j->end)
		{
			j->fail = TRUE;
			return;
		}

		CHAR c = *j->p;
		if (c == '"')
		{
			const CHAR* text;
			DWORD length;
			Str(j, &text, &length);
		}
		else if (c == '{' || c == '[')
		{
			DWORD depth = 0;
			while (j->p < j->end)
			{
				c = *j->p;
				if (c == '"')
				{
					const CHAR* text;
					DWORD length;
					if (!Str(j, &text, &length))
						return;
					continue;
				}

				++j->p;
				if (c == '{' || c == '[')
					++depth;
				else if ((c == '}' || c == ']') && !--depth)
					return;
			}
			j->fail = TRUE;
		}
		else
		{
			while (j->p < j->end && *j->p != ',' && *j->p != '}' && *j->p != ']')
				++j->p;
		}
	}

	const CHAR* Look(const Pair* list, DWORD count, const CHAR* key)
	{
		for (DWORD i = 0; i < count; ++i)
			if (!StrCompare(list[i].key, key))
				return list[i].value;
		return NULL;
	}

	#define LOOK(list, key) Look(list, sizeof(list) / sizeof(*list), key)

	VOID Upper(CHAR* text)
	{
		for (; *text; ++text)
			if (*text >= 'a' && *text <= 'z')
				*text -= 32;
	}

	BOOL Is(const CHAR* text, const CHAR* word)
	{
		return !StrCompare(text, word);
	}

	LONG Int(const CHAR* text)
	{
		LONG value = 0;
		for (; *text >= '0' && *text <= '9'; ++text)
			value = value * 10 + (*text - '0');
		return value;
	}

	BOOL GameName(const CHAR* id, DWORD length, CHAR* out, Origin* origin)
	{
		CHAR buffer[96];
		if (length >= sizeof(buffer))
			return FALSE;
		MemoryCopy(buffer, id, length);
		buffer[length] = '\0';

		CHAR* t[8] = {};
		DWORD n = 0;
		t[n++] = buffer;
		for (CHAR* p = buffer; *p && n < 8; ++p)
			if (*p == '/')
			{
				*p = '\0';
				t[n++] = p + 1;
			}
		for (DWORD i = n; i < 8; ++i)
			t[i] = "";

		const CHAR* c = t[0];
		CHAR unit[16];
		StrCopy(unit, StrLength(t[1]) < sizeof(unit) ? t[1] : "");
		Upper(unit);
		const CHAR *a, *b, *d;
		*out = '\0';

		*origin = OriginCanvas;
		if (!StrCompareNum(c, "battle-unit", 11))
		{
			const CHAR* layer = Is(c, "battle-unit") ? "A1" : Is(c, "battle-unit-shadow") ? "S1" : Is(c, "battle-unit-aura") ? "A2" : NULL;
			if (layer && (a = LOOK(poses, t[2])) && (b = LOOK(sides, t[3])))
				StrPrint(out, "%s%s%s%s00", unit, a, layer, b);
		}
		else if (Is(c, "battle-effect-attack"))
		{
			if ((a = LOOK(attackTypes, t[2])) && (b = LOOK(layers, t[3])) && (d = LOOK(attackSides, t[4])))
				StrPrint(out, "%s%s%s%s%02d", unit, a, b, d, Int(t[5]));
		}
		else if (Is(c, "battle-effect-death"))
		{
			CHAR key[32];
			StrPrint(key, "%s/%s", t[1], t[2]);
			if ((a = LOOK(deaths, key)))
				StrCopy(out, a);
		}
		else if (Is(c, "battle-effect-item"))
		{
			if ((a = LOOK(items, t[1])))
				StrPrint(out, "%sHMOVA1B00", a);
		}
		else if (Is(c, "battle-effect-level-up"))
		{
			if ((a = LOOK(races, t[1])))
				StrPrint(out, "UPGRADE%s%s", a, Is(t[2], "large") ? "L" : "S");
		}
		else if (Is(c, "battle-effect-status"))
		{
			if ((a = LOOK(statuses, t[1])))
				StrPrint(out, "%s%s", a, Is(t[2], "large") ? "L" : "S");
		}
		else if ((a = LOOK(constants, c)))
		{
			StrCopy(out, a);
			*origin = !StrCompareNum(c, "battle-", 7) || Is(c, "water") ? OriginCanvas : OriginCentre;
		}
		else
		{
			*origin = OriginCentre;
			if (!StrCompareNum(c, "stack-unit", 10))
			{
				BOOL move = Is(t[2], "move");
				const CHAR* pose = Is(c, "stack-unit") ? (move ? "MOVE" : "STOP") : Is(c, "stack-unit-shadow") ? (move ? "SMOV" : "SSTO") : Is(c, "stack-unit-aura") ? (move ? "MOV2" : "STO2") : NULL;
				if (pose)
					StrPrint(out, "%s%s%s", unit, pose, t[3]);
			}
			else if (Is(c, "stack-boat") || Is(c, "stack-boat-shadow"))
			{
				BOOL move = Is(t[2], "move");
				const CHAR* pose = Is(c, "stack-boat") ? (move ? "BTMV" : "BOAT") : (move ? "SBTM" : "SBOA");
				if ((a = LOOK(boatVariants, t[1])))
					StrPrint(out, "G000RR000%s%s%s", a, pose, t[3]);
			}
			else if (Is(c, "battle-corpse"))
				StrPrint(out, "DEAD_HUMAN_%sA%02d", Is(t[1], "large") ? "L" : "S", Int(t[2]));
			else if (Is(c, "cursor"))
			{
				if ((a = LOOK(cursors, t[1])))
					StrCopy(out, a);
			}
			else if (Is(c, "landmark") || Is(c, "water-landmark"))
				StrCopy(out, unit);
			else if (Is(c, "capital"))
			{
				BOOL captured = Is(t[1], "captured");
				const CHAR* race = captured ? t[2] : t[1];
				if ((a = Is(race, "neutral") ? "HU" : LOOK(races, race)))
					StrPrint(out, "G000FT0000%s%s0", a, captured ? "C" : "");
			}
			else if (Is(c, "city"))
				StrPrint(out, "G000FT0000NE%s", t[1]);
			else if (Is(c, "city-shadow"))
				StrPrint(out, "G000FT0000NE%sS", t[1]);
			else if (Is(c, "crystal"))
			{
				if ((a = LOOK(crystals, t[1])))
					StrPrint(out, "G000CR0000%s", a);
			}
			else if (Is(c, "ruin"))
				StrPrint(out, "G000RU0000%d%02d", Is(t[1], "looted") ? 1 : 0, Int(t[2]));
			else if (Is(c, "site") || Is(c, "site-shadow"))
			{
				if ((a = LOOK(sites, t[1])))
					StrPrint(out, "G000SI0000%s%02d%s", a, Int(t[2]), Is(c, "site-shadow") ? "S" : "");
			}
			else if (Is(c, "site-banner"))
			{
				if ((a = LOOK(sites, t[1])))
					StrPrint(out, "SITE_ICON_%s", a);
			}
			else if (Is(c, "ruin-banner"))
				StrPrint(out, "RUIN_ICON_%d", Is(t[1], "looted") ? 1 : 0);
			else if (Is(c, "stack-banner"))
				StrPrint(out, "STACK_BANNER_%04d", Int(t[1]) * 100);
			else if (Is(c, "bag"))
				StrPrint(out, "G000BG0000%03d", Is(t[1], "water") ? Int(t[2]) : 100 + Int(t[2]));
			else if (Is(c, "tombstone"))
				StrPrint(out, "G000TB0000%s", Is(t[1], "water") ? "W" : "G");
			else if (Is(c, "rod"))
			{
				if ((a = LOOK(raceIds, t[1])))
					StrPrint(out, "G000RR%sRROD8", a);
			}
			else if (Is(c, "city-shield"))
			{
				if ((a = LOOK(raceIds, t[2])))
					StrPrint(out, "G000RR%s%s8", a, Is(t[1], "capital") ? "SHLC" : "SHLV");
			}
			else if (Is(c, "cloud"))
				StrPrint(out, "CLOUDI%02d", 4 + Int(t[1]));
			else if (Is(c, "mountain"))
				StrPrint(out, "MOM%s%02d%02d", Is(t[1], "dwarf") ? "DW" : "NE", Int(t[2]), Int(t[3]));
			else if (Is(c, "mountain-remove-effect"))
				StrPrint(out, "RMVMOUNTAIN%s", t[1]);
			else if (Is(c, "world-spell-effect"))
			{
				if ((a = LOOK(spellLayers, t[2])))
					StrPrint(out, "%s%s%d", unit, a, Int(t[3]) + 1);
			}
			else
			{
				*origin = OriginSprite;
				if (Is(c, "item-icon"))
				{
					if (Is(t[1], "scroll"))
					{
						if ((a = LOOK(races, t[2])))
							StrPrint(out, "SCROLL%s", a);
					}
					else
						StrCopy(out, unit);
				}
				else if (Is(c, "spell-icon") || Is(c, "unit-event-portrait"))
					StrCopy(out, unit);
				else if (Is(c, "building-icon"))
					StrPrint(out, "%s.PNG", unit);
				else if (Is(c, "unit-face"))
					StrPrint(out, "%sFACE.PNG", unit);
				else if (Is(c, "unit-portrait"))
					StrPrint(out, "%sFACEB.PNG", unit);
				else if (Is(c, "lord-face"))
				{
					if ((a = LOOK(races, t[1])))
						StrPrint(out, "LORD%s%s%02d", a, Is(t[3], "large") ? "L" : "S", Int(t[2]) + 1);
				}
				else if (Is(c, "city-image"))
					StrPrint(out, "ALN%s", t[1]);
				else if (Is(c, "capital-image"))
				{
					if ((a = LOOK(capitalLetters, t[1])))
						StrPrint(out, "AL%s0", a);
				}
			}
		}

		return *out != '\0';
	}

	BOOL PackName(const CHAR* id, DWORD length, CHAR* out, Origin* origin)
	{
		if (!pack.gameNames)
			return GameName(id, length, out, origin);
		if (length >= 64)
			return FALSE;
		MemoryCopy(out, id, length);
		out[length] = '\0';
		Upper(out);
		*origin = OriginCanvas;
		return TRUE;
	}

	BOOL GrowFrames()
	{
		if (pack.frameCount < pack.frameCapacity)
			return TRUE;

		DWORD capacity = pack.frameCapacity ? pack.frameCapacity * 2 : 65536;
		PackFrame* list = (PackFrame*)MemoryAlloc(capacity * sizeof(PackFrame));
		if (!list)
			return FALSE;
		if (pack.frameCount)
			MemoryCopy(list, pack.frames, pack.frameCount * sizeof(PackFrame));
		if (pack.frames)
			MemoryFree(pack.frames);
		pack.frames = list;
		pack.frameCapacity = capacity;
		return TRUE;
	}

	BOOL AddName(const CHAR* text, DWORD length, DWORD* offset)
	{
		if (pack.namesSize + length + 1 > pack.namesCapacity)
		{
			DWORD capacity = max(pack.namesCapacity * 2, pack.namesSize + length + 1 + 65536);
			CHAR* list = (CHAR*)MemoryAlloc(capacity);
			if (!list)
				return FALSE;
			if (pack.namesSize)
				MemoryCopy(list, pack.names, pack.namesSize);
			if (pack.names)
				MemoryFree(pack.names);
			pack.names = list;
			pack.namesCapacity = capacity;
		}

		*offset = pack.namesSize;
		MemoryCopy(pack.names + pack.namesSize, text, length);
		pack.names[pack.namesSize + length] = '\0';
		pack.namesSize += length + 1;
		return TRUE;
	}

	VOID ParseMetadata(Json* j, PackFrame* frame)
	{
		if (!Eat(j, '{'))
		{
			j->fail = TRUE;
			return;
		}

		if (Eat(j, '}'))
			return;

		do
		{
			const CHAR* name;
			DWORD n;
			if (!Key(j, &name, &n))
				return;

			if (Is(name, n, "Width"))
				frame->w = (LONG)Num(j);
			else if (Is(name, n, "Height"))
				frame->h = (LONG)Num(j);
			else if (Is(name, n, "OffsetX"))
				frame->ox = (LONG)Num(j);
			else if (Is(name, n, "OffsetY"))
				frame->oy = (LONG)Num(j);
			else if (Is(name, n, "Additive"))
			{
				Ws(j);
				frame->additive = j->p < j->end && *j->p == 't';
				Skip(j);
			}
			else
				Skip(j);
		} while (!j->fail && Eat(j, ','));

		if (!Eat(j, '}'))
			j->fail = TRUE;
	}

	VOID ParseFrame(Json* j, PackFrame* frame, const CHAR** id, DWORD* idLength)
	{
		MemoryZero(frame, sizeof(PackFrame));
		if (!Eat(j, '{'))
		{
			j->fail = TRUE;
			return;
		}

		if (Eat(j, '}'))
			return;

		do
		{
			const CHAR* key;
			DWORD length;
			if (!Key(j, &key, &length))
				return;

			if (Is(key, length, "Metadata"))
				ParseMetadata(j, frame);
			else if (Is(key, length, "Offset"))
				frame->offset = Num(j);
			else if (Is(key, length, "Length"))
				frame->length = (DWORD)Num(j);
			else if (id && Is(key, length, "Id"))
				Str(j, id, idLength);
			else
				Skip(j);
		} while (!j->fail && Eat(j, ','));

		if (!Eat(j, '}'))
			j->fail = TRUE;
	}

	const PackAnim* FindAnim(const CHAR* name);

	// names the pack holds more than once (two archives with an image of the same name): only the first is reachable, and a frame of the other archive would take its 4K
	struct {
		DWORD count, logs;
	} packDuplicates;

	BOOL InsertAnim(const CHAR* game, const CHAR* id, DWORD idLength, DWORD first, DWORD count, Origin origin)
	{
		if (pack.animCount >= ANIM_SIZE * 3 / 4)
			return FALSE;
		const PackAnim* existing = FindAnim(game);
		if (existing)
		{
			++packDuplicates.count;
			if (packDuplicates.logs < 32 && count && first < pack.frameCount + 1)
			{
				++packDuplicates.logs;
				const PackFrame* kept = &pack.frames[existing->first];
				const PackFrame* other = &pack.frames[first];
				DebugLog("image pack duplicate name %.48s: kept %u frames, first %dx%d at %d,%d; dropped %u frames, first %dx%d at %d,%d", game, existing->count, kept->w, kept->h, kept->ox, kept->oy,
					count, other->w, other->h, other->ox, other->oy);
			}
			return FALSE;
		}

		DWORD name, idName;
		if (!AddName(game, StrLength(game), &name) || !AddName(id, idLength, &idName))
			return FALSE;

		DWORD hash = HashText(game, StrLength(game));
		DWORD index = hash & (ANIM_SIZE - 1);
		while (pack.anims[index].hash)
			index = (index + 1) & (ANIM_SIZE - 1);

		PackAnim* anim = &pack.anims[index];
		anim->hash = hash;
		anim->name = name;
		anim->id = idName;
		anim->first = first;
		anim->count = count;
		anim->origin = origin;
		++pack.animCount;
		return TRUE;
	}

	VOID ParseSprite(Json* j)
	{
		const CHAR* id = NULL;
		DWORD idLength = 0;
		PackFrame frame;
		ParseFrame(j, &frame, &id, &idLength);
		if (j->fail || !id)
			return;

		CHAR game[64];
		Origin origin;
		if (!PackName(id, idLength, game, &origin) || !GrowFrames())
			return;

		pack.frames[pack.frameCount] = frame;
		if (InsertAnim(game, id, idLength, pack.frameCount, 1, origin))
			++pack.frameCount;
	}

	VOID ParseAnim(Json* j)
	{
		if (!Eat(j, '{'))
		{
			j->fail = TRUE;
			return;
		}

		if (Eat(j, '}'))
			return;

		const CHAR* id = NULL;
		DWORD idLength = 0;
		CHAR game[64];
		Origin origin;
		do
		{
			const CHAR* key;
			DWORD length;
			if (!Key(j, &key, &length))
				return;

			if (Is(key, length, "Id"))
				Str(j, &id, &idLength);
			else if (Is(key, length, "Frames") && id && PackName(id, idLength, game, &origin))
			{
				if (!Eat(j, '['))
				{
					j->fail = TRUE;
					return;
				}

				DWORD first = pack.frameCount;
				if (!Eat(j, ']'))
				{
					do
					{
						if (!GrowFrames())
						{
							j->fail = TRUE;
							return;
						}
						ParseFrame(j, &pack.frames[pack.frameCount++], NULL, NULL);
					} while (!j->fail && Eat(j, ','));

					if (!Eat(j, ']'))
						j->fail = TRUE;
				}

				if (!j->fail && !InsertAnim(game, id, idLength, first, pack.frameCount - first, origin))
					pack.frameCount = first;
			}
			else
				Skip(j);
		} while (!j->fail && Eat(j, ','));

		if (!Eat(j, '}'))
			j->fail = TRUE;
	}

	BOOL ParseManifest(const CHAR* data, DWORD size)
	{
		Json j = { data, data + size, FALSE };
		if (!Eat(&j, '{'))
			return FALSE;

		do
		{
			const CHAR* key;
			DWORD length;
			if (!Key(&j, &key, &length))
				return FALSE;

			BOOL anims = Is(key, length, "Animations");
			if (anims || Is(key, length, "Sprites"))
			{
				if (!Eat(&j, '['))
					return FALSE;
				if (!Eat(&j, ']'))
				{
					do
					{
						if (anims)
							ParseAnim(&j);
						else
							ParseSprite(&j);
					} while (!j.fail && Eat(&j, ','));

					if (!Eat(&j, ']'))
						return FALSE;
				}
			}
			else
				Skip(&j);
		} while (!j.fail && Eat(&j, ','));

		return !j.fail;
	}

	const PackAnim* FindAnim(const CHAR* id)
	{
		DWORD length = StrLength(id);
		DWORD hash = HashText(id, length);
		DWORD index = hash & (ANIM_SIZE - 1);
		while (pack.anims[index].hash)
		{
			const PackAnim* anim = &pack.anims[index];
			if (anim->hash == hash && !StrCompare(pack.names + anim->name, id))
				return anim;
			index = (index + 1) & (ANIM_SIZE - 1);
		}
		return NULL;
	}
#pragma endregion

#pragma region Pack
	BOOL ReadAt(HANDLE file, LONGLONG offset, VOID* buffer, DWORD size)
	{
		OVERLAPPED ov = {};
		ov.Offset = (DWORD)offset;
		ov.OffsetHigh = (DWORD)(offset >> 32);
		DWORD read = 0;
		return ReadFile(file, buffer, size, &read, &ov) && read == size;
	}

	BOOL PackPresent()
	{
		static INT present = -1;
		if (present < 0)
		{
			CHAR path[MAX_PATH];
			DWORD length = GetModuleFileName(NULL, path, sizeof(path));
			while (length && path[length - 1] != '\\')
				--length;
			StrCopy(path + length, "gamenames-v1.galleanpack");
			present = GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
			if (!present)
			{
				StrCopy(path + length, "assets-v1.galleanpack");
				present = GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
			}
		}
		return present;
	}

	BOOL OpenPack()
	{
		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, sizeof(path));
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, "gamenames-v1.galleanpack");
		pack.file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
		pack.gameNames = pack.file != INVALID_HANDLE_VALUE;
		if (!pack.gameNames)
		{
			StrCopy(path + length, "assets-v1.galleanpack");
			pack.file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
		}
		if (pack.file == INVALID_HANDLE_VALUE)
		{
			pack.file = NULL;
			DebugLog("image pack not found at %s", path);
			return FALSE;
		}

		BYTE header[32];
		if (!ReadAt(pack.file, 0, header, sizeof(header)) || MemoryCompare(header, "GALLEANPACK", 11) || *(LONG*)(header + 12) != 27)
		{
			DebugLog("image pack header not recognised");
			return FALSE;
		}

		LONGLONG offset = *(LONGLONG*)(header + 16);
		LONGLONG size = *(LONGLONG*)(header + 24);
		if (size <= 0 || size > 256 * 1024 * 1024)
			return FALSE;

		CHAR* manifest = (CHAR*)MemoryAlloc((DWORD)size);
		if (!manifest)
			return FALSE;

		DWORD tick = GetTickCount();
		BOOL ok = ReadAt(pack.file, offset, manifest, (DWORD)size) && ParseManifest(manifest, (DWORD)size);
		MemoryFree(manifest);
		if (!ok)
		{
			DebugLog("image pack manifest parse failed");
			return FALSE;
		}

		pack.texs = (Tex*)MemoryAlloc(pack.frameCount * sizeof(Tex));
		if (!pack.texs)
			return FALSE;
		MemoryZero(pack.texs, pack.frameCount * sizeof(Tex));

		DebugLog("image pack loaded: %s, %d animations, %d frames in %d ms, %u duplicate names dropped", pack.gameNames ? "gamenames-v1.galleanpack (game names, canvas origin)" : "assets-v1.galleanpack (asset ids)",
			pack.animCount, pack.frameCount, GetTickCount() - tick, packDuplicates.count);
		return TRUE;
	}

	VOID ChainPush(TexChain* chain, DWORD index, TexList list, BOOL front)
	{
		Tex* tex = &pack.texs[index];
		DWORD value = index + 1;
		tex->list = list;
		if (front)
		{
			tex->prev = 0;
			tex->next = chain->head;
			if (chain->head)
				pack.texs[chain->head - 1].prev = value;
			else
				chain->tail = value;
			chain->head = value;
		}
		else
		{
			tex->next = 0;
			tex->prev = chain->tail;
			if (chain->tail)
				pack.texs[chain->tail - 1].next = value;
			else
				chain->head = value;
			chain->tail = value;
		}
		++chain->count;
	}

	VOID ChainRemove(TexChain* chain, DWORD index)
	{
		Tex* tex = &pack.texs[index];
		if (tex->prev)
			pack.texs[tex->prev - 1].next = tex->next;
		else
			chain->head = tex->next;
		if (tex->next)
			pack.texs[tex->next - 1].prev = tex->prev;
		else
			chain->tail = tex->prev;
		tex->prev = tex->next = 0;
		tex->list = ListNone;
		--chain->count;
	}

	BOOL DecodeWebP(const PackFrame* frame, Tex* tex)
	{
		BYTE* data = (BYTE*)MemoryAlloc(frame->length);
		int w = 0, h = 0;
		BYTE* rgba = NULL;
		if (data && ReadAt(pack.file, 32 + frame->offset, data, frame->length))
			rgba = WebPDecodeRGBA(data, frame->length, &w, &h);
		if (data)
			MemoryFree(data);
		if (!rgba)
			return FALSE;

		BYTE* pixels = (BYTE*)MemoryAlloc(w * h * 4);
		if (!pixels)
		{
			WebPFree(rgba);
			return FALSE;
		}

		if (frame->additive)
			MemoryCopy(pixels, rgba, w * h * 4);
		else
		{
			const BYTE* s = rgba;
			BYTE* d = pixels;
			for (LONG i = w * h; i; --i, s += 4, d += 4)
			{
				DWORD a = s[3];
				d[0] = (BYTE)((s[0] * a + 127) / 255);
				d[1] = (BYTE)((s[1] * a + 127) / 255);
				d[2] = (BYTE)((s[2] * a + 127) / 255);
				d[3] = (BYTE)a;
			}
		}
		WebPFree(rgba);

		tex->pixels = pixels;
		tex->w = w;
		tex->h = h;
		tex->bytes = w * h * 4;
		tex->compressed = 0;
		return TRUE;
	}

	VOID Decode(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		const PackFrame* frame = &pack.frames[index];

		BOOL ok = FALSE;
		switch (frame->format)
		{
		case PackWebP:
			ok = DecodeWebP(frame, tex);
			break;
		}

		if (!ok)
		{
			InterlockedExchange(&tex->state, TexFailed);
			return;
		}

		InterlockedExchangeAdd(&pack.decoded, tex->bytes);
		EnterCriticalSection(&requests.cs);
		ChainPush(&pack.decodedList, index, ListDecoded, FALSE);
		InterlockedExchange(&tex->state, TexDecoded);
		LeaveCriticalSection(&requests.cs);
	}

	BOOL Pop(Ring* ring, DWORD* index)
	{
		if (ring->head == ring->tail)
			return FALSE;
		*index = ring->queue[ring->head];
		ring->head = (ring->head + 1) % REQUEST_SIZE;
		return TRUE;
	}

	BOOL Push(Ring* ring, DWORD index)
	{
		DWORD next = (ring->tail + 1) % REQUEST_SIZE;
		if (next == ring->head)
			return FALSE;
		ring->queue[ring->tail] = index;
		ring->tail = next;
		return TRUE;
	}

	DWORD __stdcall Worker(LPVOID)
	{
		if (!OpenPack())
		{
			pack.failed = TRUE;
			return 0;
		}

		InterlockedExchange(&pack.ready, TRUE);

		HANDLE events[] = { pack.wake, pack.space };
		for (;;)
		{
			WaitForSingleObject(pack.wake, INFINITE);
			for (;;)
			{
				LONG decoded = pack.decoded;
				if (decoded >= DECODE_LIMIT)
				{
					WaitForMultipleObjects(2, events, FALSE, WORKER_WAIT_MS);
					continue;
				}

				DWORD index;
				EnterCriticalSection(&requests.cs);
				BOOL any = Pop(&requests.demand, &index) || (decoded < DECODE_PREFETCH_LIMIT && Pop(&requests.prefetch, &index));
				BOOL pending = requests.prefetch.head != requests.prefetch.tail;
				LeaveCriticalSection(&requests.cs);
				if (!any)
				{
					if (pending)
					{
						WaitForMultipleObjects(2, events, FALSE, WORKER_WAIT_MS);
						continue;
					}
					break;
				}

				if (pack.texs[index].state == TexQueued)
					Decode(index);
			}
		}
	}

	BOOL Wanted(const Tex* tex, BOOL prefetch)
	{
		LONG state = tex->state;
		return state == TexNone || (state == TexQueued && tex->prefetch && !prefetch);
	}

	VOID Request(DWORD index, BOOL prefetch)
	{
		Tex* tex = &pack.texs[index];
		if (!Wanted(tex, prefetch))
			return;

		EnterCriticalSection(&requests.cs);
		if (Wanted(tex, prefetch) && Push(prefetch ? &requests.prefetch : &requests.demand, index))
		{
			tex->prefetch = prefetch;
			tex->state = TexQueued;
		}
		LeaveCriticalSection(&requests.cs);
		SetEvent(pack.wake);
	}

	VOID Prefetch(const PackAnim* anim, DWORD index, BOOL missing)
	{
		if (!anim || anim->count < 2)
			return;

		DWORD* scan = &pack.animScan[anim - pack.anims];
		DWORD gen = (DWORD)pack.evictGen + 1;
		if (*scan == gen && !missing)
			return;

		LONG now = game.frameNo;
		for (DWORD i = 1; i < anim->count; ++i)
		{
			DWORD frame = anim->first + (index - anim->first + i) % anim->count;
			Tex* tex = &pack.texs[frame];
			if (tex->state == TexNone)
			{
				tex->lastUse = now;
				Request(frame, TRUE);
			}
		}
		*scan = gen;
	}

	VOID StartPack()
	{
		InitializeCriticalSection(&requests.cs);
		game.initialized = TRUE;
		pack.wake = CreateEvent(NULL, FALSE, FALSE, NULL);
		pack.space = CreateEvent(NULL, FALSE, FALSE, NULL);
		pack.gpuBudget = GPU_FALLBACK;
		pack.thread = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
	}

	// exact: FALSE when only a variant of the name (.PNG added or cut, spell prefix) is in the pack
	LONG PackFrameFor(const CHAR* owner, DWORD index, LONG canvasW, LONG canvasH, Origin* origin, const PackAnim** found, BOOL* exact = NULL)
	{
		*found = NULL;
		CHAR name[NAME_LENGTH + 8];
		DWORD length = min((DWORD)StrLength(owner), (DWORD)NAME_LENGTH - 1);
		MemoryCopy(name, owner, length);
		name[length] = '\0';
		Upper(name);
		const PackAnim* anim = FindAnim(name);
		if (exact)
			*exact = anim != NULL;
		if (!anim)
		{
			DWORD size = StrLength(name);
			if (size > 4 && !StrCompare(name + size - 4, ".PNG"))
				name[size - 4] = '\0';
			else
				StrCat(name, ".PNG");
			anim = FindAnim(name);
		}
		if (!anim && StrLength(name) == 15 && !StrCompareNum(name, "G000SS", 6))
		{
			name[SPELL_PREFIX] = '\0';
			anim = FindAnim(name);
		}
		if (!anim)
			return -1;

		*found = anim;
		*origin = anim->origin;
		const PackFrame* frame = &pack.frames[anim->first];
		if (anim->origin == OriginSprite)
			return canvasW == frame->w && canvasH == frame->h ? (LONG)anim->first : -1;
		return index < anim->count ? (LONG)(anim->first + index) : -1;
	}

	LONG Resolve(Entry* entry, DWORD rec)
	{
		if (entry->resolved)
			return entry->packFrame;
		if (!pack.ready)
			return -1;

		entry->resolved = TRUE;
		const DWORD* dims = (const DWORD*)rec;
		Origin origin = OriginCanvas;
		const PackAnim* anim;
		entry->packFrame = PackFrameFor(entry->owner, entry->frame, (LONG)dims[0], (LONG)dims[1], &origin, &anim);
		if (anim)
		{
			entry->anim = anim;
			entry->origin = origin;
		}
		return entry->packFrame;
	}

#pragma endregion

#pragma region Game
	BOOL Enabled()
	{
		return game.initialized && pack.ready && config.bpp32Hooked && !config.type.editor;
	}

	BOOL Touch(Tex* tex, DWORD index)
	{
		tex->lastUse = game.frameNo;
		BOOL missing = tex->state == TexNone;
		Request(index, FALSE);
		if (tex->prefetch && tex->state != TexQueued)
			InterlockedExchange(&tex->prefetch, FALSE);
		return missing;
	}

	BOOL ArtReady(LONG frame)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.frameCount)
			return FALSE;

		// frames picked by the twins (faces, cached images) may never pass through Queue, so asking is what gets them decoded
		Tex* tex = &pack.texs[frame];
		if (tex->state != TexUploaded)
		{
			Touch(tex, frame);
			return FALSE;
		}
		tex->lastUse = game.frameNo;
		return TRUE;
	}

	// -1 never, 0 requested, 1 decoded (the twins upload it at replay), 2 on the GPU
	LONG ArtState(LONG frame)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.frameCount)
			return -1;

		Tex* tex = &pack.texs[frame];
		LONG state = tex->state;
		if (state == TexUploaded)
		{
			tex->lastUse = game.frameNo;
			return 2;
		}
		if (state == TexFailed)
			return -1;
		Touch(tex, frame);
		return tex->state == TexDecoded ? 1 : 0;
	}

	LONG Queue(Entry* entry, DWORD rec)
	{
		if (!Enabled())
			return -1;

		LONG index = Resolve(entry, rec);
		if (index < 0)
			return -1;

		BOOL missing = Touch(&pack.texs[index], index);
		Prefetch(entry->anim, index, missing);
		return index;
	}

	VOID ExpectArt(const Entry* entry, LONG index, DWORD rec, const POINT* start, const POINT* offset, const POINT* size, const RECT* area)
	{
		if (index < 0 || !start)
		{
			++art.noFrame;
			return;
		}
		if (!ArtReady(index))
		{
			++art.notReady;
			return;
		}

		const PackFrame* frame = &pack.frames[index];
		const DWORD* dims = (const DWORD*)rec;
		LONG canvasW = (LONG)dims[0], canvasH = (LONG)dims[1];
		LONG ox = frame->ox, oy = frame->oy;
		if (entry->origin == OriginCentre)
		{
			ox += canvasW / 2;
			oy += canvasH / 2;
		}
		else if (entry->origin == OriginSprite)
			ox = oy = 0;

		RECT place;
		place.left = start->x + (entry->flipped ? canvasW - ox - frame->w : ox);
		place.top = start->y + oy;
		place.right = place.left + frame->w;
		place.bottom = place.top + frame->h;

		LONG offX = offset ? offset->x : 0, offY = offset ? offset->y : 0;
		LONG sizeX = size ? size->x : canvasW, sizeY = size ? size->y : canvasH;
		RECT clip = { start->x + offX, start->y + offY, start->x + sizeX, start->y + sizeY };
		if (area)
		{
			clip.left = max(clip.left, area->left);
			clip.top = max(clip.top, area->top);
			clip.right = min(clip.right, area->right);
			clip.bottom = min(clip.bottom, area->bottom);
		}

		if (Config::IsMapZoomed())
		{
			RECT zoom;
			Config::GetMapZoomRect(&zoom);
			if (place.left < zoom.right && place.top < zoom.bottom && place.right > zoom.left && place.bottom > zoom.top)
			{
				++art.zoomed;
				return;
			}
		}

		Twin::ArtPart parts[ART_PARTS];
		DWORD count = 0;
		const BYTE* list = *(const BYTE**)(rec + 8);
		DWORD total = *(const DWORD*)(rec + 0xC);
		for (DWORD i = 0; i < total; ++i)
		{
			const LONG* part = (const LONG*)(list + i * 24);
			if (part[0] < offX || part[0] + part[4] > sizeX || part[1] < offY || part[1] + part[5] > sizeY)
				continue;
			if (count >= ART_PARTS)
			{
				++art.truncated;
				break;
			}

			Twin::ArtPart* p = &parts[count++];
			p->dx = start->x + part[0];
			p->dy = start->y + part[1];
			p->sx = part[2];
			p->sy = part[3];
			p->w = part[4];
			p->h = part[5];
		}

		if (art.begins < ART_LOG)
			DebugLog("image art name=%s frame=%d place=%d,%d,%d,%d clip=%d,%d,%d,%d flipped=%d parts=%d first=%d,%d,%d,%d,%d,%d", entry->name, index,
				place.left, place.top, place.right, place.bottom, clip.left, clip.top, clip.right, clip.bottom, entry->flipped, count,
				count ? parts[0].dx : 0, count ? parts[0].dy : 0, count ? parts[0].sx : 0, count ? parts[0].sy : 0, count ? parts[0].w : 0, count ? parts[0].h : 0);
		++art.begins;
		Twin::ExpectArt(index, entry->flipped, &place, &clip, parts, count);
	}

	DWORD StoreImageKey(DWORD value)
	{
		return (DWORD)stores.list[value - 1].image;
	}

	DWORD StoreDataKey(DWORD value)
	{
		return (DWORD)stores.list[value - 1].data;
	}

	StoreImage* FindStore(const VOID* image, BOOL insert)
	{
		BOOL found;
		DWORD slot = IndexFind(stores.byImage, STORE_BITS, (DWORD)image, StoreImageKey, &found);
		if (found)
			return &stores.list[stores.byImage[slot] - 1];
		if (!insert || slot >= STORE_SIZE)
			return NULL;

		DWORD value;
		if (stores.free)
		{
			value = stores.free;
			stores.free = stores.list[value - 1].nextFree;
		}
		else if (stores.used < sizeof(stores.list) / sizeof(*stores.list))
			value = ++stores.used;
		else
			return NULL;

		StoreImage* s = &stores.list[value - 1];
		MemoryZero(s, sizeof(StoreImage));
		s->image = image;
		stores.byImage[slot] = value;
		++stores.count;
		return s;
	}

	StoreImage* FindStoreData(const BYTE* data)
	{
		BOOL found;
		DWORD slot = IndexFind(stores.byData, STORE_BITS, (DWORD)data, StoreDataKey, &found);
		return found ? &stores.list[stores.byData[slot] - 1] : NULL;
	}

	VOID UnlinkStoreData(StoreImage* s)
	{
		if (!s->data)
			return;

		BOOL found;
		DWORD slot = IndexFind(stores.byData, STORE_BITS, (DWORD)s->data, StoreDataKey, &found);
		if (found && stores.byData[slot] == (DWORD)(s - stores.list) + 1)
			IndexRemove(stores.byData, STORE_BITS, slot, StoreDataKey);
		s->data = NULL;
	}

	VOID LinkStoreData(StoreImage* s, const BYTE* data)
	{
		UnlinkStoreData(s);

		BOOL found;
		DWORD slot = IndexFind(stores.byData, STORE_BITS, (DWORD)data, StoreDataKey, &found);
		if (found)
		{
			stores.list[stores.byData[slot] - 1].data = NULL;
			IndexRemove(stores.byData, STORE_BITS, slot, StoreDataKey);
			slot = IndexFind(stores.byData, STORE_BITS, (DWORD)data, StoreDataKey, &found);
		}

		s->data = data;
		if (slot < STORE_SIZE)
			stores.byData[slot] = (DWORD)(s - stores.list) + 1;
	}

	LONG StoreFrame(StoreImage* s)
	{
		if (!s->entry)
			return -1;

		LONG index = Resolve(s->entry, (DWORD)s->dims);
		if (index < 0 || pack.frames[index].w != (LONG)s->dims[0] || pack.frames[index].h != (LONG)s->dims[1])
			return -1;
		return index;
	}

	BOOL RangeHolds(const StoreImage* s, const BYTE* ptr)
	{
		if (ptr < s->first)
			return FALSE;
		DWORD offset = (DWORD)(ptr - s->first) / sizeof(DWORD);
		return (LONG)(offset % s->pitch) < (LONG)s->dims[0] && (LONG)(offset / s->pitch) < (LONG)s->dims[1];
	}

	BOOL RangesOverlap(const StoreImage* a, const StoreImage* b)
	{
		if (a->pitch != b->pitch)
			return TRUE;

		LONG d = (LONG)(b->first - a->first) / (LONG)sizeof(DWORD);
		LONG p = a->pitch;
		LONG y = d >= 0 ? d / p : -((p - 1 - d) / p);
		LONG x = d - y * p;
		return x < (LONG)a->dims[0] && x + (LONG)b->dims[0] > 0 && y < (LONG)a->dims[1] && y + (LONG)b->dims[1] > 0;
	}

	DWORD RangeLower(const BYTE* ptr)
	{
		DWORD lo = 0, hi = memory.count;
		while (lo < hi)
		{
			DWORD mid = (lo + hi) / 2;
			if (memory.list[mid].start < ptr)
				lo = mid + 1;
			else
				hi = mid;
		}
		return lo;
	}

	VOID DropRange(StoreImage* s)
	{
		if (!s->first)
			return;

		DWORD value = s - stores.list;
		for (DWORD i = RangeLower(s->first); i < memory.count && memory.list[i].start == s->first; ++i)
			if (memory.list[i].store == value)
			{
				memmove(&memory.list[i], &memory.list[i + 1], (memory.count - i - 1) * sizeof(Range));
				--memory.count;
				break;
			}

		Twin::OnMemoryFree(s->first);
		s->first = NULL;
		++memory.dropped;
	}

	BOOL SeedStore(StoreImage* s, const BYTE* first, LONG pitch, BOOL memoryTwin);

	// indices: the decode is 8-bit (BlitObject format byte 0), whose rows are the pitch field in bytes, not twice it in DWORDs
	VOID AddRange(StoreImage* s, const BYTE* first, LONG pitch, DWORD* learned, BOOL indices)
	{
		// the same image decoded again over its own range keeps the range and its twin; only the seed is redone
		if (Twin::SeedMode() && s->first && s->first == first && s->pitch == pitch && s->rangeW == (LONG)s->dims[0] && s->rangeH == (LONG)s->dims[1])
		{
			if (Twin::Active() && SeedStore(s, first, pitch, TRUE))
				++*learned;
			return;
		}

		DropRange(s);
		LONG w = (LONG)s->dims[0], h = (LONG)s->dims[1];
		// an 8-bit decode read as 32 bpp at half its pitch is no image even where it fits: its memory twin took a seed nothing copies out of, every decode
		BOOL range = !(indices && Twin::SeedMode()) && config.bpp32Hooked && first && w > 0 && h > 0 && pitch >= w && pitch <= RANGE_PITCH && memory.count < sizeof(memory.list) / sizeof(*memory.list);
		const BYTE* end = range ? first + ((h - 1) * pitch + w) * sizeof(DWORD) : NULL;
		if (range && IsBadReadPtr(first, end - first))
		{
			++memory.rejected;
			range = FALSE;
		}
		if (!range)
		{
			// it is still captured for the indexed blit that converts it
			if (indices && first && w > 0 && h > 0 && Twin::SeedMode() && Twin::Active())
			{
				++memory.captureOnly;
				if (SeedStore(s, first, pitch, FALSE))
					++*learned;
			}
			return;
		}

		s->first = first;
		s->pitch = pitch;
		s->rangeW = w;
		s->rangeH = h;
		for (DWORD i = 0; i < memory.count;)
		{
			const Range* r = &memory.list[i];
			StoreImage* other = &stores.list[r->store];
			if (r->start < end && r->end > first && RangesOverlap(s, other))
				DropRange(other);
			else
				++i;
		}

		DWORD at = RangeLower(first);
		memmove(&memory.list[at + 1], &memory.list[at], (memory.count - at) * sizeof(Range));
		memory.list[at].start = first;
		memory.list[at].end = end;
		memory.list[at].store = s - stores.list;
		++memory.count;
		memory.span = max(memory.span, (DWORD)(end - first));

		if (Twin::SeedMode())
		{
			if (Twin::Active() && SeedStore(s, first, pitch, TRUE))
				++*learned;
			return;
		}

		LONG frame = StoreFrame(s);
		if (frame >= 0)
		{
			RECT rect = { 0, 0, w, h };
			Twin::LearnArt(first, pitch, &rect, frame, &rect, w, h, FALSE);
			++*learned;
		}
	}

	StoreImage* RangeStore(const BYTE* at)
	{
		DWORD i = memory.count ? RangeLower(at + 1) : 0;
		for (DWORD n = 0; i && n < RANGE_SCAN; --i, ++n)
		{
			const Range* r = &memory.list[i - 1];
			if ((DWORD)(at - r->start) >= memory.span)
				break;
			StoreImage* s = &stores.list[r->store];
			if (at < r->end && RangeHolds(s, at))
				return s;
		}
		return NULL;
	}

	BOOL FindMemory(const VOID* ptr, const VOID** base, LONG* pitch, LONG* width, LONG* height, LONG* frame, BOOL* flipped)
	{
		StoreImage* s = RangeStore((const BYTE*)ptr);
		if (!s)
		{
			++memory.misses;
			return FALSE;
		}

		++memory.hits;
		*base = s->first;
		*pitch = s->pitch;
		*width = (LONG)s->dims[0];
		*height = (LONG)s->dims[1];
		*frame = StoreFrame(s);
		*flipped = FALSE;
		return TRUE;
	}

	const CHAR* MemoryName(const VOID* ptr)
	{
		StoreImage* s = game.initialized && ptr ? RangeStore((const BYTE*)ptr) : NULL;
		if (!s)
			return NULL;
		return s->entry && s->entry->name[0] ? s->entry->name : s->file ? s->file->name : "?";
	}

	VOID ResetStore(StoreImage* s, ArchiveFile* file)
	{
		DropRange(s);
		UnlinkStoreData(s);
		s->printed = FALSE;
		s->decodeLogged = FALSE;
		Release(s->entry);
		s->entry = NULL;
		s->file = file;
	}

	VOID RemoveStore(const VOID* image)
	{
		BOOL found;
		DWORD slot = IndexFind(stores.byImage, STORE_BITS, (DWORD)image, StoreImageKey, &found);
		if (!found)
			return;

		DWORD value = stores.byImage[slot];
		StoreImage* s = &stores.list[value - 1];
		ResetStore(s, NULL);
		IndexRemove(stores.byImage, STORE_BITS, slot, StoreImageKey);
		MemoryZero(s, sizeof(StoreImage));
		s->nextFree = stores.free;
		stores.free = value;
		--stores.count;
	}

	struct {
		DWORD draws, drawLinks, faceLinks, faceLoads, faceCopies, faceMisses, faceRejects, takes, takeLinks, takeFrames, prefetchLoads, prefetchFrames;
	} storeStats;

	#define FACE_LINKS 1024

	struct FaceLink {
		const BYTE* data;
		const VOID* image;
		DWORD store;
		BOOL flipped;
	};

	FaceLink faceLinks[FACE_LINKS];

	FaceLink* FaceSlot(const BYTE* data)
	{
		return &faceLinks[((DWORD)data >> 2) * 2654435761u >> 22];
	}

	VOID AddFaceLink(const BYTE* data, const StoreImage* s, BOOL flipped)
	{
		FaceLink* l = FaceSlot(data);
		l->data = data;
		l->image = s->image;
		l->store = s - stores.list;
		l->flipped = flipped;
	}

	StoreImage* FindFaceLink(const BYTE* data, BOOL* flipped)
	{
		const FaceLink* l = FaceSlot(data);
		if (!data || l->data != data || stores.list[l->store].image != l->image)
			return NULL;
		*flipped = l->flipped;
		return &stores.list[l->store];
	}

	BOOL IsFace(const StoreImage* s)
	{
		return ((DWORD)s->image & 1) != 0;
	}

	VOID PrintPoint(const StoreImage* s, DWORD k, LONG* x, LONG* y)
	{
		*x = (LONG)(2 * (k % FACE_PRINT) + 1) * (LONG)s->dims[0] / (2 * FACE_PRINT);
		*y = (LONG)(2 * (k / FACE_PRINT) + 1) * (LONG)s->dims[1] / (2 * FACE_PRINT);
	}

	VOID PrintFace(StoreImage* s, const DWORD* src, LONG stride)
	{
		DWORD key = Hooks::Convert565toRGB(COLORKEY_CHECK_16);
		s->printMask = 0;
		for (DWORD k = 0; k < FACE_PRINT * FACE_PRINT; ++k)
		{
			LONG x, y;
			PrintPoint(s, k, &x, &y);
			DWORD pix = src[y * stride + x];
			if (pix == key)
				continue;
			s->print[k] = pix;
			s->printMask |= 1ull << k;
		}
		s->printed = TRUE;
	}

	VOID NoteFace(StoreImage* s, const VOID* src, LONG stride, LONG w, LONG h)
	{
		if (s && IsFace(s) && !s->printed && src && w == (LONG)s->dims[0] && h == (LONG)s->dims[1] && stride >= w && w > 0 && h > 0)
			PrintFace(s, (const DWORD*)src, stride);
	}

	// faces are followed by address through scratch buffers the game reuses for other faces of the same size, so every link is checked against the real pixels
	BOOL CheckFace(const StoreImage* s, BOOL flipped, const DWORD* src, LONG stride, LONG w, LONG h)
	{
		if (!IsFace(s))
			return TRUE;

		LONG fw = (LONG)s->dims[0], fh = (LONG)s->dims[1];
		if (!s->printed || !src || stride <= 0 || fw <= 0 || fh <= 0)
			return FALSE;

		DWORD checked = 0, total = 0;
		for (DWORD k = 0; k < FACE_PRINT * FACE_PRINT; ++k)
		{
			if (!(s->printMask & (1ull << k)))
				continue;
			++total;
			LONG x, y;
			PrintPoint(s, k, &x, &y);
			if (flipped)
				x = fw - 1 - x;
			if (x >= w || y >= h || x >= stride)
				continue;
			if (src[y * stride + x] != s->print[k])
				return FALSE;
			++checked;
		}
		return checked && checked >= min(total, (DWORD)FACE_PRINT_MIN);
	}

	BOOL VerifyFace(StoreImage* s, BOOL linked, BOOL flipped, const DWORD* src, LONG stride, LONG w, LONG h)
	{
		if (CheckFace(s, flipped, src, stride, w, h))
			return TRUE;
		if (!s->printed)
			return FALSE;

		++storeStats.faceRejects;
		if (linked)
		{
			FaceLink* l = FaceSlot((const BYTE*)src);
			if (l->data == (const BYTE*)src)
				l->data = NULL;
		}
		else if (s->data == (const BYTE*)src)
			UnlinkStoreData(s);
		return FALSE;
	}

	const CHAR* ImageName(const VOID* image)
	{
		if (!image || IsBadReadPtr((const BYTE*)image + 0x1C, 4))
			return "?";
		DWORD rec = GetRecord(image);
		Entry* entry = rec ? Find(rec, FALSE) : NULL;
		return entry && entry->name[0] ? entry->name : "?";
	}

	LONG TakeFace(const VOID* pixels, LONG width, LONG height, BOOL* flipped)
	{
		*flipped = FALSE;
		++storeStats.takes;
		StoreImage* s = game.initialized && stores.count ? FindFaceLink((const BYTE*)pixels, flipped) : NULL;
		if (s)
		{
			++storeStats.takeLinks;
			FaceSlot((const BYTE*)pixels)->data = NULL;
			if (!config.bpp32Hooked || width != (LONG)s->dims[0] || height != (LONG)s->dims[1] || !CheckFace(s, *flipped, (const DWORD*)pixels, width, width, height))
			{
				++storeStats.faceRejects;
				s = NULL;
			}
		}
		if (!s || !s->entry)
			return -1;

		LONG index = Resolve(s->entry, (DWORD)s->dims);
		if (index < 0 || pack.frames[index].w != (LONG)s->dims[0] || pack.frames[index].h != (LONG)s->dims[1])
			return -1;
		++storeStats.takeFrames;
		return index;
	}

	LONG PartFrame(const CHAR* name, const RECT* canvas, RECT* part)
	{
		const PackAnim* anim = game.initialized && pack.ready ? FindAnim(name) : NULL;
		if (!anim || !anim->count)
			return -1;

		const PackFrame* frame = &pack.frames[anim->first];
		SetRect(part, canvas->left - frame->ox, canvas->top - frame->oy, canvas->right - frame->ox, canvas->bottom - frame->oy);
		if (part->left < 0 || part->top < 0 || part->right > frame->w || part->bottom > frame->h)
			return -1;
		return anim->first;
	}

	GroundObject* FindGroundObject(const VOID* obj, BOOL insert)
	{
		DWORD slot = ((DWORD)obj >> 3) * 2654435761u & (GROUND_OBJECTS - 1);
		for (DWORD i = 0; i < GROUND_OBJECTS; ++i, slot = (slot + 1) & (GROUND_OBJECTS - 1))
		{
			GroundObject* o = &ground.objects[slot];
			if (o->obj == obj)
				return o;
			if (!o->obj)
			{
				if (!insert)
					return NULL;
				o->obj = obj;
				return o;
			}
		}
		return NULL;
	}

	LONG GroundFrame(const VOID* obj)
	{
		GroundObject* o = obj ? FindGroundObject(obj, FALSE) : NULL;
		if (!o)
			return -1;
		if (o->frame == GROUND_UNRESOLVED && pack.ready)
		{
			const PackAnim* anim = FindAnim(o->name);
			o->frame = anim ? (LONG)anim->first : -1;
		}
		return o->frame;
	}

	GroundChunk* FindChunk(const VOID* data)
	{
		for (DWORD i = 0; i < ground.chunkCount; ++i)
			if (ground.chunks[i].data == data && !ground.chunks[i].left && !ground.chunks[i].top)
				return &ground.chunks[i];
		return NULL;
	}

	DWORD HashRegion(const DWORD* data, LONG pitch, LONG left, LONG top, LONG w, LONG h)
	{
		DWORD hash = 2166136261u;
		for (LONG y = 0; y < h; ++y)
		{
			const DWORD* row = data + (top + y) * pitch + left;
			for (LONG x = 0; x < w; ++x)
				hash = (hash ^ row[x]) * 16777619u;
		}
		return hash;
	}

	VOID ResetChunk(GroundChunk* c, BOOL valid, DWORD color)
	{
		c->valid = valid;
		c->color = color;
		c->count = 0;
		c->pending = FALSE;
		++c->version;
	}

	GroundChunk* AddChunk(const VOID* data, LONG left, LONG top, LONG cw, LONG ch)
	{
		GroundChunk* c = NULL;
		for (DWORD i = 0; i < ground.chunkCount && !c; ++i)
			if (!ground.chunks[i].data)
				c = &ground.chunks[i];
		if (!c)
		{
			if (ground.chunkCount >= GROUND_CHUNKS)
				return NULL;
			c = &ground.chunks[ground.chunkCount++];
		}
		c->data = data;
		c->left = left;
		c->top = top;
		c->cw = cw;
		c->ch = ch;
		return c;
	}

	DWORD DropRegions(const VOID* data, LONG x, LONG y, LONG w, LONG h, const GroundChunk* keep)
	{
		DWORD dropped = 0;
		for (DWORD i = 0; i < ground.chunkCount; ++i)
		{
			GroundChunk* c = &ground.chunks[i];
			if (c != keep && c->data == data && x < c->left + c->cw && y < c->top + c->ch && x + w > c->left && y + h > c->top)
			{
				ResetChunk(c, FALSE, 0);
				c->data = NULL;
				++dropped;
			}
		}
		return dropped;
	}

	VOID OnGroundFill(const Hooks::BlitObject* obj, DWORD color, const RECT* rect)
	{
		if (!game.initialized || !obj->isTrueColor)
			return;

		GroundChunk* c = ground.chunkCount ? FindChunk(obj->data) : NULL;
		if (c)
			ResetChunk(c, rect->left <= obj->rect.left && rect->top <= obj->rect.top && rect->right >= obj->rect.right && rect->bottom >= obj->rect.bottom, color);
	}

	VOID OnFaceCopy(const DWORD* src, LONG srcPitch, const DWORD* dst, LONG top, LONG bottom, LONG width, BOOL mirror, BOOL plain)
	{
		if (!game.initialized || !stores.count)
			return;
		Twin::Cost cost(Twin::CostImage);
		++storeStats.faceCopies;

		BOOL flipped = FALSE, linked = FALSE;
		StoreImage* s = FindStoreData((const BYTE*)src);
		if (!s)
		{
			s = FindFaceLink((const BYTE*)src, &flipped);
			linked = s != NULL;
		}
		if (!linked)
			NoteFace(s, src, srcPitch, width, bottom);
		if (s && !linked && IsFace(s) && (s->first != (const BYTE*)src || s->pitch != srcPitch))
			AddRange(s, (const BYTE*)src, srcPitch, &memory.learnFace, FALSE);

		if (top || !plain)
			return;
		if (!s)
			++storeStats.faceMisses;
		if (!s || width != (LONG)s->dims[0] || bottom != (LONG)s->dims[1] || !VerifyFace(s, linked, flipped, src, srcPitch, width, bottom))
		{
			FaceLink* l = FaceSlot((const BYTE*)dst);
			if (l->data == (const BYTE*)dst)
				l->data = NULL;
			return;
		}

		++storeStats.faceLinks;
		AddFaceLink((const BYTE*)dst, s, flipped != (mirror != 0));
	}

	VOID BeginChunk(const Hooks::BlitObject* obj)
	{
		if (!Enabled() || !obj->isTrueColor)
			return;

		GroundChunk* c = FindChunk(obj->data);
		if (!c)
			c = AddChunk(obj->data, 0, 0, obj->rect.right, obj->rect.bottom);
		if (!c)
			return;
		c->cw = obj->rect.right;
		c->ch = obj->rect.bottom;
		c->pitch = obj->pitch >> 1;
		ResetChunk(c, TRUE, 0);
		++ground.builds;

		ground.flush = c;
		ground.flushStart = (const BYTE*)c->data;
		ground.flushEnd = c->cw > 0 && c->ch > 0 ? ground.flushStart + ((c->ch - 1) * c->pitch + c->cw) * sizeof(DWORD) : ground.flushStart;
	}

	VOID OnGround(GroundKind kind, const VOID* tile, const Hooks::BlitObject* obj, const POINT* src, const POINT* dst, const RECT* rect, const BYTE* blendMask, const DWORD* alphaMask)
	{
		Twin::CountGround();
		if (!Enabled() || !obj->isTrueColor || !sub_MaskData)
			return;
		Twin::Cost cost(Twin::CostImage);

		GroundChunk* c = FindChunk(obj->data);
		if (!c || !c->valid)
			return;
		if (c->count >= GROUND_OPS)
		{
			++ground.rejected;
			ResetChunk(c, FALSE, 0);
			return;
		}

		GroundOp* op = &c->ops[c->count];
		op->kind = kind;
		op->tex = kind == GroundClear ? -1 : GroundFrame(tile);
		op->mask1 = op->mask2 = -1;
		BOOL ok = !alphaMask && (kind == GroundClear || op->tex >= 0);
		if (ok && blendMask)
		{
			op->kind = GroundBlend;
			if (blendMask == ground.masks[0].data)
				op->mask1 = GroundFrame(ground.masks[0].obj);
			else if (blendMask == ground.masks[1].data)
				op->mask1 = GroundFrame(ground.masks[1].obj);
			else
			{
				op->mask1 = GroundFrame(ground.masks[0].obj);
				op->mask2 = GroundFrame(ground.masks[1].obj);
				ok = op->mask2 >= 0;
			}
			ok = ok && op->mask1 >= 0;
		}

		if (!ok)
		{
			if (alphaMask)
				++ground.rejected;
			else if (op->kind == GroundBlend && op->tex >= 0)
				++ground.unmasked;
			else
				++ground.unnamed;
			ResetChunk(c, FALSE, 0);
			return;
		}
		++ground.ops;

		op->dx = (SHORT)dst->x;
		op->dy = (SHORT)dst->y;
		op->sx = (SHORT)(src ? src->x : 0);
		op->sy = (SHORT)(src ? src->y : 0);
		op->left = (SHORT)rect->left;
		op->top = (SHORT)rect->top;
		op->right = (SHORT)rect->right;
		op->bottom = (SHORT)rect->bottom;

		LONG list[] = { op->tex, op->mask1, op->mask2 };
		for (DWORD k = 0; k < 3; ++k)
			if (list[k] >= 0)
				Touch(&pack.texs[list[k]], list[k]);

		++c->count;
		++c->version;
	}

	BOOL GroundReady(DWORD chunk)
	{
		chunk &= GROUND_CHUNKS - 1;
		if (!game.initialized || chunk >= ground.chunkCount)
			return FALSE;

		GroundChunk* c = &ground.chunks[chunk];
		if (!c->data || !c->valid || !c->count)
			return FALSE;

		BOOL ready = TRUE;
		for (DWORD i = 0; i < c->count; ++i)
		{
			const GroundOp* op = &c->ops[i];
			LONG list[] = { op->tex, op->mask1, op->mask2 };
			for (DWORD k = 0; k < 3; ++k)
				if (list[k] >= 0)
				{
					Tex* tex = &pack.texs[list[k]];
					Touch(tex, list[k]);
					ready = ready && tex->state == TexUploaded;
				}
		}
		return ready;
	}

	VOID SendGround(GroundChunk* c)
	{
		static GroundBlob blob;
		blob.count = c->count;
		blob.color = c->color;
		MemoryCopy(blob.ops, c->ops, c->count * sizeof(GroundOp));
		++ground.published;
		c->pending = FALSE;
		c->sentVersion = c->version;

		if (ground.arts < GROUND_LOG)
			DebugLog("image ground art chunk=%d data=%08X size=%dx%d ops=%d", c - ground.chunks, c->data, c->cw, c->ch, c->count);
		++ground.arts;

		RECT rect = { 0, 0, c->cw, c->ch };
		Twin::GroundArt(c->data, c->pitch, &rect, &blob, sizeof(blob) - sizeof(blob.ops) + c->count * sizeof(GroundOp));
	}

	VOID GroundFlush(const VOID* ptr)
	{
		const BYTE* at = (const BYTE*)ptr;
		if (at < ground.flushStart || at >= ground.flushEnd)
			return;

		GroundChunk* c = ground.flush;
		if (!c->data || at < (const BYTE*)c->data || c->pitch <= 0 || !c->valid || !c->count || c->sentVersion == c->version || !Enabled())
			return;

		DWORD offset = (DWORD)(at - (const BYTE*)c->data) / sizeof(DWORD);
		if ((LONG)(offset % c->pitch) >= c->cw || (LONG)(offset / c->pitch) >= c->ch || !GroundReady(c - ground.chunks))
			return;

		++ground.flushes;
		SendGround(c);
	}

	VOID RetryGround()
	{
		ground.retryFrame = game.frameNo;
		for (DWORD i = 0; i < ground.chunkCount; ++i)
		{
			GroundChunk* c = &ground.chunks[i];
			if (!c->pending)
				continue;
			if (!c->data || !c->valid || !c->count)
			{
				c->pending = FALSE;
				continue;
			}
			if (!GroundReady(i))
				continue;
			if (HashRegion((const DWORD*)c->data, c->pitch, 0, 0, c->cw, c->ch) != c->hash)
			{
				c->pending = FALSE;
				++ground.mismatched;
				continue;
			}

			++ground.retried;
			SendGround(c);
		}
	}

	VOID OnRelease(OpenDrawSurface* surface)
	{
		if (game.initialized && ground.chunkCount && surface->indexBuffer && surface->indexBuffer->data)
			DropRegions(surface->indexBuffer->data, 0, 0, 0x7FFF, 0x7FFF, NULL);
	}
#pragma endregion

#pragma region Render
	BOOL Prepare(StateBufferAligned* /*frame*/, BOOL /*active*/)
	{
		if (!game.initialized)
			return FALSE;

		InterlockedIncrement(&game.frameNo);
		return pack.decodedList.count != 0;
	}

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
			DebugLog("image shader compile failed type %X", type);
		return shader;
	}

	GLuint Link(const CHAR* vertex, const CHAR* fragment)
	{
		GLuint program = GLCreateProgram();
		GLBindAttribLocation(program, 0, "vCoord");
		GLBindAttribLocation(program, 1, "vTex");
		GLBindAttribLocation(program, 2, "vPlane");
		GLBindAttribLocation(program, 3, "vLayer");

		GLuint vShader = Compile(vertex, GL_VERTEX_SHADER);
		GLuint fShader = Compile(fragment, GL_FRAGMENT_SHADER);
		GLAttachShader(program, vShader);
		GLAttachShader(program, fShader);
		GLLinkProgram(program);
		GLDetachShader(program, fShader);
		GLDetachShader(program, vShader);
		GLDeleteShader(fShader);
		GLDeleteShader(vShader);
		return program;
	}

	VOID GpuBudget()
	{
		while (glGetError() != GL_NO_ERROR)
			;

		GLint kb[4] = {};
		GLGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, kb);
		if (glGetError() != GL_NO_ERROR || kb[0] <= 0)
		{
			kb[0] = 0;
			GLGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, kb);
			if (glGetError() != GL_NO_ERROR || kb[0] <= 0)
				kb[0] = 0;
		}

		LONGLONG budget = kb[0] ? (LONGLONG)kb[0] * 1024 / 2 : GPU_FALLBACK;
		pack.gpuBudget = (LONG)max(min(budget, (LONGLONG)GPU_FALLBACK), (LONGLONG)GPU_MIN);
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
			"in vec2 vPlane;\n"
			"in float vLayer;\n"
			"out vec2 fTex;\n"
			"out vec2 fPlane;\n"
			"out float fLayer;\n"
			"void main()\n"
			"{\n"
			"	gl_Position = vec4(vCoord, 0.0, 1.0);\n"
			"	fTex = vTex;\n"
			"	fPlane = vPlane;\n"
			"	fLayer = vLayer;\n"
			"}\n";

		const CHAR* groundFragment =
			"uniform sampler2D tex01;\n"
			"uniform sampler2D tex02;\n"
			"uniform sampler2D tex03;\n"
			"uniform int mode;\n"
			"uniform int two;\n"
			"in vec2 fTex;\n"
			"in vec2 fPlane;\n"
			"in float fLayer;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	vec2 d = fPlane - vec2(31.0, 16.0);\n"
			"	float n = abs(d.x) / 32.0 + abs(d.y) / 16.0;\n"
			"	if (n > 1.04)\n"
			"		discard;\n"
			"	vec2 m = (vec2(31.0, 16.0) + d * min(1.0, 0.96 / max(n, 0.001))) / vec2(64.0, 32.0);\n"
			"	if (mode == 3)\n"
			"		FRAG_COLOR = vec4(0.0, 0.0, 0.0, 1.0);\n"
			"	else if (mode == 2)\n"
			"	{\n"
			"		vec4 b = texture(tex01, m);\n"
			"		if (b.a < 0.5)\n"
			"			discard;\n"
			"		FRAG_COLOR = vec4(b.rgb / b.a, 1.0);\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		if (fTex.x < -0.02 || fTex.y < -0.02 || fTex.x > 1.02 || fTex.y > 1.02)\n"
			"			discard;\n"
			"		vec3 c = texture(tex01, fTex).rgb;\n"
			"		float a = 1.0;\n"
			"		if (mode == 1)\n"
			"		{\n"
			"			a = texture(tex02, m).r;\n"
			"			if (two != 0 && a < 0.04)\n"
			"				a = texture(tex03, m).r;\n"
			"		}\n"
			"		FRAG_COLOR = vec4(c * a, a);\n"
			"	}\n"
			"}\n";

		gl.generateMipmap = (GLGENERATEMIPMAP)wglGetProcAddress("glGenerateMipmap");
		gl.compressedTexImage2D = (GLCOMPRESSEDTEXIMAGE2D)wglGetProcAddress("glCompressedTexImage2D");
		QueryPerformanceFrequency(&gl.frequency);
		GpuBudget();

		groundGL.program = Link(vertex, groundFragment);
		GLUseProgram(groundGL.program);
		GLUniform1i(GLGetUniformLocation(groundGL.program, "tex01"), 0);
		GLUniform1i(GLGetUniformLocation(groundGL.program, "tex02"), 1);
		GLUniform1i(GLGetUniformLocation(groundGL.program, "tex03"), 2);
		groundGL.mode = GLGetUniformLocation(groundGL.program, "mode");
		groundGL.two = GLGetUniformLocation(groundGL.program, "two");

		GLGenVertexArrays(1, &gl.vao);
		GLBindVertexArray(gl.vao);
		GLGenBuffers(1, &gl.vbo);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLEnableVertexAttribArray(0);
		GLVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		GLEnableVertexAttribArray(1);
		GLVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)8);
		GLEnableVertexAttribArray(2);
		GLVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)16);
		GLEnableVertexAttribArray(3);
		GLVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)24);

		gl.ready = gl.vao && gl.vbo && groundGL.program;
		gl.failed = !gl.ready;
		if (gl.failed)
			DebugLog("image GL setup failed, 4K sprites off");
		else
			DebugLog("image GL ready, mipmaps %s", gl.generateMipmap ? "on" : "off");
		return gl.ready;
	}

	VOID FreeDecoded(Tex* tex)
	{
		MemoryFree(tex->pixels);
		tex->pixels = NULL;
		InterlockedExchangeAdd(&pack.decoded, -(LONG)tex->bytes);
		SetEvent(pack.space);
	}

	VOID Upload(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		GLGenTextures(1, &tex->id);
		GLBindTexture(GL_TEXTURE_2D, tex->id);
		BOOL mipmap = !tex->compressed && gl.generateMipmap;
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		if (tex->compressed)
			gl.compressedTexImage2D(GL_TEXTURE_2D, 0, tex->compressed, tex->w, tex->h, 0, tex->bytes, tex->pixels);
		else
			GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex->w, tex->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex->pixels);
		if (mipmap)
			gl.generateMipmap(GL_TEXTURE_2D);

		FreeDecoded(tex);
		tex->gpuBytes = mipmap ? tex->bytes + tex->bytes / 3 : tex->bytes;
		pack.gpu += tex->gpuBytes;
		ChainPush(&pack.gpuList, index, ListGpu, TRUE);
		InterlockedExchange(&tex->state, TexUploaded);
	}

	VOID Evict(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		ChainRemove(&pack.gpuList, index);
		GLDeleteTextures(1, &tex->id);
		tex->id = 0;
		pack.gpu -= tex->gpuBytes;
		tex->gpuBytes = 0;
		InterlockedExchange(&tex->state, TexNone);
	}

	VOID DropDecoded()
	{
		LONG now = game.frameNo;
		for (DWORD pass = 0; pass < 2 && pack.decoded >= DECODE_LIMIT; ++pass)
		{
			EnterCriticalSection(&requests.cs);
			DWORD value = pack.decodedList.head;
			while (value && pack.decoded >= DECODE_PREFETCH_LIMIT)
			{
				DWORD index = value - 1;
				Tex* tex = &pack.texs[index];
				value = tex->next;
				if (pass ? now - tex->lastUse < DECODE_STALE : !tex->prefetch)
					continue;

				ChainRemove(&pack.decodedList, index);
				FreeDecoded(tex);
				InterlockedExchange(&tex->state, TexNone);
			}
			LeaveCriticalSection(&requests.cs);
		}
	}

	VOID UploadPending()
	{
		LARGE_INTEGER start, now;
		QueryPerformanceCounter(&start);
		LONGLONG limit = gl.frequency.QuadPart * UPLOAD_US / 1000000;

		for (DWORD pass = 0; pass < 2; ++pass)
		{
			for (;;)
			{
				DWORD batch[UPLOAD_BATCH];
				DWORD count = 0;
				EnterCriticalSection(&requests.cs);
				for (DWORD value = pack.decodedList.head; value && count < UPLOAD_BATCH; value = pack.texs[value - 1].next)
					if (!pack.texs[value - 1].prefetch == !pass)
						batch[count++] = value - 1;
				for (DWORD i = 0; i < count; ++i)
					ChainRemove(&pack.decodedList, batch[i]);
				LeaveCriticalSection(&requests.cs);
				if (!count)
					break;

				DWORD done = 0;
				for (; done < count; ++done)
				{
					QueryPerformanceCounter(&now);
					if (now.QuadPart - start.QuadPart >= limit)
						break;
					Upload(batch[done]);
				}

				if (done < count)
				{
					EnterCriticalSection(&requests.cs);
					for (DWORD i = count; i > done; --i)
						ChainPush(&pack.decodedList, batch[i - 1], ListDecoded, TRUE);
					LeaveCriticalSection(&requests.cs);
					return;
				}
			}
		}
	}

	VOID EvictGpu()
	{
		LONG low = pack.gpuBudget - pack.gpuBudget / 8;
		if (pack.gpu <= pack.gpuBudget)
			return;

		LONG now = game.frameNo;
		DWORD value = pack.gpuList.tail;
		DWORD freed = 0;
		while (value && pack.gpu > low)
		{
			DWORD index = value - 1;
			value = pack.texs[index].prev;
			if (now - pack.texs[index].lastUse <= 1)
				continue;
			Evict(index);
			++freed;
		}
		if (freed)
			InterlockedIncrement(&pack.evictGen);
	}

	VOID Manage()
	{
		DropDecoded();
		UploadPending();
		EvictGpu();
	}

	VOID TouchGpu(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		if (tex->list == ListGpu && pack.gpuList.head != index + 1)
		{
			ChainRemove(&pack.gpuList, index);
			ChainPush(&pack.gpuList, index, ListGpu, TRUE);
		}
	}

	BOOL ArtTexture(LONG frame, GLuint* id, LONG* width, LONG* height)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.frameCount || pack.texs[frame].state != TexUploaded)
			return FALSE;

		TouchGpu(frame);
		*id = pack.texs[frame].id;
		*width = pack.frames[frame].w;
		*height = pack.frames[frame].h;
		return TRUE;
	}

	// render thread, inside the twin replay: a seed recorded once its art was decoded needs it on the GPU now
	LONG ArtBytes(LONG frame)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.frameCount)
			return 0;
		return (LONG)pack.texs[frame].bytes;
	}

	BOOL ArtUpload(LONG frame)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.frameCount || !InitGL())
			return FALSE;

		Tex* tex = &pack.texs[frame];
		if (tex->state == TexUploaded)
			return TRUE;

		BOOL take = FALSE;
		EnterCriticalSection(&requests.cs);
		if (tex->state == TexDecoded && tex->list == ListDecoded)
		{
			ChainRemove(&pack.decodedList, frame);
			take = TRUE;
		}
		LeaveCriticalSection(&requests.cs);
		if (take)
			Upload(frame);
		return take;
	}

	BOOL DrawGround(const VOID* ops, DWORD bytes, LONG width, LONG height)
	{
		const GroundBlob* blob = (const GroundBlob*)ops;
		DWORD header = sizeof(GroundBlob) - sizeof(blob->ops);
		if (!game.initialized || !ops || bytes < header || width <= 0 || height <= 0)
			return FALSE;

		DWORD count = blob->count, color = blob->color;
		if (!count || count > GROUND_OPS || bytes < header + count * sizeof(GroundOp) || !InitGL())
			return FALSE;

		for (DWORD i = 0; i < count; ++i)
		{
			const GroundOp* op = &blob->ops[i];
			LONG list[] = { op->tex, op->mask1, op->mask2 };
			for (DWORD k = 0; k < 3; ++k)
				if (list[k] >= 0 && pack.texs[list[k]].state != TexUploaded)
					return FALSE;
		}

		GLint view[4], box[4];
		GLfloat clear[4];
		GLGetIntegerv(GL_VIEWPORT, view);
		GLGetIntegerv(GL_SCISSOR_BOX, box);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);

		FLOAT alpha = (FLOAT)(color >> 24) / 255.0f;
		GLClearColor((FLOAT)(color & 0xFF) / 255.0f * alpha, (FLOAT)((color >> 8) & 0xFF) / 255.0f * alpha, (FLOAT)((color >> 16) & 0xFF) / 255.0f * alpha, alpha);
		GLClear(GL_COLOR_BUFFER_BIT);
		GLClearColor(clear[0], clear[1], clear[2], clear[3]);

		GLUseProgram(groundGL.program);
		GLBindVertexArray(gl.vao);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);

		for (DWORD i = 0; i < count; ++i)
		{
			const GroundOp* op = &blob->ops[i];
			LONG l = max(view[0] + op->left * view[2] / width, box[0]);
			LONG t = max(view[1] + op->top * view[3] / height, box[1]);
			LONG r = min(view[0] + op->right * view[2] / width, box[0] + box[2]);
			LONG b = min(view[1] + op->bottom * view[3] / height, box[1] + box[3]);
			if (l >= r || t >= b)
				continue;

			LONG list[] = { op->tex, op->mask1, op->mask2 };
			for (DWORD k = 0; k < 3; ++k)
			{
				GLActiveTexture(GL_TEXTURE0 + k);
				GLBindTexture(GL_TEXTURE_2D, list[k] >= 0 ? pack.texs[list[k]].id : 0);
				if (list[k] >= 0)
					TouchGpu(list[k]);
			}

			GLUniform1i(groundGL.mode, op->kind);
			GLUniform1i(groundGL.two, op->mask2 >= 0);
			if (op->kind == GroundCopy || op->kind == GroundClear)
				GLDisable(GL_BLEND);
			else
			{
				GLEnable(GL_BLEND);
				GLBlendFunc(op->kind == GroundBlend ? GL_DST_ALPHA : GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
			glScissor(l, t, r - l, b - t);

			FLOAT fw = 1.0f, fh = 1.0f;
			if (op->tex >= 0 && op->kind != GroundBorder)
			{
				fw = (FLOAT)pack.frames[op->tex].w;
				fh = (FLOAT)pack.frames[op->tex].h;
			}
			FLOAT nx0 = (FLOAT)op->dx / width * 2.0f - 1.0f, nx1 = (FLOAT)(op->dx + 64) / width * 2.0f - 1.0f;
			FLOAT ny0 = (FLOAT)op->dy / height * 2.0f - 1.0f, ny1 = (FLOAT)(op->dy + 32) / height * 2.0f - 1.0f;
			FLOAT u0 = op->sx / fw, u1 = (op->sx + 64) / fw;
			FLOAT v0 = op->sy / fh, v1 = (op->sy + 32) / fh;

			Vertex quad[6] = {
				{ nx0, ny0, u0, v0, 0.0f, 0.0f, 0.0f },
				{ nx1, ny0, u1, v0, 64.0f, 0.0f, 0.0f },
				{ nx1, ny1, u1, v1, 64.0f, 32.0f, 0.0f },
				{ nx0, ny0, u0, v0, 0.0f, 0.0f, 0.0f },
				{ nx1, ny1, u1, v1, 64.0f, 32.0f, 0.0f },
				{ nx0, ny1, u0, v1, 0.0f, 32.0f, 0.0f }
			};
			GLBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STREAM_DRAW);
			GLDrawArrays(GL_TRIANGLES, 0, 6);
		}

		glScissor(box[0], box[1], box[2], box[3]);
		GLDisable(GL_BLEND);
		GLActiveTexture(GL_TEXTURE0);
		return TRUE;
	}

	VOID Draw(const Rect* /*rect*/, LONG /*offset*/, DWORD /*frameWidth*/, DWORD /*frameHeight*/)
	{
		if (!pack.ready || (!pack.decodedList.count && pack.gpu <= pack.gpuBudget))
			return;

		GLint program, vao, buffer, active, texture0;
		GLGetIntegerv(GL_CURRENT_PROGRAM, &program);
		GLGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		GLGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		GLGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		GLActiveTexture(GL_TEXTURE0);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);

		if (InitGL())
			Manage();

		GLActiveTexture(GL_TEXTURE0);
		GLBindTexture(GL_TEXTURE_2D, texture0);
		GLActiveTexture(active);
		GLBindBuffer(GL_ARRAY_BUFFER, buffer);
		GLBindVertexArray(vao);
		GLUseProgram(program);
	}

	VOID ReleaseGL()
	{
		if (gl.ready)
		{
			while (pack.gpuList.tail)
				Evict(pack.gpuList.tail - 1);
			InterlockedIncrement(&pack.evictGen);

			GLDeleteBuffers(1, &gl.vbo);
			GLDeleteVertexArrays(1, &gl.vao);
			GLDeleteProgram(groundGL.program);
		}
		groundGL.program = 0;

		gl.ready = FALSE;
		gl.failed = FALSE;
		gl.vao = gl.vbo = 0;
	}
#pragma endregion

#pragma region Hooks
	VOID* __fastcall Build(VOID* thisObj, DWORD /*edx*/, const CHAR* name, DWORD flipped, VOID* idManager)
	{
		VOID* image = ((VOID*(__thiscall*)(VOID*, const CHAR*, DWORD, VOID*))sub_Build)(thisObj, name, flipped, idManager);
		++stats.builds;
		DWORD frame = current.frame++;

		if (image)
		{
			DWORD rec = GetRecord(image);
			Entry* entry = rec ? Find(rec, TRUE) : NULL;
			if (entry)
				++entry->refs;
			if (entry && !entry->name[0] && name)
			{
				entry->flipped = (BYTE)flipped;
				entry->frame = (WORD)frame;
				CopyName(entry->name, name);
				StrCopy(entry->owner, current.name);
			}
		}

		return image;
	}

	DWORD __fastcall Names(VOID* thisObj, DWORD /*edx*/, const CHAR* name, VOID* list)
	{
		if (name)
			CopyName(current.name, name);
		else
			current.name[0] = '\0';
		current.frame = 0;

		return ((DWORD(__thiscall*)(VOID*, const CHAR*, VOID*))sub_Names)(thisObj, name, list);
	}

	VOID InsertArchiveFile(Archive* archive, DWORD id, const CHAR* name)
	{
		DWORD slot = (id * 2654435761u) & archive->mask;
		while (archive->files[slot].id && archive->files[slot].id != id)
			slot = (slot + 1) & archive->mask;

		ArchiveFile* file = &archive->files[slot];
		if (file->id)
			return;

		file->id = id;
		const CHAR* end = (const CHAR*)MemoryChar(name, '\0', ARCHIVE_ENTRY_SIZE - 4);
		DWORD length = end ? end - name : 0;
		if (length > NAME_LENGTH - 1)
			length = NAME_LENGTH - 1;
		MemoryCopy(file->name, name, length);
		file->name[length] = '\0';
	}

	DWORD LoadArchive(Archive* archive)
	{
		HANDLE handle = CreateFile(archive->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (handle == INVALID_HANDLE_VALUE)
			return 0;

		BYTE* table = NULL;
		DWORD size = 0;
		BYTE head[28];
		for (LONGLONG offset = 28; ReadAt(handle, offset, head, sizeof(head)) && !MemoryCompare(head, "MQRC", 4); offset += sizeof(head) + *(const DWORD*)(head + 16))
		{
			if (*(const DWORD*)(head + 8) != 2)
				continue;

			size = *(const DWORD*)(head + 12);
			table = size >= 4 ? (BYTE*)MemoryAlloc(size) : NULL;
			if (table && !ReadAt(handle, offset + sizeof(head), table, size))
			{
				MemoryFree(table);
				table = NULL;
			}
			break;
		}
		CloseHandle(handle);
		if (!table)
			return 0;

		DWORD count = min(*(const DWORD*)table, (size - 4) / ARCHIVE_ENTRY_SIZE);
		DWORD capacity = 16;
		while (capacity < count * 2)
			capacity <<= 1;

		archive->files = (ArchiveFile*)MemoryAlloc(capacity * sizeof(ArchiveFile));
		if (archive->files)
		{
			MemoryZero(archive->files, capacity * sizeof(ArchiveFile));
			archive->mask = capacity - 1;
			for (DWORD i = 0; i < count; ++i)
			{
				const CHAR* entry = (const CHAR*)table + 4 + i * ARCHIVE_ENTRY_SIZE;
				DWORD id = *(const DWORD*)(entry + ARCHIVE_ENTRY_SIZE - 4);
				if (id)
					InsertArchiveFile(archive, id, entry);
			}
		}
		MemoryFree(table);
		return archive->files ? count : 0;
	}

	// Discipl2.exe v3.01b 0x00522E7E, same code in every build: factory+0xC -> cache -> {file} -> {MqDB holder} -> MqDB, path at MqDB+8; the translator at factory+0x14 holds archive index * 100000 at +4
	const CHAR* ArchivePath(const VOID* factory, DWORD* offset)
	{
		const BYTE* translator = *(const BYTE* const*)((const BYTE*)factory + 0x10);
		if (!translator || IsBadReadPtr(translator + 4, 4))
			return NULL;
		*offset = *(const DWORD*)(translator + 4);

		const BYTE* p = (const BYTE*)factory + 8;
		for (DWORD i = 0; i < 6; ++i)
		{
			if (!p || IsBadReadPtr(p, 4))
				return NULL;
			p = *(const BYTE* const*)p;
		}
		if (!p || IsBadReadPtr(p + 8, 4))
			return NULL;

		const CHAR* path = *(const CHAR* const*)(p + 8);
		return path && !IsBadReadPtr(path, 4) ? path : NULL;
	}

	CRITICAL_SECTION archiveCs;
	BOOL archiveCsReady;

	// the store names an archive by the path its MqDB holds, the record reads by the path CreateFileA got; both are compared as full paths
	Archive* FindArchive(const CHAR* path)
	{
		CHAR full[MAX_PATH];
		DWORD length = GetFullPathName(path, MAX_PATH, full, NULL);
		if (!length || length >= MAX_PATH)
			return NULL;

		if (archiveCsReady)
			EnterCriticalSection(&archiveCs);
		Archive* archive = NULL;
		for (DWORD i = 0; i < archives.count && !archive; ++i)
			if (!StrCompareInsensitive(archives.list[i].path, full))
				archive = &archives.list[i];
		if (!archive && archives.count < ARCHIVE_LIMIT)
		{
			archive = &archives.list[archives.count];
			StrCopy(archive->path, full);
			LoadArchive(archive);
			++archives.count;
		}
		if (archiveCsReady)
			LeaveCriticalSection(&archiveCs);
		return archive;
	}

	ArchiveFile* ArchiveRecord(Archive* archive, DWORD rec)
	{
		if (!archive || !archive->files || !rec)
			return NULL;
		for (DWORD slot = (rec * 2654435761u) & archive->mask; archive->files[slot].id; slot = (slot + 1) & archive->mask)
			if (archive->files[slot].id == rec)
				return &archive->files[slot];
		return NULL;
	}

	ArchiveFile* FindArchiveFile(const VOID* factory, DWORD id)
	{
		DWORD offset;
		const CHAR* path = ArchivePath(factory, &offset);
		if (!path)
			return NULL;

		Archive* archive = FindArchive(path);
		return ArchiveRecord(archive, id > offset ? id - offset : id);
	}

	#define SHEET_RECORD_LIMIT (64 * 1024 * 1024)
	#define SHEET_BLOCKS 32
	#define SHEET_LOGS 32

	struct {
		DWORD archives, logs, emptyLogs;
	} sheets;

	DWORD HashUpper(const CHAR* text)
	{
		DWORD hash = 2166136261u;
		for (; *text; ++text)
		{
			BYTE c = (BYTE)*text;
			if (c >= 'a' && c <= 'z')
				c -= 32;
			hash = (hash ^ c) * 16777619u;
		}
		return hash ? hash : 1;
	}

	Archive* FileArchive(const ArchiveFile* file)
	{
		for (DWORD i = 0; i < archives.count; ++i)
		{
			Archive* a = &archives.list[i];
			if (a->files && file >= a->files && file <= a->files + a->mask)
				return a;
		}
		return NULL;
	}

	const CHAR* TextAt(const BYTE* p, const BYTE* end, const BYTE** next)
	{
		const BYTE* zero = p < end ? (const BYTE*)MemoryChar(p, '\0', end - p) : NULL;
		if (!zero)
			return NULL;
		*next = zero + 1;
		return (const CHAR*)p;
	}

	// -INDEX.OPT maps each frame name to the sheet file and its block in -IMAGES.OPT; -ANIMS.OPT lists each animation's frames in order (Gallean ImagesExtractor)
	VOID LoadSheets(Archive* a)
	{
		a->sheetsLoaded = TRUE;
		if (!a->files)
			return;

		const CHAR* names[3] = { "-INDEX.OPT", "-IMAGES.OPT", "-ANIMS.OPT" };
		DWORD ids[3] = { 0, 0, 0 };
		for (DWORD i = 0; i <= a->mask; ++i)
			for (DWORD k = 0; k < 3; ++k)
				if (a->files[i].id && !StrCompareInsensitive(a->files[i].name, names[k]))
					ids[k] = a->files[i].id;
		if (!ids[0] || !ids[1])
			return;

		HANDLE handle = CreateFile(a->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (handle == INVALID_HANDLE_VALUE)
			return;

		BYTE* data[3] = { NULL, NULL, NULL };
		DWORD sizes[3] = { 0, 0, 0 };
		DWORD want = ids[2] ? 3 : 2, found = 0;
		BYTE head[28];
		for (LONGLONG offset = 28; found < want && ReadAt(handle, offset, head, sizeof(head)) && !MemoryCompare(head, "MQRC", 4); offset += sizeof(head) + *(const DWORD*)(head + 16))
		{
			DWORD id = *(const DWORD*)(head + 8);
			DWORD size = *(const DWORD*)(head + 12);
			for (DWORD k = 0; k < 3; ++k)
				if (ids[k] && id == ids[k] && !data[k] && size && size <= SHEET_RECORD_LIMIT)
				{
					data[k] = (BYTE*)MemoryAlloc(size + 1);
					if (data[k] && ReadAt(handle, offset + sizeof(head), data[k], size))
					{
						data[k][size] = 0;
						sizes[k] = size;
						++found;
					}
					else if (data[k])
					{
						MemoryFree(data[k]);
						data[k] = NULL;
					}
				}
		}
		CloseHandle(handle);

		if (!data[0] || !data[1] || sizes[0] < 4)
		{
			for (DWORD k = 0; k < 3; ++k)
				if (data[k])
					MemoryFree(data[k]);
			DebugLog("image sheets %s: index or images record not read", a->path);
			return;
		}

		a->indexData = data[0];
		a->images = data[1];
		a->imagesSize = sizes[1];
		a->animsData = data[2];

		const BYTE* p = data[0] + 4;
		const BYTE* end = data[0] + sizes[0];
		DWORD count = *(const DWORD*)data[0];
		count = min(count, sizes[0] / 13);
		a->items = (SheetItem*)MemoryAlloc(max(count, 1u) * sizeof(SheetItem));
		const CHAR** animNames = (const CHAR**)MemoryAlloc(max(count, 1u) * sizeof(CHAR*));
		DWORD animCount = 0;
		a->itemMask = 15;
		while (a->itemMask + 1 < count * 2)
			a->itemMask = a->itemMask * 2 + 1;
		a->itemHeads = (DWORD*)MemoryAlloc((a->itemMask + 1) * sizeof(DWORD));
		if (!a->items || !animNames || !a->itemHeads)
		{
			if (animNames)
				MemoryFree(animNames);
			return;
		}
		MemoryZero(a->itemHeads, (a->itemMask + 1) * sizeof(DWORD));

		for (DWORD i = 0; i < count && p + 4 <= end; ++i)
		{
			DWORD id = *(const DWORD*)p;
			const BYTE* next;
			const CHAR* name = TextAt(p + 4, end, &next);
			if (!name || next + 8 > end)
				break;
			p = next + 8;
			if (id == 0xFFFFFFFF)
			{
				animNames[animCount++] = name;
				continue;
			}

			SheetItem* item = &a->items[a->itemCount];
			item->id = id;
			item->offset = *(const DWORD*)next;
			DWORD* head = &a->itemHeads[(id * 2654435761u) & a->itemMask];
			item->next = *head;
			*head = ++a->itemCount;
		}

		if (a->animsData && animCount)
		{
			DWORD frames = 0;
			p = a->animsData;
			end = a->animsData + sizes[2];
			for (DWORD k = 0; k < animCount && p + 4 <= end; ++k)
			{
				DWORD n = *(const DWORD*)p;
				p += 4;
				for (DWORD j = 0; j < n && p < end; ++j)
				{
					const BYTE* next;
					if (!TextAt(p, end, &next))
						break;
					p = next;
					++frames;
				}
			}

			a->linkMask = 15;
			while (a->linkMask + 1 < frames * 2)
				a->linkMask = a->linkMask * 2 + 1;
			a->links = (SheetLink*)MemoryAlloc((a->linkMask + 1) * sizeof(SheetLink));
			if (a->links)
			{
				MemoryZero(a->links, (a->linkMask + 1) * sizeof(SheetLink));
				p = a->animsData;
				for (DWORD k = 0; k < animCount && p + 4 <= end; ++k)
				{
					DWORD n = *(const DWORD*)p;
					p += 4;
					for (DWORD j = 0; j < n && p < end; ++j)
					{
						const BYTE* next;
						const CHAR* frame = TextAt(p, end, &next);
						if (!frame)
							break;
						p = next;

						DWORD hash = HashUpper(frame);
						DWORD slot = hash & a->linkMask;
						while (a->links[slot].hash && (a->links[slot].hash != hash || StrCompareInsensitive(a->links[slot].frame, frame)))
							slot = (slot + 1) & a->linkMask;
						if (a->links[slot].hash)
							continue;
						a->links[slot].hash = hash;
						a->links[slot].frame = frame;
						a->links[slot].anim = animNames[k];
						a->links[slot].index = j;
						++a->linkCount;
					}
				}
			}
		}
		MemoryFree(animNames);

		++sheets.archives;
		DebugLog("image sheets %s: %u frame entries, %u animations (%u frames), images %u bytes", a->path, a->itemCount, animCount, a->linkCount, a->imagesSize);
	}

	const SheetLink* FindLink(const Archive* a, const CHAR* frame)
	{
		if (!a->links)
			return NULL;
		DWORD hash = HashUpper(frame);
		for (DWORD slot = hash & a->linkMask; a->links[slot].hash; slot = (slot + 1) & a->linkMask)
			if (a->links[slot].hash == hash && !StrCompareInsensitive(a->links[slot].frame, frame))
				return &a->links[slot];
		return NULL;
	}

	Twin::SeedPiece sheetPieces[4096];
	Twin::SeedConflict sheetConflicts[256];
	BYTE sheetShared[4096];
	// per piece, for the piece log and the overlap check: the part's place in its frame's canvas and the frame's entry in sheetFrames
	POINT sheetCanvas[4096];
	WORD sheetFrameOf[4096];
	BYTE sheetDrop[4096];

	#define SHEET_FRAME_INFO 64
	#define SHEET_PIECE_LOGS 20
	#define SHEET_UNCERTAIN_LOGS 64
	#define SHEET_FORCED_LOGS 96
	#define SHEET_OVERLAP_LIMIT 1024

	struct SheetFrameInfo {
		const CHAR* name;
		const PackAnim* anim;
		LONG frame;
		LONG cw, ch;
		BYTE origin;
		BYTE exact;
		// the pack entry is named like the sheet file; it holds the frame's canvas all the same (see SheetPieces), so this only marks it in the log
		BYTE fileNamed;
		// a file-named frame whose parts would take other 4K if the entry were read as the file itself (identity), so the log can tell the readings apart
		BYTE readingsDiffer;
		// the pack entry's box lies outside the union of the frame's parts in its canvas, so it is not this frame (left 1x)
		BYTE outsideParts;
		// DeriveResult: the entry's offset was zeroed by the repack and is taken from the parts (or the 1x key) instead; see DeriveOffset
		BYTE derived;
		// an event portrait's entry, the composite of every frame of the file: bounds is the union of all their parts
		BYTE composite;
		WORD parts;
		RECT bounds;
		// the offset the frame was placed with (the entry's, or the derived one)
		LONG ox, oy;
	};

	SheetFrameInfo sheetFrames[SHEET_FRAME_INFO];
	DWORD sheetFrameCount;

	struct {
		DWORD pieceLogs, uncertainLogs;
		DWORD wrongSize, fileNamed, fileNamedDiffer, notFrame, offsetLost, overlaps, duplicates, dropped, unchecked, forcedLogs;
	} sheetCheck;

	// Pack entries whose crop offset the repack lost. tools/repack/repack.py copies Gallean's assets-v1 entries under game names and, for the categories it files
	// with a SPRITE origin (item-icon, spell-icon, unit-event-portrait, building-icon, unit-face, unit-portrait, lord-face, city-image, capital-image), sets OffsetX
	// and OffsetY to 0. Read in canvas layout, such an entry sits at the canvas origin. Where the entry is a crop of the frame (or, for an event portrait, of the
	// composite of every frame of the file), that 0,0 is wrong, and every piece samples the 4K the union's origin away from its own place. The place is taken
	// back from geometry only: the parts union, the canvas, or the non-transparent box of the game's own 1x decode (its key), never a comparison of pixels
	enum DeriveResult { DeriveNone, DerivePlaced, DeriveFullCanvas, DeriveUnplaced };

	#define DERIVED_LIMIT 256
	#define DERIVED_LOGS 128

	struct DerivedOffset {
		LONG frame;
		DWORD fileId;
		LONG ox, oy;
		BYTE result;
	};

	struct {
		DerivedOffset list[DERIVED_LIMIT];
		DWORD count, next;
		DWORD placedUnion, placedKey, fullCanvas, unplaced, noKey, logs, noKeyLogs;
	} derived;

	const CHAR* const deriveNames[] = { "-", "derived", "full canvas", "left 1x" };

	BOOL KeyIndices(const Archive* a, DWORD fileId, LONG w, LONG h, const BYTE** data, LONG* pitch);

	// names Gallean files as SPRITE-origin categories, whose offsets the repack zeroed: spell icons (G000SSnnnn), unit faces and portraits (...FACE.PNG, ...FACEB.PNG),
	// and every entry used by a frame of Events.ff (event portraits) or Faces.ff
	BOOL ZeroedName(const Archive* a, const CHAR* entry)
	{
		DWORD pathLength = StrLength(a->path);
		if ((pathLength >= 10 && !StrCompareInsensitive(a->path + pathLength - 10, "\\Events.ff")) || (pathLength >= 9 && !StrCompareInsensitive(a->path + pathLength - 9, "\\Faces.ff")))
			return TRUE;
		DWORD length = StrLength(entry);
		if (length == 10 && !StrCompareNum(entry, "G000SS", 6))
			return TRUE;
		return (length > 8 && !StrCompareInsensitive(entry + length - 8, "FACE.PNG")) || (length > 9 && !StrCompareInsensitive(entry + length - 9, "FACEB.PNG"));
	}

	// The non-transparent box, in the canvas, of the frames Gallean puts into one entry, from the game's own 1x key: a pixel is transparent where its index is the
	// block's transparent index or a brush colour (ImagesExtractor.GetPaletteOpacity: opacity is the block's algorithm byte, 0 for brush colours and the transparent
	// index; additive algorithms 300/400 are skipped by the caller). Parts sit at their canvas place (BuildImage), clipped to the canvas (Composite, ClipToCanvas).
	// only: one frame, or NULL for every frame of the file (an event portrait's composite). 1 with *box (empty when nothing is opaque), 0 when the 8-bit decode
	// of this file is not at hand, -1 when two parts of one frame overlap: the later one overwrites the earlier one's pixels, so the union of opaque parts is not the frame
	LONG KeyBox(const Archive* a, DWORD fileId, LONG w, LONG h, const DWORD* offsets, DWORD blocks, const CHAR* only, LONG canvasW, LONG canvasH, RECT* box)
	{
		const BYTE* data;
		LONG pitch;
		if (!KeyIndices(a, fileId, w, h, &data, &pitch))
			return 0;

		LONG minX = canvasW, minY = canvasH, maxX = -1, maxY = -1;
		const BYTE* end = a->images + a->imagesSize;
		for (DWORD b = 0; b < blocks; ++b)
		{
			if (offsets[b] > a->imagesSize || a->imagesSize - offsets[b] < 1 + 2 + 4 + 4 + 1024 + 4)
				continue;
			const BYTE* base = a->images + offsets[b];
			LONG sizeX = *(const LONG*)(base + 3), sizeY = *(const LONG*)(base + 7);
			if ((sizeX || sizeY) && (sizeX != w || sizeY != h))
				continue;

			BYTE opaque[256];
			SHORT algorithm = *(const SHORT*)(base + 1);
			const BYTE* palette = base + 11;
			for (DWORD i = 0; i < 256; ++i)
			{
				const BYTE* c = palette + i * 4;
				BOOL brush = (c[0] & 0xF8) == 0xF8 && (c[1] & 0xF8) == 0 && (c[2] & 0xF8) == 0xF8;
				if (algorithm <= 255)
					opaque[i] = (BYTE)((BYTE)algorithm && !brush);
				else if (algorithm == 300 || algorithm == 400)
					opaque[i] = (BYTE)(i != 0);
				else
					opaque[i] = TRUE;
			}
			opaque[base[0]] = FALSE;

			const BYTE* p = base + 1 + 2 + 4 + 4 + 1024;
			DWORD frames = *(const DWORD*)p;
			p += 4;
			for (DWORD f = 0; f < frames; ++f)
			{
				const BYTE* next;
				const CHAR* name = TextAt(p, end, &next);
				if (!name || next + 12 > end)
					break;
				DWORD parts = *(const DWORD*)next;
				p = next + 12;
				if (parts > (DWORD)(end - p) / 24)
					break;
				const LONG* part = (const LONG*)p;
				p += parts * 24;
				if (only && StrCompareInsensitive(name, only))
					continue;

				for (DWORD i = 0; i < parts; ++i)
				{
					const LONG* q = part + i * 6;
					RECT r = { q[0], q[1], q[0] + q[4], q[1] + q[5] };
					for (DWORD k = i + 1; k < parts; ++k)
					{
						const LONG* o = part + k * 6;
						RECT s = { o[0], o[1], o[0] + o[4], o[1] + o[5] }, both;
						if (IntersectRect(&both, &r, &s))
							return -1;
					}
				}

				for (DWORD i = 0; i < parts; ++i, part += 6)
				{
					LONG cx = part[0], cy = part[1], sx = part[2], sy = part[3], pw = part[4], ph = part[5];
					if (pw <= 0 || ph <= 0 || sx < 0 || sy < 0 || sx + pw > w || sy + ph > h)
						continue;
					LONG y0 = max(0L, -cy), y1 = min(ph, canvasH - cy), x0 = max(0L, -cx), x1 = min(pw, canvasW - cx);
					for (LONG y = y0; y < y1; ++y)
					{
						const BYTE* row = data + (sy + y) * pitch + sx;
						for (LONG x = x0; x < x1; ++x)
						{
							if (!opaque[row[x]])
								continue;
							LONG X = cx + x, Y = cy + y;
							if (X < minX)
								minX = X;
							if (X > maxX)
								maxX = X;
							if (Y < minY)
								minY = Y;
							if (Y > maxY)
								maxY = Y;
						}
					}
				}
			}
		}

		if (maxX < minX || maxY < minY)
			SetRectEmpty(box);
		else
			SetRect(box, minX, minY, maxX + 1, maxY + 1);
		return 1;
	}

	// For a still whose gamenames entry sits at 0,0 where its frame cannot start: the union of the frame's parts (every frame of the file for an event portrait's
	// composite) does not start at 0,0, or the entry is a SPRITE-origin name smaller than that union. Then, from geometry only:
	// - the entry is the whole canvas: Gallean's full-canvas sprite or composite (GameImages.ToFullCanvasSprite, SpriteOperations.Composite), so 0,0 is its real place;
	// - the entry is the union's size: it is the union, at the union's origin;
	// - the entry is smaller than the union (transparent edges trimmed): its place is the origin of the non-transparent box of the 1x decode, when that box has the
	//   entry's size; otherwise, or when the decode is not at hand, the frame stays 1x;
	// - the entry is larger than the union but not the canvas: left to the outside check (another image of that name).
	// One line per entry is logged the first time; the result is kept per pack frame and file
	BYTE DeriveOffset(const Archive* a, DWORD fileId, const CHAR* label, LONG w, LONG h, const DWORD* offsets, DWORD blocks, LONG frame, const CHAR* entryName, const CHAR* frameName,
		BOOL composite, const RECT* parts, LONG canvasW, LONG canvasH, LONG* ox, LONG* oy)
	{
		const PackFrame* pf = &pack.frames[frame];
		if (pf->ox || pf->oy || IsRectEmpty(parts))
			return DeriveNone;
		LONG uw = parts->right - parts->left, uh = parts->bottom - parts->top;
		BOOL inside = pf->w <= uw && pf->h <= uh;
		BOOL canvas = pf->w == canvasW && pf->h == canvasH && (parts->left || parts->top || uw != canvasW || uh != canvasH);
		BOOL trimmedSprite = inside && (pf->w < uw || pf->h < uh) && ZeroedName(a, entryName);
		if (!canvas && !parts->left && !parts->top && !trimmedSprite)
			return DeriveNone;

		for (DWORD i = 0; i < derived.count; ++i)
		{
			const DerivedOffset* d = &derived.list[i];
			if (d->frame == frame && d->fileId == fileId)
			{
				*ox = d->ox;
				*oy = d->oy;
				return d->result;
			}
		}

		DerivedOffset d = { frame, fileId, 0, 0, DeriveNone };
		const CHAR* how;
		RECT key = { 0, 0, 0, 0 };
		LONG keyResult = 2;
		if (canvas)
		{
			d.result = DeriveFullCanvas;
			how = "the entry is the whole canvas (Gallean's full-canvas sprite or composite), so 0,0 is its real place";
			++derived.fullCanvas;
		}
		else if (pf->w == uw && pf->h == uh)
		{
			d.ox = parts->left;
			d.oy = parts->top;
			d.result = DerivePlaced;
			how = "the entry has the parts union's size, so it is the union: offset = union origin";
			++derived.placedUnion;
		}
		else if (inside)
		{
			keyResult = KeyBox(a, fileId, w, h, offsets, blocks, composite ? NULL : frameName, canvasW, canvasH, &key);
			if (!keyResult)
			{
				// not kept: the next decode of this file may have its 8-bit indices at hand
				++derived.noKey;
				if (derived.noKeyLogs < DERIVED_LOGS / 4)
				{
					++derived.noKeyLogs;
					DebugLog("image pack offset left 1x: %.48s #%d entry %dx%d at 0,0 (offset zeroed by the repack) | %.40s %s %.48s canvas %dx%d, parts union %d,%d %dx%d | smaller than the union and the 1x decode's 8-bit indices are not at hand, so its trimmed box cannot be placed",
						entryName, frame, pf->w, pf->h, label, composite ? "every frame of the file (event portrait composite), seeded frame" : "frame", frameName, canvasW, canvasH, parts->left, parts->top, uw, uh);
				}
				*ox = *oy = 0;
				return DeriveUnplaced;
			}
			if (keyResult < 0)
			{
				d.result = DeriveUnplaced;
				how = "smaller than the union, and two parts of one frame overlap, so the 1x key cannot give the composite's box: left 1x";
				++derived.unplaced;
			}
			else if (key.right - key.left == pf->w && key.bottom - key.top == pf->h)
			{
				d.ox = key.left;
				d.oy = key.top;
				d.result = DerivePlaced;
				how = "smaller than the union (transparent edges trimmed): offset = origin of the 1x decode's non-transparent box (block's transparent index and brush colours), which has the entry's size";
				++derived.placedKey;
			}
			else
			{
				d.result = DeriveUnplaced;
				how = "smaller than the union, and the 1x decode's non-transparent box is not the entry's size: left 1x";
				++derived.unplaced;
			}
		}
		else
			return DeriveNone;

		if (derived.count < DERIVED_LIMIT)
			derived.list[derived.count++] = d;
		else
		{
			derived.list[derived.next] = d;
			derived.next = (derived.next + 1) % DERIVED_LIMIT;
		}

		if (derived.logs < DERIVED_LOGS)
		{
			++derived.logs;
			CHAR keyText[64] = "";
			if (keyResult == 1)
				StrPrint(keyText, ", 1x key box %d,%d %dx%d", key.left, key.top, key.right - key.left, key.bottom - key.top);
			DebugLog("image pack offset %s: %.48s #%d entry %dx%d at 0,0 (offset zeroed by the repack) | %.40s %s %.48s canvas %dx%d, parts union %d,%d %dx%d%s | offset %d,%d: %s",
				deriveNames[d.result], entryName, frame, pf->w, pf->h, label, composite ? "every frame of the file (event portrait composite), seeded frame" : "frame", frameName, canvasW, canvasH,
				parts->left, parts->top, uw, uh, keyText, d.ox, d.oy, how);
		}

		*ox = d.ox;
		*oy = d.oy;
		return d.result;
	}

	// the same name with or without .PNG
	BOOL SameStem(const CHAR* a, const CHAR* b)
	{
		DWORD la = StrLength(a), lb = StrLength(b);
		if (la > 4 && !StrCompareInsensitive(a + la - 4, ".PNG"))
			la -= 4;
		if (lb > 4 && !StrCompareInsensitive(b + lb - 4, ".PNG"))
			lb -= 4;
		if (la != lb)
			return FALSE;
		for (DWORD i = 0; i < la; ++i)
		{
			CHAR x = a[i], y = b[i];
			if (x >= 'a' && x <= 'z')
				x -= 32;
			if (y >= 'a' && y <= 'z')
				y -= 32;
			if (x != y)
				return FALSE;
		}
		return TRUE;
	}

	const CHAR* const originNames[] = { "canvas", "centre", "sprite" };

	// the full piece list, so a misplacement can be read from the log: sheet rect -> pack frame, its part's rect in the frame canvas, the frame box placed in the sheet
	VOID SheetPieceLog(const CHAR* label, DWORD fileId, LONG w, LONG h, DWORD count, const Twin::SeedSheetStats* stats, const CHAR* why)
	{
		DebugLog("image sheet pieces %.40s file %u %dx%d (%s): frames %u, pieces %u, dropped as uncertain %u (block of another size %u, frames whose pack entry reaches outside their parts %u, frames whose zeroed offset could not be derived %u) | frames whose pack entry is named like the file %u, seeded in canvas layout | pack %s",
			label, fileId, w, h, why, sheetFrameCount, count, stats->uncertain, stats->wrongSize, stats->notFrame, stats->offsetLost, stats->fileNamed, pack.gameNames ? "gamenames" : "assets");
		for (DWORD f = 0; f < sheetFrameCount && f < 12; ++f)
		{
			const SheetFrameInfo* i = &sheetFrames[f];
			const PackFrame* pf = i->frame >= 0 ? &pack.frames[i->frame] : NULL;
			CHAR derivedText[80] = "";
			if (i->derived == DerivePlaced)
				StrPrint(derivedText, " | ZEROED OFFSET DERIVED: placed at %d,%d", i->ox, i->oy);
			else if (i->derived == DeriveFullCanvas)
				StrCopy(derivedText, " | full-canvas entry, 0,0 is its place");
			else if (i->derived == DeriveUnplaced)
				StrCopy(derivedText, " | ZEROED OFFSET NOT DERIVABLE: left 1x");
			DebugLog("image sheet pieces %.40s frame [%u] %.48s canvas %dx%d, %u parts %s %d,%d %dx%d of it -> pack %.48s%s #%d %dx%d offset %d,%d origin %s%s%s%s", label, f, i->name, i->cw, i->ch,
				i->parts, i->composite ? "with the file's other frames (event portrait composite) covering" : "covering", i->bounds.left, i->bounds.top, i->bounds.right - i->bounds.left, i->bounds.bottom - i->bounds.top,
				i->anim ? pack.names + i->anim->name : "-", i->exact ? "" : " (name variant)", i->frame, pf ? pf->w : 0, pf ? pf->h : 0, pf ? pf->ox : 0, pf ? pf->oy : 0,
				originNames[i->origin < 3 ? i->origin : 0], derivedText, i->outsideParts ? " | ENTRY OUTSIDE THE FRAME'S PARTS: not this frame, left 1x" : "", !i->fileNamed ? "" : i->readingsDiffer ? " | named like the file, canvas reading (an identity reading would differ)" : " | named like the file, both readings agree");
		}
		CHAR text[1100];
		CHAR* at = text;
		DWORD lines = 0;
		for (DWORD k = 0; k < count && lines < 16; ++k)
		{
			const Twin::SeedPiece* p = &sheetPieces[k];
			at += StrPrint(at, " %d,%d %dx%d->[%u]#%d canvas %d,%d place %d,%d%s;", p->to.left, p->to.top, p->to.right - p->to.left, p->to.bottom - p->to.top, sheetFrameOf[k], p->frame,
				sheetCanvas[k].x, sheetCanvas[k].y, p->place.left, p->place.top, sheetDrop[k] ? " DROPPED" : "");
			if (at - text > 700 || k + 1 == count)
			{
				DebugLog("image sheet pieces %.40s (sheet rect -> [frame] #pack frame, part's canvas pos, frame box in the sheet, not flipped):%s", label, text);
				at = text;
				*at = 0;
				++lines;
			}
		}
	}

	// the packer's frames sit in deduped blocks of one sheet; each block gets the 4K of the first frame that uses it, at that frame's canvas position
	// FALSE when no -INDEX.OPT entry points at the file; the pieces are in the static tables until the next call. Only pieces placed with certainty are kept
	BOOL SheetPieces(Archive* a, DWORD fileId, const CHAR* label, LONG w, LONG h, BOOL log, DWORD* pieceCount, DWORD* conflictOut, Twin::SeedSheetStats* statsOut, DWORD* blockOut)
	{
		Twin::SeedStep step(Twin::StepPieces);
		DWORD offsets[SHEET_BLOCKS];
		DWORD blocks = 0;
		for (DWORD value = a->itemHeads[(fileId * 2654435761u) & a->itemMask]; value; value = a->items[value - 1].next)
		{
			const SheetItem* item = &a->items[value - 1];
			if (item->id != fileId)
				continue;
			BOOL seen = FALSE;
			for (DWORD k = 0; k < blocks && !seen; ++k)
				seen = offsets[k] == item->offset;
			if (!seen && blocks < SHEET_BLOCKS)
				offsets[blocks++] = item->offset;
		}
		if (!blocks)
			return FALSE;

		Twin::SeedPiece* pieces = sheetPieces;
		Twin::SeedConflict* conflicts = sheetConflicts;
		BYTE* shared = sheetShared;
		const DWORD pieceLimit = sizeof(sheetPieces) / sizeof(*sheetPieces), conflictLimit = sizeof(sheetConflicts) / sizeof(*sheetConflicts);
		DWORD count = 0, conflictCount = 0;
		Twin::SeedSheetStats stats;
		MemoryZero(&stats, sizeof(stats));
		const BYTE* end = a->images + a->imagesSize;
		sheetFrameCount = 0;
		DWORD fileNamedDiffer = 0;
		// event portraits, faces and every sheet with a multi-part frame of the decode's size get their full piece list, beyond the first 20 sheets
		DWORD pathLength = StrLength(a->path);
		BOOL events = pathLength >= 10 && !StrCompareInsensitive(a->path + pathLength - 10, "\\Events.ff");
		BOOL forceLog = events || (pathLength >= 9 && !StrCompareInsensitive(a->path + pathLength - 9, "\\Faces.ff"));

		// an event portrait's entry is Gallean's composite of every frame of the file (GamePortraitSource.GetUnitEventPortrait, GameImages.TryGetCompositeSprite):
		// the frames laid on one canvas as wide and tall as the largest frame's. Its place is read against the union of all those frames' parts
		RECT fileBounds = { 0, 0, 0, 0 };
		LONG fileCanvasW = 0, fileCanvasH = 0;
		if (events && pack.gameNames)
		{
			for (DWORD b = 0; b < blocks; ++b)
			{
				if (offsets[b] > a->imagesSize || a->imagesSize - offsets[b] < 1 + 2 + 4 + 4 + 1024 + 4)
					continue;
				LONG sizeX = *(const LONG*)(a->images + offsets[b] + 3), sizeY = *(const LONG*)(a->images + offsets[b] + 7);
				if ((sizeX || sizeY) && (sizeX != w || sizeY != h))
					continue;
				const BYTE* p = a->images + offsets[b] + 1 + 2 + 4 + 4 + 1024;
				DWORD frames = *(const DWORD*)p;
				p += 4;
				for (DWORD f = 0; f < frames; ++f)
				{
					const BYTE* next;
					if (!TextAt(p, end, &next) || next + 12 > end)
						break;
					DWORD parts = *(const DWORD*)next;
					fileCanvasW = max(fileCanvasW, *(const LONG*)(next + 4));
					fileCanvasH = max(fileCanvasH, *(const LONG*)(next + 8));
					p = next + 12;
					if (parts > (DWORD)(end - p) / 24)
						break;
					const LONG* part = (const LONG*)p;
					p += parts * 24;
					for (DWORD i = 0; i < parts; ++i, part += 6)
					{
						RECT r = { part[0], part[1], part[0] + part[4], part[1] + part[5] };
						if (r.right <= r.left || r.bottom <= r.top)
							continue;
						if (IsRectEmpty(&fileBounds))
							fileBounds = r;
						else
							UnionRect(&fileBounds, &fileBounds, &r);
					}
				}
			}
		}

		for (DWORD b = 0; b < blocks; ++b)
		{
			if (offsets[b] > a->imagesSize || a->imagesSize - offsets[b] < 1 + 2 + 4 + 4 + 1024 + 4)
				continue;
			// Gallean ties a block to the file whose PNG has the block's size; a block of another size describes another sheet
			LONG sizeX = *(const LONG*)(a->images + offsets[b] + 3), sizeY = *(const LONG*)(a->images + offsets[b] + 7);
			if ((sizeX || sizeY) && (sizeX != w || sizeY != h))
			{
				++stats.wrongSize;
				++sheetCheck.wrongSize;
				continue;
			}
			const BYTE* p = a->images + offsets[b] + 1 + 2 + 4 + 4 + 1024;
			DWORD frames = *(const DWORD*)p;
			p += 4;
			for (DWORD f = 0; f < frames; ++f)
			{
				const BYTE* next;
				const CHAR* name = TextAt(p, end, &next);
				if (!name || next + 12 > end)
					break;
				DWORD parts = *(const DWORD*)next;
				LONG cw = *(const LONG*)(next + 4), ch = *(const LONG*)(next + 8);
				p = next + 12;
				if (parts > (DWORD)(end - p) / 24)
					break;
				const LONG* part = (const LONG*)p;
				p += parts * 24;
				++stats.frames;

				const SheetLink* link = FindLink(a, name);
				Origin origin = OriginCanvas;
				const PackAnim* anim;
				BOOL exact = FALSE;
				LONG frame = PackFrameFor(link ? link->anim : name, link ? link->index : 0, cw, ch, &origin, &anim, &exact);
				if (frame < 0)
				{
					++stats.unmapped;
					continue;
				}
				const PackFrame* pf = &pack.frames[frame];
				if (pf->additive)
				{
					++stats.additive;
					continue;
				}

				LONG ox = pf->ox, oy = pf->oy;
				if (origin == OriginCentre)
				{
					ox += cw / 2;
					oy += ch / 2;
				}
				else if (origin == OriginSprite)
					ox = oy = 0;

				// an entry named like the sheet file still holds the frame, not the file: Gallean's GetImage takes the -IMAGES.OPT frame of a name before the raw file of that name,
				// and builds it from the parts in canvas coordinates (ImagesExtractor.GetImage, BuildImage, GetImageBounds). Every such entry in the 19:07 log has exactly the parts'
				// canvas bounds as its box (G000MG0116 64x54 at 124,123 of 320x320), which a raw file cannot have. So these frames are placed like any other; the log only marks them,
				// and whether an identity reading (the entry over the file at 0,0, as PlainFrame reads a plain file) would have placed any part differently
				BOOL fileNamed = !exact || (label && SameStem(pack.names + anim->name, label));

				RECT bounds = { 0, 0, 0, 0 };
				for (DWORD i = 0; i < parts; ++i)
				{
					RECT r = { part[i * 6], part[i * 6 + 1], part[i * 6] + part[i * 6 + 4], part[i * 6 + 1] + part[i * 6 + 5] };
					if (r.right <= r.left || r.bottom <= r.top)
						continue;
					if (IsRectEmpty(&bounds))
						bounds = r;
					else
						UnionRect(&bounds, &bounds, &r);
				}

				// the repack zeroed the offsets of SPRITE-origin entries; where 0,0 cannot be the entry's place, it is taken back from the parts (see DeriveOffset).
				// An Events.ff entry named like the file is the composite of every frame of the file, so it is read against all of them
				BOOL composite = events && fileNamed && !IsRectEmpty(&fileBounds);
				const RECT* checkBounds = composite ? &fileBounds : &bounds;
				BYTE derivedResult = DeriveNone;
				if (pack.gameNames && !link && origin == OriginCanvas)
				{
					LONG dx = 0, dy = 0;
					derivedResult = DeriveOffset(a, fileId, label, w, h, offsets, blocks, frame, pack.names + anim->name, name, composite, checkBounds,
						composite ? fileCanvasW : cw, composite ? fileCanvasH : ch, &dx, &dy);
					if (derivedResult == DerivePlaced)
					{
						ox = dx;
						oy = dy;
					}
				}
				BOOL offsetLost = derivedResult == DeriveUnplaced;

				BOOL differ = FALSE;
				for (DWORD i = 0; i < parts && fileNamed && !differ; ++i)
					differ = part[i * 6 + 2] - part[i * 6] + ox != 0 || part[i * 6 + 3] - part[i * 6 + 1] + oy != 0;

				// the entry is the frame's content box in its canvas (Gallean builds the frame from its parts and the pack crops it to what is not transparent), so it lies
				// inside the union of the parts' canvas rects. One that reaches outside is another image of that name (another archive, or a raw file): its pieces would
				// sample the wrong 4K, so the frame stays 1x. Geometry only, no pixels. Only for stills looked up by their own name in the gamenames pack: an animation frame's
				// entry may be cropped to the whole animation's bounds, which no log has shown yet, and an asset-pack entry is the full canvas.
				// A full-canvas entry at 0,0 (Gallean's full-canvas sprite or composite) holds the parts too, and an event portrait's composite covers every frame of the file
				BOOL outsideParts = pack.gameNames && !link && origin != OriginSprite && !IsRectEmpty(checkBounds) && derivedResult != DeriveFullCanvas && !offsetLost &&
					(ox < checkBounds->left || oy < checkBounds->top || ox + pf->w > checkBounds->right || oy + pf->h > checkBounds->bottom);
				if (outsideParts)
				{
					++stats.notFrame;
					++sheetCheck.notFrame;
				}
				if (offsetLost)
				{
					++stats.offsetLost;
					++sheetCheck.offsetLost;
				}
				if (parts > 1 && cw == w && ch == h)
					forceLog = TRUE;
				WORD frameOf = (WORD)min(sheetFrameCount, (DWORD)SHEET_FRAME_INFO - 1);
				if (sheetFrameCount < SHEET_FRAME_INFO)
				{
					SheetFrameInfo* info = &sheetFrames[sheetFrameCount++];
					info->name = name;
					info->anim = anim;
					info->frame = frame;
					info->cw = cw;
					info->ch = ch;
					info->origin = (BYTE)origin;
					info->exact = (BYTE)exact;
					info->fileNamed = (BYTE)fileNamed;
					info->readingsDiffer = (BYTE)differ;
					info->outsideParts = (BYTE)outsideParts;
					info->derived = derivedResult;
					info->composite = (BYTE)composite;
					info->parts = (WORD)min(parts, 0xFFFFu);
					info->bounds = *checkBounds;
					info->ox = ox;
					info->oy = oy;
				}
				if (fileNamed)
				{
					++stats.fileNamed;
					++sheetCheck.fileNamed;
					if (differ)
					{
						++fileNamedDiffer;
						++sheetCheck.fileNamedDiffer;
					}
				}

				for (DWORD i = 0; i < parts; ++i, part += 6)
				{
					LONG cx = part[0], cy = part[1], sx = part[2], sy = part[3], pw = part[4], ph = part[5];
					if (pw <= 0 || ph <= 0)
						continue;
					if (sx < 0 || sy < 0 || sx + pw > w || sy + ph > h)
					{
						++stats.outside;
						continue;
					}

					RECT to = { sx, sy, sx + pw, sy + ph };
					RECT place = { sx - cx + ox, sy - cy + oy, sx - cx + ox + pf->w, sy - cy + oy + pf->h };
					DWORD k = 0;
					while (k < count && !EqualRect(&pieces[k].to, &to))
						++k;
					if (k < count)
					{
						if (pieces[k].frame == frame)
						{
							// one block drawn at two places of the same frame can carry the 4K of only one of them
							if (!EqualRect(&pieces[k].place, &place) && !sheetDrop[k])
							{
								sheetDrop[k] = TRUE;
								++sheetCheck.duplicates;
							}
							continue;
						}
						if (!shared[k])
						{
							shared[k] = TRUE;
							++stats.shared;
						}
						if (conflictCount < conflictLimit)
						{
							Twin::SeedConflict* c = &conflicts[conflictCount++];
							c->to = to;
							c->placeA = pieces[k].place;
							c->placeB = place;
							c->frameA = pieces[k].frame;
							c->frameB = frame;
						}
						continue;
					}
					if (count >= pieceLimit)
						continue;
					pieces[count].to = to;
					pieces[count].place = place;
					pieces[count].frame = frame;
					sheetCanvas[count].x = cx;
					sheetCanvas[count].y = cy;
					sheetFrameOf[count] = frameOf;
					sheetDrop[count] = (BYTE)(outsideParts || offsetLost);
					shared[count++] = FALSE;
				}
			}
		}

		// blocks that overlap without being the same block: the overlap holds one set of 1x pixels, so both must take the same 4K there (same frame, same frame box).
		// Across frames the first keeps it, as a shared block does; within one frame neither can be trusted
		if (count <= SHEET_OVERLAP_LIMIT)
		{
			for (DWORD i = 0; i < count; ++i)
				for (DWORD k = i + 1; k < count; ++k)
				{
					RECT part;
					if (!IntersectRect(&part, &pieces[i].to, &pieces[k].to))
						continue;
					if (pieces[i].frame == pieces[k].frame && EqualRect(&pieces[i].place, &pieces[k].place))
						continue;
					if (!sheetDrop[k])
						++sheetCheck.overlaps;
					sheetDrop[k] = TRUE;
					if (pieces[i].frame == pieces[k].frame)
						sheetDrop[i] = TRUE;
				}
		}
		else
			++sheetCheck.unchecked;

		for (DWORD i = 0; i < count; ++i)
			stats.uncertain += sheetDrop[i];
		sheetCheck.dropped += stats.uncertain;

		// logged before the uncertain pieces are taken out, so the next round sees where they would have gone
		if (log && sheetCheck.pieceLogs < SHEET_PIECE_LOGS)
		{
			++sheetCheck.pieceLogs;
			SheetPieceLog(label, fileId, w, h, count, &stats, "sheet seed");
		}
		else if (forceLog && sheetCheck.forcedLogs < SHEET_FORCED_LOGS)
		{
			++sheetCheck.forcedLogs;
			SheetPieceLog(label, fileId, w, h, count, &stats, "event portrait, face or multi-part frame of the sheet's size");
		}
		else if ((stats.uncertain || stats.wrongSize || fileNamedDiffer) && sheetCheck.uncertainLogs < SHEET_UNCERTAIN_LOGS)
		{
			// a file-named frame whose readings differ is listed too, so its placement can be read from the log
			++sheetCheck.uncertainLogs;
			SheetPieceLog(label, fileId, w, h, count, &stats, stats.uncertain || stats.wrongSize ? "uncertain" : "named like the file");
		}

		DWORD kept = 0;
		for (DWORD i = 0; i < count; ++i)
		{
			if (sheetDrop[i])
				continue;
			pieces[kept] = pieces[i];
			shared[kept] = shared[i];
			sheetCanvas[kept] = sheetCanvas[i];
			sheetFrameOf[kept] = sheetFrameOf[i];
			++kept;
		}

		*pieceCount = kept;
		*conflictOut = conflictCount;
		*statsOut = stats;
		*blockOut = blocks;
		return TRUE;
	}

	VOID SheetLog(const CHAR* label, DWORD fileId, LONG w, LONG h, DWORD blocks, const Twin::SeedSheetStats* stats, DWORD count, DWORD conflictCount)
	{
		// sheets that seed nothing are logged further, so the next round knows whether they are unmapped or additive
		if (sheets.logs >= SHEET_LOGS && (count || sheets.emptyLogs >= SHEET_LOGS * 2))
			return;
		if (sheets.logs < SHEET_LOGS)
			++sheets.logs;
		else
			++sheets.emptyLogs;
		DebugLog("image seed sheet %s file %u %dx%d: blocks %u frames %u (unmapped %u, additive %u) pieces %u shared blocks %u conflicts %u parts outside %u", label, fileId, w, h,
			blocks, stats->frames, stats->unmapped, stats->additive, count, stats->shared, conflictCount, stats->outside);
	}

	const CHAR* SheetReason(const Twin::SeedSheetStats* stats)
	{
		if (!stats->frames)
			return "the sheet's blocks list no frame";
		if (stats->additive == stats->frames)
			return "every frame is additive (aura or glow), which is not seeded";
		if (stats->unmapped == stats->frames)
			return "no frame name is in the pack";
		if (stats->unmapped + stats->additive == stats->frames)
			return "its frames are additive or not in the pack";
		if (stats->notFrame)
			return "its pack entry reaches outside the frame's parts, so it is another image of that name: left 1x";
		if (stats->offsetLost)
			return "its pack entry's offset was zeroed by the repack and neither the parts nor the 1x key place it: left 1x";
		if (stats->wrongSize)
			return "its -IMAGES.OPT block is of another sheet size";
		if (stats->uncertain)
			return "its blocks overlap with other places in the frame: left 1x";
		return "every part falls outside the sheet";
	}

	BOOL SeedSheet(StoreImage* s, Archive* a, const BYTE* first, LONG pitch, BOOL* seeded, BOOL memoryTwin)
	{
		*seeded = FALSE;
		LONG w = (LONG)s->dims[0], h = (LONG)s->dims[1];
		DWORD count, conflictCount, blocks;
		Twin::SeedSheetStats stats;
		if (!SheetPieces(a, s->file->id, s->file->name, w, h, TRUE, &count, &conflictCount, &stats, &blocks))
			return FALSE;

		const CHAR* label = s->file->name;
		SheetLog(label, s->file->id, w, h, blocks, &stats, count, conflictCount);
		Twin::SeedArt(first, pitch, w, h, Twin::SeedSheet, label, count ? sheetPieces : NULL, count, sheetConflicts, conflictCount, &stats, memoryTwin);
		*seeded = count != 0;
		return TRUE;
	}

	// asked for when the store loads the file, so the 4K is usually decoded before the game decodes and transfers the 1x
	DWORD PrefetchSheet(ArchiveFile* file)
	{
		Archive* a = file ? FileArchive(file) : NULL;
		if (!a || !pack.ready)
			return 0;
		if (!a->sheetsLoaded)
			LoadSheets(a);
		if (!a->itemCount)
			return 0;

		DWORD offsets[SHEET_BLOCKS];
		DWORD blocks = 0;
		DWORD fileId = file->id;
		for (DWORD value = a->itemHeads[(fileId * 2654435761u) & a->itemMask]; value; value = a->items[value - 1].next)
		{
			const SheetItem* item = &a->items[value - 1];
			if (item->id != fileId)
				continue;
			BOOL seen = FALSE;
			for (DWORD k = 0; k < blocks && !seen; ++k)
				seen = offsets[k] == item->offset;
			if (!seen && blocks < SHEET_BLOCKS)
				offsets[blocks++] = item->offset;
		}

		DWORD asked = 0;
		const BYTE* end = a->images + a->imagesSize;
		for (DWORD b = 0; b < blocks; ++b)
		{
			if (offsets[b] > a->imagesSize || a->imagesSize - offsets[b] < 1 + 2 + 4 + 4 + 1024 + 4)
				continue;
			const BYTE* p = a->images + offsets[b] + 1 + 2 + 4 + 4 + 1024;
			DWORD frames = *(const DWORD*)p;
			p += 4;
			for (DWORD f = 0; f < frames; ++f)
			{
				const BYTE* next;
				const CHAR* name = TextAt(p, end, &next);
				if (!name || next + 12 > end)
					break;
				DWORD parts = *(const DWORD*)next;
				LONG cw = *(const LONG*)(next + 4), ch = *(const LONG*)(next + 8);
				p = next + 12;
				if (parts > (DWORD)(end - p) / 24)
					break;
				p += parts * 24;

				const SheetLink* link = FindLink(a, name);
				Origin origin = OriginCanvas;
				const PackAnim* anim;
				LONG frame = PackFrameFor(link ? link->anim : name, link ? link->index : 0, cw, ch, &origin, &anim);
				if (frame >= 0 && !pack.frames[frame].additive && ArtState(frame) < 2)
					++asked;
			}
		}
		return asked;
	}

	VOID LogRecords();

	VOID LogSeedSources()
	{
		DebugLog("image seed sources since start: store draws %d (new memory %d) | faces loaded %d, face copies %d | ranges %d, decodes with art store %d face %d | archives with sheet tables %u | ground chunks %d builds %d published %d | prefetch at store load: files %u, frames asked %u",
			storeStats.draws, storeStats.drawLinks, storeStats.faceLoads, storeStats.faceCopies, memory.count, memory.learnStore, memory.learnFace, sheets.archives,
			ground.chunkCount, ground.builds, ground.published, storeStats.prefetchLoads, storeStats.prefetchFrames);
		DebugLog("image sheet checks since start: pieces left 1x as uncertain %u | blocks of another sheet size %u, frames whose pack entry is named like the sheet file %u (an identity reading would differ for %u; all seeded in canvas layout), frames whose pack entry reaches outside their parts (another image of the name) %u, overlapping pieces with different 4K %u, one block at two places of a frame %u, sheets too big to check %u",
			sheetCheck.dropped, sheetCheck.wrongSize, sheetCheck.fileNamed, sheetCheck.fileNamedDiffer, sheetCheck.notFrame, sheetCheck.overlaps, sheetCheck.duplicates, sheetCheck.unchecked);
		DebugLog("image pack offsets zeroed by the repack, since start: entries placed at their parts union %u, at the 1x key's non-transparent box %u, full-canvas entries kept at 0,0 %u, left 1x %u (frames seeded without them %u), decodes without 8-bit indices at hand %u",
			derived.placedUnion, derived.placedKey, derived.fullCanvas, derived.unplaced, sheetCheck.offsetLost, derived.noKey);
		LogRecords();
	}

	BOOL SeedStore(StoreImage* s, const BYTE* first, LONG pitch, BOOL memoryTwin)
	{
		Archive* a = s->file ? FileArchive(s->file) : NULL;
		if (a && !a->sheetsLoaded)
			LoadSheets(a);
		BOOL seeded;
		if (a && a->itemCount && pack.ready && SeedSheet(s, a, first, pitch, &seeded, memoryTwin))
			return seeded;

		LONG w = (LONG)s->dims[0], h = (LONG)s->dims[1];
		const CHAR* label = s->entry && s->entry->name[0] ? s->entry->name : s->file ? s->file->name : "?";
		Twin::SeedKind kind = IsFace(s) ? Twin::SeedFace : Twin::SeedStore;
		LONG frame = StoreFrame(s);
		if (frame < 0 || pack.frames[frame].additive)
		{
			Twin::SeedArt(first, pitch, w, h, kind, label, NULL, 0, NULL, 0, NULL, memoryTwin);
			return FALSE;
		}

		Twin::SeedPiece piece;
		SetRect(&piece.to, 0, 0, w, h);
		piece.place = piece.to;
		piece.frame = frame;
		Twin::SeedArt(first, pitch, w, h, kind, label, &piece, 1, NULL, 0, NULL, memoryTwin);
		return TRUE;
	}

	BOOL SheetFile(const Archive* a, DWORD id)
	{
		if (!a->itemCount)
			return FALSE;
		for (DWORD value = a->itemHeads[(id * 2654435761u) & a->itemMask]; value; value = a->items[value - 1].next)
			if (a->items[value - 1].id == id)
				return TRUE;
		return FALSE;
	}

	// a plain image (no -INDEX.OPT entry points at it, even in an archive that has sheets) is one pack frame of its own size
	LONG PlainFrame(const ArchiveFile* file, LONG w, LONG h, const CHAR** reason)
	{
		Origin origin = OriginCanvas;
		const PackAnim* anim;
		LONG frame = pack.ready ? PackFrameFor(file->name, 0, w, h, &origin, &anim) : -1;
		if (frame < 0)
		{
			*reason = pack.ready ? "its name is not in the pack" : "the pack is not loaded";
			return -1;
		}
		if (pack.frames[frame].additive)
		{
			*reason = "its pack frame is additive, which is not seeded";
			return -1;
		}
		if (w && h && (pack.frames[frame].w != w || pack.frames[frame].h != h))
		{
			*reason = "its pack frame has another size";
			return -1;
		}
		return frame;
	}

	VOID PrefetchPlain(ArchiveFile* file)
	{
		Archive* a = FileArchive(file);
		if (a && !a->sheetsLoaded)
			LoadSheets(a);
		if (a && SheetFile(a, file->id))
			return;
		const CHAR* reason;
		LONG frame = PlainFrame(file, 0, 0, &reason);
		if (frame >= 0)
			ArtState(frame);
	}

	// the game keeps one handle open per archive and StoreDraw opens the .ff again for every record it reads; 48 slots filled up with the long-lived ones
	#define RECORD_FILES 1024
	#define RECORD_READS 64
	#define RECORD_FRAMES 600
	#define RECORD_LOGS 48
	#define RECORD_CALLERS 16
	#define RECORD_HEAD 28

	struct RecordRead {
		Archive* archive;
		ArchiveFile* file;
		LONG w, h;
		LONG frameNo;
		DWORD caller;
		DWORD outer;
		BOOL consumed;
	};

	// the game reads every .ff record through ReadFile; the record header in front of the data names the file, its PNG header the size
	struct {
		BOOL ready;
		CRITICAL_SECTION cs;
		struct {
			HANDLE handle;
			Archive* archive;
		} files[RECORD_FILES];
		RecordRead list[RECORD_READS];
		DWORD head;
		DWORD gameThread;
		DWORD opens, untracked, tableFull, noTable, live, liveMax, top, reads, records, headers, inside, pngs, sheets, plains, noSize, otherThread, named, takenByStore, unmatched, logs;
		DWORD callers[RECORD_CALLERS];
		DWORD callerCount, callersDumped;
	} records;

	VOID CodeText(DWORD address, CHAR* text)
	{
		if (!address)
		{
			StrCopy(text, "-");
			return;
		}
		CHAR name[24];
		DWORD base = CrashModule(address, name, sizeof(name));
		StrPrint(text, "%s+%X", name, address - base);
	}

	BOOL EndsWith(const CHAR* text, const CHAR* tail)
	{
		DWORD a = StrLength(text), b = StrLength(tail);
		return a >= b && !StrCompareInsensitive(text + a - b, tail);
	}

	VOID OnFileOpen(const CHAR* path, HANDLE handle)
	{
		if (!records.ready || !path || handle == INVALID_HANDLE_VALUE || !EndsWith(path, ".ff") || !Twin::SeedMode() || !Twin::Active())
			return;

		Archive* a = FindArchive(path);
		EnterCriticalSection(&records.cs);
		++records.opens;
		if (a && !a->reader)
		{
			a->reader = CreateFile(a->path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (a->reader == INVALID_HANDLE_VALUE)
				a->reader = NULL;
		}
		DWORD slot = RECORD_FILES;
		for (DWORD i = 0; i < RECORD_FILES; ++i)
			if (records.files[i].handle == handle || (slot == RECORD_FILES && !records.files[i].handle))
				slot = i;
		if (a && a->files && slot < RECORD_FILES)
		{
			if (records.files[slot].handle != handle && ++records.live > records.liveMax)
				records.liveMax = records.live;
			records.files[slot].handle = handle;
			records.files[slot].archive = a;
			if (slot + 1 > records.top)
				records.top = slot + 1;
		}
		else
		{
			++records.untracked;
			if (slot == RECORD_FILES)
				++records.tableFull;
			else
				++records.noTable;
		}
		LeaveCriticalSection(&records.cs);
	}

	// read without the lock: every ReadFile of the game asks, and a handle is only ever added before the game reads it
	BOOL RecordFile(HANDLE handle)
	{
		if (!records.ready || !handle)
			return FALSE;
		// every ReadFile of the game asks; only the slots ever used are searched
		for (DWORD i = 0; i < records.top; ++i)
			if (records.files[i].handle == handle)
				return TRUE;
		return FALSE;
	}

	VOID OnFileClose(HANDLE handle)
	{
		if (!records.ready || !handle || !records.live)
			return;
		EnterCriticalSection(&records.cs);
		for (DWORD i = 0; i < records.top; ++i)
			if (records.files[i].handle == handle)
			{
				records.files[i].handle = NULL;
				--records.live;
			}
		LeaveCriticalSection(&records.cs);
	}

	DWORD BigEndian(const BYTE* p)
	{
		return ((DWORD)p[0] << 24) | ((DWORD)p[1] << 16) | ((DWORD)p[2] << 8) | p[3];
	}

	VOID OnFileRead(HANDLE handle, LONGLONG position, const VOID* buffer, DWORD size, DWORD caller, DWORD outer)
	{
		if (!records.ready || !buffer || !size)
			return;

		EnterCriticalSection(&records.cs);
		Archive* a = NULL;
		for (DWORD i = 0; i < records.top && !a; ++i)
			if (records.files[i].handle == handle)
				a = records.files[i].archive;
		if (!a)
		{
			LeaveCriticalSection(&records.cs);
			return;
		}

		++records.reads;
		const BYTE* data = (const BYTE*)buffer;
		BYTE head[RECORD_HEAD];
		DWORD id = 0, inBuffer = 0;
		LONGLONG start = position;
		if (size >= 4 && !MemoryCompare(data, "MQRC", 4))
		{
			// the header alone: the data read that follows is the record read
			if (size <= RECORD_HEAD)
			{
				++records.headers;
				LeaveCriticalSection(&records.cs);
				return;
			}
			id = *(const DWORD*)(data + 8);
			start = position + RECORD_HEAD;
			data += RECORD_HEAD;
			inBuffer = size - RECORD_HEAD;
		}
		else if (a->reader && position >= RECORD_HEAD && ReadAt(a->reader, position - RECORD_HEAD, head, sizeof(head)) && !MemoryCompare(head, "MQRC", 4) &&
			*(const DWORD*)(head + 12) >= size)
		{
			id = *(const DWORD*)(head + 8);
			inBuffer = size;
		}
		ArchiveFile* file = id ? ArchiveRecord(a, id) : NULL;
		if (!file)
		{
			++records.inside;
			LeaveCriticalSection(&records.cs);
			return;
		}
		++records.records;

		LONG w = 0, h = 0;
		BOOL png = EndsWith(file->name, ".PNG");
		if (png)
		{
			BYTE ihdr[24];
			const BYTE* p = NULL;
			if (inBuffer >= sizeof(ihdr))
				p = data;
			else if (a->reader && ReadAt(a->reader, start, ihdr, sizeof(ihdr)))
				p = ihdr;
			if (p && p[0] == 0x89 && p[1] == 'P' && !MemoryCompare(p + 12, "IHDR", 4))
			{
				w = (LONG)BigEndian(p + 16);
				h = (LONG)BigEndian(p + 20);
				++records.pngs;
			}
			else
				++records.noSize;
		}
		BOOL onGame = records.gameThread == GetCurrentThreadId();
		if (!onGame)
			++records.otherThread;

		if (w > 0 && h > 0)
		{
			RecordRead* r = &records.list[records.head++ % RECORD_READS];
			r->archive = a;
			r->file = file;
			r->w = w;
			r->h = h;
			r->frameNo = game.frameNo;
			r->caller = caller;
			r->outer = outer;
			r->consumed = FALSE;
		}

		DWORD k = 0;
		while (k < records.callerCount && records.callers[k] != caller)
			++k;
		if (k == records.callerCount && k < RECORD_CALLERS && caller)
			records.callers[records.callerCount++] = caller;

		BOOL log = records.logs < RECORD_LOGS;
		if (log)
			++records.logs;
		LeaveCriticalSection(&records.cs);

		if (log)
		{
			CHAR from[48], above[48];
			CodeText(caller, from);
			CodeText(outer, above);
			const CHAR* archive = StrLastChar(a->path, '\\');
			DebugLog("image record read #%u: %s record %u %.40s %dx%d at %u, %u bytes%s | %s thread | caller %08X %s < %08X %s", records.logs, archive ? archive + 1 : a->path, id, file->name, w, h,
				(DWORD)start, size, inBuffer < size ? " (header in front)" : "", onGame ? "game" : "other", caller, from, outer, above);
		}

		// the 4K is asked for at the read, ahead of the decode; only the game thread touches the pack's queues
		if (onGame && png && Twin::SeedMode() && Twin::Active())
		{
			if (!a->sheetsLoaded)
				LoadSheets(a);
			if (SheetFile(a, file->id))
			{
				++records.sheets;
				PrefetchSheet(file);
			}
			else
			{
				++records.plains;
				PrefetchPlain(file);
			}
		}
	}

	// a decode by StoreDraw takes the read of its own file, so that read cannot name a later decode of the same size
	VOID RecordTaken(const ArchiveFile* file)
	{
		if (!records.ready || !file)
			return;
		EnterCriticalSection(&records.cs);
		for (DWORD k = 0; k < RECORD_READS && k < records.head; ++k)
		{
			RecordRead* r = &records.list[(records.head - 1 - k) % RECORD_READS];
			if (r->file == file && !r->consumed)
			{
				r->consumed = TRUE;
				++records.takenByStore;
				break;
			}
		}
		LeaveCriticalSection(&records.cs);
	}

	// the name and the pieces of a whole decode of the file, a sheet's (from -INDEX.OPT) or the plain image's own pack frame; the pieces stay valid until the next call
	VOID NamePieces(Archive* a, const ArchiveFile* file, LONG width, LONG height, RecordName* out, BOOL log, BOOL withPieces)
	{
		DWORD length = min((DWORD)StrLength(file->name), (DWORD)sizeof(out->name) - 1);
		MemoryCopy(out->name, file->name, length);
		out->name[length] = 0;
		const CHAR* tail = StrLastChar(a->path, '\\');
		tail = tail ? tail + 1 : a->path;
		length = min((DWORD)StrLength(tail), (DWORD)sizeof(out->archive) - 1);
		MemoryCopy(out->archive, tail, length);
		out->archive[length] = 0;
		out->record = file->id;
		out->width = width;
		out->height = height;

		if (!pack.ready)
		{
			out->reason = "the pack is not loaded";
			return;
		}
		if (!a->sheetsLoaded)
			LoadSheets(a);
		if (!withPieces)
		{
			out->sheet = SheetFile(a, file->id);
			return;
		}
		DWORD count = 0, conflictCount = 0, blocks = 0;
		Twin::SeedSheetStats stats;
		if (a->itemCount && SheetPieces(a, file->id, file->name, width, height, log, &count, &conflictCount, &stats, &blocks))
		{
			out->sheet = TRUE;
			if (log)
				SheetLog(file->name, file->id, width, height, blocks, &stats, count, conflictCount);
			out->pieces = count ? sheetPieces : NULL;
			out->count = count;
			out->conflicts = sheetConflicts;
			out->conflictCount = conflictCount;
			if (!count)
				out->reason = SheetReason(&stats);
			return;
		}

		static Twin::SeedPiece piece;
		LONG frame = PlainFrame(file, width, height, &out->reason);
		if (frame < 0)
			return;
		SetRect(&piece.to, 0, 0, width, height);
		piece.place = piece.to;
		piece.frame = frame;
		out->pieces = &piece;
		out->count = 1;
	}

	BOOL RecordDecode(LONG width, LONG height, RecordName* out)
	{
		MemoryZero(out, sizeof(RecordName));
		if (!records.ready || width <= 0 || height <= 0)
			return FALSE;

		EnterCriticalSection(&records.cs);
		RecordRead found;
		BOOL any = FALSE;
		for (DWORD k = 0; k < RECORD_READS && k < records.head; ++k)
		{
			RecordRead* r = &records.list[(records.head - 1 - k) % RECORD_READS];
			if (r->consumed || r->w != width || r->h != height || game.frameNo - r->frameNo > RECORD_FRAMES)
				continue;
			++out->candidates;
			if (!any)
			{
				found = *r;
				r->consumed = TRUE;
				any = TRUE;
			}
		}
		if (any)
			++records.named;
		else
			++records.unmatched;
		LeaveCriticalSection(&records.cs);
		if (!any)
			return FALSE;

		out->caller = found.caller;
		out->outer = found.outer;
		out->framesAgo = DWORD(game.frameNo - found.frameNo);
		NamePieces(found.archive, found.file, width, height, out, TRUE, TRUE);
		return TRUE;
	}

	#define LOADER_FRAMES 600

	// the latest StoreDraw decode: the game's loader for every image of the store, full-screen backgrounds included (it reads the record itself, 0x00520D81 in v2.02b)
	struct {
		Archive* archive;
		ArchiveFile* file;
		const BYTE* data;
		LONG pitchField;
		BYTE format;
		RECT rect;
		LONG frameNo;
		DWORD caller;
		DWORD seq;
		DWORD captureSeq;
		DWORD decodes, indices, matched, partial, stale;
	} loader;

	// the 8-bit indices of the latest StoreDraw decode, when it is this file's whole sheet: StoreDrawn sets the loader before the sheet seed, and LoaderDecode names
	// a blit from it. Its rows are the pitch field in bytes (format byte 0). Used only for the key's non-transparent box (KeyBox)
	BOOL KeyIndices(const Archive* a, DWORD fileId, LONG w, LONG h, const BYTE** data, LONG* pitch)
	{
		if (loader.archive != a || !loader.file || loader.file->id != fileId || loader.format || !loader.data || loader.pitchField < w || w <= 0 || h <= 0)
			return FALSE;
		if (loader.rect.right - loader.rect.left != w || loader.rect.bottom - loader.rect.top != h || game.frameNo - loader.frameNo > LOADER_FRAMES)
			return FALSE;
		const BYTE* origin = loader.data + loader.rect.top * loader.pitchField + loader.rect.left;
		if (IsBadReadPtr(origin, (h - 1) * loader.pitchField + w))
			return FALSE;
		*data = origin;
		*pitch = loader.pitchField;
		return TRUE;
	}

	BOOL LoaderDecode(const BYTE* src, LONG srcPitch, LONG width, LONG height, RecordName* out, BOOL withPieces)
	{
		MemoryZero(out, sizeof(RecordName));
		if (!loader.file || !loader.archive)
			return FALSE;
		// set even without a match: a transfer capture other than the latest decode's is from memory the latest decode may have overwritten
		out->seq = loader.seq;
		out->captureSeq = loader.captureSeq;
		if (loader.format || loader.pitchField <= 0 || srcPitch != loader.pitchField || !src || width <= 0 || height <= 0)
			return FALSE;
		const BYTE* origin = loader.data + loader.rect.top * loader.pitchField + loader.rect.left;
		if (src < origin)
			return FALSE;
		DWORD offset = DWORD(src - origin);
		LONG sy = LONG(offset / (DWORD)srcPitch), sx = LONG(offset % (DWORD)srcPitch);
		LONG w = loader.rect.right - loader.rect.left, h = loader.rect.bottom - loader.rect.top;
		if (sx + width > w || sy + height > h)
			return FALSE;
		// the scratch is reused by the next decode; one this old is not what the scratch holds any more
		if (game.frameNo - loader.frameNo > LOADER_FRAMES)
		{
			++loader.stale;
			return FALSE;
		}

		if (!withPieces)
		{
			++loader.matched;
			if (sx || sy || width != w || height != h)
				++loader.partial;
		}
		out->caller = loader.caller;
		out->framesAgo = DWORD(game.frameNo - loader.frameNo);
		out->sx = sx;
		out->sy = sy;
		NamePieces(loader.archive, loader.file, w, h, out, FALSE, withPieces);
		return TRUE;
	}

	VOID LogRecords()
	{
		if (!records.ready)
			return;
		EnterCriticalSection(&records.cs);
		DWORD callers[RECORD_CALLERS];
		DWORD count = records.callerCount;
		MemoryCopy(callers, records.callers, count * sizeof(DWORD));
		LeaveCriticalSection(&records.cs);

		CHAR text[RECORD_CALLERS * 40 + 1];
		CHAR* at = text;
		*at = 0;
		for (DWORD i = 0; i < count; ++i)
		{
			CHAR name[48];
			CodeText(callers[i], name);
			at += StrPrint(at, " %s", name);
		}
		DebugLog("image record reads since start: .ff opens %u (untracked %u: handle table full %u, archive without a file table %u; open now %u, max %u) | reads %u: records %u (PNG %u: sheets %u, plain %u, PNG header unread %u), header only %u, inside a record %u | on other threads %u | decodes named by a read %u, reads taken by StoreDraw %u, decodes no read matched %u | callers:%s",
			records.opens, records.untracked, records.tableFull, records.noTable, records.live, records.liveMax, records.reads, records.records, records.pngs, records.sheets, records.plains, records.noSize,
			records.headers, records.inside, records.otherThread, records.named, records.takenByStore, records.unmatched, *text ? text : " none");
		DebugLog("image loader decodes since start: StoreDraw %u (8-bit %u), captured for the transfer without a 32 bpp range %u (32 bpp reading unreadable %u) | indexed blits from the latest decode %u (part of it %u), older than %u frames %u",
			loader.decodes, loader.indices, memory.captureOnly, memory.rejected, loader.matched, loader.partial, LOADER_FRAMES, loader.stale);
		for (; records.callersDumped < count; ++records.callersDumped)
			Twin::CodeDump(callers[records.callersDumped], 0x400, "record read caller");
	}

	VOID* __fastcall StoreLoad(VOID* thisObj, DWORD /*edx*/, DWORD id, DWORD flag, VOID* out)
	{
		VOID* image = ((VOID*(__thiscall*)(VOID*, DWORD, DWORD, VOID*))sub_StoreLoad)(thisObj, id, flag, out);
		if (!image)
			return image;

		ArchiveFile* file = FindArchiveFile(thisObj, id);
		StoreImage* s = FindStore(image, file != NULL);
		if (s && s->file != file)
		{
			ResetStore(s, file);
			if (file && Twin::SeedMode() && Twin::Active())
			{
				++storeStats.prefetchLoads;
				storeStats.prefetchFrames += PrefetchSheet(file);
				PrefetchPlain(file);
			}
		}

		return image;
	}

	VOID StoreDrawn(VOID* thisObj, const BYTE* desc, DWORD caller);

	// desc is the game's BlitObject (data, clip rect, pitch); the store decodes into a reused scratch one, and the surface that shows the art is written from it after
	VOID __fastcall StoreDraw(VOID* thisObj, DWORD /*edx*/, const BYTE* desc)
	{
		if (!Twin::Active())
		{
			((VOID(__thiscall*)(VOID*, const BYTE*))sub_StoreDraw)(thisObj, desc);
			return;
		}
		records.gameThread = GetCurrentThreadId();
		Twin::DecodeBegin();
		((VOID(__thiscall*)(VOID*, const BYTE*))sub_StoreDraw)(thisObj, desc);
		{
			Twin::Cost cost(Twin::CostImage);
			StoreDrawn(thisObj, desc, (DWORD)_ReturnAddress());
		}
		Twin::DecodeEnd();
	}

	VOID StoreDrawn(VOID* thisObj, const BYTE* desc, DWORD caller)
	{
		StoreImage* s = FindStore(thisObj, FALSE);
		if (!s || !s->file || !desc)
			return;
		RecordTaken(s->file);
		if (!s->decodeLogged)
		{
			s->decodeLogged = TRUE;
			DWORD n = Twin::DecodeLogNumber();
			if (n)
			{
				const LONG* r = (const LONG*)(desc + 8);
				Archive* a = FileArchive(s->file);
				const CHAR* archive = a ? StrLastChar(a->path, '\\') : NULL;
				CHAR from[48];
				CodeText(caller, from);
				DebugLog("twin decode named #%u: %.40s %dx%d record %u (%s) | loader %08X %s | found by the store decode (StoreDraw of the file the store loaded by id), format byte %u",
					n, s->file->name, r[2] - r[0], r[3] - r[1], s->file->id, archive ? archive + 1 : "?", caller, from, desc[0]);
			}
		}

		const LONG* rect = (const LONG*)(desc + 8);
		const BYTE* data = *(const BYTE* const*)(desc + 4);
		++storeStats.draws;
		Archive* archive = FileArchive(s->file);
		if (data && archive)
		{
			++loader.decodes;
			if (!desc[0])
				++loader.indices;
			loader.archive = archive;
			loader.file = s->file;
			loader.data = data;
			loader.pitchField = *(const LONG*)(desc + 0x18);
			loader.format = desc[0];
			SetRect(&loader.rect, rect[0], rect[1], rect[2], rect[3]);
			loader.frameNo = game.frameNo;
			loader.caller = caller;
			loader.captureSeq = 0;
			++loader.seq;
			if (!archive->sheetsLoaded)
				LoadSheets(archive);
			if (!SheetFile(archive, s->file->id) && rect[2] - rect[0] >= 480 && rect[3] - rect[1] >= 480)
				Twin::ScreenLabel(s->file->name);
		}
		// a repeat decode into the same memory rewrote it, which dropped its seed; it is seeded again
		if (!data || (s->data == data && !Twin::SeedMode()))
			return;

		++storeStats.drawLinks;
		LinkStoreData(s, data);
		s->left = rect[0];
		s->top = rect[1];
		s->dims[0] = rect[2] - rect[0];
		s->dims[1] = rect[3] - rect[1];
		LONG pitch = *(const LONG*)(desc + 0x18) >> 1;
		// the range assumes 32 bpp; the transfer guard reads the decode the way its format byte and pitch field allow, 8-bit indices included
		Twin::DecodeSource(data, *(const LONG*)(desc + 0x18), desc[0], (const RECT*)rect);

		if (!s->entry)
		{
			ArchiveFile* file = s->file;
			Entry* entry = Find((DWORD)file->dims, TRUE);
			if (entry)
			{
				if (!entry->name[0])
				{
					CopyName(entry->name, file->name);
					CopyName(entry->owner, file->name);
				}
				file->dims[0] = s->dims[0];
				file->dims[1] = s->dims[1];
				++entry->refs;
				s->entry = entry;
			}
		}

		BOOL valid;
		DWORD before = Twin::CaptureSeq(&valid);
		AddRange(s, data + (s->top * pitch + s->left) * sizeof(DWORD), pitch, &memory.learnStore, !desc[0]);
		DWORD after = Twin::CaptureSeq(&valid);
		if (loader.data == data && after != before && valid)
			loader.captureSeq = after;
	}

	// Discipl2.exe v2.02b 0x005A5F21: the face cache reads Faces.ff itself, so faces never pass through the image store; their odd key cannot match a game image pointer
	BYTE __stdcall FaceLoad(const BYTE* buffer, const DWORD* size, VOID* unused, VOID* archive, const CHAR* name)
	{
		BYTE ok = ((BYTE(__stdcall*)(const BYTE*, const DWORD*, VOID*, VOID*, const CHAR*))sub_FaceLoad)(buffer, size, unused, archive, name);
		if (!ok || !buffer || !size || !name || !*name)
			return ok;

		++storeStats.faceLoads;
		const BYTE* data = *(const BYTE* const*)(buffer + 4);
		StoreImage* s = data ? FindStore((const VOID*)(HashText(name, StrLength(name)) | 1), TRUE) : NULL;
		if (!s || (s->entry && StrCompareInsensitive(s->entry->owner, name)))
			return ok;

		if (!s->entry)
		{
			Entry* entry = Find((DWORD)s->dims, TRUE);
			if (!entry)
				return ok;
			CopyName(entry->name, name);
			CopyName(entry->owner, name);
			++entry->refs;
			s->entry = entry;
		}

		// the face buffer's stride is unknown here; the range is added at its first DrawFaces, which passes the real pitch
		DropRange(s);
		LinkStoreData(s, data);
		s->printed = FALSE;
		s->left = s->top = 0;
		s->dims[0] = size[0];
		s->dims[1] = size[1];
		// the face is seeded at its first DrawFaces, which copies it on at once; asked for now, its 4K is usually there by then
		if (Twin::SeedMode() && Twin::Active() && Enabled())
		{
			LONG frame = StoreFrame(s);
			if (frame >= 0)
				ArtState(frame);
		}
		return ok;
	}

	VOID* __fastcall StoreFree(VOID* thisObj, DWORD /*edx*/, DWORD flags)
	{
		RemoveStore(thisObj);
		return ((VOID*(__thiscall*)(VOID*, DWORD))sub_StoreFree)(thisObj, flags);
	}

	VOID* __fastcall ImageFree(VOID* thisObj, DWORD /*edx*/, DWORD flags)
	{
		DWORD rec = GetRecord(thisObj);
		if (rec)
			Release(Find(rec, FALSE));
		return ((VOID*(__thiscall*)(VOID*, DWORD))sub_ImageFree)(thisObj, flags);
	}

	VOID NameGround(const VOID* obj, DWORD race, LONG border, DWORD variant)
	{
		const CHAR* code = obj ? ((const CHAR*(__stdcall*)(DWORD))sub_RaceCode)(race) : NULL;
		GroundObject* o = code && StrLength(code) <= 2 ? FindGroundObject(obj, TRUE) : NULL;
		if (!o)
			return;

		if (border < 0)
			StrPrint(o->name, "%s_%02d.PNG", code, variant % 100);
		else
			StrPrint(o->name, "%s_%02d_%02d.PNG", code, border % 100, variant % 100);
		o->frame = GROUND_UNRESOLVED;

		// tiles are named when the map loads; asking for their art now has it uploaded before the first ground build needs it
		LONG frame = GroundFrame(obj);
		if (frame >= 0)
			Touch(&pack.texs[frame], frame);
	}

	VOID* __fastcall TileFactory(VOID* thisObj, DWORD /*edx*/, DWORD race, DWORD variant)
	{
		VOID* tile = ((VOID*(__thiscall*)(VOID*, DWORD, DWORD))sub_TileFactory)(thisObj, race, variant);
		NameGround(tile, race, -1, variant);
		return tile;
	}

	VOID* __fastcall BorderFactory(VOID* thisObj, DWORD /*edx*/, DWORD race, DWORD border, DWORD variant)
	{
		VOID* tile = ((VOID*(__thiscall*)(VOID*, DWORD, DWORD, DWORD))sub_BorderFactory)(thisObj, race, border, variant);
		NameGround(tile, race, (LONG)border, variant);
		return tile;
	}

	VOID __stdcall GroundUpdate(VOID* texture, const Hooks::BlitObject* obj)
	{
		if (obj)
			BeginChunk(obj);
		((VOID(__stdcall*)(VOID*, const Hooks::BlitObject*))sub_GroundUpdate)(texture, obj);

		GroundChunk* c = obj && ground.chunkCount ? FindChunk(obj->data) : NULL;
		if (c && c->valid)
		{
			c->pitch = obj->pitch >> 1;
			c->hash = HashRegion((const DWORD*)obj->data, c->pitch, 0, 0, c->cw, c->ch);
			if (Enabled() && c->count && c->sentVersion != c->version)
			{
				if (GroundReady(c - ground.chunks))
					SendGround(c);
				else
				{
					c->pending = TRUE;
					++ground.notReady;
				}
			}
		}
	}

	const VOID* __fastcall MaskData(const VOID* thisObj, DWORD /*edx*/)
	{
		const VOID* data = ((const VOID*(__thiscall*)(const VOID*))sub_MaskData)(thisObj);
		ground.masks[1] = ground.masks[0];
		ground.masks[0].obj = thisObj;
		ground.masks[0].data = data;
		return data;
	}

	struct Summary {
		const Entry* first;
		DWORD frames, mapped, sizeMiss, draws;
	};

	const CHAR* SummaryStatus(const Summary* s)
	{
		if (!s->first->anim)
			return "no-pack";
		if (s->sizeMiss)
			return "size-mismatch";
		if (s->mapped)
			return "mapped";
		return "frame-range";
	}

	VOID WriteCoverage()
	{
		Summary* list = (Summary*)MemoryAlloc(TABLE_SIZE * sizeof(Summary));
		if (!list)
			return;
		MemoryZero(list, TABLE_SIZE * sizeof(Summary));

		for (DWORD i = 0; i < entries.used; ++i)
		{
			const Entry* e = &entries.list[i];
			if (!e->rec || !e->draws || !e->owner[0])
				continue;

			DWORD index = HashText(e->owner, StrLength(e->owner)) & (TABLE_SIZE - 1);
			while (list[index].first && StrCompare(list[index].first->owner, e->owner))
				index = (index + 1) & (TABLE_SIZE - 1);

			Summary* s = &list[index];
			if (!s->first)
				s->first = e;
			++s->frames;
			if (e->packFrame >= 0)
				++s->mapped;
			else if (e->anim && e->origin == OriginSprite)
				++s->sizeMiss;
			s->draws += e->draws;
		}

		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, sizeof(path));
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, "imagecoverage.txt");

		HANDLE file = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			const CHAR* order[] = { "size-mismatch", "frame-range", "no-pack", "mapped" };
			CHAR line[256];
			DWORD written;
			for (DWORD k = 0; k < sizeof(order) / sizeof(*order); ++k)
				for (DWORD i = 0; i < TABLE_SIZE; ++i)
				{
					const Summary* s = &list[i];
					if (!s->first || StrCompare(SummaryStatus(s), order[k]))
						continue;

					const DWORD* dims = (const DWORD*)s->first->rec;
					DWORD n = StrPrint(line, "%-13s %-24s %4dx%-4d frames %d mapped %d draws %d pack %s\r\n",
						order[k], s->first->owner, dims[0], dims[1], s->frames, s->mapped, s->draws,
						s->first->anim ? pack.names + s->first->anim->id : "-");
					WriteFile(file, line, n, &written, NULL);
				}
			CloseHandle(file);
		}

		MemoryFree(list);
	}

	VOID LogStats()
	{
		MapOrder::Dump();
		Twin::LogStats();
		WriteCoverage();
		EnterCriticalSection(&requests.cs);
		DWORD demand = (requests.demand.tail + REQUEST_SIZE - requests.demand.head) % REQUEST_SIZE;
		DWORD prefetch = (requests.prefetch.tail + REQUEST_SIZE - requests.prefetch.head) % REQUEST_SIZE;
		DWORD decodedCount = pack.decodedList.count;
		LeaveCriticalSection(&requests.cs);

		DebugLog("image stats builds=%d draws=%d unknown=%d ground chunks=%d builds=%d ops=%d unnamed=%d unmasked=%d rejected=%d",
			stats.builds, stats.draws, stats.unknown, ground.chunkCount, ground.builds, ground.ops, ground.unnamed, ground.unmasked, ground.rejected);
		DebugLog("image ground published=%d arts=%d flushes=%d notReady=%d retried=%d mismatched=%d", ground.published, ground.arts, ground.flushes, ground.notReady, ground.retried, ground.mismatched);
		DebugLog("image memory ranges=%d hits=%d misses=%d dropped=%d rejected=%d learn store=%d face=%d", memory.count, memory.hits, memory.misses, memory.dropped, memory.rejected, memory.learnStore, memory.learnFace);
		DebugLog("image store draws=%d drawLinks=%d faceLinks=%d faceLoads=%d faceCopies=%d faceMisses=%d faceRejects=%d takes=%d takeLinks=%d takeFrames=%d",
			storeStats.draws, storeStats.drawLinks, storeStats.faceLinks, storeStats.faceLoads, storeStats.faceCopies, storeStats.faceMisses, storeStats.faceRejects, storeStats.takes, storeStats.takeLinks, storeStats.takeFrames);
		DebugLog("image art begins=%d notReady=%d noFrame=%d zoomed=%d truncated=%d", art.begins, art.notReady, art.noFrame, art.zoomed, art.truncated);
		DebugLog("image tables entries=%d/%d full=%d stores=%d/%d archives=%d queue demand=%d prefetch=%d decoded=%d (%d MB) gpu=%d (%d MB of %d MB budget)",
			entries.count, entries.used, stats.full, stores.count, stores.used, archives.count, demand, prefetch,
			decodedCount, pack.decoded >> 20, pack.gpuList.count, pack.gpu >> 20, pack.gpuBudget >> 20);
	}

	VOID __fastcall Render(VOID* thisObj, DWORD /*edx*/, VOID* renderer, const POINT* start, const POINT* offset, const POINT* size, const RECT* area)
	{
		++stats.draws;
		if (ground.retryFrame != game.frameNo && Enabled())
			RetryGround();

		DWORD rec = GetRecord(thisObj);
		Entry* entry = rec ? Find(rec, FALSE) : NULL;
		if (!entry || !entry->name[0])
			++stats.unknown;
		else
		{
			++entry->draws;
			if (Twin::SeedMode())
				Resolve(entry, rec);
			else
			{
				LONG index = Queue(entry, rec);
				if (!art.depth && Enabled())
					ExpectArt(entry, index, rec, start, offset, size, area);
			}
		}

		++art.depth;
		Twin::RenderBegin();
		((VOID(__thiscall*)(VOID*, VOID*, const POINT*, const POINT*, const POINT*, const RECT*))sub_Render)(thisObj, renderer, start, offset, size, area);
		Twin::RenderEnd();
		--art.depth;

		DWORD tick = GetTickCount();
		// Ctrl+F12 is the twin display toggle
		BOOL key = (GetAsyncKeyState(VK_F12) & 0x8000) && !(GetAsyncKeyState(VK_CONTROL) & 0x8000);
		if (key && !trace.key)
			LogStats();
		trace.key = key;

		if ((LONG)(tick - trace.flagCheck) >= 0)
		{
			trace.flagCheck = tick + FLAG_CHECK_MS;
			if (TakeFlag())
				LogStats();
		}
	}

	// the code the next round disassembles: the store's hooks and, in v2.02b, the code the 16:53 run named around the big images' conversion and copy
	VOID DumpCode(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (hookSpace->image_store_draw == 0x00520C53)
		{
			// Discipl2.exe v2.02b: +105107 calls the indexed blit into the scratch, +10515E the face copy, +267FB8 < +267EEE copy the scratch into the sized surface, +27298E wrote it
			const DWORD seen[] = { 0x00505107, 0x0050515E, 0x00667FB8, 0x00667EEE, 0x0067298E };
			for (DWORD i = 0; i < sizeof(seen) / sizeof(*seen); ++i)
				Twin::CodeDump(f(seen[i]), 0x400, "seen in the 16:53 log");
			// the 17:54 round: StoreDraw's caller, the record open and size behind its ReadFile, its row remap, the 8-bit conversion's thunk and call, the other ReadFile callers
			const DWORD next[] = { 0x0052AF88, 0x00523A00, 0x00523A9D, 0x005220A1, 0x0066DD79, 0x0066E011, 0x005208FE, 0x00520DE5, 0x005A5FC4, 0x0051FEE4 };
			for (DWORD i = 0; i < sizeof(next) / sizeof(*next); ++i)
				Twin::CodeDump(f(next[i]), 0x600, "seen in the 17:54 log");
		}
		Twin::CodeDump(f(hookSpace->image_store_draw), 0x400, "StoreDraw");
		Twin::CodeDump(f(hookSpace->image_store_load), 0x400, "StoreLoad");
		if (hookSpace->face_load)
			Twin::CodeDump(f(hookSpace->face_load), 0x200, "FaceLoad call");

		// the decode routine is one of the calls near the start of StoreDraw
		BYTE code[0x200];
		if (!ReadBlock(hooker, hookSpace->image_store_draw, code, sizeof(code)))
			return;
		DWORD targets[6], count = 0;
		for (DWORD i = 0; i + 5 <= sizeof(code) && count < sizeof(targets) / sizeof(*targets); ++i)
		{
			if (code[i] != 0xE8)
				continue;
			DWORD target = f(hookSpace->image_store_draw) + i + 5 + *(const DWORD*)(code + i + 1);
			BOOL seen = FALSE;
			for (DWORD k = 0; k < count && !seen; ++k)
				seen = targets[k] == target;
			if (seen || target - f(0x00401000) >= 0x00400000)
				continue;
			targets[count++] = target;
			Twin::CodeDump(target, 0x200, "called by StoreDraw");
		}
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!archiveCsReady)
		{
			InitializeCriticalSection(&archiveCs);
			archiveCsReady = TRUE;
		}
		if (!hookSpace->image_render_slot)
			return;

		DWORD render = 0;
		if (!ReadDWord(hooker, hookSpace->image_render_slot, &render) || render != f(hookSpace->image_render))
		{
			DebugLog("image render slot %08X holds %08X, expected %08X", hookSpace->image_render_slot, render, f(hookSpace->image_render));
			return;
		}

		sub_Build = RedirectCall(hooker, hookSpace->image_build_still, Build);
		RedirectCall(hooker, hookSpace->image_build_anim, Build);
		DWORD names = 0;
		if (ReadDWord(hooker, hookSpace->image_names_slot, &names) && names == f(hookSpace->image_names))
		{
			sub_Names = names;
			PatchDWord(hooker, hookSpace->image_names_slot, (DWORD)Names);
		}
		else
			DebugLog("image names slot %08X holds %08X, expected %08X", hookSpace->image_names_slot, names, f(hookSpace->image_names));

		sub_Render = render;
		PatchDWord(hooker, hookSpace->image_render_slot, (DWORD)Render);

		DWORD imageFree = 0;
		if (hookSpace->image_free_slot && ReadDWord(hooker, hookSpace->image_free_slot, &imageFree) && imageFree == f(hookSpace->image_free))
		{
			sub_ImageFree = imageFree;
			PatchDWord(hooker, hookSpace->image_free_slot, (DWORD)ImageFree);
		}
		else if (hookSpace->image_free_slot)
			DebugLog("image free slot %08X holds %08X, expected %08X", hookSpace->image_free_slot, imageFree, f(hookSpace->image_free));

		DWORD load = 0, draw = 0;
		if (hookSpace->image_store_load_slot && ReadDWord(hooker, hookSpace->image_store_load_slot, &load) && load == f(hookSpace->image_store_load)
			&& ReadDWord(hooker, hookSpace->image_store_draw_slot, &draw) && draw == f(hookSpace->image_store_draw))
		{
			sub_StoreLoad = load;
			sub_StoreDraw = draw;
			PatchDWord(hooker, hookSpace->image_store_load_slot, (DWORD)StoreLoad);
			PatchDWord(hooker, hookSpace->image_store_draw_slot, (DWORD)StoreDraw);

			DWORD storeFree = 0;
			if (ReadDWord(hooker, hookSpace->image_store_free_slot, &storeFree) && storeFree == f(hookSpace->image_store_free))
			{
				sub_StoreFree = storeFree;
				PatchDWord(hooker, hookSpace->image_store_free_slot, (DWORD)StoreFree);
			}
			else
				DebugLog("image store free slot %08X holds %08X, expected %08X", hookSpace->image_store_free_slot, storeFree, f(hookSpace->image_store_free));
		}
		else if (hookSpace->image_store_load_slot)
			DebugLog("image store slots hold %08X %08X, expected %08X %08X", load, draw, f(hookSpace->image_store_load), f(hookSpace->image_store_draw));

		if (hookSpace->face_load)
			sub_FaceLoad = RedirectCall(hooker, hookSpace->face_load, FaceLoad);

		DWORD mask = 0, update = 0;
		if (hookSpace->ground_mask_slot && ReadDWord(hooker, hookSpace->ground_mask_slot, &mask) && mask == f(hookSpace->ground_mask)
			&& ReadDWord(hooker, hookSpace->ground_update_slot, &update) && update == f(hookSpace->ground_update))
		{
			sub_GroundUpdate = update;
			PatchDWord(hooker, hookSpace->ground_update_slot, (DWORD)GroundUpdate);
			sub_RaceCode = f(hookSpace->ground_race_code);
			sub_TileFactory = RedirectCall(hooker, hookSpace->ground_tile_factory, TileFactory);
			sub_BorderFactory = RedirectCall(hooker, hookSpace->ground_border_factory, BorderFactory);
			sub_MaskData = mask;
			PatchDWord(hooker, hookSpace->ground_mask_slot, (DWORD)MaskData);
		}
		StartPack();
		InitializeCriticalSection(&records.cs);
		records.gameThread = GetCurrentThreadId();
		records.ready = TRUE;
		DumpCode(hooker, hookSpace, baseOffset);
		DebugLog("image replace loaded");
	}
#pragma endregion
}
