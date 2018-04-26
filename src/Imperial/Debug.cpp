#include "Debug.h"

#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>

#define LOG_MESSAGE_LENGTH 1024

#if DEBUG
void ILog(const char* format, const char* filename, const int line, ...)
{
	char Buffer[LOG_MESSAGE_LENGTH];
	va_list args;
	va_start(args, format);

	char msg[LOG_MESSAGE_LENGTH] = "Log[%s, %i]: ";
	strcat_s(msg, LOG_MESSAGE_LENGTH, format);

	vsprintf_s(Buffer, msg, args);
	va_end(args);

	strcat_s(Buffer, LOG_MESSAGE_LENGTH, "\n");
	OutputDebugStringA(Buffer); // TODO: Run if not in debugging mode
}
#endif // #if DEBUG

void IAssert(const char* format, const char* filename, const int line, const char* condition, ...)
{
	char Buffer[LOG_MESSAGE_LENGTH];
	va_list args;
	va_start(args, format);

	char msg[LOG_MESSAGE_LENGTH] = "Assert[%s, %i]: condition \"%s\" failed: ";
	strcat_s(msg, LOG_MESSAGE_LENGTH, format);

	vsprintf_s(Buffer, msg, args);
	va_end(args);

	strcat_s(Buffer, LOG_MESSAGE_LENGTH, "\n");
	OutputDebugStringA(Buffer); // TODO: Run if not in debugging mode
}