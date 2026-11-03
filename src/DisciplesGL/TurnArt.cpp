#include "StdAfx.h"
#include "PngLib.h"
#include "TurnArt.h"

#define ART_NAME "DLG_BATTLE_A.PNG"
#define ART_WIDTH 730
#define ART_HEIGHT 519
#define ART_RECORDS 1024
#define ART_RECORD_TAG 0x4352514D
#define ART_NAME_ENTRY 260
#define ART_SAMPLES 4

namespace TurnArt
{
	Piece crystal;

	struct Record {
		DWORD id;
		DWORD offset;
		DWORD size;
	};

	BOOL ReadAt(HANDLE file, DWORD offset, VOID* buffer, DWORD size)
	{
		DWORD read;
		return SetFilePointer(file, offset, NULL, FILE_BEGIN) == offset && ReadFile(file, buffer, size, &read, NULL) && read == size;
	}

	DWORD FindArtId(HANDLE file, const Record* records, DWORD count)
	{
		for (DWORD i = 0; i < count; ++i)
		{
			DWORD entries;
			if (records[i].size < sizeof(DWORD) || (records[i].size - sizeof(DWORD)) % ART_NAME_ENTRY
				|| !ReadAt(file, records[i].offset, &entries, sizeof(DWORD)) || sizeof(DWORD) + entries * ART_NAME_ENTRY != records[i].size)
				continue;

			DWORD id = 0;
			BYTE* names = (BYTE*)MemoryAlloc(records[i].size);
			if (ReadAt(file, records[i].offset, names, records[i].size))
			{
				for (DWORD j = 0; j < entries && !id; ++j)
				{
					CHAR* name = (CHAR*)names + sizeof(DWORD) + j * ART_NAME_ENTRY;
					name[ART_NAME_ENTRY - sizeof(DWORD) - 1] = '\0';
					if (!StrCompareInsensitive(name, ART_NAME))
						id = *(DWORD*)(name + ART_NAME_ENTRY - sizeof(DWORD));
				}
			}

			MemoryFree(names);
			if (id)
				return id;
		}

		return 0;
	}

	HANDLE OpenGameFile(const CHAR* name)
	{
		CHAR path[MAX_PATH];
		if (!GetModuleFileNameA(NULL, path, MAX_PATH))
			return INVALID_HANDLE_VALUE;

		CHAR* slash = StrLastChar(path, '\\');
		if (!slash || slash - path + StrLength(name) + 1 >= MAX_PATH)
			return INVALID_HANDLE_VALUE;

		StrCopy(slash + 1, name);
		return CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	BYTE* ReadArtPng(DWORD* size)
	{
		HANDLE file = OpenGameFile("Interf\\Interf.ff");
		if (file == INVALID_HANDLE_VALUE)
			return NULL;

		DWORD fileSize = GetFileSize(file, NULL);
		Record* records = (Record*)MemoryAlloc(ART_RECORDS * sizeof(Record));
		DWORD count = 0;
		DWORD header[7];
		for (DWORD offset = sizeof(header); count < ART_RECORDS && offset + sizeof(header) <= fileSize; offset += sizeof(header) + header[3])
		{
			if (!ReadAt(file, offset, header, sizeof(header)) || header[0] != ART_RECORD_TAG)
				break;

			records[count].id = header[2];
			records[count].offset = offset + sizeof(header);
			records[count].size = header[3];
			++count;
		}

		BYTE* data = NULL;
		DWORD id = FindArtId(file, records, count);
		for (DWORD i = 0; id && i < count; ++i)
		{
			if (records[i].id != id)
				continue;

			data = (BYTE*)MemoryAlloc(records[i].size);
			if (ReadAt(file, records[i].offset, data, records[i].size))
				*size = records[i].size;
			else
			{
				MemoryFree(data);
				data = NULL;
			}

			break;
		}

		MemoryFree(records);
		CloseHandle(file);
		return data;
	}

	DWORD BigEndian(const BYTE* data)
	{
		return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	}

	Piece Cut(const BYTE* indices, DWORD pitch, const COLORREF* palette, LONG left, LONG top, LONG width, LONG height)
	{
		Piece res = { width, height, (COLORREF*)MemoryAlloc(width * height * sizeof(COLORREF)), (BYTE*)MemoryAlloc(width * height) };
		for (LONG y = 0; y < height; ++y)
		{
			for (LONG x = 0; x < width; ++x)
			{
				COLORREF color = palette[indices[(top + y) * pitch + left + x]];
				res.pixels[y * width + x] = color;
				res.alpha[y * width + x] = !(GetRValue(color) > 200 && GetBValue(color) > 200 && GetGValue(color) < 60);
			}
		}

		return res;
	}

	BOOL Decode(BYTE* data, DWORD size)
	{
		if (size < 33 || BigEndian(data + 16) != ART_WIDTH || BigEndian(data + 20) != ART_HEIGHT || data[24] != 8 || data[25] != PNG_COLOR_TYPE_PALETTE || data[28])
			return FALSE;

		BOOL res = FALSE;
		Stream stream = { data, size, 0 };
		png_structp png_ptr = pnglib_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		png_infop info_ptr = png_ptr ? pnglib_create_info_struct(png_ptr) : NULL;
		if (info_ptr)
		{
			pnglib_set_read_fn(png_ptr, &stream, PngLib::ReadDataFromInputStream);
			pnglib_read_info(png_ptr, info_ptr);

			BYTE* indices = (BYTE*)MemoryAlloc(info_ptr->height * info_ptr->rowbytes);
			BYTE** rows = (BYTE**)MemoryAlloc(info_ptr->height * sizeof(BYTE*));
			for (DWORD i = 0; i < info_ptr->height; ++i)
				rows[i] = indices + i * info_ptr->rowbytes;

			pnglib_read_image(png_ptr, rows);
			MemoryFree(rows);

			COLORREF palette[256] = {};
			for (DWORD i = 0; i < (DWORD)info_ptr->num_palette && i < 256; ++i)
				palette[i] = RGB(info_ptr->palette[i].red, info_ptr->palette[i].green, info_ptr->palette[i].blue);

			crystal = Cut(indices, info_ptr->rowbytes, palette, 73, 22, 17, 23);
			for (LONG y = 0; y < crystal.height; ++y)
				for (LONG x = 0; x < crystal.width; ++x)
					if (fabsf(x + 0.5f - 8.5f) / 8.8f + fabsf(y + 0.5f - 11.5f) / 11.8f > 1.0f)
						crystal.alpha[y * crystal.width + x] = 0;
			MemoryFree(indices);
			res = TRUE;
		}

		if (png_ptr)
			pnglib_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : NULL, NULL);

		return res;
	}

	BOOL Load()
	{
		static BOOL tried, loaded;
		if (tried)
			return loaded;

		tried = TRUE;
		DWORD size = 0;
		BYTE* data = pnglib_create_read_struct && pnglib_read_image ? ReadArtPng(&size) : NULL;
		if (data)
		{
			loaded = Decode(data, size);
			MemoryFree(data);
		}

		DebugLog("Turn art %s", loaded ? "loaded" : "missing");
		return loaded;
	}

	Piece Resample(const Piece* src, LONG width, LONG height)
	{
		Piece res = { width, height, (COLORREF*)MemoryAlloc(width * height * sizeof(COLORREF)), (BYTE*)MemoryAlloc(width * height) };
		for (LONG y = 0; y < height; ++y)
		{
			for (LONG x = 0; x < width; ++x)
			{
				LONG r = 0, g = 0, b = 0, n = 0;
				for (LONG sy = 0; sy < ART_SAMPLES; ++sy)
				{
					LONG py = ((y * ART_SAMPLES + sy) * 2 + 1) * src->height / (height * ART_SAMPLES * 2);
					for (LONG sx = 0; sx < ART_SAMPLES; ++sx)
					{
						LONG px = ((x * ART_SAMPLES + sx) * 2 + 1) * src->width / (width * ART_SAMPLES * 2);
						LONG i = py * src->width + px;
						if (src->alpha[i])
						{
							r += GetRValue(src->pixels[i]);
							g += GetGValue(src->pixels[i]);
							b += GetBValue(src->pixels[i]);
							++n;
						}
					}
				}

				res.alpha[y * width + x] = n * 2 >= ART_SAMPLES * ART_SAMPLES;
				res.pixels[y * width + x] = n ? RGB(r / n, g / n, b / n) : 0;
			}
		}

		return res;
	}

	Piece Facet(const Piece* src, LONG width, LONG height, COLORREF rim)
	{
		LONG count = src->width * src->height;
		Piece full = { src->width, src->height, (COLORREF*)MemoryAlloc(count * sizeof(COLORREF)), (BYTE*)MemoryAlloc(count) };
		BYTE* known = (BYTE*)MemoryAlloc(count);
		MemoryCopy(full.pixels, src->pixels, count * sizeof(COLORREF));
		MemoryCopy(full.alpha, src->alpha, count);

		static const POINT steps[] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
		BOOL grew = TRUE;
		while (grew)
		{
			grew = FALSE;
			MemoryCopy(known, full.alpha, count);
			for (LONG y = 0; y < full.height; ++y)
			{
				for (LONG x = 0; x < full.width; ++x)
				{
					LONG i = y * full.width + x;
					if (known[i])
						continue;

					LONG r = 0, g = 0, b = 0, n = 0;
					for (DWORD s = 0; s < sizeof(steps) / sizeof(*steps); ++s)
					{
						LONG nx = x + steps[s].x;
						LONG ny = y + steps[s].y;
						if (nx < 0 || nx >= full.width || ny < 0 || ny >= full.height || !known[ny * full.width + nx])
							continue;

						COLORREF color = full.pixels[ny * full.width + nx];
						r += GetRValue(color);
						g += GetGValue(color);
						b += GetBValue(color);
						++n;
					}

					if (n)
					{
						full.pixels[i] = RGB(r / n, g / n, b / n);
						full.alpha[i] = TRUE;
						grew = TRUE;
					}
				}
			}
		}

		MemoryFree(known);
		Piece res = Resample(&full, width, height);
		Free(&full);

		FLOAT rx = width / 2.0f;
		FLOAT ry = height / 2.0f;
		for (LONG y = 0; y < height; ++y)
		{
			for (LONG x = 0; x < width; ++x)
			{
				if (x >= width / 2)
					res.pixels[y * width + x] = res.pixels[y * width + width - 1 - x];

				res.alpha[y * width + x] = fabsf(x + 0.5f - rx) / rx + fabsf(y + 0.5f - ry) / ry <= 1.0f;
			}
		}

		for (LONG y = 0; y < height; ++y)
		{
			for (LONG x = 0; x < width; ++x)
			{
				LONG i = y * width + x;
				if (res.alpha[i] && (!x || !y || x == width - 1 || y == height - 1
					|| !res.alpha[i - 1] || !res.alpha[i + 1] || !res.alpha[i - width] || !res.alpha[i + width]))
					res.pixels[i] = rim;
			}
		}

		return res;
	}

	VOID Free(Piece* piece)
	{
		if (piece->pixels)
			MemoryFree(piece->pixels);

		if (piece->alpha)
			MemoryFree(piece->alpha);

		MemoryZero(piece, sizeof(Piece));
	}
}
