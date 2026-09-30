#include <revolution/vi.h>
#include <revolution/os.h>
#include "../../video.hpp"

#include "../../window.hpp"
#include "aurora/math.hpp"
#include "../../dolphin/vi/vi_internal.hpp"

#include <optional>
#include <aurora/system_config.hpp>

namespace {
struct InterruptLock {
	BOOL enabled = OSDisableInterrupts();
	~InterruptLock() {
		OSRestoreInterrupts(enabled);
	}
};

OSAlarm sRetraceAlarm;
OSThreadQueue sRetraceQueue;
bool sInitialized;
u32 sRetraceCount;
u32 sTvMode = VI_TVMODE_NTSC_PROG;
VIRetraceCallback sPreRetrace;
VIRetraceCallback sPostRetrace;
void* sNextFrameBuffer;
void* sFlushedFrameBuffer;
void* sCurrentFrameBuffer;
bool sNextBlack;
bool sFlushedBlack;
bool sFlushPending;
bool sDimmingEnabled;
u32 sDimmingIdle;
uint64_t sInputGeneration;

u32 dimmingLimit() {
	if ((sTvMode >> 2) == VI_PAL || (sTvMode >> 2) == VI_DEBUG_PAL) {
		return 15000;
	}

	return 18000;
}


void retrace(OSAlarm*, OSContext*) {
	++sRetraceCount;
	const uint64_t inputGeneration = aurora::video::input_generation();
	if (!sDimmingEnabled || inputGeneration != sInputGeneration) {
		sDimmingIdle = 0;
		sInputGeneration = inputGeneration;
	} else if (sDimmingIdle < dimmingLimit()) {
		++sDimmingIdle;
	}

	float brightness = 1.0f;
	if (sDimmingEnabled && sDimmingIdle >= dimmingLimit()) {
		brightness = 0.5f;
	}

	aurora::video::set_brightness(brightness);

	if (sPreRetrace) {
		sPreRetrace(sRetraceCount);
	}

	if (sFlushPending) {
		sCurrentFrameBuffer = sFlushedFrameBuffer;
		aurora::video::set_black(sFlushedBlack);
		sFlushPending = false;
	}

	if (sPostRetrace) {
		sPostRetrace(sRetraceCount);
	}

	OSWakeupThread(&sRetraceQueue);
}

void scheduleRetrace() {
	OSTime period = static_cast<OSTime>(OSSecondsToTicks(1)) * 1001 / 60000;
	if ((sTvMode >> 2) == VI_PAL || (sTvMode >> 2) == VI_DEBUG_PAL) {
		period = OSSecondsToTicks(1) / 50;
	}

	OSSetPeriodicAlarm(&sRetraceAlarm, OSGetTime() + period, period, retrace);
}

void shutdown() {
	InterruptLock lock;
	if (sInitialized) {
		OSCancelAlarm(&sRetraceAlarm);
		sInitialized = false;
		sPreRetrace = nullptr;
		sPostRetrace = nullptr;
		OSWakeupThread(&sRetraceQueue);
	}
}
}

namespace aurora::vi {
std::optional<GXRenderModeObj> g_renderMode;

Vec2<uint32_t> render_mode_size() noexcept {
	if (!g_renderMode) {
		return {640, 480};
	}
	return {g_renderMode->fbWidth, g_renderMode->efbHeight};
}

void configure(const GXRenderModeObj* rm) noexcept {
	const auto oldSize = render_mode_size();
	if (rm == nullptr) {
		g_renderMode.reset();
	} else {
		g_renderMode = *rm;
	}
	if (render_mode_size() != oldSize) {
		window::request_frame_buffer_resize();
	}
}

Vec2<uint32_t> configured_fb_size() noexcept {
	return render_mode_size();
}
} // namespace aurora::vi

extern "C" {
void VIInit() {
	InterruptLock lock;
	if (sInitialized) {
		return;
	}

	OSCreateAlarm(&sRetraceAlarm);
	OSInitThreadQueue(&sRetraceQueue);
	sRetraceCount = 0;
	sNextFrameBuffer = nullptr;
	sFlushedFrameBuffer = nullptr;
	sCurrentFrameBuffer = nullptr;
	sNextBlack = false;
	sFlushedBlack = false;
	sFlushPending = false;
	sDimmingEnabled = aurora::system_config::settings().screenSaver;
	sDimmingIdle = 0;
	sInputGeneration = aurora::video::input_generation();
	sInitialized = true;
	scheduleRetrace();
	aurora::video::set_shutdown_callback(shutdown);
}

void VIConfigure(const GXRenderModeObj* mode) {
	aurora::vi::configure(mode);
	InterruptLock lock;
	u32 tvMode = VI_TVMODE_NTSC_PROG;
	if (mode) {
		tvMode = mode->viTVmode;
	}

	if (sTvMode != tvMode) {
		sTvMode = tvMode;
		if (sInitialized) {
			scheduleRetrace();
		}
	}
}

void VIWaitForRetrace() noexcept(false) {
	InterruptLock lock;
	const u32 count = sRetraceCount;
	while (sInitialized && count == sRetraceCount) {
		OSSleepThread(&sRetraceQueue);
	}
}

u32 VIGetRetraceCount() {
	InterruptLock lock;
	return sRetraceCount;
}

u32 VIGetDimmingCount() {
	InterruptLock lock;
	const u32 limit = dimmingLimit();
	if (sDimmingIdle >= limit) {
		return 0;
	}

	return limit - sDimmingIdle;
}

BOOL VIEnableDimming(BOOL enable) {
	InterruptLock lock;
	const BOOL previous = sDimmingEnabled;
	sDimmingEnabled = enable && aurora::system_config::settings().screenSaver;
	return previous;
}

u32 VIGetTvFormat() {
	InterruptLock lock;
	return sTvMode >> 2;
}

u32 VIGetNextField() {
	InterruptLock lock;
	if ((sTvMode & 3) == VI_INTERLACE) {
		return sRetraceCount & 1;
	}

	return 0;
}

u32 VIGetDTVStatus() {
	return 1;
}

void VISetNextFrameBuffer(void* frameBuffer) {
	InterruptLock lock;
	sNextFrameBuffer = frameBuffer;
}

void* VIGetNextFrameBuffer() {
	InterruptLock lock;
	return sNextFrameBuffer;
}

void* VIGetCurrentFrameBuffer() {
	InterruptLock lock;
	return sCurrentFrameBuffer;
}

void VISetBlack(BOOL black) {
	InterruptLock lock;
	sNextBlack = black != FALSE;
}

void VIFlush() {
	InterruptLock lock;
	sFlushedFrameBuffer = sNextFrameBuffer;
	sFlushedBlack = sNextBlack;
	sFlushPending = true;
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback callback) {
	InterruptLock lock;
	auto previous = sPreRetrace;
	sPreRetrace = callback;
	return previous;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback callback) {
	InterruptLock lock;
	auto previous = sPostRetrace;
	sPostRetrace = callback;
	return previous;
}


void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
	(void)xOrg;
	(void)yOrg;
	(void)width;
	(void)height;
}



void VISetWindowTitle(const char* title) { aurora::window::set_title(title); }
void VISetWindowFullscreen(bool fullscreen) { aurora::window::set_fullscreen(fullscreen); }
bool VIGetWindowFullscreen() { return aurora::window::get_fullscreen(); }
void VISetWindowSize(uint32_t width, uint32_t height) { aurora::window::set_window_size(width, height); }
void VISetWindowPosition(uint32_t x, uint32_t y) { aurora::window::set_window_position(x, y); }
void VICenterWindow() { aurora::window::center_window(); }
void VISetFrameBufferScale(float scale) { aurora::window::set_frame_buffer_scale(scale); }
}
