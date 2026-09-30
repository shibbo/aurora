#pragma once

#include <revolution/types.h>

#define SC_LANG_JAPANESE 0u
#define SC_LANG_ENGLISH 1u
#define SC_LANG_GERMAN 2u
#define SC_LANG_FRENCH 3u
#define SC_LANG_SPANISH 4u
#define SC_LANG_ITALIAN 5u
#define SC_LANG_DUTCH 6u
#define SC_LANG_SIMP_CHINESE 7u
#define SC_LANG_TRAD_CHINESE 8u
#define SC_LANG_KOREAN 9u

#define SC_SOUND_MODE_MONO 0u
#define SC_SOUND_MODE_STEREO 1u
#define SC_SOUND_MODE_SURROUND 2u
#define SC_SOUND_MODE_DEFAULT SC_SOUND_MODE_STEREO

#define SC_ASPECT_RATIO_4x3 0u
#define SC_ASPECT_RATIO_16x9 1u
#define SC_ASPECT_RATIO_DEFAULT SC_ASPECT_RATIO_4x3

#define SC_PROGRESSIVE_MODE_OFF 0u
#define SC_PROGRESSIVE_MODE_ON 1u
#define SC_PROGRESSIVE_MODE_DEFAULT SC_PROGRESSIVE_MODE_OFF

#define SC_EURGB60_MODE_OFF 0u
#define SC_EURGB60_MODE_ON 1u
#define SC_EURGB60_MODE_DEFAULT SC_EURGB60_MODE_OFF

#ifdef __cplusplus
extern "C" {
#endif

u8 SCGetLanguage(void);
u8 SCGetSoundMode(void);
u8 SCGetAspectRatio(void);
u8 SCGetProgressiveMode(void);
u8 SCGetEuRgb60Mode(void);

#ifdef __cplusplus
}
#endif
