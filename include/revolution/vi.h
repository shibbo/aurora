#pragma once

#include <dolphin/vi.h>

#define VI_TVMODE_EURGB60_PROG ((VITVMode)VI_TVMODE(VI_EURGB60, VI_PROGRESSIVE))
#define VI_MAX_WIDTH_NTSC 720
#define VI_MAX_HEIGHT_NTSC 480
#define VI_MAX_WIDTH_PAL 720
#define VI_MAX_HEIGHT_PAL 574
#define VI_MAX_WIDTH_MPAL 720
#define VI_MAX_HEIGHT_MPAL 480
#define VI_MAX_WIDTH_EURGB60 VI_MAX_WIDTH_NTSC
#define VI_MAX_HEIGHT_EURGB60 VI_MAX_HEIGHT_NTSC

#ifdef __cplusplus
extern "C" {
#endif

u32 VIGetDimmingCount(void);
BOOL VIEnableDimming(BOOL enable);

#ifdef __cplusplus
}
#endif
