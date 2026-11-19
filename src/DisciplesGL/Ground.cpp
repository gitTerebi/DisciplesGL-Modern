#include "StdAfx.h"
#include "Config.h"
#include "GLib.h"
#include "Ground.h"
#include "OpenDrawSurface.h"
#include "Pack.h"
#include "Twin.h"

#define f(a) (a + baseOffset)

#define GROUND_OBJECTS 4096
#define GROUND_CHUNKS 64
#define GROUND_OPS 512
#define GROUND_UNRESOLVED -2

namespace Ground
{
	struct Object {
		const VOID* obj;
		LONG frame;
		CHAR name[16];
	};

	struct Op {
		Kind kind;
		LONG tex, mask1, mask2;
		SHORT dx, dy, sx, sy;
		SHORT left, top, right, bottom;
	};

	struct Chunk {
		const VOID* data;
		LONG left, top, cw, ch;
		LONG pitch;
		BOOL valid;
		BOOL pending;
		DWORD stamp;
		DWORD color, version, sentVersion, count;
		Op ops[GROUND_OPS];
	};

	struct Blob {
		DWORD count, color;
		Op ops[GROUND_OPS];
	};

	struct Mask {
		const VOID* obj;
		const VOID* data;
	};

	struct {
		Object objects[GROUND_OBJECTS];
		Chunk chunks[GROUND_CHUNKS];
		DWORD chunkCount;
		Mask masks[2];
		Chunk* flush;
		const BYTE* flushStart;
		const BYTE* flushEnd;
	} ground;

	struct Vertex {
		FLOAT x, y, u, v, pu, pv;
	};

	struct {
		BOOL ready;
		BOOL failed;
		GLuint program, vao, vbo;
		GLint mode, two;
	} gl;

	DWORD sub_TileFactory, sub_BorderFactory, sub_MaskData, sub_RaceCode, sub_GroundUpdate;

	BOOL Enabled()
	{
		return sub_MaskData && Twin::Active() && Pack::Ready();
	}

	Object* FindObject(const VOID* obj, BOOL insert)
	{
		DWORD slot = ((DWORD)obj >> 3) * 2654435761u & (GROUND_OBJECTS - 1);
		for (DWORD i = 0; i < GROUND_OBJECTS; ++i, slot = (slot + 1) & (GROUND_OBJECTS - 1))
		{
			Object* o = &ground.objects[slot];
			if (o->obj == obj)
				return o;
			if (!o->obj)
			{
				if (!insert)
					return NULL;
				o->obj = obj;
				return o;
			}
		}
		return NULL;
	}

	LONG ObjectFrame(const VOID* obj)
	{
		Object* o = obj ? FindObject(obj, FALSE) : NULL;
		if (!o)
			return -1;
		if (o->frame == GROUND_UNRESOLVED && Pack::Ready())
			o->frame = Pack::FindRecord(o->name);
		return o->frame;
	}

	Chunk* FindChunk(const VOID* data)
	{
		for (DWORD i = 0; i < ground.chunkCount; ++i)
			if (ground.chunks[i].data == data && !ground.chunks[i].left && !ground.chunks[i].top)
				return &ground.chunks[i];
		return NULL;
	}

	VOID ResetChunk(Chunk* c, BOOL valid, DWORD color)
	{
		c->valid = valid;
		c->color = color;
		c->count = 0;
		c->pending = FALSE;
		++c->version;
	}

	VOID BeginChunk(const Hooks::BlitObject* obj)
	{
		if (!Enabled() || !obj->isTrueColor)
			return;

		Chunk* c = FindChunk(obj->data);
		for (DWORD i = 0; i < ground.chunkCount && !c; ++i)
			if (!ground.chunks[i].data)
				c = &ground.chunks[i];
		if (!c && ground.chunkCount < GROUND_CHUNKS)
			c = &ground.chunks[ground.chunkCount++];
		if (!c)
			return;

		c->data = obj->data;
		c->left = c->top = 0;
		c->cw = obj->rect.right;
		c->ch = obj->rect.bottom;
		c->pitch = obj->pitch >> 1;
		ResetChunk(c, TRUE, 0);

		ground.flush = c;
		ground.flushStart = (const BYTE*)c->data;
		ground.flushEnd = c->cw > 0 && c->ch > 0 ? ground.flushStart + ((c->ch - 1) * c->pitch + c->cw) * sizeof(DWORD) : ground.flushStart;
	}

	VOID OnFill(const Hooks::BlitObject* obj, DWORD color, const RECT* rect)
	{
		Chunk* c = obj->isTrueColor && ground.chunkCount ? FindChunk(obj->data) : NULL;
		if (c)
			ResetChunk(c, rect->left <= obj->rect.left && rect->top <= obj->rect.top && rect->right >= obj->rect.right && rect->bottom >= obj->rect.bottom, color);
	}

	VOID OnGround(Kind kind, const VOID* tile, const Hooks::BlitObject* obj, const POINT* src, const POINT* dst, const RECT* rect, const BYTE* blendMask, const DWORD* alphaMask)
	{
		if (!obj->isTrueColor || !Enabled())
			return;

		Chunk* c = FindChunk(obj->data);
		if (!c || !c->valid)
			return;
		if (c->count >= GROUND_OPS)
		{
			ResetChunk(c, FALSE, 0);
			return;
		}

		Op* op = &c->ops[c->count];
		op->kind = kind;
		op->tex = kind == OpClear ? -1 : ObjectFrame(tile);
		op->mask1 = op->mask2 = -1;
		BOOL ok = !alphaMask && (kind == OpClear || op->tex >= 0);
		if (ok && blendMask)
		{
			op->kind = OpBlend;
			if (blendMask == ground.masks[0].data)
				op->mask1 = ObjectFrame(ground.masks[0].obj);
			else if (blendMask == ground.masks[1].data)
				op->mask1 = ObjectFrame(ground.masks[1].obj);
			else
			{
				op->mask1 = ObjectFrame(ground.masks[0].obj);
				op->mask2 = ObjectFrame(ground.masks[1].obj);
				ok = op->mask2 >= 0;
			}
			ok = ok && op->mask1 >= 0;
		}
		if (!ok)
		{
			ResetChunk(c, FALSE, 0);
			return;
		}

		op->dx = (SHORT)dst->x;
		op->dy = (SHORT)dst->y;
		op->sx = (SHORT)(src ? src->x : 0);
		op->sy = (SHORT)(src ? src->y : 0);
		op->left = (SHORT)rect->left;
		op->top = (SHORT)rect->top;
		op->right = (SHORT)rect->right;
		op->bottom = (SHORT)rect->bottom;

		LONG list[] = { op->tex, op->mask1, op->mask2 };
		for (DWORD k = 0; k < 3; ++k)
			if (list[k] >= 0)
				Pack::State(list[k]);

		++c->count;
		++c->version;
	}

	BOOL Ready(const Chunk* c)
	{
		if (!c->data || !c->valid || !c->count)
			return FALSE;

		BOOL ready = TRUE;
		for (DWORD i = 0; i < c->count; ++i)
		{
			const Op* op = &c->ops[i];
			LONG list[] = { op->tex, op->mask1, op->mask2 };
			for (DWORD k = 0; k < 3; ++k)
				if (list[k] >= 0 && Pack::State(list[k]) != 2)
					ready = FALSE;
		}
		return ready;
	}

	VOID Send(Chunk* c)
	{
		static Blob blob;
		blob.count = c->count;
		blob.color = c->color;
		MemoryCopy(blob.ops, c->ops, c->count * sizeof(Op));
		c->pending = FALSE;
		c->sentVersion = c->version;

		RECT rect = { 0, 0, c->cw, c->ch };
		Twin::GroundArt(c->data, c->pitch, &rect, &blob, sizeof(blob) - sizeof(blob.ops) + c->count * sizeof(Op));
	}

	VOID Flush(const VOID* ptr)
	{
		const BYTE* at = (const BYTE*)ptr;
		if (at < ground.flushStart || at >= ground.flushEnd)
			return;

		Chunk* c = ground.flush;
		if (!c->data || at < (const BYTE*)c->data || c->pitch <= 0 || !c->valid || !c->count || c->sentVersion == c->version || !Enabled())
			return;

		DWORD offset = (DWORD)(at - (const BYTE*)c->data) / sizeof(DWORD);
		if ((LONG)(offset % c->pitch) < c->cw && (LONG)(offset / c->pitch) < c->ch && Ready(c))
			Send(c);
	}

	VOID Retry()
	{
		for (DWORD i = 0; i < ground.chunkCount; ++i)
		{
			Chunk* c = &ground.chunks[i];
			if (!c->pending)
				continue;
			if (!c->data || !c->valid || !c->count || Twin::Stamp(c->data) != c->stamp)
				c->pending = FALSE;
			else if (Ready(c))
				Send(c);
		}
	}

	VOID OnRelease(OpenDrawSurface* surface)
	{
		if (!surface->indexBuffer || !surface->indexBuffer->data)
			return;
		for (DWORD i = 0; i < ground.chunkCount; ++i)
			if (ground.chunks[i].data == surface->indexBuffer->data)
			{
				ResetChunk(&ground.chunks[i], FALSE, 0);
				ground.chunks[i].data = NULL;
			}
	}

	VOID NameGround(const VOID* obj, DWORD race, LONG border, DWORD variant)
	{
		const CHAR* code = obj ? ((const CHAR*(__stdcall*)(DWORD))sub_RaceCode)(race) : NULL;
		Object* o = code && StrLength(code) <= 2 ? FindObject(obj, TRUE) : NULL;
		if (!o)
			return;

		if (border < 0)
			StrPrint(o->name, "%s_%02d.PNG", code, variant % 100);
		else
			StrPrint(o->name, "%s_%02d_%02d.PNG", code, border % 100, variant % 100);
		o->frame = GROUND_UNRESOLVED;
		Pack::State(ObjectFrame(obj));
	}

	VOID* __fastcall TileFactory(VOID* thisObj, DWORD /*edx*/, DWORD race, DWORD variant)
	{
		VOID* tile = ((VOID*(__thiscall*)(VOID*, DWORD, DWORD))sub_TileFactory)(thisObj, race, variant);
		NameGround(tile, race, -1, variant);
		return tile;
	}

	VOID* __fastcall BorderFactory(VOID* thisObj, DWORD /*edx*/, DWORD race, DWORD border, DWORD variant)
	{
		VOID* tile = ((VOID*(__thiscall*)(VOID*, DWORD, DWORD, DWORD))sub_BorderFactory)(thisObj, race, border, variant);
		NameGround(tile, race, (LONG)border, variant);
		return tile;
	}

	VOID __stdcall GroundUpdate(VOID* texture, const Hooks::BlitObject* obj)
	{
		if (obj)
			BeginChunk(obj);
		((VOID(__stdcall*)(VOID*, const Hooks::BlitObject*))sub_GroundUpdate)(texture, obj);

		Chunk* c = obj && ground.chunkCount ? FindChunk(obj->data) : NULL;
		if (!c || !c->valid || !c->count || c->sentVersion == c->version || !Enabled())
			return;
		c->pitch = obj->pitch >> 1;
		if (Ready(c))
			Send(c);
		else
		{
			c->pending = TRUE;
			c->stamp = Twin::Stamp(c->data);
		}
	}

	const VOID* __fastcall MaskData(const VOID* thisObj, DWORD /*edx*/)
	{
		const VOID* data = ((const VOID*(__thiscall*)(const VOID*))sub_MaskData)(thisObj);
		ground.masks[1] = ground.masks[0];
		ground.masks[0].obj = thisObj;
		ground.masks[0].data = data;
		return data;
	}

	VOID Load(HOOKER hooker, const Hooks::AddressSpaceV2* hookSpace, DWORD baseOffset)
	{
		DWORD mask = 0, update = 0;
		if (!Pack::Present() || !hookSpace->ground_mask_slot || !ReadDWord(hooker, hookSpace->ground_mask_slot, &mask) || mask != f(hookSpace->ground_mask)
			|| !ReadDWord(hooker, hookSpace->ground_update_slot, &update) || update != f(hookSpace->ground_update))
			return;

		sub_GroundUpdate = update;
		PatchDWord(hooker, hookSpace->ground_update_slot, (DWORD)GroundUpdate);
		sub_RaceCode = f(hookSpace->ground_race_code);
		sub_TileFactory = RedirectCall(hooker, hookSpace->ground_tile_factory, TileFactory);
		sub_BorderFactory = RedirectCall(hooker, hookSpace->ground_border_factory, BorderFactory);
		sub_MaskData = mask;
		PatchDWord(hooker, hookSpace->ground_mask_slot, (DWORD)MaskData);
	}

	GLuint Compile(const CHAR* source, GLenum type)
	{
		CHAR* text = (CHAR*)MemoryAlloc(StrLength(GLSL_VER_1_30) + StrLength(source) + 1);
		StrCopy(text, GLSL_VER_1_30);
		StrCat(text, source);
		GLuint shader = GLCreateShader(type);
		GLShaderSource(shader, 1, (const GLchar**)&text, NULL);
		GLCompileShader(shader);
		MemoryFree(text);
		return shader;
	}

	BOOL InitGL()
	{
		if (gl.ready || gl.failed)
			return gl.ready;

		const CHAR* vertex =
			"in vec2 vCoord;\n"
			"in vec2 vTex;\n"
			"in vec2 vPlane;\n"
			"out vec2 fTex;\n"
			"out vec2 fPlane;\n"
			"void main()\n"
			"{\n"
			"	gl_Position = vec4(vCoord, 0.0, 1.0);\n"
			"	fTex = vTex;\n"
			"	fPlane = vPlane;\n"
			"}\n";

		const CHAR* fragment =
			"uniform sampler2D tex01;\n"
			"uniform sampler2D tex02;\n"
			"uniform sampler2D tex03;\n"
			"uniform int mode;\n"
			"uniform int two;\n"
			"in vec2 fTex;\n"
			"in vec2 fPlane;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	vec2 d = fPlane - vec2(31.0, 16.0);\n"
			"	float n = abs(d.x) / 32.0 + abs(d.y) / 16.0;\n"
			"	if (n > 1.04)\n"
			"		discard;\n"
			"	vec2 m = (vec2(31.0, 16.0) + d * min(1.0, 0.96 / max(n, 0.001))) / vec2(64.0, 32.0);\n"
			"	if (mode == 3)\n"
			"		FRAG_COLOR = vec4(0.0, 0.0, 0.0, 1.0);\n"
			"	else if (mode == 2)\n"
			"	{\n"
			"		vec4 b = texture(tex01, m);\n"
			"		if (b.a < 0.5)\n"
			"			discard;\n"
			"		FRAG_COLOR = vec4(b.rgb / b.a, 1.0);\n"
			"	}\n"
			"	else\n"
			"	{\n"
			"		if (fTex.x < 0.0 || fTex.y < 0.0 || fTex.x > 1.0 || fTex.y > 1.0)\n"
			"			discard;\n"
			"		vec3 c = texture(tex01, fTex).rgb;\n"
			"		float a = 1.0;\n"
			"		if (mode == 1)\n"
			"		{\n"
			"			a = texture(tex02, m).r;\n"
			"			if (two != 0 && a < 0.04)\n"
			"				a = texture(tex03, m).r;\n"
			"		}\n"
			"		FRAG_COLOR = vec4(c * a, a);\n"
			"	}\n"
			"}\n";

		gl.program = GLCreateProgram();
		GLBindAttribLocation(gl.program, 0, "vCoord");
		GLBindAttribLocation(gl.program, 1, "vTex");
		GLBindAttribLocation(gl.program, 2, "vPlane");
		GLuint vShader = Compile(vertex, GL_VERTEX_SHADER);
		GLuint fShader = Compile(fragment, GL_FRAGMENT_SHADER);
		GLAttachShader(gl.program, vShader);
		GLAttachShader(gl.program, fShader);
		GLLinkProgram(gl.program);
		GLDetachShader(gl.program, fShader);
		GLDetachShader(gl.program, vShader);
		GLDeleteShader(fShader);
		GLDeleteShader(vShader);

		GLUseProgram(gl.program);
		GLUniform1i(GLGetUniformLocation(gl.program, "tex01"), 0);
		GLUniform1i(GLGetUniformLocation(gl.program, "tex02"), 1);
		GLUniform1i(GLGetUniformLocation(gl.program, "tex03"), 2);
		gl.mode = GLGetUniformLocation(gl.program, "mode");
		gl.two = GLGetUniformLocation(gl.program, "two");

		GLGenVertexArrays(1, &gl.vao);
		GLBindVertexArray(gl.vao);
		GLGenBuffers(1, &gl.vbo);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLEnableVertexAttribArray(0);
		GLVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		GLEnableVertexAttribArray(1);
		GLVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)8);
		GLEnableVertexAttribArray(2);
		GLVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)16);

		gl.ready = gl.program && gl.vao && gl.vbo;
		gl.failed = !gl.ready;
		if (gl.failed)
			DebugLog("ground GL setup failed, map ground stays 1x");
		return gl.ready;
	}

	BOOL Draw(const VOID* ops, DWORD bytes, LONG width, LONG height)
	{
		const Blob* blob = (const Blob*)ops;
		DWORD header = sizeof(Blob) - sizeof(blob->ops);
		if (!ops || bytes < header || width <= 0 || height <= 0)
			return FALSE;

		DWORD count = blob->count, color = blob->color;
		if (!count || count > GROUND_OPS || bytes < header + count * sizeof(Op))
			return FALSE;

		GLuint ids[GROUND_OPS][3];
		for (DWORD i = 0; i < count; ++i)
		{
			const Op* op = &blob->ops[i];
			LONG list[] = { op->tex, op->mask1, op->mask2 };
			for (DWORD k = 0; k < 3; ++k)
			{
				ids[i][k] = 0;
				if (list[k] >= 0 && !Pack::Texture(list[k], &ids[i][k]))
					return FALSE;
			}
		}
		if (!InitGL())
			return FALSE;

		GLint view[4], box[4];
		GLfloat clear[4];
		GLGetIntegerv(GL_VIEWPORT, view);
		GLGetIntegerv(GL_SCISSOR_BOX, box);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);

		FLOAT alpha = (FLOAT)(color >> 24) / 255.0f;
		GLClearColor((FLOAT)(color & 0xFF) / 255.0f * alpha, (FLOAT)((color >> 8) & 0xFF) / 255.0f * alpha, (FLOAT)((color >> 16) & 0xFF) / 255.0f * alpha, alpha);
		GLClear(GL_COLOR_BUFFER_BIT);
		GLClearColor(clear[0], clear[1], clear[2], clear[3]);

		GLUseProgram(gl.program);
		GLBindVertexArray(gl.vao);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);

		for (DWORD i = 0; i < count; ++i)
		{
			const Op* op = &blob->ops[i];
			LONG l = max(view[0] + op->left * view[2] / width, box[0]);
			LONG t = max(view[1] + op->top * view[3] / height, box[1]);
			LONG r = min(view[0] + op->right * view[2] / width, box[0] + box[2]);
			LONG b = min(view[1] + op->bottom * view[3] / height, box[1] + box[3]);
			if (l >= r || t >= b)
				continue;

			for (DWORD k = 0; k < 3; ++k)
			{
				GLActiveTexture(GL_TEXTURE0 + k);
				GLBindTexture(GL_TEXTURE_2D, ids[i][k]);
			}

			GLUniform1i(gl.mode, op->kind);
			GLUniform1i(gl.two, op->mask2 >= 0);
			if (op->kind == OpCopy || op->kind == OpClear)
				GLDisable(GL_BLEND);
			else
			{
				GLEnable(GL_BLEND);
				GLBlendFunc(op->kind == OpBlend ? GL_DST_ALPHA : GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			}
			glScissor(l, t, r - l, b - t);

			LONG fw = 1, fh = 1;
			if (op->tex >= 0 && op->kind != OpBorder)
				Pack::Size(op->tex, &fw, &fh);
			FLOAT nx0 = (FLOAT)op->dx / width * 2.0f - 1.0f, nx1 = (FLOAT)(op->dx + 64) / width * 2.0f - 1.0f;
			FLOAT ny0 = (FLOAT)op->dy / height * 2.0f - 1.0f, ny1 = (FLOAT)(op->dy + 32) / height * 2.0f - 1.0f;
			FLOAT u0 = (FLOAT)op->sx / fw, u1 = (FLOAT)(op->sx + 64) / fw;
			FLOAT v0 = (FLOAT)op->sy / fh, v1 = (FLOAT)(op->sy + 32) / fh;

			Vertex quad[6] = {
				{ nx0, ny0, u0, v0, 0.0f, 0.0f },
				{ nx1, ny0, u1, v0, 64.0f, 0.0f },
				{ nx1, ny1, u1, v1, 64.0f, 32.0f },
				{ nx0, ny0, u0, v0, 0.0f, 0.0f },
				{ nx1, ny1, u1, v1, 64.0f, 32.0f },
				{ nx0, ny1, u0, v1, 0.0f, 32.0f }
			};
			GLBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STREAM_DRAW);
			GLDrawArrays(GL_TRIANGLES, 0, 6);
		}

		glScissor(box[0], box[1], box[2], box[3]);
		GLDisable(GL_BLEND);
		GLActiveTexture(GL_TEXTURE0);
		return TRUE;
	}

	VOID ReleaseGL()
	{
		if (gl.ready)
		{
			GLDeleteBuffers(1, &gl.vbo);
			GLDeleteVertexArrays(1, &gl.vao);
			GLDeleteProgram(gl.program);
		}
		MemoryZero(&gl, sizeof(gl));
	}
}
