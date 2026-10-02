/* Sound.cpp
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

#include "Sound.h"

#include "../Files.h"
#include "../Logger.h"
#include "supplier/WavSupplier.h"

#include <cstdint>

using namespace std;

namespace {
	uint32_t Read4(const shared_ptr<iostream> &in)
	{
		unsigned char data[4] = {};
		in->read(reinterpret_cast<char *>(data), 4);
		return data[0] | (data[1] << 8) | (data[2] << 16) | (static_cast<uint32_t>(data[3]) << 24);
	}


	uint16_t Read2(const shared_ptr<iostream> &in)
	{
		unsigned char data[2] = {};
		in->read(reinterpret_cast<char *>(data), 2);
		return data[0] | (data[1] << 8);
	}


	// Read a mono WAV file, either 16-bit PCM or IMA ADPCM, at any sample rate.
	bool ReadWav(const shared_ptr<iostream> &in, Sound::Data &out)
	{
		if(Read4(in) != 0x46464952) // "RIFF"
			return false;
		Read4(in);
		if(Read4(in) != 0x45564157) // "WAVE"
			return false;

		bool foundHeader = false;
		uint16_t format = 0;
		while(in->good())
		{
			uint32_t chunkID = Read4(in);
			uint32_t chunkSize = Read4(in);
			if(!in->good())
				return false;
			if(chunkID == 0x20746d66) // "fmt "
			{
				format = Read2(in);
				uint16_t channels = Read2(in);
				out.rate = Read4(in);
				Read4(in);
				out.blockAlign = Read2(in);
				uint16_t bits = Read2(in);
				uint32_t used = 16;
				if(format == 0x11 && chunkSize >= 20)
				{
					Read2(in);
					out.samplesPerBlock = Read2(in);
					used = 20;
				}
				if(chunkSize > used)
					in->seekg(chunkSize - used, ios::cur);
				if(channels != 1 || !out.rate)
					return false;
				if(format == 1 && bits != 16)
					return false;
				if(format == 0x11 && (bits != 4 || out.blockAlign <= 4))
					return false;
				if(format != 1 && format != 0x11)
					return false;
				if(format == 0x11 && !out.samplesPerBlock)
					out.samplesPerBlock = (out.blockAlign - 4) * 2 + 1;
				foundHeader = true;
			}
			else if(chunkID == 0x61746164) // "data"
			{
				if(!foundHeader)
					return false;
				out.bytes.resize(chunkSize);
				in->read(reinterpret_cast<char *>(out.bytes.data()), chunkSize);
				out.bytes.resize(in->gcount());
				out.adpcm = (format == 0x11);
				if(out.adpcm)
				{
					size_t blocks = out.bytes.size() / out.blockAlign;
					out.samples = blocks * out.samplesPerBlock;
				}
				else
					out.samples = out.bytes.size() / 2;
				return out.samples > 0;
			}
			else
				in->seekg(chunkSize + (chunkSize & 1), ios::cur);
		}
		return false;
	}
}



bool Sound::Load(const filesystem::path &path, const string &name)
{
	if(path.extension() != ".wav")
		return false;
	this->name = name;

	isLooped = path.stem().string().ends_with('~');
	bool isFast = isLooped ? path.stem().string().ends_with("@3x~") : path.stem().string().ends_with("@3x");
	Data &target = isFast ? data3x : data;

	shared_ptr<iostream> in = Files::Open(path);
	if(!in)
		return false;
	Data loaded;
	if(!ReadWav(in, loaded))
	{
		Logger::Log("WAV file \"" + path.string() + "\" uses an unsupported format. Only mono 16-bit PCM"
			" and IMA ADPCM are supported.", Logger::Level::WARNING);
		return false;
	}
	target = std::move(loaded);
	marker.assign(1, 0);
	return true;
}



const string &Sound::Name() const
{
	return name;
}



const vector<AudioSupplier::sample_t> &Sound::Buffer() const
{
	return marker;
}



const vector<AudioSupplier::sample_t> &Sound::Buffer3x() const
{
	return marker;
}



const Sound::Data &Sound::GetData(bool is3x) const
{
	if(is3x)
		return data3x.Empty() ? data : data3x;
	return data.Empty() ? data3x : data;
}



bool Sound::IsLooping() const
{
	return isLooped;
}



unique_ptr<AudioSupplier> Sound::CreateSupplier() const
{
	return unique_ptr<AudioSupplier>{new WavSupplier{*this, false, IsLooping()}};
}
