#include "StdAfx.h"
#include "GLib.h"
#include "Pack.h"
#include "../zstd/zstd.h"

#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_COMPRESSED_RGBA_BPTC_UNORM 0x8E8C
#define GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#define GL_TEXTURE_FREE_MEMORY_ATI 0x87FC

#define NAME_SIZE 65536
#define REQUEST_SIZE 4096
#define DECODE_LIMIT (128 * 1024 * 1024)
#define DECODE_WORKERS 4
#define DECODE_STALE 120
#define GPU_FALLBACK (1536 * 1024 * 1024)
#define GPU_MIN (256 * 1024 * 1024)
#define UPLOAD_US 4000
#define UPLOAD_BATCH 64
#define WORKER_WAIT_MS 100

typedef VOID(__stdcall* GLCOMPRESSEDTEXIMAGE2D)(GLenum target, GLint level, GLenum format, GLsizei width, GLsizei height, GLint border, GLsizei size, const VOID* data);

namespace Pack
{
	enum TexState { TexNone, TexQueued, TexDecoded, TexUploaded, TexFailed, TexDecoding };

	enum TexList { ListNone, ListDecoded, ListGpu };

	struct Entry {
		LONGLONG offset;
		DWORD length;
		DWORD name;
		LONG w, h;
		LONG pw, ph;
		BOOL additive;
	};

	struct Name {
		DWORD hash, name;
		LONG frame;
	};

	struct Tex {
		volatile LONG state;
		BYTE* pixels;
		LONG w, h;
		DWORD bytes, gpuBytes;
		GLuint id;
		volatile LONG lastUse;
		DWORD queuedAt;
		DWORD prev, next;
		TexList list;
	};

	struct Chain {
		DWORD head, tail, count;
	};

	struct {
		HANDLE file, wake, space;
		BOOL started, bc7;
		volatile LONG ready, failed;
		Entry* entries;
		DWORD count, capacity;
		CHAR* names;
		DWORD namesSize, namesCapacity;
		Name ids[NAME_SIZE];
		DWORD idCount;
		Name records[NAME_SIZE];
		DWORD recordCount;
		Tex* texs;
		volatile LONG decoded;
		LONG gpu, gpuBudget;
		Chain decodedList, gpuList;
		volatile LONG frameNo;
		CRITICAL_SECTION cs;
		DWORD queue[REQUEST_SIZE];
		DWORD head, tail;
	} pack;

	struct {
		BOOL ready;
		GLCOMPRESSEDTEXIMAGE2D compressedTexImage2D;
		BOOL toldError;
		LARGE_INTEGER frequency;
	} gl;

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

	BOOL Is(const CHAR* text, DWORD length, const CHAR* word)
	{
		DWORD n = StrLength(word);
		return n == length && !MemoryCompare(text, word, n);
	}

	VOID Upper(CHAR* text)
	{
		for (; *text; ++text)
			if (*text >= 'a' && *text <= 'z')
				*text -= 32;
	}

	DWORD HashText(const CHAR* text)
	{
		DWORD hash = 2166136261u;
		for (; *text; ++text)
			hash = (hash ^ (BYTE)*text) * 16777619u;
		return hash ? hash : 1;
	}

	BOOL AddName(const CHAR* text, DWORD* offset)
	{
		DWORD length = StrLength(text);
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
		MemoryCopy(pack.names + pack.namesSize, text, length + 1);
		pack.namesSize += length + 1;
		return TRUE;
	}

	Name* Slot(Name* table, const CHAR* text, DWORD hash)
	{
		DWORD index = hash & (NAME_SIZE - 1);
		while (table[index].hash && (table[index].hash != hash || StrCompare(pack.names + table[index].name, text)))
			index = (index + 1) & (NAME_SIZE - 1);
		return &table[index];
	}

	BOOL Insert(Name* table, DWORD* count, const CHAR* text, LONG frame, BOOL shared, DWORD* name)
	{
		DWORD hash = HashText(text);
		Name* n = Slot(table, text, hash);
		if (n->hash)
		{
			if (shared)
				n->frame = -1;
			return FALSE;
		}
		if (*count >= NAME_SIZE * 3 / 4 || !AddName(text, &n->name))
			return FALSE;
		n->hash = hash;
		n->frame = frame;
		++*count;
		if (name)
			*name = n->name;
		return TRUE;
	}

	LONG Lookup(Name* table, const CHAR* text)
	{
		Name* n = Slot(table, text, HashText(text));
		return n->hash ? n->frame : -1;
	}

	BOOL GrowEntries()
	{
		if (pack.count < pack.capacity)
			return TRUE;

		DWORD capacity = pack.capacity ? pack.capacity * 2 : 65536;
		Entry* list = (Entry*)MemoryAlloc(capacity * sizeof(Entry));
		if (!list)
			return FALSE;
		if (pack.count)
			MemoryCopy(list, pack.entries, pack.count * sizeof(Entry));
		if (pack.entries)
			MemoryFree(pack.entries);
		pack.entries = list;
		pack.capacity = capacity;
		return TRUE;
	}

	VOID ParseMetadata(Json* j, Entry* entry)
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
				entry->w = (LONG)Num(j);
			else if (Is(name, n, "Height"))
				entry->h = (LONG)Num(j);
			else if (Is(name, n, "PixelWidth"))
				entry->pw = (LONG)Num(j);
			else if (Is(name, n, "PixelHeight"))
				entry->ph = (LONG)Num(j);
			else if (Is(name, n, "Additive"))
			{
				Ws(j);
				entry->additive = j->p < j->end && *j->p == 't';
				Skip(j);
			}
			else
				Skip(j);
		} while (!j->fail && Eat(j, ','));

		if (!Eat(j, '}'))
			j->fail = TRUE;
	}

	VOID ParseSprite(Json* j)
	{
		Entry entry = {};
		const CHAR* id = NULL;
		DWORD idLength = 0;
		if (!Eat(j, '{'))
		{
			j->fail = TRUE;
			return;
		}

		if (!Eat(j, '}'))
		{
			do
			{
				const CHAR* key;
				DWORD length;
				if (!Key(j, &key, &length))
					return;

				if (Is(key, length, "Metadata"))
					ParseMetadata(j, &entry);
				else if (Is(key, length, "Offset"))
					entry.offset = Num(j);
				else if (Is(key, length, "Length"))
					entry.length = (DWORD)Num(j);
				else if (Is(key, length, "Id"))
					Str(j, &id, &idLength);
				else
					Skip(j);
			} while (!j->fail && Eat(j, ','));

			if (!Eat(j, '}'))
				j->fail = TRUE;
		}

		CHAR key[96];
		if (j->fail || !id || idLength >= sizeof(key) || !GrowEntries())
			return;
		MemoryCopy(key, id, idLength);
		key[idLength] = '\0';
		Upper(key);

		if (!Insert(pack.ids, &pack.idCount, key, (LONG)pack.count, FALSE, &entry.name))
			return;
		const CHAR* record = StrChar(key, '/');
		if (record)
			Insert(pack.records, &pack.recordCount, record + 1, (LONG)pack.count, TRUE, NULL);
		pack.entries[pack.count++] = entry;
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

			if (Is(key, length, "Sprites"))
			{
				if (!Eat(&j, '['))
					return FALSE;
				if (!Eat(&j, ']'))
				{
					do
						ParseSprite(&j);
					while (!j.fail && Eat(&j, ','));

					if (!Eat(&j, ']'))
						return FALSE;
				}
			}
			else if (Is(key, length, "Format"))
			{
				const CHAR* format;
				DWORD n;
				pack.bc7 = Str(&j, &format, &n) && Is(format, n, "BC7-ZSTD");
			}
			else
				Skip(&j);
		} while (!j.fail && Eat(&j, ','));

		return !j.fail;
	}

	BOOL ReadAt(HANDLE file, LONGLONG offset, VOID* buffer, DWORD size)
	{
		OVERLAPPED ov = {};
		ov.Offset = (DWORD)offset;
		ov.OffsetHigh = (DWORD)(offset >> 32);
		DWORD read = 0;
		return ReadFile(file, buffer, size, &read, &ov) && read == size;
	}

	VOID PackPath(CHAR* path)
	{
		DWORD length = GetModuleFileName(NULL, path, MAX_PATH);
		while (length && path[length - 1] != '\\')
			--length;
		StrCopy(path + length, PACK_NAME);
	}

	BOOL Present()
	{
		static INT present = -1;
		if (present < 0)
		{
			CHAR path[MAX_PATH];
			PackPath(path);
			present = GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES;
			if (!present)
				DebugLog("twin off: %s is not next to the game", PACK_NAME);
		}
		return present;
	}

	BOOL OpenPack()
	{
		CHAR path[MAX_PATH];
		PackPath(path);
		pack.file = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, NULL);
		if (pack.file == INVALID_HANDLE_VALUE)
		{
			pack.file = NULL;
			DebugLog("image pack %s could not be opened", path);
			return FALSE;
		}

		BYTE header[32];
		if (!ReadAt(pack.file, 0, header, sizeof(header)) || MemoryCompare(header, "GALLEANPACK", 11) || *(LONG*)(header + 12) != 27)
		{
			DebugLog("image pack %s: header not recognised", PACK_NAME);
			return FALSE;
		}

		LONGLONG offset = *(LONGLONG*)(header + 16);
		LONGLONG size = *(LONGLONG*)(header + 24);
		CHAR* manifest = size > 0 && size <= 256 * 1024 * 1024 ? (CHAR*)MemoryAlloc((DWORD)size) : NULL;
		BOOL ok = manifest && ReadAt(pack.file, offset, manifest, (DWORD)size) && ParseManifest(manifest, (DWORD)size);
		if (manifest)
			MemoryFree(manifest);
		if (ok && !pack.bc7)
		{
			DebugLog("image pack %s: not a BC7-ZSTD pack, rebuild it with tools/repack/ff2k.py", PACK_NAME);
			return FALSE;
		}
		pack.texs = ok ? (Tex*)MemoryAlloc(pack.count * sizeof(Tex)) : NULL;
		if (!pack.texs)
		{
			DebugLog("image pack %s: manifest not read", PACK_NAME);
			return FALSE;
		}
		MemoryZero(pack.texs, pack.count * sizeof(Tex));
		DebugLog("image pack %s loaded: %u entries", PACK_NAME, pack.count);
		return TRUE;
	}

	VOID ChainPush(Chain* chain, DWORD index, TexList list, BOOL front)
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

	VOID ChainRemove(Chain* chain, DWORD index)
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

	BOOL ReadBlocks(const Entry* entry, Tex* tex)
	{
		if (entry->pw <= 0 || entry->ph <= 0)
			return FALSE;
		DWORD bytes = ((entry->pw + 3) / 4) * ((entry->ph + 3) / 4) * 16;
		BYTE* data = (BYTE*)MemoryAlloc(entry->length);
		BYTE* blocks = data ? (BYTE*)MemoryAlloc(bytes) : NULL;
		BOOL ok = blocks && ReadAt(pack.file, 32 + entry->offset, data, entry->length) && ZSTD_decompress(blocks, bytes, data, entry->length) == bytes;
		if (data)
			MemoryFree(data);
		if (!ok)
		{
			if (blocks)
				MemoryFree(blocks);
			return FALSE;
		}

		tex->pixels = blocks;
		tex->w = entry->pw;
		tex->h = entry->ph;
		tex->bytes = bytes;
		return TRUE;
	}

	VOID Decode(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		if (!ReadBlocks(&pack.entries[index], tex))
		{
			DebugLog("image stays 1x: the ff4k entry %s failed to decode", pack.names + pack.entries[index].name);
			InterlockedExchange(&tex->state, TexFailed);
			return;
		}

		InterlockedExchangeAdd(&pack.decoded, tex->bytes);
		EnterCriticalSection(&pack.cs);
		ChainPush(&pack.decodedList, index, ListDecoded, FALSE);
		InterlockedExchange(&tex->state, TexDecoded);
		LeaveCriticalSection(&pack.cs);
	}

	DWORD __stdcall DecodeLoop(LPVOID)
	{
		for (;;)
		{
			WaitForSingleObject(pack.wake, INFINITE);
			for (;;)
			{
				if (pack.decoded >= DECODE_LIMIT)
				{
					WaitForSingleObject(pack.space, WORKER_WAIT_MS);
					continue;
				}

				DWORD index = 0;
				EnterCriticalSection(&pack.cs);
				BOOL any = pack.head != pack.tail;
				if (any)
				{
					index = pack.queue[pack.head];
					pack.head = (pack.head + 1) % REQUEST_SIZE;
				}
				BOOL more = pack.head != pack.tail;
				LeaveCriticalSection(&pack.cs);
				if (!any)
					break;
				if (more)
					SetEvent(pack.wake);

				if (InterlockedCompareExchange(&pack.texs[index].state, TexDecoding, TexQueued) == TexQueued)
					Decode(index);
			}
		}
	}

	DWORD __stdcall Worker(LPVOID)
	{
		if (!OpenPack())
		{
			InterlockedExchange(&pack.failed, TRUE);
			return 0;
		}
		InterlockedExchange(&pack.ready, TRUE);

		SYSTEM_INFO info;
		GetSystemInfo(&info);
		DWORD extra = min(max(info.dwNumberOfProcessors, 2) - 1, DECODE_WORKERS) - 1;
		for (DWORD i = 0; i < extra; ++i)
			CreateThread(NULL, 0, DecodeLoop, NULL, 0, NULL);
		return DecodeLoop(NULL);
	}

	VOID Start()
	{
		if (pack.started)
			return;
		pack.started = TRUE;
		InitializeCriticalSection(&pack.cs);
		pack.wake = CreateEvent(NULL, FALSE, FALSE, NULL);
		pack.space = CreateEvent(NULL, FALSE, FALSE, NULL);
		pack.gpuBudget = GPU_FALLBACK;
		CreateThread(NULL, 0, Worker, NULL, 0, NULL);
	}

	BOOL Ready()
	{
		return pack.ready;
	}

	BOOL Failed()
	{
		return pack.failed;
	}

	LONG Find(const CHAR* key)
	{
		return pack.ready ? Lookup(pack.ids, key) : -1;
	}

	LONG FindRecord(const CHAR* record)
	{
		return pack.ready ? Lookup(pack.records, record) : -1;
	}

	VOID Size(LONG frame, LONG* width, LONG* height)
	{
		*width = pack.entries[frame].w;
		*height = pack.entries[frame].h;
	}

	const CHAR* EntryName(LONG frame)
	{
		return pack.ready && frame >= 0 && (DWORD)frame < pack.count ? pack.names + pack.entries[frame].name : "?";
	}

	LONG State(LONG frame)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.count)
			return -1;

		Tex* tex = &pack.texs[frame];
		tex->lastUse = pack.frameNo;
		LONG state = tex->state;
		if (state == TexNone)
		{
			EnterCriticalSection(&pack.cs);
			DWORD next = (pack.tail + 1) % REQUEST_SIZE;
			if (tex->state == TexNone && next != pack.head)
			{
				pack.queue[pack.tail] = frame;
				pack.tail = next;
				tex->queuedAt = GetTickCount();
				tex->state = TexQueued;
			}
			LeaveCriticalSection(&pack.cs);
			SetEvent(pack.wake);
		}
		return state == TexFailed ? -1 : state == TexUploaded ? 2 : state == TexDecoded ? 1 : 0;
	}

	LONG QueuedFor(LONG frame)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.count || !pack.texs[frame].queuedAt)
			return -1;
		return (LONG)(GetTickCount() - pack.texs[frame].queuedAt);
	}

	VOID Prefetch(const CHAR* prefix, LONG at, CHAR ch)
	{
		if (!pack.ready)
			return;
		DWORD length = StrLength(prefix);
		for (DWORD i = 0; i < pack.count; ++i)
		{
			const CHAR* name = pack.names + pack.entries[i].name;
			if (!StrCompareNum(name, prefix, length) && (at < 0 || (StrLength(name) > (DWORD)at && name[at] == ch)))
				State(i);
		}
	}

	LONG Wait(LONG frame, DWORD timeout)
	{
		LONG state = State(frame);
		if (state != 0)
			return state;

		Tex* tex = &pack.texs[frame];
		if (InterlockedCompareExchange(&tex->state, TexDecoding, TexQueued) == TexQueued)
			Decode(frame);
		DWORD start = GetTickCount();
		while ((tex->state == TexDecoding || tex->state == TexQueued) && GetTickCount() - start < timeout)
			Sleep(1);
		return State(frame);
	}

	BOOL Prepare()
	{
		if (!pack.ready)
			return FALSE;
		InterlockedIncrement(&pack.frameNo);
		return pack.decodedList.count != 0;
	}

	VOID InitGL()
	{
		if (gl.ready)
			return;
		gl.ready = TRUE;
		gl.compressedTexImage2D = (GLCOMPRESSEDTEXIMAGE2D)wglGetProcAddress("glCompressedTexImage2D");
		if (!gl.compressedTexImage2D)
			DebugLog("image pack: glCompressedTexImage2D is missing, 4K stays off");
		QueryPerformanceFrequency(&gl.frequency);

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

	VOID FreeDecoded(Tex* tex)
	{
		MemoryFree(tex->pixels);
		tex->pixels = NULL;
		InterlockedExchangeAdd(&pack.decoded, -(LONG)tex->bytes);
		SetEvent(pack.space);
	}

	VOID UploadTex(DWORD index)
	{
		Tex* tex = &pack.texs[index];
		GLenum error = GL_INVALID_OPERATION;
		if (gl.compressedTexImage2D)
		{
			while (glGetError() != GL_NO_ERROR)
				;
			GLGenTextures(1, &tex->id);
			GLBindTexture(GL_TEXTURE_2D, tex->id);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			gl.compressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM, tex->w, tex->h, 0, tex->bytes, tex->pixels);
			error = glGetError();
		}

		FreeDecoded(tex);
		if (error != GL_NO_ERROR)
		{
			if (!gl.toldError)
				DebugLog("image pack: BC7 upload of %s %dx%d failed with GL error %X", pack.names + pack.entries[index].name, tex->w, tex->h, error);
			gl.toldError = TRUE;
			if (tex->id)
				GLDeleteTextures(1, &tex->id);
			tex->id = 0;
			InterlockedExchange(&tex->state, TexFailed);
			return;
		}
		tex->gpuBytes = tex->bytes;
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
		if (pack.decoded < DECODE_LIMIT)
			return;
		LONG now = pack.frameNo;
		EnterCriticalSection(&pack.cs);
		for (DWORD value = pack.decodedList.head; value && pack.decoded >= DECODE_LIMIT * 3 / 4;)
		{
			DWORD index = value - 1;
			Tex* tex = &pack.texs[index];
			value = tex->next;
			if (now - tex->lastUse < DECODE_STALE)
				continue;
			ChainRemove(&pack.decodedList, index);
			FreeDecoded(tex);
			InterlockedExchange(&tex->state, TexNone);
		}
		LeaveCriticalSection(&pack.cs);
	}

	VOID UploadPending()
	{
		LARGE_INTEGER start, now;
		QueryPerformanceCounter(&start);
		LONGLONG limit = gl.frequency.QuadPart * UPLOAD_US / 1000000;

		for (;;)
		{
			DWORD batch[UPLOAD_BATCH];
			DWORD count = 0;
			EnterCriticalSection(&pack.cs);
			for (DWORD value = pack.decodedList.head; value && count < UPLOAD_BATCH; value = pack.texs[value - 1].next)
				batch[count++] = value - 1;
			for (DWORD i = 0; i < count; ++i)
				ChainRemove(&pack.decodedList, batch[i]);
			LeaveCriticalSection(&pack.cs);
			if (!count)
				return;

			DWORD done = 0;
			for (; done < count; ++done)
			{
				QueryPerformanceCounter(&now);
				if (now.QuadPart - start.QuadPart >= limit)
					break;
				UploadTex(batch[done]);
			}

			if (done < count)
			{
				EnterCriticalSection(&pack.cs);
				for (DWORD i = count; i > done; --i)
					ChainPush(&pack.decodedList, batch[i - 1], ListDecoded, TRUE);
				LeaveCriticalSection(&pack.cs);
				return;
			}
		}
	}

	VOID EvictGpu()
	{
		if (pack.gpu <= pack.gpuBudget)
			return;

		LONG low = pack.gpuBudget - pack.gpuBudget / 8;
		LONG now = pack.frameNo;
		DWORD value = pack.gpuList.tail;
		while (value && pack.gpu > low)
		{
			DWORD index = value - 1;
			value = pack.texs[index].prev;
			if (now - pack.texs[index].lastUse > 1)
				Evict(index);
		}
	}

	VOID Draw()
	{
		if (!pack.ready || (!pack.decodedList.count && pack.gpu <= pack.gpuBudget))
			return;

		GLint active, texture0;
		GLGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		GLActiveTexture(GL_TEXTURE0);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);

		InitGL();
		DropDecoded();
		UploadPending();
		EvictGpu();

		GLBindTexture(GL_TEXTURE_2D, texture0);
		GLActiveTexture(active);
	}

	VOID ReleaseGL()
	{
		if (gl.ready)
			while (pack.gpuList.tail)
				Evict(pack.gpuList.tail - 1);
		gl.ready = FALSE;
	}

	BOOL Texture(LONG frame, GLuint* id)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.count || pack.texs[frame].state != TexUploaded)
			return FALSE;

		Tex* tex = &pack.texs[frame];
		if (tex->list == ListGpu && pack.gpuList.head != (DWORD)frame + 1)
		{
			ChainRemove(&pack.gpuList, frame);
			ChainPush(&pack.gpuList, frame, ListGpu, TRUE);
		}
		*id = tex->id;
		return TRUE;
	}

	LONG Bytes(LONG frame)
	{
		return pack.ready && frame >= 0 && (DWORD)frame < pack.count ? (LONG)pack.texs[frame].bytes : 0;
	}

	BOOL Upload(LONG frame)
	{
		if (!pack.ready || frame < 0 || (DWORD)frame >= pack.count)
			return FALSE;

		Tex* tex = &pack.texs[frame];
		if (tex->state == TexUploaded)
			return TRUE;

		InitGL();
		BOOL take = FALSE;
		EnterCriticalSection(&pack.cs);
		if (tex->state == TexDecoded && tex->list == ListDecoded)
		{
			ChainRemove(&pack.decodedList, frame);
			take = TRUE;
		}
		LeaveCriticalSection(&pack.cs);
		if (take)
			UploadTex(frame);
		return take;
	}
}
