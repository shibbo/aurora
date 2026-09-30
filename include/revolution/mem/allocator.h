#pragma once

#include <revolution/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MEMiHeapHead* MEMHeapHandle;
typedef struct MEMAllocator MEMAllocator;
typedef void* (*MEMFuncAllocatorAlloc)(MEMAllocator*, u32);
typedef void (*MEMFuncAllocatorFree)(MEMAllocator*, void*);

typedef struct MEMAllocatorFunc {
	MEMFuncAllocatorAlloc pfAlloc;
	MEMFuncAllocatorFree pfFree;
} MEMAllocatorFunc;

struct MEMAllocator {
	const MEMAllocatorFunc* pFunc;
	void* pHeap;
	u32 heapParam1;
	u32 heapParam2;
};

void* MEMAllocFromAllocator(MEMAllocator* allocator, u32 size);
void MEMFreeToAllocator(MEMAllocator* allocator, void* block);
void MEMInitAllocatorForExpHeap(MEMAllocator* allocator, MEMHeapHandle heap, int align);

#ifdef __cplusplus
}
#endif
