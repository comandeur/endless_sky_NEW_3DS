/* GameWindow.cpp
Copyright (c) 2014 by Michael Zahniser
Nintendo 3DS version copyright (c) 2026 by the Endless Sky 3DS port contributors

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <https://www.gnu.org/licenses/>.
*/

#include "GameWindow.h"

#include "Files.h"
#include "Logger.h"
#include "Screen.h"

#include "ctr/Display.h"
#include "ctr/Platform.h"

using namespace std;

// On the 3DS there is no window: the "window" is the pair of screens, which
// ctr/Display.cpp manages. These functions keep the interface that the rest of
// the game expects.



string GameWindow::SDLVersions()
{
	return "Nintendo 3DS (libctru, citro3d)";
}



bool GameWindow::Init(bool headless)
{
	if(!Display::Init())
	{
		ExitWithError("Unable to initialize the graphics.");
		return false;
	}
	Screen::SetRaw(Display::MENU_WIDTH, Display::MENU_HEIGHT, true);
	return true;
}



void GameWindow::Quit()
{
	Display::Quit();
}



void GameWindow::Step()
{
	Display::EndFrame();
}



void GameWindow::AdjustViewport(bool noResizeEvent)
{
}



bool GameWindow::SetVSync(Preferences::VSync state)
{
	return state == Preferences::VSync::on;
}



int GameWindow::Width()
{
	return Display::MENU_WIDTH;
}



int GameWindow::Height()
{
	return Display::MENU_HEIGHT;
}



int GameWindow::DrawWidth()
{
	return Display::MENU_WIDTH;
}



int GameWindow::DrawHeight()
{
	return Display::MENU_HEIGHT;
}



bool GameWindow::IsMaximized()
{
	return false;
}



bool GameWindow::IsFullscreen()
{
	return true;
}



void GameWindow::ToggleFullscreen()
{
}



void GameWindow::ToggleBlockScreenSaver()
{
}



void GameWindow::ExitWithError(const string &message, bool doPopUp)
{
	Logger::Log(message, Logger::Level::ERROR);
	Platform::FatalError(message);
}
