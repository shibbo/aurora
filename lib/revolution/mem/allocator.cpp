#include <revolution/mem/allocator.h>

void* MEMAllocFromAllocator(MEMAllocator* allocator, u32 size) {
	return allocator->pFunc->pfAlloc(allocator, size);
}

void MEMFreeToAllocator(MEMAllocator* allocator, void* block) {
	allocator->pFunc->pfFree(allocator, block);
}

#include <revolution/mem/expHeap.h>

namespace {
void* allocateExp(MEMAllocator* allocator, u32 size) {
	return MEMAllocFromExpHeapEx(static_cast<MEMHeapHandle>(allocator->pHeap), size, static_cast<int>(allocator->heapParam1));
}

void freeExp(MEMAllocator* allocator, void* block) {
	MEMFreeToExpHeap(static_cast<MEMHeapHandle>(allocator->pHeap), block);
}

const MEMAllocatorFunc sExpFunctions{allocateExp, freeExp};
}

void MEMInitAllocatorForExpHeap(MEMAllocator* allocator, MEMHeapHandle heap, int alignment) {
	allocator->pFunc = &sExpFunctions;
	allocator->pHeap = heap;
	allocator->heapParam1 = static_cast<u32>(alignment);
	allocator->heapParam2 = 0;
}
