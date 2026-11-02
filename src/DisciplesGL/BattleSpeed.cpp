#include "StdAfx.h"
#include "Resource.h"
#include "Config.h"
#include "Hooks.h"
#include "BattleSpeed.h"

namespace BattleSpeed
{
	#define BATTLE_SPEED_DEFAULT 4
	#define BATTLE_SPEED_MAX 8
	#define BATTLE_SPEED_VALUE(index) (10 + (index) * 5)
	#define STOCK_ANIM_TICK 66
	#define IDLE_SET_SIZE 256

	VOID Set(DWORD index)
	{
		config.battle.speedIndex = index;
		config.battle.speedValue = BATTLE_SPEED_VALUE(index);
	}

	VOID Reset()
	{
		Set(BATTLE_SPEED_DEFAULT);
	}

	VOID LoadConfig()
	{
		INT value = Config::Get(CONFIG_WRAPPER, "BattleSpeed", BATTLE_SPEED_DEFAULT);
		Set(value < 0 || value > BATTLE_SPEED_MAX ? BATTLE_SPEED_DEFAULT : value);
	}

	VOID CheckMenu()
	{
		for (DWORD id = IDM_BATTLE_SPEED_1_0; id <= IDM_BATTLE_SPEED_5_0; ++id)
			CheckMenuItem(config.menu, id, MF_BYCOMMAND | (id - IDM_BATTLE_SPEED_1_0 == config.battle.speedIndex ? MF_CHECKED : MF_UNCHECKED));
	}

	BOOL Command(WPARAM id)
	{
		if (id < IDM_BATTLE_SPEED_1_0 || id > IDM_BATTLE_SPEED_5_0)
			return FALSE;

		DWORD index = id - IDM_BATTLE_SPEED_1_0;
		if (index != config.battle.speedIndex)
		{
			Set(index);
			Config::Set(CONFIG_WRAPPER, "BattleSpeed", *(INT*)&index);

			CHAR text[64];
			StrPrint(text, "Battle Speed: %.1fx", 0.1f * config.battle.speedValue);
			Hooks::PrintText(text);

			Hooks::SetGameSpeed();
			CheckMenu();
		}

		return TRUE;
	}

	DWORD* idleAnims[IDLE_SET_SIZE];
	DWORD idleCount;
	DWORD idleLoading;

	BOOL IsIdle(DWORD* anim)
	{
		if (idleCount)
		{
			for (DWORD i = 0; i < IDLE_SET_SIZE; ++i)
			{
				if (idleAnims[i] == anim)
					return TRUE;
			}
		}

		return FALSE;
	}

	VOID TagIdle(DWORD* anim)
	{
		for (DWORD i = 0; i < IDLE_SET_SIZE; ++i)
		{
			if (!idleAnims[i])
			{
				idleAnims[i] = anim;
				++idleCount;
				return;
			}
		}
	}

	VOID UntagIdle(DWORD* anim)
	{
		if (!idleCount)
			return;

		for (DWORD i = 0; i < IDLE_SET_SIZE; ++i)
		{
			if (idleAnims[i] == anim)
			{
				idleAnims[i] = NULL;
				--idleCount;
				return;
			}
		}
	}

	DWORD sub_AddSlow, sub_RemoveSlow;
	DWORD __fastcall AddSlow(DWORD* animator, DWORD, DWORD* anim)
	{
		if (idleLoading)
			TagIdle(anim);
		return ((DWORD(__thiscall*)(DWORD*, DWORD*))sub_AddSlow)(animator, anim);
	}

	DWORD __fastcall RemoveSlow(DWORD* animator, DWORD, DWORD* anim)
	{
		UntagIdle(anim);
		return ((DWORD(__thiscall*)(DWORD*, DWORD*))sub_RemoveSlow)(animator, anim);
	}

	DWORD sub_IdleLoad, idleReturn;
	// Discipl2.exe v3.01b 0x00653AD0: thiscall with two stack args, so the stub keeps the caller's stack as is
	VOID __declspec(naked) IdleLoadHook()
	{
		__asm {
			pop idleReturn
			inc idleLoading
			call sub_IdleLoad
			dec idleLoading
			jmp idleReturn
		}
	}

	BOOL __stdcall SkipUpdate(DWORD* anim, DWORD* timer)
	{
		if (!config.battle.active || !IsIdle(anim))
			return FALSE;

		ULONGLONG interval = timer[13];
		if (!interval || interval >= STOCK_ANIM_TICK)
			return FALSE;

		ULONGLONG tick = timer[15];
		return (tick + 1) * interval / STOCK_ANIM_TICK == tick * interval / STOCK_ANIM_TICK;
	}

	// Discipl2.exe v3.01b 0x0051E166: slow animation update call, anim pushed, slow timer in esi (+0x34 interval, +0x3C tick)
	VOID __declspec(naked) DispatchHook()
	{
		__asm {
			push esi
			push dword ptr [esp+8]
			call SkipUpdate
			test eax, eax
			jnz lbl_skip

			mov eax, [esp+4]
			mov ecx, [eax]
			jmp dword ptr [ecx+4]

			lbl_skip:
			retn 4
		}
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!(hookSpace->battle_anim_dispatch && hookSpace->battle_animator && hookSpace->battle_idle_load))
			return;

		if (!ReadDWord(hooker, hookSpace->battle_animator, &sub_AddSlow) || !ReadDWord(hooker, hookSpace->battle_animator + 8, &sub_RemoveSlow))
			return;

		PatchPtr(hooker, hookSpace->battle_animator, AddSlow);
		PatchPtr(hooker, hookSpace->battle_animator + 8, RemoveSlow);
		sub_IdleLoad = RedirectCall(hooker, hookSpace->battle_idle_load, IdleLoadHook);
		PatchCall(hooker, hookSpace->battle_anim_dispatch, DispatchHook);
	}
}
