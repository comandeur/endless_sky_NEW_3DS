/* FillShader.cpp
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

#include "FillShader.h"

#include "../Color.h"
#include "../Point.h"
#include "../Rectangle.h"

#include "../ctr/Gfx.h"



void FillShader::Init()
{
}



void FillShader::Fill(const Rectangle &area, const Color &color)
{
	Fill(area.Center(), area.Dimensions(), color);
}



void FillShader::Fill(const Point &center, const Point &size, const Color &color)
{
	float halfWidth = .5f * static_cast<float>(size.X());
	float halfHeight = .5f * static_cast<float>(size.Y());
	float x = static_cast<float>(center.X());
	float y = static_cast<float>(center.Y());
	Gfx::FillRect(x - halfWidth, y - halfHeight, x + halfWidth, y + halfHeight, color);
}
