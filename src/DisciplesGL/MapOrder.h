#pragma once

#include "Hooker.h"
#include "Hooks.h"

namespace MapOrder
{
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);
	VOID Dump();
}
