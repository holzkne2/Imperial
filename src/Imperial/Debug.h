#pragma once

#define CODE_LOCATION __FILE__, __LINE__

//
// Log Message
//

#if DEBUG

#define ILOG(msg, ...) ILog(msg, CODE_LOCATION, __VA_ARGS__)
void ILog(const char* format, const char* filename, const int line, ...);

#else // #if DEBUG

#define ILOG(msg, ...) void(0)

#endif // #if DEBUG

//
// Log Error
//

#define IASSERT(condition, msg, ...) \
if (!(condition)) { \
		IAssert(msg, CODE_LOCATION, #condition, __VA_ARGS__); \
		__debugbreak(); \
} else void(0)

#define IASSERT_RETURN(condition, msg, ...) \
if (!(condition)) { \
	IAssert(msg, CODE_LOCATION, #condition, __VA_ARGS__); \
	__debugbreak(); \
	return; \
} else void(0)

#define IASSERT_RETURN_VALUE(condition, value, msg, ...) \
if (!(condition)) { \
	IAssert(msg, CODE_LOCATION, #condition, __VA_ARGS__); \
	__debugbreak(); \
	return value; \
} else void(0)

#define IASSERT_ONCE(condition, msg, ...) \
{static bool hit = false; \
if (!hit && !(condition)) { \
	IAssert(msg, CODE_LOCATION, #condition, __VA_ARGS__) ; \
	__debugbreak(); \
	hit = true; \
}}

void IAssert(const char* format, const char* filename, const int line, const char* condition, ...);