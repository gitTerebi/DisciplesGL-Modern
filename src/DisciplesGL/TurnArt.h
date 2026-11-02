#pragma once

#define FONT_GLYPHS 256

namespace TurnArt
{
	struct Piece {
		LONG width;
		LONG height;
		COLORREF* pixels;
		BYTE* alpha;
	};

	struct Glyph {
		LONG width;
		LONG height;
		LONG advance;
		LONG spacing;
		const BYTE* rows;
	};

	struct Font {
		BYTE* data;
		Glyph glyphs[FONT_GLYPHS];
		LONG inkTop;
		LONG inkBottom;
	};

	extern Piece crystal;
	extern Font normal;
	extern Font medium;

	BOOL Load();
	BOOL HasFont(const Font* font);
	LONG TextWidth(const Font* font, const CHAR* text, BOOL tight);
	VOID DrawText(const Font* font, const CHAR* text, BOOL tight, BYTE* mask, LONG width, LONG height);
	Piece Resample(const Piece* src, LONG width, LONG height);
	Piece Facet(const Piece* src, LONG width, LONG height, COLORREF rim);
	VOID Free(Piece* piece);
}
