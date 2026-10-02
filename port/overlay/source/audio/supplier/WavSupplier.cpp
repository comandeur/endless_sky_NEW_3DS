/* WavSupplier.cpp
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

#include "WavSupplier.h"

#include "../Sound.h"

#include <algorithm>
#include <cmath>

using namespace std;

namespace {
	const int16_t STEP_TABLE[89] = {
		7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80,
		88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
		598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
		3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
		12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
	const int8_t INDEX_TABLE[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

	// Number of output frames (stereo pairs) in a chunk.
	constexpr size_t CHUNK_FRAMES = 1. / 60. * AudioSupplier::SAMPLE_RATE * 5;
}



WavSupplier::WavSupplier(const Sound &sound, bool is3x, bool looping)
	: AudioSupplier(is3x, looping), sound(sound), wasStarted(false)
{
}



size_t WavSupplier::MaxChunks() const
{
	if(isLooping)
		return 2;
	if(wasStarted && !position)
		return 0;
	const Sound::Data &data = sound.GetData(is3x);
	if(data.Empty())
		return 0;
	double remaining = data.samples - (position >> 16);
	double outputFrames = remaining * SAMPLE_RATE / data.rate;
	return ceil(outputFrames / CHUNK_FRAMES);
}



size_t WavSupplier::AvailableChunks() const
{
	return MaxChunks();
}



vector<AudioSupplier::sample_t> WavSupplier::NextDataChunk()
{
	vector<sample_t> samples(OUTPUT_CHUNK);
	// If we are at the beginning of the buffer and it was already played, this is a loop.
	if(!position && wasStarted && !isLooping)
		return samples;

	size_t frame = 0;
	while(frame < CHUNK_FRAMES)
	{
		if(!position)
		{
			// If restarting the sound, check 3x status.
			is3x = nextPlaybackIs3x;
			wasStarted = true;
			decodedBlock = UINT32_MAX;
		}
		const Sound::Data &data = sound.GetData(is3x);
		if(data.Empty())
			break;
		uint64_t step = (static_cast<uint64_t>(data.rate) << 16) / SAMPLE_RATE;
		uint64_t end = static_cast<uint64_t>(data.samples) << 16;
		for( ; frame < CHUNK_FRAMES && position < end; ++frame, position += step)
		{
			// Linear interpolation between neighboring samples.
			uint32_t index = position >> 16;
			int fraction = position & 0xFFFF;
			int a = Sample(index);
			int b = (index + 1 < data.samples) ? Sample(index + 1) : a;
			sample_t value = static_cast<sample_t>(a + (((b - a) * fraction) >> 16));
			samples[2 * frame] = value;
			samples[2 * frame + 1] = value;
		}
		if(position >= end)
		{
			position = 0;
			if(!isLooping)
				break;
		}
	}
	return samples;
}



int WavSupplier::Sample(uint32_t index)
{
	const Sound::Data &data = sound.GetData(is3x);
	if(!data.adpcm)
	{
		const uint8_t *p = data.bytes.data() + 2 * index;
		return static_cast<int16_t>(p[0] | (p[1] << 8));
	}
	uint32_t block = index / data.samplesPerBlock;
	if(block != decodedBlock)
		DecodeBlock(block);
	return decoded[index - block * data.samplesPerBlock];
}



void WavSupplier::DecodeBlock(uint32_t block)
{
	const Sound::Data &data = sound.GetData(is3x);
	decoded.assign(data.samplesPerBlock, 0);
	decodedBlock = block;
	const uint8_t *in = data.bytes.data() + static_cast<size_t>(block) * data.blockAlign;
	int predictor = static_cast<int16_t>(in[0] | (in[1] << 8));
	int index = clamp<int>(in[2], 0, 88);
	decoded[0] = predictor;
	size_t out = 1;
	for(int i = 4; i < data.blockAlign && out < decoded.size(); ++i)
		for(int shift = 0; shift <= 4 && out < decoded.size(); shift += 4)
		{
			int nibble = (in[i] >> shift) & 0xF;
			int step = STEP_TABLE[index];
			int difference = step >> 3;
			if(nibble & 1)
				difference += step >> 2;
			if(nibble & 2)
				difference += step >> 1;
			if(nibble & 4)
				difference += step;
			if(nibble & 8)
				difference = -difference;
			predictor = clamp(predictor + difference, -32768, 32767);
			index = clamp(index + INDEX_TABLE[nibble], 0, 88);
			decoded[out++] = predictor;
		}
}
