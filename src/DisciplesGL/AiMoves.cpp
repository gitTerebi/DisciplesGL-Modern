#include "StdAfx.h"
#include "Config.h"
#include "Hooks.h"
#include "AiMoves.h"

#define f(a) (a + baseOffset)

namespace AiMoves
{
	#define AI_STEP_SCALE 2
	#define JUMP_TASKS 8

	DWORD sub_GetMapCenter, sub_SetMapCenter;
	DWORD* viewHolder;
	DWORD fastestStep;

	struct {
		DWORD* view;
		POINT pos;
		POINT offset;
	} saved;

	// Discipl2.exe v3.01b 0x004F5D30: walk step at [ebp+0xC] for the last opponent speed, isPlayer at [ebp+0x10]
	VOID __declspec(naked) StackStep()
	{
		__asm {
			push eax
			mov eax, fastestStep
			cmp byte ptr [ebp+0x10], 0
			jne lbl_set
			imul eax, eax, AI_STEP_SCALE

			lbl_set:
			mov [ebp+0xC], eax
			pop eax
			retn
		}
	}

	DWORD sub_StackScroll, sub_StackIsPlayer;
	BOOL IsAiStack(DWORD* id, DWORD objectMap)
	{
		return !(BYTE)((DWORD(__stdcall*)(DWORD*, DWORD))sub_StackIsPlayer)(id, objectMap);
	}

	DWORD* jumpTasks[JUMP_TASKS];
	DWORD jumpNext;
	BOOL jumpScroll;

	VOID __stdcall AddJumpTask(DWORD* task)
	{
		jumpTasks[jumpNext] = task;
		jumpNext = (jumpNext + 1) % JUMP_TASKS;
	}

	BOOL __stdcall TakeJumpTask(DWORD* task)
	{
		for (DWORD i = 0; i < JUMP_TASKS; ++i)
		{
			if (jumpTasks[i] == task)
			{
				jumpTasks[i] = NULL;
				return TRUE;
			}
		}

		return FALSE;
	}

	BOOL __stdcall StackScroll(DWORD* path, BYTE* move, DWORD objectMap, DWORD player, POINT* pos, DWORD always, DWORD nearby, DWORD anim)
	{
		jumpScroll = IsAiStack((DWORD*)(move + 0x14), objectMap);
		if (jumpScroll)
			pos = (POINT*)(((DWORD*)path[1])[1] + 8 + 8);

		BOOL res = ((BOOL(__stdcall*)(DWORD, DWORD, POINT*, DWORD, DWORD, DWORD))sub_StackScroll)(objectMap, player, pos, always, nearby, anim);
		jumpScroll = FALSE;
		return res;
	}

	VOID __declspec(naked) StackScrollHook()
	{
		__asm {
			pop eax
			push [ebp+0x14]
			push [ebp+0xC]
			push eax
			jmp StackScroll
		}
	}

	DWORD sub_SmoothScroll;
	VOID __declspec(naked) ScrollCreateSmooth()
	{
		__asm {
			cmp jumpScroll, 0
			je lbl_stock
			push [ebp+0x14]
			call AddJumpTask
			xor eax, eax
			retn

			lbl_stock:
			jmp sub_SmoothScroll
		}
	}

	VOID __declspec(naked) ScrollStartSmooth()
	{
		__asm {
			push ecx
			push esi
			call TakeJumpTask
			pop ecx
			test eax, eax
			jz lbl_stock
			xor eax, eax
			retn

			lbl_stock:
			jmp sub_SmoothScroll
		}
	}

	DWORD sub_WalkUpdate, sub_WalkCenter;
	BOOL walkStill;
	VOID __fastcall WalkUpdate(DWORD* task)
	{
		DWORD* data = (DWORD*)task[2];
		walkStill = data && IsAiStack(data + 1, data[0]);
		((VOID(__thiscall*)(DWORD*))sub_WalkUpdate)(task);
		walkStill = FALSE;
	}

	VOID __fastcall WalkCenter(DWORD* view, DWORD, POINT* pos, LONG x, LONG y)
	{
		if (!walkStill)
			((VOID(__thiscall*)(DWORD*, POINT*, LONG, LONG))sub_WalkCenter)(view, pos, x, y);
	}

	DWORD* GetView()
	{
		return (DWORD*)viewHolder[1];
	}

	DWORD sub_EndTurn;
	VOID __fastcall EndTurn(DWORD* phase)
	{
		DWORD* view = GetView();
		saved.view = view;
		if (view)
			((BOOL(__thiscall*)(DWORD*, POINT*, LONG*, LONG*))sub_GetMapCenter)(view, &saved.pos, &saved.offset.x, &saved.offset.y);

		((VOID(__thiscall*)(DWORD*))sub_EndTurn)(phase);
	}

	BOOL RestoreView()
	{
		DWORD* view = saved.view;
		saved.view = NULL;
		if (!view || view != GetView())
			return FALSE;

		((VOID(__thiscall*)(DWORD*, POINT*, LONG, LONG))sub_SetMapCenter)(view, &saved.pos, saved.offset.x, saved.offset.y);
		return TRUE;
	}

	DWORD sub_TurnCenterCapital, sub_TurnCenterStack, sub_TurnCenterObject;
	VOID __fastcall TurnCenterCapital(DWORD* handler)
	{
		if (!RestoreView())
			((VOID(__thiscall*)(DWORD*))sub_TurnCenterCapital)(handler);
	}

	VOID __fastcall TurnCenterStack(DWORD* handler, DWORD, DWORD objectMap, DWORD* id)
	{
		if (!RestoreView())
			((VOID(__thiscall*)(DWORD*, DWORD, DWORD*))sub_TurnCenterStack)(handler, objectMap, id);
	}

	VOID __fastcall TurnCenterObject(DWORD* handler, DWORD, DWORD objectMap, DWORD* id)
	{
		if (!RestoreView())
			((VOID(__thiscall*)(DWORD*, DWORD, DWORD*))sub_TurnCenterObject)(handler, objectMap, id);
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (hookSpace->ai_move_step && ReadDWord(hooker, hookSpace->ai_move_step + 3, &fastestStep))
			PatchCall(hooker, hookSpace->ai_move_step, StackStep, 2);

		if (hookSpace->ai_move_scroll_start && hookSpace->ai_move_walk_update && ReadDWord(hooker, hookSpace->ai_move_walk_update, &sub_WalkUpdate))
		{
			sub_StackIsPlayer = f(hookSpace->ai_move_is_player);
			sub_StackScroll = RedirectCall(hooker, hookSpace->ai_move_scroll, StackScrollHook);
			sub_SmoothScroll = RedirectCall(hooker, hookSpace->ai_move_scroll_create, ScrollCreateSmooth);
			RedirectCall(hooker, hookSpace->ai_move_scroll_start, ScrollStartSmooth);

			PatchPtr(hooker, hookSpace->ai_move_walk_update, WalkUpdate);
			sub_WalkCenter = RedirectCall(hooker, hookSpace->ai_move_walk_follow, WalkCenter);
			RedirectCall(hooker, hookSpace->ai_move_walk_end, WalkCenter);
		}

		if (hookSpace->ai_move_end_turn && hookSpace->turn_center_capital && ReadDWord(hooker, hookSpace->ai_move_end_turn, &sub_EndTurn))
		{
			viewHolder = (DWORD*)f(hookSpace->ai_move_view);
			sub_GetMapCenter = f(hookSpace->map_center_get);
			sub_SetMapCenter = f(hookSpace->map_center_set);
			PatchPtr(hooker, hookSpace->ai_move_end_turn, EndTurn);

			// Hooks.cpp LoadV2 redirects these for FasterEndTurn first, so the originals here are its wrappers
			sub_TurnCenterCapital = RedirectCall(hooker, hookSpace->turn_center_capital, TurnCenterCapital);
			sub_TurnCenterStack = RedirectCall(hooker, hookSpace->turn_center_stack, TurnCenterStack);
			sub_TurnCenterObject = RedirectCall(hooker, hookSpace->turn_center_object, TurnCenterObject);
		}
	}
}
