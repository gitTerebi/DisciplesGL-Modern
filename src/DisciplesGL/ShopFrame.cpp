#include "StdAfx.h"
#include "Hooks.h"
#include "ShopFrame.h"

#define f(a) (a + baseOffset)

namespace ShopFrame
{
	#define SHOP_FRAME_IMAGE "DRAGDROP_SHOP_VALID"

	DWORD sub_FrameDraw;
	DWORD sub_ListBox;
	DWORD sub_LoadImage;
	DWORD* interfImages;

	DWORD* shopImage;
	DWORD shopImageStore;

	LONG ImageHeight(DWORD* image)
	{
		POINT size;
		return ((POINT*(__thiscall*)(DWORD*, POINT*))(*(DWORD**)image)[1])(image, &size)->y;
	}

	// Discipl2.exe v2.02b 0x005C4317 returns NULL for an image not yet loaded once a screen is built
	DWORD* __stdcall LoadFrame(const CHAR* name)
	{
		DWORD* image = ((DWORD*(__stdcall*)(const CHAR*))sub_LoadImage)(name);

		if (!shopImage || shopImageStore != *interfImages)
		{
			shopImageStore = *interfImages;
			shopImage = ((DWORD*(__stdcall*)(const CHAR*))sub_LoadImage)(SHOP_FRAME_IMAGE);
		}

		return image;
	}

	// Discipl2.exe v2.02b 0x0056EEF5 centres the one fixed-size frame image on the list box
	VOID __fastcall FrameDraw(DWORD* target, DWORD /*edx*/, DWORD renderer)
	{
		DWORD* drop = target - 1;
		BYTE* data = (BYTE*)drop[2];
		DWORD** slot = (DWORD**)(data + 0x48);
		DWORD* image = *slot;

		if (data[0x4C] && image && shopImage)
		{
			DWORD* listBox = ((DWORD*(__thiscall*)(DWORD*))sub_ListBox)(drop);
			RECT* rc = ((RECT*(__thiscall*)(DWORD*))(*(DWORD**)listBox)[12])(listBox);
			LONG height = rc->bottom - rc->top;
			LONG own = ImageHeight(image);

			if (height > own && abs(ImageHeight(shopImage) - height) < abs(own - height))
			{
				*slot = shopImage;
				((VOID(__thiscall*)(DWORD*, DWORD))sub_FrameDraw)(target, renderer);
				*slot = image;
				return;
			}
		}

		((VOID(__thiscall*)(DWORD*, DWORD))sub_FrameDraw)(target, renderer);
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!(hookSpace->shop_frame_draw && hookSpace->shop_frame_list && hookSpace->shop_frame_load_call && hookSpace->shop_frame_images))
			return;

		sub_ListBox = f(hookSpace->shop_frame_list);
		interfImages = (DWORD*)f(hookSpace->shop_frame_images);
		sub_LoadImage = RedirectCall(hooker, hookSpace->shop_frame_load_call, LoadFrame);
		sub_FrameDraw = RedirectCall(hooker, hookSpace->shop_frame_draw, FrameDraw);
	}
}
