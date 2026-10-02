/* main.cpp
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

#include "audio/Audio.h"
#include "Command.h"
#include "Conversation.h"
#include "CustomEvents.h"
#include "DataFile.h"
#include "DataNode.h"
#include "Engine.h"
#include "Files.h"
#include "text/Font.h"
#include "text/Format.h"
#include "FrameTimer.h"
#include "GameData.h"
#include "GameLoadingPanel.h"
#include "GameWindow.h"
#include "Information.h"
#include "Interface.h"
#include "Logger.h"
#include "MainPanel.h"
#include "MenuPanel.h"
#include "Panel.h"
#include "PlayerInfo.h"
#include "PluginManager.h"
#include "Preferences.h"
#include "Screen.h"
#include "image/SpriteSet.h"
#include "shader/SpriteShader.h"
#include "TaskQueue.h"
#include "UI.h"

#include "ctr/Display.h"
#include "ctr/Gfx.h"
#include "ctr/Input.h"
#include "ctr/Platform.h"
#include "ctr/TextureCache.h"

#include <chrono>
#include <exception>
#include <string>

using namespace std;

namespace {
	// Settings that are too expensive for the 3DS, or meaningless on it.
	void ApplyPlatformPreferences()
	{
		Preferences::Set("Render motion blur", false);
		Preferences::Set("fullscreen", true);
		Preferences::Set("maximized", false);
	}


	// Which screen layout the current state of the panels calls for.
	Display::Mode CurrentMode(const UI &menuPanels, const UI &gamePanels)
	{
		bool inFlight = menuPanels.IsEmpty() && !gamePanels.IsEmpty() && gamePanels.Root() == gamePanels.Top();
		return inFlight ? Display::Mode::FLIGHT : Display::Mode::MENU;
	}
}

void GameLoop(PlayerInfo &player, TaskQueue &queue, const Conversation &conversation);
void DrawFrame(UI &menuPanels, UI &gamePanels, Display::Mode mode, bool isFastForward,
	std::chrono::steady_clock::duration lastFrameTime, int step);



// Entry point for the Nintendo 3DS version of Endless Sky.
int main(int argc, char *argv[])
{
	if(!Platform::Init())
		return 1;

	PlayerInfo player;
	Conversation conversation;

	Logger::SetLogCallback([](const string &errorMessage, Logger::Level level)
	{
		Files::LogErrorToFile(errorMessage);
	});

	// Files::Init() expects a null terminated argument list.
	const char *arguments[] = {"endless-sky", nullptr};
	try {
		Files::Init(arguments);
	}
	catch(const exception &error)
	{
		Platform::FatalError(string(error.what()) + "\n\nCopy the converted game data to "
			+ Platform::ResourcePath() + " (see the README).");
	}

	Logger::Session logSession{false};

	try {
		Preferences::Load();
		ApplyPlatformPreferences();
		PluginManager::LoadSettings();

		TaskQueue queue;

		// The window must exist before the sprites are loaded, since their
		// textures live in the GPU's memory.
		if(!GameWindow::Init(false))
			return 1;
		Input::Init();

		// Begin loading the game data.
		auto dataFuture = GameData::BeginLoad(queue, player, false, false, false);

		// Load global conditions:
		DataFile globalConditions(Files::Config() / "global conditions.txt");
		for(const DataNode &node : globalConditions)
			if(node.Token(0) == "conditions")
				GameData::GlobalConditions().Load(node);

		GameData::LoadSettings();
		GameData::LoadShaders();

		Audio::Init(GameData::Sources());

		CustomEvents::Init();
		GameLoop(player, queue, conversation);
	}
	catch(const exception &error)
	{
		Audio::Quit();
		GameWindow::ExitWithError(error.what());
		return 1;
	}

	Screen::SetRaw(GameWindow::Width(), GameWindow::Height(), true);
	Preferences::Save();
	PluginManager::Save();

	Audio::Quit();
	TextureCache::Quit();
	GameWindow::Quit();
	Platform::Quit();

	return 0;
}



void GameLoop(PlayerInfo &player, TaskQueue &queue, const Conversation &conversation)
{
	// gamePanels is used for the main panel where you fly your spaceship.
	// All other game content related dialogs are placed on top of the gamePanels.
	// If there are both menuPanels and gamePanels, then the menuPanels take
	// priority over the gamePanels. The gamePanels will not be shown until
	// the stack of menuPanels is empty.
	UI gamePanels;

	// menuPanels is used for the panels related to pilot creation, preferences,
	// game loading and game saving.
	UI menuPanels;

	bool dataFinishedLoading = false;
	menuPanels.Push(new GameLoadingPanel(player, queue, conversation, gamePanels, dataFinishedLoading));

	FrameTimer timer(60);
	bool isFastForward = false;
	// Skip drawing a frame when the previous one took too long, so that the
	// game keeps running at full speed (the simulation is tied to 60 steps per
	// second) and only the frame rate drops.
	bool skippedLastFrame = false;
	chrono::steady_clock::duration lastFrameTime{};
	constexpr auto FRAME_BUDGET = chrono::microseconds(16667);

	int step = 0;

	Display::Mode mode = Display::Mode::MENU;
	Display::BeginFrame(mode);
	Display::EndFrame();

	while(!menuPanels.IsDone())
	{
		chrono::steady_clock::time_point start = chrono::steady_clock::now();
		if(++step == 60)
			step = 0;

		// Choose the screen layout. When it changes, the panels are told about
		// their new canvas size right away.
		Display::Mode newMode = CurrentMode(menuPanels, gamePanels);
		if(newMode != mode)
		{
			mode = newMode;
			Display::SetMode(mode);
			menuPanels.AdjustViewport();
			gamePanels.AdjustViewport();
		}

		if(!Input::Update(mode))
			menuPanels.Quit();

		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			UI &activeUI = (menuPanels.IsEmpty() ? gamePanels : menuPanels);

			SDL_Keycode eventKeyCode = event.key.keysym.sym;
			if(event.type == SDL_KEYDOWN && menuPanels.IsEmpty()
					&& Command(eventKeyCode).Has(Command::MENU)
					&& !gamePanels.IsEmpty() && gamePanels.Top()->IsInterruptible())
			{
				// User pressed the Menu key.
				menuPanels.Push(shared_ptr<Panel>(new MenuPanel(player, gamePanels)));
				UI::PlaySound(UI::UISound::NORMAL);
			}
			else if(event.type == SDL_QUIT)
				menuPanels.Quit();
			else if(event.type == CustomEvents::GetResize())
			{
				menuPanels.AdjustViewport();
				gamePanels.AdjustViewport();
			}
			else if(event.type == CustomEvents::GetAdjustText())
			{
				menuPanels.AdjustTextDisplay();
				gamePanels.AdjustTextDisplay();
			}
			else if(event.type == SDL_KEYDOWN && Command(eventKeyCode).Has(Command::PERFORMANCE_DISPLAY))
				Preferences::Set("Show CPU / GPU load", !Preferences::Has("Show CPU / GPU load"));
			else if(activeUI.Handle(event))
			{
				// The UI handled the event.
			}
			else if(event.type == SDL_KEYDOWN && !event.key.repeat
					&& Command(eventKeyCode).Has(Command::FASTFORWARD))
				isFastForward = !isFastForward;
		}

		bool inFlight = (mode == Display::Mode::FLIGHT);
		bool allowFastForward = !gamePanels.IsEmpty() && gamePanels.Top()->AllowsFastForward();
		if(Preferences::Has("Interrupt fast-forward") && !inFlight && isFastForward && !allowFastForward)
			isFastForward = false;

		// Tell all the panels to step forward, then draw them.
		(menuPanels.IsEmpty() ? gamePanels : menuPanels).StepAll();

		// In fast-forward, only one step in three is drawn.
		bool draw = !(isFastForward && inFlight && step % 3);
		if(draw)
		{
			Audio::Step(isFastForward);

			// Panels may have opened or closed during this step: wait for the
			// next frame to draw in the new layout. Also skip a frame when the
			// previous one took too long, so that the game keeps running at
			// full speed and only the frame rate drops.
			bool modeChanged = CurrentMode(menuPanels, gamePanels) != mode;
			bool tooSlow = !skippedLastFrame && lastFrameTime > FRAME_BUDGET;
			draw = !modeChanged && !tooSlow;
			skippedLastFrame = !draw;
		}
		if(draw)
			DrawFrame(menuPanels, gamePanels, mode, isFastForward, lastFrameTime, step);

		lastFrameTime = chrono::steady_clock::now() - start;

		// Lock the game loop to 60 FPS.
		timer.Wait();

		// If the player ended this frame in-game, count the elapsed time as played time.
		if(menuPanels.IsEmpty())
			player.AddPlayTime(chrono::steady_clock::now() - start);
	}

	// If player quit while landed on a planet, save the game if there are changes.
	if(player.GetPlanet() && gamePanels.CanSave())
		player.Save();
}



void DrawFrame(UI &menuPanels, UI &gamePanels, Display::Mode mode, bool isFastForward,
	chrono::steady_clock::duration lastFrameTime, int step)
{
	static string memoryString;
	bool inFlight = (mode == Display::Mode::FLIGHT);

	Display::BeginFrame(mode);
	UI &visible = menuPanels.IsEmpty() ? gamePanels : menuPanels;
	if(inFlight)
		Display::BeginWorld();
	else
		Display::BeginCanvas();

	visible.DrawAll();

	// The engine moves to the bottom screen to draw the HUD; these overlays
	// belong to the top screen.
	MainPanel *mainPanel = static_cast<MainPanel *>(gamePanels.Root().get());
	if(inFlight)
		Display::BeginWorld();
	if(mainPanel && mainPanel->GetEngine().IsPaused())
		SpriteShader::Draw(SpriteSet::Get("ui/paused"), Screen::TopLeft() + Point(10., 10.));
	else if(isFastForward)
		SpriteShader::Draw(SpriteSet::Get("ui/fast forward"), Screen::TopLeft() + Point(10., 10.));

	if(Preferences::Has("Show CPU / GPU load"))
	{
		if(!step || memoryString.empty())
			memoryString = "MEM: " + Format::Number(Platform::HeapUsed() / 1048576., 1, false)
				+ " MB, TEX: " + Format::Number(TextureCache::BytesUsed() / 1048576., 1, false)
				+ " / " + Format::Number(TextureCache::Budget() / 1048576., 1, false) + " MB";
		Information performanceInfo;
		performanceInfo.SetString("cpu", "CPU: " + Format::Number(
			chrono::duration_cast<chrono::microseconds>(lastFrameTime).count() / 1000., 1, false) + " ms");
		performanceInfo.SetString("gpu", "Draws: " + to_string(Gfx::DrawCallsLastFrame()));
		performanceInfo.SetString("mem", memoryString);
		performanceInfo.SetCondition("ready");
		static const Interface &performanceDisplay = *GameData::Interfaces().Get("performance info");
		performanceDisplay.Draw(performanceInfo);
	}

	if(inFlight)
		Display::DrawFlightControls();
	else
		Display::PresentCanvas();
	GameWindow::Step();
}
