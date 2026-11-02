#pragma once

#include "Hooker.h"
#include "Hooks.h"

namespace FasterEndTurn
{
	DWORD SpellFactor();
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);
}
