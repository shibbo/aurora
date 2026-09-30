#ifndef WENC_H
#define WENC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "revolution/types.h"

typedef enum {
	WENC_FLAG_USER_INFO = (1 << 0),
} WENCFlag;

typedef struct WENCInfo {
	s32 xn;
	s32 dl;
	s32 qn;
	s32 dn;
	s32 dlh;
	s32 dlq;
	u8 padding[8];
} WENCInfo;

s32 WENCGetEncodeData(WENCInfo*, u32, const s16*, s32, u8*);

#ifdef __cplusplus
}
#endif

#endif
