#include "gx.hpp"
#include "../../gfx/depth_peek.hpp"
#include "../../gfx/color_peek.hpp"
#include <atomic>

#include <dolphin/gx/GXCpu2Efb.h>

void GXPeekZ(u16 x, u16 y, u32* z) {
	if (z != nullptr && aurora::gfx::color_peek::read_depth(x, y, *z)) {
		return;
	}

  if (z != nullptr) {
    u32 value = 0;
    if (aurora::gfx::depth_peek::read_latest(x, y, value)) {
      *z = value;
    } else {
      *z = 0;
    }
  }

	aurora::gfx::depth_peek::request_snapshot();
}

namespace {
std::atomic<GXAlphaReadMode> sAlphaRead{GX_READ_NONE};
}

void GXPokeAlphaRead(GXAlphaReadMode mode) {
	sAlphaRead.store(mode);
}

void GXPeekARGB(u16 x, u16 y, u32* color) {
	if (color == nullptr) {
		return;
	}

	u8 rgba[4]{};
	aurora::gfx::color_peek::read(x, y, rgba);
	const auto mode = sAlphaRead.load();
	if (mode == GX_READ_00) {
		rgba[3] = 0;
	} else if (mode == GX_READ_FF) {
		rgba[3] = 255;
	}

	*color = (u32(rgba[3]) << 24) | (u32(rgba[0]) << 16) | (u32(rgba[1]) << 8) | rgba[2];
}
