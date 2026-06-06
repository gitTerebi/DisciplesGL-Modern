/*
	MIT License

	Copyright (c) 2021 Oleksiy Ryabchun

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "stdafx.h"
#include "Config.h"
#include "Window.h"
#include "Mods.h"
#include "Resource.h"

Mod* mods;

namespace Mods
{
	VOID Load()
	{
		CHAR file[MAX_PATH];
		GetModuleFileName(NULL, file, sizeof(file));
		CHAR* p = StrLastChar(file, '\\');
		if (!p)
			return;
		StrCopy(++p, "mods\\");
		p += StrLength(p);
		StrCopy(p, "*.mod");

		WIN32_FIND_DATA fData;
		HANDLE hFile = FindFirstFile(file, &fData);
		if (hFile && hFile != INVALID_HANDLE_VALUE)
		{
			DWORD offset = 10000;
			Mod* last;
			do
			{
				StrCopy(p, fData.cFileName);
				HMODULE hModule = LoadLibrary(file);
				if (hModule)
				{
					Mod* mod = (Mod*)MemoryAlloc(sizeof(Mod));
					if (mod)
					{
						MemoryZero(mod, sizeof(Mod));

						mod->GetId = (MOD_GETID)GetProcAddress(hModule, "GetId");
						mod->GetName = (MOD_GETNAME)GetProcAddress(hModule, "GetName");
						mod->GetMenu = (MOD_GETMENU)GetProcAddress(hModule, "GetMenu");
						mod->SetHWND = (MOD_SETHWND)GetProcAddress(hModule, "SetHWND");
						mod->Launch = (MOD_LAUNCH)GetProcAddress(hModule, "Launch");
						mod->DrawFrame = (MOD_DRAWFRAME)GetProcAddress(hModule, "DrawFrame");

						if (mod->GetId && mod->GetName && mod->GetMenu && mod->SetHWND && mod->Launch && mod->DrawFrame)
						{
							mod->id = mod->GetId();
	
							Mod* check = NULL;
							if (mod->id)
								for (check = mods; check && (!check->id || *check->id != *mod->id); check = check->last)
									;

							if (!check)
							{
								mod->Launch();

								const CHAR* name = mod->GetName();
								if (name)
								{
									StrCopy(mod->name, name);
									mod->hMenu = mod->GetMenu(offset);
									offset += 1000;
								}

								if (!mods)
									mods = mod;
								else
									last->last = mod;
								last = mod;

								continue;
							}
						}
						
						MemoryFree(mod);
					}

					FreeLibrary(hModule);
				}
			} while (FindNextFile(hFile, &fData));

			FindClose(hFile);
		}

		MenuItemData mData;
		mData.childId = IDM_MODS;
		if (Window::GetMenuByChildID(&mData) && DeleteMenu(config.menu, IDM_MODS, MF_BYCOMMAND))
		{
			BOOL added = FALSE;
			for (Mod* mod = mods; mod; mod = mod->last)
			{
				if (mod->hMenu)
				{
					DWORD idx = 0;
					for (Mod* check = mods; check && check != mod; check = check->last)
						if (check->added && !StrCompare(check->name, mod->name))
							++idx;

					CHAR* pname;
					CHAR name[256];
					if (idx)
					{
						StrPrint(name, "%s\t#%d", mod->name, idx + 1);
						pname = name;
					}
					else
						pname = mod->name;

					if (AppendMenu(mData.hMenu, MF_POPUP, (UINT_PTR)mod->hMenu, pname))
						mod->added = added = TRUE;
				}
			}

			if (!added)
				DeleteMenu(config.menu, mData.index, MF_BYPOSITION);
		}
	}

	VOID SetHWND(HWND hWnd)
	{
		for (Mod* mod = mods; mod; mod = mod->last)
		{
			mod->hWnd = hWnd;
			mod->SetHWND(hWnd);
		}
	}

	VOID DrawFrame(DWORD x, DWORD y, DWORD width, DWORD height, DWORD format, DWORD pitch, VOID* buffer)
	{
		for (Mod* mod = mods; mod; mod = mod->last)
			mod->DrawFrame(x, y, width, height, format, pitch, buffer);
	}
}