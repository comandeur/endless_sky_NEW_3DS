/* opengl.h
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

// The 3DS has no OpenGL. A few panels clear the screen with glClear() before
// drawing a full-screen background; that single call is mapped onto the
// citro3d renderer (see ctr/Gfx.cpp). Everything else that used OpenGL has been
// rewritten for the PICA200 GPU.

#define GL_COLOR_BUFFER_BIT 0x00004000

void glClear(unsigned int mask);
