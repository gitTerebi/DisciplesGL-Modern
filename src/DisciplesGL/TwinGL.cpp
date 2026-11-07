#include "StdAfx.h"
#include "Config.h"
#include "GLib.h"
#include "Ground.h"
#include "Pack.h"
#include "Twin.h"
#include "TwinGL.h"

#define GL_CURRENT_PROGRAM 0x8B8D
#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_ARRAY_BUFFER_BINDING 0x8894
#define GL_ACTIVE_TEXTURE 0x84E0
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_TEXTURE2 0x84C2
#define GL_R8 0x8229
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_REVERSE_SUBTRACT 0x800B

#define TARGET_SCRATCH 0x10000
#define STAGE_SIZE 2048
#define LOG_FAILS 16

typedef VOID(__stdcall* GLBLENDEQUATION)(GLenum mode);

namespace TwinGL
{
	using namespace Twin;

	struct Vertex {
		FLOAT x, y, u, v;
	};

	struct Gpu {
		GLuint texture, fbo;
		LONG width, height, scale;
	};

	struct Placed {
		DWORD dst;
		LONG sx, sy;
		RECT rect;
	};

	struct {
		BOOL ready, failed;
		GLuint copyProgram, maskProgram, showProgram;
		GLint keyedLoc, keyLoc, artLoc, emptyLoc, weightLoc, fillLoc, tintedLoc, lighterLoc, solidLoc;
		GLint maskKeyedLoc, maskKeyLoc, maskFrameLoc, maskExtentLoc, ratioLoc, opaqueLoc;
		GLuint maskTexture;
		GLBLENDEQUATION blendEquation;
		GLuint vao, vbo, stage, scratch, scratchFbo;
		LONG scratchW, scratchH;
		Gpu twins[TWIN_MAX];
		Vertex* verts;
		DWORD vertCount, vertCapacity;
		Placed* placed;
		DWORD placedCount, placedCapacity;
		DWORD target, program;
		BOOL keyed;
		DWORD key;
		struct {
			BOOL valid;
			DWORD id;
			RECT hole;
			DWORD flags;
		} shown;
	} gl;

	GLuint Compile(const CHAR* source, GLenum type)
	{
		CHAR* text = (CHAR*)MemoryAlloc(StrLength(GLSL_VER_1_30) + StrLength(source) + 1);
		StrCopy(text, GLSL_VER_1_30);
		StrCat(text, source);

		GLuint shader = GLCreateShader(type);
		GLShaderSource(shader, 1, (const GLchar**)&text, NULL);
		GLCompileShader(shader);
		MemoryFree(text);

		GLint status = 0;
		GLGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if (!status)
		{
			DebugLog("twin shader compile failed type %X", type);
			GLDeleteShader(shader);
			return 0;
		}
		return shader;
	}

	GLuint Link(const CHAR* vertex, const CHAR* fragment)
	{
		GLuint vShader = Compile(vertex, GL_VERTEX_SHADER);
		GLuint fShader = Compile(fragment, GL_FRAGMENT_SHADER);
		GLuint program = vShader && fShader ? GLCreateProgram() : 0;
		if (program)
		{
			GLBindAttribLocation(program, 0, "vCoord");
			GLBindAttribLocation(program, 1, "vTex");
			GLAttachShader(program, vShader);
			GLAttachShader(program, fShader);
			GLLinkProgram(program);
			GLDetachShader(program, fShader);
			GLDetachShader(program, vShader);
		}
		if (vShader)
			GLDeleteShader(vShader);
		if (fShader)
			GLDeleteShader(fShader);
		return program;
	}

	VOID SetNearest(GLint filter)
	{
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}

	BOOL InitGL()
	{
		if (gl.ready || gl.failed)
			return gl.ready;

		const CHAR* vertex =
			"in vec2 vCoord;\n"
			"in vec2 vTex;\n"
			"out vec2 fTex;\n"
			"void main()\n"
			"{\n"
			"	gl_Position = vec4(vCoord, 0.0, 1.0);\n"
			"	fTex = vTex;\n"
			"}\n";

		const CHAR* copyFragment =
			"uniform sampler2D tex01;\n"
			"uniform int keyed;\n"
			"uniform int art;\n"
			"uniform int empty;\n"
			"uniform int fill;\n"
			"uniform int tinted;\n"
			"uniform int lighter;\n"
			"uniform vec2 weight;\n"
			"uniform vec4 key;\n"
			"uniform vec4 solid;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"bool isKey(vec4 c)\n"
			"{\n"
			"	return c.a < 0.5 / 255.0 || (c.a > 254.5 / 255.0 && all(lessThan(abs(c.rgb - key.rgb), vec3(0.5 / 255.0))));\n"
			"}\n"
			"void main()\n"
			"{\n"
			"	if (fill != 0)\n"
			"	{\n"
			"		FRAG_COLOR = solid;\n"
			"		return;\n"
			"	}\n"
			"	if (art != 0 && (fTex.x < 0.0 || fTex.y < 0.0 || fTex.x > 1.0 || fTex.y > 1.0))\n"
			"		discard;\n"
			"	vec4 c = texture(tex01, fTex);\n"
			"	if (keyed != 0)\n"
			"	{\n"
			"		if (isKey(c))\n"
			"			discard;\n"
			"		if (c.a > 253.5 / 255.0)\n"
			"			c.a = 1.0;\n"
			"	}\n"
			"	if (empty != 0)\n"
			"	{\n"
			"		if (c.a < 254.5 / 255.0 && all(lessThan(c.rgb, vec3(0.5 / 255.0))))\n"
			"			discard;\n"
			"		if (c.a > 253.5 / 255.0)\n"
			"			c.a = 1.0;\n"
			"	}\n"
			"	if (tinted != 0)\n"
			"		c = vec4(mix(c.rgb, solid.rgb, 0.5), 1.0);\n"
			"	if (lighter != 0)\n"
			"	{\n"
			"		if (isKey(c))\n"
			"			c = vec4(0.0);\n"
			"		else if (lighter == 2)\n"
			"			c = (c.r + c.g + c.b) * 255.0 > 56.5 ? vec4(min(c.rgb * 2.0, vec3(1.0)), 1.0) : vec4(0.0);\n"
			"	}\n"
			"	FRAG_COLOR = c * weight.x;\n"
			"}\n";

		const CHAR* maskFragment =
			"uniform sampler2D tex01;\n"
			"uniform sampler2D tex02;\n"
			"uniform sampler2D tex03;\n"
			"uniform int keyed;\n"
			"uniform vec4 key;\n"
			"uniform vec4 frame;\n"
			"uniform vec4 extent;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	vec2 local = (gl_FragCoord.xy - frame.xy) / frame.zw;\n"
			"	float m = texture(tex02, clamp(local * extent.xy, vec2(0.5 / 2048.0), extent.xy - vec2(0.5 / 2048.0))).r * 255.0;\n"
			"	if (m < 0.5)\n"
			"		discard;\n"
			"	if (keyed != 0)\n"
			"	{\n"
			"		vec4 d = texture(tex03, local * extent.zw);\n"
			"		if (d.a < 0.5 / 255.0 || (d.a > 254.5 / 255.0 && all(lessThan(abs(d.rgb - key.rgb), vec3(0.5 / 255.0)))))\n"
			"			discard;\n"
			"	}\n"
			"	vec4 c = texture(tex01, fTex);\n"
			"	FRAG_COLOR = c * (m > 254.5 ? 1.0 : (m + 1.0) / 256.0);\n"
			"}\n";

		const CHAR* showFragment =
			"uniform sampler2D tex01;\n"
			"uniform vec2 ratio;\n"
			"uniform int opaque;\n"
			"in vec2 fTex;\n"
			"out vec4 FRAG_COLOR;\n"
			"void main()\n"
			"{\n"
			"	ivec2 size = textureSize(tex01, 0);\n"
			"	vec2 r = max(ratio, vec2(1.0));\n"
			"	vec2 centre = fTex * vec2(size) - vec2(0.5);\n"
			"	vec2 lo = centre - r * 0.5 + vec2(0.5);\n"
			"	vec2 hi = centre + r * 0.5 + vec2(0.5);\n"
			"	ivec2 i0 = ivec2(floor(lo));\n"
			"	ivec2 i1 = min(ivec2(ceil(hi)) - ivec2(1), i0 + ivec2(7));\n"
			"	vec4 sum = vec4(0.0);\n"
			"	float total = 0.0;\n"
			"	for (int y = i0.y; y <= i1.y; ++y)\n"
			"	{\n"
			"		float wy = min(hi.y, float(y + 1)) - max(lo.y, float(y));\n"
			"		int cy = clamp(y, 0, size.y - 1);\n"
			"		for (int x = i0.x; x <= i1.x; ++x)\n"
			"		{\n"
			"			float w = (min(hi.x, float(x + 1)) - max(lo.x, float(x))) * wy;\n"
			"			if (w > 0.0)\n"
			"			{\n"
			"				sum += texelFetch(tex01, ivec2(clamp(x, 0, size.x - 1), cy), 0) * w;\n"
			"				total += w;\n"
			"			}\n"
			"		}\n"
			"	}\n"
			"	vec4 c = sum / max(total, 1e-6);\n"
			"	if (opaque != 0 && c.a < 254.5 / 255.0)\n"
			"		discard;\n"
			"	FRAG_COLOR = c;\n"
			"}\n";

		gl.blendEquation = (GLBLENDEQUATION)wglGetProcAddress("glBlendEquation");

		gl.copyProgram = Link(vertex, copyFragment);
		if (gl.copyProgram)
		{
			GLUseProgram(gl.copyProgram);
			GLUniform1i(GLGetUniformLocation(gl.copyProgram, "tex01"), 0);
			gl.keyedLoc = GLGetUniformLocation(gl.copyProgram, "keyed");
			gl.keyLoc = GLGetUniformLocation(gl.copyProgram, "key");
			gl.artLoc = GLGetUniformLocation(gl.copyProgram, "art");
			gl.emptyLoc = GLGetUniformLocation(gl.copyProgram, "empty");
			gl.weightLoc = GLGetUniformLocation(gl.copyProgram, "weight");
			gl.fillLoc = GLGetUniformLocation(gl.copyProgram, "fill");
			gl.tintedLoc = GLGetUniformLocation(gl.copyProgram, "tinted");
			gl.lighterLoc = GLGetUniformLocation(gl.copyProgram, "lighter");
			gl.solidLoc = GLGetUniformLocation(gl.copyProgram, "solid");
			GLUniform2f(gl.weightLoc, 1.0f, 0.0f);
		}

		gl.maskProgram = Link(vertex, maskFragment);
		if (gl.maskProgram)
		{
			GLUseProgram(gl.maskProgram);
			GLUniform1i(GLGetUniformLocation(gl.maskProgram, "tex01"), 0);
			GLUniform1i(GLGetUniformLocation(gl.maskProgram, "tex02"), 1);
			GLUniform1i(GLGetUniformLocation(gl.maskProgram, "tex03"), 2);
			gl.maskKeyedLoc = GLGetUniformLocation(gl.maskProgram, "keyed");
			gl.maskKeyLoc = GLGetUniformLocation(gl.maskProgram, "key");
			gl.maskFrameLoc = GLGetUniformLocation(gl.maskProgram, "frame");
			gl.maskExtentLoc = GLGetUniformLocation(gl.maskProgram, "extent");
		}

		gl.showProgram = Link(vertex, showFragment);
		if (gl.showProgram)
		{
			GLUseProgram(gl.showProgram);
			GLUniform1i(GLGetUniformLocation(gl.showProgram, "tex01"), 0);
			gl.ratioLoc = GLGetUniformLocation(gl.showProgram, "ratio");
			gl.opaqueLoc = GLGetUniformLocation(gl.showProgram, "opaque");
		}

		GLGenVertexArrays(1, &gl.vao);
		GLBindVertexArray(gl.vao);
		GLGenBuffers(1, &gl.vbo);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLEnableVertexAttribArray(0);
		GLVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)0);
		GLEnableVertexAttribArray(1);
		GLVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)8);

		GLGenTextures(1, &gl.stage);
		GLBindTexture(GL_TEXTURE_2D, gl.stage);
		SetNearest(GL_NEAREST);
		GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, STAGE_SIZE, STAGE_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		gl.ready = gl.copyProgram && gl.maskProgram && gl.showProgram && gl.vao && gl.vbo && gl.stage;
		gl.failed = !gl.ready;
		shared.glFailed = gl.failed;
		gl.keyed = FALSE;
		gl.key = 0;
		if (gl.failed)
			DebugLog("twin GL init failed, the game shows 1x");
		return gl.ready;
	}

	VOID Target(DWORD id, GLuint fbo, LONG width, LONG height)
	{
		if (gl.target == id)
			return;

		gl.target = id;
		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
		GLViewport(0, 0, width, height);
	}

	VOID TargetTwin(DWORD id)
	{
		Gpu* gpu = &gl.twins[id];
		Target(id, gpu->fbo, gpu->width * gpu->scale, gpu->height * gpu->scale);
	}

	VOID UseCopy(BOOL keyed, DWORD key)
	{
		if (gl.program != gl.copyProgram)
		{
			gl.program = gl.copyProgram;
			GLUseProgram(gl.copyProgram);
		}

		if (keyed != gl.keyed)
		{
			gl.keyed = keyed;
			GLUniform1i(gl.keyedLoc, keyed);
		}

		if (keyed && key != gl.key)
		{
			gl.key = key;
			GLUniform4f(gl.keyLoc, (key & 0xFF) / 255.0f, ((key >> 8) & 0xFF) / 255.0f, ((key >> 16) & 0xFF) / 255.0f, 1.0f);
		}
	}

	BOOL AddNdc(FLOAT nx0, FLOAT ny0, FLOAT nx1, FLOAT ny1, FLOAT u0, FLOAT v0, FLOAT u1, FLOAT v1)
	{
		if (gl.vertCount + 6 > gl.vertCapacity)
		{
			DWORD capacity = gl.vertCapacity ? gl.vertCapacity * 2 : 6 * 256;
			Vertex* verts = (Vertex*)MemoryAlloc(capacity * sizeof(Vertex));
			if (!verts)
				return FALSE;
			if (gl.verts)
			{
				MemoryCopy(verts, gl.verts, gl.vertCount * sizeof(Vertex));
				MemoryFree(gl.verts);
			}
			gl.verts = verts;
			gl.vertCapacity = capacity;
		}

		Vertex* v = &gl.verts[gl.vertCount];
		v[0] = { nx0, ny0, u0, v0 };
		v[1] = { nx1, ny0, u1, v0 };
		v[2] = { nx1, ny1, u1, v1 };
		v[3] = { nx0, ny0, u0, v0 };
		v[4] = { nx1, ny1, u1, v1 };
		v[5] = { nx0, ny1, u0, v1 };
		gl.vertCount += 6;
		return TRUE;
	}

	BOOL AddQuad(FLOAT width, FLOAT height, FLOAT x0, FLOAT y0, FLOAT x1, FLOAT y1, FLOAT u0, FLOAT v0, FLOAT u1, FLOAT v1)
	{
		return AddNdc(x0 / width * 2.0f - 1.0f, y0 / height * 2.0f - 1.0f, x1 / width * 2.0f - 1.0f, y1 / height * 2.0f - 1.0f, u0, v0, u1, v1);
	}

	VOID DrawVerts()
	{
		if (!gl.vertCount)
			return;

		GLBufferData(GL_ARRAY_BUFFER, gl.vertCount * sizeof(Vertex), gl.verts, GL_STREAM_DRAW);
		GLDrawArrays(GL_TRIANGLES, 0, gl.vertCount);
		gl.vertCount = 0;
	}

	BOOL Valid(DWORD id)
	{
		return id < TWIN_MAX && gl.twins[id].texture;
	}

	VOID DestroyGpu(DWORD id)
	{
		Gpu* gpu = &gl.twins[id];
		if (gpu->texture)
			GLDeleteTextures(1, &gpu->texture);
		if (gpu->fbo)
			GLDeleteFramebuffers(1, &gpu->fbo);
		MemoryZero(gpu, sizeof(Gpu));

		if (gl.target == id)
			gl.target = TWIN_NONE;
	}

	VOID CreateGpu(DWORD id, LONG width, LONG height, LONG scale)
	{
		if (id >= TWIN_MAX)
			return;

		DestroyGpu(id);
		if (width <= 0 || height <= 0 || scale <= 0)
			return;

		Gpu* gpu = &gl.twins[id];
		while (glGetError() != GL_NO_ERROR)
			;

		GLGenTextures(1, &gpu->texture);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		SetNearest(GL_NEAREST);
		GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width * scale, height * scale, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		GLenum error = glGetError();
		if (error != GL_NO_ERROR)
		{
			static DWORD logged;
			if (logged < LOG_FAILS)
			{
				++logged;
				DebugLog("twin texture %u %dx%d failed error %X", id, width * scale, height * scale, error);
			}
			DestroyGpu(id);
			return;
		}

		GLGenFramebuffers(1, &gpu->fbo);
		gpu->width = width;
		gpu->height = height;
		gpu->scale = scale;

		gl.target = TWIN_NONE;
		TargetTwin(id);
		GLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gpu->texture, 0);
		GLClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		GLClear(GL_COLOR_BUFFER_BIT);
	}


	BOOL EnsureTarget(GLuint* texture, GLuint* fbo, LONG* curW, LONG* curH, LONG width, LONG height)
	{
		if (*texture && *curW >= width && *curH >= height)
			return TRUE;

		if (*texture)
			GLDeleteTextures(1, texture);
		if (*fbo)
			GLDeleteFramebuffers(1, fbo);
		*texture = 0;
		*fbo = 0;
		*curW = *curH = 0;

		while (glGetError() != GL_NO_ERROR)
			;

		GLGenTextures(1, texture);
		GLBindTexture(GL_TEXTURE_2D, *texture);
		SetNearest(GL_NEAREST);
		GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		if (glGetError() != GL_NO_ERROR)
		{
			GLDeleteTextures(1, texture);
			*texture = 0;
			return FALSE;
		}

		GLGenFramebuffers(1, fbo);
		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, *fbo);
		GLFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);
		gl.target = TWIN_NONE;

		*curW = width;
		*curH = height;
		return TRUE;
	}

	VOID Blend(BOOL enable)
	{
		if (enable)
		{
			GLEnable(GL_BLEND);
			GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
			GLDisable(GL_BLEND);
	}

	BOOL ToScratch(Gpu* gpu, const RECT* rect, FLOAT* extentW, FLOAT* extentH);

	VOID ReplaySelfCopy(const Cmd* cmd)
	{
		Gpu* gpu = &gl.twins[cmd->dst];
		FLOAT ew, eh;
		if (!ToScratch(gpu, &cmd->rect, &ew, &eh))
			return;

		LONG scale = gpu->scale;
		LONG width = (cmd->rect.right - cmd->rect.left) * scale;
		LONG height = (cmd->rect.bottom - cmd->rect.top) * scale;
		TargetTwin(cmd->dst);
		UseCopy(cmd->flags, cmd->value);
		GLBindTexture(GL_TEXTURE_2D, gl.scratch);
		AddQuad((FLOAT)(gpu->width * scale), (FLOAT)(gpu->height * scale), (FLOAT)(cmd->x * scale), (FLOAT)(cmd->y * scale), (FLOAT)(cmd->x * scale + width), (FLOAT)(cmd->y * scale + height),
			0.0f, 0.0f, ew, eh);
		Blend(cmd->flags != 0);
		DrawVerts();
		Blend(FALSE);
	}

	DWORD ReplayCopies(const Cmd* cmds, DWORD count, DWORD index)
	{
		const Cmd* first = &cmds[index];
		if (first->src == first->dst)
		{
			if (Valid(first->dst))
				ReplaySelfCopy(first);
			return index + 1;
		}

		BOOL valid = Valid(first->src) && Valid(first->dst);
		Gpu* src = &gl.twins[first->src < TWIN_MAX ? first->src : 0];
		Gpu* dst = &gl.twins[first->dst < TWIN_MAX ? first->dst : 0];
		FLOAT tw = (FLOAT)(dst->width * dst->scale), th = (FLOAT)(dst->height * dst->scale);
		LONG scale = dst->scale;

		DWORD i = index;
		gl.vertCount = 0;
		for (; i < count; ++i)
		{
			const Cmd* cmd = &cmds[i];
			if (cmd->type != CmdCopy || cmd->src != first->src || cmd->dst != first->dst || cmd->flags != first->flags || (cmd->flags && cmd->value != first->value))
				break;

			if (valid)
			{
				LONG w = cmd->rect.right - cmd->rect.left;
				LONG h = cmd->rect.bottom - cmd->rect.top;
				AddQuad(tw, th, (FLOAT)(cmd->x * scale), (FLOAT)(cmd->y * scale), (FLOAT)((cmd->x + w) * scale), (FLOAT)((cmd->y + h) * scale),
					(FLOAT)cmd->rect.left / src->width, (FLOAT)cmd->rect.top / src->height, (FLOAT)cmd->rect.right / src->width, (FLOAT)cmd->rect.bottom / src->height);
			}
		}

		if (valid)
		{
			TargetTwin(first->dst);
			UseCopy(first->flags, first->value);
			GLBindTexture(GL_TEXTURE_2D, src->texture);
			if (first->flags)
				Blend(TRUE);
			DrawVerts();
			if (first->flags)
				Blend(FALSE);
		}
		gl.vertCount = 0;

		return i;
	}

	VOID FlushPlaced()
	{
		for (DWORD i = 0; i < gl.placedCount; ++i)
		{
			DWORD dst = gl.placed[i].dst;
			BOOL seen = FALSE;
			for (DWORD k = 0; k < i && !seen; ++k)
				seen = gl.placed[k].dst == dst;
			if (seen)
				continue;

			Gpu* gpu = &gl.twins[dst];
			LONG scale = gpu->scale;
			FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);

			gl.vertCount = 0;
			for (DWORD k = i; k < gl.placedCount; ++k)
			{
				const Placed* p = &gl.placed[k];
				if (p->dst != dst)
					continue;

				LONG w = p->rect.right - p->rect.left;
				LONG h = p->rect.bottom - p->rect.top;
				AddQuad(tw, th, (FLOAT)(p->rect.left * scale), (FLOAT)(p->rect.top * scale), (FLOAT)(p->rect.right * scale), (FLOAT)(p->rect.bottom * scale),
					(FLOAT)p->sx / STAGE_SIZE, (FLOAT)p->sy / STAGE_SIZE, (FLOAT)(p->sx + w) / STAGE_SIZE, (FLOAT)(p->sy + h) / STAGE_SIZE);
			}

			TargetTwin(dst);
			UseCopy(FALSE, 0);
			GLBindTexture(GL_TEXTURE_2D, gl.stage);
			DrawVerts();
		}

		gl.placedCount = 0;
	}

	VOID StageUpload(DWORD dst, const RECT* rect, const DWORD* pixels, LONG* shelf)
	{
		LONG width = rect->right - rect->left;
		LONG height = rect->bottom - rect->top;
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, width);

		for (LONG ty = 0; ty < height; ty += STAGE_SIZE)
		{
			LONG th = min(height - ty, STAGE_SIZE);
			for (LONG tx = 0; tx < width; tx += STAGE_SIZE)
			{
				LONG tw = min(width - tx, STAGE_SIZE);

				if (shelf[0] + tw > STAGE_SIZE)
				{
					shelf[1] += shelf[2];
					shelf[0] = 0;
					shelf[2] = 0;
				}

				if (shelf[1] + th > STAGE_SIZE)
				{
					FlushPlaced();
					shelf[0] = shelf[1] = shelf[2] = 0;
				}

				if (gl.placedCount == gl.placedCapacity)
				{
					DWORD capacity = gl.placedCapacity ? gl.placedCapacity * 2 : 256;
					Placed* placed = (Placed*)MemoryAlloc(capacity * sizeof(Placed));
					if (!placed)
						continue;
					if (gl.placed)
					{
						MemoryCopy(placed, gl.placed, gl.placedCount * sizeof(Placed));
						MemoryFree(gl.placed);
					}
					gl.placed = placed;
					gl.placedCapacity = capacity;
				}

				GLTexSubImage2D(GL_TEXTURE_2D, 0, shelf[0], shelf[1], tw, th, GL_RGBA, GL_UNSIGNED_BYTE, pixels + ty * width + tx);

				Placed* p = &gl.placed[gl.placedCount++];
				p->dst = dst;
				p->sx = shelf[0];
				p->sy = shelf[1];
				SetRect(&p->rect, rect->left + tx, rect->top + ty, rect->left + tx + tw, rect->top + ty + th);

				shelf[0] += tw;
				if (th > shelf[2])
					shelf[2] = th;
			}
		}
	}

	DWORD ReplayUploads(const List* list, DWORD index)
	{
		GLBindTexture(GL_TEXTURE_2D, gl.stage);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);

		LONG shelf[3] = { 0, 0, 0 };
		DWORD i = index;
		for (; i < list->count && list->cmds[i].type == CmdUpload; ++i)
		{
			const Cmd* cmd = &list->cmds[i];
			if (Valid(cmd->dst))
				StageUpload(cmd->dst, &cmd->rect, (const DWORD*)(list->arena + cmd->value), shelf);
		}

		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		FlushPlaced();
		return i;
	}
	DWORD ReplayPixels(const List* list, DWORD index)
	{
		const Cmd* first = &list->cmds[index];
		DWORD end = index + 1;
		while (end < list->count)
		{
			const Cmd* cmd = &list->cmds[end];
			if (cmd->type != CmdPixel || cmd->src != first->src || cmd->dst != first->dst || cmd->flags != first->flags || cmd->value != first->value || cmd->extra != first->extra)
				break;
			++end;
		}

		DWORD mode = first->flags;
		if (!Valid(first->src) || !Valid(first->dst) || (mode == PixelSub && !gl.blendEquation))
			return end;

		Gpu* src = &gl.twins[first->src];
		Gpu* dst = &gl.twins[first->dst];
		LONG scale = dst->scale;
		FLOAT tw = (FLOAT)(dst->width * scale), th = (FLOAT)(dst->height * scale);

		gl.vertCount = 0;
		for (DWORD i = index; i < end; ++i)
		{
			const Cmd* cmd = &list->cmds[i];
			LONG w = cmd->rect.right - cmd->rect.left;
			LONG h = cmd->rect.bottom - cmd->rect.top;
			FLOAT u0 = (FLOAT)cmd->rect.left / src->width, u1 = (FLOAT)cmd->rect.right / src->width;
			if ((mode == PixelCopy || mode == PixelBlend) && (cmd->extra & PixelMirror))
			{
				FLOAT u = u0;
				u0 = u1;
				u1 = u;
			}
			AddQuad(tw, th, (FLOAT)(cmd->x * scale), (FLOAT)(cmd->y * scale), (FLOAT)((cmd->x + w) * scale), (FLOAT)((cmd->y + h) * scale),
				u0, (FLOAT)cmd->rect.top / src->height, u1, (FLOAT)cmd->rect.bottom / src->height);
		}

		DWORD param = first->extra & 0xFF;
		FLOAT weight = 1.0f;
		BOOL keyed = FALSE, empty = FALSE, tinted = FALSE, blend = TRUE;
		switch (mode)
		{
		case PixelAverage:
			keyed = TRUE;
			weight = (param + 1) / 256.0f;
			break;
		case PixelKeyed:
			keyed = TRUE;
			weight = 0.5f * (param >= 255 ? 1.0f : (param + 1) / 256.0f);
			break;
		case PixelEmpty:
			empty = TRUE;
			weight = 0.5f;
			break;
		case PixelCopy:
			blend = FALSE;
			break;
		case PixelTint:
			keyed = TRUE;
			tinted = TRUE;
			blend = FALSE;
			break;
		case PixelBlend:
			weight = (first->extra & 0xFFFF) / 256.0f;
			break;
		default:
			break;
		}

		TargetTwin(first->dst);
		UseCopy(keyed, first->value);
		if (empty)
			GLUniform1i(gl.emptyLoc, 1);
		if (tinted)
		{
			DWORD color = first->extra;
			GLUniform1i(gl.tintedLoc, 1);
			GLUniform4f(gl.solidLoc, (color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, 1.0f);
		}
		if (weight != 1.0f)
			GLUniform2f(gl.weightLoc, weight, 0.0f);
		GLBindTexture(GL_TEXTURE_2D, src->texture);

		if (blend)
			GLEnable(GL_BLEND);
		if (mode == PixelAdd || mode == PixelSub)
		{
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
			GLBlendFunc(GL_ONE, GL_ONE);
			if (mode == PixelSub)
				gl.blendEquation(GL_FUNC_REVERSE_SUBTRACT);
		}
		else
			GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

		DrawVerts();

		if (mode == PixelSub)
			gl.blendEquation(GL_FUNC_ADD);
		if (mode == PixelAdd || mode == PixelSub)
			glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		GLDisable(GL_BLEND);
		if (empty)
			GLUniform1i(gl.emptyLoc, 0);
		if (tinted)
			GLUniform1i(gl.tintedLoc, 0);
		if (weight != 1.0f)
			GLUniform2f(gl.weightLoc, 1.0f, 0.0f);
		gl.vertCount = 0;
		return end;
	}

	VOID SolidColor(DWORD color, FLOAT alpha, BOOL premultiply)
	{
		FLOAT k = premultiply ? alpha : 1.0f;
		GLUniform4f(gl.solidLoc, (color & 0xFF) / 255.0f * k, ((color >> 8) & 0xFF) / 255.0f * k, ((color >> 16) & 0xFF) / 255.0f * k, alpha);
	}

	DWORD ReplayFills(const List* list, DWORD index, BOOL tint)
	{
		const Cmd* first = &list->cmds[index];
		DWORD end = index + 1;
		while (end < list->count)
		{
			const Cmd* cmd = &list->cmds[end];
			if (cmd->type != first->type || cmd->dst != first->dst || cmd->value != first->value || cmd->flags != first->flags || cmd->extra != first->extra)
				break;
			++end;
		}

		if (!Valid(first->dst))
			return end;

		Gpu* gpu = &gl.twins[first->dst];
		LONG scale = gpu->scale;
		FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);
		gl.vertCount = 0;
		for (DWORD i = index; i < end; ++i)
		{
			const RECT* r = &list->cmds[i].rect;
			AddQuad(tw, th, (FLOAT)(r->left * scale), (FLOAT)(r->top * scale), (FLOAT)(r->right * scale), (FLOAT)(r->bottom * scale), 0.0f, 0.0f, 0.0f, 0.0f);
		}

		TargetTwin(first->dst);
		UseCopy(FALSE, 0);
		GLUniform1i(gl.fillLoc, 1);
		DWORD color = first->value;
		if (tint)
		{
			SolidColor(color, first->extra / 256.0f, TRUE);
			Blend(TRUE);
		}
		else if (first->flags)
			GLUniform4f(gl.solidLoc, 0.0f, 0.0f, 0.0f, 0.0f);
		else
			SolidColor(color, color >= ALPHA_COMPONENT ? 1.0f : 254.0f / 255.0f, FALSE);
		DrawVerts();
		if (tint)
			Blend(FALSE);
		GLUniform1i(gl.fillLoc, 0);
		gl.vertCount = 0;
		return end;
	}

	BOOL ToScratch(Gpu* gpu, const RECT* rect, FLOAT* extentW, FLOAT* extentH)
	{
		LONG scale = gpu->scale;
		LONG width = (rect->right - rect->left) * scale;
		LONG height = (rect->bottom - rect->top) * scale;
		if (!EnsureTarget(&gl.scratch, &gl.scratchFbo, &gl.scratchW, &gl.scratchH, max(gl.scratchW, width), max(gl.scratchH, height)))
			return FALSE;

		Target(TARGET_SCRATCH, gl.scratchFbo, gl.scratchW, gl.scratchH);
		UseCopy(FALSE, 0);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		gl.vertCount = 0;
		AddQuad((FLOAT)gl.scratchW, (FLOAT)gl.scratchH, 0.0f, 0.0f, (FLOAT)width, (FLOAT)height,
			(FLOAT)rect->left / gpu->width, (FLOAT)rect->top / gpu->height, (FLOAT)rect->right / gpu->width, (FLOAT)rect->bottom / gpu->height);
		DrawVerts();

		*extentW = (FLOAT)width / gl.scratchW;
		*extentH = (FLOAT)height / gl.scratchH;
		return TRUE;
	}

	VOID KeyUniform(GLint location, DWORD key)
	{
		GLUniform4f(location, (key & 0xFF) / 255.0f, ((key >> 8) & 0xFF) / 255.0f, ((key >> 16) & 0xFF) / 255.0f, 1.0f);
	}

	VOID ReplayLighter(const Cmd* cmd)
	{
		if (!Valid(cmd->dst))
			return;

		Gpu* gpu = &gl.twins[cmd->dst];
		FLOAT ew, eh;
		if (!ToScratch(gpu, &cmd->rect, &ew, &eh))
			return;

		LONG scale = gpu->scale;
		TargetTwin(cmd->dst);
		UseCopy(FALSE, 0);
		KeyUniform(gl.keyLoc, cmd->value);
		gl.key = cmd->value;
		GLUniform1i(gl.lighterLoc, cmd->flags ? 2 : 1);
		GLBindTexture(GL_TEXTURE_2D, gl.scratch);
		gl.vertCount = 0;
		AddQuad((FLOAT)(gpu->width * scale), (FLOAT)(gpu->height * scale), (FLOAT)(cmd->rect.left * scale), (FLOAT)(cmd->rect.top * scale), (FLOAT)(cmd->rect.right * scale), (FLOAT)(cmd->rect.bottom * scale),
			0.0f, 0.0f, ew, eh);
		DrawVerts();
		GLUniform1i(gl.lighterLoc, 0);
	}

	VOID ReplayMask(const Cmd* cmd, const List* list)
	{
		if (!Valid(cmd->src) || !Valid(cmd->dst))
			return;

		Gpu* src = &gl.twins[cmd->src];
		Gpu* dst = &gl.twins[cmd->dst];
		LONG scale = dst->scale;
		LONG width = cmd->rect.right - cmd->rect.left;
		LONG height = cmd->rect.bottom - cmd->rect.top;
		RECT to = { cmd->x, cmd->y, cmd->x + width, cmd->y + height };

		BOOL dstKeyed = (cmd->flags & 1) != 0;
		BOOL exact = (cmd->flags & 2) != 0;
		FLOAT ew = 0.0f, eh = 0.0f;
		if (dstKeyed && !ToScratch(dst, &to, &ew, &eh))
			return;

		if (!gl.maskTexture)
		{
			GLGenTextures(1, &gl.maskTexture);
			GLActiveTexture(GL_TEXTURE1);
			GLBindTexture(GL_TEXTURE_2D, gl.maskTexture);
			SetNearest(GL_LINEAR);
			GLTexImage2D(GL_TEXTURE_2D, 0, GL_R8, MASK_SIZE, MASK_SIZE, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		}

		GLActiveTexture(GL_TEXTURE1);
		GLBindTexture(GL_TEXTURE_2D, gl.maskTexture);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, width);
		GLTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, list->arena + cmd->extra);
		GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
		GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		if (exact)
			SetNearest(GL_NEAREST);
		if (dstKeyed)
		{
			GLActiveTexture(GL_TEXTURE2);
			GLBindTexture(GL_TEXTURE_2D, gl.scratch);
		}
		GLActiveTexture(GL_TEXTURE0);
		GLBindTexture(GL_TEXTURE_2D, src->texture);

		TargetTwin(cmd->dst);
		gl.program = gl.maskProgram;
		GLUseProgram(gl.maskProgram);
		GLUniform1i(gl.maskKeyedLoc, dstKeyed ? 1 : 0);
		KeyUniform(gl.maskKeyLoc, cmd->value);
		GLUniform4f(gl.maskFrameLoc, (FLOAT)(to.left * scale), (FLOAT)(to.top * scale), (FLOAT)(width * scale), (FLOAT)(height * scale));
		GLUniform4f(gl.maskExtentLoc, (FLOAT)width / MASK_SIZE, (FLOAT)height / MASK_SIZE, ew, eh);

		gl.vertCount = 0;
		AddQuad((FLOAT)(dst->width * scale), (FLOAT)(dst->height * scale), (FLOAT)(to.left * scale), (FLOAT)(to.top * scale), (FLOAT)(to.right * scale), (FLOAT)(to.bottom * scale),
			(FLOAT)cmd->rect.left / src->width, (FLOAT)cmd->rect.top / src->height, (FLOAT)cmd->rect.right / src->width, (FLOAT)cmd->rect.bottom / src->height);
		Blend(TRUE);
		DrawVerts();
		Blend(FALSE);
		if (exact)
		{
			GLActiveTexture(GL_TEXTURE1);
			SetNearest(GL_LINEAR);
			GLActiveTexture(GL_TEXTURE0);
		}
	}

	BOOL SeedTexture(LONG frame, GLuint* texture)
	{
		if (Pack::Texture(frame, texture))
			return TRUE;
		if (!Pack::Upload(frame))
			return FALSE;
		GLActiveTexture(GL_TEXTURE0);
		return Pack::Texture(frame, texture);
	}

	DWORD ReplaySeeds(const List* list, DWORD index)
	{
		const Cmd* first = &list->cmds[index];
		DWORD end = index + 1;
		while (end < list->count && list->cmds[end].type == CmdSeed && list->cmds[end].dst == first->dst && list->cmds[end].value == first->value)
			++end;

		GLuint texture;
		if (!Valid(first->dst) || !SeedTexture((LONG)first->value, &texture))
			return end;

		Gpu* gpu = &gl.twins[first->dst];
		LONG scale = gpu->scale;
		FLOAT tw = (FLOAT)(gpu->width * scale), th = (FLOAT)(gpu->height * scale);
		gl.vertCount = 0;
		for (DWORD i = index; i < end; ++i)
		{
			const RECT* to = &list->cmds[i].rect;
			const RECT* place = &list->cmds[i].place;
			FLOAT pw = (FLOAT)(place->right - place->left), ph = (FLOAT)(place->bottom - place->top);
			AddQuad(tw, th, (FLOAT)(to->left * scale), (FLOAT)(to->top * scale), (FLOAT)(to->right * scale), (FLOAT)(to->bottom * scale),
				(to->left - place->left) / pw, (to->top - place->top) / ph, (to->right - place->left) / pw, (to->bottom - place->top) / ph);
		}

		TargetTwin(first->dst);
		UseCopy(FALSE, 0);
		GLUniform1i(gl.artLoc, 1);
		GLBindTexture(GL_TEXTURE_2D, texture);
		GLEnable(GL_BLEND);
		GLBlendFunc(GL_DST_ALPHA, GL_ZERO);
		DrawVerts();
		GLBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		GLDisable(GL_BLEND);
		GLUniform1i(gl.artLoc, 0);
		return end;
	}

	VOID ReplayGround(const Cmd* cmd, const List* list)
	{
		if (!Valid(cmd->dst))
			return;

		Gpu* gpu = &gl.twins[cmd->dst];
		LONG scale = gpu->scale;
		LONG width = cmd->rect.right - cmd->rect.left;
		LONG height = cmd->rect.bottom - cmd->rect.top;

		TargetTwin(cmd->dst);
		GLViewport(cmd->rect.left * scale, cmd->rect.top * scale, width * scale, height * scale);
		GLEnable(GL_SCISSOR_TEST);
		glScissor(cmd->rect.left * scale, cmd->rect.top * scale, width * scale, height * scale);
		BOOL drawn = Ground::Draw(list->arena + cmd->value, cmd->extra, width, height);

		gl.target = TWIN_NONE;
		gl.program = 0;
		GLUseProgram(0);
		GLBindVertexArray(gl.vao);
		GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
		GLDisable(GL_BLEND);
		GLDisable(GL_SCISSOR_TEST);
		GLActiveTexture(GL_TEXTURE0);

		if (!drawn)
		{
			GLBindTexture(GL_TEXTURE_2D, gl.stage);
			GLPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			LONG shelf[3] = { 0, 0, 0 };
			StageUpload(cmd->dst, &cmd->rect, (const DWORD*)(list->arena + cmd->src), shelf);
			GLPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			FlushPlaced();
		}
	}

	VOID Replay(const List* list)
	{
		for (DWORD i = 0; i < list->count;)
		{
			const Cmd* cmd = &list->cmds[i];
			DWORD next = i + 1;
			switch (cmd->type)
			{
			case CmdCreate:
				CreateGpu(cmd->dst, cmd->rect.right, cmd->rect.bottom, (LONG)cmd->value);
				break;
			case CmdDestroy:
				if (cmd->dst < TWIN_MAX)
					DestroyGpu(cmd->dst);
				break;
			case CmdReset:
				for (DWORD k = 0; k < TWIN_MAX; ++k)
					if (gl.twins[k].texture || gl.twins[k].fbo)
						DestroyGpu(k);
				break;
			case CmdCopy:
				next = ReplayCopies(list->cmds, list->count, i);
				break;
			case CmdFill:
			case CmdTint:
				next = ReplayFills(list, i, cmd->type == CmdTint);
				break;
			case CmdUpload:
				next = ReplayUploads(list, i);
				break;
			case CmdPixel:
				next = ReplayPixels(list, i);
				break;
			case CmdMask:
				ReplayMask(cmd, list);
				break;
			case CmdLighter:
				ReplayLighter(cmd);
				break;
			case CmdSeed:
				next = ReplaySeeds(list, i);
				break;
			case CmdGround:
				ReplayGround(cmd, list);
				break;
			case CmdPresent:
				gl.shown.valid = TRUE;
				gl.shown.id = cmd->dst;
				gl.shown.hole = cmd->rect;
				gl.shown.flags = cmd->flags;
				break;
			}
			i = max(next, i + 1);
		}
	}

	VOID AddShowQuad(const RECT* part, const Gpu* gpu, LONG baseX, LONG baseY, DWORD frameWidth, DWORD frameHeight)
	{
		if (part->left >= part->right || part->top >= part->bottom)
			return;

		FLOAT fw = (FLOAT)frameWidth, fh = (FLOAT)frameHeight;
		AddNdc((part->left - baseX) / fw * 2.0f - 1.0f, 1.0f - (part->top - baseY) / fh * 2.0f, (part->right - baseX) / fw * 2.0f - 1.0f, 1.0f - (part->bottom - baseY) / fh * 2.0f,
			(FLOAT)part->left / gpu->width, (FLOAT)part->top / gpu->height, (FLOAT)part->right / gpu->width, (FLOAT)part->bottom / gpu->height);
	}

	BOOL Show(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight, GLuint drawFbo)
	{
		if (!gl.shown.valid || !Valid(gl.shown.id) || rect->width <= 0 || rect->height <= 0 || !frameWidth || !frameHeight)
			return FALSE;

		Gpu* gpu = &gl.twins[gl.shown.id];
		gl.target = TWIN_NONE;
		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
		GLViewport(rect->x, rect->y + offset, rect->width, rect->height);

		LONG baseX = (LONG)(config.mode->width - frameWidth) >> 1;
		LONG baseY = (LONG)(config.mode->height - frameHeight) >> 1;
		RECT frame = { baseX, baseY, baseX + (LONG)frameWidth, baseY + (LONG)frameHeight };

		gl.vertCount = 0;
		RECT hole;
		BOOL zoomed = ((gl.shown.flags & PresentMapZoom) || Config::IsMapZoomed()) && !(gl.shown.flags & PresentZoomed);
		if (zoomed && IntersectRect(&hole, &gl.shown.hole, &frame))
		{
			RECT parts[4] = {
				{ frame.left, frame.top, frame.right, hole.top },
				{ frame.left, hole.bottom, frame.right, frame.bottom },
				{ frame.left, hole.top, hole.left, hole.bottom },
				{ hole.right, hole.top, frame.right, hole.bottom }
			};
			for (DWORD k = 0; k < 4; ++k)
				AddShowQuad(&parts[k], gpu, baseX, baseY, frameWidth, frameHeight);
		}
		else
			AddShowQuad(&frame, gpu, baseX, baseY, frameWidth, frameHeight);

		gl.program = gl.showProgram;
		GLUseProgram(gl.showProgram);
		GLUniform2f(gl.ratioLoc, (FLOAT)(frameWidth * gpu->scale) / rect->width, (FLOAT)(frameHeight * gpu->scale) / rect->height);
		GLUniform1i(gl.opaqueLoc, (gl.shown.flags & PresentBack) ? 1 : 0);
		GLBindTexture(GL_TEXTURE_2D, gpu->texture);
		DrawVerts();
		return TRUE;
	}

	BOOL Prepare()
	{
		if (shared.state != 2)
			return FALSE;

		Lock();
		BOOL pending = shared.head != NULL;
		Unlock();
		return pending;
	}

	BOOL Draw(const Rect* rect, LONG offset, DWORD frameWidth, DWORD frameHeight)
	{
		if (shared.state != 2)
			return FALSE;

		Lock();
		List* lists = shared.head;
		shared.head = shared.tail = NULL;
		shared.bytes = 0;
		Unlock();

		if (!lists && !gl.shown.valid)
			return FALSE;

		GLint program, vao, buffer, active, texture0, texture1, texture2, drawFbo, readFbo, viewport[4], scissor[4], blendSrc, blendDst;
		GLfloat clear[4];
		GLGetIntegerv(GL_CURRENT_PROGRAM, &program);
		GLGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
		GLGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
		GLGetIntegerv(GL_ACTIVE_TEXTURE, &active);
		GLGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
		GLGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
		GLGetIntegerv(GL_VIEWPORT, viewport);
		GLGetIntegerv(GL_SCISSOR_BOX, scissor);
		glGetFloatv(GL_COLOR_CLEAR_VALUE, clear);
		GLboolean blend = glIsEnabled(GL_BLEND);
		GLGetIntegerv(GL_BLEND_SRC, &blendSrc);
		GLGetIntegerv(GL_BLEND_DST, &blendDst);
		GLboolean scissorTest = glIsEnabled(GL_SCISSOR_TEST);
		GLActiveTexture(GL_TEXTURE2);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2);
		GLActiveTexture(GL_TEXTURE1);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture1);
		GLActiveTexture(GL_TEXTURE0);
		GLGetIntegerv(GL_TEXTURE_BINDING_2D, &texture0);

		BOOL shown = FALSE;
		if (InitGL())
		{
			gl.target = TWIN_NONE;
			gl.program = 0;
			GLUseProgram(0);
			GLBindVertexArray(gl.vao);
			GLBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
			GLDisable(GL_BLEND);
			GLDisable(GL_SCISSOR_TEST);
			for (List* list = lists; list; list = list->next)
				Replay(list);
			if (shared.show)
				shown = Show(rect, offset, frameWidth, frameHeight, drawFbo);
		}

		GLBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
		GLBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
		GLViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
		glScissor(scissor[0], scissor[1], scissor[2], scissor[3]);
		GLClearColor(clear[0], clear[1], clear[2], clear[3]);
		GLBlendFunc(blendSrc, blendDst);
		if (blend)
			GLEnable(GL_BLEND);
		else
			GLDisable(GL_BLEND);
		if (scissorTest)
			GLEnable(GL_SCISSOR_TEST);
		else
			GLDisable(GL_SCISSOR_TEST);
		GLActiveTexture(GL_TEXTURE2);
		GLBindTexture(GL_TEXTURE_2D, texture2);
		GLActiveTexture(GL_TEXTURE1);
		GLBindTexture(GL_TEXTURE_2D, texture1);
		GLActiveTexture(GL_TEXTURE0);
		GLBindTexture(GL_TEXTURE_2D, texture0);
		GLActiveTexture(active);
		GLBindBuffer(GL_ARRAY_BUFFER, buffer);
		GLBindVertexArray(vao);
		GLUseProgram(program);

		Lock();
		while (lists)
		{
			List* next = lists->next;
			RecycleList(lists);
			lists = next;
		}
		Unlock();
		return shown;
	}

	VOID ReleaseGL()
	{
		if (gl.ready)
		{
			for (DWORD i = 0; i < TWIN_MAX; ++i)
				if (gl.twins[i].texture || gl.twins[i].fbo)
					DestroyGpu(i);
			GLDeleteTextures(1, &gl.stage);
			if (gl.scratch)
				GLDeleteTextures(1, &gl.scratch);
			if (gl.scratchFbo)
				GLDeleteFramebuffers(1, &gl.scratchFbo);
			if (gl.maskTexture)
				GLDeleteTextures(1, &gl.maskTexture);
			GLDeleteBuffers(1, &gl.vbo);
			GLDeleteVertexArrays(1, &gl.vao);
			GLDeleteProgram(gl.copyProgram);
			GLDeleteProgram(gl.showProgram);
			GLDeleteProgram(gl.maskProgram);
		}
		if (gl.verts)
			MemoryFree(gl.verts);
		if (gl.placed)
			MemoryFree(gl.placed);
		MemoryZero(&gl, sizeof(gl));
		Ground::ReleaseGL();

		if (shared.state == 2)
		{
			Lock();
			DropQueued();
			shared.glFailed = FALSE;
			Unlock();
		}
		InterlockedIncrement(&shared.gen);
	}
}
