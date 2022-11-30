/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 */

#ifdef _WIN32
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include <SDL2/SDL.h>

#ifndef SAMPLE_TEST
#    include "REI_Sample/sample.h"
#endif

#include "tests.h"
#include "REI/Thread.h"

const char* dir_paths[DIRECTORY_COUNT];

void        init_directories();
const char* get_filename(const char* path);

static inline bool str_is_empty(const char* str) { return !str || !str[0]; }

static void log_output(void* userPtr, const char* msg)
{
#ifdef _DEBUG
    REI_DebugOutput(msg);
#endif
    REI_Print(msg);
    if (FILE* file = (FILE*)userPtr)
    {
        fprintf(file, "%s", msg);
        fflush(file);
    }
}

static FILE* register_log_output(const char* path)
{
    char        logFileName[1024];
    const char* filename = get_filename(path);
    snprintf(logFileName, 1024, "%s%s.log", dir_paths[DIRECTORY_LOG], str_is_empty(filename) ? "Log" : filename);

    FILE* file = fopen(logFileName, "w");
    sample_log_add_output(file, log_output);
    if (file)
    {
        fprintf(file, "date       time     [thread name/id ]                   file:line    v | message\n");
        fflush(file);

        sample_log(REI_LOG_TYPE_INFO, "Opened log file %s", logFileName);
    }
    else
    {
        sample_log(REI_LOG_TYPE_ERROR, "Failed to create log file %s , error: %s ", logFileName, strerror(errno));
    }

    return file;
}

static REI_CmdPool* cmdPool;
static REI_Cmd*     pCmd;
REI_Renderer* renderer;
REI_Queue*    gfxQueue;

int main(int argc, char* argv[])
{
#if defined(_DEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_DELAY_FREE_MEM_DF);
#endif
    init_directories();

    FILE* file = register_log_output(argv[0]);

    REI_RendererDesc rendererDesc{ "unit_test", nullptr, REI_SHADER_TARGET_5_1, false, nullptr, sample_log };

    Thread::SetMainThread();
    Thread::SetCurrentThreadName("MainThread");

    REI_initRenderer(&rendererDesc, &renderer);
    if (!renderer)
    {
        sample_log(REI_LOG_TYPE_ERROR, "Failed to create renderer");
        fclose(file);
        return 1;
    }

    REI_QueueDesc queueDesc{};
    queueDesc.type = REI_CMD_POOL_DIRECT;
    REI_addQueue(renderer, &queueDesc, &gfxQueue);
    REI_addCmdPool(renderer, gfxQueue, false, &cmdPool);
    REI_addCmd(renderer, cmdPool, false, &pCmd);

    perform_tests(renderer, gfxQueue, pCmd, cmdPool);

    REI_removeCmd(renderer, cmdPool, pCmd);
    REI_removeCmdPool(renderer, cmdPool);
    REI_removeQueue(gfxQueue);
    REI_removeRenderer(renderer);

    if (file)
    {
        fflush(file);
        fclose(file);
    }
    return 0;
}