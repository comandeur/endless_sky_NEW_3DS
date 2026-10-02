/* RingShader.cpp
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

#include "RingShader.h"

#include "../Color.h"
#include "../pi.h"
#include "../Point.h"

#include "../ctr/Gfx.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	// The desktop shader computes the ring per pixel. Here it is tessellated:
	// each slice of the arc is an inner fringe, a solid core and an outer fringe.
	void Arc(const Point &center, float radius, float width, double from, double to, const Gfx::Vertex &color)
	{
		if(to <= from)
			return;
		double innerCore = radius - width + 1.;
		double outerCore = radius + width - 1.;
		float core = 1.f;
		if(outerCore < innerCore)
		{
			// Thinner than two units: draw a faint two unit wide band.
			core = max(0.f, width);
			innerCore = outerCore = radius;
		}
		double innerEdge = max(0., innerCore - 1.);
		double outerEdge = outerCore + 1.;

		// Roughly one slice every three units along the outer edge.
		int slices = clamp(static_cast<int>(ceil(outerEdge * (to - from) / 3.)), 3, 96);
		Gfx::Vertex *v = Gfx::Triangles(slices * 18, Gfx::Material::SOLID);
		if(!v)
			return;

		Gfx::Vertex solid = color;
		solid.r = static_cast<uint8_t>(solid.r * core);
		solid.g = static_cast<uint8_t>(solid.g * core);
		solid.b = static_cast<uint8_t>(solid.b * core);
		solid.a = static_cast<uint8_t>(solid.a * core);
		Gfx::Vertex clear = color;
		clear.r = clear.g = clear.b = clear.a = 0;

		float cx = static_cast<float>(center.X());
		float cy = static_cast<float>(center.Y());
		auto at = [cx, cy](Gfx::Vertex v, double angle, double r) -> Gfx::Vertex
		{
			// Angles start at the top and increase counterclockwise.
			v.x = cx - static_cast<float>(r * sin(angle));
			v.y = cy - static_cast<float>(r * cos(angle));
			return v;
		};

		for(int i = 0; i < slices; ++i)
		{
			double a0 = from + (to - from) * i / slices;
			double a1 = from + (to - from) * (i + 1) / slices;
			Gfx::Vertex e0 = at(clear, a0, innerEdge), e1 = at(clear, a1, innerEdge);
			Gfx::Vertex i0 = at(solid, a0, innerCore), i1 = at(solid, a1, innerCore);
			Gfx::Vertex o0 = at(solid, a0, outerCore), o1 = at(solid, a1, outerCore);
			Gfx::Vertex f0 = at(clear, a0, outerEdge), f1 = at(clear, a1, outerEdge);
			Gfx::Quad(v, e0, e1, i0, i1);
			Gfx::Quad(v + 6, i0, i1, o0, o1);
			Gfx::Quad(v + 12, o0, o1, f0, f1);
			v += 18;
		}
	}
}



void RingShader::Init()
{
}



void RingShader::Draw(const Point &pos, float out, float in, const Color &color)
{
	float width = .5f * (1.f + out - in);
	Draw(pos, out - width, width, 1.f, color);
}



void RingShader::Draw(const Point &pos, float radius, float width, float fraction,
	const Color &color, float dash, float startAngle)
{
	Add(pos, radius, width, fraction, color, dash, startAngle);
}



void RingShader::Bind()
{
}



void RingShader::Add(const Point &pos, float out, float in, const Color &color)
{
	float width = .5f * (1.f + out - in);
	Add(pos, out - width, width, 1.f, color);
}



void RingShader::Add(const Point &pos, float radius, float width, float fraction,
	const Color &color, float dash, float startAngle)
{
	if(fraction <= 0.f || radius + width <= 0.f)
		return;
	Gfx::Vertex c{};
	Gfx::SetColor(c, color);
	if(!c.a && !c.r && !c.g && !c.b)
		return;

	// The visible part of the ring, counterclockwise from the top.
	double from = -startAngle * TO_RAD;
	double to = from + min(1.f, fraction) * 2. * PI;
	if(!dash)
	{
		Arc(pos, radius, width, from, to, c);
		return;
	}

	// Dashes are separated by a gap of about two units, aligned to the start angle.
	double dashAngle = 2. * PI / dash;
	double gap = 1. / max(1.f, radius);
	for(double start = from; start < to; start += dashAngle)
		Arc(pos, radius, width, start + gap, min(to, start + dashAngle) - gap, c);
}



void RingShader::Unbind()
{
}
