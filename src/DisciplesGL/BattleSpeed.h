#pragma once

#include "Hooker.h"
#include "Hooks.h"

namespace BattleSpeed
{
	VOID Reset();
	VOID LoadConfig();
	VOID CheckMenu();
	BOOL Command(WPARAM id);
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);
}
