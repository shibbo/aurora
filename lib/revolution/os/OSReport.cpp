#include <revolution/os.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

void OSVReport(const char* message, va_list args) {
	vfprintf(stdout, message, args);
	fflush(stdout);
}

void OSReport(const char* message, ...) {
	va_list args;
	va_start(args, message);
	OSVReport(message, args);
	va_end(args);
}

void OSPanic(const char* file, int line, const char* message, ...) {
	fprintf(stderr, "%s:%d: ", file, line);
	va_list args;
	va_start(args, message);
	vfprintf(stderr, message, args);
	va_end(args);
	fflush(stderr);
	abort();
}
