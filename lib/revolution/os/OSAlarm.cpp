#include <revolution/os.h>
#include "OSNative.hpp"
#include <aurora/time.hpp>

#include <algorithm>
#include <chrono>
#include <thread>

namespace {
using aurora::os::NativeLock;
using aurora::os::nativeCondition;
using aurora::os::nativeMutex;

OSAlarm* sHead;
OSAlarm* sTail;

void remove(OSAlarm* alarm) {
	if (alarm->prev != nullptr) {
		alarm->prev->next = alarm->next;
	} else {
		sHead = alarm->next;
	}

	if (alarm->next != nullptr) {
		alarm->next->prev = alarm->prev;
	} else {
		sTail = alarm->prev;
	}

	alarm->handler = nullptr;
	alarm->prev = nullptr;
	alarm->next = nullptr;
}

void insert(OSAlarm* alarm, OSAlarmHandler handler) {
	OSAlarm* next = sHead;
	OSAlarm* previous = nullptr;

	while (next != nullptr && next->fire <= alarm->fire) {
		previous = next;
		next = next->next;
	}

	alarm->handler = handler;
	alarm->prev = previous;
	alarm->next = next;

	if (previous != nullptr) {
		previous->next = alarm;
	} else {
		sHead = alarm;
	}

	if (next != nullptr) {
		next->prev = alarm;
	} else {
		sTail = alarm;
	}
}

void nextPeriod(OSAlarm* alarm, OSTime now) {
	alarm->fire = alarm->start;

	if (alarm->start < now) {
		alarm->fire += alarm->period * ((now - alarm->start) / alarm->period + 1);
	}
}

struct AlarmService {
	bool stopping = false;
	std::thread worker;

	AlarmService() : worker([this] { run(); }) {
	}

	~AlarmService() {
		{
			NativeLock lock;
			stopping = true;
			nativeCondition().notify_all();
		}

		worker.join();
	}

	void run() {
		while (true) {
			NativeLock lock;

			if (stopping) {
				return;
			}

			if (sHead == nullptr) {
				nativeCondition().wait(lock.lock);
				continue;
			}

			const OSTime now = OSGetTime();
			const OSTime remaining = sHead->fire - now;

			if (remaining > 0) {
				const OSTime ticks = std::min<OSTime>(remaining, OSMillisecondsToTicks(10));
				double seconds = 0.01;
				const float scale = aurora::time::scale();

				if (scale > 0.0f) {
					seconds = std::min(seconds, double(ticks) / OS_TIMER_CLOCK / scale);
				}

				const auto delay = std::chrono::duration<double>(seconds);
				nativeCondition().wait_for(lock.lock, delay);
				continue;
			}

			OSAlarm* alarm = sHead;
			const OSAlarmHandler handler = alarm->handler;
			remove(alarm);

			if (alarm->period > 0) {
				nextPeriod(alarm, now + 1);
				insert(alarm, handler);
			}

			lock.lock.release();
			aurora::os::sInterruptsEnabled = false;

			try {
				handler(alarm, &OSGetCurrentThread()->context);
			} catch (...) {
				OSEnableInterrupts();
				std::terminate();
			}

			OSEnableInterrupts();
		}
	}
};

AlarmService& service() {
	nativeMutex();
	nativeCondition();
	static AlarmService instance;
	return instance;
}

void set(OSAlarm* alarm, OSTime fire, OSTime period, OSAlarmHandler handler) {
	if (handler == nullptr) {
		OSPanic(__FILE__, __LINE__, "An alarm requires a handler");
	}

	service();
	NativeLock lock;

	if (alarm->handler != nullptr) {
		remove(alarm);
	}

	alarm->start = fire;
	alarm->fire = fire;
	alarm->period = period;

	if (period > 0) {
		nextPeriod(alarm, OSGetTime());
	}

	insert(alarm, handler);
	nativeCondition().notify_all();
}
}

void OSInitAlarm() {
	service();
}

void OSCreateAlarm(OSAlarm* alarm) {
	NativeLock lock;
	*alarm = {};
}

void OSSetAlarm(OSAlarm* alarm, OSTime ticks, OSAlarmHandler handler) {
	set(alarm, OSGetTime() + ticks, 0, handler);
}

void OSSetAbsAlarm(OSAlarm* alarm, OSTime time, OSAlarmHandler handler) {
	set(alarm, time, 0, handler);
}

void OSSetPeriodicAlarm(OSAlarm* alarm, OSTime start, OSTime period, OSAlarmHandler handler) {
	if (period <= 0) {
		OSPanic(__FILE__, __LINE__, "An alarm period must be positive");
	}

	set(alarm, start, period, handler);
}

void OSCancelAlarm(OSAlarm* alarm) {
	NativeLock lock;

	if (alarm->handler != nullptr) {
		remove(alarm);
		nativeCondition().notify_all();
	}
}

void OSSetAlarmTag(OSAlarm* alarm, u32 tag) {
	NativeLock lock;
	alarm->tag = tag;
}

void OSCancelAlarms(u32 tag) {
	NativeLock lock;
	OSAlarm* alarm = sHead;

	while (alarm != nullptr) {
		OSAlarm* next = alarm->next;

		if (alarm->tag == tag) {
			remove(alarm);
		}

		alarm = next;
	}

	nativeCondition().notify_all();
}

BOOL OSCheckAlarmQueue() {
	NativeLock lock;
	OSAlarm* previous = nullptr;

	for (OSAlarm* alarm = sHead; alarm != nullptr; alarm = alarm->next) {
		if (alarm->handler == nullptr || alarm->prev != previous) {
			return FALSE;
		}

		if (previous != nullptr && previous->fire > alarm->fire) {
			return FALSE;
		}

		previous = alarm;
	}

	return previous == sTail;
}

void OSSetAlarmUserData(OSAlarm* alarm, void* data) {
	NativeLock lock;
	alarm->userData = data;
}

void* OSGetAlarmUserData(const OSAlarm* alarm) {
	NativeLock lock;
	return alarm->userData;
}
