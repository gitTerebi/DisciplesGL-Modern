#include "StdAfx.h"
#include "Config.h"
#include "GLib.h"
#include "Hooks.h"
#include "ImageReplace.h"
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

namespace TextReplace
{
	extern DWORD traceLeft;
}

namespace ImageReplace
{
	#define TABLE_BITS 16
	#define TABLE_SIZE (1 << TABLE_BITS)
	#define NAME_LENGTH 40
	#define FIRST_DRAW_LIMIT 4000
	#define TRACE_MS 50
	#define FLAG_CHECK_MS 1000

	#define ANIM_BITS 15
	#define ANIM_SIZE (1 << ANIM_BITS)
	#define MAX_ITEMS 1024
	#define MAX_EXPECT 4096
	#define EXPECT_SCAN 64
	#define SLOTS 6
	#define REQUEST_SIZE 4096
	#define MEMORY_LIMIT (768u * 1024 * 1024)

	enum TexState { TexNone, TexQueued, TexDecoded, TexUploaded, TexFailed };

	struct Entry {
		DWORD rec;
		BYTE flipped;
		BYTE drawn;
		WORD frame;
		LONG packFrame;
		BOOL resolved;
		CHAR name[NAME_LENGTH];
		CHAR owner[NAME_LENGTH];
	};

	struct PackFrame {
		LONGLONG offset;
		DWORD length;
		LONG w, h, ox, oy;
	};

	struct PackAnim {
		DWORD hash;
		DWORD name;
		DWORD first;
		DWORD count;
	};

	struct Tex {
		volatile LONG state;
		BYTE* pixels;
		LONG w, h;
		GLuint id;
	};

	struct Item {
		LONG frame;
		LONG x, y, w, h;
		RECT clip;
		BOOL flipped;
		BOOL replaced;
		DWORD layer;
	};

	struct Expect {
		LONG x, y, w, h;
		DWORD item;
	};

	struct DrawItem {
		LONG frame;
		LONG x, y, w, h;
		RECT clip;
		BOOL flipped;
		DWORD layer;
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

	Entry table[TABLE_SIZE];

	DWORD sub_Build, sub_Render, sub_Names;

	struct {
		CHAR name[NAME_LENGTH];
		DWORD frame;
	} current;

	struct {
		DWORD builds;
		DWORD records;
		DWORD full;
		DWORD draws;
		DWORD unknown;
		DWORD firstDraws;
	} stats;

	struct {
		BOOL key;
		DWORD until;
		DWORD flagCheck;
	} trace;

	struct {
		HANDLE file;
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
		volatile LONG memory;
		BOOL memoryLogged;
	} pack;

	struct {
		CRITICAL_SECTION cs;
		DWORD queue[REQUEST_SIZE];
		DWORD head, tail;
	} requests;

	struct {
		BOOL initialized;
		CRITICAL_SECTION cs;
		OpenDrawSurface* target;
		DWORD* plane;
		LONG planeW, planeH;
		DWORD seq;
		BOOL dirty;
		Item items[MAX_ITEMS];
		DWORD itemCount;
		DWORD replacedCount;
		Expect expect[MAX_EXPECT];
		DWORD expectHead, expectCount;
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

	struct {
		BOOL ready;
		BOOL failed;
		GLuint program;
		GLuint vao;
		GLuint vbo;
		GLuint planeTexture;
		LONG planeW, planeH;
		FLOAT* planeData;
		Vertex* vertices;
		DWORD vertexCapacity;
		GLGENERATEMIPMAP generateMipmap;
	} gl;

	BOOL TakeFlag()
	{
		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, sizeof(path));
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, "imagetrace.flag");
		return DeleteFile(path);
	}

	Entry* Find(DWORD rec, BOOL insert)
	{
		DWORD index = (rec * 2654435761u) >> (32 - TABLE_BITS);
		for (DWORD i = 0; i < TABLE_SIZE; ++i)
		{
			Entry* entry = &table[(index + i) & (TABLE_SIZE - 1)];
			if (entry->rec == rec)
				return entry;
			if (!entry->rec)
			{
				if (!insert)
					return NULL;
				entry->rec = rec;
				++stats.records;
				return entry;
			}
		}

		++stats.full;
		return NULL;
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

	BOOL Wanted(const CHAR* id, DWORD length)
	{
		const CHAR* prefix = "battle-unit/";
		DWORD n = StrLength(prefix);
		return length > n && !MemoryCompare(id, prefix, n);
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

	VOID ParseFrame(Json* j, PackFrame* frame)
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
			{
				if (!Eat(j, '{'))
				{
					j->fail = TRUE;
					return;
				}

				if (!Eat(j, '}'))
				{
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
						else
							Skip(j);
					} while (!j->fail && Eat(j, ','));

					if (!Eat(j, '}'))
						j->fail = TRUE;
				}
			}
			else if (Is(key, length, "Offset"))
				frame->offset = Num(j);
			else if (Is(key, length, "Length"))
				frame->length = (DWORD)Num(j);
			else
				Skip(j);
		} while (!j->fail && Eat(j, ','));

		if (!Eat(j, '}'))
			j->fail = TRUE;
	}

	VOID InsertAnim(DWORD hash, DWORD name, DWORD first, DWORD count)
	{
		if (pack.animCount >= ANIM_SIZE * 3 / 4)
			return;

		DWORD index = hash & (ANIM_SIZE - 1);
		while (pack.anims[index].hash)
			index = (index + 1) & (ANIM_SIZE - 1);

		PackAnim* anim = &pack.anims[index];
		anim->hash = hash;
		anim->name = name;
		anim->first = first;
		anim->count = count;
		++pack.animCount;
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
		do
		{
			const CHAR* key;
			DWORD length;
			if (!Key(j, &key, &length))
				return;

			if (Is(key, length, "Id"))
				Str(j, &id, &idLength);
			else if (Is(key, length, "Frames") && id && Wanted(id, idLength))
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
						ParseFrame(j, &pack.frames[pack.frameCount++]);
					} while (!j->fail && Eat(j, ','));

					if (!Eat(j, ']'))
						j->fail = TRUE;
				}

				DWORD name;
				if (!j->fail && AddName(id, idLength, &name))
					InsertAnim(HashText(id, idLength), name, first, pack.frameCount - first);
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

			if (Is(key, length, "Animations"))
			{
				if (!Eat(&j, '['))
					return FALSE;
				if (!Eat(&j, ']'))
				{
					do
						ParseAnim(&j);
					while (!j.fail && Eat(&j, ','));

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
	BOOL ReadAt(LONGLONG offset, VOID* buffer, DWORD size)
	{
		OVERLAPPED ov = {};
		ov.Offset = (DWORD)offset;
		ov.OffsetHigh = (DWORD)(offset >> 32);
		DWORD read = 0;
		return ReadFile(pack.file, buffer, size, &read, &ov) && read == size;
	}

	BOOL OpenPack()
	{
		CHAR path[MAX_PATH];
		DWORD length = GetModuleFileName(NULL, path, sizeof(path));
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, "assets-v1.galleanpack");

		pack.file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
		if (pack.file == INVALID_HANDLE_VALUE)
		{
			pack.file = NULL;
			DebugLog("image pack not found at %s", path);
			return FALSE;
		}

		BYTE header[32];
		if (!ReadAt(0, header, sizeof(header)) || MemoryCompare(header, "GALLEANPACK", 11) || *(LONG*)(header + 12) != 27)
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
		BOOL ok = ReadAt(offset, manifest, (DWORD)size) && ParseManifest(manifest, (DWORD)size);
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

	VOID Decode(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		const PackFrame* frame = &pack.frames[index];

		BYTE* data = (BYTE*)MemoryAlloc(frame->length);
		int w = 0, h = 0;
		BYTE* rgba = NULL;
		if (data && ReadAt(32 + frame->offset, data, frame->length))
			rgba = WebPDecodeRGBA(data, frame->length, &w, &h);
		if (data)
			MemoryFree(data);

		if (!rgba)
		{
			InterlockedExchange(&tex->state, TexFailed);
			return;
		}

		BYTE* pixels = (BYTE*)MemoryAlloc(w * h * 4);
		if (!pixels)
		{
			WebPFree(rgba);
			InterlockedExchange(&tex->state, TexFailed);
			return;
		}

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
		WebPFree(rgba);

		tex->pixels = pixels;
		tex->w = w;
		tex->h = h;
		InterlockedExchangeAdd(&pack.memory, w * h * 4);
		InterlockedExchange(&tex->state, TexDecoded);
	}

	DWORD __stdcall Worker(LPVOID)
	{
		if (!OpenPack())
		{
			pack.failed = TRUE;
			return 0;
		}

		InterlockedExchange(&pack.ready, TRUE);

		for (;;)
		{
			WaitForSingleObject(pack.wake, INFINITE);
			for (;;)
			{
				DWORD index;
				EnterCriticalSection(&requests.cs);
				BOOL any = requests.head != requests.tail;
				if (any)
				{
					index = requests.queue[requests.head];
					requests.head = (requests.head + 1) % REQUEST_SIZE;
				}
				LeaveCriticalSection(&requests.cs);
				if (!any)
					break;

				Decode(index);
			}
		}
	}

	VOID Request(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		if (tex->state != TexNone)
			return;

		if ((DWORD)pack.memory > MEMORY_LIMIT)
		{
			if (!pack.memoryLogged)
			{
				pack.memoryLogged = TRUE;
				DebugLog("image memory limit reached, new frames stay low resolution");
			}
			return;
		}

		EnterCriticalSection(&requests.cs);
		DWORD next = (requests.tail + 1) % REQUEST_SIZE;
		if (next != requests.head)
		{
			requests.queue[requests.tail] = index;
			requests.tail = next;
			tex->state = TexQueued;
		}
		LeaveCriticalSection(&requests.cs);
		SetEvent(pack.wake);
	}

	VOID StartPack()
	{
		InitializeCriticalSection(&requests.cs);
		InitializeCriticalSection(&game.cs);
		game.initialized = TRUE;
		pack.wake = CreateEvent(NULL, FALSE, FALSE, NULL);
		pack.thread = CreateThread(NULL, 0, Worker, NULL, 0, NULL);
	}

	LONG Resolve(Entry* entry)
	{
		if (entry->resolved)
			return entry->packFrame;
		if (!pack.ready)
			return -1;

		entry->resolved = TRUE;
		entry->packFrame = -1;

		const CHAR* o = entry->owner;
		if (StrLength(o) != 19 || o[0] != 'G' || o[4] != 'U' || o[5] != 'U' || o[14] != 'A' || o[15] != '1' || o[17] != '0' || o[18] != '0')
			return -1;

		const CHAR* action;
		if (!StrCompareNum(o + 10, "IDLE", 4))
			action = "idle";
		else if (!StrCompareNum(o + 10, "HMOV", 4))
			action = "attack";
		else if (!StrCompareNum(o + 10, "HHIT", 4))
			action = "hit";
		else
			return -1;

		const CHAR* side;
		if (o[16] == 'A')
			side = "attacker";
		else if (o[16] == 'D')
			side = "defender";
		else
			return -1;

		CHAR unit[11];
		for (DWORD i = 0; i < 10; ++i)
			unit[i] = (CHAR)((o[i] >= 'A' && o[i] <= 'Z') ? o[i] + 32 : o[i]);
		unit[10] = '\0';

		CHAR id[64];
		StrPrint(id, "battle-unit/%s/%s/%s", unit, action, side);
		const PackAnim* anim = FindAnim(id);
		if (anim && entry->frame < anim->count)
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
					if (src && keyed && src[(py - y) * stride + (px - x)] == key)
						continue;
					if (mask && mask[px - x] < 128)
						continue;
					game.plane[py * game.planeW + px] = seq;
				}
		}
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

			Expect* e = &game.expect[(game.expectHead + game.expectCount++) % MAX_EXPECT];
			e->x = start->x + part[0];
			e->y = start->y + part[1];
			e->w = part[4];
			e->h = part[5];
			e->item = itemIndex;
		}
	}

	VOID Queue(Entry* entry, DWORD rec, const POINT* start, const POINT* offset, const POINT* size, const RECT* area)
	{
		if (!Enabled() || !start || Config::IsMapZoomed())
			return;

		LONG index = Resolve(entry);
		if (index < 0)
			return;

		Tex* tex = &pack.texs[index];
		if (tex->state == TexNone)
			Request(index);

		if (game.itemCount >= MAX_ITEMS)
			return;

		BOOL ready = tex->state == TexDecoded || tex->state == TexUploaded;
		if (!ready)
		{
			++game.notReady;
			return;
		}

		const PackFrame* frame = &pack.frames[index];
		const DWORD* dims = (const DWORD*)rec;
		Item* item = &game.items[game.itemCount];
		item->frame = index;
		item->flipped = entry->flipped;
		item->x = start->x + (entry->flipped ? (LONG)dims[0] - frame->ox - frame->w : frame->ox);
		item->y = start->y + frame->oy;
		item->w = frame->w;
		item->h = frame->h;
		if (area)
			item->clip = *area;
		else
			SetRect(&item->clip, -32768, -32768, 32767, 32767);
		item->replaced = FALSE;
		item->layer = 0;

		QueueParts(game.itemCount, rec, start, offset, size);
		++game.itemCount;
	}

	BOOL Match(LONG x, LONG y, LONG w, LONG h, DWORD* itemIndex)
	{
		DWORD scan = min(game.expectCount, (DWORD)EXPECT_SCAN);
		for (DWORD i = 0; i < scan; ++i)
		{
			const Expect* e = &game.expect[(game.expectHead + i) % MAX_EXPECT];
			if (x >= e->x && y >= e->y && x + w <= e->x + e->w && y + h <= e->y + e->h)
			{
				*itemIndex = e->item;
				game.expectHead = (game.expectHead + i + 1) % MAX_EXPECT;
				game.expectCount -= i + 1;
				return TRUE;
			}
		}
		return FALSE;
	}

	BOOL OnBlit(OpenDrawSurface* dst, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect, LONG srcStride, BOOL keyed, DWORD key)
	{
		if (!game.initialized || dst == src)
			return FALSE;

		LONG w = srcRect->right - srcRect->left, h = srcRect->bottom - srcRect->top;
		DWORD itemIndex;
		if (game.expectCount && Match(x, y, w, h, &itemIndex))
		{
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
			}
			game.dirty = TRUE;
			++game.skipped;
			return TRUE;
		}

		if (dst != game.target)
			return FALSE;

		game.dirty = TRUE;
		const DWORD* pixels = (const DWORD*)src->indexBuffer->data + srcRect->top * srcStride + srcRect->left;
		Cover(x, y, w, h, pixels, srcStride, keyed, key, NULL);
		return FALSE;
	}

	VOID OnFill(OpenDrawSurface* surface)
	{
		if (game.initialized && surface == game.target)
			game.dirty = TRUE;
	}

	VOID OnRelease(OpenDrawSurface* surface)
	{
		if (!game.initialized || surface != game.target)
			return;

		game.target = NULL;
		game.itemCount = game.replacedCount = 0;
		game.expectHead = game.expectCount = 0;
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
		if (!InTarget(dst, &dx, &dy))
			return;

		game.dirty = TRUE;
		if (alpha >= 128)
			Cover(dx, dy, size->cx, size->cy, srcData + srcPitch * srcPos->y + srcPos->x, srcPitch, keyed, key, NULL);
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

	BOOL SlotReserve(Slot* slot, DWORD count)
	{
		if (count <= (slot->items ? MAX_ITEMS : 0))
			return TRUE;
		slot->items = (DrawItem*)MemoryAlloc(MAX_ITEMS * sizeof(DrawItem));
		return slot->items != NULL;
	}

	VOID OnPresent(OpenDrawSurface* back, StateBuffer* frame, LONG left, LONG top)
	{
		if (!game.initialized || back != game.target)
			return;

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
		game.expectHead = game.expectCount = 0;
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

		return drawList.count || before;
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
			"in vec2 fTex;\n"
			"in vec2 fPlane;\n"
			"in float fLayer;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	if (texture(tex02, fPlane).r > fLayer + 0.5)\n"
			"		discard;\n"
			"	FRAG_COLOR = texture(tex01, fTex);\n"
			"}\n";

		gl.generateMipmap = (GLGENERATEMIPMAP)wglGetProcAddress("glGenerateMipmap");

		gl.program = GLCreateProgram();
		GLBindAttribLocation(gl.program, 0, "vCoord");
		GLBindAttribLocation(gl.program, 1, "vTex");
		GLBindAttribLocation(gl.program, 2, "vPlane");
		GLBindAttribLocation(gl.program, 3, "vLayer");

		GLuint vShader = Compile(vertex, GL_VERTEX_SHADER);
		GLuint fShader = Compile(fragment, GL_FRAGMENT_SHADER);
		GLAttachShader(gl.program, vShader);
		GLAttachShader(gl.program, fShader);
		GLLinkProgram(gl.program);
		GLDetachShader(gl.program, fShader);
		GLDetachShader(gl.program, vShader);
		GLDeleteShader(fShader);
		GLDeleteShader(vShader);

		GLUseProgram(gl.program);
		GLUniform1i(GLGetUniformLocation(gl.program, "tex01"), 0);
		GLUniform1i(GLGetUniformLocation(gl.program, "tex02"), 1);

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

		gl.ready = gl.program && gl.vao && gl.vbo && gl.planeTexture;
		gl.failed = !gl.ready;
		if (gl.failed)
			DebugLog("image GL setup failed, 4K sprites off");
		else
			DebugLog("image GL ready, mipmaps %s", gl.generateMipmap ? "on" : "off");
		return gl.ready;
	}

	BOOL Upload(Tex* tex)
	{
		if (tex->state == TexUploaded)
			return TRUE;
		if (tex->state != TexDecoded)
			return FALSE;

		GLGenTextures(1, &tex->id);
		GLBindTexture(GL_TEXTURE_2D, tex->id);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl.generateMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tex->w, tex->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex->pixels);
		if (gl.generateMipmap)
			gl.generateMipmap(GL_TEXTURE_2D);

		MemoryFree(tex->pixels);
		tex->pixels = NULL;
		InterlockedExchange(&tex->state, TexUploaded);
		return TRUE;
	}

	VOID Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight)
	{
		if (!drawList.count || rect->width <= 0 || rect->height <= 0)
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
		{
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
				Tex* tex = &pack.texs[item->frame];
				if (!Upload(tex))
					continue;

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
				GLBindTexture(GL_TEXTURE_2D, tex->id);
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
			for (DWORD i = 0; i < pack.frameCount; ++i)
			{
				Tex* tex = &pack.texs[i];
				if (tex->state == TexUploaded)
				{
					GLDeleteTextures(1, &tex->id);
					tex->id = 0;
					InterlockedExchange(&tex->state, TexNone);
				}
			}

			GLDeleteTextures(1, &gl.planeTexture);
			GLDeleteBuffers(1, &gl.vbo);
			GLDeleteVertexArrays(1, &gl.vao);
			GLDeleteProgram(gl.program);
		}

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

		if (image && name)
		{
			DWORD rec = GetRecord(image);
			Entry* entry = rec ? Find(rec, TRUE) : NULL;
			if (entry && !entry->name[0])
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

	VOID Point(CHAR* buffer, const POINT* pt)
	{
		if (pt)
			StrPrint(buffer, "%d,%d", pt->x, pt->y);
		else
			StrCopy(buffer, "-");
	}

	VOID LogStats()
	{
		DebugLog("image stats builds=%d records=%d full=%d draws=%d unknown=%d replaced=%d skipped=%d notReady=%d memoryMB=%d",
			stats.builds, stats.records, stats.full, stats.draws, stats.unknown, game.replaced, game.skipped, game.notReady, pack.memory >> 20);
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
			Queue(entry, rec, start, offset, size, area);

		DWORD tick = GetTickCount();
		BOOL key = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
		if (key && !trace.key)
		{
			trace.until = tick + TRACE_MS;
			LogStats();
		}
		trace.key = key;

		if ((LONG)(tick - trace.flagCheck) >= 0)
		{
			trace.flagCheck = tick + FLAG_CHECK_MS;
			if (TakeFlag())
			{
				trace.until = tick + TRACE_MS;
				LogStats();
			}
		}

		BOOL first = entry && !entry->drawn && stats.firstDraws < FIRST_DRAW_LIMIT;
		BOOL tracing = (LONG)(trace.until - tick) > 0 || TextReplace::traceLeft;
		if (!first && !tracing)
			return;

		if (first)
		{
			entry->drawn = TRUE;
			++stats.firstDraws;
		}

		const DWORD* dims = (const DWORD*)rec;
		CHAR s[24], o[24], z[24];
		Point(s, start);
		Point(o, offset);
		Point(z, size);
		DebugLog("image %s %s owner %s#%d flip %d image %dx%d start %s offset %s size %s area %d,%d,%d,%d rec %08X pack %d",
			first ? "first" : "trace", entry && entry->name[0] ? entry->name : "?",
			entry && entry->owner[0] ? entry->owner : "-", entry ? entry->frame : 0, entry ? entry->flipped : 0,
			dims ? dims[0] : 0, dims ? dims[1] : 0, s, o, z,
			area ? area->left : 0, area ? area->top : 0, area ? area->right : 0, area ? area->bottom : 0, rec,
			entry && entry->resolved ? entry->packFrame : -9);
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
		StartPack();
		DebugLog("image replace loaded");
	}
#pragma endregion
}
