/* Sprite.h
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

#pragma once

#include "../Point.h"

#include "../ctr/TextureCache.h"

#include <cstdint>
#include <string>

class ImageBuffer;



// Class representing a drawable sprite. A sprite can have multiple frames, for
// animation. On the 3DS the frames are not uploaded when the game starts:
// Texture() returns a handle in the TextureCache, which streams the frames in
// from the texture pack the first time they are drawn.
class Sprite {
public:
	explicit Sprite(const std::string &name = "");

	const std::string &Name() const;

	// Record the height and width from the buffer, but don't upload the buffer into memory.
	void LoadDimensions(const ImageBuffer &buffer);
	// Whether this sprite has any dimensions, guaranteeing that it exists even if it needs to be loaded.
	bool HasDimensions() const;
	// Add the given frames, uploading them. The given buffers will be cleared afterwards.
	// Receive both the 1x and 2x buffers. On the 3DS, the 1x buffer is preferred.
	void AddFrames(ImageBuffer &buffer1x, ImageBuffer &buffer2x, bool noReduction);
	void AddSwizzleMaskFrames(ImageBuffer &buffer1x, ImageBuffer &buffer2x, bool noReduction);
	// Use frames from the texture pack.
	void SetStreamed(TextureCache::StreamInfo &&info, float width, float height);
	// Whether the textures for this sprite are available (or can be streamed).
	bool IsLoaded() const;
	// Free up all textures loaded for this sprite.
	void Unload();

	// Image dimensions, in pixels.
	float Width() const;
	float Height() const;
	// Number of frames in the animation.
	int Frames() const;
	// This will either be 0, 1, or the same as the number of normal frames.
	int SwizzleMaskFrames() const;

	// Set and get the number of pixels within the sprite averaged across all frames.
	void SetArea(double area);
	double Area() const;

	// Get the offset of the center from the top left corner; this is for easy
	// shifting of corner to center coordinates.
	Point Center() const;

	// Get the texture handle (in the TextureCache).
	uint32_t Texture() const;
	// Non-zero if this sprite has swizzle mask frames (the same handle).
	uint32_t SwizzleMask() const;


private:
	std::string name;

	uint32_t texture{};
	bool streamed = false;
	bool isLoaded = false;

	float width = 0.f;
	float height = 0.f;
	int frames = 0;
	int swizzleMaskFrames = 0;
	double area = 0.;
};
