/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 * Copyright (c) 2019-2022 Mykhailo Parfeniuk, Vladyslav Serhiienko.
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 *
 * This file contains modified code from the REI project source code
 * (see https://github.com/Vi3LM/REI).
 */

#define _CRT_SECURE_NO_WARNINGS

#include <time.h>
#include <ctime>

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <vector>

#include "REI/Thread.h"
#include "REI/Common.h"

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
    Log() = default;

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

void sample_log_add_output(void* userData, void (*outputFunc)(void* userPtr, const char* msg))
{
    MutexLock lock{ gLogger.mutex };
    gLogger.outputs.push_back({ userData, outputFunc });
}

static uint32_t sample_write_preamble(char* buffer, uint32_t buffer_size)
{
    char   thread_name[MAX_THREAD_NAME_LENGTH + 1] = { 0 };
    time_t t = time(NULL);
    tm     time_info;
#ifdef _WIN32
    localtime_s(&time_info, &t);
#elif REI_PLATFORM_SWITCH
    localtime_r(&t, &time_info);
#else
    localtime_s(&t, &time_info);
#endif
    Thread::GetCurrentThreadName(thread_name, MAX_THREAD_NAME_LENGTH + 1);

    return snprintf(
        buffer, buffer_size, "%04d-%02d-%02d %02d:%02d:%02d [%-15s] ", 1900 + time_info.tm_year,
        1 + time_info.tm_mon, time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec,
        thread_name[0] == 0 ? "NoName" : thread_name);
}

void sample_log(REI_LogType level, const char* message, ...)
{
    uint32_t preable_end = sample_write_preamble(Buffer, LOG_PREAMBLE_SIZE);

    const char* log_level_str;
    switch (level)
    {
        case REI_LOG_TYPE_WARNING: log_level_str = "WARN| "; break;
        case REI_LOG_TYPE_INFO: log_level_str = "INFO| "; break;
        case REI_LOG_TYPE_DEBUG: log_level_str = " DBG| "; break;
        case REI_LOG_TYPE_ERROR: log_level_str = " ERR| "; break;
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