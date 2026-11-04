#include "StdAfx.h"
#include "Config.h"
#include "GLib.h"
#include "Hooks.h"
#include "ImageReplace.h"
#include "MapOrder.h"
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
#ifndef GL_RED
#define GL_RED 0x1903
#endif
#ifndef GL_R32F
#define GL_R32F 0x822E
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
#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
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
	#define MAX_ITEMS 4096
	#define MAX_EXPECT 8192
	#define EXPECT_SCAN 256
	#define EXPECT_BUCKETS 4096
	#define SLOTS 6
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
		DWORD draws, queued, claimed;
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

	struct Item {
		Entry* entry;
		LONG frame;
		LONG x, y, w, h;
		RECT clip;
		BOOL flipped;
		BOOL replaced;
		DWORD layer;
		StripLook look;
	};

	struct Expect {
		LONG x, y, w, h;
		LONG sx, sy;
		DWORD item;
		DWORD next;
		BOOL used;
	};

	struct DrawItem {
		LONG frame;
		LONG x, y, w, h;
		RECT clip;
		BOOL flipped;
		DWORD layer;
		StripLook look;
	};

	struct Slot {
		StateBuffer* frame;
		DrawItem* items;
		DWORD count;
		DWORD* plane;
		LONG planeW, planeH;
		DWORD age;
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
		CRITICAL_SECTION cs;
		OpenDrawSurface* target;
		OpenDrawSurface* presented;
		volatile LONG frameNo;
		DWORD* plane;
		LONG planeW, planeH;
		DWORD seq;
		BOOL dirty;
		Item items[MAX_ITEMS];
		DWORD itemCount;
		DWORD replacedCount;
		Expect expect[MAX_EXPECT];
		DWORD expectHead, expectCount;
		DWORD split;
		const VOID* splitSource;
		DWORD expectBucket[EXPECT_BUCKETS];
		Slot slots[SLOTS];
		DWORD slotAge;
		LONG replaced, skipped, notReady, matchMiss;
	} game;

	struct {
		DrawItem* items;
		DWORD count;
		DWORD capacity;
		DWORD* plane;
		LONG planeW, planeH;
		BOOL changed;
	} drawList;

	typedef VOID(__stdcall* GLGENERATEMIPMAP)(GLenum target);
	typedef VOID(__stdcall* GLCOMPRESSEDTEXIMAGE2D)(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLint border, GLsizei size, const GLvoid* data);

	struct {
		BOOL ready;
		BOOL failed;
		GLuint program;
		GLuint vao;
		GLuint vbo;
		GLuint planeTexture;
		GLint shape, tint, rim;
		LONG planeW, planeH;
		FLOAT* planeData;
		Vertex* vertices;
		DWORD vertexCapacity;
		GLGENERATEMIPMAP generateMipmap;
		GLCOMPRESSEDTEXIMAGE2D compressedTexImage2D;
		LARGE_INTEGER frequency;
	} gl;

	#define GROUND_OBJECTS 4096
	#define GROUND_CHUNKS 64
	#define GROUND_OPS 512
	#define GROUND_SCALE 2
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
		DWORD pubColor, pubVersion, pubCount;
		LONG w, h;
		GroundOp pub[GROUND_OPS];
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
		DWORD builds, ops, unnamed, unmasked, rejected, written, follows, partial, direct, stale, claims;
	} ground;

	DWORD sub_TileFactory, sub_BorderFactory, sub_MaskData, sub_RaceCode, sub_GroundUpdate;

	struct GroundTarget {
		GLuint texture;
		DWORD version;
		LONG w, h;
	};

	struct {
		GLuint program;
		GLuint fbo;
		GLint mode, two;
		GroundOp ops[GROUND_OPS];
		GroundTarget chunks[GROUND_CHUNKS];
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
		InitializeCriticalSection(&game.cs);
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

	BOOL InTarget(const VOID* data, LONG* ox, LONG* oy)
	{
		OpenDrawSurface* surface = game.target;
		if (!surface || !surface->indexBuffer || !surface->indexBuffer->data)
			return FALSE;

		const BYTE* base = (const BYTE*)surface->indexBuffer->data;
		DWORD pitch = surface->mode.width * sizeof(DWORD);
		if ((const BYTE*)data < base || (const BYTE*)data >= base + pitch * surface->mode.height)
			return FALSE;

		DWORD offset = (const BYTE*)data - base;
		*oy = offset / pitch;
		*ox = (offset % pitch) / sizeof(DWORD);
		return TRUE;
	}

	BOOL PlaneEnsure()
	{
		OpenDrawSurface* surface = game.target;
		LONG width = surface->mode.width, height = surface->mode.height;
		if (game.plane && game.planeW == width && game.planeH == height)
			return TRUE;

		if (game.plane)
			MemoryFree(game.plane);
		game.plane = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
		game.planeW = game.plane ? width : 0;
		game.planeH = game.plane ? height : 0;
		if (game.plane)
			MemoryZero(game.plane, width * height * sizeof(DWORD));
		return game.plane != NULL;
	}

	VOID Cover(LONG x, LONG y, LONG width, LONG height, const DWORD* src, LONG stride, BOOL keyed, DWORD key, const BYTE* mask)
	{
		if (!game.replacedCount || !PlaneEnsure())
			return;

		DWORD seq = ++game.seq;
		for (DWORD i = 0; i < game.itemCount; ++i)
		{
			const Item* item = &game.items[i];
			if (!item->replaced)
				continue;

			LONG l = max(max(item->x, x), 0), t = max(max(item->y, y), 0);
			LONG r = min(min(item->x + item->w, x + width), game.planeW), b = min(min(item->y + item->h, y + height), game.planeH);
			for (LONG py = t; py < b; ++py)
				for (LONG px = l; px < r; ++px)
				{
					if (src && keyed)
					{
						LONG sx = px - x, sy = py - y;
						const DWORD* p = src + sy * stride + sx;
						if (*p == key && (item->frame >= 0
							|| !((sx > 0 && p[-1] != key) || (sx + 1 < width && p[1] != key) || (sy > 0 && p[-stride] != key) || (sy + 1 < height && p[stride] != key))))
							continue;
					}
					if (mask && mask[px - x] < 128)
						continue;
					game.plane[py * game.planeW + px] = seq;
				}
		}
	}

	DWORD ExpectBucket(LONG x, LONG y)
	{
		return ((DWORD)x * 73856093u ^ (DWORD)y * 19349663u) & (EXPECT_BUCKETS - 1);
	}

	VOID ResetExpect()
	{
		if (game.expectCount)
			MemoryZero(game.expectBucket, sizeof(game.expectBucket));
		game.expectHead = game.expectCount = 0;
		game.split = 0;
	}

	VOID QueueParts(DWORD itemIndex, DWORD rec, const POINT* start, const POINT* offset, const POINT* size)
	{
		const DWORD* dims = (const DWORD*)rec;
		const BYTE* parts = *(const BYTE**)(rec + 8);
		DWORD count = *(const DWORD*)(rec + 0xC);
		LONG offX = offset ? offset->x : 0, offY = offset ? offset->y : 0;
		LONG sizeX = size ? size->x : dims[0], sizeY = size ? size->y : dims[1];

		for (DWORD i = 0; i < count; ++i)
		{
			const LONG* part = (const LONG*)(parts + i * 24);
			if (part[0] < offX || part[0] + part[4] > sizeX || part[1] < offY || part[1] + part[5] > sizeY)
				continue;
			if (game.expectCount >= MAX_EXPECT)
				return;

			DWORD index = game.expectCount++;
			Expect* e = &game.expect[index];
			e->x = start->x + part[0];
			e->y = start->y + part[1];
			e->w = part[4];
			e->h = part[5];
			e->sx = part[2];
			e->sy = part[3];
			e->item = itemIndex;
			e->used = FALSE;
			DWORD* bucket = &game.expectBucket[ExpectBucket(e->x, e->y)];
			e->next = *bucket;
			*bucket = index + 1;
		}
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

	VOID Queue(Entry* entry, DWORD rec, const POINT* start, const POINT* offset, const POINT* size, const RECT* area)
	{
		if (!Enabled() || !start)
			return;

		LONG index = Resolve(entry, rec);
		if (index < 0)
			return;

		Tex* tex = &pack.texs[index];
		BOOL missing = Touch(tex, index);
		Prefetch(entry->anim, index, missing);

		if (game.itemCount >= MAX_ITEMS)
			return;

		if (tex->state != TexUploaded)
		{
			++game.notReady;
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

		Item* item = &game.items[game.itemCount];
		item->entry = entry;
		item->frame = index;
		item->flipped = entry->flipped;
		item->x = start->x + (entry->flipped ? canvasW - ox - frame->w : ox);
		item->y = start->y + oy;
		item->w = frame->w;
		item->h = frame->h;

		LONG offX = offset ? offset->x : 0, offY = offset ? offset->y : 0;
		LONG sizeX = size ? size->x : canvasW, sizeY = size ? size->y : canvasH;
		SetRect(&item->clip, start->x + offX, start->y + offY, start->x + sizeX, start->y + sizeY);
		if (area)
		{
			item->clip.left = max(item->clip.left, area->left);
			item->clip.top = max(item->clip.top, area->top);
			item->clip.right = min(item->clip.right, area->right);
			item->clip.bottom = min(item->clip.bottom, area->bottom);
		}
		item->replaced = FALSE;
		item->layer = 0;
		item->look = StripLook();

		if (Config::IsMapZoomed())
		{
			RECT zoom;
			Config::GetMapZoomRect(&zoom);
			if (item->x < zoom.right && item->y < zoom.bottom && item->x + item->w > zoom.left && item->y + item->h > zoom.top)
				return;
		}

		QueueParts(game.itemCount, rec, start, offset, size);
		++game.itemCount;
		++entry->queued;
	}

	BOOL Take(DWORD index, DWORD* itemIndex, BOOL whole, const VOID* source)
	{
		Expect* e = &game.expect[index];
		e->used = TRUE;
		*itemIndex = e->item;
		game.split = whole || !source ? 0 : index + 1;
		game.splitSource = source;
		while (game.expectHead < game.expectCount && game.expect[game.expectHead].used)
			++game.expectHead;
		return TRUE;
	}

	BOOL Inside(const Expect* e, LONG x, LONG y, LONG w, LONG h, LONG sx, LONG sy)
	{
		return x >= e->x && y >= e->y && x + w <= e->x + e->w && y + h <= e->y + e->h && sx - e->sx == x - e->x && sy - e->sy == y - e->y;
	}

	BOOL Match(LONG x, LONG y, LONG w, LONG h, LONG sx, LONG sy, const VOID* source, DWORD* itemIndex)
	{
		if (game.split && source == game.splitSource && Inside(&game.expect[game.split - 1], x, y, w, h, sx, sy))
		{
			*itemIndex = game.expect[game.split - 1].item;
			return TRUE;
		}
		game.split = 0;

		DWORD best = 0;
		BOOL bestExact = FALSE;
		for (DWORD next = game.expectBucket[ExpectBucket(x, y)]; next; next = game.expect[next - 1].next)
		{
			const Expect* e = &game.expect[next - 1];
			if (e->used || e->x != x || e->y != y || e->sx != sx || e->sy != sy || w > e->w || h > e->h)
				continue;
			BOOL exact = w == e->w && h == e->h;
			if (!best || exact)
			{
				best = next;
				bestExact = exact;
			}
			if (exact)
				break;
		}
		if (best)
			return Take(best - 1, itemIndex, bestExact, source);

		DWORD end = min(game.expectCount, game.expectHead + EXPECT_SCAN);
		for (DWORD i = game.expectHead; i < end; ++i)
		{
			const Expect* e = &game.expect[i];
			if (!e->used && Inside(e, x, y, w, h, sx, sy))
				return Take(i, itemIndex, FALSE, source);
		}
		return FALSE;
	}

	BOOL Claim(OpenDrawSurface* dst, LONG x, LONG y, LONG w, LONG h, LONG sx, LONG sy, const VOID* source)
	{
		DWORD itemIndex;
		if (!game.expectCount || dst != game.presented || !Match(x, y, w, h, sx, sy, source, &itemIndex))
			return FALSE;

		Item* item = &game.items[itemIndex];
		if (!item->replaced)
		{
			if (game.target != dst)
			{
				game.target = dst;
				game.replacedCount = 0;
			}
			item->replaced = TRUE;
			item->layer = ++game.seq;
			++game.replacedCount;
			++game.replaced;
			++item->entry->claimed;
		}
		game.dirty = TRUE;
		++game.skipped;
		return TRUE;
	}

	BOOL OnPixelBlit(const POINT* srcPos, const DWORD* dstData, LONG dstPitch, const POINT* dstPos, const SIZE* size)
	{
		if (!game.initialized || !game.expectCount || !dstData || size->cx <= 0 || size->cy <= 0)
			return FALSE;

		OpenDrawSurface* surface = game.presented;
		if (!surface || !surface->indexBuffer || !surface->indexBuffer->data)
			return FALSE;

		const DWORD* base = (const DWORD*)surface->indexBuffer->data;
		const DWORD* dst = dstData + dstPitch / 2 * dstPos->y + dstPos->x;
		DWORD width = surface->mode.width;
		if (dst < base || dst >= base + width * surface->mode.height)
			return FALSE;

		DWORD offset = dst - base;
		return Claim(surface, offset % width, offset / width, size->cx, size->cy, srcPos->x, srcPos->y, NULL);
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

	VOID ResetStore(StoreImage* s, ArchiveFile* file)
	{
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
		DWORD tries, blocked, missed, waited, claimed, shadowed, drawLinks, blitLinks, pixelLinks, rawLinks, faceLinks, finds, faceLoads;
		DWORD faceCopies, faceMisses, faceRejects, takes, takeLinks, takeFrames, stripHits, stripAway, stripFollows, stripClaims, stripItems, stripStale;
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

	#define STRIP_FACES 160
	#define STRIP_LINKS 8
	#define STRIP_GRID 4

	struct StripLink {
		const DWORD* origin;
		LONG pitch;
	};

	struct {
		StripLink links[STRIP_LINKS];
		DWORD linkCount, linkNext;
		LONG width, height;
		DWORD count;
		StripFace faces[STRIP_FACES];
		DWORD samples[STRIP_FACES][STRIP_GRID * STRIP_GRID];
		DWORD sampled[STRIP_FACES];
	} strip;

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

	VOID DropStripLinks(const DWORD* at)
	{
		FaceLink* l = FaceSlot((const BYTE*)at);
		if (l->data == (const BYTE*)at)
			l->data = NULL;

		StoreImage* s = stores.count ? FindStoreData((const BYTE*)at) : NULL;
		if (s)
			UnlinkStoreData(s);
	}

	VOID StripPoint(const RECT* clip, DWORD k, LONG* x, LONG* y)
	{
		*x = clip->left + (LONG)(2 * (k % STRIP_GRID) + 1) * (clip->right - clip->left) / (2 * STRIP_GRID);
		*y = clip->top + (LONG)(2 * (k / STRIP_GRID) + 1) * (clip->bottom - clip->top) / (2 * STRIP_GRID);
	}

	VOID OnStrip(const VOID* origin, LONG pitch, LONG width, LONG height, const StripFace* faces, DWORD count)
	{
		if (!origin || strip.links[0].origin != origin || strip.links[0].pitch != pitch)
		{
			strip.links[0].origin = (const DWORD*)origin;
			strip.links[0].pitch = pitch;
			strip.linkCount = origin ? 1 : 0;
			strip.linkNext = 0;
		}
		if (origin)
			DropStripLinks((const DWORD*)origin);

		strip.width = width;
		strip.height = height;
		strip.count = min(count, STRIP_FACES);
		if (!strip.count)
			return;
		MemoryCopy(strip.faces, faces, strip.count * sizeof(StripFace));

		DWORD key = Hooks::Convert565toRGB(COLORKEY_CHECK_16);
		for (DWORD i = 0; i < strip.count; ++i)
		{
			strip.sampled[i] = 0;
			for (DWORD k = 0; k < STRIP_GRID * STRIP_GRID; ++k)
			{
				LONG sx, sy;
				StripPoint(&strip.faces[i].clip, k, &sx, &sy);
				if (sx < 0 || sy < 0 || sx >= width || sy >= height)
					continue;
				DWORD pix = ((const DWORD*)origin)[sy * pitch + sx];
				if (pix == key)
					continue;
				strip.samples[i][k] = pix;
				strip.sampled[i] |= 1 << k;
			}
		}
	}

	BOOL StripMatches(DWORD i, const DWORD* src, LONG stride, LONG w, LONG h, LONG ox, LONG oy, BOOL* checked)
	{
		for (DWORD k = 0; k < STRIP_GRID * STRIP_GRID; ++k)
		{
			if (!(strip.sampled[i] & (1 << k)))
				continue;
			LONG sx, sy;
			StripPoint(&strip.faces[i].clip, k, &sx, &sy);
			sx += ox;
			sy += oy;
			if (sx < 0 || sy < 0 || sx >= w || sy >= h)
				continue;
			if (src[sy * stride + sx] != strip.samples[i][k])
				return FALSE;
			*checked = TRUE;
		}
		return TRUE;
	}

	BOOL FindStrip(const DWORD* src, LONG stride, LONG w, LONG h, LONG* ox, LONG* oy, DWORD* found = NULL)
	{
		for (DWORD i = 0; i < strip.linkCount; ++i)
		{
			const StripLink* link = &strip.links[i];
			if (stride != link->pitch || stride <= 0)
				continue;

			LONG d = link->origin - src;
			*oy = d >= 0 ? d / stride : -((stride - 1 - d) / stride);
			*ox = d - *oy * stride;
			BOOL inside = *ox <= 0 && *oy <= 0 && -*ox < strip.width && -*oy < strip.height;
			BOOL around = *ox >= 0 && *oy >= 0 && *ox + strip.width <= w && *oy + strip.height <= h;
			if (inside || around)
			{
				if (found)
					*found = i;
				return TRUE;
			}
		}
		return FALSE;
	}

	VOID FollowStrip(const DWORD* src, LONG stride, LONG w, LONG h, const DWORD* dst, LONG dstStride)
	{
		LONG ox, oy;
		if (!strip.linkCount || !dst || dstStride <= 0 || !FindStrip(src, stride, w, h, &ox, &oy))
			return;
		++storeStats.stripAway;

		const DWORD* to = dst + oy * dstStride + ox;
		for (DWORD i = 0; i < strip.linkCount; ++i)
			if (strip.links[i].origin == to && strip.links[i].pitch == dstStride)
				return;

		DropStripLinks(to);
		DWORD slot = strip.linkCount < STRIP_LINKS ? strip.linkCount++ : 1 + strip.linkNext++ % (STRIP_LINKS - 1);
		strip.links[slot].origin = to;
		strip.links[slot].pitch = dstStride;
		++storeStats.stripFollows;
	}

	VOID ClaimStrip(OpenDrawSurface* dst, LONG x, LONG y, const DWORD* src, LONG stride, LONG w, LONG h)
	{
		LONG ox, oy;
		DWORD link;
		if (!strip.linkCount || !FindStrip(src, stride, w, h, &ox, &oy, &link))
			return;
		++storeStats.stripHits;
		if (!strip.count || !dst || dst != game.presented || !Enabled())
			return;

		BOOL checked[STRIP_FACES];
		for (DWORD i = 0; i < strip.count; ++i)
		{
			checked[i] = FALSE;
			if (!StripMatches(i, src, stride, w, h, ox, oy, &checked[i]))
			{
				strip.links[link].pitch = 0;
				++storeStats.stripStale;
				return;
			}
		}
		++storeStats.stripClaims;

		for (DWORD i = 0; i < strip.count && game.itemCount < MAX_ITEMS; ++i)
		{
			const StripFace* f = &strip.faces[i];
			LONG l = max(ox + f->clip.left, 0), t = max(oy + f->clip.top, 0);
			LONG r = min(ox + f->clip.right, w), b = min(oy + f->clip.bottom, h);
			if (l >= r || t >= b || !checked[i])
				continue;

			Tex* tex = &pack.texs[f->frame];
			Touch(tex, f->frame);
			if (tex->state != TexUploaded)
			{
				++game.notReady;
				continue;
			}

			if (game.target != dst)
			{
				game.target = dst;
				game.replacedCount = 0;
			}

			Item* item = &game.items[game.itemCount++];
			item->entry = NULL;
			item->frame = f->frame;
			item->flipped = f->flipped;
			item->x = x + ox + f->rect.left;
			item->y = y + oy + f->rect.top;
			item->w = f->rect.right - f->rect.left;
			item->h = f->rect.bottom - f->rect.top;
			SetRect(&item->clip, x + l, y + t, x + r, y + b);
			item->replaced = TRUE;
			item->layer = ++game.seq;
			item->look = f->look;
			++game.replacedCount;
			++game.replaced;
			++storeStats.stripItems;
			game.dirty = TRUE;
		}
	}

	BOOL ClaimStore(StoreImage* s, OpenDrawSurface* dst, LONG x, LONG y, LONG w, LONG h, BOOL flipped)
	{
		Entry* entry = s->entry;
		++storeStats.tries;
		if (!Enabled() || !entry || game.itemCount >= MAX_ITEMS)
		{
			++storeStats.blocked;
			return FALSE;
		}

		++entry->draws;
		LONG index = Resolve(entry, (DWORD)s->dims);
		if (index < 0 || pack.frames[index].w != (LONG)s->dims[0] || pack.frames[index].h != (LONG)s->dims[1])
		{
			++storeStats.missed;
			return FALSE;
		}

		Tex* tex = &pack.texs[index];
		Touch(tex, index);
		if (tex->state != TexUploaded)
		{
			++storeStats.waited;
			++game.notReady;
			return FALSE;
		}
		++storeStats.claimed;

		if (game.target != dst)
		{
			game.target = dst;
			game.replacedCount = 0;
		}

		Item* item = &game.items[game.itemCount++];
		item->entry = entry;
		item->frame = index;
		item->flipped = flipped;
		item->x = x;
		item->y = y;
		item->w = s->dims[0];
		item->h = s->dims[1];
		SetRect(&item->clip, x, y, x + w, y + h);
		item->replaced = TRUE;
		item->layer = ++game.seq;
		item->look = StripLook();
		++game.replacedCount;
		++game.replaced;
		++game.skipped;
		++entry->queued;
		++entry->claimed;
		game.dirty = TRUE;
		return TRUE;
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

	GroundChunk* FindRegion(const VOID* data, LONG x, LONG y, LONG w, LONG h)
	{
		for (DWORD i = 0; i < ground.chunkCount; ++i)
		{
			GroundChunk* c = &ground.chunks[i];
			if (c->data == data && x >= c->left && y >= c->top && x + w <= c->left + c->cw && y + h <= c->top + c->ch)
				return c;
		}
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

	VOID FollowChunk(const VOID* srcData, LONG srcPitch, LONG sx, LONG sy, LONG w, LONG h, const VOID* dstData, LONG dx, LONG dy)
	{
		GroundChunk* s = FindRegion(srcData, sx, sy, w, h);
		if (!s || !s->valid || srcData == dstData)
			return;

		if (sx != s->left || sy != s->top || w != s->cw || h != s->ch || HashRegion((const DWORD*)srcData, srcPitch, sx, sy, w, h) != s->hash)
		{
			++ground.partial;
			DropRegions(dstData, dx, dy, w, h, NULL);
			return;
		}

		GroundChunk* d = NULL;
		for (DWORD i = 0; i < ground.chunkCount && !d; ++i)
			if (ground.chunks[i].data == dstData && ground.chunks[i].left == dx && ground.chunks[i].top == dy && ground.chunks[i].cw == w && ground.chunks[i].ch == h)
				d = &ground.chunks[i];
		DropRegions(dstData, dx, dy, w, h, d);
		if (!d)
			d = AddChunk(dstData, dx, dy, w, h);
		if (!d)
			return;

		MemoryCopy(d->ops, s->ops, s->count * sizeof(GroundOp));
		d->count = s->count;
		d->color = s->color;
		d->hash = s->hash;
		d->valid = TRUE;
		++d->version;
		++ground.follows;
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

		if (top || !plain)
			return;
		if (!s)
			++storeStats.faceMisses;
		if (!s || width != (LONG)s->dims[0] || bottom != (LONG)s->dims[1] || !VerifyFace(s, linked, flipped, src, srcPitch, width, bottom))
		{
			DropStripLinks(dst);
			return;
		}

		++storeStats.faceLinks;
		AddFaceLink((const BYTE*)dst, s, flipped != (mirror != 0));
	}

	VOID OnGroundCopy(const VOID* data, const RECT* rect, LONG pitch, const Hooks::BlitObject* obj, const POINT* loc)
	{
		if (!game.initialized)
			return;

		LONG w = rect->right - rect->left, h = rect->bottom - rect->top;
		const BYTE* from = (const BYTE*)((const DWORD*)data + rect->top * pitch + rect->left);
		const BYTE* to = (const BYTE*)((const DWORD*)obj->data + loc->y * (obj->pitch >> 1) + loc->x);
		BOOL flipped = FALSE;
		StoreImage* s = stores.count && obj->isTrueColor ? FindStoreData(from) : NULL;
		StoreImage* f = s || !stores.count || !obj->isTrueColor ? NULL : FindFaceLink(from, &flipped);
		NoteFace(s, from, pitch, w, h);
		if (f && !VerifyFace(f, TRUE, flipped, (const DWORD*)from, pitch, w, h))
			f = NULL;
		if (s && w == (LONG)s->dims[0] && h == (LONG)s->dims[1])
		{
			++storeStats.rawLinks;
			LinkStoreData(s, to);
			s->left = loc->x;
			s->top = loc->y;
		}
		else if (f && w == (LONG)f->dims[0] && h == (LONG)f->dims[1])
		{
			++storeStats.rawLinks;
			AddFaceLink(to, f, flipped);
		}
		else if (stores.count)
			DropStripLinks((const DWORD*)to);

		if (!ground.chunkCount)
			return;

		if (FindRegion(data, rect->left, rect->top, w, h))
			FollowChunk(data, pitch, rect->left, rect->top, w, h, obj->data, loc->x, loc->y);
		else
			ground.written += DropRegions(obj->data, loc->x, loc->y, w, h, NULL);
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

	BOOL ClaimGround(OpenDrawSurface* dst, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect)
	{
		LONG w = srcRect->right - srcRect->left, h = srcRect->bottom - srcRect->top;
		GroundChunk* c = FindRegion(src->indexBuffer->data, srcRect->left, srcRect->top, w, h);
		if (!c || !c->valid || !Enabled() || game.itemCount >= MAX_ITEMS)
			return FALSE;

		if (c->left + c->cw > (LONG)src->mode.width || c->top + c->ch > (LONG)src->mode.height
			|| HashRegion((const DWORD*)src->indexBuffer->data, src->mode.width, c->left, c->top, c->cw, c->ch) != c->hash)
		{
			++ground.stale;
			ResetChunk(c, FALSE, 0);
			c->data = NULL;
			return FALSE;
		}

		if (Config::IsMapZoomed())
		{
			RECT zoom;
			Config::GetMapZoomRect(&zoom);
			if (x < zoom.right && y < zoom.bottom && x + w > zoom.left && y + h > zoom.top)
				return FALSE;
		}

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
		if (!ready)
		{
			++game.notReady;
			return FALSE;
		}

		if (c->pubVersion != c->version)
		{
			EnterCriticalSection(&game.cs);
			MemoryCopy(c->pub, c->ops, c->count * sizeof(GroundOp));
			c->pubCount = c->count;
			c->pubColor = c->color;
			c->pubVersion = c->version;
			c->w = c->cw;
			c->h = c->ch;
			LeaveCriticalSection(&game.cs);
		}

		if (game.target != dst)
		{
			game.target = dst;
			game.replacedCount = 0;
		}

		Item* item = &game.items[game.itemCount++];
		item->entry = NULL;
		item->frame = -1 - (LONG)(c - ground.chunks);
		item->flipped = FALSE;
		item->x = x - (srcRect->left - c->left);
		item->y = y - (srcRect->top - c->top);
		item->w = c->w;
		item->h = c->h;
		SetRect(&item->clip, x, y, x + w, y + h);
		item->replaced = TRUE;
		item->layer = ++game.seq;
		item->look = StripLook();
		++game.replacedCount;
		++game.replaced;
		++game.skipped;
		++ground.claims;
		game.dirty = TRUE;
		return TRUE;
	}

	BOOL OnBlit(OpenDrawSurface* dst, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect, LONG srcStride, BOOL keyed, DWORD key)
	{
		if (!game.initialized || dst == src)
			return FALSE;

		if (ground.chunkCount && src->indexBuffer && dst->indexBuffer)
		{
			if (dst == game.presented)
			{
				if (stores.count && src->indexBuffer->data && FindStoreData((const BYTE*)((const DWORD*)src->indexBuffer->data + srcRect->top * srcStride + srcRect->left)))
					++storeStats.shadowed;
				else if (ClaimGround(dst, x, y, src, srcRect))
					return FALSE;
			}
			else if (FindRegion(src->indexBuffer->data, srcRect->left, srcRect->top, srcRect->right - srcRect->left, srcRect->bottom - srcRect->top))
				FollowChunk(src->indexBuffer->data, src->mode.width, srcRect->left, srcRect->top, srcRect->right - srcRect->left, srcRect->bottom - srcRect->top, dst->indexBuffer->data, x, y);
			else
				ground.written += DropRegions(dst->indexBuffer->data, x, y, srcRect->right - srcRect->left, srcRect->bottom - srcRect->top, NULL);
		}

		LONG w = srcRect->right - srcRect->left, h = srcRect->bottom - srcRect->top;
		if (Claim(dst, x, y, w, h, srcRect->left, srcRect->top, src))
			return TRUE;

		if (stores.count && src->indexBuffer && src->indexBuffer->data)
		{
			const BYTE* base = (const BYTE*)src->indexBuffer->data;
			const DWORD* at = (const DWORD*)base + srcRect->top * srcStride + srcRect->left;
			StoreImage* s = FindStoreData((const BYTE*)at);
			if (!s)
			{
				s = FindStoreData(base);
				if (s && (s->left != srcRect->left || s->top != srcRect->top))
					s = NULL;
			}
			NoteFace(s, at, srcStride, w, h);
			if (s && IsFace(s) && !VerifyFace(s, FALSE, FALSE, at, srcStride, w, h))
				s = NULL;

			BOOL face = FALSE, flipped = FALSE;
			if (!s)
			{
				s = FindFaceLink((const BYTE*)at, &flipped);
				if (s && !VerifyFace(s, TRUE, flipped, at, srcStride, w, h))
					s = NULL;
				face = s != NULL;
			}

			if (s && dst == game.presented)
			{
				++storeStats.finds;
				if (ClaimStore(s, dst, x, y, w, h, flipped))
					return TRUE;
			}
			else if (s && dst->indexBuffer && dst->indexBuffer->data && w == (LONG)s->dims[0] && h == (LONG)s->dims[1])
			{
				++storeStats.blitLinks;
				const BYTE* to = (const BYTE*)((const DWORD*)dst->indexBuffer->data + y * dst->mode.width + x);
				if (face)
					AddFaceLink(to, s, flipped);
				else
				{
					LinkStoreData(s, to);
					s->left = x;
					s->top = y;
				}
			}
			else if (dst != game.presented && dst->indexBuffer && dst->indexBuffer->data)
				DropStripLinks((const DWORD*)dst->indexBuffer->data + y * dst->mode.width + x);
		}

		if (dst == game.target)
		{
			game.dirty = TRUE;
			const DWORD* pixels = (const DWORD*)src->indexBuffer->data + srcRect->top * srcStride + srcRect->left;
			Cover(x, y, w, h, pixels, srcStride, keyed, key, NULL);
		}

		if (strip.linkCount && src->indexBuffer && src->indexBuffer->data)
		{
			const DWORD* from = (const DWORD*)src->indexBuffer->data + srcRect->top * srcStride + srcRect->left;
			if (dst == game.presented)
				ClaimStrip(dst, x, y, from, srcStride, w, h);
			else if (dst->indexBuffer && dst->indexBuffer->data)
				FollowStrip(from, srcStride, w, h, (const DWORD*)dst->indexBuffer->data + y * dst->mode.width + x, dst->mode.width);
		}
		return FALSE;
	}

	VOID OnFill(OpenDrawSurface* surface)
	{
		if (game.initialized && surface == game.target)
			game.dirty = TRUE;
	}

	VOID OnRelease(OpenDrawSurface* surface)
	{
		if (game.initialized && ground.chunkCount && surface->indexBuffer && surface->indexBuffer->data)
			DropRegions(surface->indexBuffer->data, 0, 0, 0x7FFF, 0x7FFF, NULL);

		if (!game.initialized || surface != game.target)
			return;

		game.target = NULL;
		game.itemCount = game.replacedCount = 0;
		ResetExpect();
	}

	VOID OnFillRect(const VOID* data, const RECT* rect)
	{
		LONG ox, oy;
		if (!game.initialized || !InTarget(data, &ox, &oy))
			return;

		game.dirty = TRUE;
		Cover(ox + rect->left, oy + rect->top, rect->right - rect->left, rect->bottom - rect->top, NULL, 0, FALSE, 0, NULL);
	}

	VOID OnDiamond(const VOID* data, const POINT* loc, const RECT* rect)
	{
		LONG ox, oy;
		if (!game.initialized || !InTarget(data, &ox, &oy))
			return;

		game.dirty = TRUE;
		LONG l = max(loc->x, rect->left), t = max(loc->y, rect->top);
		LONG r = min(loc->x + 64, rect->right), b = min(loc->y + 32, rect->bottom);
		if (l < r && t < b)
			Cover(ox + l, oy + t, r - l, b - t, NULL, 0, FALSE, 0, NULL);
	}

	VOID OnPixelCopy(const DWORD* srcData, LONG srcPitch, const POINT* srcPos, const DWORD* dstData, LONG dstPitch, const POINT* dstPos, const SIZE* size, BOOL keyed, DWORD key, DWORD alpha)
	{
		if (!game.initialized || !dstData || !srcData || size->cx <= 0 || size->cy <= 0)
			return;

		srcPitch /= 2;
		dstPitch /= 2;
		const DWORD* dst = dstData + dstPitch * dstPos->y + dstPos->x;
		LONG dx, dy;
		BOOL fromChunk = ground.chunkCount && FindRegion(srcData, srcPos->x, srcPos->y, size->cx, size->cy);
		if (!InTarget(dst, &dx, &dy))
		{
			if (fromChunk)
				FollowChunk(srcData, srcPitch, srcPos->x, srcPos->y, size->cx, size->cy, dstData, dstPos->x, dstPos->y);
			const BYTE* from = (const BYTE*)(srcData + srcPitch * srcPos->y + srcPos->x);
			BOOL flipped = FALSE;
			StoreImage* s = stores.count ? FindStoreData(from) : NULL;
			StoreImage* f = s || !stores.count ? NULL : FindFaceLink(from, &flipped);
			NoteFace(s, from, srcPitch, size->cx, size->cy);
			if (f && !VerifyFace(f, TRUE, flipped, (const DWORD*)from, srcPitch, size->cx, size->cy))
				f = NULL;
			if (s && size->cx == (LONG)s->dims[0] && size->cy == (LONG)s->dims[1])
			{
				++storeStats.pixelLinks;
				LinkStoreData(s, (const BYTE*)dst);
				s->left = dstPos->x;
				s->top = dstPos->y;
			}
			else if (f && size->cx == (LONG)f->dims[0] && size->cy == (LONG)f->dims[1])
			{
				++storeStats.pixelLinks;
				AddFaceLink((const BYTE*)dst, f, flipped);
			}
			else if (stores.count)
				DropStripLinks(dst);
			FollowStrip((const DWORD*)from, srcPitch, size->cx, size->cy, dst, dstPitch);
			return;
		}

		if (fromChunk)
			++ground.direct;
		game.dirty = TRUE;
		if (alpha >= 128)
			Cover(dx, dy, size->cx, size->cy, srcData + srcPitch * srcPos->y + srcPos->x, srcPitch, keyed, key, NULL);
		if (game.target == game.presented)
			ClaimStrip(game.target, dx, dy, srcData + srcPitch * srcPos->y + srcPos->x, srcPitch, size->cx, size->cy);
		else
			FollowStrip(srcData + srcPitch * srcPos->y + srcPos->x, srcPitch, size->cx, size->cy, dst, dstPitch);
	}

	VOID OnPixelRows(const Hooks::BlendData* items, DWORD count)
	{
		if (!game.initialized)
			return;

		for (DWORD i = 0; i < count; ++i)
		{
			const Hooks::BlendData* item = &items[i];
			if (!item->length || !item->dstData)
				continue;

			LONG x, y;
			if (!InTarget(item->dstData, &x, &y))
				continue;

			game.dirty = TRUE;
			Cover(x, y, item->length, 1, NULL, 0, FALSE, 0, item->mskData);
		}
	}

	DWORD DrawRank(const DrawItem* d)
	{
		static const PackAnim* water;
		if (!water)
			water = FindAnim("WATERBKGND1");
		if (d->frame < 0)
			return 1;
		return water && (DWORD)d->frame >= water->first && (DWORD)d->frame < water->first + water->count ? 0 : 2;
	}

	BOOL SlotReserve(Slot* slot, DWORD count)
	{
		if (count <= (slot->items ? MAX_ITEMS : 0))
			return TRUE;
		slot->items = (DrawItem*)MemoryAlloc(MAX_ITEMS * sizeof(DrawItem));
		return slot->items != NULL;
	}

	BOOL dumpItems;

	VOID DumpItems(OpenDrawSurface* back)
	{
		dumpItems = FALSE;
		DebugLog("image items target=%d count=%d replaced=%d dirty=%d", back == game.target, game.itemCount, game.replacedCount, game.dirty);
		for (DWORD i = 0; i < game.itemCount; ++i)
		{
			const Item* item = &game.items[i];
			if (!item->replaced || item->clip.top >= 160)
				continue;
			DebugLog("image item %d frame=%d name=%s at=%d,%d size=%dx%d clip=%d,%d,%d,%d layer=%d gem=%d",
				i, item->frame, item->entry && item->entry->name[0] ? item->entry->name : (item->frame < 0 ? "ground" : "strip/face"),
				item->x, item->y, item->w, item->h, item->clip.left, item->clip.top, item->clip.right, item->clip.bottom, item->layer, item->look.gem);
		}
	}

	VOID OnPresent(OpenDrawSurface* back, StateBuffer* frame, LONG left, LONG top)
	{
		if (!game.initialized)
			return;

		if (dumpItems)
			DumpItems(back);
		game.presented = back;
		InterlockedIncrement(&game.frameNo);
		if (back != game.target)
		{
			game.itemCount = game.replacedCount = 0;
			ResetExpect();
			return;
		}

		EnterCriticalSection(&game.cs);
		{
			Slot* slot = NULL;
			for (DWORD i = 0; i < SLOTS && !slot; ++i)
				if (game.slots[i].frame == frame)
					slot = &game.slots[i];

			if (!slot)
			{
				slot = &game.slots[0];
				for (DWORD i = 1; i < SLOTS; ++i)
					if (game.slots[i].age < slot->age)
						slot = &game.slots[i];
			}

			Slot* newest = NULL;
			for (DWORD i = 0; i < SLOTS; ++i)
				if (&game.slots[i] != slot && game.slots[i].frame && (!newest || game.slots[i].age > newest->age))
					newest = &game.slots[i];

			slot->frame = frame;
			slot->age = ++game.slotAge;

			if (!game.dirty)
			{
				slot->count = 0;
				if (newest && newest->count && SlotReserve(slot, newest->count))
				{
					MemoryCopy(slot->items, newest->items, newest->count * sizeof(DrawItem));
					slot->count = newest->count;
					if (newest->plane && (!slot->plane || slot->planeW != newest->planeW || slot->planeH != newest->planeH))
					{
						if (slot->plane)
							MemoryFree(slot->plane);
						slot->plane = (DWORD*)MemoryAlloc(newest->planeW * newest->planeH * sizeof(DWORD));
						slot->planeW = slot->plane ? newest->planeW : 0;
						slot->planeH = slot->plane ? newest->planeH : 0;
					}
					if (slot->plane && newest->plane)
						MemoryCopy(slot->plane, newest->plane, slot->planeW * slot->planeH * sizeof(DWORD));
				}
			}
			else
			{
				slot->count = 0;
				if (game.replacedCount && SlotReserve(slot, game.replacedCount))
				{
					for (DWORD i = 0; i < game.itemCount; ++i)
					{
						const Item* item = &game.items[i];
						if (!item->replaced)
							continue;

						DrawItem* d = &slot->items[slot->count++];
						d->frame = item->frame;
						d->x = item->x - left;
						d->y = item->y - top;
						d->w = item->w;
						d->h = item->h;
						SetRect(&d->clip, item->clip.left - left, item->clip.top - top, item->clip.right - left, item->clip.bottom - top);
						d->flipped = item->flipped;
						d->layer = item->layer;
						d->look = item->look;

						for (DrawItem* p = d; p > slot->items && (DrawRank(p) < DrawRank(p - 1) || (DrawRank(p) == DrawRank(p - 1) && p->layer < (p - 1)->layer)); --p)
						{
							DrawItem swap = *p;
							*p = *(p - 1);
							*(p - 1) = swap;
						}
					}

					LONG width = config.mode->width, height = config.mode->height;
					if (!slot->plane || slot->planeW != width || slot->planeH != height)
					{
						if (slot->plane)
							MemoryFree(slot->plane);
						slot->plane = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
						slot->planeW = slot->plane ? width : 0;
						slot->planeH = slot->plane ? height : 0;
					}

					if (slot->plane)
					{
						for (LONG y = 0; y < height; ++y)
							for (LONG x = 0; x < width; ++x)
							{
								LONG px = x + left, py = y + top;
								slot->plane[y * width + x] = game.plane && px >= 0 && py >= 0 && px < game.planeW && py < game.planeH ? game.plane[py * game.planeW + px] : 0;
							}
					}
				}
			}
		}
		LeaveCriticalSection(&game.cs);

		game.itemCount = game.replacedCount = 0;
		ResetExpect();
		game.dirty = FALSE;
	}
#pragma endregion

#pragma region Render
	BOOL Prepare(StateBufferAligned* frame, BOOL active)
	{
		if (!game.initialized)
			return FALSE;

		DWORD before = drawList.count;
		drawList.count = 0;
		if (active)
		{
			EnterCriticalSection(&game.cs);
			for (DWORD i = 0; i < SLOTS; ++i)
			{
				Slot* slot = &game.slots[i];
				if (slot->frame != (StateBuffer*)frame || !slot->count)
					continue;

				if (drawList.capacity < slot->count)
				{
					if (drawList.items)
						MemoryFree(drawList.items);
					drawList.items = (DrawItem*)MemoryAlloc(MAX_ITEMS * sizeof(DrawItem));
					drawList.capacity = drawList.items ? MAX_ITEMS : 0;
				}

				if (drawList.items && slot->plane)
				{
					MemoryCopy(drawList.items, slot->items, slot->count * sizeof(DrawItem));
					drawList.count = slot->count;

					if (!drawList.plane || drawList.planeW != slot->planeW || drawList.planeH != slot->planeH)
					{
						if (drawList.plane)
							MemoryFree(drawList.plane);
						drawList.plane = (DWORD*)MemoryAlloc(slot->planeW * slot->planeH * sizeof(DWORD));
						drawList.planeW = drawList.plane ? slot->planeW : 0;
						drawList.planeH = drawList.plane ? slot->planeH : 0;
					}
					if (drawList.plane)
						MemoryCopy(drawList.plane, slot->plane, slot->planeW * slot->planeH * sizeof(DWORD));
					else
						drawList.count = 0;
				}
				break;
			}
			LeaveCriticalSection(&game.cs);
		}

		return drawList.count || before || pack.decodedList.count;
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

		const CHAR* fragment =
			"uniform sampler2D tex01;\n"
			"uniform sampler2D tex02;\n"
			"uniform vec4 shape;\n"
			"uniform vec4 tint;\n"
			"uniform vec4 rim;\n"
			"in vec2 fTex;\n"
			"in vec2 fPlane;\n"
			"in float fLayer;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	if (texture(tex02, fPlane).r > fLayer + 0.5)\n"
			"		discard;\n"
			"	vec4 c = texture(tex01, fTex);\n"
			"	if (shape.z > 0.0)\n"
			"	{\n"
			"		float n = abs(fTex.x - shape.x) / shape.z + abs(fTex.y - shape.y) / shape.w;\n"
			"		if (n > 1.0)\n"
			"			discard;\n"
			"		vec3 p = c.a > 0.0 ? c.rgb / c.a : vec3(0.0);\n"
			"		float s = p.r + p.g + p.b;\n"
			"		if (tint.a > 0.0 && s > 250.0 / 255.0 && abs(p.r - p.b) < 60.0 / 255.0)\n"
			"			p = min(tint.rgb * s * 13.0 / 30.0, vec3(1.0));\n"
			"		c = vec4(n > 1.0 - rim.a ? rim.rgb : p, 1.0);\n"
			"	}\n"
			"	FRAG_COLOR = c;\n"
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

		gl.program = Link(vertex, fragment);
		GLUseProgram(gl.program);
		GLUniform1i(GLGetUniformLocation(gl.program, "tex01"), 0);
		GLUniform1i(GLGetUniformLocation(gl.program, "tex02"), 1);
		gl.shape = GLGetUniformLocation(gl.program, "shape");
		gl.tint = GLGetUniformLocation(gl.program, "tint");
		gl.rim = GLGetUniformLocation(gl.program, "rim");

		groundGL.program = Link(vertex, groundFragment);
		GLUseProgram(groundGL.program);
		GLUniform1i(GLGetUniformLocation(groundGL.program, "tex01"), 0);
		GLUniform1i(GLGetUniformLocation(groundGL.program, "tex02"), 1);
		GLUniform1i(GLGetUniformLocation(groundGL.program, "tex03"), 2);
		groundGL.mode = GLGetUniformLocation(groundGL.program, "mode");
		groundGL.two = GLGetUniformLocation(groundGL.program, "two");
		GLGenFramebuffers(1, &groundGL.fbo);

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

		GLGenTextures(1, &gl.planeTexture);
		GLBindTexture(GL_TEXTURE_2D, gl.planeTexture);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		gl.planeW = gl.planeH = 0;

		gl.ready = gl.program && gl.vao && gl.vbo && gl.planeTexture && groundGL.program && groundGL.fbo;
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

	VOID RenderGround(DWORD index)
	{
		GroundChunk* c = &ground.chunks[index];
		GroundTarget* g = &groundGL.chunks[index];
		DWORD count = 0, color = 0;
		LONG w = 0, h = 0;

		EnterCriticalSection(&game.cs);
		BOOL stale = !g->texture || g->version != c->pubVersion;
		if (stale)
		{
			count = c->pubCount;
			MemoryCopy(groundGL.ops, c->pub, count * sizeof(GroundOp));
			color = c->pubColor;
			w = c->w;
			h = c->h;
			g->version = c->pubVersion;
		}
		LeaveCriticalSection(&game.cs);
		if (!stale || w <= 0 || h <= 0)
			return;

		GLActiveTexture(GL_TEXTURE0);
		if (!g->texture || g->w != w || g->h != h)
		{
			if (!g->texture)
				GLGenTextures(1, &g->texture);
			GLBindTexture(GL_TEXTURE_2D, g->texture);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w * GROUND_SCALE, h * GROUND_SCALE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
			g->w = w;
			g->h = h;
		}

		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, groundGL.fbo);
		GLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g->texture, 0);
		GLViewport(0, 0, w * GROUND_SCALE, h * GROUND_SCALE);
		FLOAT alpha = (FLOAT)(color >> 24) / 255.0f;
		GLClearColor((FLOAT)(color & 0xFF) / 255.0f * alpha, (FLOAT)((color >> 8) & 0xFF) / 255.0f * alpha, (FLOAT)((color >> 16) & 0xFF) / 255.0f * alpha, alpha);
		GLClear(GL_COLOR_BUFFER_BIT);

		GLUseProgram(groundGL.program);
		GLBindVertexArray(gl.vao);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		glEnable(GL_SCISSOR_TEST);

		for (DWORD i = 0; i < count; ++i)
		{
			const GroundOp* op = &groundGL.ops[i];
			LONG list[] = { op->tex, op->mask1, op->mask2 };
			BOOL ready = TRUE;
			for (DWORD k = 0; k < 3; ++k)
				ready = ready && (list[k] < 0 || pack.texs[list[k]].state == TexUploaded);
			if (!ready)
			{
				g->version = 0;
				continue;
			}

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
			glScissor(op->left * GROUND_SCALE, op->top * GROUND_SCALE, (op->right - op->left) * GROUND_SCALE, (op->bottom - op->top) * GROUND_SCALE);

			FLOAT fw = 1.0f, fh = 1.0f;
			if (op->tex >= 0 && op->kind != GroundBorder)
			{
				fw = (FLOAT)pack.frames[op->tex].w;
				fh = (FLOAT)pack.frames[op->tex].h;
			}
			FLOAT nx0 = (FLOAT)op->dx / w * 2.0f - 1.0f, nx1 = (FLOAT)(op->dx + 64) / w * 2.0f - 1.0f;
			FLOAT ny0 = (FLOAT)op->dy / h * 2.0f - 1.0f, ny1 = (FLOAT)(op->dy + 32) / h * 2.0f - 1.0f;
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

		glDisable(GL_SCISSOR_TEST);
		GLDisable(GL_BLEND);
	}

	VOID Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight)
	{
		BOOL visible = drawList.count && rect->width > 0 && rect->height > 0;
		if (!pack.ready || (!visible && !pack.decodedList.count && pack.gpu <= pack.gpuBudget))
			return;

		GLint program, vao, buffer, active, texture0, texture1;
		GLGetIntegerv(GL_CURRENT_PROGRAM, &program);
		GLGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		GLGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		GLGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		GLActiveTexture(GL_TEXTURE1);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture1);
		GLActiveTexture(GL_TEXTURE0);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);

		if (InitGL())
			Manage();

		if (gl.ready && visible)
		{
			BOOL grounds = FALSE;
			for (DWORD i = 0; i < drawList.count && !grounds; ++i)
				grounds = drawList.items[i].frame < 0;
			if (grounds)
			{
				GLint framebuffer, texture2;
				GLfloat clear[4];
				GLGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
				glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
				GLActiveTexture(GL_TEXTURE2);
				GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2);

				for (DWORD i = 0; i < drawList.count; ++i)
					if (drawList.items[i].frame < 0)
						RenderGround(-1 - drawList.items[i].frame);

				GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
				GLClearColor(clear[0], clear[1], clear[2], clear[3]);
				GLActiveTexture(GL_TEXTURE2);
				GLBindTexture(GL_TEXTURE_2D, texture2);
			}

			DWORD base = 0xFFFFFFFF;
			for (DWORD i = 0; i < drawList.count; ++i)
				base = min(base, drawList.items[i].layer);
			--base;

			LONG pw = drawList.planeW, ph = drawList.planeH;
			if (!gl.planeData || gl.planeW != pw || gl.planeH != ph)
			{
				if (gl.planeData)
					MemoryFree(gl.planeData);
				gl.planeData = (FLOAT*)MemoryAlloc(pw * ph * sizeof(FLOAT));
			}

			GLActiveTexture(GL_TEXTURE1);
			GLBindTexture(GL_TEXTURE_2D, gl.planeTexture);
			if (gl.planeData)
			{
				for (LONG i = pw * ph - 1; i >= 0; --i)
				{
					DWORD v = drawList.plane[i];
					gl.planeData[i] = v > base ? (FLOAT)(v - base) : 0.0f;
				}

				GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
				GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				if (gl.planeW != pw || gl.planeH != ph)
				{
					gl.planeW = pw;
					gl.planeH = ph;
					GLTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, pw, ph, 0, GL_RED, GL_FLOAT, gl.planeData);
				}
				else
					GLTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pw, ph, GL_RED, GL_FLOAT, gl.planeData);
			}

			GLUseProgram(gl.program);
			GLUniform4f(gl.shape, 0.0f, 0.0f, 0.0f, 0.0f);
			BOOL shaped = FALSE;
			GLBindVertexArray(gl.vao);
			GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
			GLViewport(rect->x, rect->y + offset, rect->width, rect->height);
			GLEnable(GL_BLEND);
			GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

			FLOAT sx = (FLOAT)rect->width / frameWidth;
			FLOAT sy = (FLOAT)rect->height / frameHeight;
			FLOAT baseX = (FLOAT)((LONG)(config.mode->width - frameWidth) >> 1);
			FLOAT baseY = (FLOAT)((LONG)(config.mode->height - frameHeight) >> 1);
			FLOAT rw = (FLOAT)rect->width, rh = (FLOAT)rect->height;

			for (DWORD i = 0; i < drawList.count && gl.planeData; ++i)
			{
				const DrawItem* item = &drawList.items[i];
				GLuint id;
				if (item->frame < 0)
				{
					id = groundGL.chunks[-1 - item->frame].texture;
					if (!id)
						continue;
				}
				else
				{
					Tex* tex = &pack.texs[item->frame];
					if (tex->state != TexUploaded)
						continue;
					TouchGpu(item->frame);
					id = tex->id;
				}

				LONG l = max(item->x, item->clip.left), t = max(item->y, item->clip.top);
				LONG r = min(item->x + item->w, item->clip.right), b = min(item->y + item->h, item->clip.bottom);
				if (l >= r || t >= b)
					continue;

				FLOAT u0 = (FLOAT)(l - item->x) / item->w, u1 = (FLOAT)(r - item->x) / item->w;
				FLOAT v0 = (FLOAT)(t - item->y) / item->h, v1 = (FLOAT)(b - item->y) / item->h;
				if (item->flipped)
				{
					u0 = 1.0f - u0;
					u1 = 1.0f - u1;
				}

				const RECT* part = &item->look.part;
				BOOL gem = FALSE;
				if (item->frame >= 0 && part->right > part->left)
				{
					const PackFrame* pf = &pack.frames[item->frame];
					FLOAT su = (FLOAT)part->left / pf->w, sw = (FLOAT)(part->right - part->left) / pf->w;
					FLOAT sv = (FLOAT)part->top / pf->h, sh = (FLOAT)(part->bottom - part->top) / pf->h;
					u0 = su + u0 * sw;
					u1 = su + u1 * sw;
					v0 = sv + v0 * sh;
					v1 = sv + v1 * sh;

					if (item->look.gem)
					{
						FLOAT rx = item->w / 2.0f, ry = item->h / 2.0f;
						GLUniform4f(gl.shape, su + sw / 2.0f, sv + sh / 2.0f, sw / 2.0f, sh / 2.0f);
						GLUniform4f(gl.tint, GetRValue(item->look.tint) / 255.0f, GetGValue(item->look.tint) / 255.0f, GetBValue(item->look.tint) / 255.0f, item->look.tint ? 1.0f : 0.0f);
						GLUniform4f(gl.rim, GetRValue(item->look.rim) / 255.0f, GetGValue(item->look.rim) / 255.0f, GetBValue(item->look.rim) / 255.0f, sqrtf(1.0f / (rx * rx) + 1.0f / (ry * ry)));
						gem = TRUE;
					}
				}
				if (shaped && !gem)
					GLUniform4f(gl.shape, 0.0f, 0.0f, 0.0f, 0.0f);
				shaped = gem;

				FLOAT x0 = (l - baseX) * sx, x1 = (r - baseX) * sx;
				FLOAT y0 = (t - baseY) * sy, y1 = (b - baseY) * sy;
				FLOAT nx0 = x0 / rw * 2.0f - 1.0f, nx1 = x1 / rw * 2.0f - 1.0f;
				FLOAT ny0 = 1.0f - y0 / rh * 2.0f, ny1 = 1.0f - y1 / rh * 2.0f;
				FLOAT pu0 = (FLOAT)l / pw, pu1 = (FLOAT)r / pw;
				FLOAT pv0 = (FLOAT)t / ph, pv1 = (FLOAT)b / ph;
				FLOAT layer = (FLOAT)(item->layer - base);

				Vertex quad[6] = {
					{ nx0, ny0, u0, v0, pu0, pv0, layer },
					{ nx1, ny0, u1, v0, pu1, pv0, layer },
					{ nx1, ny1, u1, v1, pu1, pv1, layer },
					{ nx0, ny0, u0, v0, pu0, pv0, layer },
					{ nx1, ny1, u1, v1, pu1, pv1, layer },
					{ nx0, ny1, u0, v1, pu0, pv1, layer }
				};

				GLActiveTexture(GL_TEXTURE0);
				GLBindTexture(GL_TEXTURE_2D, id);
				GLBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STREAM_DRAW);
				GLDrawArrays(GL_TRIANGLES, 0, 6);
			}

			GLDisable(GL_BLEND);
		}

		GLActiveTexture(GL_TEXTURE1);
		GLBindTexture(GL_TEXTURE_2D, texture1);
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

			GLDeleteTextures(1, &gl.planeTexture);
			GLDeleteBuffers(1, &gl.vbo);
			GLDeleteVertexArrays(1, &gl.vao);
			GLDeleteProgram(gl.program);

			for (DWORD i = 0; i < GROUND_CHUNKS; ++i)
				if (groundGL.chunks[i].texture)
					GLDeleteTextures(1, &groundGL.chunks[i].texture);
			GLDeleteFramebuffers(1, &groundGL.fbo);
			GLDeleteProgram(groundGL.program);
		}
		MemoryZero(groundGL.chunks, sizeof(groundGL.chunks));
		groundGL.program = groundGL.fbo = 0;

		gl.ready = FALSE;
		gl.failed = FALSE;
		gl.program = gl.vao = gl.vbo = gl.planeTexture = 0;
		gl.planeW = gl.planeH = 0;
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
			c->hash = HashRegion((const DWORD*)obj->data, obj->pitch >> 1, 0, 0, c->cw, c->ch);
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
		DWORD frames, mapped, sizeMiss, draws, queued, claimed;
	};

	const CHAR* SummaryStatus(const Summary* s)
	{
		if (!s->first->anim)
			return "no-pack";
		if (s->claimed)
			return "4k";
		if (s->queued)
			return "unmatched";
		if (s->sizeMiss)
			return "size-mismatch";
		if (s->mapped)
			return "not-ready";
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
			s->queued += e->queued;
			s->claimed += e->claimed;
		}

		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, sizeof(path));
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, "imagecoverage.txt");

		HANDLE file = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (file != INVALID_HANDLE_VALUE)
		{
			const CHAR* order[] = { "unmatched", "size-mismatch", "not-ready", "frame-range", "no-pack", "4k" };
			CHAR line[256];
			DWORD written;
			for (DWORD k = 0; k < sizeof(order) / sizeof(*order); ++k)
				for (DWORD i = 0; i < TABLE_SIZE; ++i)
				{
					const Summary* s = &list[i];
					if (!s->first || StrCompare(SummaryStatus(s), order[k]))
						continue;

					const DWORD* dims = (const DWORD*)s->first->rec;
					DWORD n = StrPrint(line, "%-13s %-24s %4dx%-4d frames %d mapped %d draws %d queued %d claimed %d pack %s\r\n",
						order[k], s->first->owner, dims[0], dims[1], s->frames, s->mapped, s->draws, s->queued, s->claimed,
						s->first->anim ? pack.names + s->first->anim->id : "-");
					WriteFile(file, line, n, &written, NULL);
				}
			CloseHandle(file);
		}

		MemoryFree(list);
	}

	VOID LogStats()
	{
		dumpItems = TRUE;
		MapOrder::Dump();
		WriteCoverage();
		EnterCriticalSection(&requests.cs);
		DWORD demand = (requests.demand.tail + REQUEST_SIZE - requests.demand.head) % REQUEST_SIZE;
		DWORD prefetch = (requests.prefetch.tail + REQUEST_SIZE - requests.prefetch.head) % REQUEST_SIZE;
		DWORD decodedCount = pack.decodedList.count;
		LeaveCriticalSection(&requests.cs);

		DebugLog("image stats builds=%d draws=%d unknown=%d replaced=%d skipped=%d notReady=%d ground chunks=%d builds=%d ops=%d unnamed=%d unmasked=%d rejected=%d written=%d follows=%d partial=%d direct=%d stale=%d claims=%d",
			stats.builds, stats.draws, stats.unknown, game.replaced, game.skipped, game.notReady,
			ground.chunkCount, ground.builds, ground.ops, ground.unnamed, ground.unmasked, ground.rejected, ground.written, ground.follows, ground.partial, ground.direct, ground.stale, ground.claims);
		DebugLog("image store tries=%d blocked=%d missed=%d waited=%d claimed=%d shadowed=%d drawLinks=%d blitLinks=%d pixelLinks=%d rawLinks=%d faceLinks=%d finds=%d faceLoads=%d",
			storeStats.tries, storeStats.blocked, storeStats.missed, storeStats.waited, storeStats.claimed, storeStats.shadowed, storeStats.drawLinks, storeStats.blitLinks, storeStats.pixelLinks, storeStats.rawLinks, storeStats.faceLinks, storeStats.finds, storeStats.faceLoads);
		DebugLog("image strip faceCopies=%d faceMisses=%d faceRejects=%d takes=%d takeLinks=%d takeFrames=%d hits=%d away=%d follows=%d claims=%d items=%d",
			storeStats.faceCopies, storeStats.faceMisses, storeStats.faceRejects, storeStats.takes, storeStats.takeLinks, storeStats.takeFrames, storeStats.stripHits, storeStats.stripAway, storeStats.stripFollows, storeStats.stripClaims, storeStats.stripItems);
		DebugLog("image tables entries=%d/%d full=%d stores=%d/%d archives=%d queue demand=%d prefetch=%d decoded=%d (%d MB) gpu=%d (%d MB of %d MB budget)",
			entries.count, entries.used, stats.full, stores.count, stores.used, archives.count, demand, prefetch,
			decodedCount, pack.decoded >> 20, pack.gpuList.count, pack.gpu >> 20, pack.gpuBudget >> 20);
	}

	VOID __fastcall Render(VOID* thisObj, DWORD /*edx*/, VOID* renderer, const POINT* start, const POINT* offset, const POINT* size, const RECT* area)
	{
		((VOID(__thiscall*)(VOID*, VOID*, const POINT*, const POINT*, const POINT*, const RECT*))sub_Render)(thisObj, renderer, start, offset, size, area);
		++stats.draws;

		DWORD rec = GetRecord(thisObj);
		Entry* entry = rec ? Find(rec, FALSE) : NULL;
		if (!entry || !entry->name[0])
			++stats.unknown;
		else
		{
			++entry->draws;
			Queue(entry, rec, start, offset, size, area);
		}

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
