/* OutlineShader.cpp
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

#include "OutlineShader.h"

#include "../Color.h"
#include "../image/Sprite.h"

#include "../ctr/Gfx.h"
#include "../ctr/TextureCache.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	void Silhouette(C3D_Tex *tex, float uMax, float vMin, const Point &pos, const Point &size, const Point &unit,
		const Gfx::Vertex &color)
	{
		Gfx::Vertex *v = Gfx::Triangles(6, Gfx::Material::ALPHA_MASK, tex);
		if(!v)
			return;
		Point uw = unit * size.X();
		Point uh = unit * size.Y();
		float t[4] = {
			static_cast<float>(-uw.Y()), static_cast<float>(uw.X()),
			static_cast<float>(-uh.X()), static_cast<float>(-uh.Y())};
		auto corner = [&](float x, float y, float u, float vv) -> Gfx::Vertex
		{
			Gfx::Vertex out = color;
			out.x = t[0] * x + t[2] * y + static_cast<float>(pos.X());
			out.y = t[1] * x + t[3] * y + static_cast<float>(pos.Y());
			out.u = u;
			out.v = vv;
			return out;
		};
		Gfx::Quad(v, corner(-.5f, -.5f, 0.f, 1.f), corner(.5f, -.5f, uMax, 1.f),
			corner(-.5f, .5f, 0.f, vMin), corner(.5f, .5f, uMax, vMin));
	}
}



void OutlineShader::Init()
{
}



// The desktop game finds the edges of the sprite with a Sobel filter, which the
// fixed-function PICA200 cannot do. Instead, the sprite's silhouette is drawn
// in the outline color, and a slightly smaller dark silhouette on top of it
// leaves a colored rim that reads as an outline.
void OutlineShader::Draw(const Sprite *sprite, const Point &pos, const Point &size,
	const Color &color, const Point &unit, float frame)
{
	if(!sprite)
		return;
	const vector<C3D_Tex> *frames = nullptr;
	const vector<C3D_Tex> *masks = nullptr;
	float uMax = 1.f;
	float vMin = 0.f;
	if(!TextureCache::Get(sprite->Texture(), frames, masks, uMax, vMin))
		return;
	int index = (static_cast<int>(floor(frame + .5f)) % static_cast<int>(frames->size()) + frames->size())
		% frames->size();
	C3D_Tex *tex = const_cast<C3D_Tex *>(&(*frames)[index]);

	Gfx::Vertex rim{};
	Gfx::SetColor(rim, color);
	Silhouette(tex, uMax, vMin, pos, size, unit, rim);

	double smallest = min(fabs(size.X()), fabs(size.Y()));
	if(smallest > 6.)
	{
		double shrink = 1. - 3. / smallest;
		Gfx::Vertex dark{};
		Gfx::SetColor(dark, 0, 0, 0, static_cast<uint8_t>(rim.a * .8f));
		Silhouette(tex, uMax, vMin, pos, size * shrink, unit, dark);
	}
}
