/* Sound.h
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

#pragma once

#include "supplier/AudioSupplier.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>



// This is a sound that can be played. The sound's file name will determine
// whether it is looping (ends in '~') or not.
//
// On the 3DS, sounds are kept in memory in the format of the file (mono, 16-bit
// PCM or IMA ADPCM, at any sample rate) and are converted to 44.1 kHz stereo
// as they are played. The desktop game keeps 44.1 kHz stereo copies of all of
// them, which would need about 80 MB.
class Sound {
public:
	struct Data {
		// Mono 16-bit samples, or IMA ADPCM blocks.
		std::vector<uint8_t> bytes;
		bool adpcm = false;
		uint32_t rate = AudioSupplier::SAMPLE_RATE;
		uint32_t samples = 0;
		uint16_t blockAlign = 0;
		uint16_t samplesPerBlock = 0;

		bool Empty() const { return !samples; }
	};


public:
	bool Load(const std::filesystem::path &path, const std::string &name);

	const std::string &Name() const;

	// For compatibility with the desktop code: non-empty if the sound is loaded.
	const std::vector<AudioSupplier::sample_t> &Buffer() const;
	const std::vector<AudioSupplier::sample_t> &Buffer3x() const;
	// The sound data (the "3x" version is the one used in fast-forward).
	const Data &GetData(bool is3x) const;

	bool IsLooping() const;

	std::unique_ptr<AudioSupplier> CreateSupplier() const;


private:
	std::string name;
	Data data;
	Data data3x;
	std::vector<AudioSupplier::sample_t> marker;
	bool isLooped = false;
};
