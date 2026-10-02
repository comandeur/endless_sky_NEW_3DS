/* LineShader.cpp
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

#include "LineShader.h"

#include "../Color.h"
#include "../Point.h"

#include "../ctr/Gfx.h"

#include <cmath>

using namespace std;

namespace {
	// The desktop shader draws a signed distance field with a one pixel wide
	// antialiased edge. Here the line is a quad with a transparent fringe.
	// Round caps are approximated with a few extra triangles at each end.
	constexpr int CAP_SEGMENTS = 4;

	void Vertex(Gfx::Vertex &v, const Point &p, const Gfx::Vertex &color, float alpha)
	{
		v = color;
		v.x = static_cast<float>(p.X());
		v.y = static_cast<float>(p.Y());
		v.r = static_cast<uint8_t>(v.r * alpha);
		v.g = static_cast<uint8_t>(v.g * alpha);
		v.b = static_cast<uint8_t>(v.b * alpha);
		v.a = static_cast<uint8_t>(v.a * alpha);
	}


	void Line(const Point &from, const Point &to, float width, const Color &fromColor, const Color &toColor,
		bool roundCap)
	{
		Point delta = to - from;
		double length = delta.Length();
		if(!length)
			return;
		Point unit = delta / length;
		Point normal(-unit.Y(), unit.X());

		Gfx::Vertex c0{};
		Gfx::Vertex c1{};
		Gfx::SetColor(c0, fromColor);
		Gfx::SetColor(c1, toColor);

		// Match the coverage of the desktop shader: a round capped line is
		// solid up to (width - 1) from its axis and fades out at width; a
		// square capped line is `width` wide in total, with a one unit fringe.
		double innerHalf = roundCap ? width - 1. : .5 * width;
		double outerHalf = innerHalf + 1.;
		// Lines thinner than the fringe get fainter instead of thinner.
		float core = 1.f;
		if(innerHalf < .5)
		{
			core = static_cast<float>(max(0., innerHalf + .5));
			innerHalf = 0.;
			outerHalf = 1.;
		}
		Point inner = normal * innerHalf;
		Point outer = normal * outerHalf;

		int count = 18 + (roundCap ? 2 * CAP_SEGMENTS * 9 : 0);
		Gfx::Vertex *v = Gfx::Triangles(count, Gfx::Material::SOLID);
		if(!v)
			return;

		Gfx::Vertex outerA0, innerA0, innerB0, outerB0, outerA1, innerA1, innerB1, outerB1;
		Vertex(outerA0, from + outer, c0, 0.f);
		Vertex(innerA0, from + inner, c0, core);
		Vertex(innerB0, from - inner, c0, core);
		Vertex(outerB0, from - outer, c0, 0.f);
		Vertex(outerA1, to + outer, c1, 0.f);
		Vertex(innerA1, to + inner, c1, core);
		Vertex(innerB1, to - inner, c1, core);
		Vertex(outerB1, to - outer, c1, 0.f);

		// Fringe, core, fringe.
		Gfx::Quad(v, outerA0, outerA1, innerA0, innerA1);
		Gfx::Quad(v + 6, innerA0, innerA1, innerB0, innerB1);
		Gfx::Quad(v + 12, innerB0, innerB1, outerB0, outerB1);
		v += 18;

		if(!roundCap)
			return;
		// Half discs at both ends, each a fan of triangles plus a fringe.
		for(int side = 0; side < 2; ++side)
		{
			const Point &center = side ? to : from;
			const Gfx::Vertex &color = side ? c1 : c0;
			Point forward = side ? unit : -unit;
			Gfx::Vertex mid;
			Vertex(mid, center, color, core);
			for(int i = 0; i < CAP_SEGMENTS; ++i)
			{
				double a0 = M_PI * i / CAP_SEGMENTS;
				double a1 = M_PI * (i + 1) / CAP_SEGMENTS;
				Point d0 = normal * cos(a0) + forward * sin(a0);
				Point d1 = normal * cos(a1) + forward * sin(a1);
				Gfx::Vertex p0, p1, q0, q1;
				Vertex(p0, center + d0 * innerHalf, color, core);
				Vertex(p1, center + d1 * innerHalf, color, core);
				Vertex(q0, center + d0 * outerHalf, color, 0.f);
				Vertex(q1, center + d1 * outerHalf, color, 0.f);
				v[0] = mid;
				v[1] = p0;
				v[2] = p1;
				Gfx::Quad(v + 3, p0, p1, q0, q1);
				v += 9;
			}
		}
	}
}



void LineShader::Init()
{
}



void LineShader::Draw(const Point &from, const Point &to, float width, const Color &color, bool roundCap)
{
	DrawGradient(from, to, width, color, color, roundCap);
}



void LineShader::DrawDashed(const Point &from, const Point &to, const Point &unit, const float width,
	const Color &color, const double dashLength, double spaceLength, bool roundCap)
{
	const double length = (to - from).Length();
	const double patternLength = dashLength + spaceLength;
	int segments = length / patternLength;
	// If needed, scale pattern down so we can draw at least two of them over length.
	if(segments < 2)
	{
		segments = 2;
		spaceLength *= length / (segments * patternLength);
	}
	spaceLength /= 2.;
	float capOffset = roundCap ? width : 0.;
	for(int i = 0; i < segments; ++i)
		Draw(from + unit * (i * length / segments + spaceLength + capOffset),
			from + unit * ((i + 1) * length / segments - spaceLength - capOffset),
			width, color, roundCap);
}



void LineShader::DrawGradient(const Point &from, const Point &to, float width,
	const Color &fromColor, const Color &toColor, bool roundCap)
{
	Line(from, to, width, fromColor, toColor, roundCap);
}



void LineShader::DrawGradientDashed(const Point &from, const Point &to, const Point &unit, const float width,
		const Color &fromColor, const Color &toColor, const double dashLength, double spaceLength, bool roundCap)
{
	const double length = (to - from).Length();
	const double patternLength = dashLength + spaceLength;
	int segments = length / patternLength;
	// If needed, scale pattern down so we can draw at least two of them over length.
	if(segments < 2)
	{
		segments = 2;
		spaceLength *= length / (segments * patternLength);
	}
	spaceLength /= 2.;
	float capOffset = roundCap ? width : 0.;
	for(int i = 0; i < segments; ++i)
	{
		float p = static_cast<float>(i) / segments;
		Color mixed = Color::Combine(1. - p, fromColor, p, toColor);
		float pv = static_cast<float>(i + 1) / segments;
		Color mixed2 = Color::Combine(1. - pv, fromColor, pv, toColor);
		DrawGradient(from + unit * (i * length / segments + spaceLength + capOffset),
			from + unit * ((i + 1) * length / segments - spaceLength - capOffset),
			width, mixed, mixed2, roundCap);
	}
}
