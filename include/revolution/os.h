#pragma once

#include <dolphin/os.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*OSPowerCallback)(void);
OSPowerCallback OSSetPowerCallback(OSPowerCallback callback);
#ifdef __cplusplus
void OSRestart(u32 code) noexcept(false);
void OSReturnToMenu(void) noexcept(false);
void OSRebootSystem(void) noexcept(false);
void OSShutdownSystem(void) noexcept(false);
#else
void OSRestart(u32 code);
void OSReturnToMenu(void);
void OSRebootSystem(void);
void OSShutdownSystem(void);
#endif

void* OSGetMEM1ArenaLo(void);
void* OSGetMEM1ArenaHi(void);
void OSSetMEM1ArenaLo(void* newLo);
void OSSetMEM1ArenaHi(void* newHi);
void* OSAllocFromMEM1ArenaLo(u32 size, u32 align);
void* OSAllocFromMEM1ArenaHi(u32 size, u32 align);
void* OSGetMEM2ArenaLo(void);
void* OSGetMEM2ArenaHi(void);
void OSSetMEM2ArenaLo(void* newLo);
void OSSetMEM2ArenaHi(void* newHi);
void* OSAllocFromMEM2ArenaLo(u32 size, u32 align);
void* OSAllocFromMEM2ArenaHi(u32 size, u32 align);
BOOL OSIsMEM1Region(const void* addr);
BOOL OSIsMEM2Region(const void* addr);

#ifdef __cplusplus
}
#endif
