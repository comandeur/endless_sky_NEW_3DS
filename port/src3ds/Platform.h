/* Platform.h
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

#include <cstddef>
#include <string>



// System level services of the 3DS: initialization, paths and memory.
namespace Platform {
	// Bring up the system services. Returns false if the game cannot run.
	bool Init();
	void Quit();

	// Where the converted game data lives (data/, images/, sounds/...).
	const std::string &ResourcePath();
	// Where saves and preferences are written.
	const std::string &ConfigPath();

	bool IsNew3DS();

	// Memory statistics, in bytes.
	size_t HeapUsed();
	size_t HeapFree();
	size_t LinearFree();

	// Show a fatal error on the screens and wait for the player to exit.
	[[noreturn]] void FatalError(const std::string &message);
}
