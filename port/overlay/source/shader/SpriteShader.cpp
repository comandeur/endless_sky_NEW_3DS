/* SpriteShader.cpp
Copyright (c) 2014 by Michael Zahniser
Nintendo 3DS version copyright (c) 2026 by the Endless Sky 3DS port contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "SpriteShader.h"

#include "../image/Sprite.h"
#include "../Swizzle.h"

#include "../ctr/Gfx.h"
#include "../ctr/TextureCache.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	// Below this, blending two animation frames is not worth a separate draw call.
	constexpr float MIN_FADE = 1.f / 64.f;
}



void SpriteShader::Init()
{
}



void SpriteShader::Draw(const Sprite *sprite, const Point &position,
	float zoom, const Swizzle *swizzle, float frame, const Point &unit)
{
	if(!sprite)
		return;
	Add(Prepare(sprite, position, zoom, swizzle, frame, unit));
}



SpriteShader::Item SpriteShader::Prepare(const Sprite *sprite, const Point &position,
	float zoom, const Swizzle *swizzle, float frame, const Point &unit)
{
	if(!sprite)
		return {};

	Item item;
	item.texture = sprite->Texture();
	item.swizzleMask = sprite->SwizzleMask();
	item.frame = frame;
	item.frameCount = sprite->Frames();
	item.uniqueSwizzleMaskFrames = sprite->SwizzleMaskFrames() > 1;
	// Position.
	item.position[0] = static_cast<float>(position.X());
	item.position[1] = static_cast<float>(position.Y());
	// Rotation and scale.
	Point scaledUnit = unit * zoom;
	Point uw = scaledUnit * sprite->Width();
	Point uh = scaledUnit * sprite->Height();
	item.transform[0] = static_cast<float>(-uw.Y());
	item.transform[1] = static_cast<float>(uw.X());
	item.transform[2] = static_cast<float>(-uh.X());
	item.transform[3] = static_cast<float>(-uh.Y());
	// Swizzle.
	item.swizzle = swizzle;
	return item;
}



void SpriteShader::Bind()
{
}



// Motion blur is not drawn on the 3DS: the screens are small and the extra
// texture samples would cost too much.
void SpriteShader::Add(const Item &item, bool withBlur)
{
	if(!item.texture || item.alpha <= 0.f)
		return;
	const vector<C3D_Tex> *frames = nullptr;
	const vector<C3D_Tex> *masks = nullptr;
	float uMax = 1.f;
	float vMin = 0.f;
	if(!TextureCache::Get(item.texture, frames, masks, uMax, vMin))
		return;

	int count = frames->size();
	float frame = item.frame;
	if(!(frame >= 0.f))
		frame = 0.f;
	int first = static_cast<int>(floor(frame)) % count;
	float fade = frame - floor(frame);
	int second = (first + 1) % count;

	// Pick the combiner setup.
	Gfx::Material material = Gfx::Material::TEXTURE;
	Gfx::MaterialParams params;
	C3D_Tex *tex0 = const_cast<C3D_Tex *>(&(*frames)[first]);
	C3D_Tex *tex1 = nullptr;
	C3D_Tex *tex2 = nullptr;
	if(item.swizzle && !item.swizzle->IsIdentity())
	{
		// The swizzle needs every combiner stage, so animation frames are not
		// blended: use the nearest one instead.
		if(fade >= .5f && count > 1)
			tex0 = const_cast<C3D_Tex *>(&(*frames)[second]);
		material = Gfx::Material::SWIZZLE;
		params.swizzle = item.swizzle->MatrixPtr();
		bool useMask = !item.swizzle->OverrideMask() && item.swizzleMask && !masks->empty();
		if(useMask)
		{
			int maskFrame = item.uniqueSwizzleMaskFrames ? min<int>(first, masks->size() - 1) : 0;
			tex2 = const_cast<C3D_Tex *>(&(*masks)[maskFrame]);
			params.useMask = true;
		}
	}
	else if(fade > MIN_FADE && count > 1)
	{
		material = Gfx::Material::FRAME_BLEND;
		tex1 = const_cast<C3D_Tex *>(&(*frames)[second]);
		params.fade = fade;
	}

	Gfx::Vertex *v = Gfx::Triangles(6, material, tex0, tex1, tex2, params);
	if(!v)
		return;

	// The sprite is the unit square around the origin, transformed by the
	// matrix (column major, like the desktop shader) and moved into place.
	const float *t = item.transform;
	float px = item.position[0];
	float py = item.position[1];
	auto corner = [&](float x, float y, float u, float vv) -> Gfx::Vertex
	{
		Gfx::Vertex out;
		out.x = t[0] * x + t[2] * y + px;
		out.y = t[1] * x + t[3] * y + py;
		out.u = u;
		out.v = vv;
		return out;
	};
	// Texture row 0 (the top of the image) is at v = 1. The clip shortens the
	// part of the image that is stretched over the sprite.
	float vTop = 1.f;
	float vBottom = 1.f - (1.f - vMin) * min(1.f, max(0.f, item.clip));
	Gfx::Vertex tl = corner(-.5f, -.5f, 0.f, vTop);
	Gfx::Vertex tr = corner(.5f, -.5f, uMax, vTop);
	Gfx::Vertex bl = corner(-.5f, .5f, 0.f, vBottom);
	Gfx::Vertex br = corner(.5f, .5f, uMax, vBottom);
	uint8_t a = static_cast<uint8_t>(min(1.f, item.alpha) * 255.f + .5f);
	for(Gfx::Vertex *c : {&tl, &tr, &bl, &br})
		Gfx::SetColor(*c, a, a, a, a);
	Gfx::Quad(v, tl, tr, bl, br);
}



void SpriteShader::Unbind()
{
}
