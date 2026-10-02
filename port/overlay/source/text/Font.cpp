/* Font.cpp
Copyright (c) 2014-2020 by Michael Zahniser
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

#include "Font.h"

#include "Alignment.h"
#include "../Color.h"
#include "DisplayText.h"
#include "../GameData.h"
#include "../image/ImageBuffer.h"
#include "../image/ImageFileData.h"
#include "../Point.h"
#include "../Preferences.h"
#include "../Screen.h"
#include "Truncate.h"

#include "../ctr/Gfx.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace std;

namespace {
	bool showUnderlines = false;
	const int KERN = 2;

}



Font::Font(const filesystem::path &imagePath)
{
	Load(imagePath);
}



void Font::Load(const filesystem::path &imagePath)
{
	// Load the texture.
	ImageBuffer image;
	if(!image.Read(ImageFileData(imagePath)))
		return;

	LoadTexture(image);
	CalculateAdvances(image);
	SetUpShader(image.Width() / GLYPHS, image.Height());
	widthEllipses = WidthRawString("...");
}



void Font::Draw(const DisplayText &text, const Point &point, const Color &color) const
{
	DrawAliased(text, round(point.X()), round(point.Y()), color);
}



void Font::DrawAliased(const DisplayText &text, double x, double y, const Color &color) const
{
	int width = -1;
	const string truncText = TruncateText(text, width);
	const auto &layout = text.GetLayout();
	if(width >= 0)
	{
		if(layout.align == Alignment::CENTER)
			x += (layout.width - width) / 2;
		else if(layout.align == Alignment::RIGHT)
			x += layout.width - width;
	}
	DrawAliased(truncText, x, y, color);
}



void Font::Draw(const string &str, const Point &point, const Color &color) const
{
	DrawAliased(str, round(point.X()), round(point.Y()), color);
}



void Font::DrawAliased(const string &str, double x, double y, const Color &color) const
{
	if(!texture || str.empty())
		return;

	Gfx::Vertex c{};
	Gfx::SetColor(c, color);
	if(!c.a && !c.r && !c.g && !c.b)
		return;

	// Each character may need a second quad for its underline.
	Gfx::Vertex *v = Gfx::Triangles(12 * str.size(), Gfx::Material::ALPHA_MASK, texture);
	if(!v)
		return;
	Gfx::Vertex *const begin = v;

	float textX = static_cast<float>(x - 1.);
	float textY = static_cast<float>(y);
	auto glyphQuad = [&](int glyph, float left, float aspect)
	{
		float u0 = (glyph % columns) * cellU;
		float vTop = 1.f - (glyph / columns) * cellV;
		Gfx::Vertex tl = c, tr = c, bl = c, br = c;
		tl.x = bl.x = left;
		tr.x = br.x = left + aspect * glyphWidth;
		tl.y = tr.y = textY;
		bl.y = br.y = textY + glyphHeight;
		tl.u = bl.u = u0;
		tr.u = br.u = u0 + cellU;
		tl.v = tr.v = vTop;
		bl.v = br.v = vTop - cellV;
		Gfx::Quad(v, tl, tr, bl, br);
		v += 6;
	};

	int previous = 0;
	bool isAfterSpace = true;
	bool underlineChar = false;
	const int underscoreGlyph = max(0, min(GLYPHS - 1, '_' - 32));

	for(char ch : str)
	{
		if(ch == '_')
		{
			underlineChar = showUnderlines;
			continue;
		}

		int glyph = Glyph(ch, isAfterSpace);
		if(ch != '"' && ch != '\'')
			isAfterSpace = !glyph;
		if(!glyph)
		{
			textX += space;
			continue;
		}

		textX += advance[previous * GLYPHS + glyph] + KERN;
		glyphQuad(glyph, textX, 1.f);

		if(underlineChar)
		{
			float aspect = static_cast<float>(advance[glyph * GLYPHS] + KERN)
				/ (advance[underscoreGlyph * GLYPHS] + KERN);
			glyphQuad(underscoreGlyph, textX, aspect);
			underlineChar = false;
		}

		previous = glyph;
	}

	// Turn the vertices that were not needed into degenerate triangles.
	for(Gfx::Vertex *it = v; it != begin + 12 * str.size(); ++it)
		*it = begin[0];
}



int Font::Width(const string &str, char after) const
{
	return WidthRawString(str.c_str(), after);
}



int Font::FormattedWidth(const DisplayText &text, char after) const
{
	int width = -1;
	const string truncText = TruncateText(text, width);
	return width < 0 ? WidthRawString(truncText.c_str(), after) : width;
}



int Font::Height() const noexcept
{
	return height;
}



int Font::Space() const noexcept
{
	return space;
}



void Font::ShowUnderlines(bool show) noexcept
{
	showUnderlines = show || Preferences::Has("Always underline shortcuts");
}



int Font::Glyph(char c, bool isAfterSpace) noexcept
{
	// Curly quotes.
	if(c == '\'' && isAfterSpace)
		return 96;
	if(c == '"' && isAfterSpace)
		return 97;

	return max(0, min(GLYPHS - 3, c - 32));
}



void Font::LoadTexture(ImageBuffer &image)
{
	// Rearrange the row of glyphs into a grid.
	const int cellWidth = image.Width() / GLYPHS;
	const int cellHeight = image.Height();
	columns = 1;
	while(columns * columns < GLYPHS)
		++columns;
	int rows = (GLYPHS + columns - 1) / columns;
	int width = 8;
	while(width < columns * cellWidth)
		width <<= 1;
	int height = 8;
	while(height < rows * cellHeight)
		height <<= 1;
	if(width > 1024 || height > 1024)
		return;

	// The coverage of each texel is the alpha of the image.
	vector<uint8_t> alpha(static_cast<size_t>(width) * height, 0);
	for(int glyph = 0; glyph < GLYPHS; ++glyph)
	{
		int dx = (glyph % columns) * cellWidth;
		int dy = (glyph / columns) * cellHeight;
		for(int y = 0; y < cellHeight; ++y)
		{
			const uint32_t *row = image.Begin(y) + glyph * cellWidth;
			for(int x = 0; x < cellWidth; ++x)
				alpha[(dx + x) + (dy + y) * width] = row[x] >> 24;
		}
	}

	texture = new C3D_Tex{};
	if(!Gfx::CreateTexture(texture, width, height, GPU_A8, true))
	{
		delete texture;
		texture = nullptr;
		return;
	}

	// Fill in each mipmap level, halving the image each time.
	int levelWidth = width;
	int levelHeight = height;
	for(int level = 0; level <= texture->maxLevel; ++level)
	{
		u32 size = 0;
		u8 *data = static_cast<u8 *>(C3D_Tex2DGetImagePtr(texture, level, &size));
		C3D_Tex levelTex = *texture;
		levelTex.data = data;
		levelTex.width = levelWidth;
		levelTex.height = levelHeight;
		levelTex.size = size;
		levelTex.maxLevel = 0;
		Gfx::UploadAlpha(&levelTex, alpha.data(), levelWidth, levelHeight, levelWidth);
		if(level == texture->maxLevel)
			break;

		int nextWidth = max(1, levelWidth / 2);
		int nextHeight = max(1, levelHeight / 2);
		vector<uint8_t> next(static_cast<size_t>(nextWidth) * nextHeight);
		for(int y = 0; y < nextHeight; ++y)
			for(int x = 0; x < nextWidth; ++x)
			{
				int sum = alpha[2 * x + 2 * y * levelWidth] + alpha[2 * x + 1 + 2 * y * levelWidth]
					+ alpha[2 * x + (2 * y + 1) * levelWidth] + alpha[2 * x + 1 + (2 * y + 1) * levelWidth];
				next[x + y * nextWidth] = sum / 4;
			}
		alpha.swap(next);
		levelWidth = nextWidth;
		levelHeight = nextHeight;
	}
	C3D_TexFlush(texture);

	cellU = static_cast<float>(cellWidth) / width;
	cellV = static_cast<float>(cellHeight) / height;
}



void Font::CalculateAdvances(ImageBuffer &image)
{
	// Get the format and size of the surface.
	int width = image.Width() / GLYPHS;
	height = image.Height();
	unsigned mask = 0xFF000000;
	unsigned half = 0xC0000000;
	int pitch = image.Width();

	// advance[previous * GLYPHS + next] is the x advance for each glyph pair.
	// There is no advance if the previous value is 0, i.e. we are at the very
	// start of a string.
	memset(advance, 0, GLYPHS * sizeof(advance[0]));
	for(int previous = 1; previous < GLYPHS; ++previous)
		for(int next = 0; next < GLYPHS; ++next)
		{
			int maxD = 0;
			int glyphWidth = 0;
			uint32_t *begin = image.Pixels();
			for(int y = 0; y < height; ++y)
			{
				// Find the last non-empty pixel in the previous glyph.
				uint32_t *pend = begin + previous * width;
				uint32_t *pit = pend + width;
				while(pit != pend && (*--pit & mask) < half) {}
				int distance = (pit - pend) + 1;
				glyphWidth = max(distance, glyphWidth);

				// Special case: if "next" is zero (i.e. end of line of text),
				// calculate the full width of this character. Otherwise:
				if(next)
				{
					// Find the first non-empty pixel in this glyph.
					uint32_t *nit = begin + next * width;
					uint32_t *nend = nit + width;
					while(nit != nend && (*nit++ & mask) < half) {}

					// How far apart do you want these glyphs drawn? If drawn at
					// an advance of "width", there would be:
					// pend + width - pit   <- pixels after the previous glyph.
					// nit - (nend - width) <- pixels before the next glyph.
					// So for zero kerning distance, you would want:
					distance += 1 - (nit - (nend - width));
				}
				maxD = max(maxD, distance);

				// Update the pointer to point to the beginning of the next row.
				begin += pitch;
			}
			// This is a fudge factor to avoid over-kerning, especially for the
			// underscore and for glyph combinations like AV.
			advance[previous * GLYPHS + next] = max(maxD, glyphWidth - 4) / 2;
		}

	// Set the space size based on the character width.
	width /= 2;
	height /= 2;
	space = (width + 3) / 6 + 1;
}



void Font::SetUpShader(float glyphW, float glyphH)
{
	// The glyph images are drawn at half size.
	glyphWidth = glyphW * .5f;
	glyphHeight = glyphH * .5f;
}



int Font::WidthRawString(const char *str, char after) const noexcept
{
	int width = 0;
	int previous = 0;
	bool isAfterSpace = true;

	for( ; *str; ++str)
	{
		if(*str == '_')
			continue;

		int glyph = Glyph(*str, isAfterSpace);
		if(*str != '"' && *str != '\'')
			isAfterSpace = !glyph;
		if(!glyph)
			width += space;
		else
		{
			width += advance[previous * GLYPHS + glyph] + KERN;
			previous = glyph;
		}
	}
	width += advance[previous * GLYPHS + max(0, min(GLYPHS - 1, after - 32))];

	return width;
}



// Param width will be set to the width of the return value, unless the layout width is negative.
string Font::TruncateText(const DisplayText &text, int &width) const
{
	width = -1;
	const auto &layout = text.GetLayout();
	const string &str = text.GetText();
	if(layout.width < 0 || (layout.align == Alignment::LEFT && layout.truncate == Truncate::NONE))
		return str;
	width = layout.width;
	switch(layout.truncate)
	{
		case Truncate::NONE:
			width = WidthRawString(str.c_str());
			return str;
		case Truncate::FRONT:
			return TruncateFront(str, width);
		case Truncate::MIDDLE:
			return TruncateMiddle(str, width);
		case Truncate::BACK:
		default:
			return TruncateBack(str, width);
	}
}



string Font::TruncateBack(const string &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const string &str, int charCount)
		{
			return str.substr(0, charCount) + "...";
		});
}



string Font::TruncateFront(const string &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const string &str, int charCount)
		{
			return "..." + str.substr(str.size() - charCount);
		});
}



string Font::TruncateMiddle(const string &str, int &width) const
{
	return TruncateEndsOrMiddle(str, width,
		[](const string &str, int charCount)
		{
			return str.substr(0, (charCount + 1) / 2) + "..." + str.substr(str.size() - charCount / 2);
		});
}



string Font::TruncateEndsOrMiddle(const string &str, int &width,
	function<string(const string &, int)> getResultString) const
{
	int firstWidth = WidthRawString(str.c_str());
	if(firstWidth <= width)
	{
		width = firstWidth;
		return str;
	}

	int workingChars = 0;
	int workingWidth = 0;

	int low = 0, high = str.size() - 1;
	while(low <= high)
	{
		// Think "how many chars to take from both ends, omitting in the middle".
		int nextChars = (low + high) / 2;
		int nextWidth = WidthRawString(getResultString(str, nextChars).c_str());
		if(nextWidth <= width)
		{
			if(nextChars > workingChars)
			{
				workingChars = nextChars;
				workingWidth = nextWidth;
			}
			low = nextChars + (nextChars == low);
		}
		else
			high = nextChars - 1;
	}
	width = workingWidth;
	return getResultString(str, workingChars);
}
