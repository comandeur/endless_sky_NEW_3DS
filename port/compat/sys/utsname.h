/* sys/utsname.h
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

// newlib has no uname(); the logger only uses it to print the system name.

#include <cstring>

struct utsname {
	char sysname[32];
	char nodename[32];
	char release[32];
	char version[32];
	char machine[32];
};

inline int uname(struct utsname *name)
{
	strcpy(name->sysname, "Nintendo 3DS");
	strcpy(name->nodename, "3ds");
	strcpy(name->release, "Horizon");
	strcpy(name->version, "libctru");
	strcpy(name->machine, "armv6k");
	return 0;
}
