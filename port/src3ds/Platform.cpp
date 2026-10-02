/* Platform.cpp
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

#include "Platform.h"

#include <3ds.h>

#include <malloc.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>

using namespace std;

// Not declared by newlib's headers in strict C++ mode.
extern "C" void *sbrk(ptrdiff_t increment);
extern char *fake_heap_end;

namespace {
	// The game data is installed on the SD card. If it is missing, the RomFS
	// of the executable is tried as well, so that a self-contained build works.
	const char *const SD_ROOT = "sdmc:/3ds/endless-sky/";
	const char *const ROMFS_ROOT = "romfs:/";

	string resourcePath;
	string configPath;
	bool romfsMounted = false;
	bool isNew3DS = false;

	bool FileExists(const string &path)
	{
		struct stat info;
		return !stat(path.c_str(), &info);
	}
}



bool Platform::Init()
{
	osSetSpeedupEnable(true);
	APT_CheckNew3DS(&isNew3DS);
	// Allow a worker thread on the system core.
	APT_SetAppCpuTimeLimit(30);

	romfsMounted = R_SUCCEEDED(romfsInit());

	if(FileExists(string(SD_ROOT) + "credits.txt"))
		resourcePath = SD_ROOT;
	else if(romfsMounted && FileExists(string(ROMFS_ROOT) + "credits.txt"))
		resourcePath = ROMFS_ROOT;
	else
		resourcePath = SD_ROOT;

	configPath = string(SD_ROOT) + "config/";
	mkdir("sdmc:/3ds", 0777);
	mkdir(SD_ROOT, 0777);
	mkdir(configPath.c_str(), 0777);
	return true;
}



void Platform::Quit()
{
	if(romfsMounted)
		romfsExit();
}



const string &Platform::ResourcePath()
{
	return resourcePath;
}



const string &Platform::ConfigPath()
{
	return configPath;
}



bool Platform::IsNew3DS()
{
	return isNew3DS;
}



size_t Platform::HeapUsed()
{
	struct mallinfo info = mallinfo();
	return info.uordblks;
}



size_t Platform::HeapFree()
{
	struct mallinfo info = mallinfo();
	// Memory in the free lists, plus what the heap can still grow into.
	char *top = static_cast<char *>(sbrk(0));
	return info.fordblks + (fake_heap_end > top ? fake_heap_end - top : 0);
}



size_t Platform::LinearFree()
{
	return linearSpaceFree();
}



void Platform::FatalError(const string &message)
{
	errorConf error;
	errorInit(&error, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
	errorText(&error, ("Endless Sky has encountered an error and must close:\n\n" + message).c_str());
	errorDisp(&error);
	exit(1);
}
