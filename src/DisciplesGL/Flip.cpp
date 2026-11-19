#include "StdAfx.h"
#include "Flip.h"

#define FLIP_HITS 200
#define FLIP_LINES 20
#define FLIP_EIPS 64
#define FLIP_BLTS 40
#define FLIP_PAGE 4096

namespace Flip
{
	const LONG rows[3] = { 100, 300, 400 };

	struct {
		Twin::Record* pending;
		Twin::Record* watched;
		DWORD watchedId;
		volatile LONG state;
		BOOL checked;
		DWORD expected[3], mirrored[3];
		BYTE* volatile lo;
		BYTE* volatile hi;
		DWORD protect;
		PVOID handler;
		volatile LONG hits, lines, removing;
		volatile DWORD rearmThread;
		BYTE* rearm[4];
		volatile LONG rearmCount;
		DWORD eips[FLIP_EIPS];
		volatile LONG eipCount;
		LONG blts;
	} flip;

	DWORD Crc(const DWORD* row, LONG width, BOOL mirrored)
	{
		DWORD crc = 0xFFFFFFFF;
		for (LONG i = 0; i < width; ++i)
		{
			DWORD pixel = row[mirrored ? width - 1 - i : i];
			for (DWORD b = 0; b < 4; ++b)
			{
				crc ^= (pixel >> (b * 8)) & 0xFF;
				for (DWORD k = 0; k < 8; ++k)
					crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
			}
		}
		return ~crc;
	}

	VOID Hash(const Twin::Record* twin, DWORD* normal, DWORD* mirrored)
	{
		for (DWORD i = 0; i < 3; ++i)
		{
			normal[i] = mirrored[i] = 0;
			if (rows[i] < twin->height)
			{
				const DWORD* row = (const DWORD*)twin->base + rows[i] * twin->pitch;
				normal[i] = Crc(row, twin->width, FALSE);
				mirrored[i] = Crc(row, twin->width, TRUE);
			}
		}
	}

	const CHAR* Match(const DWORD* normal)
	{
		BOOL original = TRUE, flipped = TRUE;
		for (DWORD i = 0; i < 3; ++i)
		{
			original = original && normal[i] == flip.expected[i];
			flipped = flipped && normal[i] == flip.mirrored[i];
		}
		return original ? "matches the original" : flipped ? "matches the MIRRORED original" : "matches neither";
	}

	VOID Guard(BOOL on)
	{
		DWORD old;
		if (flip.lo && flip.hi > flip.lo)
			VirtualProtect(flip.lo, flip.hi - flip.lo, on ? flip.protect | PAGE_GUARD : flip.protect, &old);
	}

	VOID Disarm()
	{
		Guard(FALSE);
		flip.lo = flip.hi = NULL;
		flip.watched = NULL;
		flip.state = 0;
	}

	BOOL Inside(const VOID* p)
	{
		const Twin::Record* w = flip.watched;
		const BYTE* end = w->base + ((w->height - 1) * w->pitch + w->width) * sizeof(DWORD);
		return (const BYTE*)p >= w->base && (const BYTE*)p < end;
	}

	VOID Pos(const VOID* p, LONG* x, LONG* y)
	{
		LONG at = LONG(((const BYTE*)p - flip.watched->base) / (LONG)sizeof(DWORD));
		*x = at % flip.watched->pitch;
		*y = at / flip.watched->pitch;
	}

	LONG CALLBACK Handler(EXCEPTION_POINTERS* info)
	{
		EXCEPTION_RECORD* record = info->ExceptionRecord;
		CONTEXT* context = info->ContextRecord;
		DWORD old;
		if (record->ExceptionCode == STATUS_SINGLE_STEP)
		{
			if (!flip.rearmThread || flip.rearmThread != GetCurrentThreadId())
				return EXCEPTION_CONTINUE_SEARCH;
			LONG count = flip.rearmCount;
			for (LONG i = 0; i < count; ++i)
				if (flip.state == 2 && flip.rearm[i] >= flip.lo && flip.rearm[i] < flip.hi)
					VirtualProtect(flip.rearm[i], FLIP_PAGE, flip.protect | PAGE_GUARD, &old);
			flip.rearmCount = 0;
			flip.rearmThread = 0;
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		if (record->ExceptionCode != STATUS_GUARD_PAGE_VIOLATION || record->NumberParameters < 2)
			return EXCEPTION_CONTINUE_SEARCH;
		BYTE* at = (BYTE*)record->ExceptionInformation[1];
		if (!flip.lo || at < flip.lo || at >= flip.hi)
			return EXCEPTION_CONTINUE_SEARCH;

		LONG hit = InterlockedIncrement(&flip.hits);
		DWORD eip = context->Eip;
		BOOL fresh = TRUE;
		for (LONG i = 0; i < flip.eipCount && fresh; ++i)
			fresh = flip.eips[i] != eip;
		if (fresh && flip.eipCount < FLIP_EIPS)
			flip.eips[flip.eipCount++] = eip;
		if (InterlockedIncrement(&flip.lines) <= FLIP_LINES || fresh)
		{
			CHAR module[24];
			DWORD base = CrashModule(eip, module, sizeof(module));
			const DWORD* stack = (const DWORD*)context->Esp;
			LONG x = 0, y = 0;
			if (flip.watched && Inside(at))
				Pos(at, &x, &y);
			DebugLog("flip: hit %d eip %08X %s+%X %s %08X (x %d y %d) thread %u stack %08X %08X %08X %08X", hit, eip, module, eip - base,
				record->ExceptionInformation[0] == 8 ? "exec" : record->ExceptionInformation[0] ? "write" : "read", (DWORD)at, x, y, GetCurrentThreadId(),
				stack[0], stack[1], stack[2], stack[3]);
		}

		if (hit < FLIP_HITS)
		{
			if (flip.rearmThread != GetCurrentThreadId())
			{
				flip.rearmCount = 0;
				flip.rearmThread = GetCurrentThreadId();
			}
			if (flip.rearmCount < 4)
				flip.rearm[flip.rearmCount++] = (BYTE*)((DWORD)at & ~(FLIP_PAGE - 1));
			context->EFlags |= 0x100;
		}
		else if (hit == FLIP_HITS)
		{
			Guard(FALSE);
			flip.state = 3;
			flip.removing = TRUE;
			DebugLog("flip: guard stopped after %d hits", hit);
		}
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	VOID NoteSource(Twin::Record* twin)
	{
		flip.pending = twin;
	}

	VOID Settle()
	{
		if (flip.removing && flip.handler && !flip.rearmThread)
		{
			RemoveVectoredExceptionHandler(flip.handler);
			flip.handler = NULL;
			flip.removing = FALSE;
			DebugLog("flip: handler removed");
		}
		if (flip.state != 1)
			return;

		DWORD normal[3], mirrored[3];
		Twin::Record* w = flip.watched;
		Hash(w, normal, mirrored);
		DebugLog("flip: settled twin %u rows 100/300/400 %08X %08X %08X mirrored %08X %08X %08X, %s", flip.watchedId, normal[0], normal[1], normal[2],
			mirrored[0], mirrored[1], mirrored[2], Match(normal));

		BYTE* end = w->base + ((w->height - 1) * w->pitch + w->width) * sizeof(DWORD);
		BYTE* lo = (BYTE*)(((DWORD)w->base + FLIP_PAGE - 1) & ~(FLIP_PAGE - 1));
		BYTE* hi = (BYTE*)((DWORD)end & ~(FLIP_PAGE - 1));
		MEMORY_BASIC_INFORMATION memory;
		if (hi <= lo || !VirtualQuery(lo, &memory, sizeof(memory)) || memory.State != MEM_COMMIT)
		{
			DebugLog("flip: no whole committed page to guard in %p-%p", w->base, end);
			flip.state = 3;
			return;
		}
		flip.protect = memory.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE);
		flip.hits = 0;
		flip.lines = 0;
		flip.eipCount = 0;
		flip.rearmCount = 0;
		flip.rearmThread = 0;
		flip.removing = FALSE;
		if (!flip.handler)
			flip.handler = AddVectoredExceptionHandler(1, Handler);
		flip.lo = lo;
		flip.hi = hi;
		flip.state = 2;
		Guard(TRUE);
		DebugLog("flip: guarding %p-%p (%u pages, protect %X) of %p-%p, handler %p", lo, hi, DWORD(hi - lo) / FLIP_PAGE, flip.protect, w->base, end, flip.handler);
	}

	VOID OnCopy(Twin::Record* target, DWORD dstId, const RECT* to, Twin::Record* source, DWORD srcId, const RECT* from, const CHAR* how, DWORD param)
	{
		if (!source)
			return;

		if (flip.pending && source == flip.pending && target != source && !target->screen)
		{
			if (flip.state)
			{
				DebugLog("flip: rearming, dropping the watch on twin %u", flip.watchedId);
				Disarm();
			}
			flip.pending = NULL;
			flip.watched = target;
			flip.watchedId = dstId;
			flip.checked = FALSE;
			flip.blts = 0;
			flip.state = 1;
			LONG width = to->right - to->left;
			for (DWORD i = 0; i < 3; ++i)
			{
				flip.expected[i] = flip.mirrored[i] = 0;
				if (to->left == 0 && to->right == target->width && rows[i] >= to->top && rows[i] < to->bottom)
				{
					const DWORD* row = (const DWORD*)source->base + (rows[i] - to->top + from->top) * source->pitch + from->left;
					flip.expected[i] = Crc(row, width, FALSE);
					flip.mirrored[i] = Crc(row, width, TRUE);
				}
			}
			DebugLog("flip: armed on twin %u %dx%d base %p pitch %d, %s param %X from twin %u rect %d,%d,%d,%d to %d,%d", dstId, target->width, target->height,
				target->base, target->pitch, how, param, srcId, from->left, from->top, from->right, from->bottom, to->left, to->top);
			DebugLog("flip: original rows 100/300/400 (read from the source) %08X %08X %08X mirrored %08X %08X %08X", flip.expected[0], flip.expected[1],
				flip.expected[2], flip.mirrored[0], flip.mirrored[1], flip.mirrored[2]);
			return;
		}

		if (flip.state >= 2 && source == flip.watched && !flip.checked)
		{
			flip.checked = TRUE;
			Guard(FALSE);
			DWORD normal[3], mirrored[3];
			Hash(source, normal, mirrored);
			if (flip.state == 2)
				Guard(TRUE);
			DebugLog("flip: first copy out of twin %u (%s param %X) to twin %u%s rect %d,%d,%d,%d to %d,%d: rows %08X %08X %08X mirrored %08X %08X %08X, %s",
				srcId, how, param, dstId, target->screen ? " screen" : "", from->left, from->top, from->right, from->bottom, to->left, to->top,
				normal[0], normal[1], normal[2], mirrored[0], mirrored[1], mirrored[2], Match(normal));
		}
	}

	VOID OnPixel(DWORD mode, const DWORD* src, const DWORD* dst, LONG width, LONG height, DWORD param)
	{
		if (!flip.watched || flip.blts >= FLIP_BLTS)
			return;
		BOOL s = Inside(src), d = Inside(dst);
		if (!s && !d)
			return;
		++flip.blts;
		LONG sx = 0, sy = 0, dx = 0, dy = 0;
		if (s)
			Pos(src, &sx, &sy);
		if (d)
			Pos(dst, &dx, &dy);
		DebugLog("flip: pixel op mode %u param %X %dx%d src %p%s %d,%d dst %p%s %d,%d", mode, param, width, height, src, s ? " watched" : "", sx, sy,
			dst, d ? " watched" : "", dx, dy);
	}

	VOID OnBlt(const CHAR* how, StateBuffer* dst, StateBuffer* src, const RECT* dstRect, LONG x, LONG y, const RECT* srcRect, DWORD flags, const DDBLTFX* fx)
	{
		if (!flip.watched || flip.blts >= FLIP_BLTS)
			return;
		const Twin::Record* w = flip.watched;
		BOOL d = dst && (dst == w->owner || dst->data == w->base);
		BOOL s = src && (src == w->owner || src->data == w->base);
		if (!s && !d)
			return;
		++flip.blts;
		RECT none = { -1, -1, -1, -1 };
		RECT at = { x, y, x, y };
		const RECT* to = dstRect ? dstRect : how[3] == 'f' ? &at : &none;
		const RECT* from = srcRect ? srcRect : &none;
		DebugLog("flip: %s dst %p%s %d,%d,%d,%d src %p%s %d,%d,%d,%d flags %08X fx %s ddfx %08X", how, dst, d ? " watched" : "", to->left, to->top, to->right,
			to->bottom, src, s ? " watched" : "", from->left, from->top, from->right, from->bottom, flags, fx ? "set" : "null", fx ? fx->dwDDFX : 0);
	}

	VOID OnRelease(Twin::Record* twin)
	{
		if (twin == flip.pending)
			flip.pending = NULL;
		if (twin == flip.watched && flip.watched)
		{
			DebugLog("flip: watched twin %u released", flip.watchedId);
			Disarm();
		}
	}
}
