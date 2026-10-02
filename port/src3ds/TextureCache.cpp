/* TextureCache.cpp
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

#include "TextureCache.h"

#include "Gfx.h"
#include "Platform.h"

#include "../image/ImageBuffer.h"
#include "../Logger.h"

#include <3ds.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>

using namespace std;

namespace {
	enum class State : uint8_t {
		UNLOADED,
		QUEUED,
		LOADING,
		LOADED,
		FAILED,
	};

	struct Entry {
		TextureCache::StreamInfo info;
		bool streamed = false;
		bool removed = false;
		bool unloadWhenDone = false;
		State state = State::UNLOADED;
		vector<C3D_Tex> frames;
		vector<C3D_Tex> masks;
		float u = 1.f;
		float v = 0.f;
		uint32_t lastUse = 0;
		size_t bytes = 0;
	};

	// A request for the loader thread: fill these textures from the pack.
	struct Job {
		uint32_t handle = 0;
		vector<void *> destinations;
		vector<size_t> destinationSizes;
		vector<uint32_t> offsets;
		vector<uint32_t> sizes;
		bool success = false;
	};

	// Leave this much linear memory for everything else (audio, render targets...).
	constexpr size_t LINEAR_RESERVE = 6 << 20;
	// Don't evict sprites that were drawn this recently, in frames.
	constexpr uint32_t MIN_AGE = 3;
	// Limit how much loading work is in flight.
	constexpr size_t MAX_IN_FLIGHT = 6;

	vector<unique_ptr<Entry>> entries;
	deque<uint32_t> requests;
	vector<uint32_t> residentStreamed;
	size_t bytesUsed = 0;
	size_t budget = 0;
	uint32_t frameCount = 0;
	size_t inFlight = 0;

	string pack;
	Thread loader = nullptr;
	mutex jobMutex;
	condition_variable jobCondition;
	deque<unique_ptr<Job>> jobs;
	deque<unique_ptr<Job>> finished;
	atomic<bool> quitting = false;


	size_t TextureBytes(GPU_TEXCOLOR format, int width, int height)
	{
		size_t pixels = static_cast<size_t>(width) * height;
		switch(format)
		{
			case GPU_RGBA8:
				return pixels * 4;
			case GPU_RGB8:
				return pixels * 3;
			case GPU_RGBA5551:
			case GPU_RGB565:
			case GPU_RGBA4:
			case GPU_LA8:
				return pixels * 2;
			case GPU_ETC1:
			case GPU_L4:
			case GPU_A4:
				return pixels / 2;
			default:
				return pixels;
		}
	}


	Entry *Find(uint32_t handle)
	{
		if(!handle || handle > entries.size())
			return nullptr;
		Entry *entry = entries[handle - 1].get();
		return (entry && !entry->removed) ? entry : nullptr;
	}


	void FreeTextures(Entry &entry)
	{
		for(C3D_Tex &tex : entry.frames)
			Gfx::DeleteTexture(&tex);
		for(C3D_Tex &tex : entry.masks)
			Gfx::DeleteTexture(&tex);
		entry.frames.clear();
		entry.masks.clear();
		bytesUsed -= min(bytesUsed, entry.bytes);
		entry.bytes = 0;
	}


	// Evict the least recently used streamed sprites until `needed` more bytes
	// fit in the budget. Returns false if that is not possible.
	bool MakeRoom(size_t needed)
	{
		while(bytesUsed + needed > budget)
		{
			Entry *oldest = nullptr;
			size_t oldestIndex = 0;
			for(size_t i = 0; i < residentStreamed.size(); ++i)
			{
				Entry *entry = Find(residentStreamed[i]);
				if(!entry || entry->state != State::LOADED)
					continue;
				if(frameCount - entry->lastUse < MIN_AGE)
					continue;
				if(!oldest || entry->lastUse < oldest->lastUse)
				{
					oldest = entry;
					oldestIndex = i;
				}
			}
			if(!oldest)
				return false;
			FreeTextures(*oldest);
			oldest->state = State::UNLOADED;
			residentStreamed[oldestIndex] = residentStreamed.back();
			residentStreamed.pop_back();
		}
		return true;
	}


	// Allocate the textures for a streamed sprite and hand the job to the loader.
	bool StartLoad(uint32_t handle, Entry &entry)
	{
		const TextureCache::StreamInfo &info = entry.info;
		size_t count = info.frames + info.maskFrames;
		size_t frameBytes = TextureBytes(info.format, info.width, info.height);
		size_t maskBytes = TextureBytes(GPU_L8, info.width, info.height);
		size_t total = info.frames * frameBytes + info.maskFrames * maskBytes;
		if(!MakeRoom(total))
			return false;

		auto job = make_unique<Job>();
		job->handle = handle;
		entry.frames.resize(info.frames);
		entry.masks.resize(info.maskFrames);
		bool ok = true;
		for(size_t i = 0; i < count && ok; ++i)
		{
			bool isMask = i >= info.frames;
			C3D_Tex &tex = isMask ? entry.masks[i - info.frames] : entry.frames[i];
			tex = C3D_Tex{};
			ok = Gfx::CreateTexture(&tex, info.width, info.height, isMask ? GPU_L8 : info.format);
			if(ok)
			{
				job->destinations.push_back(tex.data);
				job->destinationSizes.push_back(tex.size);
			}
		}
		if(!ok)
		{
			// Out of linear memory: shrink the budget so this does not repeat.
			for(C3D_Tex &tex : entry.frames)
				Gfx::DeleteTexture(&tex);
			for(C3D_Tex &tex : entry.masks)
				Gfx::DeleteTexture(&tex);
			entry.frames.clear();
			entry.masks.clear();
			budget = max<size_t>(bytesUsed, 4 << 20);
			return false;
		}
		job->offsets = info.offsets;
		job->sizes = info.sizes;
		entry.bytes = total;
		bytesUsed += total;
		entry.state = State::LOADING;
		++inFlight;
		{
			lock_guard<mutex> lock(jobMutex);
			jobs.push_back(std::move(job));
		}
		jobCondition.notify_one();
		return true;
	}


	void LoaderThread(void *)
	{
		FILE *file = fopen(pack.c_str(), "rb");
		vector<uint8_t> buffer;
		while(true)
		{
			unique_ptr<Job> job;
			{
				unique_lock<mutex> lock(jobMutex);
				jobCondition.wait(lock, [] { return quitting || !jobs.empty(); });
				if(quitting)
					break;
				job = std::move(jobs.front());
				jobs.pop_front();
			}

			job->success = file != nullptr;
			for(size_t i = 0; job->success && i < job->destinations.size(); ++i)
			{
				buffer.resize(job->sizes[i]);
				if(fseek(file, job->offsets[i], SEEK_SET) || fread(buffer.data(), 1, buffer.size(), file) != buffer.size())
					job->success = false;
				else
					job->success = decompress(job->destinations[i], job->destinationSizes[i], nullptr,
						buffer.data(), buffer.size());
			}

			lock_guard<mutex> lock(jobMutex);
			finished.push_back(std::move(job));
		}
		if(file)
			fclose(file);
	}


	// Pick a format and a power-of-two size for an image that is not in the pack.
	bool UploadBuffer(ImageBuffer &buffer, int frame, C3D_Tex &tex, bool isMask, float &u, float &v)
	{
		// Textures are at most 1024 pixels wide or tall.
		while(buffer.Width() > 1024 || buffer.Height() > 1024)
			if(!buffer.ShrinkToHalfSize())
				return false;
		int width = 8;
		while(width < buffer.Width())
			width <<= 1;
		int height = 8;
		while(height < buffer.Height())
			height <<= 1;
		GPU_TEXCOLOR format = isMask ? GPU_L8 : (width * height > 256 * 256 ? GPU_RGBA4 : GPU_RGBA8);
		if(!Gfx::CreateTexture(&tex, width, height, format))
			return false;
		Gfx::UploadPixels(&tex, buffer.Begin(0, frame), buffer.Width(), buffer.Height(), buffer.Width());
		u = static_cast<float>(buffer.Width()) / width;
		v = 1.f - static_cast<float>(buffer.Height()) / height;
		return true;
	}
}



void TextureCache::Init(const string &packPath)
{
	pack = packPath;
	size_t available = Platform::LinearFree();
	budget = available > LINEAR_RESERVE + (8 << 20) ? available - LINEAR_RESERVE : (8 << 20);
	Logger::Log("Texture budget: " + to_string(budget >> 20) + " MB.", Logger::Level::INFO);

	quitting = false;
	s32 priority = 0x30;
	svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
	int core = Platform::IsNew3DS() ? 2 : 1;
	loader = threadCreate(LoaderThread, nullptr, 32 * 1024, priority + 1, core, false);
	if(!loader)
		loader = threadCreate(LoaderThread, nullptr, 32 * 1024, priority + 1, -2, false);
}



void TextureCache::Quit()
{
	quitting = true;
	jobCondition.notify_all();
	if(loader)
	{
		threadJoin(loader, U64_MAX);
		threadFree(loader);
		loader = nullptr;
	}
	for(auto &entry : entries)
		if(entry)
			FreeTextures(*entry);
	entries.clear();
}



uint32_t TextureCache::AddStreamed(StreamInfo &&info)
{
	auto entry = make_unique<Entry>();
	entry->streamed = true;
	entry->u = info.u;
	entry->v = info.v;
	entry->info = std::move(info);
	entries.push_back(std::move(entry));
	return entries.size();
}



uint32_t TextureCache::AddImage(const string &name, ImageBuffer &buffer, bool isMask)
{
	if(!buffer.Pixels() || !buffer.Frames())
		return 0;
	auto entry = make_unique<Entry>();
	vector<C3D_Tex> &target = isMask ? entry->masks : entry->frames;
	target.resize(buffer.Frames());
	for(int i = 0; i < buffer.Frames(); ++i)
	{
		target[i] = C3D_Tex{};
		if(!UploadBuffer(buffer, i, target[i], isMask, entry->u, entry->v))
		{
			Logger::Log("Unable to upload sprite \"" + name + "\".", Logger::Level::WARNING);
			FreeTextures(*entry);
			return 0;
		}
		entry->bytes += target[i].size;
	}
	bytesUsed += entry->bytes;
	entry->state = State::LOADED;
	entries.push_back(std::move(entry));
	return entries.size();
}



void TextureCache::AddMaskImage(uint32_t handle, ImageBuffer &buffer)
{
	Entry *entry = Find(handle);
	if(!entry || entry->streamed || !buffer.Pixels())
		return;
	entry->masks.resize(buffer.Frames());
	float u, v;
	for(int i = 0; i < buffer.Frames(); ++i)
	{
		entry->masks[i] = C3D_Tex{};
		if(UploadBuffer(buffer, i, entry->masks[i], true, u, v))
		{
			entry->bytes += entry->masks[i].size;
			bytesUsed += entry->masks[i].size;
		}
	}
}



void TextureCache::Remove(uint32_t handle)
{
	Entry *entry = Find(handle);
	if(!entry)
		return;
	// A sprite that is being loaded is freed once the loader is done with it.
	if(entry->state == State::LOADING)
	{
		entry->unloadWhenDone = true;
		entry->removed = !entry->streamed;
		return;
	}
	FreeTextures(*entry);
	entry->state = State::UNLOADED;
	// Streamed sprites can be loaded again later; other images are gone for good.
	if(!entry->streamed)
		entry->removed = true;
}



bool TextureCache::Get(uint32_t handle, const vector<C3D_Tex> *&frames, const vector<C3D_Tex> *&masks,
	float &u, float &v)
{
	Entry *entry = Find(handle);
	if(!entry)
		return false;
	entry->lastUse = frameCount;
	if(entry->state == State::UNLOADED && entry->streamed)
	{
		entry->state = State::QUEUED;
		requests.push_back(handle);
	}
	if(entry->state != State::LOADED || entry->frames.empty())
		return false;
	frames = &entry->frames;
	masks = &entry->masks;
	u = entry->u;
	v = entry->v;
	return true;
}



void TextureCache::Prefetch(uint32_t handle)
{
	Entry *entry = Find(handle);
	if(!entry)
		return;
	entry->lastUse = frameCount;
	if(entry->state == State::UNLOADED && entry->streamed)
	{
		entry->state = State::QUEUED;
		requests.push_back(handle);
	}
}



bool TextureCache::IsResident(uint32_t handle)
{
	Entry *entry = Find(handle);
	return entry && entry->state == State::LOADED;
}



void TextureCache::Update()
{
	++frameCount;

	// Finish the loads that the loader thread has completed.
	deque<unique_ptr<Job>> done;
	{
		lock_guard<mutex> lock(jobMutex);
		done.swap(finished);
	}
	for(auto &job : done)
	{
		--inFlight;
		if(job->handle > entries.size() || !entries[job->handle - 1])
			continue;
		Entry &entry = *entries[job->handle - 1];
		if(entry.unloadWhenDone)
		{
			FreeTextures(entry);
			entry.state = State::UNLOADED;
			entry.unloadWhenDone = false;
			continue;
		}
		if(job->success)
		{
			for(C3D_Tex &tex : entry.frames)
				C3D_TexFlush(&tex);
			for(C3D_Tex &tex : entry.masks)
				C3D_TexFlush(&tex);
			entry.state = State::LOADED;
			residentStreamed.push_back(job->handle);
		}
		else
		{
			FreeTextures(entry);
			entry.state = State::FAILED;
			Logger::Log("Unable to read a sprite from the texture pack.", Logger::Level::WARNING);
		}
	}

	// Start new loads, most recent requests first: they are the ones on screen.
	while(!requests.empty() && inFlight < MAX_IN_FLIGHT)
	{
		uint32_t handle = requests.back();
		requests.pop_back();
		Entry *entry = Find(handle);
		if(!entry || entry->state != State::QUEUED)
			continue;
		// Requests for sprites that are no longer drawn are dropped.
		if(frameCount - entry->lastUse > 30)
		{
			entry->state = State::UNLOADED;
			continue;
		}
		if(!StartLoad(handle, *entry))
		{
			// No room right now; try again later.
			requests.push_front(handle);
			break;
		}
	}
}



size_t TextureCache::BytesUsed()
{
	return bytesUsed;
}



size_t TextureCache::Budget()
{
	return budget;
}



int TextureCache::PendingLoads()
{
	return requests.size() + inFlight;
}
