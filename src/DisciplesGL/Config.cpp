/*
	MIT License

	Copyright (c) 2020 Oleksiy Ryabchun

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
#include "Mmsystem.h"
#include "intrin.h"
#include "Resource.h"
#include "Window.h"
#include "Hooker.h"

LONG GAME_WIDTH;
LONG GAME_HEIGHT;

FLOAT GAME_WIDTH_FLOAT;
FLOAT GAME_HEIGHT_FLOAT;

ConfigItems config;

DisplayMode modesList[] = {
	640, 480, 8,
	800, 600, 16,
	1024, 768, 16,
	1280, 1024, 16
};

const Resolution resolutionsList[] = {
	640, 480,
	800, 600,
	960, 768,
	1024, 600,
	1024, 768,
	1152, 864,
	1280, 720,
	1280, 768,
	1280, 800,
	1280, 960,
	1280, 1024,
	1360, 768,
	1366, 768,
	1400, 1050,
	1440, 900,
	1440, 1080,
	1536, 864,
	1600, 900,
	1600, 1200,
	1680, 1050,
	1920, 1080,
	1920, 1200,
	1920, 1440,
	2048, 1536,
	2560, 1440,
	2560, 1600,
	3840, 2160,
	7680, 4320
};

const BindInfo bindsList[] = {
	"ScrollUp", VK_UP, FALSE, 'W', ScopeMap, 0, 'W',
	"ScrollDown", VK_DOWN, FALSE, 'S', ScopeMap, 0, 'S',
	"ScrollLeft", VK_LEFT, FALSE, 'A', ScopeMap, 0, 'A',
	"ScrollRight", VK_RIGHT, FALSE, 'D', ScopeMap, 0, 'D',
	"EndTurn", VK_SPACE, FALSE, 0, ScopeMap, 0, 'E',
	"SpellBook", 'S', FALSE, 'E', ScopeMap, 0, 'R',
	"Capital", 'A', FALSE, 'Q', ScopeMap, 0, 'F',
	"QuickSave", 'Q', TRUE, VK_F5, ScopeMap, 0, VK_F5,
	"ClosePopup", VK_RETURN, FALSE, 0, ScopePopup, VK_ESCAPE, VK_SPACE,
	"AutoBattle", 'A', FALSE, 0, ScopeBattle, 0, 'Q',
	"Defend", 'D', FALSE, 0, ScopeBattle, 0, 0,
	"Retreat", 'R', FALSE, 0, ScopeBattle, 0, 'R',
	"Wait", 'W', FALSE, 0, ScopeBattle, 0, 0,
	"BuildStructure", 'S', FALSE, 0, ScopeScreen, 0, 'F',
	"ResearchSpells", 'R', FALSE, 0, ScopeScreen, 0, 'R',
	"Party", 'P', FALSE, 0, ScopeScreen, 0, 0,
	"Diplomacy", 'P', FALSE, 0, ScopeMap, 0, 0,
	"Exchange", 'X', FALSE, 0, ScopeMap, 0, 0,
	"Guard", 'G', FALSE, 0, ScopeMap, 0, 0,
	"NextLeader", VK_TAB, FALSE, 0, ScopeMap, 0, 0,
	"WandScroll", 'U', FALSE, 0, ScopeMap, 0, 0,
	"DropRod", 'R', FALSE, 0, ScopeMap, 0, 0,
	"ContinueMove", 'M', FALSE, 0, ScopeMap, 0, 0,
	"Banners", 'B', FALSE, 0, ScopeMap, 0, 0,
	"CenterStack", 'C', FALSE, 0, ScopeMap, 0, 0,
	"CenterTarget", 'D', FALSE, 0, ScopeMap, 0, 0,
	"QuestLog", VK_F3, FALSE, 0, ScopeMap, 0, 0,
	"Report", VK_F1, FALSE, 0, ScopeMap, 0, 0,
	"Options", VK_F4, FALSE, 0, ScopeMap, 0, 0,
	"TopPalette", VK_F5, FALSE, 0, ScopeMap, 0, 0,
	"GrowCity", 'G', FALSE, 0, ScopeScreen, 0, 0,
	"NextCity", VK_RIGHT, FALSE, 0, ScopeScreen, 0, 0,
	"PrevCity", VK_LEFT, FALSE, 0, ScopeScreen, 0, 0,
	"KnownSpells", 'S', FALSE, 0, ScopeScreen, 0, 0,
	"ResourcePopup", VK_F6, FALSE, 0, ScopeMap, 0, 0,
	"Heal", 'H', FALSE, 0, ScopeScreen, 0, 0,
	"Revive", 'R', FALSE, 0, ScopeScreen, 0, 0,
	"Dismiss", 'D', FALSE, 0, ScopeScreen, 0, 0,
	"LevelToggle", 'L', FALSE, 0, ScopeScreen, 0, 0,
	"UnitInfo", VK_TAB, FALSE, 0, ScopeScreen, 0, 0,
	"PickUpDrop", 'B', FALSE, 0, ScopeScreen, 0, 0,
	"Training", 'T', FALSE, 0, ScopeScreen, 0, 0,
	"SpellLevel1", '1', FALSE, 0, ScopeScreen, 0, 0,
	"SpellLevel2", '2', FALSE, 0, ScopeScreen, 0, 0,
	"SpellLevel3", '3', FALSE, 0, ScopeScreen, 0, 0,
	"SpellLevel4", '4', FALSE, 0, ScopeScreen, 0, 0,
	"SpellLevel5", '5', FALSE, 0, ScopeScreen, 0, 0,
	"OtherParty", VK_TAB, FALSE, 0, ScopeBattle, 0, 0,
	"QuickLoad", 'L', TRUE, VK_F9, ScopeMap, 0, VK_F9,
	"BattleClose", VK_ESCAPE, FALSE, 0, ScopeBattle, 0, 0
};

const KeyName keyNames[] = {
	'A', "A", 'B', "B", 'C', "C", 'D', "D", 'E', "E", 'F', "F", 'G', "G", 'H', "H", 'I', "I",
	'J', "J", 'K', "K", 'L', "L", 'M', "M", 'N', "N", 'O', "O", 'P', "P", 'Q', "Q", 'R', "R",
	'S', "S", 'T', "T", 'U', "U", 'V', "V", 'W', "W", 'X', "X", 'Y', "Y", 'Z', "Z",
	'0', "0", '1', "1", '2', "2", '3', "3", '4', "4", '5', "5", '6', "6", '7', "7", '8', "8", '9', "9",
	VK_F1, "F1", VK_F2, "F2", VK_F3, "F3", VK_F4, "F4", VK_F5, "F5", VK_F6, "F6",
	VK_F7, "F7", VK_F8, "F8", VK_F9, "F9", VK_F10, "F10", VK_F11, "F11", VK_F12, "F12",
	VK_SPACE, "Space", VK_RETURN, "Enter", VK_ESCAPE, "Esc", VK_TAB, "Tab", VK_BACK, "Backspace",
	VK_UP, "Up", VK_DOWN, "Down", VK_LEFT, "Left", VK_RIGHT, "Right",
	VK_INSERT, "Insert", VK_DELETE, "Delete", VK_HOME, "Home", VK_END, "End", VK_PRIOR, "Page Up", VK_NEXT, "Page Down",
	VK_NUMPAD0, "Num 0", VK_NUMPAD1, "Num 1", VK_NUMPAD2, "Num 2", VK_NUMPAD3, "Num 3", VK_NUMPAD4, "Num 4",
	VK_NUMPAD5, "Num 5", VK_NUMPAD6, "Num 6", VK_NUMPAD7, "Num 7", VK_NUMPAD8, "Num 8", VK_NUMPAD9, "Num 9",
	VK_MULTIPLY, "Num *", VK_DIVIDE, "Num /", VK_DECIMAL, "Num .",
	VK_OEM_1, ";", VK_OEM_2, "/", VK_OEM_3, "`", VK_OEM_4, "[", VK_OEM_5, "\\", VK_OEM_6, "]", VK_OEM_7, "'",
	VK_OEM_COMMA, ",", VK_OEM_PERIOD, "."
};

const DWORD keyNamesCount = sizeof(keyNames) / sizeof(KeyName);

Adjustment activeColors;
const Adjustment inactiveColors = {
	0.5f,
	0.0f,

	0.0f,
	0.15200001f,
	0.0700000003f,
	0.0f,

	1.0f,
	1.0f,
	1.0f,
	1.0f,

	0.5f,
	0.604000032f,
	0.539000034f,
	0.478000015f,

	0.0430000015f,
	0.0f,
	0.0f,
	0.0f,

	1.0f,
	1.0f,
	1.0f,
	1.0f
};

const Adjustment defaultColors = {
	0.5f,
	0.5f,

	0.0f,
	0.0f,
	0.0f,
	0.0f,

	1.0f,
	1.0f,
	1.0f,
	1.0f,

	0.5f,
	0.5f,
	0.5f,
	0.5f,

	0.0f,
	0.0f,
	0.0f,
	0.0f,

	1.0f,
	1.0f,
	1.0f,
	1.0f
};

namespace Config
{
	BOOL __stdcall EnumLocalesCount(LPTSTR lpLocaleString)
	{
		++config.locales.count;

		DWORD locale = strtoul(lpLocaleString, NULL, 16);

		CHAR name[64];
		GetLocaleInfo(locale, LOCALE_SENGLISHLANGUAGENAME, name, 64);

		CHAR ch = ToUpper(*name);
		HMENU* hPopup = &config.locales.menus[ch - 'A'];

		if (!*hPopup)
			*hPopup = CreatePopupMenu();

		return TRUE;
	}

	DWORD localeIndex;
	BOOL __stdcall EnumLocales(LPTSTR lpLocaleString)
	{
		DWORD locale = strtoul(lpLocaleString, NULL, 16);

		CHAR name[64];
		GetLocaleInfo(locale, LOCALE_SENGLISHLANGUAGENAME, name, 64);

		CHAR ch = ToUpper(*name);
		HMENU* hPopup = &config.locales.menus[ch - 'A'];
		if (*hPopup)
		{
			config.locales.list[++localeIndex] = locale;

			CHAR country[64];
			GetLocaleInfo(locale, LOCALE_SENGLISHCOUNTRYNAME, country, 64);

			CHAR title[256];
			StrPrint(title, "%s (%s)", name, country);

			CHAR temp[256];
			BOOL inserted = FALSE;
			INT count = GetMenuItemCount(*hPopup);
			for (INT i = 0; i < count; ++i)
			{
				if (GetMenuString(*hPopup, i, temp, sizeof(temp), MF_BYPOSITION))
				{
					if (StrCompare(title, temp) < 0)
					{
						InsertMenu(*hPopup, i, MF_STRING | MF_BYPOSITION, IDM_LOCALE_DEFAULT + localeIndex, title);
						inserted = TRUE;
						break;
					}
				}
			}

			if (!inserted)
				AppendMenu(*hPopup, MF_STRING, IDM_LOCALE_DEFAULT + localeIndex, title);
		}

		return TRUE;
	}

	BOOL Load()
	{
		HMODULE hModule = GetModuleHandle(NULL);
		GetModuleFileName(hModule, config.file, MAX_PATH);

		DWORD hSize;
		DWORD verSize = GetFileVersionInfoSize(config.file, &hSize);
		if (verSize)
		{
			CHAR* verData = (CHAR*)MemoryAlloc(verSize);
			{
				if (GetFileVersionInfo(config.file, hSize, verSize, verData))
				{
					VOID* buffer;
					UINT size;
					if (VerQueryValue(verData, "\\", &buffer, &size) && size)
					{
						VS_FIXEDFILEINFO* verInfo = (VS_FIXEDFILEINFO*)buffer;
						config.version.major.value = verInfo->dwProductVersionMS;
						config.version.minor.value = verInfo->dwProductVersionLS;
					}
				}
			}
			MemoryFree(verData);
		}

		StrCopy(StrLastChar(config.file, '\\') + 1, "disciple.ini");

		HOOKER hooker = CreateHooker(hModule);
		if (hooker)
		{
			config.type.sacred = RedirectImports(hooker, "smackw32.dll");
			config.type.editor = RedirectImports(hooker, "comdlg32.dll");
			ReleaseHooker(hooker);
		}

		if (config.type.sacred)
		{
			GAME_WIDTH = 640;
			GAME_HEIGHT = 480;
		}
		else
		{
			GAME_WIDTH = 800;
			GAME_HEIGHT = 600;
		}

		GAME_WIDTH_FLOAT = (FLOAT)GAME_WIDTH;
		GAME_HEIGHT_FLOAT = (FLOAT)GAME_HEIGHT;

		config.isExist = !(BOOL)Config::Get(CONFIG_WRAPPER, "ReInit", TRUE);

		if (!config.isExist)
		{
			ImageQuality quality = (ImageQuality)Config::Get(CONFIG_DISCIPLE, "ImageQuality", QualityNormal);
			
			config.windowedMode = TRUE;

			if (config.type.sacred)
			{
				Config::Set(CONFIG_DISCIPLE, "DDraw", TRUE);
				Config::Set(CONFIG_DISCIPLE, "InWindow", config.windowedMode);
			}
			else
			{
				Config::Set(CONFIG_DISCIPLE, "UseD3D", FALSE);
				Config::Set(CONFIG_DISCIPLE, "DisplayMode", config.windowedMode);
			}

			{
				config.hd = TRUE;
				config.image.aspect = TRUE;
				config.image.vSync = TRUE;

				switch (quality)
				{
				case QualityLow:
					config.image.interpolation = InterpolateLinear;
					break;
				case QualityHigh:
					config.image.interpolation = InterpolateCubic;
					break;
				case QualityVeryHigh:
					config.image.interpolation = InterpolateLanczos;
					break;
				default:
					config.image.interpolation = InterpolateHermite;
					break;
				}

				config.image.upscaling = UpscaleNone;
				config.image.value = 0;
				config.image.sharpness = FSR_SHARP_DEFAULT;

				if (!config.type.sacred)
				{
					Config::Set(CONFIG_DISCIPLE, "ShowInterfBorder", TRUE);
					Config::Set(CONFIG_DISCIPLE, "EnableZoom", TRUE);

					config.borders.type = BordersClassic;
					config.background.enabled = TRUE;
					config.battle.mirror = TRUE;
				}
				config.battle.speedIndex = 20;
				config.battle.speedValue = config.battle.speedIndex + 10;

				config.zoom.enabled = TRUE;
				config.zoom.value = 100;
				config.mapZoom.value = 100;

				config.speed.index = 5;
				config.speed.value = config.speed.index + 10;
				config.speed.enabled = FALSE;

				config.snapshot.type = ImagePNG;
				config.snapshot.level = 9;
				config.locales.current.id = GetUserDefaultLCID();
				config.updateMode = UpdateSSE;
				config.cpu.sse2 = TRUE;
				config.scroll.buttons.left = config.scroll.buttons.middle = TRUE;
				config.scroll.edge.active = TRUE;
				config.scroll.easy.value = 150;
				config.cloudsFactor = 1.0f;

				CHAR* buffer = (CHAR*)MemoryAlloc(8192);
				if (buffer)
				{
					CHAR* ptr = buffer;
					ptr = Add(ptr, "ReInit", FALSE, "Reset all wrapper configurations (0 - no; 1 - yes)");

					ptr = Add(ptr, "ShowBanners", config.toogle.banners, "Show in-game banners for troops (0 - no 'default'; 1 - yes)");
					ptr = Add(ptr, "ShowResources", config.toogle.resources, "Show resources panel (0 - no 'default'; 1 - yes)");
					ptr = Add(ptr, "SceneSort", *(INT*)&config.scene.sort.real, "Sort scenarios (0 - by title 'default'; 1 - by file name; 2 - by map size 'ascending'; 3 - by map size 'descending')");
					ptr = Add(ptr, "Renderer", *(INT*)&config.renderer, "Image renderer (0 - auto 'default'; 1 - opengl 1.1; 2 - opengl 2.0; 3 - opengl 3.0; 4 - windows gdi)");
					ptr = Add(ptr, "HD", config.hd, "Enables HD options, like 32 bpp rendering and different resolutions support (0 - no; 1 - yes 'default')");

					if (config.type.sacred)
					{
						ptr = Add(ptr, "DisplayWidth", GAME_WIDTH, "Image resolution width (640 - min; 2048 - max)");
						ptr = Add(ptr, "DisplayHeight", GAME_HEIGHT, "Image resolution height (480 - min; 1024 - max)");
					}
					else
					{
						ptr = Add(ptr, "DisplayWidth", GAME_WIDTH, "Image resolution width (800 - min)");
						ptr = Add(ptr, "DisplayHeight", GAME_HEIGHT, "Image resolution height (600 - min)");
					}

					ptr = Add(ptr, "ImageAspect", config.image.aspect, "Keep image aspect ratio (0 - no; 1 - yes 'default')");
					ptr = Add(ptr, "ImageVSync", config.image.vSync, "Enables vertical synchronization (0 - no; 1 - yes 'default')");
					ptr = Add(ptr, "Interpolation", (INT)config.image.interpolation, "Image interpolation filter (0 - none; 1 - linear; 2 - hermite; 3 - cubic; 4 - lanczos; 5 - fsr)");
					ptr = Add(ptr, "Upscaling", MAKELONG(config.image.upscaling, config.image.value), "Image upscaling filter (low word: 0 - none 'default'; 1 - xbrz; 2 - scalehq; 3 - xsal; 4 - eagle; 5 - scalenx; high word - value)");
					ptr = Add(ptr, "FsrSharpness", config.image.sharpness, "FSR sharpening strength (0 - off; 1 - low; 2 - medium 'default'; 3 - high; 4 - maximum)");

					if (!config.type.sacred)
					{
						ptr = Add(ptr, "Borders", *(INT*)&config.borders.type, "Enables borders for in-game windows (0 - no; 1 - classic style 'default', 2 - alternative style)");
						ptr = Add(ptr, "Background", config.background.enabled, "Enables background for in-game windows (0 - no; 1 - yes 'default')");
					}

					ptr = Add(ptr, "EnableZoom", config.zoom.enabled, "Enables in-game windows zoom (0 - no; 1 - yes 'default')");
					ptr = Add(ptr, "ZoomFactor", *(INT*)&config.zoom.value, "Zoom factor for in-game windows (0 - 100 'default')");
					ptr = Add(ptr, "MapZoom", *(INT*)&config.mapZoom.value, "Strategic map zoom in percent (100 'default' - 300), changed with Ctrl + mouse wheel");
					ptr = Add(ptr, "FullScreenMode", config.borderless.mode, "Full screen window mode (0 - exclusive 'default'; 1 - borderless)");
					ptr = Add(ptr, "MaximizedWindow", config.maximizedWindow, "Starts windowed mode maximized (0 - no 'default'; 1 - yes)");
					ptr = Add(ptr, "GameSpeed", config.speed.index, "Animation speed (1 - ..., 5 - 1.5x 'deafult'");
					ptr = Add(ptr, "SpeedEnabled", config.speed.enabled, "Enables animation speed (0 - no 'default'; 1 - yes)");
					ptr = Add(ptr, "BattleSpeed", config.battle.speedIndex, "Battle animation speed (0 - 1.0x, 10 - 2.0x, 20 - 3.0x 'default')");
					ptr = Add(ptr, "AlwaysActive", config.alwaysActive, "Game window is always active (0 - no 'default'; 1 - yes)");
					ptr = Add(ptr, "ColdCPU", config.cpu.cold, "Decrease CPU usage for OpenGL renderer (0 - no 'default'; 1 - yes)");

					if (!config.type.sacred)
					{
						ptr = Add(ptr, "WideBattle", config.wide.allowed, "Makes window battle wider. Depends on image aspect ratio (0 - no; 1 - yes)");
						ptr = Add(ptr, "MirrorBattle", config.battle.mirror, "Allows mirrored bacgrounds for battles (0 - no; 1 - yes 'default')");
					}

					ptr = Add(ptr, "ScreenshotType", *(INT*)&config.snapshot.type, "Screenshots type (0 - bmp; 1 - png 'default')");
					ptr = Add(ptr, "ScreenshotLevel", *(INT*)&config.snapshot.level, "Compression level for PNG screenshots (0 - 9, 5 - 'default')");
					ptr = Add(ptr, "Locale", *(INT*)&config.locales.current.id, "Locale id (by default is determined by system locale)");
					ptr = Add(ptr, "NoSSE2", !config.cpu.sse2, "Disables using of SSE2 instruction set, even if it's available (0 - no 'default'; 1 - yes)");
					ptr = Add(ptr, "UpdateMode", (INT)config.updateMode, "Image comparison. Affects on rendering performance (0 - none; 1 - SSE2 'default'; 2 - classic; 3 - alternative)");
					ptr = Add(ptr, "FastAI", config.ai.fast, "Increases AI performance, but may cause an unexpected game crash (0 - no 'default'; 1 - yes)");
					ptr = Add(ptr, "MouseScroll", config.scroll.buttons.left | (config.scroll.buttons.middle << 1), "Allows map scrolling by pressing mouse button (0 - no; 1 - left button; 2 - middle button; 3 - both buttons 'default')");
					ptr = Add(ptr, "EdgeScroll", config.scroll.edge.active, "Allows map scrolling on window/screen edge detection (0 - no; 1 - yes 'default')");
					ptr = Add(ptr, "EasyScroll", config.scroll.easy.value, "Easy function timeout for map scrolling (150 - 'default')");
					ptr = Add(ptr, "CloudsFactor", 1, "Defines clouds count multiplier (1 - min 'default')");
					*ptr = NULL;

					WritePrivateProfileSection(CONFIG_WRAPPER, buffer + (*(WORD*)buffer == '\r\n' ? 2 : 0), config.file);
					MemoryFree(buffer);
				}
			}

			{
				config.keys.imageFilter = 3;
				Config::Set(CONFIG_KEYS, "ImageFilter", config.keys.imageFilter);

				config.keys.windowedMode = 4;
				Config::Set(CONFIG_KEYS, "WindowedMode", config.keys.windowedMode);

				Config::Set(CONFIG_KEYS, "AspectRatio", "");
				Config::Set(CONFIG_KEYS, "VSync", "");

				if (!config.type.sacred)
					Config::Set(CONFIG_KEYS, "ZoomImage", "");

				config.keys.speedToggle = 7;
				Config::Set(CONFIG_KEYS, "SpeedToggle", config.keys.speedToggle);

				config.keys.snapshot = 12;
				Config::Set(CONFIG_KEYS, "Screenshot", config.keys.snapshot);
			}

			for (DWORD i = 0; i < BindCount; ++i)
				Config::Set(CONFIG_BINDS, bindsList[i].key, bindsList[i].def);

			{
				config.colors.active.satHue.hueShift = 0.5f;
				config.colors.active.satHue.saturation = 0.5f;
				Config::Set(CONFIG_COLORS, "HueSat", 0x01F401F4);

				config.colors.active.input.left.rgb = 0.0f;
				config.colors.active.input.right.rgb = 1.0f;
				Config::Set(CONFIG_COLORS, "RgbInput", 0x03E80000);

				config.colors.active.input.left.red = 0.0f;
				config.colors.active.input.right.red = 1.0f;
				Config::Set(CONFIG_COLORS, "RedInput", 0x03E80000);

				config.colors.active.input.left.green = 0.0f;
				config.colors.active.input.right.green = 1.0f;
				Config::Set(CONFIG_COLORS, "GreenInput", 0x03E80000);

				config.colors.active.input.left.blue = 0.0f;
				config.colors.active.input.right.blue = 1.0f;
				Config::Set(CONFIG_COLORS, "BlueInput", 0x03E80000);

				config.colors.active.gamma.rgb = 0.5f;
				Config::Set(CONFIG_COLORS, "RgbGamma", 500);

				config.colors.active.gamma.red = 0.5f;
				Config::Set(CONFIG_COLORS, "RedGamma", 500);

				config.colors.active.gamma.green = 0.5f;
				Config::Set(CONFIG_COLORS, "GreenGamma", 500);

				config.colors.active.gamma.blue = 0.5f;
				Config::Set(CONFIG_COLORS, "BlueGamma", 500);

				config.colors.active.output.left.rgb = 0.0f;
				config.colors.active.output.right.rgb = 1.0f;
				Config::Set(CONFIG_COLORS, "RgbOutput", 0x03E80000);

				config.colors.active.output.left.red = 0.0f;
				config.colors.active.output.right.red = 1.0f;
				Config::Set(CONFIG_COLORS, "RedOutput", 0x03E80000);

				config.colors.active.output.left.green = 0.0f;
				config.colors.active.output.right.green = 1.0f;
				Config::Set(CONFIG_COLORS, "GreenOutput", 0x03E80000);

				config.colors.active.output.left.blue = 0.0f;
				config.colors.active.output.right.blue = 1.0f;
				Config::Set(CONFIG_COLORS, "BlueOutput", 0x03E80000);
			}
		}
		else
		{
			if (config.type.sacred && !Config::Get(CONFIG_DISCIPLE, "DDraw", TRUE) || !config.type.sacred && Config::Get(CONFIG_DISCIPLE, "UseD3D", FALSE))
				return FALSE;

			config.windowedMode = (BOOL)Config::Get(CONFIG_DISCIPLE, config.type.sacred ? "InWindow" : "DisplayMode", TRUE);

			config.scene.all = (BOOL)Config::Get(CONFIG_DISCIPLE, "IncludeCampaign", FALSE);
			config.skipIntro = !(BOOL)Config::Get(CONFIG_DISCIPLE, "Intro", TRUE);
			config.editorCamp = (BOOL)Config::Get(CONFIG_DISCIPLE, "ScenEditDatabase", FALSE);

			{
				config.toogle.banners = (BOOL)Config::Get(CONFIG_WRAPPER, "ShowBanners", FALSE);
				config.toogle.resources = (BOOL)Config::Get(CONFIG_WRAPPER, "ShowResources", FALSE);

				INT value = Config::Get(CONFIG_WRAPPER, "SceneSort", SceneByTitle);
				config.scene.sort.real = *(SceneSort*)&value;
				if (config.scene.sort.real < SceneByTitle || config.scene.sort.real > SceneBySizeDesc)
					config.scene.sort.real = SceneByTitle;
				config.scene.sort.value = config.scene.sort.real;

				value = Config::Get(CONFIG_WRAPPER, "Renderer", RendererAuto);
				config.renderer = *(RendererType*)&value;
				if (config.renderer < RendererAuto || config.renderer > RendererGDI)
					config.renderer = RendererAuto;

				config.hd = (BOOL)Config::Get(CONFIG_WRAPPER, "HD", TRUE);
				config.maximizedWindow = (BOOL)Config::Get(CONFIG_WRAPPER, "MaximizedWindow", FALSE);

				config.image.aspect = (BOOL)Config::Get(CONFIG_WRAPPER, "ImageAspect", TRUE);
				config.image.vSync = (BOOL)Config::Get(CONFIG_WRAPPER, "ImageVSync", TRUE);

				value = Config::Get(CONFIG_WRAPPER, "Interpolation", InterpolateHermite);
				config.image.interpolation = *(InterpolationFilter*)&value;
				if (config.image.interpolation < InterpolateNearest || config.image.interpolation > InterpolateFsr)
					config.image.interpolation = InterpolateHermite;

				value = Config::Get(CONFIG_WRAPPER, "Upscaling", UpscaleNone);
				config.image.upscaling = UpscalingFilter(LOBYTE(value));
				config.image.value = LOBYTE(HIWORD(value));
				switch (config.image.upscaling)
				{
				case UpscaleXBRZ:
					if (config.image.value < 2 || config.image.value > 6)
						config.image.value = 2;
					break;
				case UpscaleScaleHQ:
					if (config.image.value != 2 && config.image.value != 4)
						config.image.value = 2;
					break;
				case UpscaleXSal:
					if (config.image.value != 2)
						config.image.value = 2;
					break;
				case UpscaleEagle:
					if (config.image.value != 2)
						config.image.value = 2;
					break;
				case UpscaleScaleNx:
					if (config.image.value != 2 && config.image.value != 3)
						config.image.value = 2;
					break;
				default:
					config.image.upscaling = UpscaleNone;
					config.image.value = 0;
					break;
				}

				config.image.sharpness = (BYTE)Config::Get(CONFIG_WRAPPER, "FsrSharpness", FSR_SHARP_DEFAULT);
				if (config.image.sharpness > FSR_SHARP_MAX)
					config.image.sharpness = FSR_SHARP_DEFAULT;

				if (!config.type.sacred)
				{
					BOOL value = Config::Get(CONFIG_DISCIPLE, "ShowInterfBorder", TRUE);
					config.borders.type = (BordersType)Config::Get(CONFIG_WRAPPER, "Borders", value ? BordersClassic : BordersNone);
					config.background.enabled = (BOOL)Config::Get(CONFIG_WRAPPER, "Background", value);

					config.zoom.enabled = (BOOL)Config::Get(CONFIG_DISCIPLE, "EnableZoom", TRUE);
				}
				else
					config.zoom.enabled = (BOOL)Config::Get(CONFIG_WRAPPER, "EnableZoom", TRUE);

				config.borderless.real = config.borderless.mode = (BOOL)Config::Get(CONFIG_WRAPPER, "FullScreenMode", FALSE);

				value = Config::Get(CONFIG_WRAPPER, "GameSpeed", 5);
				if (value < 1)
					value = 5;
				config.speed.index = *(DWORD*)&value;
				config.speed.value = config.speed.index + 10;

				config.speed.enabled = (BOOL)Config::Get(CONFIG_WRAPPER, "SpeedEnabled", FALSE);

				value = Config::Get(CONFIG_WRAPPER, "BattleSpeed", 20);
				if (value < 0)
					value = 20;
				else if (value > 20)
					value = 20;
				config.battle.speedIndex = *(DWORD*)&value;
				config.battle.speedValue = config.battle.speedIndex + 10;

				config.alwaysActive = (BOOL)Config::Get(CONFIG_WRAPPER, "AlwaysActive", FALSE);
				config.cpu.cold = (BOOL)Config::Get(CONFIG_WRAPPER, "ColdCPU", FALSE);

				if (!config.type.sacred)
				{
					config.wide.allowed = (BOOL)Config::Get(CONFIG_WRAPPER, "WideBattle", FALSE);
					config.battle.mirror = (BOOL)Config::Get(CONFIG_WRAPPER, "MirrorBattle", TRUE);
				}

				value = Config::Get(CONFIG_WRAPPER, "ScreenshotType", ImagePNG);
				config.snapshot.type = *(ImageType*)&value;
				if (config.snapshot.type < ImageBMP || config.snapshot.type > ImagePNG)
					config.snapshot.type = ImagePNG;

				value = Config::Get(CONFIG_WRAPPER, "ScreenshotLevel", 9);
				config.snapshot.level = *(DWORD*)&value;
				if (config.snapshot.level < 1 || config.snapshot.level > 9)
					config.snapshot.level = 9;

				value = Config::Get(CONFIG_WRAPPER, "ZoomFactor", 100);
				config.zoom.value = *(DWORD*)&value;
				if (!config.zoom.value || config.zoom.value > 100)
					config.zoom.value = 100;

				value = Config::Get(CONFIG_WRAPPER, "MapZoom", 100);
				config.mapZoom.value = value < MAP_ZOOM_MIN ? MAP_ZOOM_MIN : value > MAP_ZOOM_MAX ? MAP_ZOOM_MAX : *(DWORD*)&value;
				config.mapZoom.current = 1.0f;

				config.locales.current.id = (LCID)Config::Get(CONFIG_WRAPPER, "Locale", (INT)GetUserDefaultLCID());

				config.cpu.sse2 = !(BOOL)Config::Get(CONFIG_WRAPPER, "NoSSE2", FALSE);

				config.updateMode = (UpdateMode)Config::Get(CONFIG_WRAPPER, "UpdateMode", (INT)UpdateSSE);
				if (config.updateMode < UpdateNone || config.updateMode > UpdateASM)
					config.updateMode = UpdateSSE;

				if (!config.type.editor)
					config.ai.fast = (BOOL)Config::Get(CONFIG_WRAPPER, "FastAI", FALSE);

				value = Config::Get(CONFIG_WRAPPER, "MouseScroll", 3);
				if (value < 0 || value > 3)
					value = 3;

				config.scroll.buttons.left = value & 1;
				config.scroll.buttons.middle = (value & 2) >> 1;

				config.scroll.edge.active = (BOOL)Config::Get(CONFIG_WRAPPER, "EdgeScroll", TRUE);

				value = Config::Get(CONFIG_WRAPPER, "EasyScroll", 100);
				if (value < 0)
					value = 150;
				config.scroll.easy.value = *(DWORD*)&value;

				value = Config::Get(CONFIG_WRAPPER, "CloudsFactor", 1);
				if (value < 1)
					value = 1;
				config.cloudsFactor = (FLOAT)value;
			}

			{
				// F1 - reserved for "About"
				CHAR buffer[20];
				if (Config::Get(CONFIG_KEYS, "ImageFilter", "", buffer, sizeof(buffer)))
				{
					INT value = Config::Get(CONFIG_KEYS, "ImageFilter", 0);
					config.keys.imageFilter = LOBYTE(value);
					if (config.keys.imageFilter > 1 && config.keys.imageFilter > 24)
						config.keys.imageFilter = 0;
				}

				if (Config::Get(CONFIG_KEYS, "WindowedMode", "", buffer, sizeof(buffer)))
				{
					INT value = Config::Get(CONFIG_KEYS, "WindowedMode", 0);
					config.keys.windowedMode = LOBYTE(value);
					if (config.keys.windowedMode > 1 && config.keys.windowedMode > 24)
						config.keys.windowedMode = 0;
				}

				if (Config::Get(CONFIG_KEYS, "AspectRatio", "", buffer, sizeof(buffer)))
				{
					INT value = Config::Get(CONFIG_KEYS, "AspectRatio", 0);
					config.keys.aspectRatio = LOBYTE(value);
					if (config.keys.aspectRatio > 1 && config.keys.aspectRatio > 24)
						config.keys.aspectRatio = 0;
				}

				if (Config::Get(CONFIG_KEYS, "VSync", "", buffer, sizeof(buffer)))
				{
					INT value = Config::Get(CONFIG_KEYS, "VSync", 0);
					config.keys.vSync = LOBYTE(value);
					if (config.keys.vSync > 1 && config.keys.vSync > 24)
						config.keys.vSync = 0;
				}

				if (!config.type.sacred)
				{
					if (Config::Get(CONFIG_KEYS, "ZoomImage", "", buffer, sizeof(buffer)))
					{
						INT value = Config::Get(CONFIG_KEYS, "ZoomImage", 0);
						config.keys.zoomImage = LOBYTE(value);
						if (config.keys.zoomImage > 1 && config.keys.zoomImage > 24)
							config.keys.zoomImage = 0;
					}
				}

				if (Config::Get(CONFIG_KEYS, "SpeedToggle", "", buffer, sizeof(buffer)))
				{
					INT value = Config::Get(CONFIG_KEYS, "SpeedToggle", 0);
					config.keys.speedToggle = LOBYTE(value);
					if (config.keys.speedToggle > 1 && config.keys.speedToggle > 24)
						config.keys.speedToggle = 0;
				}

				if (Config::Get(CONFIG_KEYS, "Screenshot", "", buffer, sizeof(buffer)))
				{
					INT value = Config::Get(CONFIG_KEYS, "Screenshot", 0);
					config.keys.snapshot = LOBYTE(value);
					if (config.keys.snapshot > 1 && config.keys.snapshot > 24)
						config.keys.snapshot = 0;
				}
			}

			{
				BOOL wasd = (BOOL)Config::Get(CONFIG_WRAPPER, "WasdScroll", FALSE);

				CHAR buffer[20];
				for (DWORD i = 0; i < BindCount; ++i)
				{
					if (Config::Get(CONFIG_BINDS, bindsList[i].key, "", buffer, sizeof(buffer)))
						config.binds[i] = LOBYTE(Config::Get(CONFIG_BINDS, bindsList[i].key, 0));
					else
						config.binds[i] = wasd ? bindsList[i].wasd : 0;
				}
			}

			{
				INT value = Config::Get(CONFIG_COLORS, "HueSat", 0x01F401F4);
				config.colors.active.satHue.hueShift = 0.001f * min(1000, max(0, LOWORD(value)));
				config.colors.active.satHue.saturation = 0.001f * min(1000, max(0, HIWORD(value)));

				value = Config::Get(CONFIG_COLORS, "RgbInput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.input.left.rgb = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.input.right.rgb = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.input.left.rgb = 0.0f;
					config.colors.active.input.right.rgb = 1.0f;
				}

				value = Config::Get(CONFIG_COLORS, "RedInput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.input.left.red = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.input.right.red = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.input.left.red = 0.0f;
					config.colors.active.input.right.red = 1.0f;
				}

				value = Config::Get(CONFIG_COLORS, "GreenInput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.input.left.green = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.input.right.green = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.input.left.green = 0.0f;
					config.colors.active.input.right.green = 1.0f;
				}

				value = Config::Get(CONFIG_COLORS, "BlueInput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.input.left.blue = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.input.right.blue = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.input.left.blue = 0.0f;
					config.colors.active.input.right.blue = 1.0f;
				}

				config.colors.active.gamma.rgb = 0.001f * min(1000, max(0, Config::Get(CONFIG_COLORS, "RgbGamma", 500)));
				config.colors.active.gamma.red = 0.001f * min(1000, max(0, Config::Get(CONFIG_COLORS, "RedGamma", 500)));
				config.colors.active.gamma.green = 0.001f * min(1000, max(0, Config::Get(CONFIG_COLORS, "GreenGamma", 500)));
				config.colors.active.gamma.blue = 0.001f * min(1000, max(0, Config::Get(CONFIG_COLORS, "BlueGamma", 500)));

				value = Config::Get(CONFIG_COLORS, "RgbOutput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.output.left.rgb = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.output.right.rgb = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.output.left.rgb = 0.0f;
					config.colors.active.output.right.rgb = 1.0f;
				}

				value = Config::Get(CONFIG_COLORS, "RedOutput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.output.left.red = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.output.right.red = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.output.left.red = 0.0f;
					config.colors.active.output.right.red = 1.0f;
				}

				value = Config::Get(CONFIG_COLORS, "GreenOutput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.output.left.green = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.output.right.green = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.output.left.green = 0.0f;
					config.colors.active.output.right.green = 1.0f;
				}

				value = Config::Get(CONFIG_COLORS, "BlueOutput", 0x03E80000);
				if (LOWORD(value) < HIWORD(value))
				{
					config.colors.active.output.left.blue = 0.001f * min(1000, max(0, LOWORD(value)));
					config.colors.active.output.right.blue = 0.001f * min(1000, max(0, HIWORD(value)));
				}
				else
				{
					config.colors.active.output.left.blue = 0.0f;
					config.colors.active.output.right.blue = 1.0f;
				}
			}
		}

		config.colors.current = &config.colors.active;

		config.menu = LoadMenu(hDllModule, MAKEINTRESOURCE(IDR_MENU));
		config.font = (HFONT)CreateFont(16, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, ANSI_CHARSET,
			OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
			DEFAULT_PITCH | FF_DONTCARE, TEXT("MS Shell Dlg"));
		config.msgSnapshot = RegisterWindowMessage(WM_SNAPSHOT);
		config.msgMenu = RegisterWindowMessage(WM_CHECK_MENU);

		if (config.type.sacred)
			config.mode = modesList;
		else
		{
			DWORD modeIdx = Config::Get(CONFIG_DISCIPLE, "DisplaySize", 0);
			if (modeIdx > 2)
				modeIdx = 0;
			config.mode = &modesList[modeIdx + 1];
		}

		config.resolution.width = LOWORD(config.mode->width);
		config.resolution.height = LOWORD(config.mode->height);

		if (config.renderer != RendererGDI)
		{
			HMODULE hLibrary = LoadLibrary("NTDLL.dll");
			if (hLibrary)
			{
				if (GetProcAddress(hLibrary, "wine_get_version"))
					config.singleWindow = TRUE;
				FreeLibrary(hLibrary);
			}
		}
		else
			config.singleWindow = TRUE;

		if (config.cpu.sse2)
		{
			INT cpuinfo[4];
			__cpuid(cpuinfo, 1);
			config.cpu.sse2 = cpuinfo[3] & (1 << 26) || FALSE;
		}

		if (!config.cpu.sse2 && config.updateMode == UpdateSSE)
			config.updateMode = UpdateCPP;

		config.singleThread = TRUE;
		DWORD processMask, systemMask;
		HANDLE process = GetCurrentProcess();
		if (GetProcessAffinityMask(process, &processMask, &systemMask))
		{
			if (processMask != systemMask && SetProcessAffinityMask(process, systemMask))
				GetProcessAffinityMask(process, &processMask, &systemMask);

			BOOL isSingle = FALSE;
			DWORD count = sizeof(DWORD) << 3;
			do
			{
				if (processMask & 1)
				{
					if (isSingle)
					{
						config.singleThread = FALSE;
						break;
					}
					else
						isSingle = TRUE;
				}

				processMask >>= 1;
			} while (--count);
		}

		if (config.renderer == RendererGDI || config.singleThread)
		{
			config.cpu.cold = FALSE;
			config.borderless.real = FALSE;
			config.borderless.mode = FALSE;
		}

		Config::CalcZoomed();
		Config::GetSyncStep();

		if (EnumSystemLocales(EnumLocalesCount, LCID_INSTALLED))
		{
			MenuItemData mData;
			mData.childId = IDM_LOCALE_DEFAULT;
			if (Window::GetMenuByChildID(&mData))
			{
				CHAR title[4];
				*(DWORD*)title = NULL;

				HMENU* hMenu = config.locales.menus;
				for (DWORD i = 0; i < sizeof(config.locales.menus) / sizeof(HMENU); ++i, ++hMenu)
				{
					if (*hMenu)
					{
						*title = 'A' + LOBYTE(i);
						AppendMenu(mData.hMenu, MF_STRING | MF_POPUP, (UINT_PTR)*hMenu, title);
					}
				}
			}
		}
		else
			config.locales.count = 0;

		config.locales.list = (LCID*)MemoryAlloc((config.locales.count + 1) * sizeof(LCID));
		config.locales.list[0] = 0;

		if (config.locales.count)
			EnumSystemLocales(EnumLocales, LCID_INSTALLED);

		UpdateLocale();

		UpdateKeyLabels();

		return TRUE;
	}

	VOID SetKeyLabel(HMENU hMenu, UINT item, BOOL byPosition, BYTE key)
	{
		CHAR buffer[256];
		MENUITEMINFO info = {};
		info.cbSize = sizeof(MENUITEMINFO);
		info.fMask = MIIM_TYPE;
		info.fType = MFT_STRING;
		info.dwTypeData = buffer;
		info.cch = sizeof(buffer);
		if (!GetMenuItemInfo(hMenu, item, byPosition, &info))
			return;

		CHAR* tab = StrChar(buffer, '\b');
		if (tab)
			tab[1] = NULL;

		if (key)
			StrPrint(buffer + StrLength(buffer), "F%d", key);

		SetMenuItemInfo(hMenu, item, byPosition, &info);
	}

	VOID SetKeyLabel(DWORD childId, BYTE key)
	{
		MenuItemData mData;
		mData.childId = childId;
		if (Window::GetMenuByChildID(&mData))
			SetKeyLabel(mData.hParent, mData.index, TRUE, key);
	}

	WPARAM GetMessageKey(UINT uMsg, WPARAM wParam)
	{
		if (uMsg != WM_CHAR)
			return wParam;

		SHORT vk = VkKeyScan((CHAR)wParam);
		return vk == -1 ? 0 : LOBYTE(vk);
	}

	INT FindBind(UINT uMsg, WPARAM wParam)
	{
		wParam = GetMessageKey(uMsg, wParam);
		if (!wParam)
			return -1;

		for (DWORD pass = 0; pass < 2; ++pass)
		{
			for (DWORD i = 0; i < BindCount; ++i)
			{
				if ((bindsList[i].scope == ScopeMap) == (pass != 0) && config.binds[i] == wParam && GetBindNative((BindAction)i) != wParam && IsBindActive((BindAction)i))
					return i;
			}
		}

		return -1;
	}

	BYTE GetBindNative(BindAction action)
	{
		return config.screen.full && bindsList[action].fullNative ? bindsList[action].fullNative : bindsList[action].native;
	}

	BOOL IsBindActive(BindAction action)
	{
		switch (bindsList[action].scope)
		{
		case ScopeAny:
			return TRUE;
		case ScopePopup:
			return config.screen.popup || config.screen.menu || config.screen.full && bindsList[action].fullNative;
		case ScopeBattle:
			return config.battle.active;
		case ScopeScreen:
			return config.screen.full;
		default:
			return config.screen.map;
		}
	}

	BOOL IsBindBlocked(UINT uMsg, WPARAM wParam)
	{
		if (!config.screen.map || !config.binds[BindClosePopup])
			return FALSE;

		wParam = GetMessageKey(uMsg, wParam);
		return wParam == config.binds[BindClosePopup] && wParam != bindsList[BindClosePopup].native;
	}

	BOOL IsBindHeld(BindAction action)
	{
		return config.binds[action] && IsBindActive(action) && (GetAsyncKeyState(config.binds[action]) & 0x8000);
	}

	VOID UpdateKeyLabels()
	{
		SetKeyLabel(config.menu, IDM_RES_FULL_SCREEN, FALSE, config.keys.windowedMode);
		SetKeyLabel(config.menu, IDM_ASPECT_RATIO, FALSE, config.keys.aspectRatio);
		SetKeyLabel(config.menu, IDM_VSYNC, FALSE, config.keys.vSync);
		SetKeyLabel(IDM_STRETCH_OFF, config.keys.zoomImage);
		SetKeyLabel(IDM_FILT_OFF, config.keys.imageFilter);
		SetKeyLabel(IDM_SPEED_1_0, config.keys.speedToggle);
	}

	INT Get(const CHAR* app, const CHAR* key, INT default)
	{
		return GetPrivateProfileInt(app, key, (INT) default, config.file);
	}

	DWORD Get(const CHAR* app, const CHAR* key, CHAR* default, CHAR* returnString, DWORD nSize)
	{
		return GetPrivateProfileString(app, key, default, returnString, nSize, config.file);
	}

	BOOL Set(const CHAR* app, const CHAR* key, INT value)
	{
		CHAR res[20];
		StrFromInt(value, res, 10);
		return WritePrivateProfileString(app, key, res, config.file);
	}

	BOOL Set(const CHAR* app, const CHAR* key, CHAR* value)
	{
		return WritePrivateProfileString(app, key, value, config.file);
	}

	CHAR* Add(CHAR* ptr, const CHAR* key, INT value, const CHAR* comments)
	{
		if (comments)
		{
			*(DWORD*)ptr = '\r\n; ';
			ptr += 4;
			StrCopy(ptr, comments);
			ptr += StrLength(comments) + 1;
		}

		{
			CHAR dst[64];
			StrPrint(dst, "%s=%d", key, value);
			StrCopy(ptr, dst);
			ptr += StrLength(dst) + 1;
		}

		return ptr;
	}

	BOOL IsZoomed()
	{
		return config.zoom.glallow && config.zoom.enabled && config.borders.active && (!config.battle.active || !config.battle.wide || config.battle.zoomable);
	}

	VOID GetMapZoomRect(RECT* rect)
	{
		*rect = { 0, 0, *(LONG*)&config.mode->width - MAP_ZOOM_PANEL_WIDTH, *(LONG*)&config.mode->height - MAP_ZOOM_BAR_HEIGHT };
	}

	BOOL IsMapZoomed()
	{
		return config.mapZoom.allowed && config.mapZoom.current > 1.001f;
	}

	FLOAT GetMapZoomTarget()
	{
		return config.mapZoom.allowed && config.screen.strategic && !config.screen.popup ? config.mapZoom.value * 0.01f : 1.0f;
	}

	BOOL IsMapZoomPending()
	{
		return config.mapZoom.allowed && config.mapZoom.current != GetMapZoomTarget();
	}

	BOOL StepMapZoom()
	{
		static DWORD lastTime;
		static BOOL animating;
		DWORD now = timeGetTime();
		DWORD delta = animating ? now - lastTime : 16;
		lastTime = now;
		if (delta > 50)
			delta = 50;

		FLOAT target = GetMapZoomTarget();
		FLOAT current = config.mapZoom.current;
		animating = current != target;
		if (!animating)
			return FALSE;

		FLOAT k = delta * 0.012f;
		current += (target - current) * (k > 1.0f ? 1.0f : k);
		FLOAT diff = target - current;
		if (diff < 0.002f && diff > -0.002f)
			current = target;

		config.mapZoom.current = current;
		return TRUE;
	}

	VOID MapZoomPoint(LPPOINT p)
	{
		RECT rect;
		GetMapZoomRect(&rect);
		FLOAT zoom = config.mapZoom.current;
		if (!config.mapZoom.allowed || zoom <= 1.001f || p->x >= rect.right || p->y >= rect.bottom)
			return;

		FLOAT cx = rect.right * 0.5f;
		FLOAT cy = rect.bottom * 0.5f;
		if (p->x > MAP_ZOOM_EDGE)
			p->x = LONG(cx + (p->x - cx) / zoom);
		if (p->y > MAP_ZOOM_EDGE)
			p->y = LONG(cy + (p->y - cy) / zoom);
	}

	VOID MapUnzoomPoint(LPPOINT p)
	{
		RECT rect;
		GetMapZoomRect(&rect);
		FLOAT zoom = config.mapZoom.current;
		if (!config.mapZoom.allowed || zoom <= 1.001f || p->x >= rect.right || p->y >= rect.bottom)
			return;

		FLOAT cx = rect.right * 0.5f;
		FLOAT cy = rect.bottom * 0.5f;
		LONG x = LONG(cx + (p->x - cx) * zoom);
		LONG y = LONG(cy + (p->y - cy) * zoom);
		p->x = x < 0 ? 0 : x >= rect.right ? rect.right - 1 : x;
		p->y = y < 0 ? 0 : y >= rect.bottom ? rect.bottom - 1 : y;
	}

	VOID CalcZoomed(Size* dst, Size* src, DWORD scale)
	{
		FLOAT k = (FLOAT)src->width / src->height;
		if (k >= 4.0f / 3.0f)
		{
			FLOAT width = GAME_HEIGHT_FLOAT * k;
			dst->width = src->width - DWORD(((FLOAT)src->width - width) * scale * 0.01);
			dst->height = src->height - DWORD((src->height - GAME_HEIGHT) * scale * 0.01);
		}
		else
		{
			FLOAT height = GAME_WIDTH_FLOAT / k;
			dst->width = src->width - DWORD((src->width - GAME_WIDTH) * scale * 0.01);
			dst->height = src->height - DWORD(((FLOAT)src->height - height) * scale * 0.01);
		}
	}

	VOID CalcZoomed(SizeFloat* dst, SizeFloat* src, DWORD scale)
	{
		FLOAT k = src->width / src->height;
		if (k >= 4.0f / 3.0f)
		{
			FLOAT width = GAME_HEIGHT_FLOAT * k;
			dst->width = src->width - FLOAT((src->width - width) * scale * 0.01);
			dst->height = src->height - FLOAT((src->height - GAME_HEIGHT) * scale * 0.01);
		}
		else
		{
			FLOAT height = GAME_WIDTH_FLOAT / k;
			dst->width = src->width - FLOAT((src->width - GAME_WIDTH) * scale * 0.01);
			dst->height = src->height - FLOAT((src->height - height) * scale * 0.01);
		}
	}

	VOID CalcZoomed()
	{
		CalcZoomed(&config.zoom.size, (Size*)config.mode, config.zoom.value);

		SizeFloat size = { (FLOAT)config.mode->width, (FLOAT)config.mode->height };
		CalcZoomed(&config.zoom.sizeFloat, &size, config.zoom.value);
	}

	VOID CalcScroll()
	{
		config.scroll.check = 1000 / config.speed.frequency;
		config.scroll.multi = DWORD(MathSqrtFloat(FLOAT(config.mode->width * config.mode->width + config.mode->height * config.mode->height)) * 600 / (config.speed.frequency * 800));
	}

	VOID UpdateLocale()
	{
		if (config.locales.current.id)
		{
			CHAR cp[16];
			GetLocaleInfo(config.locales.current.id, LOCALE_IDEFAULTCODEPAGE, cp, sizeof(cp));
			config.locales.current.oem = StrToInt(cp);

			GetLocaleInfo(config.locales.current.id, LOCALE_IDEFAULTANSICODEPAGE, cp, sizeof(cp));
			config.locales.current.ansi = StrToInt(cp);

			CHAR name[64];
			GetLocaleInfo(config.locales.current.id, LOCALE_SENGLISHLANGUAGENAME, name, sizeof(name));

			CHAR country[64];
			GetLocaleInfo(config.locales.current.id, LOCALE_SENGLISHCOUNTRYNAME, country, sizeof(country));

			CHAR crtName[256];
			StrPrint(crtName, "%s_%s.%d", name, country, config.locales.current.ansi);

			SetLocale(LC_CTYPE, crtName);
		}
		else
			SetLocale(LC_CTYPE, "C");
	}

	VOID GetSyncStep()
	{
		DEVMODE devMode = {};
		devMode.dmSize = sizeof(DEVMODE);
		config.speed.frequency = EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode) && (devMode.dmFields & DM_DISPLAYFREQUENCY) ? devMode.dmDisplayFrequency : 60u;
		
		QueryPerformanceFrequency((LARGE_INTEGER*)&config.syncStep.frequency);
		config.syncStep.value = config.syncStep.frequency / config.speed.frequency;

		CalcScroll();
	}
}
