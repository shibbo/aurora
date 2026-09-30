#ifndef REVOLUTION_THP_H
#define REVOLUTION_THP_H

#include <dolphin/thp.h>
#include <dolphin/gx/GXStruct.h>

typedef struct {
	char magic[4];
	u32 version;
	u32 bufSize;
	u32 audioMaxSamples;
	f32 frameRate;
	u32 numFrames;
	u32 firstFrameSize;
	u32 movieDataSize;           
	u32 compInfoDataOffsets;
	u32 offsetDataOffsets;
	u32 movieDataOffsets;
	u32 finalFrameDataOffsets;
} THPHeader;

typedef struct {
	u32 xSize;
	u32 ySize;
	u32 videoType;
} THPVideoInfo;

typedef struct {
	u32 numComponents;
	u8 frameComp[16];
} THPFrameCompInfo;

typedef struct {
	u32 sndChannels;
	u32 sndFrequency;
	u32 sndNumSamples;
	u32 sndNumTracks;
} THPAudioInfo;

typedef struct {
	u8* ptr;
	s32 frameNumber;
	volatile BOOL isValid;
} THPReadBuffer;

typedef struct {
	u8* ytexture;
	u8* utexture;
	u8* vtexture;
	s32 frameNumber;
} THPTextureSet;

typedef struct {
	s16* buffer;
	s16* curPtr;
	u32 validSample;
} THPAudioBuffer;

#ifdef __cplusplus
extern "C" {
#endif

void THPGXSetTexObjFilter(GXTexFilter);
GXTexFilter THPGXGetTexObjFilter(void);
void THPGXRestore(void);
void THPGXYuv2RgbSetup(GXRenderModeObj*);
void THPGXYuv2RgbDraw(u8*, u8*, u8*, s16, s16, s16, s16, s16, s16);

#ifdef __cplusplus
}
#endif

#endif
