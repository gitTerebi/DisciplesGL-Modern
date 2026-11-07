#include "StdAfx.h"
#include "intrin.h"
#include "Pack.h"
#include "Seed.h"
#include "Twin.h"

#define f(a) (a + baseOffset)

#define NAME_LENGTH 128
#define ARCHIVE_LIMIT 64
#define ARCHIVE_ENTRY_SIZE 260
#define STORE_BITS 12
#define STORE_SIZE (1 << STORE_BITS)
#define FACE_SLOTS 256
#define INSIDE_MAX 16
#define SEED_RECORDS 1024
#define SEED_INLINE_BYTES (16 * 1024 * 1024)
#define WAIT_PIXELS (300 * 300)
#define WAIT_MS 2000
#define SEED_POLL_BYTES (24 * 1024 * 1024)
#define SEED_POLL_BASE_PX (1024 * 1024)
#define TOLD_SIZE 4096

namespace Seed
{
	struct ArchiveFile {
		DWORD id;
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

	struct StoreImage {
		const VOID* image;
		ArchiveFile* file;
		const BYTE* first;
		DWORD nextFree;
	};

	struct {
		StoreImage list[STORE_SIZE * 3 / 4];
		DWORD index[STORE_SIZE];
		DWORD used, free;
	} stores;

	struct Face {
		const BYTE* data;
		LONG width, height, pitch;
		CHAR name[NAME_LENGTH];
	};

	struct {
		Face list[FACE_SLOTS];
		DWORD next;
	} faces;

	struct Blit {
		const BYTE* src;
		LONG srcPitch;
		const DWORD* dst;
		LONG dstPitch;
		LONG width, height;
	};

	struct {
		const BYTE* data;
		LONG pitch;
		RECT rect;
		LONG frame;
		DWORD depth, count;
		Blit inside[INSIDE_MAX];
	} loader;

	struct SeedRecord {
		StateBuffer* owner;
		DWORD id;
		RECT rect, place;
		LONG frame;
		BOOL pending, based;
	};

	struct {
		SeedRecord list[SEED_RECORDS];
		DWORD top, next;
		LONGLONG inlineBytes;
		LONG basePx;
	} seeds;

	struct {
		DWORD keys[TOLD_SIZE];
		DWORD count, exeLo, exeHi;
	} told;

	DWORD sub_StoreLoad, sub_StoreDraw, sub_StoreFree, sub_FaceLoad;

	BOOL ReadAt(HANDLE file, LONGLONG offset, VOID* buffer, DWORD size)
	{
		OVERLAPPED ov = {};
		ov.Offset = (DWORD)offset;
		ov.OffsetHigh = (DWORD)(offset >> 32);
		DWORD read = 0;
		return ReadFile(file, buffer, size, &read, &ov) && read == size;
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

	// Discipl2.exe v3.01b 0x00522E7E: six links from factory+8 reach the MqDB, path at +8; factory+0x10 holds index * 100000 at +4
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

	BOOL StemEquals(const CHAR* name, const CHAR* stem, DWORD stemLength)
	{
		DWORD i = 0;
		for (; i < stemLength; ++i)
		{
			CHAR x = name[i], y = stem[i];
			if (!x)
				return FALSE;
			if (x >= 'a' && x <= 'z')
				x -= 32;
			if (x != y)
				return FALSE;
		}
		return !name[i] || !StrCompareInsensitive(name + i, ".PNG");
	}

	ArchiveFile* FaceFile(const CHAR* name, Archive** archive)
	{
		*archive = NULL;
		CHAR stem[NAME_LENGTH];
		DWORD length = 0;
		for (; name[length] && length < NAME_LENGTH - 1; ++length)
		{
			CHAR c = name[length];
			stem[length] = c >= 'a' && c <= 'z' ? c - 32 : c;
		}
		stem[length] = 0;
		if (length > 4 && !StrCompare(stem + length - 4, ".PNG"))
			stem[length -= 4] = 0;
		if (!length)
			return NULL;

		ArchiveFile* found = NULL;
		if (archiveCsReady)
			EnterCriticalSection(&archiveCs);
		for (DWORD i = 0; i < archives.count && !found; ++i)
		{
			Archive* a = &archives.list[i];
			DWORD pathLength = StrLength(a->path);
			if (!a->files || pathLength < 9 || StrCompareInsensitive(a->path + pathLength - 9, "\\Faces.ff"))
				continue;
			for (DWORD slot = 0; slot <= a->mask; ++slot)
				if (a->files[slot].id && StemEquals(a->files[slot].name, stem, length))
				{
					found = &a->files[slot];
					*archive = a;
					break;
				}
		}
		if (archiveCsReady)
			LeaveCriticalSection(&archiveCs);
		return found;
	}

	const CHAR* ArchiveTail(const Archive* a)
	{
		if (!a)
			return "no archive";
		const CHAR* tail = StrLastChar(a->path, '\\');
		return tail ? tail + 1 : a->path;
	}

	DWORD HashText(const CHAR* text)
	{
		DWORD hash = 2166136261u;
		for (; *text; ++text)
			hash = (hash ^ (BYTE)*text) * 16777619u;
		return hash ? hash : 1;
	}

	BOOL Once(DWORD key)
	{
		key = key ? key : 1;
		for (DWORD slot = (key * 2654435761u) & (TOLD_SIZE - 1), n = 0; n < TOLD_SIZE; ++n, slot = (slot + 1) & (TOLD_SIZE - 1))
		{
			if (told.keys[slot] == key)
				return FALSE;
			if (!told.keys[slot])
			{
				if (told.count >= TOLD_SIZE * 3 / 4)
					return FALSE;
				told.keys[slot] = key;
				++told.count;
				return TRUE;
			}
		}
		return FALSE;
	}

	const CHAR* Key(const Archive* archive, const CHAR* record, CHAR* key)
	{
		StrPrint(key, "%.24s/%.127s", ArchiveTail(archive), record);
		CharUpper(key);
		return key;
	}

	LONG Lookup(const Archive* archive, const CHAR* record, LONG* outWidth, LONG* outHeight)
	{
		CHAR key[160];
		LONG width = *outWidth, height = *outHeight;
		LONG frame = Pack::Find(Key(archive, record, key)), w = 0, h = 0;
		if (frame >= 0)
		{
			Pack::Size(frame, &w, &h);
			if (w > 0 && h > 0 && w <= width && h <= height)
			{
				*outWidth = w;
				*outHeight = h;
				return frame;
			}
		}

		DWORD hash = HashText(key);
		if (!Pack::Ready())
		{
			if (Once(hash + 1))
				DebugLog("image stays 1x: %s %dx%d was decoded before %s was loaded%s", key, width, height, PACK_NAME, Pack::Failed() ? " (it failed to load)" : "");
		}
		else if (Once(hash))
		{
			if (frame < 0)
				DebugLog("image stays 1x: no ff4k entry for %s (%dx%d)", key, width, height);
			else
				DebugLog("image stays 1x: the ff4k entry for %s is %dx%d, the decode %dx%d", key, w, h, width, height);
		}
		return -1;
	}

	DWORD StoreKey(DWORD value)
	{
		return (DWORD)stores.list[value - 1].image;
	}

	DWORD StoreHome(DWORD key)
	{
		return (key * 2654435761u) >> (32 - STORE_BITS);
	}

	DWORD StoreSlot(const VOID* image, BOOL* found)
	{
		DWORD slot = StoreHome((DWORD)image);
		for (DWORD i = 0; i < STORE_SIZE; ++i, slot = (slot + 1) & (STORE_SIZE - 1))
		{
			*found = stores.index[slot] && StoreKey(stores.index[slot]) == (DWORD)image;
			if (*found || !stores.index[slot])
				return slot;
		}
		return STORE_SIZE;
	}

	StoreImage* FindStore(const VOID* image, BOOL insert)
	{
		BOOL found;
		DWORD slot = StoreSlot(image, &found);
		if (found)
			return &stores.list[stores.index[slot] - 1];
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
		stores.index[slot] = value;
		return s;
	}

	VOID RemoveStore(const VOID* image)
	{
		BOOL found;
		DWORD slot = StoreSlot(image, &found);
		if (!found)
			return;

		DWORD value = stores.index[slot];
		StoreImage* s = &stores.list[value - 1];
		Twin::DropMemory(s->first);
		DWORD hole = slot;
		for (DWORD next = (slot + 1) & (STORE_SIZE - 1); stores.index[next]; next = (next + 1) & (STORE_SIZE - 1))
		{
			DWORD home = StoreHome(StoreKey(stores.index[next]));
			if (((next - home) & (STORE_SIZE - 1)) >= ((next - hole) & (STORE_SIZE - 1)))
			{
				stores.index[hole] = stores.index[next];
				hole = next;
			}
		}
		stores.index[hole] = 0;
		MemoryZero(s, sizeof(StoreImage));
		s->nextFree = stores.free;
		stores.free = value;
	}

	VOID Forget(SeedRecord* r)
	{
		Twin::Record* twin = Twin::At(r->id);
		if (twin->owner == r->owner && twin->seeds)
			--twin->seeds;
		MemoryZero(r, sizeof(SeedRecord));
		while (seeds.top && !seeds.list[seeds.top - 1].owner)
			--seeds.top;
	}

	VOID Write(Twin::Record* twin, const RECT* rect)
	{
		RECT part;
		for (DWORD i = 0; i < seeds.top && twin->seeds; ++i)
			if (seeds.list[i].owner == twin->owner && IntersectRect(&part, &seeds.list[i].rect, rect))
				Forget(&seeds.list[i]);
	}

	VOID DropTwin(Twin::Record* twin)
	{
		for (DWORD i = 0; i < seeds.top && twin->seeds; ++i)
			if (seeds.list[i].owner == twin->owner)
				Forget(&seeds.list[i]);
		twin->seeds = 0;
	}

	VOID Repend()
	{
		for (DWORD i = 0; i < seeds.top; ++i)
			if (seeds.list[i].owner)
			{
				seeds.list[i].pending = TRUE;
				seeds.list[i].based = FALSE;
			}
	}

	VOID Emit(SeedRecord* r)
	{
		LONG state = Pack::State(r->frame);
		if (state < 0)
		{
			Forget(r);
			return;
		}
		if (!state)
			return;
		if (state == 1)
		{
			LONG bytes = Pack::Bytes(r->frame);
			if (seeds.inlineBytes && seeds.inlineBytes + bytes > SEED_INLINE_BYTES)
				return;
			seeds.inlineBytes += bytes;
		}

		Twin::Record* twin = Twin::At(r->id);
		if (!r->based)
		{
			Twin::Cover(twin, &r->rect);
			Twin::CutRegion(twin, &r->rect);
			Twin::Upload(twin, r->id, &r->rect, TRUE);
			r->based = TRUE;
			seeds.basePx += (r->rect.right - r->rect.left) * (r->rect.bottom - r->rect.top);
		}

		Twin::Cmd* cmd = Twin::Emit(Twin::CmdSeed);
		if (!cmd)
			return;
		cmd->dst = r->id;
		cmd->rect = r->rect;
		cmd->place = r->place;
		cmd->value = r->frame;
		Twin::AddArt(twin, &r->rect);
		r->pending = FALSE;
	}

	VOID Poll()
	{
		for (DWORD i = 0; i < seeds.top && Twin::ListBytes() <= SEED_POLL_BYTES; ++i)
		{
			SeedRecord* r = &seeds.list[i];
			if (r->owner && r->pending && (r->based || seeds.basePx < SEED_POLL_BASE_PX))
				Emit(r);
		}
		seeds.inlineBytes = 0;
		seeds.basePx = 0;
	}

	VOID SeedAt(Twin::Record* twin, DWORD id, const RECT* rect, const RECT* place, LONG frame, BOOL derived, BOOL based)
	{
		if (!derived)
		{
			for (DWORD i = 0; i < seeds.top && twin->seeds; ++i)
			{
				const SeedRecord* other = &seeds.list[i];
				if (other->owner == twin->owner && other->frame == frame && EqualRect(&other->rect, rect) && EqualRect(&other->place, place))
					return;
			}
			Twin::MarkDirty(twin, *rect);
		}

		SeedRecord* r = NULL;
		for (DWORD i = 0; i < SEED_RECORDS && !r; ++i)
			if (!seeds.list[i].owner)
				r = &seeds.list[i];
		if (!r)
		{
			r = &seeds.list[seeds.next];
			seeds.next = (seeds.next + 1) % SEED_RECORDS;
			Forget(r);
		}

		r->owner = twin->owner;
		r->id = id;
		r->rect = *rect;
		r->place = *place;
		r->frame = frame;
		r->pending = TRUE;
		r->based = based;
		++twin->seeds;
		seeds.top = max(seeds.top, DWORD(r - seeds.list) + 1);
		if (!derived)
			Emit(r);
	}

	VOID Derive(Twin::Record* target, DWORD dstId, const RECT* to, Twin::Record* source, const RECT* from, BOOL mirror)
	{
		if (!source->seeds || target == source || target->screen)
			return;

		LONG ox = to->left - from->left, oy = to->top - from->top, axis = to->left + from->right;
		for (DWORD i = 0, top = seeds.top; i < top; ++i)
		{
			SeedRecord r = seeds.list[i];
			RECT rect;
			if (r.owner != source->owner || !r.pending || !IntersectRect(&rect, &r.rect, from))
				continue;

			RECT place = r.place;
			if (mirror)
			{
				SetRect(&rect, axis - rect.right, rect.top, axis - rect.left, rect.bottom);
				place.left = axis - r.place.left;
				place.right = axis - r.place.right;
				ox = 0;
			}
			OffsetRect(&rect, ox, oy);
			OffsetRect(&place, ox, oy);
			SeedAt(target, dstId, &rect, &place, r.frame, TRUE, r.based);
		}
	}

	BOOL Pending(const Twin::Record* source, const RECT* from)
	{
		RECT part;
		for (DWORD i = 0; i < seeds.top && source->seeds; ++i)
			if (seeds.list[i].owner == source->owner && seeds.list[i].pending && IntersectRect(&part, &seeds.list[i].rect, from))
				return TRUE;
		return FALSE;
	}

	__declspec(noinline) DWORD GameCaller()
	{
		if (!told.exeHi)
		{
			const BYTE* module = (const BYTE*)GetModuleHandle(NULL);
			const IMAGE_OPTIONAL_HEADER* header = &((const IMAGE_NT_HEADERS*)(module + ((const IMAGE_DOS_HEADER*)module)->e_lfanew))->OptionalHeader;
			told.exeLo = (DWORD)module + (header->SizeOfCode ? header->BaseOfCode : 0);
			told.exeHi = told.exeLo + (header->SizeOfCode ? header->SizeOfCode : header->SizeOfImage);
		}

		const DWORD* slot = (const DWORD*)_AddressOfReturnAddress();
		const NT_TIB* tib = (const NT_TIB*)__readfsdword(0x18);
		for (DWORD i = 0; i < 96 && (DWORD)(slot + i + 1) <= (DWORD)tib->StackBase; ++i)
		{
			DWORD value = slot[i];
			const BYTE* code = (const BYTE*)value;
			if (value - told.exeLo >= 6 && value - told.exeLo < told.exeHi - told.exeLo && (code[-5] == 0xE8 || code[-6] == 0xFF || code[-3] == 0xFF || code[-2] == 0xFF))
				return value;
		}
		return 0;
	}

	VOID Tell1x(const Twin::Record* target, Twin::Record* source, const RECT* from, DWORD caller, const CHAR* how)
	{
		if (!target->screen || (source && (source->told1x || source->screen || Twin::Carries4K(source, from) || Pending(source, from))))
			return;
		if (!caller)
			caller = GameCaller();
		if (source)
			source->told1x = TRUE;
		else if (!Once(caller))
			return;

		CHAR module[24];
		DWORD base = caller ? CrashModule(caller, module, sizeof(module)) : 0;
		if (!caller)
			StrCopy(module, "?");
		if (source)
			DebugLog("image stays 1x: a %dx%d %s with no 4K at %d,%d %dx%d reached the screen through game code at %s+%X (%s)", source->width, source->height,
				source->memory ? "memory image" : "surface", from->left, from->top, from->right - from->left, from->bottom - from->top, module, caller - base, how);
		else
			DebugLog("image stays 1x: memory no twin covers reached the screen through game code at %s+%X (%s)", module, caller - base, how);
	}

	VOID SeedMemory(const BYTE* first, LONG pitch, LONG width, LONG height, LONG frame)
	{
		DWORD id;
		RECT rect;
		Twin::Record* twin = Twin::MemoryTwin(first, pitch, width, height, frame >= 0, &id, &rect);
		if (twin && frame < 0)
			Twin::MarkDirty(twin, rect);
		else if (twin)
			SeedAt(twin, id, &rect, &rect, frame, FALSE, FALSE);
	}

	VOID SeedBlit(const Blit* b)
	{
		LONG w = loader.rect.right - loader.rect.left, h = loader.rect.bottom - loader.rect.top;
		const BYTE* origin = loader.data + loader.rect.top * loader.pitch + loader.rect.left;
		if (loader.frame < 0 || b->srcPitch != loader.pitch || b->src < origin)
			return;

		DWORD offset = DWORD(b->src - origin);
		LONG sx = LONG(offset % (DWORD)loader.pitch), sy = LONG(offset / (DWORD)loader.pitch);
		LONG cw = min(b->width, w - sx), ch = min(b->height, h - sy);
		if (cw <= 0 || ch <= 0)
			return;

		DWORD id;
		LONG x, y;
		Twin::Record* twin = Twin::FindAddress(b->dst, &id);
		if (!twin)
		{
			RECT whole;
			twin = Twin::MemoryTwin(b->dst, b->dstPitch, b->width, b->height, TRUE, &id, &whole);
			if (twin && Once(HashText(Pack::EntryName(loader.frame)) + 5))
				DebugLog("loader blit into memory with no twin: made a %dx%d twin for %s at %d,%d", b->width, b->height, Pack::EntryName(loader.frame), sx, sy);
			if (!twin || !Twin::PixelPos(twin, b->dst, b->dstPitch, b->width, b->height, &x, &y))
				return;
		}
		else if (twin->memory || !Twin::PixelPos(twin, b->dst, b->dstPitch, b->width, b->height, &x, &y))
			return;

		RECT rect = { x, y, x + cw, y + ch };
		RECT place = { x - sx, y - sy, x - sx + w, y - sy + h };
		SeedAt(twin, id, &rect, &place, loader.frame, FALSE, FALSE);
	}

	VOID OnIndexedBlit(const BYTE* src, LONG srcPitch, const DWORD* dst, LONG dstPitch, LONG width, LONG height)
	{
		if (!src || !dst || width <= 0 || height <= 0 || !Twin::Active())
			return;

		static LONG told;
		DWORD id;
		if (told < 64 && !Twin::FindAddress(dst, &id))
		{
			++told;
			const BYTE* origin = loader.data ? loader.data + loader.rect.top * loader.pitch + loader.rect.left : NULL;
			DebugLog("indexed blit %d into memory with no twin: %dx%d dst %p src %p in %s, loader frame %d %s depth %d pitch %d/%d %s", told, width, height, dst, src,
				DescribeMemory(src), loader.frame, Pack::EntryName(loader.frame), loader.depth, srcPitch, loader.pitch, origin && src >= origin ? "inside" : "before decode");
		}

		Blit b = { src, srcPitch, dst, dstPitch, width, height };
		if (!loader.depth)
			SeedBlit(&b);
		else if (loader.count < INSIDE_MAX)
			loader.inside[loader.count++] = b;
	}

	VOID PrefetchUnit(const Archive* archive, const CHAR* record)
	{
		const CHAR* tail = ArchiveTail(archive);
		BOOL battle = !StrCompareInsensitive(tail, "BatUnits.ff"), map = !StrCompareInsensitive(tail, "IsoUnit.ff");
		if ((!battle && !map) || StrCompareNum(record, "G000UU", 6) || StrLength(record) < 17)
			return;

		CHAR prefix[40];
		StrPrint(prefix, "%s/%.10s", battle ? "BATUNITS.FF" : "ISOUNIT.FF", record);
		CharUpper(prefix);
		LONG at = battle ? (LONG)StrLength(prefix) + 6 : -1;
		CHAR side = battle ? record[16] : 0;
		if (Once(HashText(prefix) + side + 4))
			Pack::Prefetch(prefix, at, side);
	}

	struct Decoded {
		const BYTE* lo;
		const BYTE* hi;
		CHAR name[96];
	};

	struct {
		Decoded list[512];
		DWORD next;
	} decodes;

	VOID NoteDecode(const StoreImage* s, Archive* archive, const BYTE* desc)
	{
		const BYTE* raw = *(const BYTE* const*)(desc + 4);
		const LONG* rect = (const LONG*)(desc + 8);
		LONG pitchField = *(const LONG*)(desc + 0x18);
		if (!raw || pitchField <= 0 || rect[3] <= rect[1])
			return;
		LONG rowBytes = desc[0] ? (pitchField >> 1) * (LONG)sizeof(DWORD) : pitchField;
		Decoded* d = &decodes.list[decodes.next++ % 512];
		d->lo = raw + rect[1] * rowBytes;
		d->hi = raw + rect[3] * rowBytes;
		CHAR key[160];
		const CHAR* name = !s ? "no store" : !s->file ? "store with no file" : archive ? Key(archive, s->file->name, key) : s->file->name;
		StrPrint(d->name, "%.70s (%s%s)", name, desc[0] ? "memory" : "loader", archive ? "" : ", no archive");
	}

	const CHAR* DescribeMemory(const VOID* p)
	{
		for (DWORD i = 0; i < 512; ++i)
		{
			const Decoded* d = &decodes.list[(decodes.next - 1 - i) % 512];
			if (d->lo && (const BYTE*)p >= d->lo && (const BYTE*)p < d->hi)
				return d->name;
		}
		return "no recent store decode";
	}

	VOID StoreDrawn(VOID* thisObj, const BYTE* desc)
	{
		if (desc && !desc[0])
			loader.frame = -1;
		StoreImage* s = desc ? FindStore(thisObj, FALSE) : NULL;
		Archive* archive = s && s->file ? FileArchive(s->file) : NULL;
		if (desc)
			NoteDecode(s, archive, desc);
		const BYTE* data = archive ? *(const BYTE* const*)(desc + 4) : NULL;
		const LONG* rect = (const LONG*)(desc + 8);
		LONG pitchField = data ? *(const LONG*)(desc + 0x18) : 0;
		if (pitchField <= 0 || rect[2] <= rect[0] || rect[3] <= rect[1])
			return;

		LONG width = rect[2] - rect[0], height = rect[3] - rect[1];
		LONG frame = Lookup(archive, s->file->name, &width, &height);
		PrefetchUnit(archive, s->file->name);
		if (frame >= 0 && width * height >= WAIT_PIXELS)
		{
			LONG asked = Pack::QueuedFor(frame);
			LARGE_INTEGER t0, t1, f;
			QueryPerformanceCounter(&t0);
			LONG state = Pack::Wait(frame, WAIT_MS);
			QueryPerformanceCounter(&t1);
			QueryPerformanceFrequency(&f);
			CHAR key[160];
			if (Once(HashText(Key(archive, s->file->name, key)) + 3))
				DebugLog("image waited %u ms for the 4K of %s (%s, %s %d ms before)", (DWORD)((t1.QuadPart - t0.QuadPart) * 1000 / f.QuadPart), key,
					state > 0 ? "ready" : "not ready, shows 1x first", asked < 0 ? "first asked for" : "asked for", asked < 0 ? 0 : asked);
		}
		else
			Pack::State(frame);
		if (!desc[0])
		{
			loader.data = data;
			loader.pitch = pitchField;
			SetRect(&loader.rect, rect[0], rect[1], rect[0] + width, rect[1] + height);
			loader.frame = frame;
			return;
		}

		LONG pitch = pitchField >> 1;
		const BYTE* first = data + (rect[1] * pitch + rect[0]) * sizeof(DWORD);
		if (s->first != first)
			Twin::DropMemory(s->first);
		s->first = first;
		SeedMemory(first, pitch, width, height, frame);
	}

	VOID __fastcall StoreDraw(VOID* thisObj, DWORD /*edx*/, const BYTE* desc)
	{
		BOOL active = Twin::Active();
		loader.depth += active;
		((VOID(__thiscall*)(VOID*, const BYTE*))sub_StoreDraw)(thisObj, desc);
		if (!active)
			return;

		StoreDrawn(thisObj, desc);
		if (--loader.depth)
			return;
		DWORD count = loader.count;
		loader.count = 0;
		for (DWORD i = 0; i < count; ++i)
			SeedBlit(&loader.inside[i]);
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
			Twin::DropMemory(s->first);
			s->first = NULL;
			s->file = file;
			CHAR key[160];
			if (file)
			{
				Pack::State(Pack::Find(Key(FileArchive(file), file->name, key)));
				PrefetchUnit(FileArchive(file), file->name);
			}
		}
		return image;
	}

	VOID* __fastcall StoreFree(VOID* thisObj, DWORD /*edx*/, DWORD flags)
	{
		RemoveStore(thisObj);
		return ((VOID*(__thiscall*)(VOID*, DWORD))sub_StoreFree)(thisObj, flags);
	}

	Face* FindFace(const BYTE* data)
	{
		for (DWORD i = 0; i < FACE_SLOTS; ++i)
			if (faces.list[i].data == data)
				return &faces.list[i];
		return NULL;
	}

	LONG FaceFrame(const Face* face)
	{
		Archive* archive;
		ArchiveFile* file = FaceFile(face->name, &archive);
		if (file)
		{
			LONG w = face->width, h = face->height;
			LONG frame = Lookup(archive, file->name, &w, &h);
			return w == face->width && h == face->height ? frame : -1;
		}
		if (Once(HashText(face->name) + 2))
			DebugLog("image stays 1x: face %s has no record of that name in Faces.ff", face->name);
		return -1;
	}

	// Discipl2.exe v2.02b 0x005A5F21: the face cache reads Faces.ff itself, so faces never pass the image store
	BYTE __stdcall FaceLoad(const BYTE* buffer, const DWORD* size, VOID* unused, VOID* archive, const CHAR* name)
	{
		BYTE ok = ((BYTE(__stdcall*)(const BYTE*, const DWORD*, VOID*, VOID*, const CHAR*))sub_FaceLoad)(buffer, size, unused, archive, name);
		const BYTE* data = ok && buffer && size && name && *name ? *(const BYTE* const*)(buffer + 4) : NULL;
		if (!data)
			return ok;

		Twin::DropMemory(data);
		Face* face = FindFace(data);
		if (!face)
		{
			face = &faces.list[faces.next];
			faces.next = (faces.next + 1) % FACE_SLOTS;
		}
		face->data = data;
		face->width = (LONG)size[0];
		face->height = (LONG)size[1];
		face->pitch = 0;
		lstrcpyn(face->name, name, NAME_LENGTH);
		Pack::State(FaceFrame(face));
		return ok;
	}

	VOID OnFaceCopy(const DWORD* src, LONG srcPitch)
	{
		Face* face = src && Twin::Active() ? FindFace((const BYTE*)src) : NULL;
		if (!face || face->pitch == srcPitch)
			return;
		face->pitch = srcPitch;
		SeedMemory((const BYTE*)src, srcPitch, face->width, face->height, FaceFrame(face));
	}

	VOID OnFileOpen(const CHAR* path, HANDLE handle)
	{
		DWORD length = path ? StrLength(path) : 0;
		if (handle != INVALID_HANDLE_VALUE && archiveCsReady && length > 3 && !StrCompareInsensitive(path + length - 3, ".ff") && Pack::Present())
			FindArchive(path);
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!archiveCsReady)
		{
			InitializeCriticalSection(&archiveCs);
			archiveCsReady = TRUE;
		}
		if (!Pack::Present())
			return;

		DWORD load = 0, draw = 0, storeFree = 0;
		if (hookSpace->image_store_load_slot && ReadDWord(hooker, hookSpace->image_store_load_slot, &load) && load == f(hookSpace->image_store_load)
			&& ReadDWord(hooker, hookSpace->image_store_draw_slot, &draw) && draw == f(hookSpace->image_store_draw))
		{
			sub_StoreLoad = load;
			sub_StoreDraw = draw;
			PatchDWord(hooker, hookSpace->image_store_load_slot, (DWORD)StoreLoad);
			PatchDWord(hooker, hookSpace->image_store_draw_slot, (DWORD)StoreDraw);
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
		Pack::Start();
	}
}
