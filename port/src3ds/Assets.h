/* Assets.h
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

#pragma once

#include "TextureCache.h"

#include "../Point.h"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

class ImageSet;



// A sprite converted by the asset pipeline (tools/assets): its frames are
// GPU-ready textures in images.pak, and its collision masks were computed ahead
// of time with the game's own algorithm.
struct PackedSprite {
	TextureCache::StreamInfo stream;
	// Size of the original image, in pixels.
	float width = 0.f;
	float height = 0.f;
	float area = 0.f;
	// Collision mask outlines, one set per frame (only for ships and asteroids).
	std::vector<std::vector<std::vector<Point>>> masks;
};



namespace Assets {
	// The files written by the asset pipeline, relative to the resource folder.
	constexpr const char *IMAGE_INDEX = "images.idx";
	constexpr const char *IMAGE_PACK = "images.pak";

	// Read the image index of a resource folder and add an ImageSet for each
	// sprite. Returns false if there is no index (the images are then searched
	// for as loose files, like on the desktop).
	bool LoadImageIndex(const std::filesystem::path &folder, std::map<std::string, std::shared_ptr<ImageSet>> &images);
}
