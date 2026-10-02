/* SDL2/SDL.h
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

// Nintendo 3DS replacement for the SDL2 headers.
//
// The game code only uses a small part of SDL2: key codes, a handful of event
// structures and a few query functions. This header provides exactly that
// subset with the same names and numeric values as SDL2 (so saved key bindings
// stay compatible with the desktop game), implemented on top of libctru in
// ctr/SDLShim.cpp. The upstream source/SDL.h wrapper includes this header.

#include <cstddef>
#include <cstdint>

using Uint8 = uint8_t;
using Uint16 = uint16_t;
using Uint32 = uint32_t;
using Uint64 = uint64_t;
using Sint16 = int16_t;
using Sint32 = int32_t;

#define SDL_MAJOR_VERSION 2
#define SDL_MINOR_VERSION 30
#define SDL_PATCHLEVEL 0
#define SDL_VERSIONNUM(X, Y, Z) ((X) * 1000 + (Y) * 100 + (Z))
#define SDL_COMPILEDVERSION SDL_VERSIONNUM(SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL)
#define SDL_VERSION_ATLEAST(X, Y, Z) (SDL_COMPILEDVERSION >= SDL_VERSIONNUM(X, Y, Z))

// Key codes.
// SDL2 declares this as Sint32; on the 3DS that is a long, while the game
// expects an int (e.g. in Command::Command(int)).
using SDL_Keycode = int;
using SDL_Scancode = int;

#define SDLK_SCANCODE_MASK (1 << 30)
#define SDL_SCANCODE_TO_KEYCODE(X) ((X) | SDLK_SCANCODE_MASK)

enum : SDL_Keycode {
	SDLK_UNKNOWN = 0,
	SDLK_RETURN = '\r',
	SDLK_ESCAPE = '\x1B',
	SDLK_BACKSPACE = '\b',
	SDLK_TAB = '\t',
	SDLK_SPACE = ' ',
	SDLK_EXCLAIM = '!',
	SDLK_QUOTEDBL = '"',
	SDLK_HASH = '#',
	SDLK_PERCENT = '%',
	SDLK_DOLLAR = '$',
	SDLK_AMPERSAND = '&',
	SDLK_QUOTE = '\'',
	SDLK_LEFTPAREN = '(',
	SDLK_RIGHTPAREN = ')',
	SDLK_ASTERISK = '*',
	SDLK_PLUS = '+',
	SDLK_COMMA = ',',
	SDLK_MINUS = '-',
	SDLK_PERIOD = '.',
	SDLK_SLASH = '/',
	SDLK_0 = '0', SDLK_1 = '1', SDLK_2 = '2', SDLK_3 = '3', SDLK_4 = '4',
	SDLK_5 = '5', SDLK_6 = '6', SDLK_7 = '7', SDLK_8 = '8', SDLK_9 = '9',
	SDLK_COLON = ':',
	SDLK_SEMICOLON = ';',
	SDLK_LESS = '<',
	SDLK_EQUALS = '=',
	SDLK_GREATER = '>',
	SDLK_QUESTION = '?',
	SDLK_AT = '@',
	SDLK_LEFTBRACKET = '[',
	SDLK_BACKSLASH = '\\',
	SDLK_RIGHTBRACKET = ']',
	SDLK_CARET = '^',
	SDLK_UNDERSCORE = '_',
	SDLK_BACKQUOTE = '`',
	SDLK_a = 'a', SDLK_b = 'b', SDLK_c = 'c', SDLK_d = 'd', SDLK_e = 'e', SDLK_f = 'f',
	SDLK_g = 'g', SDLK_h = 'h', SDLK_i = 'i', SDLK_j = 'j', SDLK_k = 'k', SDLK_l = 'l',
	SDLK_m = 'm', SDLK_n = 'n', SDLK_o = 'o', SDLK_p = 'p', SDLK_q = 'q', SDLK_r = 'r',
	SDLK_s = 's', SDLK_t = 't', SDLK_u = 'u', SDLK_v = 'v', SDLK_w = 'w', SDLK_x = 'x',
	SDLK_y = 'y', SDLK_z = 'z',
	SDLK_DELETE = '\x7F',

	SDLK_CAPSLOCK = SDL_SCANCODE_TO_KEYCODE(57),
	SDLK_F1 = SDL_SCANCODE_TO_KEYCODE(58),
	SDLK_F2 = SDL_SCANCODE_TO_KEYCODE(59),
	SDLK_F3 = SDL_SCANCODE_TO_KEYCODE(60),
	SDLK_F4 = SDL_SCANCODE_TO_KEYCODE(61),
	SDLK_F5 = SDL_SCANCODE_TO_KEYCODE(62),
	SDLK_F6 = SDL_SCANCODE_TO_KEYCODE(63),
	SDLK_F7 = SDL_SCANCODE_TO_KEYCODE(64),
	SDLK_F8 = SDL_SCANCODE_TO_KEYCODE(65),
	SDLK_F9 = SDL_SCANCODE_TO_KEYCODE(66),
	SDLK_F10 = SDL_SCANCODE_TO_KEYCODE(67),
	SDLK_F11 = SDL_SCANCODE_TO_KEYCODE(68),
	SDLK_F12 = SDL_SCANCODE_TO_KEYCODE(69),
	SDLK_PRINTSCREEN = SDL_SCANCODE_TO_KEYCODE(70),
	SDLK_SCROLLLOCK = SDL_SCANCODE_TO_KEYCODE(71),
	SDLK_PAUSE = SDL_SCANCODE_TO_KEYCODE(72),
	SDLK_INSERT = SDL_SCANCODE_TO_KEYCODE(73),
	SDLK_HOME = SDL_SCANCODE_TO_KEYCODE(74),
	SDLK_PAGEUP = SDL_SCANCODE_TO_KEYCODE(75),
	SDLK_END = SDL_SCANCODE_TO_KEYCODE(77),
	SDLK_PAGEDOWN = SDL_SCANCODE_TO_KEYCODE(78),
	SDLK_RIGHT = SDL_SCANCODE_TO_KEYCODE(79),
	SDLK_LEFT = SDL_SCANCODE_TO_KEYCODE(80),
	SDLK_DOWN = SDL_SCANCODE_TO_KEYCODE(81),
	SDLK_UP = SDL_SCANCODE_TO_KEYCODE(82),
	SDLK_NUMLOCKCLEAR = SDL_SCANCODE_TO_KEYCODE(83),
	SDLK_KP_DIVIDE = SDL_SCANCODE_TO_KEYCODE(84),
	SDLK_KP_MULTIPLY = SDL_SCANCODE_TO_KEYCODE(85),
	SDLK_KP_MINUS = SDL_SCANCODE_TO_KEYCODE(86),
	SDLK_KP_PLUS = SDL_SCANCODE_TO_KEYCODE(87),
	SDLK_KP_ENTER = SDL_SCANCODE_TO_KEYCODE(88),
	SDLK_KP_1 = SDL_SCANCODE_TO_KEYCODE(89),
	SDLK_KP_2 = SDL_SCANCODE_TO_KEYCODE(90),
	SDLK_KP_3 = SDL_SCANCODE_TO_KEYCODE(91),
	SDLK_KP_4 = SDL_SCANCODE_TO_KEYCODE(92),
	SDLK_KP_5 = SDL_SCANCODE_TO_KEYCODE(93),
	SDLK_KP_6 = SDL_SCANCODE_TO_KEYCODE(94),
	SDLK_KP_7 = SDL_SCANCODE_TO_KEYCODE(95),
	SDLK_KP_8 = SDL_SCANCODE_TO_KEYCODE(96),
	SDLK_KP_9 = SDL_SCANCODE_TO_KEYCODE(97),
	SDLK_KP_0 = SDL_SCANCODE_TO_KEYCODE(98),
	SDLK_KP_PERIOD = SDL_SCANCODE_TO_KEYCODE(99),
	SDLK_APPLICATION = SDL_SCANCODE_TO_KEYCODE(101),
	SDLK_POWER = SDL_SCANCODE_TO_KEYCODE(102),
	SDLK_KP_EQUALS = SDL_SCANCODE_TO_KEYCODE(103),
	SDLK_KP_COMMA = SDL_SCANCODE_TO_KEYCODE(133),
	SDLK_KP_LEFTPAREN = SDL_SCANCODE_TO_KEYCODE(182),
	SDLK_KP_RIGHTPAREN = SDL_SCANCODE_TO_KEYCODE(183),
	SDLK_KP_LEFTBRACE = SDL_SCANCODE_TO_KEYCODE(184),
	SDLK_KP_RIGHTBRACE = SDL_SCANCODE_TO_KEYCODE(185),
	SDLK_KP_TAB = SDL_SCANCODE_TO_KEYCODE(186),
	SDLK_KP_BACKSPACE = SDL_SCANCODE_TO_KEYCODE(187),
	SDLK_KP_A = SDL_SCANCODE_TO_KEYCODE(188),
	SDLK_KP_B = SDL_SCANCODE_TO_KEYCODE(189),
	SDLK_KP_C = SDL_SCANCODE_TO_KEYCODE(190),
	SDLK_KP_D = SDL_SCANCODE_TO_KEYCODE(191),
	SDLK_KP_E = SDL_SCANCODE_TO_KEYCODE(192),
	SDLK_KP_F = SDL_SCANCODE_TO_KEYCODE(193),
	SDLK_KP_XOR = SDL_SCANCODE_TO_KEYCODE(194),
	SDLK_KP_POWER = SDL_SCANCODE_TO_KEYCODE(195),
	SDLK_KP_PERCENT = SDL_SCANCODE_TO_KEYCODE(196),
	SDLK_KP_LESS = SDL_SCANCODE_TO_KEYCODE(197),
	SDLK_KP_GREATER = SDL_SCANCODE_TO_KEYCODE(198),
	SDLK_KP_AMPERSAND = SDL_SCANCODE_TO_KEYCODE(199),
	SDLK_KP_DBLAMPERSAND = SDL_SCANCODE_TO_KEYCODE(200),
	SDLK_KP_VERTICALBAR = SDL_SCANCODE_TO_KEYCODE(201),
	SDLK_KP_DBLVERTICALBAR = SDL_SCANCODE_TO_KEYCODE(202),
	SDLK_KP_COLON = SDL_SCANCODE_TO_KEYCODE(203),
	SDLK_KP_HASH = SDL_SCANCODE_TO_KEYCODE(204),
	SDLK_KP_SPACE = SDL_SCANCODE_TO_KEYCODE(205),
	SDLK_KP_AT = SDL_SCANCODE_TO_KEYCODE(206),
	SDLK_KP_EXCLAM = SDL_SCANCODE_TO_KEYCODE(207),
	SDLK_LCTRL = SDL_SCANCODE_TO_KEYCODE(224),
	SDLK_LSHIFT = SDL_SCANCODE_TO_KEYCODE(225),
	SDLK_LALT = SDL_SCANCODE_TO_KEYCODE(226),
	SDLK_LGUI = SDL_SCANCODE_TO_KEYCODE(227),
	SDLK_RCTRL = SDL_SCANCODE_TO_KEYCODE(228),
	SDLK_RSHIFT = SDL_SCANCODE_TO_KEYCODE(229),
	SDLK_RALT = SDL_SCANCODE_TO_KEYCODE(230),
	SDLK_RGUI = SDL_SCANCODE_TO_KEYCODE(231),
	SDLK_AC_BACK = SDL_SCANCODE_TO_KEYCODE(270),
};

// Number of entries in the array returned by SDL_GetKeyboardState().
#define SDL_NUM_SCANCODES 640

// Key modifiers.
using SDL_Keymod = Uint16;
enum : Uint16 {
	KMOD_NONE = 0x0000,
	KMOD_LSHIFT = 0x0001,
	KMOD_RSHIFT = 0x0002,
	KMOD_LCTRL = 0x0040,
	KMOD_RCTRL = 0x0080,
	KMOD_LALT = 0x0100,
	KMOD_RALT = 0x0200,
	KMOD_LGUI = 0x0400,
	KMOD_RGUI = 0x0800,
	KMOD_NUM = 0x1000,
	KMOD_CAPS = 0x2000,
	KMOD_CTRL = KMOD_LCTRL | KMOD_RCTRL,
	KMOD_SHIFT = KMOD_LSHIFT | KMOD_RSHIFT,
	KMOD_ALT = KMOD_LALT | KMOD_RALT,
	KMOD_GUI = KMOD_LGUI | KMOD_RGUI,
};

// Mouse buttons.
#define SDL_BUTTON(X) (1 << ((X) - 1))
#define SDL_BUTTON_LEFT 1
#define SDL_BUTTON_MIDDLE 2
#define SDL_BUTTON_RIGHT 3
#define SDL_BUTTON_LMASK SDL_BUTTON(SDL_BUTTON_LEFT)
#define SDL_BUTTON_MMASK SDL_BUTTON(SDL_BUTTON_MIDDLE)
#define SDL_BUTTON_RMASK SDL_BUTTON(SDL_BUTTON_RIGHT)

#define SDL_PRESSED 1
#define SDL_RELEASED 0

// Events.
enum : Uint32 {
	SDL_FIRSTEVENT = 0,
	SDL_QUIT = 0x100,
	SDL_WINDOWEVENT = 0x200,
	SDL_KEYDOWN = 0x300,
	SDL_KEYUP,
	SDL_TEXTEDITING,
	SDL_TEXTINPUT,
	SDL_MOUSEMOTION = 0x400,
	SDL_MOUSEBUTTONDOWN,
	SDL_MOUSEBUTTONUP,
	SDL_MOUSEWHEEL,
	SDL_USEREVENT = 0x8000,
	SDL_LASTEVENT = 0xFFFF
};

enum : Uint8 {
	SDL_WINDOWEVENT_NONE = 0,
	SDL_WINDOWEVENT_SIZE_CHANGED = 6,
};

struct SDL_Keysym {
	SDL_Scancode scancode;
	SDL_Keycode sym;
	Uint16 mod;
	Uint32 unused;
};

struct SDL_KeyboardEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint8 state;
	Uint8 repeat;
	SDL_Keysym keysym;
};

struct SDL_TextInputEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	char text[32];
};

struct SDL_MouseMotionEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint32 which;
	Uint32 state;
	Sint32 x;
	Sint32 y;
	Sint32 xrel;
	Sint32 yrel;
};

struct SDL_MouseButtonEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint32 which;
	Uint8 button;
	Uint8 state;
	Uint8 clicks;
	Sint32 x;
	Sint32 y;
};

struct SDL_MouseWheelEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint32 which;
	Sint32 x;
	Sint32 y;
};

struct SDL_WindowEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Uint8 event;
	Sint32 data1;
	Sint32 data2;
};

struct SDL_UserEvent {
	Uint32 type;
	Uint32 timestamp;
	Uint32 windowID;
	Sint32 code;
	void *data1;
	void *data2;
};

union SDL_Event {
	Uint32 type;
	SDL_KeyboardEvent key;
	SDL_TextInputEvent text;
	SDL_MouseMotionEvent motion;
	SDL_MouseButtonEvent button;
	SDL_MouseWheelEvent wheel;
	SDL_WindowEvent window;
	SDL_UserEvent user;
};

struct SDL_Window;

// Functions implemented by the 3DS platform layer.
int SDL_PollEvent(SDL_Event *event);
int SDL_PushEvent(SDL_Event *event);
Uint32 SDL_RegisterEvents(int count);

const Uint8 *SDL_GetKeyboardState(int *numkeys);
SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key);
SDL_Keymod SDL_GetModState();
const char *SDL_GetKeyName(SDL_Keycode key);
SDL_Keycode SDL_GetKeyFromName(const char *name);

Uint32 SDL_GetMouseState(int *x, int *y);
void SDL_WarpMouseInWindow(SDL_Window *window, int x, int y);

void SDL_StartTextInput();
void SDL_StopTextInput();

Uint32 SDL_GetTicks();
void SDL_Delay(Uint32 ms);

int SDL_SetClipboardText(const char *text);
char *SDL_GetClipboardText();
bool SDL_HasClipboardText();

int SDL_OpenURL(const char *url);
const char *SDL_GetError();
void SDL_free(void *mem);
char *SDL_GetBasePath();
char *SDL_GetPrefPath(const char *org, const char *app);
