/* Input.h
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

#include "Display.h"

#include <SDL2/SDL.h>



// Translates the 3DS buttons, circle pad and touch screen into the keyboard
// and mouse events that the game understands. Buttons are mapped to the
// default key bindings of the desktop game, so all of the game's input code
// works unchanged.
namespace Input {
	void Init();

	// Read the hardware and queue the resulting events. Returns false if the
	// application should quit (HOME menu "close", power button...).
	bool Update(Display::Mode mode);

	// Event queue used by the SDL replacement.
	bool PollEvent(SDL_Event *event);
	void PushEvent(const SDL_Event &event);

	const Uint8 *KeyboardState();
	SDL_Scancode ScancodeFromKey(SDL_Keycode key);
	SDL_Keymod ModState();
	Uint32 MouseState(int *x, int *y);
	void WarpMouse(int x, int y);

	void StartTextInput();
	void StopTextInput();
	bool IsTextInputActive();
	// Open the system software keyboard and send its text to the game.
	void OpenKeyboard();

	// What the Y button does in menus, depending on the panel on top. Text
	// fields that are not Edit widgets read their characters as key presses.
	enum class TextTarget {
		// Nothing to type into: Y does nothing (typing letters there would
		// trigger the menu's keyboard shortcuts).
		NONE,
		// A text field that reads key presses (pilot name, dialogs).
		KEYS,
		// A map: Y opens its search box.
		SEARCH,
	};
	void SetTextTarget(TextTarget target);

	// Name shown to the player for a key: the 3DS button it is mapped to, if any.
	const char *KeyName(SDL_Keycode key);
	SDL_Keycode KeyFromName(const char *name);
}
