#include <time.h>
#include <ctime>

#include <stdio.h>
#include <string.h>
#include <vector>

#include "REI/Interface/Thread.h"
#include "REI/Interface/Common.h"

enum
{
    FILENAME_NAME_LENGTH_LOG = 23,
    INDENTATION_SIZE_LOG = 4,
    LOG_LEVEL_SIZE = 6,
    LOG_PREAMBLE_SIZE = (56 + MAX_THREAD_NAME_LENGTH + FILENAME_NAME_LENGTH_LOG),
    LOG_MESSAGE_OFFSET = (LOG_PREAMBLE_SIZE + LOG_LEVEL_SIZE),
    BUFFER_SIZE = 1024,
};

/// Logging subsystem.
struct Log
{
    Log();

    Mutex mutex;
    struct LogOutput
    {
        void* userData;
        void (*outputFunc)(void* userPtr, const char* msg);
    };
    std::vector<LogOutput> outputs;
};

static thread_local char Buffer[BUFFER_SIZE + 2];
static Log               gLogger;

// Returns the part of the path after the last / or \ (if any).
const char* get_filename(const char* path)
{
    for (auto ptr = path; *ptr; ++ptr)
    {
        if (*ptr == '/' || *ptr == '\\')
        {
            path = ptr + 1;
        }
    }
    return path;
}

void REI_LogAddOutput(void* userData, void (*outputFunc)(void* userPtr, const char* msg))
{
    MutexLock lock{ gLogger.mutex };
    gLogger.outputs.push_back({ userData, outputFunc });
}

uint32_t WritePreamble(char* buffer, uint32_t buffer_size, const char* file, int line)
{
    char   thread_name[MAX_THREAD_NAME_LENGTH + 1] = { 0 };
    time_t t = time(NULL);
    tm     time_info;
#ifdef _WIN32
    localtime_s(&time_info, &t);
#else
    localtime_r(&t, &time_info);
#endif
    Thread::GetCurrentThreadName(thread_name, MAX_THREAD_NAME_LENGTH + 1);
    file = get_filename(file);

    return snprintf(
        buffer, buffer_size, "%04d-%02d-%02d %02d:%02d:%02d [%-15s] %22.*s:%-5u ", 1900 + time_info.tm_year,
        1 + time_info.tm_mon, time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec,
        thread_name[0] == 0 ? "NoName" : thread_name, FILENAME_NAME_LENGTH_LOG, file, line);
}

void REI_LogWrite(uint32_t level, const char* filename, int line_number, const char* message, ...)
{
    uint32_t preable_end = WritePreamble(Buffer, LOG_PREAMBLE_SIZE, filename, line_number);

    const char* log_level_str;
    switch (level)
    {
        case LL_WARNING: log_level_str = "WARN| "; break;
        case LL_INFO: log_level_str = "INFO| "; break;
        case LL_DEBUG: log_level_str = " DBG| "; break;
        case LL_ERROR: log_level_str = " ERR| "; break;
        default: log_level_str = "NONE| "; break;
    }
    strncpy(Buffer + preable_end, log_level_str, LOG_LEVEL_SIZE);

    uint32_t offset = preable_end + LOG_LEVEL_SIZE;
    va_list  ap;
    va_start(ap, message);
    offset += vsnprintf(Buffer + offset, BUFFER_SIZE - offset, message, ap);
    va_end(ap);

    offset = (offset > BUFFER_SIZE) ? BUFFER_SIZE : offset;
    Buffer[offset] = '\n';
    Buffer[offset + 1] = 0;

    MutexLock lock{ gLogger.mutex };
    for (auto& output: gLogger.outputs)
    {
        output.outputFunc(output.userData, Buffer);
    }
}

void REI_LogWriteRaw(const char* message, ...)
{
    va_list ap;
    va_start(ap, message);
    vsnprintf(Buffer, BUFFER_SIZE, message, ap);
    va_end(ap);

    MutexLock lock{ gLogger.mutex };
    for (auto& output: gLogger.outputs)
    {
        output.outputFunc(output.userData, Buffer);
    }
}

Log::Log() {}
