#pragma once

namespace TurnArt
{
	struct Piece {
		LONG width;
		LONG height;
		COLORREF* pixels;
		BYTE* alpha;
	};

	extern Piece crystal;

	BOOL Load();
	Piece Resample(const Piece* src, LONG width, LONG height);
	Piece Facet(const Piece* src, LONG width, LONG height, COLORREF rim);
	VOID Free(Piece* piece);
}
