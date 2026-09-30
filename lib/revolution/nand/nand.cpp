#include <aurora/nand.hpp>
#include <revolution/nand.h>
#include <array>
#include <condition_variable>
#include <deque>
#include <functional>
#include <thread>
#include <unordered_set>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <new>
#include <system_error>
#include <utility>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {
namespace fs = std::filesystem;

struct File {
	FILE* stream = nullptr;
	const NANDFileInfo* owner = nullptr;
	fs::path path;
	fs::path replacement;
	u8 access = NAND_ACCESS_NONE;
};

struct Storage {
	std::mutex mutex;
	fs::path root;
	std::string home;
	std::array<File, 64> files;
	bool initialized = false;

	~Storage() {
		for (auto& file : files) {
			if (file.stream) {
				std::fclose(file.stream);
			}
		}
	}
};

Storage& storage() {
	static Storage value;
	return value;
}

s32 error(const std::error_code& code) {
	if (!code) {
		return NAND_RESULT_OK;
	}

	if (code == std::errc::no_such_file_or_directory) {
		return NAND_RESULT_NOEXISTS;
	}

	if (code == std::errc::file_exists) {
		return NAND_RESULT_EXISTS;
	}

	if (code == std::errc::permission_denied || code == std::errc::read_only_file_system) {
		return NAND_RESULT_ACCESS;
	}

	if (code == std::errc::no_space_on_device) {
		return NAND_RESULT_MAXBLOCKS;
	}

	if (code == std::errc::too_many_files_open || code == std::errc::too_many_files_open_in_system) {
		return NAND_RESULT_MAXFD;
	}

	if (code == std::errc::directory_not_empty) {
		return NAND_RESULT_NOTEMPTY;
	}

	if (code == std::errc::invalid_argument || code == std::errc::is_a_directory || code == std::errc::not_a_directory) {
		return NAND_RESULT_INVALID;
	}

	return NAND_RESULT_UNKNOWN;
}

s32 ioError() {
	if (errno == 0) {
		return NAND_RESULT_UNKNOWN;
	}

	return error(std::error_code(errno, std::generic_category()));
}

template<class Function>
s32 operation(Function function, bool requireInit = true) {
	auto& state = storage();
	std::lock_guard lock(state.mutex);

	if (requireInit && !state.initialized) {
		return NAND_RESULT_FATAL_ERROR;
	}

	try {
		return function(state);
	} catch (const fs::filesystem_error& exception) {
		return error(exception.code());
	} catch (const std::bad_alloc&) {
		return NAND_RESULT_ALLOC_FAILED;
	}
}

bool validPath(std::string_view path) {
	if (path.empty() || path.size() >= NAND_MAX_PATH || path.find('\0') != path.npos ||
		path.find('\\') != path.npos || path.find(':') != path.npos) {
		return false;
	}

	for (const auto& part : fs::path(path)) {
		if (part == "..") {
			return false;
		}
	}

	return true;
}

s32 resolve(Storage& state, const char* name, fs::path& result) {
	if (!name || !validPath(name)) {
		return NAND_RESULT_INVALID;
	}

	fs::path path(name);
	if (!path.is_absolute()) {
		path = fs::path(state.home) / path;
	}

	result = fs::weakly_canonical(state.root / path.relative_path());
	auto entry = result.begin();
	for (const auto& part : state.root) {
		if (entry == result.end() || *entry != part) {
			return NAND_RESULT_ACCESS;
		}

		++entry;
	}

	return NAND_RESULT_OK;
}

FILE* openFile(const fs::path& path, const char* mode) {
#ifdef _WIN32
	wchar_t wideMode[8]{};
	for (size_t i = 0; mode[i] != 0; ++i) {
		wideMode[i] = mode[i];
	}

	return _wfopen(path.c_str(), wideMode);
#else
	return std::fopen(path.c_str(), mode);
#endif
}

File* findFile(Storage& state, const NANDFileInfo* info) {
	if (!info || info->fileDescriptor < 0 || info->fileDescriptor >= state.files.size()) {
		return nullptr;
	}

	auto& file = state.files[info->fileDescriptor];
	if (!file.stream || file.owner != info) {
		return nullptr;
	}

	return &file;
}

bool isOpen(const Storage& state, const fs::path& path) {
	for (const auto& file : state.files) {
		if (file.stream && (file.path == path || file.replacement == path)) {
			return true;
		}
	}

	return false;
}
}

int32_t aurora::nand::configure(const fs::path& directory, std::string_view home) {
	return operation([&](Storage& state) -> s32 {
		if (directory.empty() || !validPath(home) || home.front() != '/') {
			return NAND_RESULT_INVALID;
		}

		for (const auto& file : state.files) {
			if (file.stream) {
				return NAND_RESULT_BUSY;
			}
		}

		auto root = fs::weakly_canonical(fs::absolute(directory));
		std::string homePath(home);
		state.root = std::move(root);
		state.home = std::move(homePath);
		state.initialized = false;
		return NAND_RESULT_OK;
	}, false);
}

s32 NANDInit() {
	return operation([](Storage& state) -> s32 {
		if (state.root.empty()) {
			return NAND_RESULT_FATAL_ERROR;
		}

		fs::path home;
		s32 result = resolve(state, state.home.c_str(), home);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		fs::path temporary;
		result = resolve(state, "/tmp", temporary);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		fs::create_directories(home);
		fs::create_directories(temporary);
		state.initialized = true;
		return NAND_RESULT_OK;
	}, false);
}

BOOL nandIsInitialized() {
	auto& state = storage();
	std::lock_guard lock(state.mutex);
	return state.initialized;
}

s32 NANDGetHomeDir(char* directory) {
	return operation([&](Storage& state) -> s32 {
		if (!directory) {
			return NAND_RESULT_INVALID;
		}

		std::memcpy(directory, state.home.c_str(), state.home.size() + 1);
		return NAND_RESULT_OK;
	});
}

s32 NANDCreate(const char* name, u8 permission, u8 attribute) {
	return operation([&](Storage& state) -> s32 {
		if ((permission & ~0x3F) || attribute != 0) {
			return NAND_RESULT_INVALID;
		}

		fs::path path;
		s32 result = resolve(state, name, path);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		FILE* file = openFile(path, "wbx");
		if (!file) {
			return ioError();
		}

		if (std::fclose(file) != 0) {
			return ioError();
		}

		fs::perms access = fs::perms::none;
		const std::pair<u8, fs::perms> permissions[] = {
			{NAND_PERM_RUSR, fs::perms::owner_read}, {NAND_PERM_WUSR, fs::perms::owner_write},
			{NAND_PERM_RGRP, fs::perms::group_read}, {NAND_PERM_WGRP, fs::perms::group_write},
			{NAND_PERM_ROTH, fs::perms::others_read}, {NAND_PERM_WOTH, fs::perms::others_write}
		};
		for (const auto& [bit, value] : permissions) {
			if (permission & bit) {
				access |= value;
			}
		}

		fs::permissions(path, access);

		return NAND_RESULT_OK;
	});
}

s32 NANDOpen(const char* name, NANDFileInfo* info, u8 access) {
	return operation([&](Storage& state) -> s32 {
		if (!info || access < NAND_ACCESS_READ || access > NAND_ACCESS_RW) {
			return NAND_RESULT_INVALID;
		}

		for (const auto& file : state.files) {
			if (file.stream && file.owner == info) {
				return NAND_RESULT_OPENFD;
			}
		}

		fs::path path;
		s32 result = resolve(state, name, path);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		for (size_t i = 0; i < state.files.size(); ++i) {
			auto& file = state.files[i];
			if (file.stream) {
				continue;
			}

			const char* mode = "rb";
			if (access != NAND_ACCESS_READ) {
				mode = "r+b";
			}

			file.path = path;
			file.stream = openFile(path, mode);
			if (!file.stream) {
				return ioError();
			}

			file.owner = info;
			file.access = access;
			std::memset(info, 0, sizeof(*info));
			info->fileDescriptor = static_cast<s32>(i);
			info->origFd = -1;
			info->accType = access;
			std::strcpy(info->origPath, name);
			return NAND_RESULT_OK;
		}

		return NAND_RESULT_MAXFD;
	});
}

static s32 closeFile(NANDFileInfo* info, bool commit) {
	return operation([&](Storage& state) -> s32 {
		auto* file = findFile(state, info);
		if (!file) {
			return NAND_RESULT_INVALID;
		}

		s32 result = NAND_RESULT_OK;
		if (file->access & NAND_ACCESS_WRITE) {
			if (std::fflush(file->stream) != 0) {
				result = ioError();
			} else {
#ifdef _WIN32
				const int synced = _commit(_fileno(file->stream));
#else
				const int synced = fsync(fileno(file->stream));
#endif
				if (synced != 0) {
					result = ioError();
				}
			}
		}

		const int closed = std::fclose(file->stream);
		file->stream = nullptr;
		file->owner = nullptr;
		info->fileDescriptor = -1;
		if (closed != 0 && result == NAND_RESULT_OK) {
			result = ioError();
		}

		if (!file->replacement.empty()) {
			if (commit && result == NAND_RESULT_OK) {
#ifdef _WIN32
				if (!MoveFileExW(file->path.c_str(), file->replacement.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
					result = error(std::error_code(GetLastError(), std::system_category()));
				}
#else
				std::error_code code;
				fs::rename(file->path, file->replacement, code);
				result = error(code);
#endif
			} else {
				std::error_code code;
				fs::remove(file->path, code);
			}

			file->replacement.clear();
		}

		return result;
	});
}

s32 NANDRead(NANDFileInfo* info, void* buffer, u32 length) {
	return operation([&](Storage& state) -> s32 {
		auto* file = findFile(state, info);
		if (!file || (!buffer && length) || length > INT_MAX) {
			return NAND_RESULT_INVALID;
		}

		if (!(file->access & NAND_ACCESS_READ)) {
			return NAND_RESULT_ACCESS;
		}

		if (std::fseek(file->stream, 0, SEEK_CUR) != 0) {
			return ioError();
		}

		const size_t count = std::fread(buffer, 1, length, file->stream);
		if (std::ferror(file->stream)) {
			return ioError();
		}

		return static_cast<s32>(count);
	});
}

s32 NANDWrite(NANDFileInfo* info, const void* buffer, u32 length) {
	return operation([&](Storage& state) -> s32 {
		auto* file = findFile(state, info);
		if (!file || (!buffer && length) || length > INT_MAX) {
			return NAND_RESULT_INVALID;
		}

		if (!(file->access & NAND_ACCESS_WRITE)) {
			return NAND_RESULT_ACCESS;
		}

		if (std::fseek(file->stream, 0, SEEK_CUR) != 0) {
			return ioError();
		}

		const size_t count = std::fwrite(buffer, 1, length, file->stream);
		if (count != length || std::fflush(file->stream) != 0) {
			return ioError();
		}

		return static_cast<s32>(count);
	});
}

s32 NANDGetLength(NANDFileInfo* info, u32* length) {
	return operation([&](Storage& state) -> s32 {
		auto* file = findFile(state, info);
		if (!file || !length) {
			return NAND_RESULT_INVALID;
		}

		const auto size = fs::file_size(file->path);
		if (size > UINT_MAX) {
			return NAND_RESULT_INVALID;
		}

		*length = static_cast<u32>(size);
		return NAND_RESULT_OK;
	});
}

s32 NANDDelete(const char* name) {
	return operation([&](Storage& state) -> s32 {
		fs::path path;
		s32 result = resolve(state, name, path);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		if (path == state.root) {
			return NAND_RESULT_ACCESS;
		}

		if (isOpen(state, path)) {
			return NAND_RESULT_OPENFD;
		}

		if (!fs::remove(path)) {
			return NAND_RESULT_NOEXISTS;
		}

		return NAND_RESULT_OK;
	});
}

s32 NANDMove(const char* name, const char* directory) {
	return operation([&](Storage& state) -> s32 {
		fs::path source;
		fs::path destination;
		s32 result = resolve(state, name, source);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		result = resolve(state, directory, destination);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		if (!fs::is_directory(destination)) {
			return NAND_RESULT_NOEXISTS;
		}

		if (source == state.root) {
			return NAND_RESULT_ACCESS;
		}

		destination /= source.filename();
		if (isOpen(state, source) || isOpen(state, destination)) {
			return NAND_RESULT_OPENFD;
		}

		fs::rename(source, destination);
		return NAND_RESULT_OK;
	});
}

s32 NANDCheck(u32 blocks, u32, u32* answer) {
	return operation([&](Storage& state) -> s32 {
		if (!answer) {
			return NAND_RESULT_INVALID;
		}

		*answer = 0;
		if (fs::space(state.root).available < static_cast<uint64_t>(blocks) * 16384) {
			*answer = NAND_CHECK_HOME_INSSPACE | NAND_CHECK_SYS_INSSPACE;
		}

		return NAND_RESULT_OK;
	});
}

void NANDInitBanner(NANDBanner* banner, u32 flag, const u16* title, const u16* comment) {
	std::memset(banner, 0, sizeof(*banner));
	banner->signature = NAND_BANNER_SIGNATURE;
	banner->flag = flag;
	const u16* text[] = {title, comment};
	for (size_t row = 0; row < 2; ++row) {
		if (!text[row] || !text[row][0]) {
			banner->comment[row][0] = ' ';
			continue;
		}

		for (size_t i = 0; i < NAND_BANNER_COMMENT_SIZE && text[row][i]; ++i) {
			banner->comment[row][i] = text[row][i];
		}
	}
}

void NANDSetUserData(NANDCommandBlock* block, void* data) {
	block->userData = data;
}

void* NANDGetUserData(const NANDCommandBlock* block) {
	return block->userData;
}


s32 NANDClose(NANDFileInfo* info) {
	return closeFile(info, false);
}

s32 NANDPrivateOpen(const char* path, NANDFileInfo* info, u8 access) {
	return NANDOpen(path, info, access);
}

s32 NANDPrivateCreate(const char* path, u8 permission, u8 attribute) {
	return NANDCreate(path, permission, attribute);
}

s32 NANDPrivateDelete(const char* path) {
	return NANDDelete(path);
}

s32 NANDSeek(NANDFileInfo* info, s32 offset, s32 origin) {
	return operation([&](Storage& state) -> s32 {
		auto* file = findFile(state, info);
		if (file == nullptr || origin < 0 || origin > 2) {
			return NAND_RESULT_INVALID;
		}

		const int origins[] = {SEEK_SET, SEEK_CUR, SEEK_END};
		if (std::fseek(file->stream, offset, origins[origin]) != 0) {
			return ioError();
		}

		const long position = std::ftell(file->stream);
		if (position < 0) {
			return ioError();
		}

		return static_cast<s32>(position);
	});
}

namespace {
struct Request {
	std::function<s32()> operation;
	NANDCallback callback;
	NANDCommandBlock* block;
};

class RequestQueue {
public:
	RequestQueue() : worker([this] { run(); }) {
	}

	~RequestQueue() {
		{
			std::lock_guard lock(mutex);
			stopping = true;
		}

		condition.notify_one();
		worker.join();
	}

	s32 submit(std::function<s32()> operation, NANDCallback callback, NANDCommandBlock* block) {
		if (block == nullptr || callback == nullptr) {
			return NAND_RESULT_INVALID;
		}

		std::lock_guard lock(mutex);
		if (stopping || pending.contains(block)) {
			return NAND_RESULT_BUSY;
		}

		pending.insert(block);
		try {
			requests.push_back({std::move(operation), callback, block});
		} catch (...) {
			pending.erase(block);
			throw;
		}

		condition.notify_one();
		return NAND_RESULT_OK;
	}

private:
	void run() {
		for (;;) {
			Request request;
			{
				std::unique_lock lock(mutex);
				condition.wait(lock, [&] { return stopping || !requests.empty(); });
				if (requests.empty()) {
					return;
				}

				request = std::move(requests.front());
				requests.pop_front();
			}

			const s32 result = request.operation();
			{
				std::lock_guard lock(mutex);
				pending.erase(request.block);
			}

			request.callback(result, request.block);
		}
	}

	std::mutex mutex;
	std::condition_variable condition;
	std::deque<Request> requests;
	std::unordered_set<NANDCommandBlock*> pending;
	bool stopping = false;
	std::thread worker;
};

RequestQueue& requestQueue() {
	(void)storage();
	static RequestQueue queue;
	return queue;
}

template<class Function>
s32 submit(Function function, NANDCallback callback, NANDCommandBlock* block) {
	try {
		return requestQueue().submit(std::move(function), callback, block);
	} catch (const std::bad_alloc&) {
		return NAND_RESULT_ALLOC_FAILED;
	} catch (const std::system_error&) {
		return NAND_RESULT_BUSY;
	}
}

s32 createDirectory(const char* name, u8 permission, u8 attribute) {
	return operation([&](Storage& state) -> s32 {
		if ((permission & ~0x3f) != 0 || attribute != 0) {
			return NAND_RESULT_INVALID;
		}

		fs::path path;
		const s32 result = resolve(state, name, path);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		if (!fs::create_directory(path)) {
			return NAND_RESULT_EXISTS;
		}

		return NAND_RESULT_OK;
	});
}

s32 safeOpen(const char* name, NANDFileInfo* info, u8 access) {
	if (access == NAND_ACCESS_READ) {
		return NANDOpen(name, info, access);
	}

	return operation([&](Storage& state) -> s32 {
		if (info == nullptr || (access != NAND_ACCESS_WRITE && access != NAND_ACCESS_RW)) {
			return NAND_RESULT_INVALID;
		}

		fs::path path;
		s32 result = resolve(state, name, path);
		if (result != NAND_RESULT_OK) {
			return result;
		}

		if (isOpen(state, path)) {
			return NAND_RESULT_OPENFD;
		}

		for (const auto& file : state.files) {
			if (file.stream && file.owner == info) {
				return NAND_RESULT_OPENFD;
			}
		}

		for (size_t i = 0; i < state.files.size(); ++i) {
			auto& file = state.files[i];
			if (file.stream != nullptr) {
				continue;
			}

			static uint64_t sequence;
			fs::path temporary;
			do {
				temporary = path;
				temporary += ".nand-" + std::to_string(++sequence) + ".tmp";
			} while (fs::exists(temporary));

			fs::copy_file(path, temporary);
			FILE* stream = openFile(temporary, "r+b");
			if (stream == nullptr) {
				const s32 failure = ioError();
				std::error_code code;
				fs::remove(temporary, code);
				return failure;
			}

			file.path = std::move(temporary);
			file.replacement = std::move(path);
			file.stream = stream;
			file.owner = info;
			file.access = access;
			std::memset(info, 0, sizeof(*info));
			info->fileDescriptor = static_cast<s32>(i);
			info->origFd = -1;
			info->accType = access;
			std::strcpy(info->origPath, name);
			return NAND_RESULT_OK;
		}

		return NAND_RESULT_MAXFD;
	});
}
}

s32 NANDOpenAsync(const char* path, NANDFileInfo* info, u8 access, NANDCallback callback, NANDCommandBlock* block) {
	if (path == nullptr || !validPath(path)) {
		return NAND_RESULT_INVALID;
	}

	return submit([path = std::string(path), info, access] { return NANDOpen(path.c_str(), info, access); }, callback, block);
}

s32 NANDPrivateOpenAsync(const char* path, NANDFileInfo* info, u8 access, NANDCallback callback, NANDCommandBlock* block) {
	return NANDOpenAsync(path, info, access, callback, block);
}

s32 NANDPrivateSafeOpenAsync(const char* path, NANDFileInfo* info, u8 access, void* buffer, u32 size, NANDCallback callback, NANDCommandBlock* block) {
	if (path == nullptr || !validPath(path) || buffer == nullptr || size == 0) {
		return NAND_RESULT_INVALID;
	}

	return submit([path = std::string(path), info, access] { return safeOpen(path.c_str(), info, access); }, callback, block);
}

s32 NANDCloseAsync(NANDFileInfo* info, NANDCallback callback, NANDCommandBlock* block) {
	return submit([=] { return NANDClose(info); }, callback, block);
}

s32 NANDSafeCloseAsync(NANDFileInfo* info, NANDCallback callback, NANDCommandBlock* block) {
	return submit([=] { return closeFile(info, true); }, callback, block);
}

s32 NANDReadAsync(NANDFileInfo* info, void* data, u32 size, NANDCallback callback, NANDCommandBlock* block) {
	return submit([=] { return NANDRead(info, data, size); }, callback, block);
}

s32 NANDWriteAsync(NANDFileInfo* info, const void* data, u32 size, NANDCallback callback, NANDCommandBlock* block) {
	return submit([=] { return NANDWrite(info, data, size); }, callback, block);
}

s32 NANDSeekAsync(NANDFileInfo* info, s32 offset, s32 origin, NANDCallback callback, NANDCommandBlock* block) {
	return submit([=] { return NANDSeek(info, offset, origin); }, callback, block);
}

s32 NANDGetLengthAsync(NANDFileInfo* info, u32* length, NANDCallback callback, NANDCommandBlock* block) {
	return submit([=] { return NANDGetLength(info, length); }, callback, block);
}

s32 NANDPrivateCreateAsync(const char* path, u8 permission, u8 attribute, NANDCallback callback, NANDCommandBlock* block) {
	if (path == nullptr || !validPath(path)) {
		return NAND_RESULT_INVALID;
	}

	return submit([path = std::string(path), permission, attribute] { return NANDCreate(path.c_str(), permission, attribute); }, callback, block);
}

s32 NANDPrivateCreateDirAsync(const char* path, u8 permission, u8 attribute, NANDCallback callback, NANDCommandBlock* block) {
	if (path == nullptr || !validPath(path)) {
		return NAND_RESULT_INVALID;
	}

	return submit([path = std::string(path), permission, attribute] { return createDirectory(path.c_str(), permission, attribute); }, callback, block);
}

s32 NANDPrivateDeleteAsync(const char* path, NANDCallback callback, NANDCommandBlock* block) {
	if (path == nullptr || !validPath(path)) {
		return NAND_RESULT_INVALID;
	}

	return submit([path = std::string(path)] { return NANDDelete(path.c_str()); }, callback, block);
}
