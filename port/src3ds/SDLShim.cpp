/* SDLShim.cpp
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

// Implementation of the small subset of SDL2 used by the game (see
// compat/SDL2/SDL.h), on top of libctru.

#include "Input.h"
#include "Platform.h"

#include <3ds.h>

#include <cstdlib>
#include <cstring>
#include <string>

using namespace std;

namespace {
	Uint32 nextUserEvent = SDL_USEREVENT;
	string clipboard;

	// A copy that the caller frees with SDL_free().
	char *Duplicate(const string &text)
	{
		char *result = static_cast<char *>(malloc(text.size() + 1));
		if(result)
			memcpy(result, text.c_str(), text.size() + 1);
		return result;
	}
}



int SDL_PollEvent(SDL_Event *event)
{
	return Input::PollEvent(event);
}



int SDL_PushEvent(SDL_Event *event)
{
	if(!event)
		return -1;
	Input::PushEvent(*event);
	return 1;
}



Uint32 SDL_RegisterEvents(int count)
{
	Uint32 first = nextUserEvent;
	nextUserEvent += count;
	return first;
}



const Uint8 *SDL_GetKeyboardState(int *numkeys)
{
	if(numkeys)
		*numkeys = SDL_NUM_SCANCODES;
	return Input::KeyboardState();
}



SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key)
{
	return Input::ScancodeFromKey(key);
}



SDL_Keymod SDL_GetModState()
{
	return Input::ModState();
}



const char *SDL_GetKeyName(SDL_Keycode key)
{
	return Input::KeyName(key);
}



SDL_Keycode SDL_GetKeyFromName(const char *name)
{
	return Input::KeyFromName(name);
}



Uint32 SDL_GetMouseState(int *x, int *y)
{
	return Input::MouseState(x, y);
}



void SDL_WarpMouseInWindow(SDL_Window *, int x, int y)
{
	Input::WarpMouse(x, y);
}



void SDL_StartTextInput()
{
	Input::StartTextInput();
}



void SDL_StopTextInput()
{
	Input::StopTextInput();
}



Uint32 SDL_GetTicks()
{
	return static_cast<Uint32>(osGetTime());
}



void SDL_Delay(Uint32 ms)
{
	svcSleepThread(static_cast<s64>(ms) * 1000000);
}



int SDL_SetClipboardText(const char *text)
{
	clipboard = text ? text : "";
	return 0;
}



char *SDL_GetClipboardText()
{
	return Duplicate(clipboard);
}



bool SDL_HasClipboardText()
{
	return !clipboard.empty();
}



int SDL_OpenURL(const char *)
{
	return -1;
}



const char *SDL_GetError()
{
	return "Not supported on the Nintendo 3DS.";
}



void SDL_free(void *mem)
{
	free(mem);
}



char *SDL_GetBasePath()
{
	return Duplicate(Platform::ResourcePath());
}



char *SDL_GetPrefPath(const char *, const char *)
{
	return Duplicate(Platform::ConfigPath());
}
