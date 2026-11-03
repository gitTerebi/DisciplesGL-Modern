#include "StdAfx.h"
#include "Config.h"
#include "FastAI.h"

namespace FastAI
{
	#define FAST_AI_INTERVAL 10
	#define FAST_AI_BUDGET 9

	HWND aiWindow;
	UINT_PTR timerId;
	UINT stockInterval;
	UINT currentInterval;

	UINT Interval()
	{
		return config.ai.fast && config.ai.thinking && stockInterval > FAST_AI_INTERVAL ? FAST_AI_INTERVAL : stockInterval;
	}

	UINT_PTR __stdcall SetTimerHook(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, TIMERPROC lpTimerFunc)
	{
		if (hWnd && hWnd == aiWindow && !lpTimerFunc)
		{
			timerId = nIDEvent;
			stockInterval = uElapse;
			uElapse = currentInterval = Interval();
		}

		return SetTimer(hWnd, nIDEvent, uElapse, lpTimerFunc);
	}

	VOID SetWindow(HWND hWnd)
	{
		aiWindow = hWnd;
	}

	BOOL Thinking()
	{
		return config.ai.fast && config.ai.thinking;
	}

	VOID Tick(HWND hWnd, WNDPROC proc, WPARAM wParam, LPARAM lParam)
	{
		if (hWnd != aiWindow || !stockInterval)
			return;

		UINT interval = Interval();
		if (interval != currentInterval)
		{
			currentInterval = interval;
			SetTimer(hWnd, timerId, interval, NULL);
		}

		if (wParam != timerId || !Thinking())
			return;

		LARGE_INTEGER freq, start, now;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&start);
		LONGLONG budget = freq.QuadPart * FAST_AI_BUDGET / 1000;

		do
		{
			CallWindowProc(proc, hWnd, WM_TIMER, wParam, lParam);
			QueryPerformanceCounter(&now);
		} while (now.QuadPart - start.QuadPart < budget && Thinking() && IsWindow(hWnd));
	}

	VOID Load(HOOKER hooker)
	{
		PatchImportByName(hooker, "SetTimer", SetTimerHook);
	}
}
