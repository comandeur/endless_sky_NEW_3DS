/* masktool.cpp
Copyright (c) 2026 by the Endless Sky 3DS port contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

// Host tool used by convert.py: computes collision masks with the game's own
// algorithm (upstream source/image/Mask.cpp), so that the 3DS does not have to
// decode full resolution images to build them.
//
// Input (stdin): uint32 width, height, frames, then the pixels of each frame
// (32-bit, 0xAARRGGBB as in ImageBuffer, little endian).
// Output (stdout): for each frame, uint16 outline count, then for each outline
// uint16 point count and the points as pairs of float32.

#include "image/ImageBuffer.h"
#include "image/Mask.h"
#include "Logger.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;

// Minimal implementations of the parts of ImageBuffer and Logger that Mask uses.
ImageBuffer::ImageBuffer(int frames) : width(0), height(0), frames(frames), pixels(nullptr) {}
ImageBuffer::~ImageBuffer() { delete [] pixels; }
void ImageBuffer::Allocate(int width, int height)
{
	this->width = width;
	this->height = height;
	pixels = new uint32_t[static_cast<size_t>(width) * height * frames];
}
int ImageBuffer::Width() const { return width; }
int ImageBuffer::Height() const { return height; }
int ImageBuffer::Frames() const { return frames; }
const uint32_t *ImageBuffer::Pixels() const { return pixels; }
uint32_t *ImageBuffer::Pixels() { return pixels; }
const uint32_t *ImageBuffer::Begin(int y, int frame) const
{
	return pixels + width * (y + height * frame);
}
uint32_t *ImageBuffer::Begin(int y, int frame)
{
	return pixels + width * (y + height * frame);
}

void Logger::Log(const string &message, Level level)
{
	cerr << message << endl;
}



int main()
{
	uint32_t header[3];
	if(fread(header, sizeof(uint32_t), 3, stdin) != 3)
		return 1;
	ImageBuffer image(header[2]);
	image.Allocate(header[0], header[1]);
	size_t count = static_cast<size_t>(header[0]) * header[1] * header[2];
	if(fread(image.Pixels(), sizeof(uint32_t), count, stdin) != count)
		return 1;

	for(uint32_t frame = 0; frame < header[2]; ++frame)
	{
		Mask mask;
		mask.Create(image, frame, "frame " + to_string(frame));
		const auto &outlines = mask.Outlines();
		uint16_t outlineCount = outlines.size();
		fwrite(&outlineCount, sizeof(outlineCount), 1, stdout);
		for(const auto &outline : outlines)
		{
			uint16_t pointCount = outline.size();
			fwrite(&pointCount, sizeof(pointCount), 1, stdout);
			for(const Point &point : outline)
			{
				float xy[2] = {static_cast<float>(point.X()), static_cast<float>(point.Y())};
				fwrite(xy, sizeof(float), 2, stdout);
			}
		}
	}
	return 0;
}
