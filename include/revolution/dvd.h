#pragma once

#include <dolphin/dvd.h>

#ifdef __cplusplus
extern "C" {
#endif
BOOL DVDCheckDiskAsync(DVDCommandBlock* block, DVDCBCallback callback);
#ifdef __cplusplus
}
#endif
