#include "backend.hpp"
#include "../../input.hpp"

#include <SDL3/SDL_timer.h>

namespace aurora::wpad {
namespace {
Vec acceleration(u32 chan, PADSensorType sensor) {
	float data[3];

	if (PADHasSensor(chan, sensor) && PADSetSensorEnabled(chan, sensor, TRUE) &&
		PADGetSensorData(chan, sensor, data, 3)) {
		constexpr float gravity = 9.80665f;

		return {-data[0] / gravity, -data[1] / gravity, -data[2] / gravity};
	}

	return {0, -1, 0};
}
} // namespace

std::array<HostState, WPAD_MAX_CONTROLLERS> read_host() {
	std::array<PADStatus, PAD_CHANMAX> pads{};
	PADRead(pads.data());
	std::array<HostState, WPAD_MAX_CONTROLLERS> result{};

	for (u32 i = 0; i < result.size(); ++i) {
		auto& state = result[i];
		state.pad = pads[i];
		state.blocked = PADIsInputBlocked();

		if (state.pad.err != PAD_ERR_NONE || state.blocked) {
			continue;
		}

		PADSensorType coreSensor = PAD_SENSOR_ACCEL;

		if (PADHasSensor(i, PAD_SENSOR_ACCEL_RIGHT)) {
			coreSensor = PAD_SENSOR_ACCEL_RIGHT;
		}

		state.coreAcc = acceleration(i, coreSensor);
		state.nunchukAcc = acceleration(i, PAD_SENSOR_ACCEL_LEFT);
	}

	float x = 0, y = 0;
	if (!result[0].blocked && input::get_mouse_pointer(&x, &y)) {
		result[0].pointer = {x, y, TRUE};
	}

	return result;
}

void install_hooks(void (*update)(), void (*shutdown)()) {
	input::set_wpad_hooks(update, shutdown);
}

double now() {
	return static_cast<double>(SDL_GetTicksNS()) / 1e9;
}
} // namespace aurora::wpad
