/* Uuid.cpp
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

#include <uuid/uuid.h>

#include <cstdio>
#include <cstring>
#include <random>

namespace {
	int HexValue(char c)
	{
		if(c >= '0' && c <= '9')
			return c - '0';
		if(c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if(c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return -1;
	}
}



void uuid_copy(uuid_t dst, const uuid_t src)
{
	memcpy(dst, src, sizeof(uuid_t));
}



// Parse the canonical 8-4-4-4-12 representation. Returns 0 on success.
int uuid_parse(const char *in, uuid_t uu)
{
	if(!in || strlen(in) != 36)
		return -1;
	uuid_t result;
	int byte = 0;
	for(int i = 0; i < 36; )
	{
		if(i == 8 || i == 13 || i == 18 || i == 23)
		{
			if(in[i] != '-')
				return -1;
			++i;
			continue;
		}
		int high = HexValue(in[i]);
		int low = HexValue(in[i + 1]);
		if(high < 0 || low < 0)
			return -1;
		result[byte++] = static_cast<unsigned char>(high * 16 + low);
		i += 2;
	}
	uuid_copy(uu, result);
	return 0;
}



int uuid_is_null(const uuid_t uu)
{
	for(int i = 0; i < 16; ++i)
		if(uu[i])
			return 0;
	return 1;
}



void uuid_unparse_lower(const uuid_t uu, char *out)
{
	snprintf(out, 37, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		uu[0], uu[1], uu[2], uu[3], uu[4], uu[5], uu[6], uu[7],
		uu[8], uu[9], uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}



int uuid_compare(const uuid_t uu1, const uuid_t uu2)
{
	return memcmp(uu1, uu2, sizeof(uuid_t));
}



// Generate a version 4 (random) UUID.
void uuid_generate_random(uuid_t out)
{
	static thread_local std::mt19937_64 engine(std::random_device{}());
	for(int i = 0; i < 16; i += 8)
	{
		uint64_t value = engine();
		memcpy(out + i, &value, 8);
	}
	out[6] = (out[6] & 0x0F) | 0x40;
	out[8] = (out[8] & 0x3F) | 0x80;
}
