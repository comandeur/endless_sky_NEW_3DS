/* BatchShader.cpp
Copyright (c) 2017 by Michael Zahniser
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

#include "BatchShader.h"

#include "../image/Sprite.h"

#include "../ctr/Gfx.h"
#include "../ctr/TextureCache.h"

#include <algorithm>
#include <cmath>

using namespace std;



void BatchShader::Init()
{
}



void BatchShader::Bind()
{
}



// The data holds, for each sprite, six vertices of a triangle strip (the first
// and last are repeated to separate the sprites), each with six values:
// x, y, s, t, frame, alpha. Here each sprite becomes two triangles, and
// consecutive sprites that use the same animation frame share a draw call.
void BatchShader::Add(const Sprite *sprite, const vector<float> &data)
{
	if(!sprite || data.empty())
		return;
	const vector<C3D_Tex> *frames = nullptr;
	const vector<C3D_Tex> *masks = nullptr;
	float uMax = 1.f;
	float vMin = 0.f;
	if(!TextureCache::Get(sprite->Texture(), frames, masks, uMax, vMin))
		return;
	int count = frames->size();

	constexpr size_t STRIDE = 6;
	constexpr size_t PER_SPRITE = 6 * STRIDE;
	for(size_t i = 0; i + PER_SPRITE <= data.size(); i += PER_SPRITE)
	{
		const float *s = data.data() + i;
		float frame = s[4];
		int index = (static_cast<int>(floor(frame + .5f)) % count + count) % count;
		Gfx::Vertex *v = Gfx::Triangles(6, Gfx::Material::TEXTURE, const_cast<C3D_Tex *>(&(*frames)[index]));
		if(!v)
			return;
		// Vertices 1 to 4 are the corners, in strip order.
		Gfx::Vertex corner[4];
		for(int c = 0; c < 4; ++c)
		{
			const float *p = s + (c + 1) * STRIDE;
			corner[c].x = p[0];
			corner[c].y = p[1];
			corner[c].u = p[2] * uMax;
			// The desktop texture coordinate t = 0 is the top of the image.
			corner[c].v = 1.f - p[3] * (1.f - vMin);
			uint8_t a = static_cast<uint8_t>(clamp(p[5], 0.f, 1.f) * 255.f + .5f);
			Gfx::SetColor(corner[c], a, a, a, a);
		}
		Gfx::Quad(v, corner[0], corner[1], corner[2], corner[3]);
	}
}



void BatchShader::Unbind()
{
}
