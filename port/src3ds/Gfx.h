/* Gfx.h
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

#pragma once

#include <citro3d.h>

#include <cstdint>

class Color;



// Low level renderer for the PICA200 GPU, built on citro3d.
//
// The desktop game draws everything with small GLSL programs. The PICA200 has
// a programmable vertex stage but a fixed-function fragment stage (the "texture
// combiners"), so the port computes all geometry on the CPU, streams it into a
// per-frame vertex arena, and selects one of a few combiner setups
// ("materials"). Consecutive primitives that use the same material and
// textures are merged into a single draw call.
//
// Coordinates are always "canvas" coordinates, exactly like the desktop game:
// the origin is at the center of the current canvas and y points down. The
// canvas region that is visible on the current render target is chosen with
// SetView(), which is how the same UI can be shown scaled down on one screen
// and magnified on the other.
namespace Gfx {
	struct Vertex {
		float x;
		float y;
		float u;
		float v;
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	};

	enum class Material : uint8_t {
		// Vertex color only.
		SOLID,
		// Texture 0 multiplied by the vertex color.
		TEXTURE,
		// Vertex color multiplied by the alpha of texture 0 (fonts, fog, outlines).
		ALPHA_MASK,
		// Linear blend of textures 0 and 1 (animation frames), times the vertex color.
		FRAME_BLEND,
		// Color swizzle of texture 0 through a matrix, optionally limited by the
		// red channel of texture 2 (the swizzle mask), times the vertex color.
		SWIZZLE,
	};

	enum class Blend : uint8_t {
		// Premultiplied alpha, the blending mode of the whole game.
		PREMULTIPLIED,
		// Overwrite the destination (used to clear rectangles).
		REPLACE,
	};

	enum class Target : uint8_t {
		TOP,
		BOTTOM,
		TEXTURE,
	};

	// Parameters of the more complex materials.
	struct MaterialParams {
		// FRAME_BLEND: weight of texture 1.
		float fade = 0.f;
		// SWIZZLE: the 4x4 matrix in the layout used by the Swizzle class.
		const float *swizzle = nullptr;
		// SWIZZLE: whether texture 2 holds a swizzle mask.
		bool useMask = false;
	};


	bool Init();
	void Quit();

	// Frame handling. Everything is drawn between these two calls.
	void BeginFrame();
	void EndFrame();
	bool InFrame();

	// Select the physical screen to draw on. The previous view is kept per screen.
	void SetTarget(Target target);
	Target CurrentTarget();
	// Draw into a texture. The texture must have been created with
	// CreateRenderTexture(). Use SetTarget() to return to a screen.
	void SetTextureTarget(C3D_RenderTarget *target, int width, int height);
	// Remember the current target (and its view) to restore it later; used to
	// draw into a texture in the middle of drawing something else.
	struct TargetState {
		Target target = Target::TOP;
		C3D_RenderTarget *renderTarget = nullptr;
		int width = 0;
		int height = 0;
		float view[4] = {};
	};
	TargetState SaveTarget();
	void RestoreTarget(const TargetState &state);
	// Physical size of the current target, in pixels.
	int TargetWidth();
	int TargetHeight();

	// Choose which part of the canvas is visible on the current target.
	void SetView(float left, float top, float right, float bottom);
	void GetView(float &left, float &top, float &right, float &bottom);
	// Physical pixels per canvas unit on the current target.
	float ViewScale();

	// Fill the visible part of the current target with a color.
	void Clear(float r = 0.f, float g = 0.f, float b = 0.f, float a = 1.f);

	// Primitive submission. Returns room for the requested number of vertices
	// (forming independent triangles), or nullptr if the vertex arena is full.
	Vertex *Triangles(int vertexCount, Material material, C3D_Tex *tex0 = nullptr, C3D_Tex *tex1 = nullptr,
		C3D_Tex *tex2 = nullptr, const MaterialParams &params = MaterialParams(), Blend blend = Blend::PREMULTIPLIED);
	// Submit any pending primitives.
	void Flush();

	// Helpers.
	uint32_t PackColor(const Color &color, float alphaScale = 1.f);
	void SetColor(Vertex &v, const Color &color, float alphaScale = 1.f);
	void SetColor(Vertex &v, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
	// Write a quad (two triangles) into six vertices. Corners are given in
	// order top left, top right, bottom left, bottom right.
	void Quad(Vertex *out, const Vertex &tl, const Vertex &tr, const Vertex &bl, const Vertex &br);
	// An axis aligned rectangle of a single color.
	void FillRect(float left, float top, float right, float bottom, const Color &color);
	void FillRect(float left, float top, float right, float bottom, uint8_t r, uint8_t g, uint8_t b, uint8_t a);

	// Textures in linear memory. All allocations of linear memory made by the
	// port go through these helpers so that they are serialized.
	void *LinearAlloc(size_t size);
	void LinearFree(void *memory);
	size_t LinearFreeSpace();
	bool CreateTexture(C3D_Tex *tex, int width, int height, GPU_TEXCOLOR format, bool mipmap = false);
	void DeleteTexture(C3D_Tex *tex);
	// A texture that can also be drawn into.
	C3D_RenderTarget *CreateRenderTexture(C3D_Tex *tex, int width, int height);
	void DeleteRenderTexture(C3D_RenderTarget *target, C3D_Tex *tex);

	// Upload linear RGBA8 pixels (row 0 at the top, rows of `stride` pixels)
	// into a texture, converting to the texture's format and tiling it.
	// Supported formats: RGBA8, RGBA4, A8, L8. Rows are flipped as needed so
	// that v = 0 is the top of the image.
	void UploadPixels(C3D_Tex *tex, const uint32_t *rgba, int width, int height, int stride);
	// Same for an 8-bit single channel image into an A8 or L8 texture.
	void UploadAlpha(C3D_Tex *tex, const uint8_t *alpha, int width, int height, int stride);

	// Simple statistics for the performance display.
	int DrawCallsLastFrame();
	int VerticesLastFrame();
}
