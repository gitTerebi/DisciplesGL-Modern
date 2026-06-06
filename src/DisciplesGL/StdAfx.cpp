/*
	MIT License

	Copyright (c) 2020 Oleksiy Ryabchun

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "stdafx.h"
#include "intrin.h"
#include "Mmsystem.h"

HMODULE hDllModule;
HANDLE hActCtx;
CHAR snapshotName[MAX_PATH];

void DebugLog(const char* format, ...)
{
	CHAR path[MAX_PATH];
	DWORD length = GetModuleFileName(NULL, path, sizeof(path));
	while (length && path[length - 1] != '\\')
		--length;
	StrCopy(path + length, "DisciplesGL-debug.log");

	HANDLE file = CreateFile(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return;

	CHAR text[512];
	SYSTEMTIME time;
	GetLocalTime(&time);
	INT offset = wsprintf(text, "%02u:%02u:%02u.%03u ", time.wHour, time.wMinute, time.wSecond, time.wMilliseconds);

	va_list args;
	va_start(args, format);
	wvsprintf(text + offset, format, args);
	va_end(args);

	StrCat(text, "\r\n");
	DWORD written;
	WriteFile(file, text, StrLength(text), &written, NULL);
	CloseHandle(file);
}

CREATEACTCTXA CreateActCtxC;
RELEASEACTCTX ReleaseActCtxC;
ACTIVATEACTCTX ActivateActCtxC;
DEACTIVATEACTCTX DeactivateActCtxC;

SETPROCESSDPIAWARENESS SetProcessDpiAwarenessC;

VOID __declspec(naked) __stdcall ex_l4linkSt()
{
	_asm
	{
		mov eax, ecx
		mov ecx, [esp+4]
		mov edx, [ecx]
		mov [eax], edx
		mov ecx, [ecx+4]
		mov [eax+4], ecx
		retn 4
	}
}

typedef VOID _P0;
struct _P4 { BYTE p[4]; };
struct _P8 { BYTE p[8]; };
struct _P12 { BYTE p[12]; };
struct _P16 { BYTE p[16]; };
struct _P20 { BYTE p[20]; };
struct _P24 { BYTE p[24]; };

#define LIBIMP(a, b) extern "C" VOID __stdcall _##a(_P##b);
#define LIBEXP(a, b) LIBIMP(a, b) VOID __declspec(naked) __stdcall ex_##a() { _asm jmp _##a }

LIBEXP(area4add_object, 8)
LIBEXP(area4create, 16)
LIBEXP(area4free, 4)
LIBEXP(area4objFirst, 4)
LIBEXP(area4objLast, 4)
LIBEXP(area4objNext, 8)
LIBEXP(area4objPrev, 8)
LIBEXP(area4pageBreak, 8)
LIBEXP(area4sort_obj_tree, 4)
LIBEXP(bmp4FindDIBBits, 4)
LIBEXP(bmp4GetDIB, 8)
LIBEXP(bmp4PaletteSize, 4)
LIBEXP(bmp4WriteDIB, 8)
LIBEXP(c4atod2, 12)
LIBEXP(c4atod, 8)
LIBEXP(c4atoi, 8)
LIBEXP(c4atol, 8)
LIBEXP(c4calcType, 4)
LIBEXP(c4dtoa45, 20)
LIBEXP(c4encode, 16)
LIBEXP(c4getAccessMode, 4)
LIBEXP(c4getAutoOpen, 4)
LIBEXP(c4getErrDefaultUnique, 4)
LIBEXP(c4getErrExpr, 4)
LIBEXP(c4getErrFieldName, 4)
LIBEXP(c4getErrGo, 4)
LIBEXP(c4getErrOpen, 4)
LIBEXP(c4getErrRelate, 4)
LIBEXP(c4getErrSkip, 4)
LIBEXP(c4getErrTagName, 4)
LIBEXP(c4getErrorCode, 4)
LIBEXP(c4getLockAttempts, 4)
LIBEXP(c4getLockEnforce, 4)
LIBEXP(c4getOptimize, 4)
LIBEXP(c4getOptimizeWrite, 4)
LIBEXP(c4getReadLock, 4)
LIBEXP(c4getReadOnly, 4)
LIBEXP(c4getSafety, 4)
LIBEXP(c4getSingleOpen, 4)
LIBEXP(c4lower, 4)
LIBEXP(c4ltoa45, 12)
LIBEXP(c4setAccessMode, 8)
LIBEXP(c4setAutoOpen, 8)
LIBEXP(c4setErrDefaultUnique, 8)
LIBEXP(c4setErrExpr, 8)
LIBEXP(c4setErrFieldName, 8)
LIBEXP(c4setErrGo, 8)
LIBEXP(c4setErrOpen, 8)
LIBEXP(c4setErrRelate, 8)
LIBEXP(c4setErrSkip, 8)
LIBEXP(c4setErrTagName, 8)
LIBEXP(c4setErrorCode, 8)
LIBEXP(c4setLockAttempts, 8)
LIBEXP(c4setLockEnforce, 8)
LIBEXP(c4setOptimize, 8)
LIBEXP(c4setOptimizeWrite, 8)
LIBEXP(c4setReadLock, 8)
LIBEXP(c4setReadOnly, 8)
LIBEXP(c4setSafety, 8)
LIBEXP(c4setSingleOpen, 8)
LIBEXP(c4trimN, 8)
LIBEXP(code4allocLow, 12)
LIBEXP(code4calcCreate, 12)
LIBEXP(code4calcReset, 4)
LIBEXP(code4close, 4)
LIBEXP(code4connect, 24)
LIBEXP(code4data, 8)
LIBEXP(code4dateFormat, 4)
LIBEXP(code4dateFormatSet, 8)
LIBEXP(code4exit, 4)
LIBEXP(code4flush, 4)
LIBEXP(code4indexExtension, 4)
LIBEXP(code4indexFormat, 4)
LIBEXP(code4info, 4)
LIBEXP(code4initLow, 12)
LIBEXP(code4initUndo, 4)
LIBEXP(code4lock, 4)
LIBEXP(code4lockClear, 4)
LIBEXP(code4lockFileName, 4)
LIBEXP(code4lockItem, 4)
LIBEXP(code4lockNetworkId, 4)
LIBEXP(code4lockUserId, 4)
LIBEXP(code4logCreate, 12)
LIBEXP(code4logFileName, 4)
LIBEXP(code4logOpen, 12)
LIBEXP(code4logOpenOff, 4)
LIBEXP(code4optAll, 4)
LIBEXP(code4optStart, 4)
LIBEXP(code4optSuspend, 4)
LIBEXP(code4serverName, 4)
LIBEXP(code4timeout, 4)
LIBEXP(code4timeoutSet, 8)
LIBEXP(code4tranCommit, 4)
LIBEXP(code4tranRollback, 4)
LIBEXP(code4tranStart, 4)
LIBEXP(code4unlock, 4)
LIBEXP(d4alias, 4)
LIBEXP(d4aliasSet, 8)
LIBEXP(d4append, 4)
LIBEXP(d4appendBlank, 4)
LIBEXP(d4appendStart, 8)
LIBEXP(d4blank, 4)
LIBEXP(d4bof, 4)
LIBEXP(d4bottom, 4)
LIBEXP(d4changed, 8)
LIBEXP(d4check, 4)
LIBEXP(d4close, 4)
LIBEXP(d4create, 16)
LIBEXP(d4createTemp, 12)
LIBEXP(d4delete, 4)
LIBEXP(d4deleted, 4)
LIBEXP(d4eof, 4)
LIBEXP(d4field, 8)
LIBEXP(d4fieldInfo, 4)
LIBEXP(d4fieldJ, 8)
LIBEXP(d4fieldNumber, 8)
LIBEXP(d4fileName, 4)
LIBEXP(d4flush, 4)
LIBEXP(d4freeBlocks, 4)
LIBEXP(d4go, 8)
LIBEXP(d4goBof, 4)
LIBEXP(d4goEof, 4)
LIBEXP(d4index, 8)
LIBEXP(d4lock, 8)
LIBEXP(d4lockAdd, 8)
LIBEXP(d4lockAddAll, 4)
LIBEXP(d4lockAddAppend, 4)
LIBEXP(d4lockAddFile, 4)
LIBEXP(d4lockAll, 4)
LIBEXP(d4lockAppend, 4)
LIBEXP(d4lockFile, 4)
LIBEXP(d4lockTest, 8)
LIBEXP(d4lockTestAppendLow, 4)
LIBEXP(d4log, 8)
LIBEXP(d4memoCompress, 4)
LIBEXP(d4numFields, 4)
LIBEXP(d4open, 8)
LIBEXP(d4openClone, 4)
LIBEXP(d4optimize, 8)
LIBEXP(d4optimizeWrite, 8)
LIBEXP(d4pack, 4)
LIBEXP(d4position2, 8)
LIBEXP(d4position, 4)
LIBEXP(d4positionSet, 12)
LIBEXP(d4recCountDo, 4)
LIBEXP(d4recNo, 4)
LIBEXP(d4recPosition, 8)
LIBEXP(d4recWidth, 4)
LIBEXP(d4recall, 4)
LIBEXP(d4record, 4)
LIBEXP(d4recordOld, 4)
LIBEXP(d4refresh, 4)
LIBEXP(d4refreshRecord, 4)
LIBEXP(d4reindex, 4)
LIBEXP(d4remove, 4)
LIBEXP(d4seek, 8)
LIBEXP(d4seekDouble, 12)
LIBEXP(d4seekN, 12)
LIBEXP(d4seekNext, 8)
LIBEXP(d4seekNextDouble, 12)
LIBEXP(d4seekNextN, 12)
LIBEXP(d4skip, 8)
LIBEXP(d4tag, 8)
LIBEXP(d4tagDefault, 4)
LIBEXP(d4tagNext, 8)
LIBEXP(d4tagPrev, 8)
LIBEXP(d4tagSelect, 8)
LIBEXP(d4tagSelected, 4)
LIBEXP(d4tagSync, 8)
LIBEXP(d4top, 4)
LIBEXP(d4unlock, 4)
LIBEXP(d4unlockRecord, 8)
LIBEXP(d4writeLow, 12)
LIBEXP(d4zap, 12)
LIBEXP(date4assign, 8)
LIBEXP(date4cdow, 4)
LIBEXP(date4cmonth, 4)
LIBEXP(date4dow, 4)
LIBEXP(date4format, 12)
LIBEXP(date4formatMdx2, 8)
LIBEXP(date4formatMdx, 4)
LIBEXP(date4init, 12)
LIBEXP(date4isLeap, 4)
LIBEXP(date4long, 4)
LIBEXP(date4timeNow, 4)
LIBEXP(date4today, 4)
LIBEXP(dfile4recCount, 8)
LIBEXP(dfile4remove, 4)
LIBEXP(error4default, 12)
LIBEXP(error4describeDefault, 24)
LIBEXP(error4describeExecute, 24)
LIBEXP(error4exitTest, 4)
LIBEXP(error4file, 12)
LIBEXP(error4hook, 24)
LIBEXP(error4set, 8)
LIBEXP(error4text, 8)
LIBEXP(expr4calcDelete, 4)
LIBEXP(expr4calcLookup, 12)
LIBEXP(expr4calcMassage, 4)
LIBEXP(expr4calcNameChange, 12)
LIBEXP(expr4calcResultPos, 8)
LIBEXP(expr4double2, 8)
LIBEXP(expr4double, 4)
LIBEXP(expr4execute, 12)
LIBEXP(expr4functions, 4)
LIBEXP(expr4key, 12)
LIBEXP(expr4nullLow, 8)
LIBEXP(expr4parseLow, 12)
LIBEXP(expr4source, 4)
LIBEXP(expr4str, 4)
LIBEXP(expr4true, 4)
LIBEXP(expr4vary, 8)
LIBEXP(f4assign, 8)
LIBEXP(f4assignChar, 8)
LIBEXP(f4assignDouble, 12)
LIBEXP(f4assignField, 8)
LIBEXP(f4assignInt, 8)
LIBEXP(f4assignLong, 8)
LIBEXP(f4assignN, 12)
LIBEXP(f4assignNotNull, 4)
LIBEXP(f4assignNull, 4)
LIBEXP(f4assignPtr, 4)
LIBEXP(f4blank, 4)
LIBEXP(f4char, 4)
LIBEXP(f4data, 4)
LIBEXP(f4decimals, 4)
LIBEXP(f4double2, 8)
LIBEXP(f4double, 4)
LIBEXP(f4flagAnd, 8)
LIBEXP(f4flagFlipReturns, 4)
LIBEXP(f4flagInit, 12)
LIBEXP(f4flagIsAllSet, 12)
LIBEXP(f4flagIsAnySet, 12)
LIBEXP(f4flagIsSet, 8)
LIBEXP(f4flagOr, 8)
LIBEXP(f4flagReset, 8)
LIBEXP(f4flagSet, 8)
LIBEXP(f4flagSetAll, 4)
LIBEXP(f4flagSetRange, 12)
LIBEXP(f4int, 4)
LIBEXP(f4len, 4)
LIBEXP(f4long, 4)
LIBEXP(f4memoAssign, 8)
LIBEXP(f4memoAssignN, 12)
LIBEXP(f4memoFree, 4)
LIBEXP(f4memoLen, 4)
LIBEXP(f4memoNcpy, 12)
LIBEXP(f4memoPtr, 4)
LIBEXP(f4memoSetLen, 8)
LIBEXP(f4memoStr, 4)
LIBEXP(f4name, 4)
LIBEXP(f4ncpy, 12)
LIBEXP(f4null, 4)
LIBEXP(f4number, 4)
LIBEXP(f4ptr, 4)
LIBEXP(f4str, 4)
LIBEXP(f4true, 4)
LIBEXP(f4type, 4)
LIBEXP(file4close, 4)
LIBEXP(file4create, 16)
LIBEXP(file4flush, 4)
LIBEXP(file4len, 4)
LIBEXP(file4lenSet, 8)
LIBEXP(file4lock, 12)
LIBEXP(file4open, 16)
LIBEXP(file4openTest, 4)
LIBEXP(file4optimizeLow, 20)
LIBEXP(file4optimizeWrite, 8)
LIBEXP(file4read, 16)
LIBEXP(file4readAll, 16)
LIBEXP(file4readError, 16)
LIBEXP(file4refresh, 4)
LIBEXP(file4replace, 8)
LIBEXP(file4seqRead, 12)
LIBEXP(file4seqReadAll, 12)
LIBEXP(file4seqReadInitDo, 20)
LIBEXP(file4seqWrite, 12)
LIBEXP(file4seqWriteDelay, 4)
LIBEXP(file4seqWriteFlush, 4)
LIBEXP(file4seqWriteInit, 20)
LIBEXP(file4seqWriteRepeat, 12)
LIBEXP(file4unlock, 12)
LIBEXP(file4write, 16)
LIBEXP(group4create, 12)
LIBEXP(group4footerFirst, 4)
LIBEXP(group4footerNext, 8)
LIBEXP(group4footerPrev, 8)
LIBEXP(group4free, 4)
LIBEXP(group4headerFirst, 4)
LIBEXP(group4headerNext, 8)
LIBEXP(group4headerPrev, 8)
LIBEXP(group4numFooters, 4)
LIBEXP(group4positionSet, 8)
LIBEXP(group4repeatHeader, 8)
LIBEXP(group4resetExprSet, 8)
LIBEXP(group4resetPage, 8)
LIBEXP(group4resetPageNum, 8)
LIBEXP(group4swapFooter, 8)
LIBEXP(group4swapHeader, 8)
LIBEXP(i4close, 4)
LIBEXP(i4create, 12)
LIBEXP(i4fileName, 4)
LIBEXP(i4open, 8)
LIBEXP(i4reindex, 4)
LIBEXP(i4tag, 8)
LIBEXP(i4tagAdd, 8)
LIBEXP(i4tagInfo, 4)
LIBEXP(l4addAfter, 12)
LIBEXP(l4addBefore, 12)
LIBEXP(l4addLow, 8)
LIBEXP(l4firstLow, 4)
LIBEXP(l4lastLow, 4)
LIBEXP(l4nextLow, 8)
LIBEXP(l4pop, 4)
LIBEXP(l4prev, 8)
LIBEXP(l4remove, 8)
LIBEXP(mem4alloc2Default, 8)
LIBEXP(mem4allocDefault, 4)
LIBEXP(mem4checkMemory, 0)
LIBEXP(mem4createAllocDefault, 24)
LIBEXP(mem4createDefault, 20)
LIBEXP(mem4freeCheck, 4)
LIBEXP(mem4freeDefault, 8)
LIBEXP(mem4release, 4)
LIBEXP(obj4bitmapFieldCreate, 24)
LIBEXP(obj4bitmapFieldFree, 4)
LIBEXP(obj4bitmapFileCreate, 24)
LIBEXP(obj4bitmapFileFree, 4)
LIBEXP(obj4bitmapStaticCreate, 24)
LIBEXP(obj4bitmapStaticFree, 4)
LIBEXP(obj4brackets, 8)
LIBEXP(obj4calcCreate, 24)
LIBEXP(obj4calcFree, 4)
LIBEXP(obj4dataFieldSet, 20)
LIBEXP(obj4dateFormat, 8)
LIBEXP(obj4decimals, 8)
LIBEXP(obj4delete, 4)
LIBEXP(obj4displayOnce, 8)
LIBEXP(obj4displayZero, 8)
LIBEXP(obj4exprCreate, 24)
LIBEXP(obj4exprFree, 4)
LIBEXP(obj4fieldCreate, 24)
LIBEXP(obj4fieldFree, 4)
LIBEXP(obj4frameCorners, 8)
LIBEXP(obj4frameCreate, 20)
LIBEXP(obj4frameFill, 8)
LIBEXP(obj4frameFree, 4)
LIBEXP(obj4justify, 8)
LIBEXP(obj4leadingZero, 8)
LIBEXP(obj4lineCreate, 20)
LIBEXP(obj4lineFree, 4)
LIBEXP(obj4lineWidth, 8)
LIBEXP(obj4lookAhead, 8)
LIBEXP(obj4numericType, 8)
LIBEXP(obj4remove, 4)
LIBEXP(obj4style, 8)
LIBEXP(obj4textCreate, 24)
LIBEXP(obj4textFree, 4)
LIBEXP(obj4totalCreate, 24)
LIBEXP(obj4totalFree, 4)
LIBEXP(relate4bottom, 4)
LIBEXP(relate4changed, 4)
LIBEXP(relate4createSlave, 16)
LIBEXP(relate4doAll, 4)
LIBEXP(relate4doOne, 4)
LIBEXP(relate4eof, 4)
LIBEXP(relate4errorAction, 8)
LIBEXP(relate4free, 8)
LIBEXP(relate4freeRelate, 8)
LIBEXP(relate4init, 4)
LIBEXP(relate4lockAdd, 4)
LIBEXP(relate4lookup_data, 8)
LIBEXP(relate4matchLen, 8)
LIBEXP(relate4next, 4)
LIBEXP(relate4optimizeable, 4)
LIBEXP(relate4querySet, 8)
LIBEXP(relate4retrieve2, 24)
LIBEXP(relate4retrieve, 16)
LIBEXP(relate4save2, 20)
LIBEXP(relate4save, 12)
LIBEXP(relate4skip, 8)
LIBEXP(relate4skipEnable, 8)
LIBEXP(relate4sortSet, 8)
LIBEXP(relate4top, 4)
LIBEXP(relate4type, 8)
LIBEXP(report4caption, 8)
LIBEXP(report4currency, 8)
LIBEXP(report4dataDo, 4)
LIBEXP(report4dataFileSet, 8)
LIBEXP(report4dataGroup, 8)
LIBEXP(report4decimal, 8)
LIBEXP(report4do, 4)
LIBEXP(report4free, 12)
LIBEXP(report4free_styles, 4)
LIBEXP(report4generatePage, 8)
LIBEXP(report4groupFirst, 4)
LIBEXP(report4groupHardResets, 8)
LIBEXP(report4groupLast, 4)
LIBEXP(report4groupLookup, 8)
LIBEXP(report4groupNext, 8)
LIBEXP(report4groupPrev, 8)
LIBEXP(report4index_type, 0)
LIBEXP(report4init, 4)
LIBEXP(report4margins, 24)
LIBEXP(report4numGroups, 4)
LIBEXP(report4numStyles, 4)
LIBEXP(report4off_write, 0)
LIBEXP(report4output, 12)
LIBEXP(report4pageFree, 4)
LIBEXP(report4pageHeaderFooter, 4)
LIBEXP(report4pageInit, 4)
LIBEXP(report4pageMarginsGet, 20)
LIBEXP(report4pageObjFirst, 4)
LIBEXP(report4pageObjNext, 4)
LIBEXP(report4pageSize, 16)
LIBEXP(report4pageSizeGet, 12)
LIBEXP(report4parent, 8)
LIBEXP(report4printerDC, 8)
LIBEXP(report4printerSelect, 4)
LIBEXP(report4querySet, 8)
LIBEXP(report4retrieve2, 24)
LIBEXP(report4retrieve, 16)
LIBEXP(report4save, 12)
LIBEXP(report4separator, 8)
LIBEXP(report4sortSet, 8)
LIBEXP(report4styleFirst, 4)
LIBEXP(report4styleLast, 4)
LIBEXP(report4styleNext, 8)
LIBEXP(report4styleSelect, 8)
LIBEXP(report4styleSelected, 4)
LIBEXP(report4styleSheetLoad, 12)
LIBEXP(report4styleSheetSave, 8)
LIBEXP(report4titlePage, 8)
LIBEXP(report4titleSummary, 4)
LIBEXP(report4toScreen, 8)
LIBEXP(sort4free, 4)
LIBEXP(sort4get, 16)
LIBEXP(sort4getInit, 4)
LIBEXP(sort4getInitFree, 8)
LIBEXP(sort4getMemInit, 4)
LIBEXP(sort4init, 16)
LIBEXP(sort4initAlloc, 4)
LIBEXP(sort4initFree, 20)
LIBEXP(sort4initSet, 16)
LIBEXP(sort4put, 16)
LIBEXP(sort4spoolsInit, 8)
LIBEXP(style4color, 8)
LIBEXP(style4create, 20)
LIBEXP(style4delete, 8)
LIBEXP(style4free, 8)
LIBEXP(style4index, 8)
LIBEXP(style4lookup, 8)
LIBEXP(t4alias, 4)
LIBEXP(t4close, 4)
LIBEXP(t4exprLow, 4)
LIBEXP(t4filterLow, 4)
LIBEXP(t4getExprSource, 4)
LIBEXP(t4openLow, 16)
LIBEXP(t4unique, 4)
LIBEXP(t4uniqueModify, 8)
LIBEXP(t4uniqueSet, 8)
LIBEXP(total4addCondition, 12)
LIBEXP(total4create, 20)
LIBEXP(total4free, 4)
LIBEXP(total4lookup, 8)
LIBEXP(u4allocAgainDefault, 16)
LIBEXP(u4allocDefault, 4)
LIBEXP(u4allocErDefault, 8)
LIBEXP(u4allocFixedDefault, 8)
LIBEXP(u4allocFreeDefault, 8)
LIBEXP(u4delayHundredth, 4)
LIBEXP(u4freeDefault, 4)
LIBEXP(u4freeFixedDefault, 4)
LIBEXP(u4nameChar, 4)
LIBEXP(u4nameCurrent, 12)
LIBEXP(u4nameExt, 16)
LIBEXP(u4namePiece, 20)
LIBEXP(u4ncpy, 12)
LIBEXP(u4remove, 4)
LIBEXP(u4switch, 0)
LIBEXP(u4yymmdd, 4)
LIBEXP(x4reverseShort, 4)

DOUBLE MathRound(DOUBLE number)
{
	DOUBLE floorVal = MathFloor(number);
	return floorVal + 0.5 > number ? floorVal : MathCeil(number);
}

struct Aligned {
	Aligned* last;
	VOID* block;
	DWORD data[1];
} * alignedList;

VOID* AlignedAlloc(size_t size)
{
	Aligned* entry = (Aligned*)MemoryAlloc(sizeof(Aligned) + size + 12);
	entry->last = alignedList;
	entry->block = (VOID*)(DWORD(entry->data + 4) & 0xFFFFFFF0);
	alignedList = entry;
	
	return entry->block;
}

VOID AlignedFree(VOID* block)
{
	Aligned** list = &alignedList;
	Aligned* entry = alignedList;
	while (entry)
	{
		if (entry->block == block)
		{
			*list = entry->last;
			MemoryFree(entry);
			break;
		}

		list = &entry->last;
		entry = entry->last;
	}
}

VOID LoadKernel32()
{
	HMODULE hLib = GetModuleHandle("KERNEL32.dll");
	if (hLib)
	{
		CreateActCtxC = (CREATEACTCTXA)GetProcAddress(hLib, "CreateActCtxA");
		ReleaseActCtxC = (RELEASEACTCTX)GetProcAddress(hLib, "ReleaseActCtx");
		ActivateActCtxC = (ACTIVATEACTCTX)GetProcAddress(hLib, "ActivateActCtx");
		DeactivateActCtxC = (DEACTIVATEACTCTX)GetProcAddress(hLib, "DeactivateActCtx");
	}
}

VOID LoadShcore()
{
	HMODULE hLib = LoadLibrary("SHCORE.dll");
	if (hLib)
		SetProcessDpiAwarenessC = (SETPROCESSDPIAWARENESS)GetProcAddress(hLib, "SetProcessDpiAwareness");
}
