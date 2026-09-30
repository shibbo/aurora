#include <revolution/dvd.h>

extern "C" BOOL DVDCheckDiskAsync(DVDCommandBlock* block, DVDCBCallback callback) {
	if (block == nullptr) {
		return FALSE;
	}

	const BOOL present = DVDCheckDisk();
	block->state = DVD_STATE_END;
	if (callback != nullptr) {
		callback(present, block);
	}

	return TRUE;
}
