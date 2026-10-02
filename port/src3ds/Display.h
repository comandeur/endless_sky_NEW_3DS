/* Display.h
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

#include "../Point.h"

#include <string>



// Decides what is shown on each of the two screens of the 3DS.
//
// FLIGHT mode (only the main flight panel is open):
//   - top screen: the game world, on a small canvas so that ships are big enough;
//   - bottom screen: the heads-up display (radar, status, target, messages...)
//     drawn on its own canvas, plus touch buttons for the flight commands.
//
// MENU mode (any other panel, e.g. the main menu, a planet, the map, a shop):
//   - the panels are drawn on a large virtual canvas, like a desktop window;
//   - top screen: the whole canvas, scaled down, as an overview;
//   - bottom screen: a "magnifying glass" on part of the canvas at full size,
//     which is what the touch screen interacts with. The circle pad moves it.
namespace Display {
	enum class Mode {
		FLIGHT,
		MENU,
	};

	// Canvas sizes, in game units.
	constexpr int MENU_WIDTH = 1024;
	constexpr int MENU_HEIGHT = 768;
	constexpr int WORLD_WIDTH = 600;
	constexpr int WORLD_HEIGHT = 360;
	constexpr int HUD_WIDTH = 640;
	constexpr int HUD_HEIGHT = 480;

	bool Init();
	void Quit();

	Mode GetMode();
	// Switch the layout. This changes the canvas size that the panels see.
	void SetMode(Mode mode);

	// Frame structure.
	void BeginFrame(Mode mode);
	void EndFrame();

	// FLIGHT mode: switch between drawing the world (top) and the HUD (bottom).
	void BeginWorld();
	void BeginHud();
	bool IsDrawingHud();
	// Draw the touch buttons; call after the HUD.
	void DrawFlightControls();
	// The key code of the flight touch button under the given bottom screen
	// pixel, or 0 if there is none.
	int FlightButtonAt(int x, int y);
	// Highlight the pressed button.
	void SetPressedFlightButton(int keycode);

	// MENU mode: draw the panels into the canvas, then show it on both screens.
	void BeginCanvas();
	void PresentCanvas();

	// Lens (bottom screen view of the menu canvas).
	void MoveLens(double dx, double dy);
	void CenterLensOn(const Point &canvasPoint);
	void CycleLensZoom();
	double LensScale();

	// Map a touch on the bottom screen (in pixels) to the coordinates that the
	// game expects for a mouse event: in MENU mode, a point of the canvas
	// relative to its top left corner; in FLIGHT mode, a point of the HUD canvas.
	Point TouchToMouse(int x, int y);
	// The last mouse position, in canvas coordinates relative to the top left.
	void SetMouse(const Point &point);
	Point Mouse();

	// Show a short hint on the top screen (e.g. "Y: keyboard").
	void SetHint(const std::string &text);
	// Show a status message (loading, errors...) for a few seconds.
	void ShowToast(const std::string &text);
}
