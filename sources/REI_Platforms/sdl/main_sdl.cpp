/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 * Copyright (c) 2019-2022 Mykhailo Parfeniuk, Vladyslav Serhiienko.
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

#ifdef _WIN32
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include <sys/stat.h>
#if defined(_WIN32)
#include <crtdbg.h>
#else
#    include <unistd.h>
#endif
#include <errno.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "REI/Renderer.h"

#include "REI_Sample/sample.h"
#include "REI_Sample/Log.h"

int  sample_init(REI_RendererDesc* rendererDesc, REI_SwapchainDesc* swapchainDesc);
void sample_fini();
void sample_resize(REI_SwapchainDesc* swapchainDesc);
void sample_render(uint64_t dt, uint32_t w, uint32_t h);

void sample_input_init(SDL_Window* window);
void sample_input_resize();
void sample_input_fini();
void sample_input_on_event(SDL_Event* event);
void sample_input_update();

enum
{
    IMAGE_COUNT = 2
};

SDL_Window*       window = 0;
REI_SwapchainDesc swapchainDesc;

static uint64_t baseCntr = 0;
static uint64_t cntrFreq = 0;
static bool     disableRender = false;

const char* dir_paths[DIRECTORY_COUNT];

const char* get_filename(const char* path);

static inline bool str_is_empty(const char* str) { return !str || !str[0]; }

static inline uint64_t cntr_to_ns(uint64_t t)
{
    return (t / cntrFreq) * 1000000000ull + ((t % cntrFreq) * 1000000000ull) / cntrFreq;
}

void         init_directories();
static FILE* register_log_output(const char* path);
static void  log_output(void* userPtr, const char* msg);
REI_WindowHandle getPlatformWindowHandle(const SDL_SysWMinfo* inSysWMinfo);
void             getPlatformWindowProperties(int32_t* outWidth, int32_t* outHeight, uint32_t* outFlags);

static bool sample_sdl_handleEvents()
{
    SDL_Event evt;
    while (SDL_PollEvent(&evt))
    {
        switch (evt.type)
        {
            case SDL_QUIT: return false;
            case SDL_WINDOWEVENT:
                switch (evt.window.event)
                {
                    case SDL_WINDOWEVENT_RESIZED:
                    {
                        disableRender = false;
                        int width, height;
                        SDL_GL_GetDrawableSize(window, &width, &height);
                        swapchainDesc.width = width;
                        swapchainDesc.height = height;

                        sample_resize(&swapchainDesc);
                        sample_input_resize();
                        break;
                    }
                    case SDL_WINDOWEVENT_MINIMIZED:
                    {
                        disableRender = true;
                        break;
                    }
                    case SDL_WINDOWEVENT_RESTORED:
                    {
                        disableRender = false;
                        break;
                    }
                }
                break;
            case SDL_KEYDOWN:
                switch (evt.key.keysym.sym)
                {
                    case SDLK_PRINTSCREEN: sample_request_screenshot(); break;
                }
        }

        sample_on_event(&evt);
        sample_input_on_event(&evt);
    }

    return true;
}

int main(int argc, char* argv[])
{
#if defined(_DEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF|_CRTDBG_DELAY_FREE_MEM_DF);
#endif

    init_directories();

    FILE* file = register_log_output(argv[0]);

    int res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (res)
    {
        sample_log(REI_LOG_TYPE_ERROR, "Failed to create window");
        fclose(file);
        return 0;
    }

    uint32_t flags = 0;
    int32_t width = 0;
    int32_t height = 0;
    getPlatformWindowProperties(&width, &height, &flags);

    bool vsync = true;
    const char* gpu_name = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (sscanf(argv[i], "-w=%d", &width) == 1) continue;
        if (sscanf(argv[i], "-h=%d", &height) == 1) continue;
        if (strncmp(argv[i], "-gpu=", sizeof("-gpu=") - 1)==0) {gpu_name = argv[i] + sizeof("-gpu=") - 1; continue;}
        if (strcmp(argv[i], "-novsync")==0) {vsync = false; continue;}
    }

    window = SDL_CreateWindow("Vulkan Sample", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!window)
    {
        sample_log(REI_LOG_TYPE_ERROR, "Failed to create window");
        fclose(file);
        return 0;
    }

    SDL_GL_GetDrawableSize(window, &width, &height);

    sample_input_init(window);

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);

    swapchainDesc.presentQueueCount = 1;
    swapchainDesc.ppPresentQueues = &gfxQueue;
    swapchainDesc.width = width;
    swapchainDesc.height = height;
    swapchainDesc.imageCount = IMAGE_COUNT;
    swapchainDesc.sampleCount = REI_SAMPLE_COUNT_1;
    swapchainDesc.colorFormat = REI_getRecommendedSwapchainFormat(false);
    swapchainDesc.enableVsync = vsync;
    swapchainDesc.windowHandle = getPlatformWindowHandle(&info);

    REI_RendererDesc rendererDesc{ "triangle_test", gpu_name, REI_SHADER_TARGET_5_1, false, nullptr, sample_log };

    int run = sample_init(&rendererDesc, &swapchainDesc);
    baseCntr = SDL_GetPerformanceCounter();
    cntrFreq = SDL_GetPerformanceFrequency();
    uint64_t t1, t0 = baseCntr;
    while (run)
    {
        run = sample_sdl_handleEvents();
        if (disableRender)
            continue;

        sample_input_update();
        t1 = SDL_GetPerformanceCounter();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        sample_render(cntr_to_ns(t1 - t0), w, h);
        t0 = t1;
    }

    sample_fini();
    sample_input_fini();

    SDL_DestroyWindow(window);

    SDL_Quit();

    if (file)
    {
        fflush(file);
        fclose(file);
    }
    return 0;
}

uint64_t sample_time_ns() { return cntr_to_ns(SDL_GetPerformanceCounter() - baseCntr); }

void sample_quit() { SDL_Quit(); }

const char* sample_get_path(DirectoryEnum dir, const char* path, char* out, size_t size)
{
    strcpy(out, dir_paths[dir]);
    strcat(out, "/");
    strcat(out, path);
    return out;
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
        sample_log(
            REI_LOG_TYPE_ERROR, "Failed to create log file %s , error: %s ", logFileName, strerror(errno));
    }

    return file;
}

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
