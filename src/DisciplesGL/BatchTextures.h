#pragma once

#include "Hooker.h"
#include "Hooks.h"

namespace BatchTextures
{
	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset);
}
