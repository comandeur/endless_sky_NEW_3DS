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
#include <algorithm>
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

	// A release build carries the game data in its romfs, which then always
	// matches the program. Otherwise the data is on the SD card.
	if(romfsMounted && FileExists(string(ROMFS_ROOT) + "credits.txt"))
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
	// Keep a copy of the message: the error screen is easy to dismiss.
	FILE *file = fopen("sdmc:/3ds/endless-sky/fatal-error.txt", "w");
	if(file)
	{
		fputs(message.c_str(), file);
		fputc('\n', file);
		fclose(file);
	}

	errorConf error;
	errorInit(&error, ERROR_TEXT_WORD_WRAP, CFG_LANGUAGE_EN);
	errorText(&error, ("Endless Sky has encountered an error and must close:\n\n" + message).c_str());
	errorDisp(&error);
	exit(1);
}



// Split the application's memory between the heap and the linear heap (the
// memory that the GPU and the DSP can access). libctru caps the linear heap at
// 32 MB, which is too little for the game's textures; here the heap gets what
// the game data needs and the linear heap everything else (about 52 MB in the
// New 3DS memory mode). This replaces libctru's weak implementation.
extern "C" {
	extern char *fake_heap_start;
	extern u32 __ctru_heap;
	extern u32 __ctru_heap_size;
	extern u32 __ctru_linear_heap;
	extern u32 __ctru_linear_heap_size;

	void __system_allocateHeaps(void)
	{
		Handle reslimit = 0;
		if(R_FAILED(svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE)))
			svcBreak(USERBREAK_PANIC);
		s64 maxCommit = 0;
		s64 currentCommit = 0;
		ResourceLimitType reslimitType = RESLIMIT_COMMIT;
		svcGetResourceLimitLimitValues(&maxCommit, reslimit, &reslimitType, 1);
		svcGetResourceLimitCurrentValues(&currentCommit, reslimit, &reslimitType, 1);
		svcCloseHandle(reslimit);

		// The game data needs about 50 MB of regular heap once loaded, plus
		// room for playing. Everything else goes to the linear heap, which
		// holds the textures: the more it has, the less often they are loaded.
		constexpr u32 HEAP_TARGET = 64 << 20;
		constexpr u32 LINEAR_MIN = 16 << 20;
		constexpr u32 LINEAR_MAX = 80 << 20;
		u32 remaining = static_cast<u32>(maxCommit - currentCommit) & ~0xFFF;
		u32 linear = remaining > HEAP_TARGET ? remaining - HEAP_TARGET : 0;
		linear = std::max(LINEAR_MIN, std::min(linear, LINEAR_MAX));
		if(linear > remaining / 2)
			linear = (remaining / 2) & ~0xFFF;
		__ctru_linear_heap_size = linear;
		__ctru_heap_size = remaining - linear;

		if(R_FAILED(svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, __ctru_heap_size, MEMOP_ALLOC,
				static_cast<MemPerm>(MEMPERM_READ | MEMPERM_WRITE))))
			svcBreak(USERBREAK_PANIC);
		if(R_FAILED(svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, __ctru_linear_heap_size, MEMOP_ALLOC_LINEAR,
				static_cast<MemPerm>(MEMPERM_READ | MEMPERM_WRITE))))
			svcBreak(USERBREAK_PANIC);

		mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

		fake_heap_start = reinterpret_cast<char *>(__ctru_heap);
		fake_heap_end = fake_heap_start + __ctru_heap_size;
	}
}
