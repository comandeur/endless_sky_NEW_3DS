/* FogShader.cpp
Copyright (c) 2016 by Michael Zahniser
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

#include "FogShader.h"

#include "../GameData.h"
#include "../PlayerInfo.h"
#include "../Point.h"
#include "../Screen.h"
#include "../System.h"

#include "../ctr/Gfx.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace std;

namespace {
	// Scale of the mask image:
	const int GRID = 16;
	// Distance represented by one orthogonal or diagonal step:
	const int ORTH = 5;
	const int DIAG = 7;
	// Limit distances to the size of an unsigned char.
	const int LIMIT = 255;
	// Pad beyond the screen enough to include any system that might "cast light"
	// on the on-screen view.
	const int PAD = LIMIT / ORTH;

	// The mask texture (an A8 texture: the fog is black with this alpha).
	C3D_Tex texture{};
	int textureWidth = 0;
	int textureHeight = 0;

	// Keep track of the previous frame's view so that if it is unchanged we can
	// skip regenerating the mask.
	double previousZoom = 0.;
	double previousLeft = 0.;
	double previousTop = 0.;
	int previousColumns = 0;
	int previousRows = 0;
	Point previousCenter;


	int NextPowerOfTwo(int value)
	{
		int result = 8;
		while(result < value)
			result <<= 1;
		return result;
	}
}



void FogShader::Init()
{
}



void FogShader::Redraw()
{
	previousZoom = 0.;
}



void FogShader::Draw(const Point &center, double zoom, const PlayerInfo &player)
{
	// Generate a scaled-down mask image that represents the entire screen plus
	// enough pixels beyond the screen to include any systems that may be off
	// screen but close enough to "illuminate" part of the on-screen map.
	double left = Screen::Left() - GRID * PAD * zoom + fmod(center.X(), GRID) * zoom;
	double top = Screen::Top() - GRID * PAD * zoom + fmod(center.Y(), GRID) * zoom;
	int columns = ceil(Screen::Width() / (GRID * zoom)) + 1 + 2 * PAD;
	int rows = ceil(Screen::Height() / (GRID * zoom)) + 1 + 2 * PAD;
	// Textures are at most 1024 texels wide.
	columns = min(columns, 1024);
	rows = min(rows, 1024);

	bool shouldRegenerate = (
		zoom != previousZoom || center.X() != previousCenter.X() || center.Y() != previousCenter.Y() ||
		left != previousLeft || top != previousTop || columns != previousColumns || rows != previousRows
		|| !texture.data);
	if(shouldRegenerate)
	{
		// Remember the current viewport attributes.
		previousZoom = zoom;
		previousCenter = center;
		previousLeft = left;
		previousTop = top;
		previousColumns = columns;
		previousRows = rows;

		// This buffer will hold the mask image.
		auto buffer = vector<unsigned char>(static_cast<size_t>(rows) * columns, LIMIT);
		// For each system the player knows about, its "distance" pixel in the
		// buffer should be set to 0.
		for(const auto &it : GameData::Systems())
		{
			const System &system = it.second;
			if(!system.IsValid() || !player.CanView(system))
				continue;
			Point pos = zoom * (system.Position() + center);

			int x = round((pos.X() - left) / (GRID * zoom));
			int y = round((pos.Y() - top) / (GRID * zoom));
			if(x >= 0 && y >= 0 && x < columns && y < rows)
				buffer[x + y * columns] = 0;
		}

		// Distance transformation: make two passes through the buffer. In the first
		// pass, propagate down and to the right. In the second, propagate in the
		// opposite direction. Once these two passes are done, each value is equal
		// to the (approximate) distance to the closest known system.
		for(int y = 1; y < rows; ++y)
			for(int x = 1; x < columns - 1; ++x)
				buffer[x + y * columns] = min<int>(buffer[x + y * columns], min(
					ORTH + min(buffer[(x - 1) + y * columns], buffer[x + (y - 1) * columns]),
					DIAG + min(buffer[(x - 1) + (y - 1) * columns], buffer[(x + 1) + (y - 1) * columns])));
		for(int y = rows - 2; y >= 0; --y)
			for(int x = columns - 2; x >= 1; --x)
				buffer[x + y * columns] = min<int>(buffer[x + y * columns], min(
					ORTH + min(buffer[(x + 1) + y * columns], buffer[x + (y + 1) * columns]),
					DIAG + min(buffer[(x - 1) + (y + 1) * columns], buffer[(x + 1) + (y + 1) * columns])));

		// Stretch the distance values so there is no shading up to about 200 pixels
		// away, then it transitions somewhat quickly.
		for(unsigned char &value : buffer)
			value = max(0, min(LIMIT, (value - 60) * 4));

		int width = NextPowerOfTwo(columns);
		int height = NextPowerOfTwo(rows);
		if(!texture.data || width != textureWidth || height != textureHeight)
		{
			Gfx::DeleteTexture(&texture);
			texture = C3D_Tex{};
			if(!Gfx::CreateTexture(&texture, width, height, GPU_A8))
				return;
			textureWidth = width;
			textureHeight = height;
		}
		Gfx::UploadAlpha(&texture, buffer.data(), columns, rows, columns);
	}
	if(!texture.data)
		return;

	// The texture covers (columns + 1) by (rows + 1) cells, starting half a cell
	// before the first sample, like the desktop shader.
	float x0 = left - .5 * GRID * zoom;
	float y0 = top - .5 * GRID * zoom;
	float x1 = x0 + GRID * zoom * (columns + 1.);
	float y1 = y0 + GRID * zoom * (rows + 1.);
	float u1 = static_cast<float>(columns) / textureWidth;
	float v1 = 1.f - static_cast<float>(rows) / textureHeight;

	Gfx::Vertex *v = Gfx::Triangles(6, Gfx::Material::ALPHA_MASK, &texture);
	if(!v)
		return;
	Gfx::Vertex c{};
	Gfx::SetColor(c, 0, 0, 0, 255);
	Gfx::Vertex tl = c, tr = c, bl = c, br = c;
	tl.x = bl.x = x0;
	tr.x = br.x = x1;
	tl.y = tr.y = y0;
	bl.y = br.y = y1;
	tl.u = bl.u = 0.f;
	tr.u = br.u = u1;
	tl.v = tr.v = 1.f;
	bl.v = br.v = v1;
	Gfx::Quad(v, tl, tr, bl, br);
}
