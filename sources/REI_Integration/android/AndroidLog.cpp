#include "REI/Interface/OperatingSystem.h"
#include "REI/Interface/Common.h"

int REI_FailedAssert(const char* file, int line, const char* statement, const char* message, ...)
{
    __android_log_print(
        ANDROID_LOG_ERROR, REI_ANDROID_LOG_TAG, "Assertion failed: (%s)\n\nFile: %s\nLine: %d\n\n", statement, file,
        line);
    if (message)
    {
        va_list ap;
        va_start(ap, message);
        __android_log_vprint(ANDROID_LOG_ERROR, REI_ANDROID_LOG_TAG, message, ap);
        va_end(ap);
    }

    return true;
}

void REI_DebugOutput(const char* str) { printf("%s", str); }

void REI_Print(const char* str) { __android_log_write(ANDROID_LOG_INFO, REI_ANDROID_LOG_TAG, str); }
