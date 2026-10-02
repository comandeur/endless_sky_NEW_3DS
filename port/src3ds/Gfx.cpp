/* Gfx.cpp
Copyright (c) 2026 by the Endless Sky 3DS port contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "Gfx.h"

#include "../Color.h"
#include "../Logger.h"

#include "es_shbin.h"

#include <3ds.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

using namespace std;

namespace {
	// Physical sizes of the two screens.
	constexpr int TOP_WIDTH = 400;
	constexpr int BOTTOM_WIDTH = 320;
	constexpr int SCREEN_HEIGHT = 240;

	constexpr u32 DISPLAY_TRANSFER_FLAGS = GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0)
		| GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)
		| GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);

	// The command buffer has to hold every draw call of a frame.
	constexpr size_t COMMAND_BUFFER_SIZE = 0x200000;
	// Vertex arenas: one per frame in flight.
	constexpr int ARENA_COUNT = 2;
	constexpr int ARENA_VERTICES = 48 * 1024;

	DVLB_s *shaderDvlb = nullptr;
	shaderProgram_s program;
	int projectionLocation = -1;

	C3D_RenderTarget *screens[2] = {nullptr, nullptr};

	Gfx::Vertex *arenas[ARENA_COUNT] = {};
	int arenaIndex = 0;
	int arenaUsed = 0;
	bool warnedFull = false;

	bool inFrame = false;
	Gfx::Target currentTarget = Gfx::Target::TOP;
	int targetWidth = TOP_WIDTH;
	int targetHeight = SCREEN_HEIGHT;
	bool targetIsTexture = false;
	C3D_RenderTarget *textureTarget = nullptr;

	struct ViewRect {
		float left = -200.f;
		float top = -120.f;
		float right = 200.f;
		float bottom = 120.f;
	};
	ViewRect views[3];

	// State of the pending batch of triangles.
	struct BatchKey {
		Gfx::Material material = Gfx::Material::SOLID;
		Gfx::Blend blend = Gfx::Blend::PREMULTIPLIED;
		C3D_Tex *tex[3] = {nullptr, nullptr, nullptr};
		float fade = 0.f;
		const float *swizzle = nullptr;
		bool useMask = false;

		bool operator==(const BatchKey &other) const
		{
			return material == other.material && blend == other.blend && tex[0] == other.tex[0]
				&& tex[1] == other.tex[1] && tex[2] == other.tex[2] && fade == other.fade
				&& swizzle == other.swizzle && useMask == other.useMask;
		}
	};
	BatchKey pending;
	int pendingFirst = 0;
	int pendingCount = 0;

	// The state that was last sent to citro3d.
	BatchKey applied;
	bool appliedValid = false;

	int drawCalls = 0;
	int lastDrawCalls = 0;
	int lastVertices = 0;

	mutex linearMutex;


	u8 ToByte(float value)
	{
		return static_cast<u8>(clamp(value, 0.f, 1.f) * 255.f + .5f);
	}


	u32 ConstantColor(float r, float g, float b, float a)
	{
		return ToByte(r) | (ToByte(g) << 8) | (ToByte(b) << 16) | (ToByte(a) << 24);
	}


	void ResetStage(int id)
	{
		C3D_TexEnv *env = C3D_GetTexEnv(id);
		C3D_TexEnvInit(env);
	}


	// Configure the texture combiners and blending for the given batch.
	void ApplyState(const BatchKey &key)
	{
		if(appliedValid && applied == key)
			return;

		bool sameCombiner = appliedValid && applied.material == key.material && applied.fade == key.fade
			&& applied.swizzle == key.swizzle && applied.useMask == key.useMask;

		for(int i = 0; i < 3; ++i)
			if(key.tex[i] && (!appliedValid || applied.tex[i] != key.tex[i]))
				C3D_TexBind(i, key.tex[i]);

		if(!appliedValid || applied.blend != key.blend)
		{
			if(key.blend == Gfx::Blend::REPLACE)
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
			else
				C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA,
					GPU_ONE, GPU_ONE_MINUS_SRC_ALPHA);
		}

		applied = key;
		appliedValid = true;
		if(sameCombiner)
			return;

		for(int i = 0; i < 6; ++i)
			ResetStage(i);

		C3D_TexEnv *env = C3D_GetTexEnv(0);
		switch(key.material)
		{
			case Gfx::Material::SOLID:
				C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR);
				C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
				break;
			case Gfx::Material::TEXTURE:
				C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR);
				C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
				break;
			case Gfx::Material::ALPHA_MASK:
				C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_TEXTURE0);
				C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
				C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
				C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
				break;
			case Gfx::Material::FRAME_BLEND:
			{
				// result = tex1 * fade + tex0 * (1 - fade)
				C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE1, GPU_TEXTURE0, GPU_CONSTANT);
				C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_ALPHA);
				C3D_TexEnvOpAlpha(env, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA, GPU_TEVOP_A_SRC_ALPHA);
				C3D_TexEnvFunc(env, C3D_Both, GPU_INTERPOLATE);
				C3D_TexEnvColor(env, ConstantColor(key.fade, key.fade, key.fade, key.fade));

				env = C3D_GetTexEnv(1);
				C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_PRIMARY_COLOR);
				C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
				break;
			}
			case Gfx::Material::SWIZZLE:
			{
				// The combiners cannot do a matrix product directly, so build it
				// one input channel at a time: each stage adds the contribution of
				// one channel of the texture, broadcast to all three components and
				// scaled per output channel by a constant color.
				const float *m = key.swizzle;
				// Input channel i contributes m[j * 4 + i] to output channel j.
				auto column = [m](int i) -> u32
				{
					return ConstantColor(m[i], m[4 + i], m[8 + i], 1.f);
				};
				static const GPU_TEVOP_RGB CHANNEL[3] = {
					GPU_TEVOP_RGB_SRC_R, GPU_TEVOP_RGB_SRC_G, GPU_TEVOP_RGB_SRC_B};
				for(int i = 0; i < 3; ++i)
				{
					env = C3D_GetTexEnv(i);
					if(!i)
					{
						C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT);
						C3D_TexEnvOpRgb(env, CHANNEL[i], GPU_TEVOP_RGB_SRC_COLOR);
						C3D_TexEnvFunc(env, C3D_RGB, GPU_MODULATE);
						C3D_TexEnvSrc(env, C3D_Alpha, GPU_TEXTURE0);
						C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
					}
					else
					{
						C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS);
						C3D_TexEnvOpRgb(env, CHANNEL[i], GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR);
						C3D_TexEnvFunc(env, C3D_RGB, GPU_MULTIPLY_ADD);
						C3D_TexEnvSrc(env, C3D_Alpha, GPU_PREVIOUS);
						C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
					}
					C3D_TexEnvColor(env, column(i));
				}
				int next = 3;
				if(key.useMask)
				{
					// Where the mask is white the original color is kept:
					// result = tex0 * mask + swizzled * (1 - mask)
					env = C3D_GetTexEnv(next++);
					C3D_TexEnvSrc(env, C3D_RGB, GPU_TEXTURE0, GPU_PREVIOUS, GPU_TEXTURE2);
					C3D_TexEnvOpRgb(env, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_R);
					C3D_TexEnvFunc(env, C3D_RGB, GPU_INTERPOLATE);
					C3D_TexEnvSrc(env, C3D_Alpha, GPU_PREVIOUS);
					C3D_TexEnvFunc(env, C3D_Alpha, GPU_REPLACE);
				}
				env = C3D_GetTexEnv(next);
				C3D_TexEnvSrc(env, C3D_Both, GPU_PREVIOUS, GPU_PRIMARY_COLOR);
				C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
				break;
			}
		}
	}


	void ApplyProjection()
	{
		const ViewRect &view = views[static_cast<int>(currentTarget)];
		C3D_Mtx projection;
		if(targetIsTexture)
			Mtx_Ortho(&projection, view.left, view.right, view.bottom, view.top, -1.f, 1.f, true);
		else
			Mtx_OrthoTilt(&projection, view.left, view.right, view.bottom, view.top, -1.f, 1.f, true);
		C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projectionLocation, &projection);
	}


	void SetupAttributes()
	{
		C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
		AttrInfo_Init(attrInfo);
		AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 2);
		AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 2);
		AttrInfo_AddLoader(attrInfo, 2, GPU_UNSIGNED_BYTE, 4);
	}


	void BindArena()
	{
		C3D_BufInfo *bufInfo = C3D_GetBufInfo();
		BufInfo_Init(bufInfo);
		BufInfo_Add(bufInfo, arenas[arenaIndex], sizeof(Gfx::Vertex), 3, 0x210);
	}


	// Morton order of a pixel within an 8x8 tile.
	inline u32 Morton(u32 x, u32 y)
	{
		static const u8 TABLE[8] = {0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15};
		return TABLE[x] | (TABLE[y] << 1);
	}


	inline u32 TiledOffset(u32 x, u32 y, u32 width)
	{
		return (((y >> 3) * (width >> 3) + (x >> 3)) << 6) + Morton(x & 7, y & 7);
	}
}



bool Gfx::Init()
{
	gfxInitDefault();
	gfxSet3D(false);
	if(!C3D_Init(COMMAND_BUFFER_SIZE))
		return false;

	screens[0] = C3D_RenderTargetCreate(SCREEN_HEIGHT, TOP_WIDTH, GPU_RB_RGBA8, C3D_DEPTHTYPE(-1));
	C3D_RenderTargetSetOutput(screens[0], GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);
	screens[1] = C3D_RenderTargetCreate(SCREEN_HEIGHT, BOTTOM_WIDTH, GPU_RB_RGBA8, C3D_DEPTHTYPE(-1));
	C3D_RenderTargetSetOutput(screens[1], GFX_BOTTOM, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

	shaderDvlb = DVLB_ParseFile(reinterpret_cast<u32 *>(const_cast<u8 *>(es_shbin)), es_shbin_size);
	shaderProgramInit(&program);
	shaderProgramSetVsh(&program, &shaderDvlb->DVLE[0]);
	C3D_BindProgram(&program);
	projectionLocation = shaderInstanceGetUniformLocation(program.vertexShader, "projection");

	SetupAttributes();

	for(Vertex *&arena : arenas)
	{
		arena = static_cast<Vertex *>(LinearAlloc(ARENA_VERTICES * sizeof(Vertex)));
		if(!arena)
			return false;
	}

	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
	C3D_CullFace(GPU_CULL_NONE);
	C3D_AlphaTest(false, GPU_ALWAYS, 0);

	views[0] = {-TOP_WIDTH * .5f, -SCREEN_HEIGHT * .5f, TOP_WIDTH * .5f, SCREEN_HEIGHT * .5f};
	views[1] = {-BOTTOM_WIDTH * .5f, -SCREEN_HEIGHT * .5f, BOTTOM_WIDTH * .5f, SCREEN_HEIGHT * .5f};
	return true;
}



void Gfx::Quit()
{
	for(Vertex *&arena : arenas)
	{
		LinearFree(arena);
		arena = nullptr;
	}
	shaderProgramFree(&program);
	if(shaderDvlb)
		DVLB_Free(shaderDvlb);
	for(C3D_RenderTarget *&target : screens)
	{
		if(target)
			C3D_RenderTargetDelete(target);
		target = nullptr;
	}
	C3D_Fini();
	gfxExit();
}



void Gfx::BeginFrame()
{
	if(inFrame)
		return;
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
	inFrame = true;

	arenaIndex = (arenaIndex + 1) % ARENA_COUNT;
	arenaUsed = 0;
	pendingCount = 0;
	appliedValid = false;
	drawCalls = 0;

	// citro3d forgets the bound program, attributes and buffers between frames.
	C3D_BindProgram(&program);
	SetupAttributes();
	BindArena();
	C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
	C3D_CullFace(GPU_CULL_NONE);

	for(C3D_RenderTarget *target : screens)
		C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0x000000FF, 0);

	SetTarget(Target::TOP);
}



void Gfx::EndFrame()
{
	if(!inFrame)
		return;
	Flush();
	C3D_FrameEnd(0);
	inFrame = false;
	lastDrawCalls = drawCalls;
	lastVertices = arenaUsed;
}



bool Gfx::InFrame()
{
	return inFrame;
}



void Gfx::SetTarget(Target target)
{
	if(target == Target::TEXTURE)
		return;
	Flush();
	currentTarget = target;
	targetIsTexture = false;
	targetWidth = (target == Target::TOP ? TOP_WIDTH : BOTTOM_WIDTH);
	targetHeight = SCREEN_HEIGHT;
	if(inFrame)
	{
		C3D_FrameDrawOn(screens[static_cast<int>(target)]);
		ApplyProjection();
	}
}



Gfx::Target Gfx::CurrentTarget()
{
	return currentTarget;
}



void Gfx::SetTextureTarget(C3D_RenderTarget *target, int width, int height)
{
	Flush();
	currentTarget = Target::TEXTURE;
	targetIsTexture = true;
	textureTarget = target;
	targetWidth = width;
	targetHeight = height;
	views[2] = {-width * .5f, -height * .5f, width * .5f, height * .5f};
	if(inFrame)
	{
		C3D_RenderTargetClear(target, C3D_CLEAR_ALL, 0, 0);
		C3D_FrameDrawOn(target);
		ApplyProjection();
	}
}



Gfx::TargetState Gfx::SaveTarget()
{
	TargetState state;
	state.target = currentTarget;
	state.renderTarget = (currentTarget == Target::TEXTURE ? textureTarget : nullptr);
	state.width = targetWidth;
	state.height = targetHeight;
	const ViewRect &view = views[static_cast<int>(currentTarget)];
	state.view[0] = view.left;
	state.view[1] = view.top;
	state.view[2] = view.right;
	state.view[3] = view.bottom;
	return state;
}



void Gfx::RestoreTarget(const TargetState &state)
{
	Flush();
	if(state.target == Target::TEXTURE && state.renderTarget)
	{
		currentTarget = Target::TEXTURE;
		targetIsTexture = true;
		textureTarget = state.renderTarget;
		targetWidth = state.width;
		targetHeight = state.height;
		if(inFrame)
			C3D_FrameDrawOn(state.renderTarget);
	}
	else
		SetTarget(state.target);
	SetView(state.view[0], state.view[1], state.view[2], state.view[3]);
}



int Gfx::TargetWidth()
{
	return targetWidth;
}



int Gfx::TargetHeight()
{
	return targetHeight;
}



void Gfx::SetView(float left, float top, float right, float bottom)
{
	Flush();
	views[static_cast<int>(currentTarget)] = {left, top, right, bottom};
	if(inFrame)
		ApplyProjection();
}



void Gfx::GetView(float &left, float &top, float &right, float &bottom)
{
	const ViewRect &view = views[static_cast<int>(currentTarget)];
	left = view.left;
	top = view.top;
	right = view.right;
	bottom = view.bottom;
}



float Gfx::ViewScale()
{
	const ViewRect &view = views[static_cast<int>(currentTarget)];
	return targetWidth / max(1.f, view.right - view.left);
}



void Gfx::Clear(float r, float g, float b, float a)
{
	const ViewRect &view = views[static_cast<int>(currentTarget)];
	Vertex *v = Triangles(6, Material::SOLID, nullptr, nullptr, nullptr, MaterialParams(), Blend::REPLACE);
	if(!v)
		return;
	Vertex corner{};
	SetColor(corner, ToByte(r), ToByte(g), ToByte(b), ToByte(a));
	Vertex tl = corner, tr = corner, bl = corner, br = corner;
	tl.x = bl.x = view.left;
	tr.x = br.x = view.right;
	tl.y = tr.y = view.top;
	bl.y = br.y = view.bottom;
	Quad(v, tl, tr, bl, br);
}



Gfx::Vertex *Gfx::Triangles(int vertexCount, Material material, C3D_Tex *tex0, C3D_Tex *tex1,
	C3D_Tex *tex2, const MaterialParams &params, Blend blend)
{
	if(!inFrame || vertexCount <= 0)
		return nullptr;
	if(arenaUsed + vertexCount > ARENA_VERTICES)
	{
		if(!warnedFull)
		{
			Logger::Log("Gfx: vertex arena is full; some primitives were dropped.", Logger::Level::WARNING);
			warnedFull = true;
		}
		return nullptr;
	}

	BatchKey key;
	key.material = material;
	key.blend = blend;
	key.tex[0] = tex0;
	key.tex[1] = tex1;
	key.tex[2] = tex2;
	if(material == Material::FRAME_BLEND)
		key.fade = params.fade;
	if(material == Material::SWIZZLE)
	{
		key.swizzle = params.swizzle;
		key.useMask = params.useMask && tex2;
	}

	if(!pendingCount || !(key == pending))
	{
		Flush();
		pending = key;
		pendingFirst = arenaUsed;
	}
	Vertex *result = arenas[arenaIndex] + arenaUsed;
	arenaUsed += vertexCount;
	pendingCount += vertexCount;
	return result;
}



void Gfx::Flush()
{
	if(!pendingCount)
		return;
	ApplyState(pending);
	C3D_DrawArrays(GPU_TRIANGLES, pendingFirst, pendingCount);
	++drawCalls;
	pendingCount = 0;
}



uint32_t Gfx::PackColor(const Color &color, float alphaScale)
{
	const float *c = color.Get();
	return ToByte(c[0] * alphaScale) | (ToByte(c[1] * alphaScale) << 8) | (ToByte(c[2] * alphaScale) << 16)
		| (ToByte(c[3] * alphaScale) << 24);
}



void Gfx::SetColor(Vertex &v, const Color &color, float alphaScale)
{
	const float *c = color.Get();
	v.r = ToByte(c[0] * alphaScale);
	v.g = ToByte(c[1] * alphaScale);
	v.b = ToByte(c[2] * alphaScale);
	v.a = ToByte(c[3] * alphaScale);
}



void Gfx::SetColor(Vertex &v, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	v.r = r;
	v.g = g;
	v.b = b;
	v.a = a;
}



void Gfx::Quad(Vertex *out, const Vertex &tl, const Vertex &tr, const Vertex &bl, const Vertex &br)
{
	out[0] = tl;
	out[1] = bl;
	out[2] = tr;
	out[3] = tr;
	out[4] = bl;
	out[5] = br;
}



void Gfx::FillRect(float left, float top, float right, float bottom, const Color &color)
{
	Vertex corner{};
	SetColor(corner, color);
	FillRect(left, top, right, bottom, corner.r, corner.g, corner.b, corner.a);
}



void Gfx::FillRect(float left, float top, float right, float bottom, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	Vertex *v = Triangles(6, Material::SOLID);
	if(!v)
		return;
	Vertex corner{};
	SetColor(corner, r, g, b, a);
	Vertex tl = corner, tr = corner, bl = corner, br = corner;
	tl.x = bl.x = left;
	tr.x = br.x = right;
	tl.y = tr.y = top;
	bl.y = br.y = bottom;
	Quad(v, tl, tr, bl, br);
}



void *Gfx::LinearAlloc(size_t size)
{
	lock_guard<mutex> lock(linearMutex);
	return linearAlloc(size);
}



void Gfx::LinearFree(void *memory)
{
	if(!memory)
		return;
	lock_guard<mutex> lock(linearMutex);
	linearFree(memory);
}



size_t Gfx::LinearFreeSpace()
{
	lock_guard<mutex> lock(linearMutex);
	return linearSpaceFree();
}



bool Gfx::CreateTexture(C3D_Tex *tex, int width, int height, GPU_TEXCOLOR format, bool mipmap)
{
	bool success;
	{
		lock_guard<mutex> lock(linearMutex);
		success = mipmap ? C3D_TexInitMipmap(tex, width, height, format) : C3D_TexInit(tex, width, height, format);
	}
	if(!success)
		return false;
	C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
	if(mipmap)
		C3D_TexSetFilterMipmap(tex, GPU_LINEAR);
	C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	return true;
}



void Gfx::DeleteTexture(C3D_Tex *tex)
{
	if(!tex || !tex->data)
		return;
	// Make sure that no pending draw call still refers to this texture.
	if(pendingCount && (pending.tex[0] == tex || pending.tex[1] == tex || pending.tex[2] == tex))
		Flush();
	if(appliedValid && (applied.tex[0] == tex || applied.tex[1] == tex || applied.tex[2] == tex))
		appliedValid = false;
	lock_guard<mutex> lock(linearMutex);
	C3D_TexDelete(tex);
	tex->data = nullptr;
}



C3D_RenderTarget *Gfx::CreateRenderTexture(C3D_Tex *tex, int width, int height)
{
	{
		// Prefer the (faster) video memory, but fall back to the main memory.
		lock_guard<mutex> lock(linearMutex);
		if(!C3D_TexInitVRAM(tex, width, height, GPU_RGBA8) && !C3D_TexInit(tex, width, height, GPU_RGBA8))
			return nullptr;
	}
	C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
	C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
	return C3D_RenderTargetCreateFromTex(tex, GPU_TEXFACE_2D, 0, C3D_DEPTHTYPE(-1));
}



void Gfx::DeleteRenderTexture(C3D_RenderTarget *target, C3D_Tex *tex)
{
	Flush();
	if(target)
		C3D_RenderTargetDelete(target);
	appliedValid = false;
	if(tex && tex->data)
	{
		lock_guard<mutex> lock(linearMutex);
		C3D_TexDelete(tex);
		tex->data = nullptr;
	}
}



void Gfx::UploadPixels(C3D_Tex *tex, const uint32_t *rgba, int width, int height, int stride)
{
	const int texWidth = tex->width;
	const int texHeight = tex->height;
	width = min(width, texWidth);
	height = min(height, texHeight);
	const GPU_TEXCOLOR format = tex->fmt;
	u8 *data = static_cast<u8 *>(tex->data);
	memset(data, 0, tex->size);

	// The source pixels are in memory order R, G, B, A (little endian ABGR).
	for(int y = 0; y < height; ++y)
	{
		const uint32_t *row = rgba + static_cast<size_t>(y) * stride;
		for(int x = 0; x < width; ++x)
		{
			uint32_t p = row[x];
			u8 r = p & 0xFF;
			u8 g = (p >> 8) & 0xFF;
			u8 b = (p >> 16) & 0xFF;
			u8 a = p >> 24;
			u32 offset = TiledOffset(x, y, texWidth);
			switch(format)
			{
				case GPU_RGBA8:
					reinterpret_cast<u32 *>(data)[offset] = (r << 24) | (g << 16) | (b << 8) | a;
					break;
				case GPU_RGBA4:
					reinterpret_cast<u16 *>(data)[offset] = ((r >> 4) << 12) | ((g >> 4) << 8) | ((b >> 4) << 4) | (a >> 4);
					break;
				case GPU_A8:
					data[offset] = a;
					break;
				case GPU_L8:
					data[offset] = r;
					break;
				default:
					break;
			}
		}
	}
	if(tex->maxLevel > 0)
		C3D_TexGenerateMipmap(tex, GPU_TEXFACE_2D);
	C3D_TexFlush(tex);
}



void Gfx::UploadAlpha(C3D_Tex *tex, const uint8_t *alpha, int width, int height, int stride)
{
	const int texWidth = tex->width;
	width = min<int>(width, texWidth);
	height = min<int>(height, tex->height);
	u8 *data = static_cast<u8 *>(tex->data);
	memset(data, 0, tex->size);
	for(int y = 0; y < height; ++y)
	{
		const uint8_t *row = alpha + static_cast<size_t>(y) * stride;
		for(int x = 0; x < width; ++x)
			data[TiledOffset(x, y, texWidth)] = row[x];
	}
	C3D_TexFlush(tex);
}



int Gfx::DrawCallsLastFrame()
{
	return lastDrawCalls;
}



int Gfx::VerticesLastFrame()
{
	return lastVertices;
}



// The few panels that clear the screen before drawing a full-screen
// background call this OpenGL function (see opengl.h).
void glClear(unsigned int)
{
	Gfx::Clear();
}
