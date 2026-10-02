/* TextureCache.h
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

#include <citro3d.h>

#include <cstdint>
#include <string>
#include <vector>

class ImageBuffer;



// The desktop game uploads every sprite to the GPU when it starts. The 3DS has
// far too little memory for that, so sprite textures are streamed instead:
// a sprite's frames are read from the texture pack the first time the sprite is
// drawn, and the least recently drawn sprites are evicted when the texture
// budget is exceeded. A sprite that is still loading is simply not drawn.
namespace TextureCache {
	// Where the frames of a streamed sprite are stored in the texture pack.
	struct StreamInfo {
		GPU_TEXCOLOR format = GPU_RGBA8;
		uint16_t width = 0;
		uint16_t height = 0;
		// Extent of the image within the (power of two) texture.
		float u = 1.f;
		float v = 0.f;
		// Offset and size of each frame's data in the pack: first the color
		// frames, then the swizzle mask frames.
		std::vector<uint32_t> offsets;
		std::vector<uint32_t> sizes;
		uint16_t frames = 0;
		uint16_t maskFrames = 0;
	};

	// What a sprite needs in order to be drawn.
	struct Textures {
		const std::vector<C3D_Tex> &frames;
		const std::vector<C3D_Tex> &masks;
		// Texture coordinates of the bottom right corner of the image (the
		// top left is always u = 0, v = 1).
		float u;
		float v;
	};

	void Init(const std::string &packPath);
	void Quit();

	// Register a sprite whose frames are in the pack. Returns a handle (never 0).
	uint32_t AddStreamed(StreamInfo &&info);
	// Upload the frames of a decoded image right away. Used for images that
	// are not in the pack (fonts, plugins). Returns 0 on failure.
	uint32_t AddImage(const std::string &name, ImageBuffer &buffer, bool isMask);
	// Append mask frames from a decoded image to an existing handle.
	void AddMaskImage(uint32_t handle, ImageBuffer &buffer);
	void Remove(uint32_t handle);

	// Get the textures of a sprite, requesting them if they are not loaded.
	// Returns false if the sprite cannot be drawn yet.
	bool Get(uint32_t handle, const std::vector<C3D_Tex> *&frames, const std::vector<C3D_Tex> *&masks,
		float &u, float &v);
	// Request a sprite without drawing it (e.g. when a panel is about to show it).
	void Prefetch(uint32_t handle);
	bool IsResident(uint32_t handle);

	// Once per frame: start new loads, finish completed ones, evict old sprites.
	void Update();

	// Statistics.
	size_t BytesUsed();
	size_t Budget();
	int PendingLoads();
}
