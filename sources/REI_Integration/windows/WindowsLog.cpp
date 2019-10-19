#include <stdio.h>

#include "REI/Interface/OperatingSystem.h"
#include "REI/Interface/Common.h"

int REI_FailedAssert(const char* file, int line, const char* statement, const char* message, ...)
{
    char    str[1024];
    int     pos = sprintf_s(str, "Assertion failed: (%s)\n\nFile: %s\nLine: %d\n\n", statement, file, line);
    if (message)
    {
        va_list ap;
        va_start(ap, message);
        if (pos < 1023)
            vsprintf_s(str + pos, 1024 - pos, message, ap);
        va_end(ap);
    }

    strcat_s(str, message ? "\n\nDebug?" : "Debug?");

    return MessageBoxA(NULL, str, "Assert failed", MB_YESNO | MB_ICONERROR) == IDYES;
}

void REI_DebugOutput(const char* str)
{
    OutputDebugStringA(str);
}

void REI_Print(const char* str)
{
    printf("%s", str);
}
