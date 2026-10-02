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


using namespace std;

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



RenderBuffer::RenderBuffer(const Point &dimensions)
	: size(dimensions)
{
}



RenderBuffer::~RenderBuffer()
{
}



// Start recording the contents of the buffer. Its coordinates have their
// origin at its center, like a small screen.
RenderBuffer::RenderTargetGuard RenderBuffer::SetTarget()
{
	Gfx::BeginRecording(commands, -.5f * size.X(), -.5f * size.Y(), .5f * size.X(), .5f * size.Y());
	active = true;
	return RenderTargetGuard(*this, size.X(), size.Y());
}



void RenderBuffer::Deactivate()
{
	if(!active)
		return;
	Gfx::EndRecording();
	active = false;
}



void RenderBuffer::Draw(const Point &position)
{
	Draw(position, size);
}



// Draw part of the buffer (starting at `srcposition` relative to its top left
// corner, of size `clipsize`) centered on `position`. The desktop version fades
// the edges out over the fade padding; here they are simply clipped.
void RenderBuffer::Draw(const Point &position, const Point &clipsize, const Point &srcposition)
{
	if(clipsize.X() <= 0. || clipsize.Y() <= 0.)
		return;
	// A point p of the buffer is drawn at p + size / 2 - srcposition + (position - clipsize / 2).
	Point offset = .5 * size - srcposition + position - .5 * clipsize;
	float clip[4] = {
		static_cast<float>(position.X() - .5 * clipsize.X()),
		static_cast<float>(position.Y() - .5 * clipsize.Y()),
		static_cast<float>(position.X() + .5 * clipsize.X()),
		static_cast<float>(position.Y() + .5 * clipsize.Y())};
	Gfx::Replay(commands, offset.X(), offset.Y(), clip);
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
