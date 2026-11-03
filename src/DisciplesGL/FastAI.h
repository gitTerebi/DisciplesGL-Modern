#pragma once

#include "Hooker.h"

namespace FastAI
{
	VOID SetWindow(HWND hWnd);
	VOID Tick(HWND hWnd, WNDPROC proc, WPARAM wParam, LPARAM lParam);
	VOID Load(HOOKER hooker);
}
