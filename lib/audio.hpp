#pragma once

namespace aurora::audio {
bool initialize() noexcept;
void set_shutdown_callback(void (*callback)()) noexcept;
void shutdown() noexcept;
}
