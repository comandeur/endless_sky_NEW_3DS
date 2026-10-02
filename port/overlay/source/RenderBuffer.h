/* RenderBuffer.h
Copyright (c) 2023 by thewierdnut
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

#include "Point.h"
#include "Screen.h"

#include "ctr/Gfx.h"



// Class that can redirect all drawing commands to an internal texture.
// This buffer uses coordinates from (0, 0) in the top left, to (width, height)
// in the bottom right.
class RenderBuffer {
public:
	// RAII wrapper to prevent accidentally not unbinding the render target.
	class RenderTargetGuard final {
	public:
		~RenderTargetGuard();

		// Explicitly deactivate render target;
		void Deactivate();


	protected:
		RenderTargetGuard(RenderBuffer &b, int screenWidth, int screenHeight);


	private:
		RenderBuffer &buffer;
		Screen::ScreenDimensionsGuard screenGuard;
		friend class RenderBuffer;
	};


public:
	// Create a texture of the given size that can be used as a render target.
	RenderBuffer(const Point &dimensions);
	virtual ~RenderBuffer();

	// Initialize the shaders used internally.
	static void Init();

	// Turn this buffer on as a render target. The render target is restored if
	// the Activation object goes out of scope.
	[[nodiscard]] RenderTargetGuard SetTarget();

	// Draw the contents of this buffer at the specified position.
	void Draw(const Point &position);
	// Draw the contents of this buffer at the specified position, clipping the contents.
	void Draw(const Point &position, const Point &clipsize, const Point &srcposition = Point());

	double Top() const;
	double Bottom() const;
	double Left() const;
	double Right() const;
	const Point &Dimensions() const;
	double Height() const;
	double Width() const;

	void SetFadePadding(float top, float bottom, float right = 0, float left = 0);


protected:
	void Deactivate();


private:
	Point size;
	// The texture is a power of two, at least as large as the buffer.
	C3D_Tex texture{};
	C3D_RenderTarget *target = nullptr;
	Gfx::TargetState previous;
	bool active = false;
	// Texture pixels per unit (less than one if the buffer is larger than 1024).
	float scale = 1.f;
	float fadePadding[4] = {};
};
