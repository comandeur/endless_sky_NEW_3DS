/* Sprite.cpp
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

#include "Sprite.h"

#include "ImageBuffer.h"

using namespace std;



Sprite::Sprite(const string &name)
	: name(name)
{
}



const string &Sprite::Name() const
{
	return name;
}



void Sprite::LoadDimensions(const ImageBuffer &buffer)
{
	width = buffer.Width();
	height = buffer.Height();
	frames = buffer.Frames();
}



bool Sprite::HasDimensions() const
{
	return width != 0 && height != 0 && frames != 0;
}



// Add the given frames. The given buffers will be cleared afterwards.
void Sprite::AddFrames(ImageBuffer &buffer1x, ImageBuffer &buffer2x, bool noReduction)
{
	isLoaded = true;
	width = buffer1x.Width();
	height = buffer1x.Height();
	frames = buffer1x.Frames();
	if(buffer1x.Pixels())
	{
		if(texture)
			TextureCache::Remove(texture);
		// The 3DS screens have a low resolution, so @2x images are never needed.
		texture = TextureCache::AddImage(name, buffer1x, false);
	}
	buffer1x.Clear();
	buffer2x.Clear();
}



void Sprite::AddSwizzleMaskFrames(ImageBuffer &buffer1x, ImageBuffer &buffer2x, bool noReduction)
{
	if(!swizzleMaskFrames)
	{
		swizzleMaskFrames = buffer1x.Frames();
		if(swizzleMaskFrames > 1 && swizzleMaskFrames < frames)
			swizzleMaskFrames = 1;
	}
	if(buffer1x.Pixels() && texture)
		TextureCache::AddMaskImage(texture, buffer1x);
	else if(!buffer1x.Pixels())
		swizzleMaskFrames = 0;
	buffer1x.Clear();
	buffer2x.Clear();
}



void Sprite::SetStreamed(TextureCache::StreamInfo &&info, float width, float height)
{
	this->width = width;
	this->height = height;
	frames = info.frames;
	swizzleMaskFrames = info.maskFrames;
	isLoaded = true;
	// A streamed sprite keeps its handle; it only needs to be registered once.
	if(streamed)
		return;
	if(texture)
		TextureCache::Remove(texture);
	texture = TextureCache::AddStreamed(std::move(info));
	streamed = true;
}



bool Sprite::IsLoaded() const
{
	return isLoaded;
}



void Sprite::Unload()
{
	if(!texture)
		return;
	TextureCache::Remove(texture);
	// Streamed sprites come back on their own when they are drawn again;
	// uploaded images are gone.
	if(!streamed)
	{
		texture = 0;
		isLoaded = false;
	}
}



float Sprite::Width() const
{
	return width;
}



float Sprite::Height() const
{
	return height;
}



int Sprite::Frames() const
{
	return frames;
}



int Sprite::SwizzleMaskFrames() const
{
	return swizzleMaskFrames;
}



void Sprite::SetArea(double area)
{
	this->area = area;
}



double Sprite::Area() const
{
	return area;
}



Point Sprite::Center() const
{
	return Point(.5 * width, .5 * height);
}



uint32_t Sprite::Texture() const
{
	return texture;
}



uint32_t Sprite::SwizzleMask() const
{
	return swizzleMaskFrames ? texture : 0;
}
