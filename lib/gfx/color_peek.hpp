#pragma once
#include "types.hpp"

namespace aurora::gfx::color_peek {
void initialize();
void shutdown();
bool is_idle();
bool queue(const wgpu::Texture& texture, const wgpu::TextureView& depthView, uint32_t msaaSamples,
	uint16_t token, void (*callback)(uint16_t));
bool read(uint16_t x, uint16_t y, uint8_t* rgba);
bool read_depth(uint16_t x, uint16_t y, uint32_t& depth);
}
