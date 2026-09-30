#include <revolution/os.h>
#include "OSNative.hpp"
#include <revolution/os/OSMessage.h>
#include <revolution/os/OSMutex.h>

#include <condition_variable>
#include <chrono>
#include <cstdlib>
#include <new>
#include <mutex>
#include <thread>

namespace aurora::os {
std::mutex& nativeMutex() {
	static std::mutex mutex;
	return mutex;
}

std::condition_variable& nativeCondition() {
	static std::condition_variable condition;
	return condition;
}

thread_local bool sInterruptsEnabled = true;
thread_local OSThread sCurrentThread = [] {
	OSThread thread{};
	thread.state = OS_THREAD_STATE_RUNNING;
	thread.priority = 16;
	thread.base = 16;
	return thread;
}();

struct NativeThread {
	NativeThread* next;
	OSThread* thread;
	void* (*entry)(void*);
	void* argument;
	unsigned references;
	bool cancelled;
	bool finished;
	bool registered;
};

struct ThreadExit {
	void* value;
};

NativeThread* sThreads;
thread_local NativeThread* sNativeThread;
thread_local bool sExiting;
OSThread* sSchedulerOwner;
s32 sSchedulerDepth;
unsigned sCheckpointWaiters;

struct Waiter {
	OSThreadQueue* queue;
	Waiter* next;
	bool ready = false;
};

Waiter* sWaiters;


NativeThread* findThread(OSThread* thread) {
	for (auto* state = sThreads; state != nullptr; state = state->next) {
		if (state->thread == thread) {
			return state;
		}
	}

	return nullptr;
}

void releaseThread(NativeThread* state) {
	if (--state->references == 0) {
		state->~NativeThread();
		std::free(state);
	}
}

struct NativeReference {
	NativeThread* state;

	explicit NativeReference(NativeThread* value) : state(value) {
		++state->references;
	}

	~NativeReference() {
		releaseThread(state);
	}
};

void removeThread(NativeThread* state) {
	NativeThread** link = &sThreads;

	while (*link != state) {
		link = &(*link)->next;
	}

	*link = state->next;
	state->registered = false;
	releaseThread(state);
}

bool cancelled() {
	return !sExiting && sNativeThread != nullptr && sNativeThread->cancelled;
}

void checkpoint(NativeLock& lock) {
	if (sExiting) {
		return;
	}

	OSThread* current = OSGetCurrentThread();

	while (!cancelled() && (current->suspend > 0 || (sSchedulerOwner != nullptr && sSchedulerOwner != current))) {
		++sCheckpointWaiters;
		nativeCondition().wait(lock.lock);
		--sCheckpointWaiters;
	}

	if (cancelled()) {
		sExiting = true;
		throw ThreadExit{reinterpret_cast<void*>(~uintptr_t(0))};
	}
}

void ownMutex(OSMutex* mutex, OSThread* thread) {
	mutex->thread = thread;
	mutex->link.prev = thread->queueMutex.tail;
	mutex->link.next = nullptr;

	if (thread->queueMutex.tail != nullptr) {
		thread->queueMutex.tail->link.next = mutex;
	} else {
		thread->queueMutex.head = mutex;
	}

	thread->queueMutex.tail = mutex;
}

void releaseMutex(OSMutex* mutex) {
	OSThread* thread = mutex->thread;

	if (mutex->link.prev != nullptr) {
		mutex->link.prev->link.next = mutex->link.next;
	} else {
		thread->queueMutex.head = mutex->link.next;
	}

	if (mutex->link.next != nullptr) {
		mutex->link.next->link.prev = mutex->link.prev;
	} else {
		thread->queueMutex.tail = mutex->link.prev;
	}

	mutex->thread = nullptr;
	mutex->count = 0;
	mutex->link = {};
}

void sleep(NativeLock& lock, OSThreadQueue* queue) {
	OSThread* current = OSGetCurrentThread();
	const u16 state = current->state;
	current->state = OS_THREAD_STATE_WAITING;
	current->queue = queue;
	current->link.prev = queue->tail;
	current->link.next = nullptr;

	if (queue->tail != nullptr) {
		queue->tail->link.next = current;
	} else {
		queue->head = current;
	}

	queue->tail = current;
	Waiter waiter{queue, sWaiters};
	sWaiters = &waiter;
	nativeCondition().wait(lock.lock, [&] { return waiter.ready || cancelled(); });
	Waiter** link = &sWaiters;

	while (*link != &waiter) {
		link = &(*link)->next;
	}

	*link = waiter.next;

	if (current->link.prev != nullptr) {
		current->link.prev->link.next = current->link.next;
	} else {
		queue->head = current->link.next;
	}

	if (current->link.next != nullptr) {
		current->link.next->link.prev = current->link.prev;
	} else {
		queue->tail = current->link.prev;
	}

	current->link = {};
	current->queue = nullptr;
	current->state = state;
	checkpoint(lock);
}

void wake(OSThreadQueue* queue) {
	bool changed = false;

	for (auto* waiter = sWaiters; waiter != nullptr; waiter = waiter->next) {
		if (waiter->queue == queue) {
			waiter->ready = true;
			changed = true;
		}
	}

	if (changed) {
		nativeCondition().notify_all();
	}
}

void runThread(NativeThread* state) {
	sNativeThread = state;
	void* result = nullptr;

	try {
		{
			NativeLock lock;
			checkpoint(lock);
			state->thread->state = OS_THREAD_STATE_RUNNING;
		}

		result = state->entry(state->argument);
	} catch (const ThreadExit& exit) {
		result = exit.value;
	} catch (...) {
		std::terminate();
	}

	OSEnableInterrupts();
	NativeLock lock;
	OSThread* thread = state->thread;

	while (thread->queueMutex.head != nullptr) {
		OSMutex* mutex = thread->queueMutex.head;
		releaseMutex(mutex);
		wake(&mutex->queue);
	}

	if (sSchedulerOwner == thread) {
		sSchedulerOwner = nullptr;
		sSchedulerDepth = 0;
	}

	thread->val = result;
	thread->state = OS_THREAD_STATE_MORIBUND;
	state->finished = true;

	if ((thread->attr & OS_THREAD_ATTR_DETACH) != 0) {
		thread->state = 0;
		removeThread(state);
	}

	nativeCondition().notify_all();
	sNativeThread = nullptr;
	releaseThread(state);
}

BOOL send(OSMessageQueue* queue, OSMessage message, s32 flags, bool front) {
	NativeLock lock;
	checkpoint(lock);

	while (queue->usedCount >= queue->msgCount) {
		if ((flags & OS_MESSAGE_BLOCK) == 0) {
			return FALSE;
		}

		sleep(lock, &queue->queueSend);
	}

	s32 index = (queue->firstIndex + queue->usedCount) % queue->msgCount;

	if (front) {
		queue->firstIndex = (queue->firstIndex + queue->msgCount - 1) % queue->msgCount;
		index = queue->firstIndex;
	}

	queue->msgArray[index] = message;
	++queue->usedCount;
	wake(&queue->queueReceive);
	return TRUE;
}
}

using namespace aurora::os;

BOOL OSDisableInterrupts() {
	const bool previous = sInterruptsEnabled;

	if (previous) {
		nativeMutex().lock();
		sInterruptsEnabled = false;
	}

	return previous;
}

BOOL OSEnableInterrupts() {
	const bool previous = sInterruptsEnabled;

	if (!previous) {
		sInterruptsEnabled = true;
		nativeMutex().unlock();
	}

	return previous;
}

BOOL OSRestoreInterrupts(BOOL enabled) {
	if (enabled) {
		return OSEnableInterrupts();
	}

	return OSDisableInterrupts();
}

OSThread* OSGetCurrentThread() {
	if (sNativeThread != nullptr) {
		return sNativeThread->thread;
	}

	return &sCurrentThread;
}

void OSYieldThread() noexcept(false) {
	bool canYield;
	{
		NativeLock lock;
		checkpoint(lock);
		canYield = sInterruptsEnabled && sSchedulerDepth == 0;
	}

	if (canYield) {
		std::this_thread::yield();
	}
}

void OSInitThreadQueue(OSThreadQueue* queue) {
	NativeLock lock;
	queue->head = nullptr;
	queue->tail = nullptr;
}

void OSSleepThread(OSThreadQueue* queue) noexcept(false) {
	NativeLock lock;
	checkpoint(lock);
	sleep(lock, queue);
}

void OSWakeupThread(OSThreadQueue* queue) {
	NativeLock lock;
	wake(queue);
}

void OSInitMutex(OSMutex* mutex) {
	NativeLock lock;
	*mutex = {};
}

void OSLockMutex(OSMutex* mutex) noexcept(false) {
	NativeLock lock;
	checkpoint(lock);
	OSThread* current = OSGetCurrentThread();

	while (mutex->thread != nullptr && mutex->thread != current) {
		sleep(lock, &mutex->queue);
	}

	if (mutex->thread == nullptr) {
		ownMutex(mutex, current);
	}

	++mutex->count;
}

BOOL OSTryLockMutex(OSMutex* mutex) {
	NativeLock lock;
	OSThread* current = OSGetCurrentThread();

	if (mutex->thread != nullptr && mutex->thread != current) {
		return FALSE;
	}

	if (mutex->thread == nullptr) {
		ownMutex(mutex, current);
	}

	++mutex->count;
	return TRUE;
}

void OSUnlockMutex(OSMutex* mutex) {
	NativeLock lock;

	if (mutex->thread != OSGetCurrentThread()) {
		return;
	}

	if (--mutex->count == 0) {
		releaseMutex(mutex);
		wake(&mutex->queue);
	}
}

void OSInitCond(OSCond* cond) {
	OSInitThreadQueue(&cond->queue);
}

void OSWaitCond(OSCond* cond, OSMutex* mutex) noexcept(false) {
	NativeLock lock;

	if (mutex->thread != OSGetCurrentThread()) {
		return;
	}

	const s32 count = mutex->count;
	releaseMutex(mutex);
	wake(&mutex->queue);
	sleep(lock, &cond->queue);

	while (mutex->thread != nullptr) {
		sleep(lock, &mutex->queue);
	}

	ownMutex(mutex, OSGetCurrentThread());
	mutex->count = count;
}

void OSSignalCond(OSCond* cond) {
	OSWakeupThread(&cond->queue);
}

void OSInitMessageQueue(OSMessageQueue* queue, OSMessage* messages, s32 count) {
	NativeLock lock;
	*queue = {};
	queue->msgArray = messages;
	queue->msgCount = count;
}

BOOL OSSendMessage(OSMessageQueue* queue, OSMessage message, s32 flags) noexcept(false) {
	return send(queue, message, flags, false);
}

BOOL OSJamMessage(OSMessageQueue* queue, OSMessage message, s32 flags) noexcept(false) {
	return send(queue, message, flags, true);
}

BOOL OSReceiveMessage(OSMessageQueue* queue, OSMessage* message, s32 flags) noexcept(false) {
	NativeLock lock;
	checkpoint(lock);

	while (queue->usedCount == 0) {
		if ((flags & OS_MESSAGE_BLOCK) == 0) {
			return FALSE;
		}

		sleep(lock, &queue->queueReceive);
	}

	if (message != nullptr) {
		*message = queue->msgArray[queue->firstIndex];
	}

	queue->firstIndex = (queue->firstIndex + 1) % queue->msgCount;
	--queue->usedCount;
	wake(&queue->queueSend);
	return TRUE;
}

BOOL OSCreateThread(OSThread* thread, void* (*entry)(void*), void* argument, void* stack, u32 stackSize, OSPriority priority, u16 attr) {
	if (thread == nullptr || entry == nullptr || stack == nullptr || stackSize < sizeof(u32) || priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
		return FALSE;
	}

	void* storage = std::malloc(sizeof(NativeThread));

	if (storage == nullptr) {
		return FALSE;
	}

	auto* state = new (storage) NativeThread{};
	state->thread = thread;
	state->entry = entry;
	state->argument = argument;
	state->references = 2;
	state->registered = true;

	{
		NativeLock lock;

		if (findThread(thread) != nullptr) {
			state->~NativeThread();
			std::free(state);
			return FALSE;
		}

		*thread = {};
		thread->state = OS_THREAD_STATE_READY;
		thread->attr = attr & OS_THREAD_ATTR_DETACH;
		thread->suspend = 1;
		thread->priority = priority;
		thread->base = priority;
		thread->stackBase = static_cast<u8*>(stack);
		thread->stackEnd = thread->stackBase - stackSize;
		state->next = sThreads;
		sThreads = state;
	}

	try {
		std::thread worker(runThread, state);
		worker.detach();
	} catch (...) {
		NativeLock lock;
		thread->state = 0;
		removeThread(state);
		releaseThread(state);
		return FALSE;
	}

	return TRUE;
}

s32 OSResumeThread(OSThread* thread) {
	NativeLock lock;
	const s32 previous = thread->suspend;

	if (previous > 0) {
		--thread->suspend;

		if (thread->suspend == 0) {
			nativeCondition().notify_all();
		}
	}

	return previous;
}

s32 OSSuspendThread(OSThread* thread) noexcept(false) {
	NativeLock lock;
	const s32 previous = thread->suspend;
	++thread->suspend;

	if (thread == OSGetCurrentThread()) {
		checkpoint(lock);
	}

	return previous;
}

BOOL OSIsThreadSuspended(OSThread* thread) {
	NativeLock lock;
	return thread->suspend > 0;
}

BOOL OSIsThreadTerminated(OSThread* thread) {
	NativeLock lock;
	return thread->state == OS_THREAD_STATE_MORIBUND || thread->state == 0;
}

BOOL OSJoinThread(OSThread* thread, void** value) noexcept(false) {
	NativeLock lock;
	NativeThread* state = findThread(thread);

	if (state == nullptr || thread == OSGetCurrentThread() || (thread->attr & OS_THREAD_ATTR_DETACH) != 0) {
		return FALSE;
	}

	NativeReference reference(state);
	nativeCondition().wait(lock.lock, [&] {
		return state->finished || (thread->attr & OS_THREAD_ATTR_DETACH) != 0 || cancelled();
	});
	checkpoint(lock);

	if (!state->registered || (thread->attr & OS_THREAD_ATTR_DETACH) != 0) {
		return FALSE;
	}

	if (value != nullptr) {
		*value = thread->val;
	}

	thread->state = 0;
	removeThread(state);
	return TRUE;
}

void OSDetachThread(OSThread* thread) {
	NativeLock lock;
	NativeThread* state = findThread(thread);
	thread->attr |= OS_THREAD_ATTR_DETACH;

	if (state != nullptr && state->finished) {
		thread->state = 0;
		removeThread(state);
	}

	nativeCondition().notify_all();
}

void OSCancelThread(OSThread* thread) noexcept(false) {
	NativeLock lock;
	NativeThread* state = findThread(thread);

	if (state == nullptr) {
		return;
	}

	state->cancelled = true;
	nativeCondition().notify_all();

	if (thread == OSGetCurrentThread()) {
		checkpoint(lock);
	}

	NativeReference reference(state);
	nativeCondition().wait(lock.lock, [&] { return state->finished; });

	if (state->registered) {
		thread->state = OS_THREAD_STATE_MORIBUND;
	}
}

void OSExitThread(void* value) noexcept(false) {
	sExiting = true;
	throw ThreadExit{value};
}

BOOL OSSetThreadPriority(OSThread* thread, OSPriority priority) {
	if (priority < OS_PRIORITY_MIN || priority > OS_PRIORITY_MAX) {
		return FALSE;
	}

	NativeLock lock;
	thread->base = priority;
	thread->priority = priority;
	return TRUE;
}

s32 OSGetThreadPriority(OSThread* thread) {
	NativeLock lock;
	return thread->priority;
}

s32 OSDisableScheduler() noexcept(false) {
	NativeLock lock;
	checkpoint(lock);
	OSThread* current = OSGetCurrentThread();

	const s32 previous = sSchedulerDepth;
	sSchedulerOwner = current;
	++sSchedulerDepth;
	return previous;
}

s32 OSEnableScheduler() {
	NativeLock lock;
	const s32 previous = sSchedulerDepth;

	if (sSchedulerOwner == OSGetCurrentThread() && sSchedulerDepth > 0) {
		--sSchedulerDepth;

		if (sSchedulerDepth == 0) {
			sSchedulerOwner = nullptr;

			if (sCheckpointWaiters != 0) {
				nativeCondition().notify_all();
			}
		}
	}

	return previous;
}

void OSSetThreadSpecific(s32 index, void* value) {
	if (index >= 0 && index < OS_THREAD_SPECIFIC_MAX) {
		OSGetCurrentThread()->specific[index] = value;
	}
}

void* OSGetThreadSpecific(s32 index) {
	if (index >= 0 && index < OS_THREAD_SPECIFIC_MAX) {
		return OSGetCurrentThread()->specific[index];
	}

	return nullptr;
}

void OSSleepTicks(OSTime ticks) noexcept(false) {
	if (ticks <= 0) {
		OSYieldThread();
		return;
	}

	const auto duration = std::chrono::duration<double>(static_cast<double>(ticks) / OS_TIMER_CLOCK);
	const auto deadline = std::chrono::steady_clock::now() + duration;
	NativeLock lock;
	checkpoint(lock);
	while (std::chrono::steady_clock::now() < deadline) {
		nativeCondition().wait_until(lock.lock, deadline);
		checkpoint(lock);
	}
}
