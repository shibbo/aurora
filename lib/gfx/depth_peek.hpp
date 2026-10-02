#pragma once

#include "types.hpp"
#include <dolphin/gx/GXAurora.h>

#include <vector>

namespace aurora::gfx::depth_peek {

void initialize();
void shutdown();

void request_snapshot() noexcept;
bool read_latest(uint16_t x, uint16_t y, uint32_t& z) noexcept;

void encode_frame_snapshot(const wgpu::CommandEncoder& cmd, const wgpu::TextureView& depthView,
                           wgpu::Extent3D sourceSize, uint32_t msaaSamples) noexcept;
bool encode_snapshot(const wgpu::CommandEncoder& cmd, const wgpu::TextureView& depthView,
	wgpu::Extent3D sourceSize, Vec2<uint32_t> logicalSize, AuroraViewportPolicy policy,
	uint32_t msaaSamples, const wgpu::Buffer& destination, uint64_t offset);
void after_submit() noexcept;

namespace testing {
void reset() noexcept;
bool snapshot_requested() noexcept;
void set_latest(uint32_t width, uint32_t height, const std::vector<uint32_t>& data);
} // namespace testing

} // namespace aurora::gfx::depth_peek
