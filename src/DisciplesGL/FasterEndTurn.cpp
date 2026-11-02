#include "StdAfx.h"
#include "Config.h"
#include "Hooks.h"
#include "FasterEndTurn.h"

#define f(a) (a + baseOffset)

namespace FasterEndTurn
{
	#define AI_STEP_SCALE 2
	#define TASK_SET_SIZE 8

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
		return config.fasterEndTurn.enabled && !(BYTE)((DWORD(__stdcall*)(DWORD*, DWORD))sub_StackIsPlayer)(id, objectMap);
	}

	struct TaskSet {
		DWORD* items[TASK_SET_SIZE];
		DWORD next;
	} jumpTasks, enemySpells;

	VOID AddTask(TaskSet* set, DWORD* task)
	{
		set->items[set->next] = task;
		set->next = (set->next + 1) % TASK_SET_SIZE;
	}

	BOOL TakeTask(TaskSet* set, DWORD* task)
	{
		for (DWORD i = 0; i < TASK_SET_SIZE; ++i)
		{
			if (set->items[i] == task)
			{
				set->items[i] = NULL;
				return TRUE;
			}
		}

		return FALSE;
	}

	VOID __stdcall AddJumpTask(DWORD* task)
	{
		AddTask(&jumpTasks, task);
	}

	BOOL __stdcall TakeJumpTask(DWORD* task)
	{
		return TakeTask(&jumpTasks, task);
	}

	BOOL jumpScroll;

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

	DWORD sub_GetApp, sub_GetSettings, sub_GetOpponentSpeed;
	DWORD GetOpponentSpeed()
	{
		DWORD app = ((DWORD(__stdcall*)())sub_GetApp)();
		DWORD settings = ((DWORD(__thiscall*)(DWORD))sub_GetSettings)(app);
		return ((DWORD(__thiscall*)(DWORD))sub_GetOpponentSpeed)(settings);
	}

	DWORD spellFactor = 1;
	DWORD SpellFactor()
	{
		return spellFactor;
	}

	VOID SetSpellFactor(DWORD factor)
	{
		if (spellFactor != factor)
		{
			spellFactor = factor;
			Hooks::SetGameSpeed();
		}
	}

	DWORD sub_SpellTask, sub_SpellCaster;
	DWORD* __stdcall SpellTask(DWORD* msg, DWORD* player, DWORD objectMap, DWORD target, DWORD flag)
	{
		DWORD* task = ((DWORD*(__stdcall*)(DWORD, DWORD, DWORD))sub_SpellTask)(objectMap, target, flag);
		if (task && *((DWORD*(__thiscall*)(DWORD*))sub_SpellCaster)(msg) != *player)
			AddTask(&enemySpells, task);

		return task;
	}

	// Discipl2.exe v3.01b 0x004F31D0: CCmdCastSpellMsg keeps the message at [ebp+0x14] and the viewing player id at [ebp+8]
	VOID __declspec(naked) SpellTaskHook()
	{
		__asm {
			pop eax
			push [ebp+0x8]
			push [ebp+0x14]
			push eax
			jmp SpellTask
		}
	}

	DWORD* activeSpell;
	VOID EndSpell(DWORD* task)
	{
		TakeTask(&enemySpells, task);
		if (task == activeSpell)
		{
			activeSpell = NULL;
			SetSpellFactor(1);
		}
	}

	DWORD sub_SpellStart, sub_SpellFinish, sub_SpellRelease;
	VOID __fastcall SpellStart(DWORD* task)
	{
		if (TakeTask(&enemySpells, task))
		{
			DWORD speed = GetOpponentSpeed();
			activeSpell = task;
			SetSpellFactor(speed ? speed : 1);
		}

		((VOID(__thiscall*)(DWORD*))sub_SpellStart)(task);
	}

	VOID __fastcall SpellFinish(DWORD* task)
	{
		EndSpell(task);
		((VOID(__thiscall*)(DWORD*))sub_SpellFinish)(task);
	}

	DWORD* __fastcall SpellRelease(DWORD* task, DWORD, DWORD flag)
	{
		EndSpell(task);
		return ((DWORD*(__thiscall*)(DWORD*, DWORD))sub_SpellRelease)(task, flag);
	}

	DWORD* GetView()
	{
		return (DWORD*)viewHolder[1];
	}

	DWORD sub_EndTurn;
	VOID __fastcall EndTurn(DWORD* phase)
	{
		DWORD* view = config.fasterEndTurn.enabled ? GetView() : NULL;
		saved.view = view;
		if (view)
			((BOOL(__thiscall*)(DWORD*, POINT*, LONG*, LONG*))sub_GetMapCenter)(view, &saved.pos, &saved.offset.x, &saved.offset.y);

		((VOID(__thiscall*)(DWORD*))sub_EndTurn)(phase);
	}

	BOOL SkipTurnCenter()
	{
		DWORD* view = saved.view;
		saved.view = NULL;
		if (!config.fasterEndTurn.enabled)
			return FALSE;

		if (view && view == GetView())
			((VOID(__thiscall*)(DWORD*, POINT*, LONG, LONG))sub_SetMapCenter)(view, &saved.pos, saved.offset.x, saved.offset.y);

		return TRUE;
	}

	DWORD sub_TurnCenterCapital, sub_TurnCenterStack, sub_TurnCenterObject;
	VOID __fastcall TurnCenterCapital(DWORD* handler)
	{
		if (!SkipTurnCenter())
			((VOID(__thiscall*)(DWORD*))sub_TurnCenterCapital)(handler);
	}

	VOID __fastcall TurnCenterStack(DWORD* handler, DWORD, DWORD objectMap, DWORD* id)
	{
		if (!SkipTurnCenter())
			((VOID(__thiscall*)(DWORD*, DWORD, DWORD*))sub_TurnCenterStack)(handler, objectMap, id);
	}

	VOID __fastcall TurnCenterObject(DWORD* handler, DWORD, DWORD objectMap, DWORD* id)
	{
		if (!SkipTurnCenter())
			((VOID(__thiscall*)(DWORD*, DWORD, DWORD*))sub_TurnCenterObject)(handler, objectMap, id);
	}

	DWORD sub_TerrainScroll;
	BOOL __stdcall TerrainScroll(DWORD objectMap, DWORD player, POINT* pos, DWORD always, DWORD nearby, DWORD anim)
	{
		return !config.fasterEndTurn.enabled && ((BOOL(__stdcall*)(DWORD, DWORD, POINT*, DWORD, DWORD, DWORD))sub_TerrainScroll)(objectMap, player, pos, always, nearby, anim);
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (hookSpace->ai_move_step && ReadDWord(hooker, hookSpace->ai_move_step + 3, &fastestStep))
			PatchCall(hooker, hookSpace->ai_move_step, StackStep, 2);

		if (config.speed.hooked && hookSpace->ai_spell_vtable && ReadDWord(hooker, hookSpace->ai_spell_vtable, &sub_SpellRelease)
			&& ReadDWord(hooker, hookSpace->ai_spell_vtable + 8, &sub_SpellStart) && ReadDWord(hooker, hookSpace->ai_spell_vtable + 12, &sub_SpellFinish))
		{
			sub_GetApp = f(hookSpace->ai_opponent_app);
			sub_GetSettings = f(hookSpace->ai_opponent_settings);
			sub_GetOpponentSpeed = f(hookSpace->ai_opponent_speed);
			sub_SpellCaster = f(hookSpace->ai_spell_caster);
			sub_SpellTask = RedirectCall(hooker, hookSpace->ai_spell_task, SpellTaskHook);
			RedirectCall(hooker, hookSpace->ai_spell_task_target, SpellTaskHook);

			PatchPtr(hooker, hookSpace->ai_spell_vtable, SpellRelease);
			PatchPtr(hooker, hookSpace->ai_spell_vtable + 8, SpellStart);
			PatchPtr(hooker, hookSpace->ai_spell_vtable + 12, SpellFinish);
		}

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

		// Discipl2.exe v3.01b 0x004912B8: CCmdTurnInfoMsg centres the map on the last selection or the capital
		// 0x004F3179: CCmdTerrainChangeMsg scrolls to each town spreading terrain; false is the stock unseen path
		if (hookSpace->turn_center_capital)
		{
			config.fasterEndTurn.hooked = TRUE;
			sub_TerrainScroll = RedirectCall(hooker, hookSpace->turn_terrain_scroll, TerrainScroll);

			if (hookSpace->ai_move_end_turn && ReadDWord(hooker, hookSpace->ai_move_end_turn, &sub_EndTurn))
			{
				viewHolder = (DWORD*)f(hookSpace->ai_move_view);
				sub_GetMapCenter = f(hookSpace->map_center_get);
				sub_SetMapCenter = f(hookSpace->map_center_set);
				PatchPtr(hooker, hookSpace->ai_move_end_turn, EndTurn);
			}

			sub_TurnCenterCapital = RedirectCall(hooker, hookSpace->turn_center_capital, TurnCenterCapital);
			sub_TurnCenterStack = RedirectCall(hooker, hookSpace->turn_center_stack, TurnCenterStack);
			sub_TurnCenterObject = RedirectCall(hooker, hookSpace->turn_center_object, TurnCenterObject);
		}
	}
}
