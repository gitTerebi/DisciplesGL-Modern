#include "StdAfx.h"
#include "Config.h"
#include "GLib.h"
#include "Hooks.h"
#include "ImageReplace.h"
#include "MapOrder.h"
#include "Twin.h"
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

	struct Archive {
		CHAR path[MAX_PATH];
		ArchiveFile* files;
		DWORD mask;
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
		Entry* entry;
		ArchiveFile* file;
		DWORD nextFree;
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
		DWORD hits, misses, learnStore, learnFace, dropped, rejected;
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

	BOOL InsertAnim(const CHAR* game, const CHAR* id, DWORD idLength, DWORD first, DWORD count, Origin origin)
	{
		if (pack.animCount >= ANIM_SIZE * 3 / 4 || FindAnim(game))
			return FALSE;

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

		DebugLog("image pack loaded: %d animations, %d frames in %d ms", pack.animCount, pack.frameCount, GetTickCount() - tick);
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

	LONG Resolve(Entry* entry, DWORD rec)
	{
		if (entry->resolved)
			return entry->packFrame;
		if (!pack.ready)
			return -1;

		entry->resolved = TRUE;
		entry->packFrame = -1;

		CHAR name[NAME_LENGTH + 8];
		StrCopy(name, entry->owner);
		Upper(name);
		const PackAnim* anim = FindAnim(name);
		if (!anim)
		{
			DWORD length = StrLength(name);
			if (length > 4 && !StrCompare(name + length - 4, ".PNG"))
				name[length - 4] = '\0';
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

		entry->anim = anim;
		entry->origin = anim->origin;
		const DWORD* dims = (const DWORD*)rec;
		const PackFrame* frame = &pack.frames[anim->first];
		if (anim->origin == OriginSprite)
		{
			if ((LONG)dims[0] == frame->w && (LONG)dims[1] == frame->h)
				entry->packFrame = anim->first;
		}
		else if (entry->frame < anim->count)
			entry->packFrame = anim->first + entry->frame;

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
		return pack.ready && frame >= 0 && (DWORD)frame < pack.frameCount && pack.texs[frame].state == TexUploaded;
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

	VOID AddRange(StoreImage* s, const BYTE* first, LONG pitch, DWORD* learned)
	{
		DropRange(s);
		LONG w = (LONG)s->dims[0], h = (LONG)s->dims[1];
		if (!config.bpp32Hooked || !first || w <= 0 || h <= 0 || pitch < w || pitch > RANGE_PITCH || memory.count >= sizeof(memory.list) / sizeof(*memory.list))
			return;

		const BYTE* end = first + ((h - 1) * pitch + w) * sizeof(DWORD);
		if (IsBadReadPtr(first, end - first))
		{
			++memory.rejected;
			return;
		}

		s->first = first;
		s->pitch = pitch;
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

		LONG frame = StoreFrame(s);
		if (frame >= 0)
		{
			RECT rect = { 0, 0, w, h };
			Twin::LearnArt(first, pitch, &rect, frame, &rect, w, h, FALSE);
			++*learned;
		}
	}

	BOOL FindMemory(const VOID* ptr, const VOID** base, LONG* pitch, LONG* width, LONG* height, LONG* frame, BOOL* flipped)
	{
		const BYTE* at = (const BYTE*)ptr;
		DWORD i = memory.count ? RangeLower(at + 1) : 0;
		for (DWORD n = 0; i && n < RANGE_SCAN; --i, ++n)
		{
			const Range* r = &memory.list[i - 1];
			if ((DWORD)(at - r->start) >= memory.span)
				break;
			StoreImage* s = &stores.list[r->store];
			if (at < r->end && RangeHolds(s, at))
			{
				++memory.hits;
				*base = s->first;
				*pitch = s->pitch;
				*width = (LONG)s->dims[0];
				*height = (LONG)s->dims[1];
				*frame = StoreFrame(s);
				*flipped = FALSE;
				return TRUE;
			}
		}
		++memory.misses;
		return FALSE;
	}

	VOID ResetStore(StoreImage* s, ArchiveFile* file)
	{
		DropRange(s);
		UnlinkStoreData(s);
		s->printed = FALSE;
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
		DWORD drawLinks, faceLinks, faceLoads, faceCopies, faceMisses, faceRejects, takes, takeLinks, takeFrames;
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
			AddRange(s, (const BYTE*)src, srcPitch, &memory.learnFace);

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
		ResetChunk(c, TRUE, 0);
		++ground.builds;
	}

	VOID OnGround(GroundKind kind, const VOID* tile, const Hooks::BlitObject* obj, const POINT* src, const POINT* dst, const RECT* rect, const BYTE* blendMask, const DWORD* alphaMask)
	{
		if (!Enabled() || !obj->isTrueColor || !sub_MaskData)
			return;

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

		if (ground.arts < GROUND_LOG)
			DebugLog("image ground art chunk=%d data=%08X size=%dx%d ops=%d", c - ground.chunks, c->data, c->cw, c->ch, c->count);
		++ground.arts;

		RECT rect = { 0, 0, c->cw, c->ch };
		Twin::GroundArt(c->data, c->pitch, &rect, &blob, sizeof(blob) - sizeof(blob.ops) + c->count * sizeof(GroundOp));
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
			"	if (n > 1.001)\n"
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
			"		if (fTex.x < -0.0001 || fTex.y < -0.0001 || fTex.x > 1.0001 || fTex.y > 1.0001)\n"
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

	ArchiveFile* FindArchiveFile(const VOID* factory, DWORD id)
	{
		DWORD offset;
		const CHAR* path = ArchivePath(factory, &offset);
		if (!path)
			return NULL;

		Archive* archive = NULL;
		for (DWORD i = 0; i < archives.count && !archive; ++i)
			if (!StrCompareInsensitive(archives.list[i].path, path))
				archive = &archives.list[i];

		if (!archive)
		{
			if (archives.count >= ARCHIVE_LIMIT || StrLength(path) >= MAX_PATH)
				return NULL;
			archive = &archives.list[archives.count++];
			StrCopy(archive->path, path);
			LoadArchive(archive);
		}
		if (!archive->files)
			return NULL;

		DWORD rec = id > offset ? id - offset : id;
		for (DWORD slot = (rec * 2654435761u) & archive->mask; archive->files[slot].id; slot = (slot + 1) & archive->mask)
			if (archive->files[slot].id == rec)
				return &archive->files[slot];
		return NULL;
	}

	VOID* __fastcall StoreLoad(VOID* thisObj, DWORD /*edx*/, DWORD id, DWORD flag, VOID* out)
	{
		VOID* image = ((VOID*(__thiscall*)(VOID*, DWORD, DWORD, VOID*))sub_StoreLoad)(thisObj, id, flag, out);
		if (!image)
			return image;

		ArchiveFile* file = FindArchiveFile(thisObj, id);
		StoreImage* s = FindStore(image, file != NULL);
		if (s && s->file != file)
			ResetStore(s, file);

		return image;
	}

	VOID __fastcall StoreDraw(VOID* thisObj, DWORD /*edx*/, const BYTE* desc)
	{
		((VOID(__thiscall*)(VOID*, const BYTE*))sub_StoreDraw)(thisObj, desc);

		StoreImage* s = FindStore(thisObj, FALSE);
		if (!s || !s->file || !desc)
			return;

		const LONG* rect = (const LONG*)(desc + 8);
		const BYTE* data = *(const BYTE* const*)(desc + 4);
		if (!data || s->data == data)
			return;

		++storeStats.drawLinks;
		LinkStoreData(s, data);
		s->left = rect[0];
		s->top = rect[1];
		s->dims[0] = rect[2] - rect[0];
		s->dims[1] = rect[3] - rect[1];
		LONG pitch = *(const LONG*)(desc + 0x18) >> 1;

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

		AddRange(s, data + (s->top * pitch + s->left) * sizeof(DWORD), pitch, &memory.learnStore);
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
			if (Enabled() && c->count)
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
		DebugLog("image ground published=%d arts=%d notReady=%d retried=%d mismatched=%d", ground.published, ground.arts, ground.notReady, ground.retried, ground.mismatched);
		DebugLog("image memory ranges=%d hits=%d misses=%d dropped=%d rejected=%d learn store=%d face=%d", memory.count, memory.hits, memory.misses, memory.dropped, memory.rejected, memory.learnStore, memory.learnFace);
		DebugLog("image store drawLinks=%d faceLinks=%d faceLoads=%d faceCopies=%d faceMisses=%d faceRejects=%d takes=%d takeLinks=%d takeFrames=%d",
			storeStats.drawLinks, storeStats.faceLinks, storeStats.faceLoads, storeStats.faceCopies, storeStats.faceMisses, storeStats.faceRejects, storeStats.takes, storeStats.takeLinks, storeStats.takeFrames);
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
			LONG index = Queue(entry, rec);
			if (!art.depth && Enabled())
				ExpectArt(entry, index, rec, start, offset, size, area);
		}

		++art.depth;
		((VOID(__thiscall*)(VOID*, VOID*, const POINT*, const POINT*, const POINT*, const RECT*))sub_Render)(thisObj, renderer, start, offset, size, area);
		--art.depth;

		DWORD tick = GetTickCount();
		BOOL key = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
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

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
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
		DebugLog("image replace loaded");
	}
#pragma endregion
}
