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
#include "TextureCache.h"

#include "../Color.h"
#include "../Screen.h"
#include "../text/Font.h"
#include "../text/FontSet.h"

#include <3ds.h>

#include <algorithm>
#include <cmath>
#include <memory>

using namespace std;

namespace {
	constexpr int TOP_WIDTH = 400;
	constexpr int BOTTOM_WIDTH = 320;
	constexpr int SCREEN_HEIGHT = 240;

	// The menu canvas is drawn into this texture at full size.
	constexpr int CANVAS_TEXTURE_SIZE = 1024;
	C3D_Tex canvasTexture{};
	C3D_RenderTarget *canvasTarget = nullptr;

	Display::Mode mode = Display::Mode::MENU;

	// The lens: the part of the menu canvas shown on the bottom screen.
	const double LENS_SCALES[] = {1., .75, .5};
	int lensZoom = 0;
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


	// Draw a textured rectangle from the canvas texture.
	void CanvasQuad(float left, float top, float right, float bottom, float u0, float v0, float u1, float v1)
	{
		Gfx::Vertex *v = Gfx::Triangles(6, Gfx::Material::TEXTURE, &canvasTexture, nullptr, nullptr,
			Gfx::MaterialParams(), Gfx::Blend::REPLACE);
		if(!v)
			return;
		Gfx::Vertex tl{}, tr{}, bl{}, br{};
		for(Gfx::Vertex *c : {&tl, &tr, &bl, &br})
			Gfx::SetColor(*c, 255, 255, 255, 255);
		tl.x = bl.x = left;
		tr.x = br.x = right;
		tl.y = tr.y = top;
		bl.y = br.y = bottom;
		tl.u = bl.u = u0;
		tr.u = br.u = u1;
		tl.v = tr.v = v0;
		bl.v = br.v = v1;
		Gfx::Quad(v, tl, tr, bl, br);
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
	if(!Gfx::Init())
		return false;
	canvasTarget = Gfx::CreateRenderTexture(&canvasTexture, CANVAS_TEXTURE_SIZE, CANVAS_TEXTURE_SIZE);
	return canvasTarget;
}



void Display::Quit()
{
	Gfx::DeleteRenderTexture(canvasTarget, &canvasTexture);
	canvasTarget = nullptr;
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
	if(!canvasTarget)
		return;
	Gfx::SetTextureTarget(canvasTarget, CANVAS_TEXTURE_SIZE, CANVAS_TEXTURE_SIZE);
	// The canvas fills the top left of the texture, at one texel per unit.
	float left = -MENU_WIDTH * .5f;
	float top = -MENU_HEIGHT * .5f;
	Gfx::SetView(left, top, left + CANVAS_TEXTURE_SIZE, top + CANVAS_TEXTURE_SIZE);
}



void Display::PresentCanvas()
{
	if(!canvasTarget)
		return;
	const float u1 = static_cast<float>(MENU_WIDTH) / CANVAS_TEXTURE_SIZE;
	const float v1 = 1.f - static_cast<float>(MENU_HEIGHT) / CANVAS_TEXTURE_SIZE;
	ClampLens();
	Point lensSize = LensSize();
	Point lensTopLeft = lensCenter - lensSize * .5;

	// Top screen: the whole canvas, keeping its aspect ratio.
	Gfx::SetTarget(Gfx::Target::TOP);
	Gfx::SetView(0.f, 0.f, TOP_WIDTH, SCREEN_HEIGHT);
	const float scale = static_cast<float>(SCREEN_HEIGHT) / MENU_HEIGHT;
	const float width = MENU_WIDTH * scale;
	const float x0 = .5f * (TOP_WIDTH - width);
	CanvasQuad(x0, 0.f, x0 + width, SCREEN_HEIGHT, 0.f, 1.f, u1, v1);
	// Show where the lens is, and where the stylus last touched.
	Frame(x0 + lensTopLeft.X() * scale, lensTopLeft.Y() * scale,
		x0 + (lensTopLeft.X() + lensSize.X()) * scale, (lensTopLeft.Y() + lensSize.Y()) * scale,
		1.f, Color(.8f, .7f, .2f, .9f));
	float mx = x0 + mouse.X() * scale;
	float my = mouse.Y() * scale;
	Gfx::FillRect(mx - 1.5f, my - 1.5f, mx + 1.5f, my + 1.5f, Color(1.f, 1.f, 1.f, 1.f));

	// Bottom screen: the lens.
	Gfx::SetTarget(Gfx::Target::BOTTOM);
	Gfx::SetView(0.f, 0.f, BOTTOM_WIDTH, SCREEN_HEIGHT);
	const float s = 1.f / CANVAS_TEXTURE_SIZE;
	CanvasQuad(0.f, 0.f, BOTTOM_WIDTH, SCREEN_HEIGHT,
		lensTopLeft.X() * s, 1.f - lensTopLeft.Y() * s,
		(lensTopLeft.X() + lensSize.X()) * s, 1.f - (lensTopLeft.Y() + lensSize.Y()) * s);
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



void Display::CycleLensZoom()
{
	lensZoom = (lensZoom + 1) % (sizeof(LENS_SCALES) / sizeof(LENS_SCALES[0]));
	ClampLens();
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
