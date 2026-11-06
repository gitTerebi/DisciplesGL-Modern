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

	LONGLONG period;
	LONGLONG anchor;
	LONGLONG monitorCheckedAt;
	HMONITOR monitor;
	BOOL fixedPeriod;

	LONGLONG last;
	DOUBLE ax, ay;

	VOID Init(LONGLONG now)
	{
		if (freq)
			return;
		QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		period = freq / 240;
		anchor = now;
		HMODULE dwm = LoadLibrary("DWMAPI.dll");
		if (dwm)
			getTiming = (DWMGETCOMPOSITIONTIMINGINFO)GetProcAddress(dwm, "DwmGetCompositionTimingInfo");
	}

	VOID RefreshDwm(LONGLONG now)
	{
		Init(now);
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

	VOID CheckMonitor(HWND hWnd, LONGLONG now)
	{
		if (monitorCheckedAt && now - monitorCheckedAt < freq / 4)
			return;
		monitorCheckedAt = now;

		HMONITOR current = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
		if (current == monitor)
			return;
		monitor = current;
		fixedPeriod = FALSE;

		MONITORINFOEX info;
		info.cbSize = sizeof(info);
		if (!GetMonitorInfo(current, &info) || (info.dwFlags & MONITORINFOF_PRIMARY))
			return;

		DEVMODE mode = {};
		mode.dmSize = sizeof(mode);
		if (!EnumDisplaySettings(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) || mode.dmDisplayFrequency <= 1)
			return;

		period = freq / mode.dmDisplayFrequency;
		anchor = now;
		fixedPeriod = TRUE;
	}

	LONGLONG NextBoundary(LONGLONG now, LONGLONG step, LONGLONG origin)
	{
		LONGLONG offset = (now - origin) % step;
		if (offset < 0)
			offset += step;
		return now - offset + step;
	}

	LONGLONG Target(HWND hWnd, LONGLONG now)
	{
		RefreshDwm(now);
		CheckMonitor(hWnd, now);
		if (!fixedPeriod && dwmPeriod)
		{
			period = dwmPeriod;
			anchor = dwmAnchor - dwmPeriod / 2;
		}
		return NextBoundary(now, period, anchor);
	}

	LONGLONG NextFlush(LONGLONG now, LONGLONG fallbackStep)
	{
		RefreshDwm(now);
		return dwmPeriod ? NextBoundary(now, dwmPeriod, dwmAnchor) : fallbackStep * (now / fallbackStep + 1);
	}

	VOID Reset()
	{
		last = 0;
		ax = ay = 0.0;
	}

	BOOL Step(HWND hWnd, LONG tickX, LONG tickY, DWORD ticksPerSecond, POINT* offset)
	{
		LONGLONG now;
		QueryPerformanceCounter((LARGE_INTEGER*)&now);
		LONGLONG target = Target(hWnd, now);

		if (target == last)
			return FALSE;

		DOUBLE dt = last ? DOUBLE(target - last) / freq : DOUBLE(period) / freq;
		last = target;
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
