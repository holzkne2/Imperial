#pragma once

#define FILE_LOCATION __FILE__, __LINE__

//
// Log Message
//

#if DEBUG

#define ILOG(msg, ...) ILog(msg, FILE_LOCATION, __VA_ARGS__)
void ILog(const char* format, const char* filename, const int line, ...);

#else // #if DEBUG

#define ILOG(msg, ...) void(0)

#endif // #if DEBUG

//
// Log Error
//

#define IERROR(condition, msg, ...) \
if (!condition) { \
	IError(msg, FILE_LOCATION, #condition, __VA_ARGS__); \
	DebugBreak(); \
} else void(0)

void IError(const char* format, const char* filename, const int line, const char* condition, ...);