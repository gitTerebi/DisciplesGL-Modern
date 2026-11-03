#include "StdAfx.h"
#include "Config.h"
#include "Hooks.h"
#include "TurnSummary.h"
#include "TurnIncome.h"

#define f(a) (a + baseOffset)

namespace TurnSummary
{
	DWORD sub_SetState, sub_Ok, stateVtable, dialogVtable;

	// Discipl2.exe v2.02b 0x004D4E9D: the begin turn state keeps its DLG_BEGIN_TURN dialog at [[state+0x10]+4]
	VOID __fastcall SetState(DWORD machine, DWORD, DWORD* state)
	{
		((VOID(__thiscall*)(DWORD, DWORD*))sub_SetState)(machine, state);

		if (!config.skipTurnSummary.enabled || !state || *state != stateVtable)
			return;

		TurnIncome::Arm();

		DWORD* data = (DWORD*)state[4];
		DWORD* dialog = data ? (DWORD*)data[1] : NULL;
		if (dialog && *dialog == dialogVtable)
			((VOID(__thiscall*)(DWORD*))sub_Ok)(state);
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (hookSpace->begin_turn_info_state)
		{
			config.skipTurnSummary.hooked = TRUE;
			stateVtable = f(hookSpace->begin_turn_vtable);
			dialogVtable = f(hookSpace->begin_turn_dialog_vtable);
			sub_Ok = f(hookSpace->begin_turn_ok);
			sub_SetState = RedirectCall(hooker, hookSpace->begin_turn_info_state, SetState);
			RedirectCall(hooker, hookSpace->begin_turn_load_state, SetState);
		}
	}
}
