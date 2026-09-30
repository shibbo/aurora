#pragma once

#include <cstdint>

namespace aurora::video {
void set_shutdown_callback(void (*callback)()) noexcept;
void shutdown() noexcept;
void set_black(bool black) noexcept;
bool is_black() noexcept;
void notify_input() noexcept;
uint64_t input_generation() noexcept;
void set_brightness(float value) noexcept;
float brightness() noexcept;
}
