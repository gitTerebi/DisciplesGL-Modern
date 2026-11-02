#include "StdAfx.h"
#include "Config.h"
#include "Hooks.h"
#include "WalkSpeed.h"

namespace WalkSpeed
{
	#define STOCK_MAP_TICK 33
	#define TARGET_TICK_RATE 60

	LONG __stdcall ScaleStep(LONG step)
	{
		LONG tick = config.speed.hooked && config.scroll.check ? config.scroll.check : STOCK_MAP_TICK;
		LONG scaled = (step * tick * TARGET_TICK_RATE + 500) / 1000;
		return scaled > 0 ? scaled : 1;
	}

	// Discipl2.exe v3.01b 0x004F5D7E: walk step at [ebp+0xC] after the fast-movement halvings, before the frame loop
	VOID __declspec(naked) StepHook()
	{
		__asm {
			push ecx
			push edx
			push [ebp+0xC]
			call ScaleStep
			mov [ebp+0xC], eax
			pop edx
			pop ecx

			mov eax, [ebp-0x7C]
			and dword ptr [ebp-0x10], 0
			retn
		}
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (hookSpace->walk_step)
			PatchCall(hooker, hookSpace->walk_step, StepHook, 2);
	}
}
