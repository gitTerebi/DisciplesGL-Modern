#include "StdAfx.h"
#include "dwmapi.h"
#include "ScrollPacing.h"

namespace ScrollPacing
{
	#define UPLOAD_PHASE_AFTER_VBLANK 0.30
	#define MAX_STEP_SECONDS 0.05

	typedef HRESULT(WINAPI* DWMGETCOMPOSITIONTIMINGINFO)(HWND, DWM_TIMING_INFO*);

	LONGLONG freq;
	DWMGETCOMPOSITIONTIMINGINFO getTiming;
	LONGLONG dwmPeriod;
	LONGLONG dwmAnchor;
	LONGLONG dwmRefreshedAt;

	LONGLONG flushTarget;
	LONGLONG flushStep;
	DWORD flushes;
	DWORD steppedAt;

	LONGLONG last;
	DOUBLE ax, ay;

	VOID Init()
	{
		if (freq)
			return;
		QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		HMODULE dwm = LoadLibrary("DWMAPI.dll");
		if (dwm)
			getTiming = (DWMGETCOMPOSITIONTIMINGINFO)GetProcAddress(dwm, "DwmGetCompositionTimingInfo");
	}

	VOID RefreshDwm(LONGLONG now)
	{
		Init();
		if (dwmRefreshedAt && now - dwmRefreshedAt < freq / 4)
			return;
		dwmRefreshedAt = now;
		if (!getTiming)
			return;

		DWM_TIMING_INFO info = {};
		info.cbSize = sizeof(info);
		if (FAILED(getTiming(NULL, &info)) || !info.qpcRefreshPeriod || !info.qpcVBlank)
			return;

		dwmPeriod = (LONGLONG)info.qpcRefreshPeriod;
		dwmAnchor = (LONGLONG)info.qpcVBlank + (LONGLONG)(dwmPeriod * UPLOAD_PHASE_AFTER_VBLANK);
	}

	LONGLONG NextBoundary(LONGLONG now, LONGLONG step, LONGLONG origin)
	{
		LONGLONG offset = (now - origin) % step;
		if (offset < 0)
			offset += step;
		return now - offset + step;
	}

	LONGLONG NextFlush(LONGLONG now, LONGLONG step, BOOL capped)
	{
		RefreshDwm(now);
		BOOL vblank = dwmPeriod && !capped;
		flushStep = vblank ? dwmPeriod : step;
		flushTarget = vblank ? NextBoundary(now, dwmPeriod, dwmAnchor) : step * (now / step + 1);
		++flushes;
		return flushTarget;
	}

	VOID Reset()
	{
		last = 0;
		ax = ay = 0.0;
	}

	BOOL Step(LONG tickX, LONG tickY, DWORD ticksPerSecond, POINT* offset)
	{
		if (!flushTarget || steppedAt == flushes)
			return FALSE;
		steppedAt = flushes;

		DOUBLE dt = DOUBLE(last ? flushTarget - last : flushStep) / freq;
		last = flushTarget;
		if (dt > MAX_STEP_SECONDS)
			dt = MAX_STEP_SECONDS;

		ax += tickX * DOUBLE(ticksPerSecond) * dt;
		ay += tickY * DOUBLE(ticksPerSecond) * dt;

		LONG mx = (LONG)ax;
		LONG my = (LONG)ay;
		if (!mx && !my)
			return FALSE;

		ax -= mx;
		ay -= my;
		offset->x = mx;
		offset->y = my;
		return TRUE;
	}
}
