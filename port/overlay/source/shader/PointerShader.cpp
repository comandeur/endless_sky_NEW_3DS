/* PointerShader.cpp
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

#include "PointerShader.h"

#include "../Color.h"
#include "../Point.h"

#include "../ctr/Gfx.h"

using namespace std;



void PointerShader::Init()
{
}



void PointerShader::Draw(const Point &center, const Point &angle,
	float width, float height, float offset, const Color &color)
{
	Add(center, angle, width, height, offset, color);
}



void PointerShader::Bind()
{
}



// The pointer is a triangle whose tip is `offset` away from the center, in the
// direction of `angle`, and whose base is `height` further away. The desktop
// shader softens its edges and tapers the base; here the base fades out instead.
void PointerShader::Add(const Point &center, const Point &angle,
	float width, float height, float offset, const Color &color)
{
	Gfx::Vertex *v = Gfx::Triangles(3, Gfx::Material::SOLID);
	if(!v)
		return;

	Point tip = center + angle * offset;
	Point base = center + angle * (offset - height);
	Point wing(angle.Y() * width * .5, -angle.X() * width * .5);

	Gfx::Vertex c{};
	Gfx::SetColor(c, color);
	Gfx::Vertex faded = c;
	faded.r /= 4;
	faded.g /= 4;
	faded.b /= 4;
	faded.a /= 4;

	v[0] = c;
	v[0].x = static_cast<float>(tip.X());
	v[0].y = static_cast<float>(tip.Y());
	v[1] = faded;
	v[1].x = static_cast<float>((base + wing).X());
	v[1].y = static_cast<float>((base + wing).Y());
	v[2] = faded;
	v[2].x = static_cast<float>((base - wing).X());
	v[2].y = static_cast<float>((base - wing).Y());
}



void PointerShader::Unbind()
{
}
