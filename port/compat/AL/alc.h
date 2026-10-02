/* AL/alc.h
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

#include "al.h"

typedef char ALCboolean;
typedef char ALCchar;
typedef int ALCint;

struct ALCdevice;
struct ALCcontext;

ALCdevice *alcOpenDevice(const ALCchar *name);
ALCboolean alcCloseDevice(ALCdevice *device);
ALCcontext *alcCreateContext(ALCdevice *device, const ALCint *attributes);
ALCboolean alcMakeContextCurrent(ALCcontext *context);
void alcDestroyContext(ALCcontext *context);
