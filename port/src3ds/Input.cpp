/* Input.cpp
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

#include "Input.h"

#include <3ds.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>

using namespace std;

namespace {
	// A 3DS button and the key it stands for.
	struct Binding {
		u32 button;
		SDL_Keycode key;
		const char *label;
	};

	// Flight: the default desktop key bindings of the corresponding commands.
	const Binding FLIGHT_BINDINGS[] = {
		{KEY_A, SDLK_TAB, "A"},          // Fire primary weapon
		{KEY_X, SDLK_q, "X"},            // Fire secondary weapon
		{KEY_Y, SDLK_w, "Y"},            // Select secondary weapon
		{KEY_B, SDLK_a, "B"},            // Fire afterburner
		{KEY_L, SDLK_n, "L"},            // Select next ship
		{KEY_R, SDLK_r, "R"},            // Select nearest hostile ship
		{KEY_ZL, SDLK_l, "ZL"},          // Land on planet / station
		{KEY_ZR, SDLK_j, "ZR"},          // Initiate hyperspace jump
		{KEY_DUP, SDLK_t, "D-Pad Up"},   // Talk to selected ship
		{KEY_DDOWN, SDLK_b, "D-Pad Down"}, // Board selected ship
		{KEY_DLEFT, SDLK_s, "D-Pad Left"}, // Scan selected ship
		{KEY_DRIGHT, SDLK_d, "D-Pad Right"}, // Deploy / recall fighters
		{KEY_START, SDLK_ESCAPE, "Start"}, // Show main menu
		{KEY_SELECT, SDLK_m, "Select"},  // View star map
	};

	// Menus: navigation keys.
	const Binding MENU_BINDINGS[] = {
		{KEY_A, SDLK_RETURN, "A"},
		{KEY_B, SDLK_ESCAPE, "B"},
		{KEY_START, SDLK_ESCAPE, "Start"},
		{KEY_X, SDLK_TAB, "X"},
		{KEY_DUP, SDLK_UP, "D-Pad Up"},
		{KEY_DDOWN, SDLK_DOWN, "D-Pad Down"},
		{KEY_DLEFT, SDLK_LEFT, "D-Pad Left"},
		{KEY_DRIGHT, SDLK_RIGHT, "D-Pad Right"},
		{KEY_ZL, SDLK_PAGEUP, "ZL"},
		{KEY_ZR, SDLK_PAGEDOWN, "ZR"},
	};
	// Keys that repeat while held in menus.
	constexpr u32 REPEATING = KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_ZL | KEY_ZR;
	constexpr u64 REPEAT_DELAY_MS = 400;
	constexpr u64 REPEAT_RATE_MS = 80;

	// Circle pad thresholds (the pad reports roughly -156 to 156).
	constexpr int PAD_DEADZONE = 40;
	constexpr int PAD_TURN = 50;

	deque<SDL_Event> events;
	Uint8 keyboard[SDL_NUM_SCANCODES] = {};
	// Which keys the current input state holds down.
	map<SDL_Keycode, bool> heldKeys;
	SDL_Keymod modState = KMOD_NONE;

	Display::Mode lastMode = Display::Mode::MENU;
	u64 repeatStart = 0;
	u64 lastRepeat = 0;
	u32 repeatButton = 0;

	// Mouse emulation.
	bool touching = false;
	int touchKey = 0;
	Point mousePoint;
	u64 lastTapTime = 0;
	Point lastTapPoint;
	u64 lastWheel = 0;

	bool textInput = false;
	Input::TextTarget textTarget = Input::TextTarget::NONE;


	void RefreshHint()
	{
		if(textInput || textTarget == Input::TextTarget::KEYS)
			Display::SetHint("Y : clavier / keyboard");
		else if(textTarget == Input::TextTarget::SEARCH)
			Display::SetHint("Y : rechercher / search");
		else
			Display::SetHint("");
	}


	SDL_Event KeyEvent(Uint32 type, SDL_Keycode key, bool repeat)
	{
		SDL_Event event;
		memset(&event, 0, sizeof(event));
		event.type = type;
		event.key.state = (type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED);
		event.key.repeat = repeat;
		event.key.keysym.sym = key;
		event.key.keysym.scancode = Input::ScancodeFromKey(key);
		event.key.keysym.mod = modState;
		return event;
	}


	void SetKey(SDL_Keycode key, bool down)
	{
		bool &held = heldKeys[key];
		if(held == down)
			return;
		held = down;
		keyboard[Input::ScancodeFromKey(key)] = down;
		events.push_back(KeyEvent(down ? SDL_KEYDOWN : SDL_KEYUP, key, false));
	}


	void PressKey(SDL_Keycode key, bool repeat = false)
	{
		events.push_back(KeyEvent(SDL_KEYDOWN, key, repeat));
	}


	void ReleaseAllKeys()
	{
		for(auto &it : heldKeys)
			if(it.second)
			{
				it.second = false;
				keyboard[Input::ScancodeFromKey(it.first)] = 0;
				events.push_back(KeyEvent(SDL_KEYUP, it.first, false));
			}
		modState = KMOD_NONE;
	}


	void MouseMotion(const Point &to, bool pressed)
	{
		SDL_Event event;
		memset(&event, 0, sizeof(event));
		event.type = SDL_MOUSEMOTION;
		event.motion.state = pressed ? SDL_BUTTON_LMASK : 0;
		event.motion.x = lround(to.X());
		event.motion.y = lround(to.Y());
		event.motion.xrel = lround(to.X() - mousePoint.X());
		event.motion.yrel = lround(to.Y() - mousePoint.Y());
		mousePoint = to;
		Display::SetMouse(to);
		events.push_back(event);
	}


	void MouseButton(Uint32 type, int clicks)
	{
		SDL_Event event;
		memset(&event, 0, sizeof(event));
		event.type = type;
		event.button.button = SDL_BUTTON_LEFT;
		event.button.state = (type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED);
		event.button.clicks = clicks;
		event.button.x = lround(mousePoint.X());
		event.button.y = lround(mousePoint.Y());
		events.push_back(event);
	}


	void MouseWheel(int dy)
	{
		SDL_Event event;
		memset(&event, 0, sizeof(event));
		event.type = SDL_MOUSEWHEEL;
		event.wheel.y = dy;
		events.push_back(event);
	}


	void UpdateFlight(u32 down, u32 held, u32 up)
	{
		for(const Binding &binding : FLIGHT_BINDINGS)
		{
			if(down & binding.button)
				SetKey(binding.key, true);
			if(up & binding.button)
				SetKey(binding.key, false);
		}

		// Circle pad: classic tank controls, like the arrow keys.
		circlePosition pad;
		hidCircleRead(&pad);
		SetKey(SDLK_UP, pad.dy > PAD_DEADZONE);
		SetKey(SDLK_DOWN, pad.dy < -PAD_DEADZONE * 2);
		SetKey(SDLK_LEFT, pad.dx < -PAD_TURN);
		SetKey(SDLK_RIGHT, pad.dx > PAD_TURN);

		// C-stick: zoom the view in and out.
		u64 now = osGetTime();
		if((held & (KEY_CSTICK_UP | KEY_CSTICK_DOWN)) && now - lastWheel > 150)
		{
			lastWheel = now;
			MouseWheel((held & KEY_CSTICK_UP) ? 1 : -1);
		}

		// Touch screen: the flight buttons.
		if(down & KEY_TOUCH)
		{
			touchPosition touch;
			hidTouchRead(&touch);
			touchKey = Display::FlightButtonAt(touch.px, touch.py);
			if(touchKey)
			{
				SetKey(touchKey, true);
				Display::SetPressedFlightButton(touchKey);
			}
		}
		if((up & KEY_TOUCH) && touchKey)
		{
			SetKey(touchKey, false);
			Display::SetPressedFlightButton(0);
			touchKey = 0;
		}
	}


	void UpdateMenu(u32 down, u32 held, u32 up)
	{
		u64 now = osGetTime();
		// Modifiers: L = shift, R = control (e.g. to buy several items at once).
		SDL_Keymod mod = KMOD_NONE;
		if(held & KEY_L)
			mod |= KMOD_LSHIFT;
		if(held & KEY_R)
			mod |= KMOD_LCTRL;
		modState = mod;
		SetKey(SDLK_LSHIFT, held & KEY_L);
		SetKey(SDLK_LCTRL, held & KEY_R);

		for(const Binding &binding : MENU_BINDINGS)
		{
			if(down & binding.button)
			{
				PressKey(binding.key);
				keyboard[Input::ScancodeFromKey(binding.key)] = 1;
				if(binding.button & REPEATING)
				{
					repeatButton = binding.button;
					repeatStart = now;
					lastRepeat = now;
				}
			}
			if(up & binding.button)
			{
				events.push_back(KeyEvent(SDL_KEYUP, binding.key, false));
				keyboard[Input::ScancodeFromKey(binding.key)] = 0;
				if(repeatButton == binding.button)
					repeatButton = 0;
			}
		}
		if(repeatButton && (held & repeatButton) && now - repeatStart > REPEAT_DELAY_MS
				&& now - lastRepeat > REPEAT_RATE_MS)
		{
			lastRepeat = now;
			for(const Binding &binding : MENU_BINDINGS)
				if(binding.button == repeatButton)
					PressKey(binding.key, true);
		}

		if(down & KEY_SELECT)
			Display::CycleLensZoom();
		// Some panels (the pilot name, text dialogs) read typed characters as
		// key presses instead of text input events. Elsewhere, typed letters
		// would act as keyboard shortcuts, so the keyboard only opens when
		// there is something to type into.
		if(down & KEY_Y)
		{
			if(textInput || textTarget == Input::TextTarget::KEYS)
				Input::OpenKeyboard();
			else if(textTarget == Input::TextTarget::SEARCH)
				PressKey(SDLK_f);
			else
				Display::ShowToast("Pas de champ de texte ici / No text field here");
		}

		// Circle pad: move the lens over the canvas.
		circlePosition pad;
		hidCircleRead(&pad);
		if(abs(pad.dx) > 15 || abs(pad.dy) > 15)
		{
			double speed = 12. / Display::LensScale();
			Display::MoveLens(pad.dx / 156. * speed, -pad.dy / 156. * speed);
		}

		// C-stick left / right: lens size.
		if(down & KEY_CSTICK_RIGHT)
			Display::ZoomLens(1);
		if(down & KEY_CSTICK_LEFT)
			Display::ZoomLens(-1);

		// C-stick up / down: mouse wheel.
		if((held & (KEY_CSTICK_UP | KEY_CSTICK_DOWN)) && now - lastWheel > 100)
		{
			lastWheel = now;
			MouseWheel((held & KEY_CSTICK_UP) ? 1 : -1);
		}

		// Touch screen: the mouse.
		if(held & KEY_TOUCH)
		{
			touchPosition touch;
			hidTouchRead(&touch);
			Point point = Display::TouchToMouse(touch.px, touch.py);
			if(down & KEY_TOUCH)
			{
				// Hover first, so that the panel knows where the "mouse" is.
				MouseMotion(point, false);
				int clicks = 1;
				if(now - lastTapTime < 400 && point.Distance(lastTapPoint) < 12.)
					clicks = 2;
				lastTapTime = now;
				lastTapPoint = point;
				MouseButton(SDL_MOUSEBUTTONDOWN, clicks);
				touching = true;
			}
			else if(touching && point.Distance(mousePoint) >= 1.)
				MouseMotion(point, true);
		}
		if((up & KEY_TOUCH) && touching)
		{
			MouseButton(SDL_MOUSEBUTTONUP, 1);
			touching = false;
		}
	}
}



void Input::Init()
{
	hidSetRepeatParameters(0, 0);
}



bool Input::Update(Display::Mode mode)
{
	if(!aptMainLoop())
		return false;

	hidScanInput();
	u32 down = hidKeysDown();
	u32 held = hidKeysHeld();
	u32 up = hidKeysUp();

	if(mode != lastMode)
	{
		// Don't leave keys stuck down when the controls change.
		ReleaseAllKeys();
		if(touching)
		{
			MouseButton(SDL_MOUSEBUTTONUP, 1);
			touching = false;
		}
		if(touchKey)
		{
			Display::SetPressedFlightButton(0);
			touchKey = 0;
		}
		repeatButton = 0;
		lastMode = mode;
		// A button that is still held from the previous mode must not trigger
		// the new mapping until it is pressed again.
		down = 0;
		up = 0;
	}

	if(mode == Display::Mode::FLIGHT)
		UpdateFlight(down, held, up);
	else
		UpdateMenu(down, held, up);
	return true;
}



bool Input::PollEvent(SDL_Event *event)
{
	if(events.empty())
		return false;
	if(event)
		*event = events.front();
	events.pop_front();
	return true;
}



void Input::PushEvent(const SDL_Event &event)
{
	events.push_back(event);
}



const Uint8 *Input::KeyboardState()
{
	return keyboard;
}



SDL_Scancode Input::ScancodeFromKey(SDL_Keycode key)
{
	if(key >= 0 && key < 128)
		return key;
	if(key & SDLK_SCANCODE_MASK)
	{
		int code = key & ~SDLK_SCANCODE_MASK;
		if(code >= 0 && code < SDL_NUM_SCANCODES - 128)
			return 128 + code;
	}
	return 0;
}



SDL_Keymod Input::ModState()
{
	return modState;
}



Uint32 Input::MouseState(int *x, int *y)
{
	if(x)
		*x = lround(mousePoint.X());
	if(y)
		*y = lround(mousePoint.Y());
	return touching ? SDL_BUTTON_LMASK : 0;
}



void Input::WarpMouse(int x, int y)
{
	mousePoint = Point(x, y);
	Display::SetMouse(mousePoint);
}



void Input::StartTextInput()
{
	textInput = true;
	RefreshHint();
}



void Input::StopTextInput()
{
	textInput = false;
	RefreshHint();
}



void Input::SetTextTarget(TextTarget target)
{
	if(target == textTarget)
		return;
	textTarget = target;
	RefreshHint();
}



bool Input::IsTextInputActive()
{
	return textInput;
}



void Input::OpenKeyboard()
{
	SwkbdState keyboardState;
	swkbdInit(&keyboardState, SWKBD_TYPE_NORMAL, 2, 100);
	swkbdSetHintText(&keyboardState, "Endless Sky");
	swkbdSetValidation(&keyboardState, SWKBD_ANYTHING, 0, 0);
	char buffer[256] = {};
	SwkbdButton button = swkbdInputText(&keyboardState, buffer, sizeof(buffer));
	if(button != SWKBD_BUTTON_CONFIRM)
		return;

	// Replace whatever is in the text field: move to the end and erase it.
	PressKey(SDLK_END);
	for(int i = 0; i < 100; ++i)
		PressKey(SDLK_BACKSPACE);

	if(!textInput)
	{
		// Type the characters one at a time. Panels apply the shift key
		// themselves, so capital letters are sent as shifted lowercase keys.
		for(const char *it = buffer; *it; ++it)
		{
			char c = *it;
			if(c < ' ' || c > '~')
				continue;
			SDL_Event event = KeyEvent(SDL_KEYDOWN, (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c, false);
			event.key.keysym.mod = (c >= 'A' && c <= 'Z') ? KMOD_LSHIFT : KMOD_NONE;
			events.push_back(event);
		}
		return;
	}

	// Text input events hold at most 31 bytes; split on UTF-8 boundaries.
	size_t length = strlen(buffer);
	size_t start = 0;
	while(start < length)
	{
		size_t end = min(length, start + 31);
		while(end > start && end < length && (static_cast<unsigned char>(buffer[end]) & 0xC0) == 0x80)
			--end;
		SDL_Event event;
		memset(&event, 0, sizeof(event));
		event.type = SDL_TEXTINPUT;
		memcpy(event.text.text, buffer + start, end - start);
		events.push_back(event);
		start = end;
	}
}



const char *Input::KeyName(SDL_Keycode key)
{
	// Report the flight button, since that is where the key bindings matter.
	for(const Binding &binding : FLIGHT_BINDINGS)
		if(binding.key == key)
			return binding.label;

	static const map<SDL_Keycode, const char *> NAMES = {
		{SDLK_UP, "Circle Pad Up"}, {SDLK_DOWN, "Circle Pad Down"},
		{SDLK_LEFT, "Circle Pad Left"}, {SDLK_RIGHT, "Circle Pad Right"},
		{SDLK_RETURN, "Return"}, {SDLK_ESCAPE, "Escape"}, {SDLK_BACKSPACE, "Backspace"},
		{SDLK_TAB, "Tab"}, {SDLK_SPACE, "Space"}, {SDLK_DELETE, "Delete"},
		{SDLK_CAPSLOCK, "CapsLock"}, {SDLK_PAGEUP, "PageUp"}, {SDLK_PAGEDOWN, "PageDown"},
		{SDLK_HOME, "Home"}, {SDLK_END, "End"}, {SDLK_INSERT, "Insert"},
		{SDLK_LSHIFT, "Left Shift"}, {SDLK_RSHIFT, "Right Shift"},
		{SDLK_LCTRL, "Left Ctrl"}, {SDLK_RCTRL, "Right Ctrl"},
		{SDLK_LALT, "Left Alt"}, {SDLK_RALT, "Right Alt"},
		{SDLK_KP_ENTER, "Keypad Enter"}, {SDLK_KP_PLUS, "Keypad +"}, {SDLK_KP_MINUS, "Keypad -"},
	};
	auto it = NAMES.find(key);
	if(it != NAMES.end())
		return it->second;

	static char buffer[16];
	if(key >= SDLK_F1 && key <= SDLK_F12)
		snprintf(buffer, sizeof(buffer), "F%d", static_cast<int>(key - SDLK_F1 + 1));
	else if(key > ' ' && key < 127)
	{
		buffer[0] = (key >= 'a' && key <= 'z') ? static_cast<char>(key - 'a' + 'A') : static_cast<char>(key);
		buffer[1] = '\0';
	}
	else
		buffer[0] = '\0';
	return buffer;
}



SDL_Keycode Input::KeyFromName(const char *name)
{
	if(!name || !*name)
		return SDLK_UNKNOWN;
	for(const Binding &binding : FLIGHT_BINDINGS)
		if(!strcmp(binding.label, name))
			return binding.key;
	if(!name[1])
	{
		char c = name[0];
		if(c >= 'A' && c <= 'Z')
			c = c - 'A' + 'a';
		return c;
	}
	for(SDL_Keycode key : {SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT, SDLK_RETURN, SDLK_ESCAPE,
			SDLK_BACKSPACE, SDLK_TAB, SDLK_SPACE, SDLK_DELETE, SDLK_CAPSLOCK, SDLK_PAGEUP, SDLK_PAGEDOWN,
			SDLK_HOME, SDLK_END})
		if(!strcmp(KeyName(key), name))
			return key;
	return SDLK_UNKNOWN;
}
