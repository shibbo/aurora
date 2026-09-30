#include "../../dolphin/gx/gx.hpp"
#include "../../dolphin/gx/__gx.h"

extern "C" void GXSetScissorBoxOffset(s32 x, s32 y) {
	const u32 horizontal = (static_cast<u32>(x) + 342) >> 1;
	const u32 vertical = (static_cast<u32>(y) + 342) >> 1;
	GX_WRITE_RAS_REG(0x59000000 | (horizontal & 0x3ff) | ((vertical & 0x3ff) << 10));
	__gx->bpSent = 0;
}
