/* WavSupplier.h
Copyright (c) 2025 by tibetiroka
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

#include "AudioSupplier.h"

#include <cstdint>
#include <vector>

class Sound;



/// A supplier for sounds that are held in memory. On the 3DS the sound data is
/// mono, possibly IMA ADPCM compressed, at its own sample rate; it is decoded
/// and converted to 44.1 kHz stereo one chunk at a time.
class WavSupplier : public AudioSupplier {
public:
	WavSupplier(const Sound &sound, bool is3x, bool looping = false);

	// Inherited pure virtual methods
	size_t MaxChunks() const override;
	size_t AvailableChunks() const override;
	std::vector<sample_t> NextDataChunk() override;


private:
	// Get the sample at the given index of the current data.
	int Sample(uint32_t index);
	void DecodeBlock(uint32_t block);


private:
	const Sound &sound;
	bool wasStarted;
	// Playback position in the source data, in 16.16 fixed point samples.
	uint64_t position = 0;
	// The decoded ADPCM block.
	std::vector<int16_t> decoded;
	uint32_t decodedBlock = UINT32_MAX;
};
