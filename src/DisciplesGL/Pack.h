#pragma once

#define PACK_NAME "ff2k-v1.galleanpack"
#define PACK_SCALE 2

namespace Pack
{
	BOOL Present();
	VOID Start();
	BOOL Ready();
	BOOL Failed();
	LONG Find(const CHAR* key);
	LONG FindRecord(const CHAR* record);
	VOID Size(LONG frame, LONG* width, LONG* height);
	const CHAR* EntryName(LONG frame);
	LONG State(LONG frame);
	LONG Wait(LONG frame, DWORD timeout);
	LONG QueuedFor(LONG frame);
	VOID Prefetch(const CHAR* prefix, LONG at, CHAR ch);

	BOOL Prepare();
	VOID Draw();
	VOID ReleaseGL();
	BOOL Texture(LONG frame, GLuint* id);
	BOOL Upload(LONG frame);
	LONG Bytes(LONG frame);
}
