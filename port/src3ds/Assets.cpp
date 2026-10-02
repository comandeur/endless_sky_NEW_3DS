/* Assets.cpp
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

#include "Assets.h"

#include "../image/ImageSet.h"
#include "../image/Mask.h"
#include "../Logger.h"

#include <cstdio>
#include <cstring>

using namespace std;

namespace {
	// Reads little endian values from a buffer, remembering any overrun.
	class Reader {
	public:
		explicit Reader(const vector<uint8_t> &data) : data(data) {}

		bool Ok() const { return ok; }

		template <class T>
		T Get()
		{
			T value{};
			if(position + sizeof(T) > data.size())
			{
				ok = false;
				return value;
			}
			memcpy(&value, data.data() + position, sizeof(T));
			position += sizeof(T);
			return value;
		}

		string String(size_t length)
		{
			if(position + length > data.size())
			{
				ok = false;
				return {};
			}
			string result(reinterpret_cast<const char *>(data.data() + position), length);
			position += length;
			return result;
		}


	private:
		const vector<uint8_t> &data;
		size_t position = 0;
		bool ok = true;
	};
}



bool Assets::LoadImageIndex(const filesystem::path &folder, map<string, shared_ptr<ImageSet>> &images)
{
	filesystem::path indexPath = folder / IMAGE_INDEX;
	FILE *file = fopen(indexPath.string().c_str(), "rb");
	if(!file)
		return false;
	vector<uint8_t> data;
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);
	if(size > 0)
	{
		data.resize(size);
		if(fread(data.data(), 1, size, file) != static_cast<size_t>(size))
			data.clear();
	}
	fclose(file);

	Reader in(data);
	if(in.String(4) != "ESX1")
	{
		Logger::Log("The image index \"" + indexPath.string() + "\" is invalid.", Logger::Level::ERROR);
		return false;
	}
	uint32_t count = in.Get<uint32_t>();
	for(uint32_t i = 0; i < count && in.Ok(); ++i)
	{
		auto sprite = make_shared<PackedSprite>();
		string name = in.String(in.Get<uint16_t>());
		TextureCache::StreamInfo &stream = sprite->stream;
		stream.format = static_cast<GPU_TEXCOLOR>(in.Get<uint8_t>());
		in.Get<uint8_t>();
		stream.frames = in.Get<uint16_t>();
		stream.maskFrames = in.Get<uint16_t>();
		sprite->width = in.Get<float>();
		sprite->height = in.Get<float>();
		stream.width = in.Get<uint16_t>();
		stream.height = in.Get<uint16_t>();
		stream.u = in.Get<float>();
		stream.v = in.Get<float>();
		sprite->area = in.Get<float>();
		size_t total = stream.frames + stream.maskFrames;
		stream.offsets.resize(total);
		stream.sizes.resize(total);
		for(uint32_t &offset : stream.offsets)
			offset = in.Get<uint32_t>();
		for(uint32_t &value : stream.sizes)
			value = in.Get<uint32_t>();

		sprite->masks.resize(in.Get<uint16_t>());
		for(auto &mask : sprite->masks)
		{
			mask.resize(in.Get<uint16_t>());
			for(auto &outline : mask)
			{
				outline.resize(in.Get<uint16_t>());
				for(Point &point : outline)
				{
					float x = in.Get<float>();
					float y = in.Get<float>();
					point = Point(x, y);
				}
			}
		}

		if(!in.Ok())
			break;
		shared_ptr<ImageSet> &imageSet = images[name];
		if(!imageSet)
			imageSet = make_shared<ImageSet>(name);
		imageSet->SetPacked(std::move(sprite));
	}
	if(!in.Ok())
	{
		Logger::Log("The image index \"" + indexPath.string() + "\" is truncated.", Logger::Level::ERROR);
		return false;
	}
	TextureCache::Init((folder / IMAGE_PACK).string());
	return true;
}
