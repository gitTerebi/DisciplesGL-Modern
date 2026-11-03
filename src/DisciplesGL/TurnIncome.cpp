#include "StdAfx.h"
#include "Mmsystem.h"
#include "Config.h"
#include "Hooks.h"
#include "TurnIncome.h"

#define f(a) (a + baseOffset)

namespace TurnIncome
{
	#define CURRENCY_COUNT 6
	#define SHOW_MS 2000
	#define COUNT_MS 1000
	#define ARM_MS 5000
	#define STEP_MS 16

	// Discipl2.exe v2.02b 0x00486313 writes TXT_GOLD..TXT_ROUGE from these bank slots; v3.01b 0x004891BB adds TXT_BLUE from 5
	const DWORD fieldCurrency[CURRENCY_COUNT] = { 0, 3, 4, 2, 1, 5 };

	DWORD sub_Text, sub_GetBank, sub_FindText, sub_SetText, sub_ViewFree, sub_Render;
	const CHAR** names;
	DWORD popupOffset;

	struct {
		DWORD* bank;
		DWORD player;
		SHORT value[CURRENCY_COUNT];
		BOOL has[CURRENCY_COUNT];
	} read;

	struct {
		DWORD player;
		SHORT value[CURRENCY_COUNT];
	} last;

	struct Anim {
		BOOL active;
		SHORT from[CURRENCY_COUNT];
		SHORT to[CURRENCY_COUNT];
		DWORD start;
		DWORD player;
	} anim, pending;

	DWORD pendingTime, armTime, stepTime;
	BOOL armed;
	DWORD* popupView;

	VOID Start(const Anim* from)
	{
		anim = *from;
		anim.active = TRUE;
		anim.start = timeGetTime();
		stepTime = 0;
	}

	VOID Arm()
	{
		if (!config.skipTurnSummary.enabled)
			return;

		DWORD now = timeGetTime();
		if (pending.player && now - pendingTime <= ARM_MS)
		{
			Start(&pending);
			pending.player = 0;
		}
		else
		{
			armed = TRUE;
			armTime = now;
		}
	}

	VOID Observe(DWORD player)
	{
		if (anim.active && anim.player == read.player)
		{
			for (DWORD i = 0; i < CURRENCY_COUNT; ++i)
				if (read.has[i] && read.value[i] != anim.to[i])
					anim.active = FALSE;
		}

		if (last.player == read.player && popupView)
		{
			Anim next = {};
			BOOL up = FALSE;
			for (DWORD i = 0; i < CURRENCY_COUNT; ++i)
			{
				next.from[i] = last.value[i];
				next.to[i] = read.has[i] ? read.value[i] : last.value[i];
				up |= next.to[i] > next.from[i];
			}

			if (up)
			{
				next.player = player;
				DWORD now = timeGetTime();
				if (armed && now - armTime <= ARM_MS)
				{
					armed = FALSE;
					Start(&next);
				}
				else
				{
					pending = next;
					pendingTime = now;
				}
			}
		}

		last.player = read.player;
		for (DWORD i = 0; i < CURRENCY_COUNT; ++i)
			if (read.has[i])
				last.value[i] = read.value[i];
	}

	DWORD sub_DialogName, viewOffset;
	DWORD* captureView;

	BOOL IsPopup(DWORD dialog)
	{
		const CHAR* name = ((const CHAR*(__thiscall*)(DWORD))sub_DialogName)(dialog);
		return name && !StrCompare(name, "DLG_ISO_RESPOPUP");
	}

	BOOL popupHas[CURRENCY_COUNT];
	CHAR shown[CURRENCY_COUNT][16];

	VOID Write(DWORD dialog, DWORD field, const CHAR* text)
	{
		if (!StrCompare(shown[field], text))
			return;

		StrCopy(shown[field], text);
		DWORD box = ((DWORD(__stdcall*)(DWORD, const CHAR*))sub_FindText)(dialog, names[field]);
		if (box)
			((VOID(__thiscall*)(DWORD, const CHAR*))sub_SetText)(box, text);
	}

	VOID Override(DWORD dialog)
	{
		if (!anim.active)
			return;

		DWORD elapsed = timeGetTime() - anim.start;
		BOOL done = elapsed >= SHOW_MS + COUNT_MS;
		FLOAT t = elapsed < SHOW_MS ? 0.0f : done ? 1.0f : FLOAT(elapsed - SHOW_MS) / COUNT_MS;
		FLOAT ease = 1.0f - (1.0f - t) * (1.0f - t);

		for (DWORD i = 0; i < CURRENCY_COUNT; ++i)
		{
			DWORD currency = fieldCurrency[i];
			LONG delta = anim.to[currency] - anim.from[currency];
			if (!popupHas[currency] || delta <= 0)
				continue;

			CHAR text[16];
			if (elapsed < SHOW_MS)
				StrPrint(text, "+%d", delta);
			else
				StrPrint(text, "%d", done ? anim.to[currency] : anim.from[currency] + LONG(delta * ease));

			Write(dialog, i, text);
		}

		if (done)
			anim.active = FALSE;
	}

	SHORT __fastcall GetBank(DWORD* bank, DWORD, DWORD currency)
	{
		SHORT value = ((SHORT(__thiscall*)(DWORD*, DWORD))sub_GetBank)(bank, currency);
		if (currency < CURRENCY_COUNT)
		{
			read.bank = bank;
			read.value[currency] = value;
			read.has[currency] = TRUE;
		}

		return value;
	}

	VOID __stdcall Text(DWORD dialog, DWORD player, DWORD objectMap)
	{
		DWORD* view = captureView;
		captureView = NULL;

		MemoryZero(&read, sizeof(read));
		((VOID(__stdcall*)(DWORD, DWORD, DWORD))sub_Text)(dialog, player, objectMap);
		if (!read.bank)
			return;

		if (view && IsPopup(dialog))
			popupView = view;

		read.player = player;
		Observe(player);
		if (anim.active && anim.player == player && IsPopup(dialog))
		{
			MemoryCopy(popupHas, read.has, sizeof(popupHas));
			MemoryZero(shown, sizeof(shown));
			Override(dialog);
		}
	}

	// Discipl2.exe v2.02b 0x004892B4: the refresh replaces esi with [esi+4] before it writes the DLG_ISO_RESPOPUP texts
	VOID __declspec(naked) CaptureView()
	{
		__asm {
			mov captureView, esi
			mov esi, [esi+4]
			lea ecx, [esi+8]
			retn
		}
	}

	VOID Tick()
	{
		if (!anim.active || !popupView)
			return;

		DWORD now = timeGetTime();
		if (now - stepTime < STEP_MS)
			return;

		stepTime = now;
		DWORD dialog = popupView[popupOffset / sizeof(DWORD)];
		if (dialog && IsPopup(dialog))
			Override(dialog);
	}

	// Discipl2.exe v2.02b 0x00512CE0: UI-thread update event of the presentation manager; 0x00512BF0 draws the frame
	VOID __fastcall Render(DWORD* manager)
	{
		Tick();
		((VOID(__thiscall*)(DWORD*))sub_Render)(manager);
	}

	VOID __fastcall ViewFree(DWORD* view)
	{
		if (popupView == (DWORD*)((BYTE*)view + viewOffset))
		{
			popupView = NULL;
			anim.active = FALSE;
			pending.player = 0;
			last.player = 0;
			armed = FALSE;
		}

		((VOID(__thiscall*)(DWORD*))sub_ViewFree)(view);
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!hookSpace->income_text_popup)
			return;

		sub_FindText = f(hookSpace->income_find_text);
		sub_SetText = f(hookSpace->income_set_text);
		sub_DialogName = f(hookSpace->income_dialog_name);
		names = (const CHAR**)f(hookSpace->income_names);
		viewOffset = hookSpace->income_view_offset;
		popupOffset = hookSpace->income_popup;

		sub_Text = RedirectCall(hooker, hookSpace->income_text_popup, Text);
		RedirectCall(hooker, hookSpace->income_text_popup_new, Text);

		// Discipl2.exe v3.01b 0x0048BBF8: the v3 refresh writes only DLG_ISO_RESPOPUP, so the v3 rows have no palette sites
		if (hookSpace->income_text_view)
			RedirectCall(hooker, hookSpace->income_text_view, Text);
		if (hookSpace->income_text_palette_new)
			RedirectCall(hooker, hookSpace->income_text_palette_new, Text);

		sub_GetBank = RedirectCall(hooker, hookSpace->income_bank_gold, GetBank);
		RedirectCall(hooker, hookSpace->income_bank_orange, GetBank);
		RedirectCall(hooker, hookSpace->income_bank_blanc, GetBank);
		RedirectCall(hooker, hookSpace->income_bank_jaune, GetBank);
		RedirectCall(hooker, hookSpace->income_bank_rouge, GetBank);
		if (hookSpace->income_bank_blue)
			RedirectCall(hooker, hookSpace->income_bank_blue, GetBank);

		PatchCall(hooker, hookSpace->income_view_capture, CaptureView, 1);
		sub_ViewFree = f(hookSpace->income_view_free);
		RedirectAllCalls(hooker, hookSpace->income_view_free, ViewFree);

		if (hookSpace->income_frame)
			sub_Render = RedirectCall(hooker, hookSpace->income_frame, Render);
	}
}
