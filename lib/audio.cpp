#include "audio.hpp"
#include <atomic>
#include <SDL3/SDL.h>

namespace aurora::audio {
namespace {
std::atomic<void (*)()> sShutdown;
bool sInitialized;
}

bool initialize() noexcept {
	if (!sInitialized) {
		sInitialized = SDL_InitSubSystem(SDL_INIT_AUDIO);
	}

	return sInitialized;
}

void set_shutdown_callback(void (*callback)()) noexcept {
	sShutdown.store(callback);
}

void shutdown() noexcept {
	auto callback = sShutdown.exchange(nullptr);
	if (callback) {
		callback();
	}

	if (sInitialized) {
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		sInitialized = false;
	}
}
}
