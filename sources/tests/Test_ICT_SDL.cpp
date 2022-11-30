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

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_loadso.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <errno.h>

#include "REI/Common.h"
#include "REI_Sample/Log.h"

// NanoVG dependency
#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT REI_ASSERT
#if defined(__ANDROID__)
#    define STBI_NO_SIMD
#endif
#include "REI_integration/3rdParty/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT REI_ASSERT
#include "REI_integration/3rdParty/stb/stb_image_write.h"

#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

#ifdef _WIN32
#include "REI/3rdparty/renderdoc/renderdoc_app.h"
#endif
#include "REI/Renderer.h"

#include "REI_Integration/rm_math.h"
#include "REI_Integration/SimpleCamera.h"
#include "REI_Integration/BasicDraw.h"
#include "REI_Integration/REI_Fontstash.h"
#include "REI_Integration/REI_nanovg.h"

#include "REI_Sample/sample.h"

#ifdef _WIN32
#    define RENDERDOC_SO_FILE "renderdoc.dll"
#else
#    define RENDERDOC_SO_FILE "librenderdoc.so"
#endif

#define BEGIN_SAMPLE(name) \
    namespace name         \
    {
#define END_SAMPLE }

#define DECLARE_SAMPLE(name)                                                               \
    {                                                                                      \
#        name, name::sample_on_init, name::sample_on_fini, name::sample_on_swapchain_init, \
            name::sample_on_swapchain_fini, name::sample_on_event, name::sample_on_frame,  \
    }

#define SAMPLE_TEST

BEGIN_SAMPLE(sample_basic_draw)
#include "samples/sample_basic_draw.cpp"
END_SAMPLE
BEGIN_SAMPLE(sample_fontstash)
#include "samples/sample_fontstash.cpp"
END_SAMPLE
BEGIN_SAMPLE(sample_triangle)
#include "samples/sample_triangle.cpp"
END_SAMPLE
BEGIN_SAMPLE(sample_nanovg)
#include "samples/sample_nanovg.cpp"
END_SAMPLE

struct Sample
{
    const char* name;
    int (*fp_on_init)();
    void (*fp_on_fini)();
    void (*fp_on_swapchain_init)(const REI_SwapchainDesc* swapchainDesc);
    void (*fp_on_swapchain_fini)();
    void (*fp_on_event)(SDL_Event* evt);
    void (*fp_on_frame)(const FrameData* frameDesc);
};

Sample samples[] = { DECLARE_SAMPLE(sample_basic_draw), DECLARE_SAMPLE(sample_fontstash), DECLARE_SAMPLE(sample_nanovg),
                     DECLARE_SAMPLE(sample_triangle) };

REI_SwapchainDesc swapchainDesc;

REI_Renderer* renderer;
REI_Queue*    gfxQueue;

static REI_Cmd* frameCmd = NULL;

static REI_Texture* backbuffer;
static REI_Buffer*  screenshotBuffer;
static void*        screenshotData;
static uint32_t     screenshotSize;

const char* dir_paths[DIRECTORY_COUNT];

void init_directories();

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

const char* get_filename(const char* path);

static inline bool str_is_empty(const char* str) { return !str || !str[0]; }

void sample_save_screenshot(void* data, int w, int h, const char* name)
{
    unsigned char* image = (unsigned char*)malloc(w * h * 4);
    if (image == NULL)
        return;
    memcpy(image, data, w * h * 4);
    stbi_write_png(name, w, h, 4, image, w * 4);
    free(image);
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
            REI_LOG_TYPE_INFO, "Failed to create log file %s , error: %s ", logFileName,
            strerror(errno));
    }

    return file;
}

int sample_init(Sample* sample, REI_SwapchainDesc* swapchainDesc)
{
    REI_RendererDesc desc{ "Test_ICT", nullptr, REI_SHADER_TARGET_5_1, false, nullptr, sample_log };

    REI_initRenderer(&desc, &renderer);

    if (!renderer)
    {
        return 0;
    }

    REI_QueueDesc queueDesc{};
    queueDesc.type = REI_CMD_POOL_DIRECT;
    REI_addQueue(renderer, &queueDesc, &gfxQueue);

    sample->fp_on_init();

    REI_TextureDesc backbufferDesc{};
    backbufferDesc.format = swapchainDesc->colorFormat;
    backbufferDesc.width = swapchainDesc->width;
    backbufferDesc.height = swapchainDesc->height;
    backbufferDesc.sampleCount = REI_SAMPLE_COUNT_1;
    backbufferDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE | REI_DESCRIPTOR_TYPE_RENDER_TARGET;
    REI_addTexture(renderer, &backbufferDesc, &backbuffer);

    uint32_t sizeofBlock = 0;
    switch (swapchainDesc->colorFormat)
    {
        case REI_FMT_A2B10G10R10_UNORM:
        case REI_FMT_B8G8R8A8_UNORM:
        case REI_FMT_R8G8B8A8_UNORM: sizeofBlock = 4; break;
        case REI_FMT_R5G5B5A1_UNORM:
        case REI_FMT_R5G5B5X1_UNORM:
        case REI_FMT_R5G6B5_UNORM: sizeofBlock = 2; break;
        default: REI_ASSERT(0);
    }
    screenshotSize = sizeofBlock * swapchainDesc->width * swapchainDesc->height;

    REI_BufferDesc bufDesc = {};
    bufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
    bufDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    bufDesc.size = screenshotSize;
    bufDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
    REI_addBuffer(renderer, &bufDesc, &screenshotBuffer);
    REI_mapBuffer(renderer, screenshotBuffer, &screenshotData);
    sample->fp_on_swapchain_init(swapchainDesc);

    return 1;
}

void sample_fini(Sample* sample)
{
    REI_waitQueueIdle(gfxQueue);

    sample->fp_on_swapchain_fini();
    REI_removeBuffer(renderer, screenshotBuffer);
    REI_removeTexture(renderer, backbuffer);

    sample->fp_on_fini();

    REI_removeQueue(gfxQueue);
    REI_removeRenderer(renderer);
}

void sample_render(Sample* sample, uint64_t dt, uint32_t w, uint32_t h)
{
    FrameData frameData{ /*.dt = */ 0,
                         /*.backBuffer = */ backbuffer,
                         /*.setIndex = */ 0,
                         /*.bbWidth = */ swapchainDesc.width,
                         /*.bbHeight = */ swapchainDesc.height,
                         /*.winWidth = */ (uint32_t)w,
                         /*.winHeight = */ (uint32_t)h };
    sample->fp_on_frame(&frameData);

    REI_ASSERT(frameCmd);
    REI_queueSubmit(gfxQueue, 1, &frameCmd, 0, 0, 0, 0, 0);
}

#define SAFE_CALL(obj, call) \
    do                       \
        if (obj)             \
            obj->call;       \
    while (0)

int main(int argc, char* argv[])
{
#ifdef _WIN32
    RENDERDOC_API_1_1_2* rd_api = NULL;
    void*                solib = SDL_LoadObject(RENDERDOC_SO_FILE);
    if (solib)
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)SDL_LoadFunction(solib, "RENDERDOC_GetAPI");
        int               ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&rd_api);
        REI_ASSERT(ret == 1);
    }

    SAFE_CALL(rd_api, SetCaptureFilePathTemplate("./REI_ICT_Test"));
#endif

    init_directories();

    FILE* file = register_log_output(argv[0]);

    uint32_t w = 1024;
    uint32_t h = 512;

    swapchainDesc.width = w;
    swapchainDesc.height = h;
    swapchainDesc.colorFormat = REI_FMT_R8G8B8A8_UNORM;

    for (Sample& sample: samples)
    {
        sample_init(&sample, &swapchainDesc);
#ifdef _WIN32
        SAFE_CALL(rd_api, StartFrameCapture(0, 0));
#endif
        sample_render(&sample, 0, w, h);
        REI_waitQueueIdle(gfxQueue);

        char fileName[256];
        snprintf(fileName, 256, "%sICT_%s_frame_%d.png", dir_paths[DIRECTORY_LOG], sample.name, 0);

        sample_save_screenshot(screenshotData, w, h, fileName);
#ifdef _WIN32
        SAFE_CALL(rd_api, EndFrameCapture(0, 0));
#endif
        sample_fini(&sample);
    }

    if (file)
    {
        fflush(file);
        fclose(file);
    }
#ifdef _WIN32
    SDL_UnloadObject(solib);
#endif
    return 0;
}

uint64_t sample_time_ns() { return 0; }

void sample_quit() {}

void sample_cmdPrepareBackbuffer(REI_Cmd* cmd, REI_Texture* backbuffer, REI_ResourceState startState)
{
    REI_cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

    REI_TextureBarrier barrier{ backbuffer, startState, REI_RESOURCE_STATE_COPY_SOURCE };
    REI_cmdResourceBarrier(cmd, 0, 0, 1, &barrier);

    REI_SubresourceDesc texData{};
    texData.region.w = swapchainDesc.width;
    texData.region.h = swapchainDesc.height;
    REI_cmdCopyTextureToBuffer(cmd, screenshotBuffer, barrier.pTexture, &texData);
}

void sample_submit(REI_Cmd* pCmd) { frameCmd = pCmd; }

void sample_request_screenshot() {}

const char* sample_get_path(DirectoryEnum dir, const char* path, char* out, size_t size)
{
    strcpy(out, dir_paths[dir]);
    strcat(out, "/");
    strcat(out, path);
    return out;
}
