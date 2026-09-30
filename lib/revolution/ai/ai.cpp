#include <dolphin/ai.h>
#include <revolution/os.h>
#include "../os/OSNative.hpp"
#include "../../audio.hpp"
#include <SDL3/SDL.h>
#include <chrono>
#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

namespace {
using Clock = std::chrono::steady_clock;
using aurora::os::NativeLock;

constexpr u32 MaxDMALength = 0x7FFF * 32;

struct Audio {
	std::thread worker;
	bool starting = false;
	bool initialized = false;
	bool failed = false;
	bool shutdown = false;
	bool enabled = false;
	uintptr_t address = 0;
	u32 length = 0;
	u32 rate = AI_SAMPLERATE_32KHZ;
	u32 activeLength = 0;
	u32 activeRate = 32028;
	u32 generation = 0;
	Clock::time_point start;
	AIDCallback callback = nullptr;

	void stop() {
		{
			NativeLock lock;
			shutdown = true;
			aurora::os::nativeCondition().notify_all();
		}

		if (worker.joinable()) {
			worker.join();
		}

		NativeLock lock;
		initialized = false;
		starting = false;
		failed = false;
		enabled = false;
		address = 0;
		length = 0;
		callback = nullptr;
		activeLength = 0;
		shutdown = false;
	}

	~Audio() {
		stop();
	}
};

Audio& audio() {
	aurora::os::nativeMutex();
	aurora::os::nativeCondition();
	static Audio state;
	return state;
}

void shutdown() {
	audio().stop();
}

void run(Audio& state) {
	std::unique_ptr<void, decltype(&std::free)> buffer(std::malloc(MaxDMALength), std::free);
	SDL_AudioStream* stream = nullptr;
	SDL_AudioSpec format{SDL_AUDIO_S16, 2, 32028};
	if (buffer) {
		stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &format, nullptr, nullptr);
	}

	bool ready = false;
	if (stream) {
		ready = SDL_ResumeAudioStreamDevice(stream);
	}

	{
		NativeLock lock;
		state.initialized = ready;
		state.failed = !ready;
		state.starting = false;
		aurora::os::nativeCondition().notify_all();
	}

	bool prime = true;
	u32 streamGeneration = 0;
	Clock::time_point nextStart;
	while (ready) {
		u32 length;
		u32 rate;
		u32 generation;
		Clock::time_point deadline;
		{
			NativeLock lock;
			if (!state.enabled || state.generation != streamGeneration) {
				lock.lock.unlock();
				SDL_ClearAudioStream(stream);
				prime = true;
				lock.lock.lock();
			}

			aurora::os::nativeCondition().wait(lock.lock, [&] { return state.shutdown || state.enabled; });
			if (state.shutdown) {
				break;
			}

			length = state.length;
			rate = 32028;
			if (state.rate == AI_SAMPLERATE_48KHZ) {
				rate = 48043;
			}

			generation = state.generation;
			streamGeneration = generation;
			std::memcpy(buffer.get(), reinterpret_cast<const void*>(state.address), length);
			state.activeLength = length;
			state.activeRate = rate;
			const auto duration = std::chrono::nanoseconds(uint64_t(length) * 1000000000 / (rate * 4));
			const auto now = Clock::now();
			if (prime || now > nextStart + duration) {
				nextStart = now;
			}

			state.start = nextStart;
			deadline = nextStart + duration;
			nextStart = deadline;
		}

		if (format.freq != rate) {
			format.freq = static_cast<int>(rate);
			ready = SDL_SetAudioStreamFormat(stream, &format, nullptr);
		}

		if (ready) {
			if (prime) {
				const std::array<s16, 512> silence{};
				u32 remaining = length;
				while (remaining && ready) {
					const auto bytes = std::min<u32>(remaining, sizeof(silence));
					ready = SDL_PutAudioStreamData(stream, silence.data(), static_cast<int>(bytes));
					remaining -= bytes;
				}

				prime = false;
			}
		}

		if (ready) {
			ready = SDL_PutAudioStreamData(stream, buffer.get(), static_cast<int>(length));
		}

		bool interrupted;
		{
			NativeLock lock;
			if (!ready) {
				state.failed = true;
				state.enabled = false;
				state.activeLength = 0;
				break;
			}

			interrupted = aurora::os::nativeCondition().wait_until(lock.lock, deadline, [&] {
				return state.shutdown || !state.enabled || state.generation != generation;
			});
			state.activeLength = 0;
			if (!interrupted && state.callback) {
				auto callback = state.callback;
				aurora::os::sInterruptsEnabled = false;
				lock.lock.release();
				callback();
				OSEnableInterrupts();
			}
		}

		if (interrupted) {
			SDL_ClearAudioStream(stream);
			prime = true;
		}
	}

	if (stream) {
		SDL_DestroyAudioStream(stream);
	}
}
}

void AIInit(u8*) {
	auto& state = audio();
	bool start = false;
	{
		NativeLock lock;
		if (state.initialized) {
			return;
		}

		if (!state.starting && !state.failed) {
			state.starting = true;
			start = true;
		}
	}

	if (start) {
		try {
			state.worker = std::thread(run, std::ref(state));
		} catch (...) {
			NativeLock lock;
			state.failed = true;
			state.starting = false;
			aurora::os::nativeCondition().notify_all();
		}
	}

	bool failed;
	{
		NativeLock lock;
		aurora::os::nativeCondition().wait(lock.lock, [&] { return !state.starting; });
		failed = state.failed;
	}

	if (failed) {
		OSPanic(__FILE__, __LINE__, "Unable to initialize native audio output");
	}

	aurora::audio::set_shutdown_callback(shutdown);
}

BOOL AICheckInit() {
	auto& state = audio();
	NativeLock lock;
	return state.initialized;
}

AIDCallback AIRegisterDMACallback(AIDCallback callback) {
	auto& state = audio();
	NativeLock lock;
	auto previous = state.callback;
	state.callback = callback;
	return previous;
}

void AIInitDMA(uintptr_t address, u32 length) {
	if (!address || (address & 31) || !length || (length & 31) || length > MaxDMALength) {
		OSPanic(__FILE__, __LINE__, "Invalid audio DMA buffer");
	}

	auto& state = audio();
	NativeLock lock;
	state.address = address;
	state.length = length;
}

void AIStartDMA() {
	auto& state = audio();
	bool invalid;
	{
		NativeLock lock;
		invalid = !state.initialized || state.failed || !state.address || !state.length;
		if (!invalid) {
			state.enabled = true;
			aurora::os::nativeCondition().notify_all();
		}
	}

	if (invalid) {
		OSPanic(__FILE__, __LINE__, "Audio DMA is not initialized");
	}
}

void AIStopDMA() {
	auto& state = audio();
	NativeLock lock;
	state.enabled = false;
	state.activeLength = 0;
	++state.generation;
	aurora::os::nativeCondition().notify_all();
}

BOOL AIGetDMAEnableFlag() {
	auto& state = audio();
	NativeLock lock;
	return state.enabled;
}

uintptr_t AIGetDMAStartAddr() {
	auto& state = audio();
	NativeLock lock;
	return state.address;
}

u32 AIGetDMALength() {
	auto& state = audio();
	NativeLock lock;
	return state.length;
}

u32 AIGetDMABytesLeft() {
	auto& state = audio();
	NativeLock lock;
	if (!state.enabled || state.activeLength == 0) {
		return 0;
	}

	const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - state.start).count();
	const uint64_t consumed = uint64_t(elapsed) * state.activeRate * 4 / 1000000000;
	if (consumed >= state.activeLength) {
		return 0;
	}

	return (state.activeLength - static_cast<u32>(consumed)) & ~u32(31);
}

void AISetDSPSampleRate(u32 rate) {
	if (rate != AI_SAMPLERATE_32KHZ && rate != AI_SAMPLERATE_48KHZ) {
		OSPanic(__FILE__, __LINE__, "Invalid audio sample rate");
	}

	auto& state = audio();
	NativeLock lock;
	state.rate = rate;
}

u32 AIGetDSPSampleRate() {
	auto& state = audio();
	NativeLock lock;
	return state.rate;
}

void AIReset() {
	auto& state = audio();
	NativeLock lock;
	state.enabled = false;
	state.address = 0;
	state.length = 0;
	state.activeLength = 0;
	state.callback = nullptr;
	++state.generation;
	aurora::os::nativeCondition().notify_all();
}
