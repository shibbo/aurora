#include <aurora/system_config.hpp>
#include <revolution/sc.h>

#include <atomic>

namespace {

std::atomic<uint32_t> sSettings{1u | (1u << 8) | (1u << 17) | (1u << 19)};

}

namespace aurora::system_config {

bool configure(const Settings& settings) {
	const auto language = static_cast<uint32_t>(settings.language);
	const auto sound = static_cast<uint32_t>(settings.soundMode);

	if (language > 9 || sound > 2) {
		return false;
	}

	sSettings.store(language | (sound << 8) | (uint32_t(settings.widescreen) << 16) |
		(uint32_t(settings.progressive) << 17) | (uint32_t(settings.euRgb60) << 18) | (uint32_t(settings.screenSaver) << 19));
	return true;
}

Settings settings() {
	const uint32_t value = sSettings.load();
	Settings result;
	result.language = static_cast<Language>(value & 0xff);
	result.soundMode = static_cast<SoundMode>((value >> 8) & 0xff);
	result.widescreen = (value & (1u << 16)) != 0;
	result.progressive = (value & (1u << 17)) != 0;
	result.euRgb60 = (value & (1u << 18)) != 0;
	result.screenSaver = (value & (1u << 19)) != 0;
	return result;
}

}

extern "C" {

u8 SCGetLanguage() {
	return static_cast<u8>(aurora::system_config::settings().language);
}

u8 SCGetSoundMode() {
	return static_cast<u8>(aurora::system_config::settings().soundMode);
}

u8 SCGetAspectRatio() {
	return aurora::system_config::settings().widescreen;
}

u8 SCGetProgressiveMode() {
	return aurora::system_config::settings().progressive;
}

u8 SCGetEuRgb60Mode() {
	return aurora::system_config::settings().euRgb60;
}

}
