#pragma once

#include <cstdint>

namespace aurora::system_config {

enum class Language : uint8_t {
	Japanese, English, German, French, Spanish, Italian, Dutch, SimplifiedChinese, TraditionalChinese, Korean
};

enum class SoundMode : uint8_t {
	Mono, Stereo, Surround
};

struct Settings {
	Language language = Language::English;
	SoundMode soundMode = SoundMode::Stereo;
	bool widescreen = false;
	bool progressive = true;
	bool euRgb60 = false;
	bool screenSaver = true;
};

bool configure(const Settings& settings);
Settings settings();

}
