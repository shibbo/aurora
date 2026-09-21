#ifndef AURORA_THP_H
#define AURORA_THP_H

#include <stddef.h>
#include <dolphin/types.h>

#ifdef __cplusplus
extern "C" {
#endif

s32 aurora_thp_decode_video(const void* data, size_t size, void* y, void* u, void* v, u32 width, u32 height);

#ifdef __cplusplus
}
#endif

#endif
