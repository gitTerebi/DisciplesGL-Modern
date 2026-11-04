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

#include "StdAfx.h"
#include "Mmsystem.h"
#include "stdio.h"
#include "Config.h"
#include "Hooks.h"
#include "ImageReplace.h"
#include "TurnArt.h"
#include "TurnOrder.h"
#include "Twin.h"

#define f(a) (a + baseOffset)

namespace TurnOrder
{
	#define TURN_ORDER_SLOTS 13
	// Discipl2.exe v3.01b 0x00646C69: the battle engine draws lower layer keys in front, units use 20 - 130, the big face 10
	#define TURN_ORDER_LAYER 11
	#define TURN_ORDER_GAP 4
	#define TURN_ORDER_FACE_WIDTH 40
	#define TURN_ORDER_FACE_HEIGHT 48
	#define TURN_ORDER_BIG_FACE_WIDTH 48
	#define TURN_ORDER_BIG_FACE_HEIGHT 58
	#define TURN_ORDER_SLOT_WIDTH (TURN_ORDER_FACE_WIDTH + 4)
	#define TURN_ORDER_BIG_EXTRA (TURN_ORDER_BIG_FACE_WIDTH - TURN_ORDER_FACE_WIDTH)
	#define TURN_ORDER_TOP 6
	#define TURN_ORDER_BAND 12
	#define TURN_ORDER_BAND_SHADE 156
	#define TURN_ORDER_MARKER 26
	#define TURN_ORDER_CRYSTAL_WIDTH 22
	#define TURN_ORDER_CRYSTAL_HEIGHT 30
	#define TURN_ORDER_START_MARKER 1
	#define TURN_ORDER_NEXT_MARKER 2
	#define TURN_ORDER_GEM_WIDTH 12
	#define TURN_ORDER_GEM_HEIGHT 16
	#define TURN_ORDER_INSET 2
	#define TURN_ORDER_LIFT 0
	#define TURN_ORDER_BOTTOM 1
	#define TURN_ORDER_PANEL 175
	#define TURN_ORDER_SLIDE 300
	#define TURN_ORDER_CARDS 32
	#define TURN_ORDER_UNITS 22
	// Discipl2.exe v3.01b 0x0053A2A0: CImage2Outline, which leaves its middle transparent, builds its surface with hint 0
	#define TURN_ORDER_KEYED 0
	#define STATUS_DEAD 1
	#define STATUS_LOWER_INITIATIVE 13
	// Discipl2.exe v2.02b 0x0082EBF8: the empty CMidgardID the turn order is cleared with holds 0x003F0000 at runtime
	#define EMPTY_ID 0x003F0000
	#define TURN_LOOK_ENEMY 1
	#define TURN_LOOK_CURRENT 2

	struct {
		DWORD order;
		DWORD engine;
		DWORD area;
		DWORD objectMap;
		DWORD unitInfo;
		DWORD hp;
		DWORD round;
		DWORD hidden;
		DWORD retreated;
		DWORD unsummoned;
	} turnOrderOffsets;

	struct TurnStrip {
		DWORD* imageVtbl;
		DWORD* textureVtbl;
		DWORD surface[6];
		POINT size;
		BYTE dirty;
	};

	struct TurnCard {
		DWORD unitId;
		DWORD* face;
		BYTE* facePixels;
		BYTE* pixels;
		LONG hp;
		LONG hpMax;
		LONG shownHp;
		LONG shownHpMax;
		DWORD look;
		POINT size;
		POINT faceSize;
		LONG faceFrame;
		BOOL faceFlipped;
		BOOL attacker;
		BOOL current;
		BOOL leaving;
		BOOL next;
		BOOL divider;
		LONG from;
		LONG to;
		DWORD start;
	};

	DWORD sub_BattleViewerUpdate, sub_BattleViewerAttack, sub_BattleViewerEnd;
	DWORD sub_UnitStatus, sub_UnitAttacker, sub_BaseUnitImpl, sub_UnitFace, sub_LayerSet, sub_LayerRemove, sub_ImageSurface, sub_UnitAttack, sub_UnitHpMax;
	DWORD sub_TurnStripDestructor, sub_FormattedText;
	DWORD turnTextPtr[2];
	DWORD* turnText;
	DWORD turnStripImageVtbl[12], turnStripTextureVtbl[4];
	DWORD* turnViewer;
	DWORD turnViewerVtbl;
	BYTE turnOwnAttacker;
	TurnStrip* turnStrip;
	TurnCard turnCards[TURN_ORDER_CARDS];
	DWORD turnCardCount;
	LONG turnStep;
	LONG turnRowHeight;
	BOOL turnArtReady;
	TurnArt::Piece turnGems[2];
	TurnArt::Piece turnCrystal;

	static const COLORREF turnIron = RGB(104, 108, 96);
	static const COLORREF turnOutline = RGB(18, 16, 12);
	static const COLORREF turnGold = RGB(232, 200, 64);
	static const COLORREF turnTeams[] = { RGB(110, 230, 110), RGB(240, 90, 70) };
	// Interf.ff -IMAGES.OPT: DLG_BATTLE_A.PNG sits at 0,32 of DLG_BATTLE_A_LUNITGROUP, TurnArt's crystal 73,22 at 73,54
	static const CHAR turnArtFrame[] = "DLG_BATTLE_A_LUNITGROUP";
	static const RECT turnArtCrystal = { 73, 54, 90, 77 };

	VOID FreeTurnCard(TurnCard* card)
	{
		if (card->face)
			((VOID(__thiscall*)(DWORD*, BYTE))((DWORD*)card->face[0])[0])(card->face, 1);

		if (card->facePixels)
			MemoryFree(card->facePixels);

		if (card->pixels)
			MemoryFree(card->pixels);
	}

	VOID FreeTurnCards()
	{
		for (DWORD i = 0; i < turnCardCount; ++i)
			FreeTurnCard(&turnCards[i]);

		turnCardCount = 0;
	}

	LONG TurnCardX(const TurnCard* card, DWORD now)
	{
		DWORD elapsed = now - card->start;
		if (elapsed >= TURN_ORDER_SLIDE)
			return card->to;

		FLOAT t = (FLOAT)elapsed / TURN_ORDER_SLIDE;
		t = t * t * (3.0f - 2.0f * t);
		return card->from + (LONG)((card->to - card->from) * t);
	}

	VOID __fastcall TurnStripDestructor(TurnStrip* obj, DWORD /*edx*/, BYTE flags)
	{
		if (turnStrip == obj)
			turnStrip = NULL;

		((VOID(__thiscall*)(TurnStrip*, BYTE))sub_TurnStripDestructor)(obj, flags & ~1);
		if (flags & 1)
			MemoryFree(obj);
	}

	DWORD DimPixel(DWORD pix, const DWORD* masks)
	{
		DWORD res = pix & masks[3];
		for (DWORD i = 0; i < 3; ++i)
			res |= ((pix & masks[i]) >> 1) & masks[i];

		return res;
	}

	DWORD BlendTurnPixel(DWORD dst, DWORD src, LONG alpha, const DWORD* masks)
	{
		DWORD res = dst & masks[3];
		for (DWORD i = 0; i < 3; ++i)
		{
			LONGLONG d = dst & masks[i];
			LONGLONG s = src & masks[i];
			res |= (DWORD)(d + (s - d) * alpha / 256) & masks[i];
		}

		return res;
	}

	DWORD GetTurnPixel(const BYTE* src, DWORD bpp)
	{
		return bpp == sizeof(DWORD) ? *(DWORD*)src : *(WORD*)src;
	}

	VOID SetTurnPixel(BYTE* dst, DWORD bpp, DWORD pix)
	{
		if (bpp == sizeof(DWORD))
			*(DWORD*)dst = pix;
		else
			*(WORD*)dst = (WORD)pix;
	}

	VOID FillTurnRect(BYTE* buffer, LONG pitch, LONG left, LONG top, LONG right, LONG bottom, DWORD pix, DWORD bpp)
	{
		for (LONG y = top; y <= bottom; ++y)
			for (LONG x = left; x <= right; ++x)
				SetTurnPixel(buffer + (y * pitch + x) * bpp, bpp, pix);
	}

	COLORREF MixColor(COLORREF from, COLORREF to, LONG part, LONG whole)
	{
		return RGB(
			GetRValue(from) + ((LONG)GetRValue(to) - GetRValue(from)) * part / whole,
			GetGValue(from) + ((LONG)GetGValue(to) - GetGValue(from)) * part / whole,
			GetBValue(from) + ((LONG)GetBValue(to) - GetBValue(from)) * part / whole);
	}

	DWORD TurnColor(COLORREF color, BOOL isTrueColor, const DWORD* masks)
	{
		if (isTrueColor)
			return Hooks::Convert565toRGB(((GetRValue(color) >> 3) << 11) | ((GetGValue(color) >> 2) << 5) | (GetBValue(color) >> 3));

		BYTE channels[] = { GetRValue(color), GetGValue(color), GetBValue(color) };
		DWORD res = 0;
		for (DWORD i = 0; i < 3; ++i)
		{
			DWORD shift = 0, bits = 0;
			while (!(masks[i] >> shift & 1))
				++shift;
			while (masks[i] >> (shift + bits) & 1)
				++bits;

			res |= (DWORD)(channels[i] >> (8 - bits)) << shift;
		}

		return res;
	}

	LONG TurnNoise(LONG x, LONG y)
	{
		DWORD h = (DWORD)x * 374761393 + (DWORD)y * 668265263;
		h = (h ^ (h >> 13)) * 1274126177;
		return (LONG)((h ^ (h >> 16)) & 0xFF) - 128;
	}

	COLORREF ShadeColor(COLORREF color, LONG percent)
	{
		percent = max(0, percent);
		return RGB(
			min(255, GetRValue(color) * percent / 100),
			min(255, GetGValue(color) * percent / 100),
			min(255, GetBValue(color) * percent / 100));
	}

	BOOL TurnGem(LONG dx, LONG dy, LONG radius, COLORREF metal, COLORREF light, COLORREF dark, COLORREF* color)
	{
		LONG dist = abs(dx) + abs(dy);
		if (dist > radius)
			return FALSE;

		if (dist == radius)
			*color = turnOutline;
		else if (dist == radius - 1)
			*color = ShadeColor(metal, dx + dy < 0 ? 150 : 55);
		else if (dx == -1 && dy == -1)
			*color = MixColor(light, RGB(255, 255, 255), 2, 3);
		else
			*color = MixColor(light, dark, dx + dy + radius - 2, (radius - 2) * 2);

		return TRUE;
	}

	VOID DrawTurnGem(BYTE* buffer, LONG pitch, LONG cx, LONG cy, LONG radius, COLORREF light, COLORREF dark, BOOL isTrueColor, DWORD bpp, const DWORD* masks)
	{
		for (LONG dy = -radius; dy <= radius; ++dy)
		{
			for (LONG dx = -radius; dx <= radius; ++dx)
			{
				COLORREF color;
				if (cy + dy >= 0 && TurnGem(dx, dy, radius, turnIron, light, dark, &color))
					SetTurnPixel(buffer + ((cy + dy) * pitch + cx + dx) * bpp, bpp, TurnColor(color, isTrueColor, masks));
			}
		}
	}

	VOID PasteTurnPiece(BYTE* buffer, LONG pitch, LONG left, LONG top, const TurnArt::Piece* piece, BOOL isTrueColor, DWORD bpp, const DWORD* masks)
	{
		for (LONG y = 0; y < piece->height; ++y)
			for (LONG x = 0; x < piece->width; ++x)
				if (piece->alpha[y * piece->width + x])
					SetTurnPixel(buffer + ((top + y) * pitch + left + x) * bpp, bpp, TurnColor(piece->pixels[y * piece->width + x], isTrueColor, masks));
	}

	TurnArt::Piece TintTurnPiece(const TurnArt::Piece* src, COLORREF team)
	{
		TurnArt::Piece res = TurnArt::Resample(src, src->width, src->height);
		for (LONG i = 0; i < res.width * res.height; ++i)
		{
			COLORREF color = res.pixels[i];
			LONG sum = GetRValue(color) + GetGValue(color) + GetBValue(color);
			if (res.alpha[i] && sum > 250 && abs((LONG)GetRValue(color) - GetBValue(color)) < 60)
				res.pixels[i] = RGB(
					min(255, GetRValue(team) * sum * 13 / 7650),
					min(255, GetGValue(team) * sum * 13 / 7650),
					min(255, GetBValue(team) * sum * 13 / 7650));
		}

		return res;
	}

	VOID PrepareTurnArt()
	{
		if (turnArtReady)
			return;

		turnArtReady = TRUE;
		if (!TurnArt::Load())
			return;

		TurnArt::Piece gem = TurnArt::Facet(&TurnArt::crystal, TURN_ORDER_GEM_WIDTH, TURN_ORDER_GEM_HEIGHT, turnOutline);
		turnGems[0] = TintTurnPiece(&gem, turnTeams[0]);
		turnGems[1] = TintTurnPiece(&gem, turnTeams[1]);
		TurnArt::Free(&gem);
		turnCrystal = TurnArt::Facet(&TurnArt::crystal, TURN_ORDER_CRYSTAL_WIDTH, TURN_ORDER_CRYSTAL_HEIGHT, turnOutline);
	}

	VOID ComposeTurnCard(TurnCard* card, DWORD look, DWORD key, BOOL isTrueColor, DWORD bpp, const DWORD* masks)
	{
		LONG faceWidth = card->faceSize.x;
		LONG faceHeight = card->faceSize.y;
		LONG slot = faceWidth + 4;
		LONG width = slot + 2;
		LONG height = TURN_ORDER_TOP + faceHeight + 4;
		LONG recessBottom = height - 1;
		if (card->pixels && (card->size.x != width || card->size.y != height))
		{
			MemoryFree(card->pixels);
			card->pixels = NULL;
		}

		if (!card->pixels)
			card->pixels = (BYTE*)MemoryAlloc(width * height * bpp);

		card->size = { width, height };
		FillTurnRect(card->pixels, width, 0, 0, width - 1, height - 1, key, bpp);

		DWORD outline = TurnColor(turnOutline, isTrueColor, masks);
		DWORD edge = TurnColor(RGB(120, 116, 100), isTrueColor, masks);
		FillTurnRect(card->pixels, width, 1, TURN_ORDER_TOP, slot, recessBottom, outline, bpp);
		FillTurnRect(card->pixels, width, 2, recessBottom - 1, slot - 1, recessBottom - 1, edge, bpp);
		FillTurnRect(card->pixels, width, slot - 1, TURN_ORDER_TOP + 1, slot - 1, recessBottom - 1, edge, bpp);

		if (look & TURN_LOOK_CURRENT)
		{
			DWORD gold = TurnColor(turnGold, isTrueColor, masks);
			FillTurnRect(card->pixels, width, 1, TURN_ORDER_TOP, slot, TURN_ORDER_TOP + 1, gold, bpp);
			FillTurnRect(card->pixels, width, 1, recessBottom - 1, slot, recessBottom, gold, bpp);
			FillTurnRect(card->pixels, width, 1, TURN_ORDER_TOP, 2, recessBottom, gold, bpp);
			FillTurnRect(card->pixels, width, slot - 1, TURN_ORDER_TOP, slot, recessBottom, gold, bpp);
		}

		DWORD red = TurnColor(RGB(200, 0, 0), isTrueColor, masks);
		DWORD black = TurnColor(RGB(0, 0, 0), isTrueColor, masks);
		LONG missing = card->hpMax > 0 ? faceHeight * max(0, card->hpMax - card->hp) / card->hpMax : 0;
		for (LONG y = 0; y < faceHeight; ++y)
		{
			for (LONG x = 0; x < faceWidth; ++x)
			{
				DWORD pix = GetTurnPixel(card->facePixels + (y * faceWidth + x) * bpp, bpp);
				if (y >= faceHeight - missing)
					pix = BlendTurnPixel(pix, red, 141, masks);

				if (y >= faceHeight - TURN_ORDER_BAND)
					pix = BlendTurnPixel(pix, black, TURN_ORDER_BAND_SHADE, masks);

				SetTurnPixel(card->pixels + ((TURN_ORDER_TOP + 2 + y) * width + 3 + x) * bpp, bpp, pix);
			}
		}

		BOOL enemy = (look & TURN_LOOK_ENEMY) != 0;
		if (turnGems[enemy].pixels)
			PasteTurnPiece(card->pixels, width, 1 + (slot - TURN_ORDER_GEM_WIDTH) / 2, 0, &turnGems[enemy], isTrueColor, bpp, masks);
		else
			DrawTurnGem(card->pixels, width, 1 + slot / 2, TURN_ORDER_GEM_HEIGHT / 2 - 1, TURN_ORDER_GEM_WIDTH / 2,
				enemy ? RGB(255, 140, 110) : RGB(150, 240, 140), enemy ? RGB(100, 10, 6) : RGB(16, 84, 24), isTrueColor, bpp, masks);

		card->look = look;
		card->shownHp = card->hp;
		card->shownHpMax = card->hpMax;
	}

	VOID ComposeTurnMarker(TurnCard* card, DWORD key, BOOL isTrueColor, DWORD bpp, const DWORD* masks)
	{
		LONG width = TURN_ORDER_CRYSTAL_WIDTH;
		LONG height = turnCrystal.pixels ? TURN_ORDER_CRYSTAL_HEIGHT : width;
		if (!card->pixels)
			card->pixels = (BYTE*)MemoryAlloc(width * height * bpp);

		card->size = { width, height };
		FillTurnRect(card->pixels, width, 0, 0, width - 1, height - 1, key, bpp);
		if (turnCrystal.pixels)
			PasteTurnPiece(card->pixels, width, 0, 0, &turnCrystal, isTrueColor, bpp, masks);
		else
			DrawTurnGem(card->pixels, width, width / 2, height / 2, width / 2, RGB(200, 240, 236), RGB(40, 96, 100), isTrueColor, bpp, masks);

		card->shownHp = card->hp;
	}

	VOID DrawTurnText(BYTE* data, const CHAR* font, const CHAR* align, BOOL outline, const CHAR* value, COLORREF color, BOOL dim, LONG left, LONG top, LONG right, LONG bottom)
	{
		if (!turnText)
		{
			((DWORD*(__stdcall*)(DWORD*))sub_FormattedText)(turnTextPtr);
			turnText = (DWORD*)turnTextPtr[1];
			if (!turnText)
				return;
		}

		if (dim)
			color = RGB(GetRValue(color) / 2, GetGValue(color) / 2, GetBValue(color) / 2);

		CHAR text[64];
		StrPrint(text, "\\f%s;\\hC;\\v%s;%s\\c%03d;%03d;%03d;%s", font, align, outline ? "\\o000;000;000;" : "", GetRValue(color), GetGValue(color), GetBValue(color), value);

		RECT area = { left, top, right, bottom };
		POINT size;
		((POINT*(__thiscall*)(DWORD*, POINT*, BYTE*, const CHAR*, RECT*, DWORD))((DWORD*)*turnText)[2])(turnText, &size, data, text, &area, 1);
	}

	VOID RenderTurnFace(TurnCard* card, BYTE* data, LONG faceWidth, LONG faceHeight, DWORD bpp, const DWORD* masks)
	{
		if (card->facePixels)
			MemoryFree(card->facePixels);

		POINT size;
		((POINT*(__thiscall*)(DWORD*, POINT*))((DWORD*)card->face[0])[1])(card->face, &size);

		BYTE* pixels = (BYTE*)MemoryAlloc(size.x * size.y * bpp);

		// Discipl2.exe v3.01b 0x005B149D: SurfaceDecompressData is 1072 bytes, memory at +0x4, area at +0x8, pitch at +0x18
		BYTE faceData[1072];
		MemoryCopy(faceData, data, sizeof(faceData));
		*(BYTE**)(faceData + 0x4) = pixels;
		*(RECT*)(faceData + 0x8) = { 0, 0, size.x, size.y };
		*(LONG*)(faceData + 0x18) = size.x << 1;
		((VOID(__stdcall*)(DWORD*, BYTE*))((DWORD*)card->face[1])[1])(card->face + 1, faceData);
		ImageReplace::TakeFace(pixels, size.x, size.y, &card->faceFlipped);
		card->faceFrame = -1;

		card->facePixels = (BYTE*)MemoryAlloc(faceWidth * faceHeight * bpp);
		for (LONG y = 0; y < faceHeight; ++y)
		{
			for (LONG x = 0; x < faceWidth; ++x)
			{
				ULONGLONG sums[4] = {};
				for (LONG sy = 0; sy < 4; ++sy)
				{
					LONG py = ((y * 4 + sy) * 2 + 1) * size.y / (faceHeight * 8);
					for (LONG sx = 0; sx < 4; ++sx)
					{
						LONG px = ((x * 4 + sx) * 2 + 1) * size.x / (faceWidth * 8);
						DWORD pix = GetTurnPixel(pixels + (py * size.x + px) * bpp, bpp);
						for (DWORD i = 0; i < 4; ++i)
							sums[i] += pix & masks[i];
					}
				}

				DWORD pix = 0;
				for (DWORD i = 0; i < 4; ++i)
					pix |= (DWORD)(sums[i] / 16) & masks[i];

				SetTurnPixel(card->facePixels + (y * faceWidth + x) * bpp, bpp, pix);
			}
		}

		MemoryFree(pixels);
		card->faceSize = { faceWidth, faceHeight };
	}

	VOID __stdcall TurnStripDraw(DWORD* texture, BYTE* data)
	{
		TurnStrip* obj = (TurnStrip*)(texture - 1);

		BOOL isTrueColor = config.bpp32Hooked;
		DWORD bpp = isTrueColor ? sizeof(DWORD) : sizeof(WORD);

		static const DWORD masks32[] = { 0xFF, 0xFF00, 0xFF0000, 0xFF000000 };
		static const DWORD masks565[] = { 0xF800, 0x07E0, 0x001F, 0 };
		DWORD masks16[] = { *(DWORD*)(data + 0x420), *(DWORD*)(data + 0x424), *(DWORD*)(data + 0x428), 0 };
		const DWORD* masks = isTrueColor ? masks32 : (masks16[0] && masks16[1] && masks16[2] ? masks16 : masks565);

		// Discipl2.exe v3.01b 0x00560E5B: the area is this texture's place inside a shared surface
		RECT* area = (RECT*)(data + 0x8);
		LONG pitch = (*(LONG*)(data + 0x18) >> 1) * bpp;
		BYTE* surface = *(BYTE**)(data + 0x4) + area->top * pitch + area->left * bpp;

		PrepareTurnArt();

		DWORD key = isTrueColor ? Hooks::Convert565toRGB(COLORKEY_CHECK_16) : COLORKEY_CHECK_16;
		LONG width = obj->size.x;
		LONG height = obj->size.y;
		for (LONG y = 0; y < height; ++y)
			FillTurnRect(surface + y * pitch, 0, 0, 0, width - 1, 0, key, bpp);

		if (isTrueColor)
			Twin::OnWrite(surface, pitch / (LONG)bpp, width, height);

		DWORD now = timeGetTime();
		BOOL moving = FALSE;
		for (DWORD i = 0; i < turnCardCount; ++i)
		{
			TurnCard* card = &turnCards[i];
			if (card->divider)
			{
				if (!card->pixels || card->shownHp != card->hp)
					ComposeTurnMarker(card, key, isTrueColor, bpp, masks);
			}
			else
			{
				LONG faceWidth = card->current ? TURN_ORDER_BIG_FACE_WIDTH : TURN_ORDER_FACE_WIDTH;
				LONG faceHeight = card->current ? TURN_ORDER_BIG_FACE_HEIGHT : TURN_ORDER_FACE_HEIGHT;
				if (!card->facePixels || card->faceSize.x != faceWidth)
					RenderTurnFace(card, data, faceWidth, faceHeight, bpp, masks);

				DWORD look = (card->attacker == turnOwnAttacker ? 0 : TURN_LOOK_ENEMY) | (card->current ? TURN_LOOK_CURRENT : 0);
				if (!card->pixels || card->look != look || card->shownHp != card->hp || card->shownHpMax != card->hpMax || card->size.y != TURN_ORDER_TOP + faceHeight + 4)
					ComposeTurnCard(card, look, key, isTrueColor, bpp, masks);
			}

			if (now - card->start < TURN_ORDER_SLIDE)
				moving = TRUE;

			LONG cardWidth = card->size.x;
			LONG left = TURN_ORDER_INSET + TurnCardX(card, now) + (card->divider ? (TURN_ORDER_MARKER - TURN_ORDER_GAP - cardWidth) / 2 : -1);
			LONG clip = TURN_ORDER_INSET - 1 + (card->divider ? 0 : TURN_ORDER_MARKER);
			LONG top = TURN_ORDER_LIFT + (turnRowHeight - card->size.y) / 2;
			for (LONG y = 0; y < card->size.y && top + y < height; ++y)
			{
				BYTE* row = surface + (top + y) * pitch;
				for (LONG x = 0; x < cardWidth; ++x)
				{
					LONG dx = left + x;
					if (dx < clip || dx > width - TURN_ORDER_INSET)
						continue;

					DWORD pix = GetTurnPixel(card->pixels + (y * cardWidth + x) * bpp, bpp);
					if (pix == key)
						continue;

					if (card->next)
						pix = DimPixel(pix, masks);

					SetTurnPixel(row + dx * bpp, bpp, pix);
				}
			}

			LONG textLeft = card->divider ? left : left + 1;
			LONG textRight = card->divider ? left + cardWidth : left + cardWidth - 1;
			if (textLeft >= clip && textRight <= width - TURN_ORDER_INSET + 1 && top >= 0 && top + card->size.y <= height)
			{
				CHAR value[32];
				if (card->divider)
				{
					StrPrint(value, "%d", card->hp);
					DrawTurnText(data, "Medium", "C", TRUE, value, RGB(250, 244, 224), card->next, textLeft + 1, max(top - 1, 0), textRight + 1, top + card->size.y - 1);
				}
				else
				{
					StrPrint(value, "%d/%d", card->hp, card->hpMax);
					LONG bandBottom = top + TURN_ORDER_TOP + 2 + card->faceSize.y;
					LONG bandMiddle = bandBottom - TURN_ORDER_BAND / 2;
					LONG half = min(13, min(bandMiddle, height - bandMiddle));
					DrawTurnText(data, "Small", "C", FALSE, value, RGB(240, 232, 208), card->next, max(textLeft - 2, 0), bandMiddle - half, min(textRight + 2, width), bandMiddle + half);
				}
			}
		}

		// Discipl2.exe v3.01b 0x006775E0 sets the staging colour key from +0x1C after the draw, as 0x00560E31 stores it
		*(DWORD*)(data + 0x1C) = COLORKEY_CHECK_16;
		obj->dirty = moving;
	}

	TurnStrip* CreateTurnStrip(LONG width, LONG height)
	{
		TurnStrip* obj = (TurnStrip*)MemoryAlloc(sizeof(TurnStrip));
		((TurnStrip*(__thiscall*)(TurnStrip*, DWORD, DWORD, DWORD, DWORD))sub_ImageSurface)(obj, width, height, TURN_ORDER_KEYED, 0xFF);

		if (!sub_TurnStripDestructor)
		{
			MemoryCopy(turnStripImageVtbl, obj->imageVtbl, sizeof(turnStripImageVtbl));
			MemoryCopy(turnStripTextureVtbl, obj->textureVtbl, sizeof(turnStripTextureVtbl));
			sub_TurnStripDestructor = turnStripImageVtbl[0];
			turnStripImageVtbl[0] = (DWORD)TurnStripDestructor;
			turnStripTextureVtbl[1] = (DWORD)TurnStripDraw;
		}

		obj->imageVtbl = turnStripImageVtbl;
		obj->textureVtbl = turnStripTextureVtbl;
		return obj;
	}

	BOOL TurnUnitStatus(BYTE* msg, DWORD* unitId, DWORD status)
	{
		return ((BYTE(__thiscall*)(BYTE*, DWORD*, DWORD))sub_UnitStatus)(msg, unitId, status);
	}

	LONG TurnUnitHp(BYTE* msg, DWORD unitId)
	{
		for (DWORD i = 0; i < TURN_ORDER_UNITS; ++i)
		{
			BYTE* info = msg + i * turnOrderOffsets.unitInfo;
			if (*(DWORD*)info == unitId)
				return *(WORD*)(info + turnOrderOffsets.hp);
		}

		return 0;
	}

	DWORD* LoadTurnFace(BYTE* msg, DWORD objectMap, DWORD* unitId)
	{
		DWORD implId;
		((DWORD*(__stdcall*)(DWORD*, DWORD, DWORD*, BOOL))sub_BaseUnitImpl)(&implId, objectMap, unitId, FALSE);
		BYTE attacker = ((BYTE(__thiscall*)(BYTE*, DWORD*))sub_UnitAttacker)(msg, unitId);

		// Discipl2.exe v3.01b 0x00647EF8 never reads its this pointer
		return ((DWORD*(__thiscall*)(DWORD, DWORD*, DWORD, DWORD))sub_UnitFace)(NULL, &implId, attacker, FALSE);
	}

	VOID ShowTurnOrder(DWORD* viewer, BYTE* msg)
	{
		BYTE* data = (BYTE*)viewer[1];
		DWORD engine = *(DWORD*)(data + turnOrderOffsets.engine);

		if (!config.turnOrder.enabled)
		{
			if (turnStrip)
				((VOID(__thiscall*)(DWORD, DWORD))sub_LayerRemove)(engine, TURN_ORDER_LAYER);

			FreeTurnCards();
			return;
		}

		DWORD units[TURN_ORDER_SLOTS];
		DWORD count = 0;
		DWORD* turn = (DWORD*)(msg + turnOrderOffsets.order);
		for (DWORD i = 0; i < TURN_ORDER_SLOTS && turn[i * 2] && turn[i * 2] != EMPTY_ID; ++i)
			if (!((BYTE(__thiscall*)(BYTE*, DWORD*, DWORD))sub_UnitStatus)(msg, &turn[i * 2], STATUS_DEAD))
				units[count++] = turn[i * 2];

		DWORD objectMap = *(DWORD*)(data + turnOrderOffsets.objectMap);
		DWORD now = timeGetTime();

		struct {
			DWORD unitId;
			LONG initiative;
		} estimate[TURN_ORDER_UNITS];
		DWORD estimateCount = 0;
		for (DWORD i = 0; i < TURN_ORDER_UNITS; ++i)
		{
			DWORD* id = (DWORD*)(msg + i * turnOrderOffsets.unitInfo);
			if (!*id || *id == EMPTY_ID || TurnUnitStatus(msg, id, turnOrderOffsets.hidden) || TurnUnitStatus(msg, id, STATUS_DEAD) || TurnUnitStatus(msg, id, turnOrderOffsets.retreated) || TurnUnitStatus(msg, id, turnOrderOffsets.unsummoned))
				continue;

			// Discipl2.exe v3.01b 0x0062457C: round initiative is the attack's plus a random roll, halved by lower initiative
			DWORD* attack = ((DWORD*(__stdcall*)(DWORD, DWORD*, BOOL))sub_UnitAttack)(objectMap, id, TRUE);
			if (!attack)
				continue;

			LONG initiative = ((LONG(__thiscall*)(DWORD*))((DWORD*)attack[0])[5])(attack);
			if (TurnUnitStatus(msg, id, STATUS_LOWER_INITIATIVE))
				initiative >>= 1;

			DWORD pos = estimateCount++;
			while (pos && estimate[pos - 1].initiative < initiative)
			{
				estimate[pos] = estimate[pos - 1];
				--pos;
			}

			estimate[pos].unitId = *id;
			estimate[pos].initiative = initiative;
		}

		TurnCard next[TURN_ORDER_CARDS];
		DWORD nextCount = 0;
		BOOL used[TURN_ORDER_CARDS] = {};
		LONG round = *(CHAR*)(msg + turnOrderOffsets.round);
		DWORD lead = count ? count + 1 : 0;
		DWORD total = lead + (estimateCount ? estimateCount + 1 : 0);
		LONG x = 0;
		for (DWORD i = 0; i < total; ++i)
		{
			BOOL isDivider = i == lead || count && !i;
			BOOL isNext = i > lead;
			BOOL isCurrent = count && i == 1;
			DWORD unitId = isDivider ? (i == lead ? TURN_ORDER_NEXT_MARKER : TURN_ORDER_START_MARKER) : (isNext ? estimate[i - lead - 1].unitId : units[i - 1]);
			LONG to = x;
			x += isDivider ? TURN_ORDER_MARKER : turnStep + (isCurrent ? TURN_ORDER_BIG_EXTRA : 0);

			DWORD j = 0;
			while (j < turnCardCount && (used[j] || turnCards[j].leaving || turnCards[j].divider != isDivider || turnCards[j].unitId != unitId || turnCards[j].next != isNext))
				++j;

			if (j == turnCardCount && !isNext && !isDivider)
			{
				j = 0;
				while (j < turnCardCount && (used[j] || turnCards[j].leaving || !turnCards[j].next || turnCards[j].unitId != unitId))
					++j;
			}

			TurnCard* card = &next[nextCount];
			if (j < turnCardCount)
			{
				used[j] = TRUE;
				*card = turnCards[j];
				card->from = TurnCardX(card, now);
			}
			else
			{
				MemoryZero(card, sizeof(TurnCard));
				card->unitId = unitId;
				card->divider = isDivider;
				if (!isDivider)
				{
					card->face = LoadTurnFace(msg, objectMap, &unitId);
					if (!card->face)
						continue;
				}

				card->from = isDivider ? to : to + turnStep;
			}

			if (isDivider)
				card->hp = unitId == TURN_ORDER_START_MARKER ? round : round + 1;
			else
			{
				card->attacker = ((BYTE(__thiscall*)(BYTE*, DWORD*))sub_UnitAttacker)(msg, &unitId);
				card->hp = TurnUnitHp(msg, unitId);
				card->hpMax = ((LONG(__stdcall*)(DWORD, DWORD*))sub_UnitHpMax)(objectMap, &unitId);
			}

			card->current = isCurrent;
			card->next = isNext;
			card->to = to;
			card->start = now;
			++nextCount;
		}

		for (DWORD j = 0; j < turnCardCount; ++j)
		{
			if (used[j])
				continue;

			TurnCard* card = &turnCards[j];
			if (!card->leaving && card->current)
			{
				card->from = TurnCardX(card, now);
				card->to = TURN_ORDER_MARKER - turnStep - TURN_ORDER_BIG_EXTRA;
				card->start = now;
				card->leaving = TRUE;
			}

			if (card->leaving && now - card->start < TURN_ORDER_SLIDE && nextCount < TURN_ORDER_CARDS)
				next[nextCount++] = *card;
			else
				FreeTurnCard(card);
		}

		MemoryCopy(turnCards, next, nextCount * sizeof(TurnCard));
		turnCardCount = nextCount;

		if (!turnCardCount)
			return;

		if (!turnStrip)
		{
			RECT* area = (RECT*)(data + turnOrderOffsets.area);
			LONG width = area->right - area->left;
			LONG frame = TURN_ORDER_INSET * 2 + TURN_ORDER_MARKER * 2 + TURN_ORDER_BIG_EXTRA - TURN_ORDER_GAP;
			LONG slots = min(TURN_ORDER_SLOTS, max(1, (width - TURN_ORDER_PANEL * 2 - frame) / turnStep));

			turnStrip = CreateTurnStrip(frame + slots * turnStep, TURN_ORDER_LIFT + turnRowHeight + TURN_ORDER_BOTTOM);
			POINT pos = { (width - turnStrip->size.x) / 2, 0 };
			((VOID(__thiscall*)(DWORD, TurnStrip*, POINT*, DWORD, BOOL))sub_LayerSet)(engine, turnStrip, &pos, TURN_ORDER_LAYER, FALSE);
		}

		turnStrip->dirty = TRUE;
	}

	VOID TurnViewer(DWORD* viewer)
	{
		HookUpdate();
		if (turnViewer != viewer)
		{
			turnViewer = viewer;
			turnOwnAttacker = TRUE;
			turnStrip = NULL;
			FreeTurnCards();
		}
	}

	VOID __fastcall BattleViewerUpdate(DWORD* viewer, DWORD /*edx*/, BYTE* msg, DWORD* unitId, DWORD actions, DWORD targets)
	{
		((VOID(__thiscall*)(DWORD*, BYTE*, DWORD*, DWORD, DWORD))sub_BattleViewerUpdate)(viewer, msg, unitId, actions, targets);

		TurnViewer(viewer);
		turnOwnAttacker = ((BYTE(__thiscall*)(BYTE*, DWORD*))sub_UnitAttacker)(msg, unitId);
		ShowTurnOrder(viewer, msg);
	}

	VOID __fastcall BattleViewerAttack(DWORD* viewer, DWORD /*edx*/, BYTE* msg, DWORD attackInfo, DWORD attackClass)
	{
		((VOID(__thiscall*)(DWORD*, BYTE*, DWORD, DWORD))sub_BattleViewerAttack)(viewer, msg, attackInfo, attackClass);

		TurnViewer(viewer);
		ShowTurnOrder(viewer, msg);
	}

	VOID __fastcall BattleViewerEnd(DWORD* viewer, DWORD /*edx*/, BYTE* msg, DWORD a3)
	{
		turnViewer = NULL;
		((VOID(__thiscall*)(DWORD*, BYTE*, DWORD))sub_BattleViewerEnd)(viewer, msg, a3);
	}

	VOID HookUpdate()
	{
		if (!turnViewerVtbl || sub_BattleViewerUpdate)
			return;

		HOOKER hooker = CreateHooker(GetModuleHandle(NULL));
		if (hooker)
		{
			if (ReadDWord(hooker, turnViewerVtbl + 4, &sub_BattleViewerUpdate))
				PatchPtr(hooker, turnViewerVtbl + 4, BattleViewerUpdate);

			ReleaseHooker(hooker);
		}
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		if (!(hookSpace->turn_order_attack && hookSpace->turn_order_hp_max && hookSpace->turn_order_text && ReadDWord(hooker, hookSpace->turn_order_viewer + 8, &sub_BattleViewerAttack) && ReadDWord(hooker, hookSpace->turn_order_viewer + 12, &sub_BattleViewerEnd)))
			return;

		config.turnOrder.hooked = TRUE;
		turnRowHeight = TURN_ORDER_TOP + TURN_ORDER_BIG_FACE_HEIGHT + 4;
		turnStep = TURN_ORDER_SLOT_WIDTH + TURN_ORDER_GAP;

		BOOL isV2 = hookSpace->version.high == 2;
		turnOrderOffsets.order = isV2 ? 0x4D0 : 0xE70;
		turnOrderOffsets.engine = isV2 ? 0x688 : 0x1028;
		turnOrderOffsets.area = isV2 ? 0x68C : 0x102C;
		turnOrderOffsets.objectMap = isV2 ? 0xB60 : 0x1500;
		turnOrderOffsets.unitInfo = isV2 ? 0x38 : 0xA8;
		// Discipl2.exe v2.02b 0x0061AEB2 and v3.01b 0x00622D99 read the battle HP as a word here
		turnOrderOffsets.hp = isV2 ? 0x28 : 0x2C;
		// Discipl2.exe v2.02b 0x00619FB9 and v3.01b 0x00621FC4 read the round counter byte 0xC0 past the turn order
		turnOrderOffsets.round = turnOrderOffsets.order + 0xC0;
		turnOrderOffsets.hidden = isV2 ? 0x1C : 0x1E;
		turnOrderOffsets.retreated = isV2 ? 0x1A : 0x1C;
		turnOrderOffsets.unsummoned = isV2 ? 0x1E : 0x20;

		sub_UnitStatus = f(hookSpace->turn_order_status);
		sub_UnitAttacker = f(hookSpace->turn_order_attacker);
		sub_BaseUnitImpl = f(hookSpace->turn_order_impl);
		sub_UnitFace = f(hookSpace->turn_order_face);
		sub_LayerSet = f(hookSpace->turn_order_layer_set);
		sub_LayerRemove = f(hookSpace->turn_order_layer_remove);
		sub_ImageSurface = f(hookSpace->turn_order_surface);
		sub_UnitAttack = f(hookSpace->turn_order_attack);
		sub_UnitHpMax = f(hookSpace->turn_order_hp_max);
		sub_FormattedText = f(hookSpace->turn_order_text);

		turnViewerVtbl = hookSpace->turn_order_viewer;
		PatchPtr(hooker, hookSpace->turn_order_viewer + 8, BattleViewerAttack);
		PatchPtr(hooker, hookSpace->turn_order_viewer + 12, BattleViewerEnd);
	}
}
