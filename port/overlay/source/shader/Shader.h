/* Shader.h
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



// On the 3DS there are no GLSL programs: the PICA200 renderer in ctr/Gfx.cpp
// replaces all of them. This class only exists so that the game's shader
// catalog (GameData::Shaders()) still compiles; it is never given any source.
class Shader {
public:
	Shader() noexcept = default;

	void Load(const char *vertex, const char *fragment) {}
	unsigned Object() const noexcept { return 1; }
	int Attrib(const char *name) const { return -1; }
	int Uniform(const char *name) const { return -1; }
};
