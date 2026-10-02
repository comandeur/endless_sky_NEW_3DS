/* RenderBuffer.cpp
Copyright (c) 2023 by thewierdnut
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

#include "RenderBuffer.h"

#include "Logger.h"
#include "Screen.h"

#include <algorithm>

using namespace std;

namespace {
	int NextPowerOfTwo(int value)
	{
		int result = 8;
		while(result < value && result < 1024)
			result <<= 1;
		return result;
	}
}



void RenderBuffer::Init()
{
}



RenderBuffer::RenderTargetGuard::~RenderTargetGuard()
{
	Deactivate();
}



void RenderBuffer::RenderTargetGuard::Deactivate()
{
	buffer.Deactivate();
	screenGuard.Deactivate();
}



RenderBuffer::RenderTargetGuard::RenderTargetGuard(RenderBuffer &b, int screenWidth, int screenHeight)
	: buffer(b), screenGuard(screenWidth, screenHeight)
{
}



// Create a texture of the given size that can be used as a render target.
RenderBuffer::RenderBuffer(const Point &dimensions)
	: size(dimensions)
{
	double largest = max(size.X(), size.Y());
	scale = largest > 1024. ? static_cast<float>(1024. / largest) : 1.f;
	int width = NextPowerOfTwo(ceil(size.X() * scale));
	int height = NextPowerOfTwo(ceil(size.Y() * scale));
	target = Gfx::CreateRenderTexture(&texture, width, height);
	if(!target)
		Logger::Log("Failed to create a render buffer.", Logger::Level::WARNING);
}



RenderBuffer::~RenderBuffer()
{
	Gfx::DeleteRenderTexture(target, &texture);
}



RenderBuffer::RenderTargetGuard RenderBuffer::SetTarget()
{
	if(target && Gfx::InFrame())
	{
		previous = Gfx::SaveTarget();
		Gfx::SetTextureTarget(target, texture.width, texture.height);
		// The buffer's coordinates have their origin at its center; its top
		// left corner is the top left of the texture.
		float left = -.5f * size.X();
		float top = -.5f * size.Y();
		Gfx::SetView(left, top, left + texture.width / scale, top + texture.height / scale);
		active = true;
	}
	return RenderTargetGuard(*this, size.X(), size.Y());
}



void RenderBuffer::Deactivate()
{
	if(!active)
		return;
	Gfx::RestoreTarget(previous);
	active = false;
}



void RenderBuffer::Draw(const Point &position)
{
	Draw(position, size);
}



// Draw part of the buffer (starting at `srcposition` relative to its top left
// corner, of size `clipsize`) centered on `position`. The edges fade out over
// the fade padding, like the desktop shader.
void RenderBuffer::Draw(const Point &position, const Point &clipsize, const Point &srcposition)
{
	if(!target || clipsize.X() <= 0. || clipsize.Y() <= 0.)
		return;

	const float w = clipsize.X();
	const float h = clipsize.Y();
	const float x0 = position.X() - .5f * w;
	const float y0 = position.Y() - .5f * h;
	const float texW = texture.width;
	const float texH = texture.height;

	// Grid lines of a 3x3 mesh: the outer cells are the fading margins.
	// The padding is stored as top, bottom, left, right.
	float xs[4] = {0.f, min(fadePadding[2], w * .5f), w - min(fadePadding[3], w * .5f), w};
	float ys[4] = {0.f, min(fadePadding[0], h * .5f), h - min(fadePadding[1], h * .5f), h};
	bool fadeX[4] = {fadePadding[2] > 0.f, false, false, fadePadding[3] > 0.f};
	bool fadeY[4] = {fadePadding[0] > 0.f, false, false, fadePadding[1] > 0.f};

	Gfx::Vertex *v = Gfx::Triangles(9 * 6, Gfx::Material::TEXTURE, &texture);
	if(!v)
		return;
	auto vertex = [&](int i, int j) -> Gfx::Vertex
	{
		Gfx::Vertex out;
		out.x = x0 + xs[i];
		out.y = y0 + ys[j];
		out.u = (srcposition.X() + xs[i]) * scale / texW;
		out.v = 1.f - (srcposition.Y() + ys[j]) * scale / texH;
		uint8_t a = (fadeX[i] || fadeY[j]) ? 0 : 255;
		Gfx::SetColor(out, a, a, a, a);
		return out;
	};
	for(int j = 0; j < 3; ++j)
		for(int i = 0; i < 3; ++i)
		{
			Gfx::Quad(v, vertex(i, j), vertex(i + 1, j), vertex(i, j + 1), vertex(i + 1, j + 1));
			v += 6;
		}
}



double RenderBuffer::Top() const
{
	return -size.Y() / 2;
}



double RenderBuffer::Bottom() const
{
	return size.Y() / 2;
}



double RenderBuffer::Left() const
{
	return -size.X() / 2;
}



double RenderBuffer::Right() const
{
	return size.X() / 2;
}



const Point &RenderBuffer::Dimensions() const
{
	return size;
}



double RenderBuffer::Height() const
{
	return size.Y();
}



double RenderBuffer::Width() const
{
	return size.X();
}



void RenderBuffer::SetFadePadding(float top, float bottom, float left, float right)
{
	fadePadding[0] = top;
	fadePadding[1] = bottom;
	fadePadding[2] = left;
	fadePadding[3] = right;
}
