/* Display.cpp
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

#include "Display.h"

#include "Gfx.h"
#include "Input.h"
#include "Platform.h"
#include "TextureCache.h"

#include "../Color.h"
#include "../Logger.h"
#include "../Screen.h"
#include "../text/Font.h"
#include "../text/FontSet.h"

#include <3ds.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

using namespace std;

namespace {
	constexpr int TOP_WIDTH = 400;
	constexpr int BOTTOM_WIDTH = 320;
	constexpr int SCREEN_HEIGHT = 240;

	// The drawing commands of the menu canvas, replayed on both screens.
	Gfx::CommandList canvas;
	bool recordingCanvas = false;

	Display::Mode mode = Display::Mode::MENU;

	// The lens: the part of the menu canvas shown on the bottom screen.
	// Lens sizes, from the most magnified to the least. 1 means one canvas
	// pixel per screen pixel; the menus are made for 1024x768.
	const double LENS_SCALES[] = {1.25, 1., .875, .75, .625, .5};
	constexpr int LENS_ZOOMS = sizeof(LENS_SCALES) / sizeof(LENS_SCALES[0]);
	constexpr int DEFAULT_LENS_ZOOM = 3;
	int lensZoom = DEFAULT_LENS_ZOOM;
	// Center of the lens, relative to the top left of the canvas.
	Point lensCenter(Display::MENU_WIDTH * .5, Display::MENU_HEIGHT * .5);

	Point mouse;
	string hint;
	string toast;
	u64 toastEnd = 0;

	bool drawingHud = false;
	unique_ptr<Screen::ScreenDimensionsGuard> hudGuard;

	// The flight touch buttons, in HUD canvas coordinates (top left origin).
	struct FlightButton {
		const char *label;
		SDL_Keycode key;
		float left;
		float top;
		float right;
		float bottom;
	};
	const FlightButton FLIGHT_BUTTONS[] = {
		{"INFO", SDLK_i, 258.f, 382.f, 310.f, 428.f},
		{"LOG", SDLK_SLASH, 313.f, 382.f, 365.f, 428.f},
		{"CLOAK", SDLK_c, 368.f, 382.f, 420.f, 428.f},
		{"GATHER", SDLK_g, 258.f, 432.f, 310.f, 478.f},
		{"PAUSE", SDLK_p, 313.f, 432.f, 365.f, 478.f},
		{">>", SDLK_CAPSLOCK, 368.f, 432.f, 420.f, 478.f},
	};
	int pressedButton = 0;


	double LensScale()
	{
		return LENS_SCALES[lensZoom];
	}


	Point LensSize()
	{
		return Point(BOTTOM_WIDTH, SCREEN_HEIGHT) / LensScale();
	}


	void ClampLens()
	{
		Point half = LensSize() * .5;
		double x = clamp(lensCenter.X(), half.X(), max(half.X(), Display::MENU_WIDTH - half.X()));
		double y = clamp(lensCenter.Y(), half.Y(), max(half.Y(), Display::MENU_HEIGHT - half.Y()));
		lensCenter = Point(x, y);
	}


	// The lens size is remembered in its own small file of the configuration.
	string LensSettingPath()
	{
		return Platform::ConfigPath() + "3ds lens.txt";
	}


	void LoadLensZoom()
	{
		FILE *file = fopen(LensSettingPath().c_str(), "r");
		if(!file)
			return;
		int value = 0;
		if(fscanf(file, "%d", &value) == 1 && value >= 0 && value < LENS_ZOOMS)
			lensZoom = value;
		fclose(file);
	}


	void SaveLensZoom()
	{
		FILE *file = fopen(LensSettingPath().c_str(), "w");
		if(!file)
			return;
		fprintf(file, "%d\n", lensZoom);
		fclose(file);
	}


	void Frame(float left, float top, float right, float bottom, float width, const Color &color)
	{
		Gfx::FillRect(left, top, right, top + width, color);
		Gfx::FillRect(left, bottom - width, right, bottom, color);
		Gfx::FillRect(left, top, left + width, bottom, color);
		Gfx::FillRect(right - width, top, right, bottom, color);
	}


	// Text on a screen, drawn at its native size (one canvas unit per pixel).
	void DrawMessages()
	{
		const string &text = (osGetTime() < toastEnd) ? toast : hint;
		if(text.empty())
			return;
		const Font &font = FontSet::Get(14);
		Gfx::SetTarget(Gfx::Target::TOP);
		Gfx::SetView(0.f, 0.f, TOP_WIDTH, SCREEN_HEIGHT);
		int width = font.Width(text);
		float x = .5f * (TOP_WIDTH - width);
		Gfx::FillRect(x - 6.f, SCREEN_HEIGHT - 22.f, x + width + 6.f, SCREEN_HEIGHT - 2.f, 0, 0, 0, 200);
		font.Draw(text, Point(x, SCREEN_HEIGHT - 19.f), Color(.9f, .9f, .9f, 1.f));
	}
}



bool Display::Init()
{
	LoadLensZoom();
	return Gfx::Init();
}



void Display::Quit()
{
	canvas.Clear();
	Gfx::Quit();
}



Display::Mode Display::GetMode()
{
	return mode;
}



void Display::SetMode(Mode newMode)
{
	mode = newMode;
	if(mode == Mode::FLIGHT)
		Screen::SetRaw(WORLD_WIDTH, WORLD_HEIGHT, true);
	else
		Screen::SetRaw(MENU_WIDTH, MENU_HEIGHT, true);
}



void Display::BeginFrame(Mode frameMode)
{
	if(frameMode != mode)
		SetMode(frameMode);
	TextureCache::Update();
	Gfx::BeginFrame();
}



void Display::EndFrame()
{
	hudGuard.reset();
	drawingHud = false;
	if(!Gfx::InFrame())
		Gfx::BeginFrame();
	DrawMessages();
	Gfx::EndFrame();
}



void Display::BeginWorld()
{
	hudGuard.reset();
	drawingHud = false;
	Gfx::SetTarget(Gfx::Target::TOP);
	Gfx::SetView(-WORLD_WIDTH * .5f, -WORLD_HEIGHT * .5f, WORLD_WIDTH * .5f, WORLD_HEIGHT * .5f);
}



void Display::BeginHud()
{
	if(mode != Mode::FLIGHT || drawingHud)
		return;
	drawingHud = true;
	Gfx::SetTarget(Gfx::Target::BOTTOM);
	Gfx::SetView(-HUD_WIDTH * .5f, -HUD_HEIGHT * .5f, HUD_WIDTH * .5f, HUD_HEIGHT * .5f);
	hudGuard = make_unique<Screen::ScreenDimensionsGuard>(HUD_WIDTH, HUD_HEIGHT);
}



bool Display::IsDrawingHud()
{
	return drawingHud;
}



void Display::DrawFlightControls()
{
	BeginHud();
	const Font &font = FontSet::Get(14);
	const Color background(.06f, .07f, .09f, .85f);
	const Color pressed(.25f, .3f, .4f, .9f);
	const Color border(.35f, .38f, .45f, 1.f);
	const Color text(.85f, .85f, .85f, 1.f);
	for(const FlightButton &button : FLIGHT_BUTTONS)
	{
		// The button positions are relative to the top left of the HUD.
		float left = button.left - HUD_WIDTH * .5f;
		float right = button.right - HUD_WIDTH * .5f;
		float top = button.top - HUD_HEIGHT * .5f;
		float bottom = button.bottom - HUD_HEIGHT * .5f;
		Gfx::FillRect(left, top, right, bottom, pressedButton == button.key ? pressed : background);
		Frame(left, top, right, bottom, 2.f, border);
		int width = font.Width(button.label);
		font.Draw(button.label, Point(.5f * (left + right) - .5f * width, .5f * (top + bottom) - 7.f), text);
	}
}



int Display::FlightButtonAt(int x, int y)
{
	// Touch pixels to HUD canvas coordinates.
	float hx = x * static_cast<float>(HUD_WIDTH) / BOTTOM_WIDTH;
	float hy = y * static_cast<float>(HUD_HEIGHT) / SCREEN_HEIGHT;
	for(const FlightButton &button : FLIGHT_BUTTONS)
		if(hx >= button.left && hx < button.right && hy >= button.top && hy < button.bottom)
			return button.key;
	return 0;
}



void Display::SetPressedFlightButton(int keycode)
{
	pressedButton = keycode;
}



void Display::BeginCanvas()
{
	Gfx::BeginRecording(canvas, -MENU_WIDTH * .5f, -MENU_HEIGHT * .5f, MENU_WIDTH * .5f, MENU_HEIGHT * .5f);
	recordingCanvas = true;
	// The canvas starts out black.
	Gfx::Clear();
}



void Display::PresentCanvas()
{
	if(!recordingCanvas)
		return;
	Gfx::EndRecording();
	recordingCanvas = false;

	ClampLens();
	Point lensSize = LensSize();
	Point lensTopLeft = lensCenter - lensSize * .5;

	// Top screen: the whole canvas, keeping its aspect ratio.
	const float scale = static_cast<float>(SCREEN_HEIGHT) / MENU_HEIGHT;
	const float halfWidth = .5f * TOP_WIDTH / scale;
	Gfx::SetTarget(Gfx::Target::TOP);
	Gfx::SetView(-halfWidth, -MENU_HEIGHT * .5f, halfWidth, MENU_HEIGHT * .5f);
	Gfx::Replay(canvas);

	// Show where the lens is, and where the stylus last touched.
	Gfx::SetView(0.f, 0.f, TOP_WIDTH, SCREEN_HEIGHT);
	const float x0 = .5f * TOP_WIDTH - MENU_WIDTH * .5f * scale;
	Frame(x0 + lensTopLeft.X() * scale, lensTopLeft.Y() * scale,
		x0 + (lensTopLeft.X() + lensSize.X()) * scale, (lensTopLeft.Y() + lensSize.Y()) * scale,
		1.f, Color(.8f, .7f, .2f, .9f));
	float mx = x0 + mouse.X() * scale;
	float my = mouse.Y() * scale;
	Gfx::FillRect(mx - 1.5f, my - 1.5f, mx + 1.5f, my + 1.5f, Color(1.f, 1.f, 1.f, 1.f));

	// Bottom screen: the lens.
	Gfx::SetTarget(Gfx::Target::BOTTOM);
	float left = lensTopLeft.X() - MENU_WIDTH * .5f;
	float top = lensTopLeft.Y() - MENU_HEIGHT * .5f;
	Gfx::SetView(left, top, left + lensSize.X(), top + lensSize.Y());
	Gfx::Replay(canvas);
}



void Display::MoveLens(double dx, double dy)
{
	lensCenter += Point(dx, dy);
	ClampLens();
}



void Display::CenterLensOn(const Point &canvasPoint)
{
	lensCenter = canvasPoint;
	ClampLens();
}



Point Display::LensCenter()
{
	return lensCenter;
}



void Display::CycleLensZoom()
{
	SetLensZoom((lensZoom + 1) % LENS_ZOOMS);
}



void Display::ZoomLens(int steps)
{
	SetLensZoom(clamp(lensZoom - steps, 0, LENS_ZOOMS - 1));
}



void Display::SetLensZoom(int zoom)
{
	if(zoom != lensZoom)
	{
		lensZoom = zoom;
		SaveLensZoom();
	}
	ClampLens();
	ShowToast("Loupe / Magnifier : " + to_string(static_cast<int>(lround(LensScale() * 100.))) + " %");
}



double Display::LensScale()
{
	return ::LensScale();
}



Point Display::TouchToMouse(int x, int y)
{
	if(mode == Mode::FLIGHT)
		return Point(x * static_cast<double>(HUD_WIDTH) / BOTTOM_WIDTH, y * static_cast<double>(HUD_HEIGHT) / SCREEN_HEIGHT);
	ClampLens();
	Point topLeft = lensCenter - LensSize() * .5;
	return topLeft + Point(x, y) / ::LensScale();
}



void Display::SetMouse(const Point &point)
{
	mouse = point;
}



Point Display::Mouse()
{
	return mouse;
}



void Display::SetHint(const string &text)
{
	hint = text;
}



void Display::ShowToast(const string &text)
{
	toast = text;
	toastEnd = osGetTime() + 3000;
}
