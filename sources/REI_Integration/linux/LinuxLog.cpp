/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#ifdef __linux__

#include <cstdio>
#include <iostream>
#include <unistd.h>
#include "../Interfaces/IOperatingSystem.h"

// interfaces
#include "../Interfaces/ILog.h"
#include <assert.h>
#include "../Interfaces/IMemory.h"

void outputLogString(const char* pszStr)
{
	_OutputDebugString(pszStr);
	_OutputDebugString("\n");
}

void _OutputDebugString(const char* str, ...)
{
#ifdef _DEBUG
	const unsigned BUFFER_SIZE = 4096;
	char           buf[BUFFER_SIZE];

	va_list arglist;
	va_start(arglist, str);
	vsprintf_s(buf, BUFFER_SIZE, str, arglist);
	va_end(arglist);

	printf("%s\n", buf);
#endif
}

void _FailedAssert(const char* file, int line, const char* statement)
{
	static bool debug = true;

	if (debug)
	{
		printf("Failed: (%s)\n\nFile: %s\nLine: %d\n\n", statement, file, line);
	}
	//assert(0);
}

void _PrintUnicode(const eastl::string& str, bool error) { outputLogString(str.c_str()); }

void _PrintUnicodeLine(const eastl::string& str, bool error) { _PrintUnicode(str, error); }
#endif    // ifdef __linux__