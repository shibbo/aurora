#pragma once

#include <condition_variable>
#include <mutex>

namespace aurora::os {
std::mutex& nativeMutex();
std::condition_variable& nativeCondition();
extern thread_local bool sInterruptsEnabled;

struct NativeLock {
	std::unique_lock<std::mutex> lock;

	NativeLock() : lock(nativeMutex(), std::defer_lock) {
		if (sInterruptsEnabled) {
			lock.lock();
		} else {
			lock = std::unique_lock<std::mutex>(nativeMutex(), std::adopt_lock);
		}
	}

	~NativeLock() {
		if (!sInterruptsEnabled) {
			lock.release();
		}
	}
};
}
