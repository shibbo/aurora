#include "color_peek.hpp"
#include "recording.hpp"
#include "../dolphin/vi/vi_internal.hpp"
#include <cstring>
#include <deque>
#include <mutex>

namespace aurora::gfx::color_peek {
namespace {
struct Readback {
	wgpu::Texture texture;
	wgpu::Buffer buffer;
	std::vector<uint8_t> pixels;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t logicalWidth = 0;
	uint32_t logicalHeight = 0;
	uint32_t stride = 0;
	uint16_t token = 0;
	void (*callback)(uint16_t) = nullptr;
	bool ready = false;
	bool active = true;
	bool bgra = false;
};
std::mutex sMutex;
std::mutex sDispatchMutex;
std::deque<std::shared_ptr<Readback>> sPending;
std::shared_ptr<Readback> sLatest;
thread_local const Readback* sCurrent = nullptr;
EncoderTaskId sTask = InvalidEncoderTask;

Readback* request(const void* payload) {
	Readback* result;
	std::memcpy(&result, payload, sizeof(result));
	return result;
}

void encode(const EncoderTaskContext& context, const wgpu::CommandEncoder& encoder,
	const void* payload, size_t, void*) {
	auto* item = request(payload);
	const wgpu::BufferDescriptor descriptor{
		.label = "GX color readback",
		.usage = wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
		.size = static_cast<uint64_t>(item->stride) * item->height,
	};
	item->buffer = context.device.CreateBuffer(&descriptor);
	const wgpu::TexelCopyTextureInfo source{.texture = item->texture};
	const wgpu::TexelCopyBufferInfo destination{
		.layout = {.bytesPerRow = item->stride, .rowsPerImage = item->height},
		.buffer = item->buffer,
	};
	const wgpu::Extent3D extent{item->width, item->height, 1};
	encoder.CopyTextureToBuffer(&source, &destination, &extent);
}

void dispatch() {
	std::lock_guard dispatchLock{sDispatchMutex};
	for (;;) {
		std::shared_ptr<Readback> item;
		{
			std::lock_guard lock{sMutex};
			if (sPending.empty() || !sPending.front()->ready) {
				return;
			}

			item = sPending.front();
			sPending.pop_front();
			if (!item->pixels.empty()) {
				sLatest = item;
			}
		}

		sCurrent = item.get();
		if (item->active && item->callback != nullptr) {
			item->callback(item->token);
		}

		sCurrent = nullptr;
	}
}

void submitted(const EncoderTaskCompletionContext&, const void* payload, size_t, void*) {
	std::shared_ptr<Readback> item;
	{
		std::lock_guard lock{sMutex};
		for (const auto& candidate : sPending) {
			if (candidate.get() == request(payload)) {
				item = candidate;
				break;
			}
		}
	}

	if (!item) {
		return;
	}

	const uint64_t size = static_cast<uint64_t>(item->stride) * item->height;
	item->buffer.MapAsync(wgpu::MapMode::Read, 0, size, wgpu::CallbackMode::AllowSpontaneous,
		[item, size](wgpu::MapAsyncStatus status, wgpu::StringView) {
			std::vector<uint8_t> pixels;
			if (status == wgpu::MapAsyncStatus::Success) {
				const auto* data = static_cast<const uint8_t*>(item->buffer.GetConstMappedRange(0, size));
				pixels.assign(data, data + size);
				item->buffer.Unmap();
			}

			{
				std::lock_guard lock{sMutex};
				item->pixels = std::move(pixels);
				item->ready = true;
			}

			dispatch();
		});
}
}

void initialize() {
	sTask = register_encoder_task_type({
		.label = "GX draw sync color readback",
		.callback = encode,
		.afterSubmit = submitted,
	});
}

void shutdown() {
	std::lock_guard dispatchLock{sDispatchMutex};
	std::lock_guard lock{sMutex};
	for (const auto& item : sPending) {
		item->active = false;
	}

	sPending.clear();
	sLatest.reset();
	unregister_encoder_task_type(sTask);
	sTask = InvalidEncoderTask;
}

bool is_idle() {
	dispatch();
	std::lock_guard dispatchLock{sDispatchMutex};
	std::lock_guard lock{sMutex};
	return sPending.empty();
}

bool queue(const wgpu::Texture& texture, uint16_t token, void (*callback)(uint16_t)) {
	if (!texture || sTask == InvalidEncoderTask) {
		return false;
	}

	auto item = std::make_shared<Readback>();
	item->texture = texture;
	item->width = texture.GetWidth();
	item->height = texture.GetHeight();
	const auto logicalSize = vi::configured_fb_size();
	item->logicalWidth = logicalSize.x;
	item->logicalHeight = logicalSize.y;
	item->stride = (item->width * 4 + 255) & ~255u;
	item->token = token;
	item->callback = callback;
	const auto format = texture.GetFormat();
	item->bgra = format == wgpu::TextureFormat::BGRA8Unorm || format == wgpu::TextureFormat::BGRA8UnormSrgb;
	if (!item->bgra && format != wgpu::TextureFormat::RGBA8Unorm && format != wgpu::TextureFormat::RGBA8UnormSrgb) {
		return false;
	}

	{
		std::lock_guard lock{sMutex};
		sPending.push_back(item);
	}

	auto* pointer = item.get();
	if (!push_encoder_task_from_fifo(sTask, &pointer, sizeof(pointer))) {
		std::lock_guard lock{sMutex};
		sPending.pop_back();
		return false;
	}

	return true;
}

bool read(uint16_t x, uint16_t y, uint8_t* rgba) {
	std::lock_guard lock{sMutex};
	const Readback* item = sCurrent;
	if (item == nullptr) {
		item = sLatest.get();
	}

	if (item == nullptr || item->pixels.empty() || x >= item->logicalWidth || y >= item->logicalHeight) {
		return false;
	}

	const uint32_t px = static_cast<uint64_t>(x) * item->width / item->logicalWidth;
	const uint32_t py = static_cast<uint64_t>(y) * item->height / item->logicalHeight;
	const auto* pixel = item->pixels.data() + static_cast<size_t>(py) * item->stride + px * 4;
	std::memcpy(rgba, pixel, 4);
	if (item->bgra) {
		std::swap(rgba[0], rgba[2]);
	}

	return true;
}
}
