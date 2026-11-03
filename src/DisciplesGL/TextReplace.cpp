#include "StdAfx.h"
#include "TextReplace.h"
#include "Config.h"
#include "GLib.h"
#include "Hooks.h"
#include "Main.h"
#include "Resource.h"
#include "intrin.h"

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
#ifndef GL_R8
#define GL_R8 0x8229
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT 0x0CF5
#endif

#define TEXT_MAX_FONTS 16
#define TEXT_SLOTS 6
#define TEXT_LIST_LIMIT 16384
#define COVER_ALPHA 128

namespace TextReplace
{
	struct Glyph
	{
		DWORD font;
		LONG x, y;
		LONG l, t, r, b;
		DWORD fill, line;
		DWORD layer;
		BYTE ch, outline, alpha;
	};

	struct GlyphList
	{
		Glyph* items;
		DWORD count;
		DWORD capacity;
	};

	struct SurfaceEntry
	{
		OpenDrawSurface* surface;
		GlyphList list;
		DWORD* plane;
		LONG planeW, planeH;
		DWORD seq;
		BOOL dirty;
		BOOL pixelLock;
	};

	struct Slot
	{
		StateBuffer* frame;
		GlyphList list;
		BYTE* vis;
		BOOL masked;
		DWORD age;
	};

#pragma pack(push, 1)
	struct SheetHeader
	{
		CHAR magic[4];
		DWORD version, texels, distScale, distZero, atlasW, atlasH, fontCount, charsetCount, refBytes, packedSize;
		BYTE charsets[4];
	};

	struct SheetGlyph
	{
		WORD ax, ay, aw, ah;
		SHORT left, top, refX, refY;
		BYTE refW, refH;
		WORD reserved;
		DWORD refOffset;
	};
#pragma pack(pop)

	struct
	{
		const SheetHeader* header;
		const BYTE* fonts;
		const BYTE* refs;
		const BYTE* packed;
		DWORD fontSize;
		BOOL failed;
	} sheet;

	struct Ref
	{
		const BYTE* bits;
		LONG x, y, w, h;
	};

	struct FontCal
	{
		DWORD font;
		BOOL valid;
		DWORD index;
		DWORD base;
		BYTE set[256];
		BYTE charset;
		LONG offX, offY;
		BYTE ok[256];
		CHAR charX[256], charY[256];
	};

	struct Vertex
	{
		FLOAT x, y, u, v, vu, vv;
		DWORD fill, line;
	};

	BOOL initialized;
	BOOL rendererReady;
	CRITICAL_SECTION cs;

	SurfaceEntry* entries;
	DWORD entryCount;
	DWORD entryCapacity;
	SurfaceEntry* lastHit;

	Slot slots[TEXT_SLOTS];
	DWORD slotAge;

	GlyphList current;
	BYTE* currentVis;
	BOOL currentMasked;
	BOOL lastMasked;
	DWORD lastHash;

	FontCal cals[TEXT_MAX_FONTS];
	DWORD calCount;

	struct
	{
		BOOL ready;
		BOOL failed;
		GLuint program;
		GLuint vao;
		GLuint vbo;
		GLuint texture;
		GLuint visTexture;
		GLint visLocation;
		LONG visW, visH;
		Vertex* vertices;
		DWORD vertexCapacity;
	} gl;

	struct
	{
		LONG glyphs, skipped, copies, carried, occluded, dropped, pixelCopies, rows;
		LONG presents, presentGlyphs, maskedFrames, drawn, noAtlas;
		LONG cleanPresents, emptyDirty, slotMiss, slotEmpty, renders;
		LONG atlasCount, atlasMicro, calMicro, lockCleared;
		DWORD lastTick, logs;
	} diag;

	VOID DiagFlush()
	{
		DWORD tick = GetTickCount();
		if (diag.logs >= 60 || tick - diag.lastTick < 2000)
			return;

		DebugLog("TextReplace diag glyphs=%d skipped=%d copies=%d carried=%d occluded=%d dropped=%d pixelCopies=%d rows=%d presents=%d presentGlyphs=%d maskedFrames=%d drawn=%d noAtlas=%d",
			diag.glyphs, diag.skipped, diag.copies, diag.carried, diag.occluded, diag.dropped, diag.pixelCopies, diag.rows, diag.presents, diag.presentGlyphs, diag.maskedFrames, diag.drawn, diag.noAtlas);

		DebugLog("TextReplace diag2 cleanPresents=%d emptyDirty=%d renders=%d slotMiss=%d slotEmpty=%d atlas=%d in %d us calibrate %d us lockCleared=%d", diag.cleanPresents, diag.emptyDirty, diag.renders, diag.slotMiss, diag.slotEmpty, diag.atlasCount, diag.atlasMicro, diag.calMicro, diag.lockCleared);
		DWORD logs = diag.logs + 1;
		MemoryZero(&diag, sizeof(diag));
		diag.logs = logs;
		diag.lastTick = tick;
	}

	DWORD traceLeft;
	DWORD traceFrame;
	BOOL traceKey;

	SurfaceEntry* FindData(const VOID* data, LONG* ox, LONG* oy);

	VOID TraceSurface(const CHAR* tag, OpenDrawSurface* surface)
	{
		DebugLog("trace %s %08X type %d %dx%d bpp %d", tag, surface, surface->type, surface->mode.width, surface->mode.height, surface->mode.bpp);
	}

	VOID TraceFrame()
	{
		++traceFrame;
		BOOL key = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
		DWORD frames = 4;
		if (!(traceFrame & 255))
		{
			CHAR path[MAX_PATH];
			DWORD length = GetModuleFileName(NULL, path, sizeof(path));
			while (length && path[length - 1] != '\\')
				--length;
			StrCopy(path + length, "trace.flag");
			if (DeleteFile(path))
			{
				key = TRUE;
				frames = 30;
			}
		}
		if (key && !traceKey && !traceLeft)
		{
			traceLeft = frames;
			DebugLog("trace start frame %d surfaces %d", traceFrame, entryCount);
			for (DWORD i = 0; i < entryCount; ++i)
				TraceSurface("surface", entries[i].surface);
		}
		traceKey = key;

		if (traceLeft)
		{
			--traceLeft;
			DebugLog("trace frame %d", traceFrame);
		}
	}

	VOID TraceLock(OpenDrawSurface* surface, const RECT* rect, const VOID* ret)
	{
		if (!traceLeft)
			return;
		if (rect)
			DebugLog("trace lock %08X rect %d,%d,%d,%d ret %08X", surface, rect->left, rect->top, rect->right, rect->bottom, ret);
		else
			DebugLog("trace lock %08X full ret %08X", surface, ret);
	}

	VOID TraceBlt(const CHAR* op, OpenDrawSurface* dst, const RECT* dstRect, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect, DWORD flags, const VOID* ret)
	{
		if (!traceLeft)
			return;
		RECT d = { x, y, x, y }, s = { 0, 0, 0, 0 };
		if (dstRect)
			d = *dstRect;
		if (srcRect)
			s = *srcRect;
		DebugLog("trace %s dst %08X %d,%d,%d,%d src %08X %d,%d,%d,%d flags %08X ret %08X", op, dst, d.left, d.top, d.right, d.bottom, src, s.left, s.top, s.right, s.bottom, flags, ret);
	}

	VOID TracePixel(const CHAR* op, const VOID* dst, const VOID* src, LONG width, LONG height, DWORD alpha, const VOID* ret)
	{
		if (!traceLeft)
			return;
		LONG dx = 0, dy = 0, sx = 0, sy = 0;
		SurfaceEntry* d = dst ? FindData(dst, &dx, &dy) : NULL;
		SurfaceEntry* s = src ? FindData(src, &sx, &sy) : NULL;
		DebugLog("trace pixel %s dst %08X %d,%d src %08X %d,%d size %dx%d alpha %d ret %08X", op, d ? d->surface : NULL, dx, dy, s ? s->surface : NULL, sx, sy, width, height, alpha, ret);
	}

	BOOL Reserve(GlyphList* list, DWORD count)
	{
		if (count <= list->capacity)
			return TRUE;

		DWORD capacity = list->capacity ? list->capacity * 2 : 256;
		while (capacity < count)
			capacity *= 2;

		Glyph* items = (Glyph*)MemoryAlloc(capacity * sizeof(Glyph));
		if (!items)
			return FALSE;

		if (list->count)
			MemoryCopy(items, list->items, list->count * sizeof(Glyph));
		if (list->items)
			MemoryFree(list->items);

		list->items = items;
		list->capacity = capacity;
		return TRUE;
	}

	VOID Push(GlyphList* list, const Glyph* glyph)
	{
		if (list->count >= TEXT_LIST_LIMIT)
		{
			DWORD keep = list->count / 2;
			MemoryCopy(list->items, list->items + list->count - keep, keep * sizeof(Glyph));
			list->count = keep;
		}

		if (Reserve(list, list->count + 1))
			list->items[list->count++] = *glyph;
	}

	VOID Add(GlyphList* list, const Glyph* glyph)
	{
		for (DWORD i = 0; i < list->count; ++i)
		{
			Glyph* item = &list->items[i];
			if (item->x == glyph->x && item->y == glyph->y && item->font == glyph->font && item->ch == glyph->ch)
			{
				*item = *glyph;
				return;
			}
		}

		Push(list, glyph);
	}

	VOID FreeList(GlyphList* list)
	{
		if (list->items)
			MemoryFree(list->items);
		MemoryZero(list, sizeof(GlyphList));
	}

	const LONG* Symbol(DWORD font, BYTE ch)
	{
		return *(const LONG**)(font + 16 + 4 * ch);
	}

	BOOL Ink(const LONG* symbol, LONG x, LONG y)
	{
		if (x < 0 || y < 0 || x >= symbol[0] || y >= symbol[1])
			return FALSE;

		const BYTE* bits = (const BYTE*)symbol[4];
		return bits[y * symbol[3] + (x >> 3)] >> (7 - (x & 7)) & 1;
	}

	SurfaceEntry* Find(OpenDrawSurface* surface)
	{
		for (DWORD i = 0; i < entryCount; ++i)
			if (entries[i].surface == surface)
				return &entries[i];
		return NULL;
	}

	BOOL InSurface(SurfaceEntry* entry, const VOID* data, LONG* ox, LONG* oy)
	{
		OpenDrawSurface* surface = entry->surface;
		if (!surface->indexBuffer || !surface->indexBuffer->data)
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

	SurfaceEntry* FindData(const VOID* data, LONG* ox, LONG* oy)
	{
		if (lastHit && lastHit >= entries && lastHit < entries + entryCount && InSurface(lastHit, data, ox, oy))
			return lastHit;

		for (DWORD i = 0; i < entryCount; ++i)
			if (InSurface(&entries[i], data, ox, oy))
				return lastHit = &entries[i];

		return NULL;
	}

	BOOL Enabled()
	{
		return initialized && rendererReady && config.bpp32Hooked && !config.type.editor
			&& config.image.interpolation == InterpolateFsr && config.image.upscaling == UpscaleNone;
	}

	VOID SetRenderer(BOOL ready)
	{
		rendererReady = ready;
	}

	VOID OnBuffer(OpenDrawSurface* surface)
	{
		if (!config.bpp32Hooked || surface->mode.bpp == 8)
			return;

		if (!initialized)
		{
			InitializeCriticalSection(&cs);
			initialized = TRUE;
		}

		SurfaceEntry* found = Find(surface);
		if (found)
		{
			found->list.count = 0;
			return;
		}

		if (entryCount == entryCapacity)
		{
			DWORD capacity = entryCapacity ? entryCapacity * 2 : 128;
			SurfaceEntry* list = (SurfaceEntry*)MemoryAlloc(capacity * sizeof(SurfaceEntry));
			if (!list)
				return;

			if (entryCount)
				MemoryCopy(list, entries, entryCount * sizeof(SurfaceEntry));
			if (entries)
				MemoryFree(entries);

			entries = list;
			entryCapacity = capacity;
			lastHit = NULL;
		}

		SurfaceEntry* entry = &entries[entryCount++];
		MemoryZero(entry, sizeof(SurfaceEntry));
		entry->surface = surface;
		if (traceLeft)
			TraceSurface("create", surface);
	}

	VOID OnRelease(OpenDrawSurface* surface)
	{
		SurfaceEntry* entry = Find(surface);
		if (!entry)
			return;

		FreeList(&entry->list);
		if (entry->plane)
			MemoryFree(entry->plane);
		*entry = entries[--entryCount];
		lastHit = NULL;
	}

	VOID OnIsLost(OpenDrawSurface* surface)
	{
		SurfaceEntry* entry = Find(surface);
		if (entry)
			entry->pixelLock = TRUE;
	}

	VOID OnLock(OpenDrawSurface* surface)
	{
		SurfaceEntry* entry = Find(surface);
		if (!entry)
			return;

		if (!entry->pixelLock && entry->surface->type == SurfaceOther)
		{
			if (entry->list.count)
				++diag.lockCleared;
			entry->list.count = 0;
		}
		entry->pixelLock = FALSE;
	}

	VOID OnFill(OpenDrawSurface* surface)
	{
		SurfaceEntry* entry = Find(surface);
		if (traceLeft)
			DebugLog("trace fill %08X glyphs %d", surface, entry ? entry->list.count : 0);
		if (entry)
		{
			entry->list.count = 0;
			entry->dirty = TRUE;
		}
	}

	BOOL PlaneEnsure(SurfaceEntry* entry)
	{
		LONG width = entry->surface->mode.width;
		LONG height = entry->surface->mode.height;
		if (entry->plane && entry->planeW == width && entry->planeH == height)
			return TRUE;

		if (entry->plane)
			MemoryFree(entry->plane);

		entry->plane = (DWORD*)MemoryAlloc(width * height * sizeof(DWORD));
		if (!entry->plane)
		{
			entry->planeW = entry->planeH = 0;
			return FALSE;
		}

		MemoryZero(entry->plane, width * height * sizeof(DWORD));
		entry->planeW = width;
		entry->planeH = height;
		return TRUE;
	}

	const CHAR* coverCaller = "";

	VOID Occlude(SurfaceEntry* dst, LONG x, LONG y, LONG width, LONG height, const DWORD* src, LONG stride, BOOL keyed, DWORD key, DWORD seq)
	{
		GlyphList* list = &dst->list;
		DWORD keep = 0;
		for (DWORD i = 0; i < list->count; ++i)
		{
			Glyph* glyph = &list->items[i];
			LONG l = max(glyph->l, x), t = max(glyph->t, y);
			LONG r = min(glyph->r, x + width), b = min(glyph->b, y + height);
			if (l < r && t < b)
			{
				if (!src || !keyed)
				{
					BOOL trimmed = TRUE;
					if (t == glyph->t && b == glyph->b && (l == glyph->l || r == glyph->r))
					{
						if (l == glyph->l)
							glyph->l = r;
						else
							glyph->r = l;
					}
					else if (l == glyph->l && r == glyph->r && (t == glyph->t || b == glyph->b))
					{
						if (t == glyph->t)
							glyph->t = b;
						else
							glyph->b = t;
					}
					else
						trimmed = FALSE;

					if (trimmed)
					{
						if (glyph->l >= glyph->r || glyph->t >= glyph->b)
						{
							++diag.dropped;
							continue;
						}

						list->items[keep++] = *glyph;
						continue;
					}
				}

				if (PlaneEnsure(dst))
				{
					++diag.occluded;
					for (LONG py = t; py < b; ++py)
						for (LONG px = l; px < r; ++px)
							if (!src || !keyed || src[(py - y) * stride + (px - x)] != key)
								dst->plane[py * dst->planeW + px] = seq;
				}
			}

			list->items[keep++] = *glyph;
		}
		list->count = keep;
	}

	VOID Carry(SurfaceEntry* dst, LONG dx, LONG dy, SurfaceEntry* src, const RECT* srcRect, DWORD alpha, DWORD seq)
	{
		LONG sx = dx - srcRect->left;
		LONG sy = dy - srcRect->top;
		LONG width = dst->surface->mode.width;
		LONG height = dst->surface->mode.height;

		for (DWORD i = 0; i < src->list.count; ++i)
		{
			Glyph glyph = src->list.items[i];
			LONG l = max(max(glyph.l, srcRect->left) + sx, 0);
			LONG t = max(max(glyph.t, srcRect->top) + sy, 0);
			LONG r = min(min(glyph.r, srcRect->right) + sx, width);
			LONG b = min(min(glyph.b, srcRect->bottom) + sy, height);
			if (l >= r || t >= b)
				continue;

			if (src->plane)
			{
				for (LONG py = t; py < b; ++py)
					for (LONG px = l; px < r; ++px)
						if (src->plane[(py - sy) * src->planeW + (px - sx)] > glyph.layer && PlaneEnsure(dst))
							dst->plane[py * dst->planeW + px] = seq + 1;
			}

			DWORD layer = glyph.layer;
			glyph.x += sx;
			glyph.y += sy;
			glyph.l = l;
			glyph.t = t;
			glyph.r = r;
			glyph.b = b;
			glyph.alpha = (BYTE)(glyph.alpha * alpha / 255);
			glyph.layer = seq;
			Add(&dst->list, &glyph);
			++diag.carried;
		}
	}

	VOID Copy(SurfaceEntry* dst, LONG dx, LONG dy, SurfaceEntry* src, const DWORD* pixels, LONG stride, const RECT* srcRect, BOOL keyed, DWORD key, DWORD alpha)
	{
		LONG width = srcRect->right - srcRect->left;
		LONG height = srcRect->bottom - srcRect->top;
		if (width <= 0 || height <= 0)
			return;

		++diag.copies;
		DWORD seq = dst->seq += 2;
		if (dst->list.count && alpha >= COVER_ALPHA)
		{
			LONG x0 = max(dx, 0), y0 = max(dy, 0);
			LONG x1 = min(dx + width, (LONG)dst->surface->mode.width), y1 = min(dy + height, (LONG)dst->surface->mode.height);
			if (x0 < x1 && y0 < y1)
				Occlude(dst, x0, y0, x1 - x0, y1 - y0, pixels ? pixels + (y0 - dy) * stride + (x0 - dx) : NULL, stride, keyed, key, seq);
		}

		if (src && src != dst && src->list.count && alpha)
		{
			DWORD before = dst->list.count;
			Carry(dst, dx, dy, src, srcRect, alpha, seq);
			static DWORD offLogs;
			if (dst->surface->type == SurfaceOther && offLogs < 80 && dst->list.count != before)
			{
				++offLogs;
				DebugLog("TextReplace offcarry %s src %08X %dx%d glyphs %d rect %d,%d,%d,%d -> dst %08X %dx%d at %d,%d now %d (+%d) first ch %d alpha %d",
					coverCaller, src->surface, src->surface->mode.width, src->surface->mode.height, src->list.count, srcRect->left, srcRect->top, srcRect->right, srcRect->bottom,
					dst->surface, dst->surface->mode.width, dst->surface->mode.height, dx, dy, dst->list.count, dst->list.count - before, src->list.items[0].ch, alpha);
			}
			if (traceLeft)
				DebugLog("trace carry %s src %08X %dx%d glyphs %d rect %d,%d,%d,%d -> dst %08X %dx%d at %d,%d now %d (+%d) first ch %d",
					coverCaller, src->surface, src->surface->mode.width, src->surface->mode.height, src->list.count, srcRect->left, srcRect->top, srcRect->right, srcRect->bottom,
					dst->surface, dst->surface->mode.width, dst->surface->mode.height, dx, dy, dst->list.count, dst->list.count - before, src->list.items[0].ch);
		}
	}

	VOID OnBlit(OpenDrawSurface* dst, LONG x, LONG y, OpenDrawSurface* src, const RECT* srcRect, LONG srcStride, BOOL keyed, DWORD key)
	{
		if (!initialized || dst == src)
			return;
		coverCaller = "blit";

		SurfaceEntry* dstEntry = Find(dst);
		if (!dstEntry)
			return;

		dstEntry->dirty = TRUE;
		SurfaceEntry* srcEntry = Find(src);
		if (!dstEntry->list.count && (!srcEntry || !srcEntry->list.count))
			return;

		const DWORD* pixels = (const DWORD*)src->indexBuffer->data + srcRect->top * srcStride + srcRect->left;
		Copy(dstEntry, x, y, srcEntry, pixels, srcStride, srcRect, keyed, key, 255);
	}

	VOID OnFillRect(const VOID* data, const RECT* rect)
	{
		if (!initialized)
			return;
		coverCaller = "fillrect";

		LONG ox, oy;
		SurfaceEntry* entry = FindData(data, &ox, &oy);
		if (!entry || !entry->list.count)
			return;

		RECT area = { rect->left, rect->top, rect->right, rect->bottom };
		Copy(entry, ox + rect->left, oy + rect->top, NULL, NULL, 0, &area, FALSE, 0, 255);
	}

	VOID OnDiamond(const VOID* data, const POINT* loc, const RECT* rect)
	{
		if (!initialized)
			return;

		LONG ox, oy;
		SurfaceEntry* entry = FindData(data, &ox, &oy);
		if (!entry)
			return;

		entry->dirty = TRUE;
		if (!entry->list.count)
			return;

		LONG left = ox + loc->x, top = oy + loc->y;
		GlyphList* list = &entry->list;
		BOOL any = FALSE;
		for (DWORD i = 0; i < list->count && !any; ++i)
		{
			const Glyph* glyph = &list->items[i];
			any = glyph->l < left + 64 && glyph->r > left && glyph->t < top + 32 && glyph->b > top;
		}
		if (!any || !PlaneEnsure(entry))
			return;

		DWORD seq = entry->seq += 2;
		LONG offset = 30, width = 2;
		for (LONG idx = 0; idx < 32; ++idx)
		{
			LONG y = loc->y + idx;
			if (y >= rect->top && y < rect->bottom)
			{
				LONG x0 = max(loc->x + offset, rect->left) + ox;
				LONG x1 = min(loc->x + offset + width, rect->right) + ox;
				LONG py = y + oy;
				for (DWORD i = 0; i < list->count; ++i)
				{
					const Glyph* glyph = &list->items[i];
					if (py < glyph->t || py >= glyph->b)
						continue;
					LONG l = max(glyph->l, x0), r = min(glyph->r, x1);
					for (LONG px = l; px < r; ++px)
						entry->plane[py * entry->planeW + px] = seq;
				}
			}

			if (idx < 15)
			{
				offset -= 2;
				width += 4;
			}
			else if (idx > 15)
			{
				offset += 2;
				width -= 4;
			}
		}

		DWORD keep = 0;
		for (DWORD i = 0; i < list->count; ++i)
		{
			Glyph* glyph = &list->items[i];
			BOOL visible = FALSE;
			for (LONG py = glyph->t; py < glyph->b && !visible; ++py)
				for (LONG px = glyph->l; px < glyph->r; ++px)
					if (entry->plane[py * entry->planeW + px] <= glyph->layer)
					{
						visible = TRUE;
						break;
					}

			if (visible)
				list->items[keep++] = *glyph;
			else
				++diag.dropped;
		}
		list->count = keep;
	}

	VOID OnPixelCopy(const DWORD* srcData, LONG srcPitch, const POINT* srcPos, const DWORD* dstData, LONG dstPitch, const POINT* dstPos, const SIZE* size, BOOL keyed, DWORD key, DWORD alpha)
	{
		if (!initialized || !dstData || !srcData || size->cx <= 0 || size->cy <= 0)
			return;

		++diag.pixelCopies;
		coverCaller = "pixel";
		srcPitch /= 2;
		dstPitch /= 2;

		const DWORD* src = srcData + srcPitch * srcPos->y + srcPos->x;
		const DWORD* dst = dstData + dstPitch * dstPos->y + dstPos->x;

		LONG dx, dy;
		SurfaceEntry* dstEntry = FindData(dst, &dx, &dy);
		if (!dstEntry)
			return;

		dstEntry->dirty = TRUE;

		LONG sx = 0, sy = 0;
		SurfaceEntry* srcEntry = FindData(src, &sx, &sy);
		if (!dstEntry->list.count && (!srcEntry || !srcEntry->list.count))
			return;

		RECT srcRect = { sx, sy, sx + size->cx, sy + size->cy };
		Copy(dstEntry, dx, dy, srcEntry, src, srcPitch, &srcRect, keyed, key, alpha);
	}

	VOID OnPixelRows(const Hooks::BlendData* items, DWORD count)
	{
		if (!initialized)
			return;

		for (DWORD i = 0; i < count; ++i)
		{
			const Hooks::BlendData* item = &items[i];
			if (!item->length || !item->dstData)
				continue;

			LONG x, y;
			SurfaceEntry* entry = FindData(item->dstData, &x, &y);
			if (!entry || !entry->list.count)
				continue;

			++diag.rows;
			DWORD seq = entry->seq += 2;
			LONG length = item->length;
			for (DWORD g = 0; g < entry->list.count; ++g)
			{
				Glyph* glyph = &entry->list.items[g];
				if (y < glyph->t || y >= glyph->b)
					continue;

				LONG l = max(glyph->l, x), r = min(glyph->r, x + length);
				for (LONG px = l; px < r; ++px)
					if (item->mskData[px - x] >= COVER_ALPHA && PlaneEnsure(entry))
						entry->plane[y * entry->planeW + px] = seq;
			}
		}
	}

	FontCal* GetCal(DWORD font);
	BOOL CharOk(FontCal* cal, BYTE ch);

	BOOL OnGlyph(const DWORD* font, const VOID* data, POINT pt, const RECT* clipper, DWORD fill, DWORD line, BOOL outline, BYTE symbol)
	{
		++diag.glyphs;
		if (!Enabled() || !entryCount || !font || !*font)
			return FALSE;

		const LONG* sym = Symbol(*font, symbol);
		if (!sym || sym[0] <= 0 || sym[1] <= 0)
			return FALSE;

		LONG ox, oy;
		SurfaceEntry* entry = FindData(data, &ox, &oy);
		if (!entry)
			return FALSE;

		entry->dirty = TRUE;

		FontCal* cal = GetCal(*font);
		if (!cal || !cal->valid || !CharOk(cal, symbol))
			return FALSE;

		Glyph glyph;
		glyph.font = *font;
		glyph.ch = symbol;
		glyph.x = ox + pt.x;
		glyph.y = oy + pt.y;
		glyph.l = max(glyph.x, ox + clipper->left);
		glyph.t = max(glyph.y, oy + clipper->top);
		glyph.r = min(glyph.x + sym[0], ox + clipper->right);
		glyph.b = min(glyph.y + sym[1], oy + clipper->bottom);
		if (glyph.l >= glyph.r || glyph.t >= glyph.b)
			return TRUE;

		glyph.fill = Hooks::Convert565toRGB(fill) & 0x00FFFFFF;
		glyph.line = Hooks::Convert565toRGB(line) & 0x00FFFFFF;
		glyph.outline = outline ? 1 : 0;
		glyph.alpha = 255;
		glyph.layer = entry->seq += 2;

		GlyphList* list = &entry->list;
		DWORD keep = 0;
		LONG area = (glyph.r - glyph.l) * (glyph.b - glyph.t);
		for (DWORD i = 0; i < list->count; ++i)
		{
			Glyph* item = &list->items[i];
			LONG w = min(item->r, glyph.r) - max(item->l, glyph.l);
			LONG h = min(item->b, glyph.b) - max(item->t, glyph.t);
			LONG itemArea = (item->r - item->l) * (item->b - item->t);
			if (w > 0 && h > 0 && w * h * 2 >= min(area, itemArea))
				continue;
			list->items[keep++] = *item;
		}
		list->count = keep;

		Push(list, &glyph);
		++diag.skipped;

		if (traceLeft)
			DebugLog("trace glyph %08X at %d,%d ch %d font %08X", entry->surface, glyph.x, glyph.y, symbol, *font);
		return TRUE;
	}

	VOID OnPresent(OpenDrawSurface* back, StateBuffer* frame, LONG left, LONG top)
	{
		if (!initialized)
			return;

		SurfaceEntry* entry = Find(back);
		if (traceLeft)
			DebugLog("trace present back %08X entry %d glyphs %d frame %08X", back, entry != NULL, entry ? entry->list.count : 0, frame);
		++diag.presents;
		if (entry)
			diag.presentGlyphs += entry->list.count;

		EnterCriticalSection(&cs);
		{
			Slot* slot = NULL;
			for (DWORD i = 0; i < TEXT_SLOTS && !slot; ++i)
				if (slots[i].frame == frame)
					slot = &slots[i];

			if (!slot)
			{
				slot = &slots[0];
				for (DWORD i = 1; i < TEXT_SLOTS; ++i)
					if (slots[i].age < slot->age)
						slot = &slots[i];
			}

			if (entry && entry->dirty && !entry->list.count)
				++diag.emptyDirty;
			if (!entry || !entry->dirty)
			{
				++diag.cleanPresents;
				if (slot->frame != frame)
				{
					Slot* newest = NULL;
					for (DWORD i = 0; i < TEXT_SLOTS; ++i)
						if (&slots[i] != slot && slots[i].frame && (!newest || slots[i].age > newest->age))
							newest = &slots[i];

					slot->frame = frame;
					slot->list.count = 0;
					slot->masked = FALSE;
					if (newest && newest->list.count && Reserve(&slot->list, newest->list.count))
					{
						MemoryCopy(slot->list.items, newest->list.items, newest->list.count * sizeof(Glyph));
						slot->list.count = newest->list.count;
					}
				}

				slot->age = ++slotAge;
				LeaveCriticalSection(&cs);
				if (entry)
					entry->list.count = 0;
				return;
			}

			slot->frame = frame;
			slot->age = ++slotAge;
			slot->list.count = 0;
			slot->masked = FALSE;

			LONG width = config.mode->width;
			LONG height = config.mode->height;
			if (entry && entry->list.count && Reserve(&slot->list, entry->list.count))
			{
				for (DWORD i = 0; i < entry->list.count; ++i)
				{
					Glyph glyph = entry->list.items[i];
					if (entry->plane)
					{
						for (LONG py = glyph.t; py < glyph.b; ++py)
							for (LONG px = glyph.l; px < glyph.r; ++px)
							{
								if (entry->plane[py * entry->planeW + px] <= glyph.layer)
									continue;

								LONG fx = px - left, fy = py - top;
								if (fx < 0 || fy < 0 || fx >= width || fy >= height)
									continue;

								if (!slot->masked)
								{
									if (!slot->vis)
										slot->vis = (BYTE*)MemoryAlloc(width * height);
									if (!slot->vis)
										continue;
									MemorySet(slot->vis, 255, width * height);
									slot->masked = TRUE;
								}
								slot->vis[fy * width + fx] = 0;
							}
					}

					glyph.x -= left;
					glyph.y -= top;
					glyph.l = max(glyph.l - left, 0);
					glyph.t = max(glyph.t - top, 0);
					glyph.r = min(glyph.r - left, width);
					glyph.b = min(glyph.b - top, height);
					if (glyph.l < glyph.r && glyph.t < glyph.b && glyph.alpha)
						slot->list.items[slot->list.count++] = glyph;
				}
			}
		}
		LeaveCriticalSection(&cs);

		if (entry)
		{
			entry->list.count = 0;
			entry->dirty = FALSE;
		}
	}

	VOID OnFlushEnd(OpenDrawSurface* back)
	{
	}

	LONGLONG Micro()
	{
		LARGE_INTEGER now, freq;
		QueryPerformanceCounter(&now);
		QueryPerformanceFrequency(&freq);
		return now.QuadPart * 1000000 / freq.QuadPart;
	}

	BOOL SheetLoad()
	{
		if (sheet.header)
			return TRUE;
		if (sheet.failed)
			return FALSE;

		sheet.failed = TRUE;
		Stream stream;
		if (!Main::LoadResource(MAKEINTRESOURCE(IDR_TEXT_SHEET), &stream) || stream.size < sizeof(SheetHeader))
			return FALSE;

		const SheetHeader* header = (const SheetHeader*)stream.data;
		if (MemoryCompare(header->magic, "D2TS", 4) || header->version != 1 || !header->texels || !header->distScale
			|| !header->charsetCount || header->charsetCount > 4)
			return FALSE;

		DWORD fontSize = 16 + header->charsetCount * 256 * sizeof(SheetGlyph);
		DWORD tableSize = header->fontCount * fontSize;
		if (stream.size < sizeof(SheetHeader) + tableSize + header->refBytes + header->packedSize)
			return FALSE;

		sheet.fonts = stream.data + sizeof(SheetHeader);
		sheet.fontSize = fontSize;
		sheet.refs = sheet.fonts + tableSize;
		sheet.packed = sheet.refs + header->refBytes;
		sheet.header = header;
		sheet.failed = FALSE;
		return TRUE;
	}

	typedef LONG(__stdcall* RTLDECOMPRESSBUFFER)(USHORT format, PUCHAR out, ULONG outSize, PUCHAR in, ULONG inSize, PULONG done);

	BYTE* SheetAtlas()
	{
		HMODULE ntdll = GetModuleHandle("NTDLL.dll");
		RTLDECOMPRESSBUFFER decompress = ntdll ? (RTLDECOMPRESSBUFFER)GetProcAddress(ntdll, "RtlDecompressBuffer") : NULL;
		if (!decompress)
			return NULL;

		DWORD width = sheet.header->atlasW;
		DWORD height = sheet.header->atlasH;
		DWORD size = width * height;
		BYTE* atlas = (BYTE*)MemoryAlloc(size);
		if (!atlas)
			return NULL;

		ULONG done = 0;
		if (decompress(COMPRESSION_FORMAT_LZNT1, atlas, size, (PUCHAR)sheet.packed, sheet.header->packedSize, &done) || done != size)
		{
			DebugLog("TextReplace: glyph sheet decompress failed");
			MemoryFree(atlas);
			return NULL;
		}

		for (DWORD y = 0; y < height; ++y)
		{
			BYTE* row = atlas + y * width;
			for (DWORD x = 1; x < width; ++x)
				row[x] = (BYTE)(row[x] + row[x - 1]);
		}

		return atlas;
	}

	const CHAR* SheetName(DWORD index)
	{
		return (const CHAR*)(sheet.fonts + index * sheet.fontSize);
	}

	const SheetGlyph* SheetGlyphs(DWORD index, DWORD charset)
	{
		return (const SheetGlyph*)(sheet.fonts + index * sheet.fontSize + 16) + charset * 256;
	}

	BOOL GetRef(const SheetGlyph* glyph, Ref* ref)
	{
		if (!glyph->refW || !glyph->refH)
			return FALSE;

		ref->bits = sheet.refs + glyph->refOffset;
		ref->x = glyph->refX;
		ref->y = glyph->refY;
		ref->w = glyph->refW;
		ref->h = glyph->refH;
		return TRUE;
	}

	BOOL RefInk(const Ref* ref, LONG x, LONG y)
	{
		x -= ref->x;
		y -= ref->y;
		if (x < 0 || y < 0 || x >= ref->w || y >= ref->h)
			return FALSE;
		return ref->bits[y * ((ref->w + 7) >> 3) + (x >> 3)] >> (7 - (x & 7)) & 1;
	}

	BOOL SymbolBox(const LONG* symbol, RECT* box)
	{
		box->left = box->top = 0x7FFFFFFF;
		box->right = box->bottom = -1;
		for (LONG y = 0; y < symbol[1]; ++y)
			for (LONG x = 0; x < symbol[0]; ++x)
				if (Ink(symbol, x, y))
				{
					box->left = min(box->left, x);
					box->top = min(box->top, y);
					box->right = max(box->right, x + 1);
					box->bottom = max(box->bottom, y + 1);
				}
		return box->right > 0;
	}

	FLOAT Overlap(const LONG* symbol, LONG sx, LONG sy, const Ref* ref, LONG cx, LONG cy, LONG width, LONG height)
	{
		DWORD inter = 0, uni = 0;
		for (LONG y = -1; y <= height; ++y)
			for (LONG x = -1; x <= width; ++x)
			{
				BOOL a = Ink(symbol, sx + x, sy + y);
				BOOL b = RefInk(ref, cx + x, cy + y);
				inter += a && b;
				uni += a || b;
			}
		return uni ? (FLOAT)inter / uni : 0.0f;
	}

	FLOAT TrimScore(const LONG* symbol, const Ref* ref)
	{
		RECT sb;
		if (!SymbolBox(symbol, &sb))
			return 0.0f;

		LONG width = max(sb.right - sb.left, ref->w);
		LONG height = max(sb.bottom - sb.top, ref->h);

		FLOAT best = 0.0f;
		for (LONG dy = -1; dy <= 1; ++dy)
			for (LONG dx = -1; dx <= 1; ++dx)
			{
				FLOAT score = Overlap(symbol, sb.left, sb.top, ref, ref->x + dx, ref->y + dy, width, height);
				if (score > best)
					best = score;
			}
		return best;
	}

	DWORD InkCount(const LONG* symbol)
	{
		DWORD count = 0;
		for (LONG y = 0; y < symbol[1]; ++y)
			for (LONG x = 0; x < symbol[0]; ++x)
				count += Ink(symbol, x, y);
		return count;
	}

	const LONG* InkSymbol(DWORD font, BYTE ch)
	{
		const LONG* symbol = Symbol(font, ch);
		return symbol && symbol[0] > 0 && symbol[1] > 0 && InkCount(symbol) ? symbol : NULL;
	}

	FontCal* Calibrate(DWORD font)
	{
		if (calCount == TEXT_MAX_FONTS)
			return NULL;

		FontCal* cal = &cals[calCount++];
		MemoryZero(cal, sizeof(FontCal));
		cal->font = font;

		const CHAR* name = *(const CHAR**)font;
		if (!SheetLoad() || !name || IsBadStringPtrA(name, 64))
			return cal;

		DWORD index = 0;
		while (index < sheet.header->fontCount && lstrcmpiA(name, SheetName(index)))
			++index;
		if (index == sheet.header->fontCount)
		{
			DebugLog("TextReplace font '%s' not in glyph sheet, left as is", name);
			return cal;
		}

		FLOAT bestCharset = -1.0f;
		for (DWORD c = 0; c < sheet.header->charsetCount; ++c)
		{
			const SheetGlyph* glyphs = SheetGlyphs(index, c);
			FLOAT total = 0.0f;
			DWORD count = 0;
			for (DWORD ch = 0xC0; ch <= 0xFF && count < 24; ++ch)
			{
				const LONG* symbol = InkSymbol(font, (BYTE)ch);
				if (!symbol)
					continue;

				Ref ref;
				if (GetRef(&glyphs[ch], &ref))
					total += TrimScore(symbol, &ref);
				++count;
			}

			FLOAT score = count ? total / count : 0.0f;
			if (score > bestCharset)
			{
				bestCharset = score;
				cal->base = c;
				cal->charset = sheet.header->charsets[c];
			}
		}

		cal->index = index;
		const SheetGlyph* glyphs = SheetGlyphs(index, cal->base);
		static const CHAR test[] = "ABDEGHKMNOPRSWabdeghkmnopsw0235";
		LONG votes[17][64];
		MemoryZero(votes, sizeof(votes));
		for (const CHAR* ch = test; *ch; ++ch)
		{
			const LONG* symbol = InkSymbol(font, (BYTE)*ch);
			Ref ref;
			RECT sb;
			if (!symbol || !GetRef(&glyphs[(BYTE)*ch], &ref) || !SymbolBox(symbol, &sb))
				continue;

			LONG ax = sb.left - ref.x;
			LONG ay = sb.top - ref.y;
			FLOAT bestScore = -1.0f;
			LONG bx = 0, by = 0;
			for (LONG oy = max(ay - 1, -40); oy <= min(ay + 1, 23); ++oy)
				for (LONG ox = max(ax - 1, -8); ox <= min(ax + 1, 8); ++ox)
				{
					FLOAT score = Overlap(symbol, 0, 0, &ref, -ox, -oy, symbol[0], symbol[1]);
					if (score > bestScore)
					{
						bestScore = score;
						bx = ox;
						by = oy;
					}
				}

			++votes[bx + 8][by + 40];
		}

		LONG bestVotes = -1;
		for (LONG ox = 0; ox < 17; ++ox)
			for (LONG oy = 0; oy < 64; ++oy)
				if (votes[ox][oy] > bestVotes)
				{
					bestVotes = votes[ox][oy];
					cal->offX = ox - 8;
					cal->offY = oy - 40;
				}

		cal->valid = TRUE;
		DebugLog("TextReplace font %08X '%s': charset %d score %d/1000 offset %d,%d",
			font, name, cal->charset, (LONG)(bestCharset * 1000.0f), cal->offX, cal->offY);
		return cal;
	}

	FLOAT MatchGlyph(const LONG* symbol, const SheetGlyph* glyph, LONG offX, LONG offY, FLOAT threshold, LONG* bx, LONG* by)
	{
		Ref ref;
		if (!glyph->aw || !GetRef(glyph, &ref))
			return -1.0f;

		*bx = offX;
		*by = offY;
		FLOAT best = Overlap(symbol, 0, 0, &ref, -offX, -offY, symbol[0], symbol[1]);

		RECT sb;
		if (best < threshold && SymbolBox(symbol, &sb))
		{
			LONG ax = sb.left - ref.x;
			LONG ay = sb.top - ref.y;
			for (LONG oy = ay - 1; oy <= ay + 1; ++oy)
				for (LONG ox = ax - 1; ox <= ax + 1; ++ox)
				{
					FLOAT score = Overlap(symbol, 0, 0, &ref, -ox, -oy, symbol[0], symbol[1]);
					if (score > best)
					{
						best = score;
						*bx = ox;
						*by = oy;
					}
				}
		}
		return best;
	}

	BOOL CharOk(FontCal* cal, BYTE ch)
	{
		if (!cal->ok[ch])
		{
			cal->ok[ch] = 2;
			const LONG* symbol = InkSymbol(cal->font, ch);
			if (symbol && ch > 32)
			{
				FLOAT threshold = InkCount(symbol) < 8 ? 0.34f : 0.55f;
				LONG bx, by;
				if (MatchGlyph(symbol, &SheetGlyphs(cal->index, cal->base)[ch], cal->offX, cal->offY, threshold, &bx, &by) >= threshold)
				{
					cal->ok[ch] = 1;
					cal->set[ch] = (BYTE)cal->base;
				}
				else if (ch >= 0x80)
					for (DWORD set = 0; set < sheet.header->charsetCount; ++set)
						if (set != cal->base && MatchGlyph(symbol, &SheetGlyphs(cal->index, set)[ch], cal->offX, cal->offY, threshold, &bx, &by) >= threshold)
						{
							cal->ok[ch] = 1;
							cal->set[ch] = (BYTE)set;
							break;
						}

				if (cal->ok[ch] == 1)
				{
					cal->charX[ch] = (CHAR)bx;
					cal->charY[ch] = (CHAR)by;
				}
			}
		}
		return cal->ok[ch] == 1;
	}

	FontCal* GetCal(DWORD font)
	{
		for (DWORD i = 0; i < calCount; ++i)
			if (cals[i].font == font)
				return &cals[i];

		LONGLONG start = Micro();
		FontCal* cal = Calibrate(font);
		LONG spent = (LONG)(Micro() - start);
		diag.calMicro += spent;
		const CHAR* name = *(const CHAR**)font;
		DebugLog("TextReplace font %08X name '%s' calibrated in %d us", font, name && !IsBadStringPtrA(name, 64) ? name : "?", spent);
		return cal;
	}

	DWORD currentVisW, currentVisH;

	DWORD Hash(const GlyphList* list)
	{
		DWORD hash = 2166136261u;
		const BYTE* data = (const BYTE*)list->items;
		for (DWORD i = 0, size = list->count * sizeof(Glyph); i < size; ++i)
			hash = (hash ^ data[i]) * 16777619u;
		return hash ^ list->count;
	}

	BOOL Prepare(StateBufferAligned* frame, BOOL active)
	{
		if (!initialized)
			return FALSE;

		DiagFlush();
		current.count = 0;
		currentMasked = FALSE;
		if (active)
		{
			++diag.renders;
			++diag.slotMiss;
			EnterCriticalSection(&cs);
			{
				for (DWORD i = 0; i < TEXT_SLOTS; ++i)
				{
					Slot* slot = &slots[i];
					if (slot->frame != (StateBuffer*)frame)
						continue;

					--diag.slotMiss;
					if (!slot->list.count)
						++diag.slotEmpty;

					if (slot->list.count && Reserve(&current, slot->list.count))
					{
						MemoryCopy(current.items, slot->list.items, slot->list.count * sizeof(Glyph));
						current.count = slot->list.count;
					}

					if (slot->masked && slot->vis)
					{
						DWORD width = config.mode->width;
						DWORD height = config.mode->height;
						if (!currentVis || currentVisW != width || currentVisH != height)
						{
							if (currentVis)
								MemoryFree(currentVis);
							currentVis = (BYTE*)MemoryAlloc(width * height);
							currentVisW = currentVis ? width : 0;
							currentVisH = currentVis ? height : 0;
						}

						if (currentVis)
						{
							MemoryCopy(currentVis, slot->vis, width * height);
							currentMasked = TRUE;
						}
					}
					break;
				}
			}
			LeaveCriticalSection(&cs);
		}

		DWORD hash = Hash(&current);
		BOOL changed = hash != lastHash || currentMasked || lastMasked;
		lastHash = hash;
		lastMasked = currentMasked;
		return changed;
	}

	BOOL InitGL()
	{
		if (gl.ready)
			return TRUE;
		if (gl.failed)
			return FALSE;

		CHAR prefix[32];
		StrCopy(prefix, GLSL_VER_1_30);

		gl.program = GLCreateProgram();
		GLBindAttribLocation(gl.program, 0, "vCoord");
		GLBindAttribLocation(gl.program, 1, "vTex");
		GLBindAttribLocation(gl.program, 2, "vFill");
		GLBindAttribLocation(gl.program, 3, "vLine");
		GLBindAttribLocation(gl.program, 4, "vVis");

		GLuint vShader = GL::CompileShaderSource(IDR_TEXT_VERTEX, prefix, GL_VERTEX_SHADER);
		GLuint fShader = GL::CompileShaderSource(IDR_TEXT_FRAGMENT, prefix, GL_FRAGMENT_SHADER);
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
		gl.visLocation = GLGetUniformLocation(gl.program, "useVis");

		GLGenVertexArrays(1, &gl.vao);
		GLBindVertexArray(gl.vao);
		GLGenBuffers(1, &gl.vbo);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLEnableVertexAttribArray(0);
		GLVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		GLEnableVertexAttribArray(1);
		GLVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)8);
		GLEnableVertexAttribArray(4);
		GLVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)16);
		GLEnableVertexAttribArray(2);
		GLVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLvoid*)24);
		GLEnableVertexAttribArray(3);
		GLVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (GLvoid*)28);

		BYTE* atlas = SheetLoad() ? SheetAtlas() : NULL;
		if (atlas)
		{
			FLOAT texels = (FLOAT)sheet.header->texels;
			GLUniform2f(GLGetUniformLocation(gl.program, "texScale"), sheet.header->atlasW / texels, sheet.header->atlasH / texels);
			GLUniform2f(GLGetUniformLocation(gl.program, "sdf"), 255.0f / sheet.header->distScale, (FLOAT)sheet.header->distZero / sheet.header->distScale);

			GLGenTextures(1, &gl.texture);
			GLBindTexture(GL_TEXTURE_2D, gl.texture);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			GLPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			GLTexImage2D(GL_TEXTURE_2D, 0, GL_R8, sheet.header->atlasW, sheet.header->atlasH, 0, GL_RED, GL_UNSIGNED_BYTE, atlas);
			GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			MemoryFree(atlas);
		}

		GLGenTextures(1, &gl.visTexture);
		GLBindTexture(GL_TEXTURE_2D, gl.visTexture);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		gl.visW = gl.visH = 0;

		gl.ready = gl.program && gl.vao && gl.vbo && gl.texture && gl.visTexture;
		gl.failed = !gl.ready;
		if (gl.failed)
			DebugLog("TextReplace: GL setup failed, text overlay off");
		return gl.ready;
	}

	VOID Emit(Vertex* v, FLOAT x0, FLOAT y0, FLOAT x1, FLOAT y1, FLOAT u0, FLOAT v0, FLOAT u1, FLOAT v1, FLOAT a0, FLOAT b0, FLOAT a1, FLOAT b1, DWORD fill, DWORD line)
	{
		Vertex quad[6] = {
			{ x0, y0, u0, v0, a0, b0, fill, line },
			{ x1, y0, u1, v0, a1, b0, fill, line },
			{ x1, y1, u1, v1, a1, b1, fill, line },
			{ x0, y0, u0, v0, a0, b0, fill, line },
			{ x1, y1, u1, v1, a1, b1, fill, line },
			{ x0, y1, u0, v1, a0, b1, fill, line }
		};
		MemoryCopy(v, quad, sizeof(quad));
	}

	VOID Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight)
	{
		if (!current.count || rect->width <= 0 || rect->height <= 0)
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
			FLOAT sx = (FLOAT)rect->width / frameWidth;
			FLOAT sy = (FLOAT)rect->height / frameHeight;

			BOOL mapZoomed = Config::IsMapZoomed();
			RECT zoomRect = { 0, 0, 0, 0 };
			FLOAT zoom = 1.0f, zx = 0.0f, zy = 0.0f;
			if (mapZoomed)
			{
				Config::GetMapZoomRect(&zoomRect);
				zoom = config.mapZoom.current;
				FLOAT cx = zoomRect.right * 0.5f;
				FLOAT cy = zoomRect.bottom * 0.5f;
				zx = cx - cx / zoom;
				zy = cy - cy / zoom;
			}

			GLUseProgram(gl.program);
			GLBindVertexArray(gl.vao);
			GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);

			GLActiveTexture(GL_TEXTURE1);
			GLBindTexture(GL_TEXTURE_2D, gl.visTexture);
			if (currentMasked)
			{
				if (gl.visW != (LONG)currentVisW || gl.visH != (LONG)currentVisH)
				{
					gl.visW = currentVisW;
					gl.visH = currentVisH;
					GLTexImage2D(GL_TEXTURE_2D, 0, GL_R8, gl.visW, gl.visH, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
				}

				GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
				GLPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				GLTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gl.visW, gl.visH, GL_RED, GL_UNSIGNED_BYTE, currentVis);
				GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
				++diag.maskedFrames;
			}
			GLUniform1i(gl.visLocation, currentMasked ? 1 : 0);

			GLActiveTexture(GL_TEXTURE0);
			GLBindTexture(GL_TEXTURE_2D, gl.texture);

			DWORD needed = current.count * 6;
			if (needed > gl.vertexCapacity)
			{
				if (gl.vertices)
					MemoryFree(gl.vertices);
				gl.vertexCapacity = needed * 2;
				gl.vertices = (Vertex*)MemoryAlloc(gl.vertexCapacity * sizeof(Vertex));
				if (!gl.vertices)
					gl.vertexCapacity = 0;
			}

			DWORD count = 0;
			FLOAT rw = (FLOAT)rect->width;
			FLOAT rh = (FLOAT)rect->height;
			FLOAT vw = (FLOAT)(currentMasked ? currentVisW : frameWidth);
			FLOAT vh = (FLOAT)(currentMasked ? currentVisH : frameHeight);
			FLOAT texels = (FLOAT)sheet.header->texels;
			FLOAT baseX = (FLOAT)((LONG)(config.mode->width - frameWidth) >> 1);
			FLOAT baseY = (FLOAT)((LONG)(config.mode->height - frameHeight) >> 1);
			FLOAT aw = (FLOAT)sheet.header->atlasW;
			FLOAT ah = (FLOAT)sheet.header->atlasH;
			for (DWORD i = 0; gl.vertices && i < current.count; ++i)
			{
				const Glyph* glyph = &current.items[i];

				DWORD calIndex = 0;
				while (calIndex < calCount && cals[calIndex].font != glyph->font)
					++calIndex;
				if (calIndex == calCount)
					continue;

				BOOL inMap = mapZoomed && glyph->l < zoomRect.right && glyph->t < zoomRect.bottom;
				FLOAT gx = sx, gy = sy, ox = baseX, oy = baseY;
				FLOAT clipR = rw, clipB = rh;
				if (inMap)
				{
					gx = sx * zoom;
					gy = sy * zoom;
					ox = baseX + zx;
					oy = baseY + zy;
					clipR = zoomRect.right * sx;
					clipB = zoomRect.bottom * sy;
				}

				const SheetGlyph* sg = &SheetGlyphs(cals[calIndex].index, cals[calIndex].set[glyph->ch])[glyph->ch];
				if (!sg->aw)
				{
					++diag.noAtlas;
					continue;
				}

				FLOAT px = (glyph->x - ox + cals[calIndex].charX[glyph->ch]) * gx;
				FLOAT py = (FLOAT)MathFloor((glyph->y - oy) * gy + 0.5f) + cals[calIndex].charY[glyph->ch] * gy;
				FLOAT tx = gx / texels;
				FLOAT ty = gy / texels;

				const LONG* symbol = Symbol(glyph->font, glyph->ch);
				FLOAT x0 = px + sg->left * tx;
				FLOAT y0 = py + sg->top * ty;
				FLOAT x1 = x0 + sg->aw * tx;
				FLOAT y1 = y0 + sg->ah * ty;

				FLOAT qx0 = glyph->l > glyph->x ? max(x0, (FLOAT)MathFloor((glyph->l - ox) * gx)) : x0;
				FLOAT qy0 = glyph->t > glyph->y ? max(y0, (FLOAT)MathFloor((glyph->t - oy) * gy)) : y0;
				FLOAT qx1 = glyph->r < glyph->x + symbol[0] ? min(x1, (FLOAT)MathCeil((glyph->r - ox) * gx)) : x1;
				FLOAT qy1 = glyph->b < glyph->y + symbol[1] ? min(y1, (FLOAT)MathCeil((glyph->b - oy) * gy)) : y1;
				qx0 = max(qx0, 0.0f);
				qy0 = max(qy0, 0.0f);
				qx1 = min(qx1, clipR);
				qy1 = min(qy1, clipB);
				if (qx0 >= qx1 || qy0 >= qy1)
					continue;

				Emit(&gl.vertices[count],
					qx0 / rw * 2.0f - 1.0f, 1.0f - qy0 / rh * 2.0f, qx1 / rw * 2.0f - 1.0f, 1.0f - qy1 / rh * 2.0f,
					(sg->ax + (qx0 - x0) / tx) / aw, (sg->ay + (qy0 - y0) / ty) / ah,
					(sg->ax + (qx1 - x0) / tx) / aw, (sg->ay + (qy1 - y0) / ty) / ah,
					(qx0 / gx + ox) / vw, (qy0 / gy + oy) / vh, (qx1 / gx + ox) / vw, (qy1 / gy + oy) / vh,
					glyph->fill | ((DWORD)glyph->alpha << 24), glyph->outline ? glyph->line | 0xFF000000 : glyph->line);
				count += 6;
			}
			diag.drawn += count / 6;

			if (count)
			{
				GLViewport(rect->x, rect->y + offset, rect->width, rect->height);
				GLBufferData(GL_ARRAY_BUFFER, count * sizeof(Vertex), gl.vertices, GL_STREAM_DRAW);
				GLEnable(GL_BLEND);
				GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				GLDrawArrays(GL_TRIANGLES, 0, count);
				GLDisable(GL_BLEND);
			}
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
			GLDeleteTextures(1, &gl.texture);
			GLDeleteTextures(1, &gl.visTexture);
			GLDeleteBuffers(1, &gl.vbo);
			GLDeleteVertexArrays(1, &gl.vao);
			GLDeleteProgram(gl.program);
		}

		gl.ready = FALSE;
		gl.failed = FALSE;
		gl.program = gl.vao = gl.vbo = gl.texture = gl.visTexture = 0;
		gl.visW = gl.visH = 0;
	}
}
