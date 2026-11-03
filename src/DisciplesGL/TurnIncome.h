#pragma once

#include "Hooker.h"
#include "Hooks.h"

namespace TurnIncome
{
	VOID Arm();
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);
}
