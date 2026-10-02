/* OpenAL.cpp
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

// A small software implementation of the parts of OpenAL that the game's audio
// code uses (see compat/AL/al.h). Sources play queues of buffers; a mixer
// thread adds them up, with each source's gain, pitch and stereo position, and
// streams the result to the DSP through a single NDSP channel.

#include <AL/al.h>
#include <AL/alc.h>

#include "Gfx.h"
#include "Platform.h"

#include <3ds.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <vector>

using namespace std;

struct ALCdevice {
	int unused = 0;
};
struct ALCcontext {
	int unused = 0;
};

namespace {
	constexpr int OUTPUT_RATE = 44100;
	constexpr int CHANNEL = 0;
	constexpr int WAVE_BUFFERS = 4;
	constexpr int FRAMES_PER_BUFFER = 1024;

	struct Buffer {
		vector<int16_t> samples;
		int channels = 1;
		int rate = OUTPUT_RATE;
		size_t Frames() const { return samples.size() / channels; }
	};

	struct Source {
		float gain = 1.f;
		float pitch = 1.f;
		float position[3] = {0.f, 0.f, 0.f};
		float referenceDistance = 1.f;
		float rolloff = 1.f;
		float maxDistance = 100.f;
		bool looping = false;
		ALint state = AL_INITIAL;
		// Whether the source stopped because it ran out of queued data. The
		// 3DS is slow enough for this to happen now and then; such a source
		// resumes as soon as more data is queued.
		bool starved = false;
		// Queued buffers, the first `processed` of which have been played.
		deque<ALuint> queue;
		size_t processed = 0;
		// Playback position in the current buffer, in 16.16 fixed point frames.
		uint64_t cursor = 0;
	};

	mutex alMutex;
	map<ALuint, Buffer> buffers;
	map<ALuint, Source> sources;
	ALuint nextBuffer = 1;
	ALuint nextSource = 1;
	float listenerGain = 1.f;

	bool initialized = false;
	ndspWaveBuf waveBuffers[WAVE_BUFFERS];
	int16_t *waveMemory = nullptr;
	Thread mixerThread = nullptr;
	LightEvent frameEvent;
	atomic<bool> quitting = false;
	ALCdevice theDevice;
	ALCcontext theContext;

	vector<int32_t> mixBuffer(FRAMES_PER_BUFFER * 2);


	// Gains (left, right) of a source, including distance attenuation.
	void SourceGains(const Source &source, const Buffer &buffer, float &left, float &right)
	{
		float gain = source.gain * listenerGain;
		left = right = gain;
		// Only mono buffers are positioned, like in OpenAL.
		if(buffer.channels != 1)
			return;
		float x = source.position[0];
		float y = source.position[1];
		float z = source.position[2];
		float distance = sqrt(x * x + y * y + z * z);
		float clamped = clamp(distance, source.referenceDistance, max(source.referenceDistance, source.maxDistance));
		float attenuation = source.referenceDistance
			/ (source.referenceDistance + source.rolloff * (clamped - source.referenceDistance));
		// Constant power panning, from the side to side position.
		float pan = distance > 0.f ? clamp(x / distance, -1.f, 1.f) : 0.f;
		float angle = (pan + 1.f) * static_cast<float>(M_PI) * .25f;
		// Keep both speakers audible: this is a handheld, not a pair of headphones.
		left = gain * attenuation * (.3f + .7f * cos(angle) * 1.41421356f) / 1.3f;
		right = gain * attenuation * (.3f + .7f * sin(angle) * 1.41421356f) / 1.3f;
	}


	// Add one source to the mix buffer. Must be called with the lock held.
	void MixSource(Source &source)
	{
		if(source.state != AL_PLAYING)
			return;
		size_t frame = 0;
		while(frame < FRAMES_PER_BUFFER)
		{
			if(source.processed >= source.queue.size())
			{
				// Out of data: the source stops, like in OpenAL.
				source.state = AL_STOPPED;
				source.starved = true;
				return;
			}
			auto it = buffers.find(source.queue[source.processed]);
			if(it == buffers.end() || it->second.samples.empty())
			{
				++source.processed;
				source.cursor = 0;
				continue;
			}
			const Buffer &buffer = it->second;
			float leftGain, rightGain;
			SourceGains(source, buffer, leftGain, rightGain);
			int32_t left = static_cast<int32_t>(leftGain * 4096.f);
			int32_t right = static_cast<int32_t>(rightGain * 4096.f);
			uint64_t step = static_cast<uint64_t>(65536.f * source.pitch * buffer.rate / OUTPUT_RATE);
			if(!step)
				step = 1;
			const size_t frames = buffer.Frames();
			const int16_t *samples = buffer.samples.data();
			int32_t *out = mixBuffer.data() + 2 * frame;
			if(buffer.channels == 1)
				for( ; frame < FRAMES_PER_BUFFER && (source.cursor >> 16) < frames; ++frame, source.cursor += step)
				{
					int32_t s = samples[source.cursor >> 16];
					*out++ += (s * left) >> 12;
					*out++ += (s * right) >> 12;
				}
			else
				for( ; frame < FRAMES_PER_BUFFER && (source.cursor >> 16) < frames; ++frame, source.cursor += step)
				{
					const int16_t *s = samples + 2 * (source.cursor >> 16);
					*out++ += (s[0] * left) >> 12;
					*out++ += (s[1] * right) >> 12;
				}

			if((source.cursor >> 16) >= frames)
			{
				source.cursor = 0;
				if(source.looping && source.queue.size() == 1)
					continue;
				++source.processed;
			}
		}
	}


	void FillWaveBuffer(ndspWaveBuf &waveBuffer)
	{
		fill(mixBuffer.begin(), mixBuffer.end(), 0);
		{
			lock_guard<mutex> lock(alMutex);
			for(auto &it : sources)
				MixSource(it.second);
		}
		int16_t *out = static_cast<int16_t *>(waveBuffer.data_pcm16);
		for(size_t i = 0; i < mixBuffer.size(); ++i)
			out[i] = static_cast<int16_t>(clamp<int32_t>(mixBuffer[i], -32768, 32767));
		DSP_FlushDataCache(out, FRAMES_PER_BUFFER * 2 * sizeof(int16_t));
		ndspChnWaveBufAdd(CHANNEL, &waveBuffer);
	}


	void FrameCallback(void *)
	{
		LightEvent_Signal(&frameEvent);
	}


	void MixerThread(void *)
	{
		while(!quitting)
		{
			for(ndspWaveBuf &waveBuffer : waveBuffers)
				if(waveBuffer.status == NDSP_WBUF_DONE || waveBuffer.status == NDSP_WBUF_FREE)
					FillWaveBuffer(waveBuffer);
			LightEvent_Wait(&frameEvent);
		}
	}


	Source *FindSource(ALuint id)
	{
		auto it = sources.find(id);
		return it == sources.end() ? nullptr : &it->second;
	}
}



ALCdevice *alcOpenDevice(const ALCchar *)
{
	if(initialized)
		return &theDevice;
	// This needs the DSP firmware (sdmc:/3ds/dspfirm.cdc), which homebrew
	// users dump once with the "DSP1" tool.
	if(R_FAILED(ndspInit()))
		return nullptr;

	ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(CHANNEL);
	ndspChnSetInterp(CHANNEL, NDSP_INTERP_LINEAR);
	ndspChnSetRate(CHANNEL, OUTPUT_RATE);
	ndspChnSetFormat(CHANNEL, NDSP_FORMAT_STEREO_PCM16);
	float mix[12] = {1.f, 1.f};
	ndspChnSetMix(CHANNEL, mix);

	waveMemory = static_cast<int16_t *>(Gfx::LinearAlloc(WAVE_BUFFERS * FRAMES_PER_BUFFER * 2 * sizeof(int16_t)));
	if(!waveMemory)
	{
		ndspExit();
		return nullptr;
	}
	memset(waveMemory, 0, WAVE_BUFFERS * FRAMES_PER_BUFFER * 2 * sizeof(int16_t));
	memset(waveBuffers, 0, sizeof(waveBuffers));
	for(int i = 0; i < WAVE_BUFFERS; ++i)
	{
		waveBuffers[i].data_vaddr = waveMemory + i * FRAMES_PER_BUFFER * 2;
		waveBuffers[i].nsamples = FRAMES_PER_BUFFER;
		waveBuffers[i].status = NDSP_WBUF_FREE;
	}

	LightEvent_Init(&frameEvent, RESET_ONESHOT);
	ndspSetCallback(FrameCallback, nullptr);
	quitting = false;
	// Mix with a higher priority than the game, on the same core.
	s32 priority = 0x30;
	svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
	mixerThread = threadCreate(MixerThread, nullptr, 32 * 1024, max(0x18, static_cast<int>(priority) - 1), -2, false);
	initialized = true;
	return &theDevice;
}



ALCboolean alcCloseDevice(ALCdevice *)
{
	if(!initialized)
		return AL_FALSE;
	quitting = true;
	LightEvent_Signal(&frameEvent);
	if(mixerThread)
	{
		threadJoin(mixerThread, U64_MAX);
		threadFree(mixerThread);
		mixerThread = nullptr;
	}
	ndspSetCallback(nullptr, nullptr);
	ndspChnWaveBufClear(CHANNEL);
	ndspExit();
	Gfx::LinearFree(waveMemory);
	waveMemory = nullptr;
	initialized = false;
	return AL_TRUE;
}



ALCcontext *alcCreateContext(ALCdevice *device, const ALCint *)
{
	return device ? &theContext : nullptr;
}



ALCboolean alcMakeContextCurrent(ALCcontext *context)
{
	return AL_TRUE;
}



void alcDestroyContext(ALCcontext *)
{
	lock_guard<mutex> lock(alMutex);
	sources.clear();
	buffers.clear();
}



void alGenBuffers(ALsizei n, ALuint *ids)
{
	lock_guard<mutex> lock(alMutex);
	for(ALsizei i = 0; i < n; ++i)
	{
		ids[i] = nextBuffer++;
		buffers[ids[i]];
	}
}



void alDeleteBuffers(ALsizei n, const ALuint *ids)
{
	lock_guard<mutex> lock(alMutex);
	for(ALsizei i = 0; i < n; ++i)
		buffers.erase(ids[i]);
}



void alBufferData(ALuint id, ALenum format, const ALvoid *data, ALsizei size, ALsizei frequency)
{
	// Copy the data before taking the lock, so the mixer is not held up.
	Buffer buffer;
	buffer.channels = (format == AL_FORMAT_STEREO16 || format == AL_FORMAT_STEREO8) ? 2 : 1;
	buffer.rate = frequency;
	if(format == AL_FORMAT_MONO16 || format == AL_FORMAT_STEREO16)
	{
		const int16_t *samples = static_cast<const int16_t *>(data);
		buffer.samples.assign(samples, samples + size / sizeof(int16_t));
	}
	else
	{
		const uint8_t *samples = static_cast<const uint8_t *>(data);
		buffer.samples.resize(size);
		for(ALsizei i = 0; i < size; ++i)
			buffer.samples[i] = (static_cast<int>(samples[i]) - 128) << 8;
	}
	lock_guard<mutex> lock(alMutex);
	buffers[id] = std::move(buffer);
}



void alGenSources(ALsizei n, ALuint *ids)
{
	lock_guard<mutex> lock(alMutex);
	for(ALsizei i = 0; i < n; ++i)
	{
		ids[i] = nextSource++;
		sources[ids[i]];
	}
}



void alDeleteSources(ALsizei n, const ALuint *ids)
{
	lock_guard<mutex> lock(alMutex);
	for(ALsizei i = 0; i < n; ++i)
		sources.erase(ids[i]);
}



void alSourcef(ALuint id, ALenum param, ALfloat value)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(!source)
		return;
	if(param == AL_GAIN)
		source->gain = max(0.f, value);
	else if(param == AL_PITCH)
		source->pitch = clamp(value, .1f, 4.f);
	else if(param == AL_REFERENCE_DISTANCE)
		source->referenceDistance = max(.001f, value);
	else if(param == AL_ROLLOFF_FACTOR)
		source->rolloff = max(0.f, value);
	else if(param == AL_MAX_DISTANCE)
		source->maxDistance = value;
}



void alSource3f(ALuint id, ALenum param, ALfloat x, ALfloat y, ALfloat z)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(source && param == AL_POSITION)
	{
		source->position[0] = x;
		source->position[1] = y;
		source->position[2] = z;
	}
}



void alSourcei(ALuint id, ALenum param, ALint value)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(source && param == AL_LOOPING)
		source->looping = value;
}



void alGetSourcef(ALuint id, ALenum param, ALfloat *value)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(!source || !value)
		return;
	if(param == AL_GAIN)
		*value = source->gain;
	else if(param == AL_PITCH)
		*value = source->pitch;
}



void alGetSourcei(ALuint id, ALenum param, ALint *value)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(!source || !value)
		return;
	if(param == AL_SOURCE_STATE)
		*value = source->state;
	else if(param == AL_BUFFERS_QUEUED)
		*value = source->queue.size();
	else if(param == AL_BUFFERS_PROCESSED)
		*value = source->processed;
}



void alSourcePlay(ALuint id)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(!source)
		return;
	if(source->state == AL_STOPPED && !source->starved)
	{
		// Playing a stopped source starts it over with all its buffers.
		source->processed = 0;
		source->cursor = 0;
	}
	source->starved = false;
	source->state = AL_PLAYING;
}



void alSourcePause(ALuint id)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(source && source->state == AL_PLAYING)
		source->state = AL_PAUSED;
}



void alSourceStop(ALuint id)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(source)
	{
		source->state = AL_STOPPED;
		source->starved = false;
		source->processed = source->queue.size();
	}
}



void alSourceQueueBuffers(ALuint id, ALsizei n, const ALuint *ids)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(!source)
		return;
	for(ALsizei i = 0; i < n; ++i)
		source->queue.push_back(ids[i]);
	if(source->starved && n > 0)
	{
		source->starved = false;
		source->state = AL_PLAYING;
	}
}



void alSourceUnqueueBuffers(ALuint id, ALsizei n, ALuint *ids)
{
	lock_guard<mutex> lock(alMutex);
	Source *source = FindSource(id);
	if(!source)
		return;
	size_t count = min<size_t>(n, source->processed);
	for(size_t i = 0; i < count; ++i)
	{
		ids[i] = source->queue.front();
		source->queue.pop_front();
	}
	source->processed -= count;
	// Unqueued slots that could not be filled are reported as "no buffer".
	for(ALsizei i = count; i < n; ++i)
		ids[i] = 0;
}



void alListenerf(ALenum param, ALfloat value)
{
	lock_guard<mutex> lock(alMutex);
	if(param == AL_GAIN)
		listenerGain = max(0.f, value);
}



void alListenerfv(ALenum, const ALfloat *)
{
	// The listener is always at the origin, looking into the screen.
}



void alDistanceModel(ALenum)
{
}



void alDopplerFactor(ALfloat)
{
}
