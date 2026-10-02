/* uuid/uuid.h
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

// Minimal libuuid-compatible API for the 3DS (implemented in ctr/Uuid.cpp).

typedef unsigned char uuid_t[16];

void uuid_copy(uuid_t dst, const uuid_t src);
int uuid_parse(const char *in, uuid_t uu);
int uuid_is_null(const uuid_t uu);
void uuid_unparse_lower(const uuid_t uu, char *out);
int uuid_compare(const uuid_t uu1, const uuid_t uu2);
void uuid_generate_random(uuid_t out);
