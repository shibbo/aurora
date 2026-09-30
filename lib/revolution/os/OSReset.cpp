#include <aurora/system.hpp>
#include <revolution/os.h>
#include <atomic>

namespace {
std::atomic<OSResetCallback> sResetCallback;
std::atomic<OSPowerCallback> sPowerCallback;
std::atomic<bool> sResetPressed;
}

namespace aurora::system {
void set_reset_button(bool pressed) {
	const bool previous = sResetPressed.exchange(pressed);
	if (pressed && !previous) {
		if (auto callback = sResetCallback.load()) {
			callback();
		}
	}
}

void press_power_button() {
	if (auto callback = sPowerCallback.load()) {
		callback();
	}
}
}

extern "C" {
OSResetCallback OSSetResetCallback(OSResetCallback callback) {
	return sResetCallback.exchange(callback);
}

OSPowerCallback OSSetPowerCallback(OSPowerCallback callback) {
	return sPowerCallback.exchange(callback);
}

BOOL OSGetResetSwitchState() {
	return sResetPressed.load();
}

BOOL OSGetResetButtonState() {
	return sResetPressed.load();
}

void OSRestart(u32 code) noexcept(false) {
	throw aurora::system::Request(aurora::system::Operation::Restart, code);
}

void OSReturnToMenu() noexcept(false) {
	throw aurora::system::Request(aurora::system::Operation::ReturnToMenu, 0);
}

void OSRebootSystem() noexcept(false) {
	throw aurora::system::Request(aurora::system::Operation::Reboot, 0);
}

void OSShutdownSystem() noexcept(false) {
	throw aurora::system::Request(aurora::system::Operation::Shutdown, 0);
}
}
