#include "video.hpp"
#include <atomic>

namespace aurora::video {
namespace {
std::atomic<void (*)()> sShutdown;
std::atomic<bool> sBlack;
std::atomic<uint64_t> sInputGeneration;
std::atomic<float> sBrightness{1.0f};
}

void set_shutdown_callback(void (*callback)()) noexcept {
	sShutdown.store(callback);
}

void shutdown() noexcept {
	auto callback = sShutdown.exchange(nullptr);
	if (callback) {
		callback();
	}

	sBlack.store(false);
	sBrightness.store(1.0f);
}

void set_black(bool black) noexcept {
	sBlack.store(black);
}

bool is_black() noexcept {
	return sBlack.load();
}

void notify_input() noexcept {
	sInputGeneration.fetch_add(1, std::memory_order_relaxed);
}

uint64_t input_generation() noexcept {
	return sInputGeneration.load(std::memory_order_relaxed);
}

void set_brightness(float value) noexcept {
	sBrightness.store(value);
}

float brightness() noexcept {
	return sBrightness.load();
}

}
