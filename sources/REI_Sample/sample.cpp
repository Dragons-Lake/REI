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

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include "REI/Common.h"
#include "REI/Thread.h"
#include "REI/Renderer.h"
#include "REI_Integration/ResourceLoader.h"

// NanoVG dependency
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ASSERT REI_ASSERT
#if defined(__ANDROID__)
#    define STBI_NO_SIMD
#endif
#include "REI_Integration/3rdParty/stb/stb_image.h"

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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_ASSERT REI_ASSERT
#include "REI_Integration/3rdParty/stb/stb_image_write.h"

#include "REI_Sample/sample.h"

struct FrameSync
{
    REI_Fence*     fence;
    REI_Semaphore* imageSem;
    REI_Semaphore* renderEndSem;
};

REI_Renderer* renderer;
REI_Queue*    gfxQueue;

static REI_Swapchain*    swapchain;
static REI_Texture**     ppSwapchainTextures;
static REI_SwapchainDesc swapChainDesc;
static FrameSync         frameSyncs[FRAME_COUNT];
static REI_Cmd*          frameCmd = 0;
static uint64_t          frameIndex = 0;
static uint32_t          setIndex = 0;

static REI_Buffer* screenshotBuffer;
static void*       screenshotData;
static uint32_t    screenshotSize;
static bool        doScreenshot = false;
static uint32_t    screenshotMask = 0;

static void sample_init_swapchain(REI_SwapchainDesc* swapchainDesc)
{
    REI_addSwapchain(renderer, swapchainDesc, &swapchain);
    swapChainDesc = *swapchainDesc;

    uint32_t count = 0;
    REI_getSwapchainTextures(swapchain, &count, NULL);
    ppSwapchainTextures = (REI_Texture**)malloc(count * sizeof(REI_Texture*));
    REI_getSwapchainTextures(swapchain, &count, ppSwapchainTextures);

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
    bufDesc.size = screenshotSize * FRAME_COUNT;
    bufDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
    REI_addBuffer(renderer, &bufDesc, &screenshotBuffer);
    REI_mapBuffer(renderer, screenshotBuffer, &screenshotData);
    sample_on_swapchain_init(swapchainDesc);
}

static void sample_fini_swapchain()
{
    sample_on_swapchain_fini();
    REI_removeBuffer(renderer, screenshotBuffer);
    free(ppSwapchainTextures);
    REI_removeSwapchain(renderer, swapchain);
}

int sample_init(REI_RendererDesc* rendererDesc, REI_SwapchainDesc* swapchainDesc)
{
    Thread::SetMainThread();
    Thread::SetCurrentThreadName("MainThread");

    REI_initRenderer(rendererDesc, &renderer);

    if (!renderer)
    {
        return 0;
    }

    REI_QueueDesc queueDesc{};
    queueDesc.type = REI_CMD_POOL_DIRECT;
    REI_addQueue(renderer, &queueDesc, &gfxQueue);

    for (uint32_t i = 0; i < FRAME_COUNT; ++i)
    {
        REI_addFence(renderer, &frameSyncs[i].fence);
        REI_addSemaphore(renderer, &frameSyncs[i].renderEndSem);
        REI_addSemaphore(renderer, &frameSyncs[i].imageSem);
    }

    sample_on_init();

    sample_init_swapchain(swapchainDesc);

    return 1;
}

void sample_fini()
{
    REI_waitQueueIdle(gfxQueue);

    sample_fini_swapchain();

    sample_on_fini();

    for (uint32_t i = 0; i < FRAME_COUNT; ++i)
    {
        REI_removeFence(renderer, frameSyncs[i].fence);
        REI_removeSemaphore(renderer, frameSyncs[i].renderEndSem);
        REI_removeSemaphore(renderer, frameSyncs[i].imageSem);
    }

    REI_removeQueue(gfxQueue);
    REI_removeRenderer(renderer);
}

void sample_resize(REI_SwapchainDesc* swapchainDesc)
{
    REI_waitQueueIdle(gfxQueue);

    sample_fini_swapchain();
    sample_init_swapchain(swapchainDesc);
}

void sample_save_screenshot(void* data, int w, int h, int premult, int bgra, const char* name);
void sample_render(uint64_t dt, uint32_t w, uint32_t h)
{
    setIndex = frameIndex % FRAME_COUNT;

    FrameSync& sync = frameSyncs[setIndex];
    uint32_t   frameImageIndex = 0;

    REI_acquireNextImage(renderer, swapchain, sync.imageSem, NULL, &frameImageIndex);
    REI_waitForFences(renderer, 1, &sync.fence);

    REI_Texture* backbuffer = ppSwapchainTextures[frameImageIndex];

    if (screenshotMask & (1u << setIndex))
    {
        screenshotMask &= ~(1u << setIndex);
        sample_save_screenshot(
            (uint8_t*)screenshotData + screenshotSize * setIndex, /*TODO: save info*/ swapChainDesc.width,
            /*TODO: save info*/ swapChainDesc.height, 0, 1, "screenshot.png");
    }

    FrameData frameData{ /*.dt = */ dt,
                         /*.backBuffer = */ backbuffer,
                         /*.setIndex = */ setIndex,
                         /*.bbWidth = */ swapChainDesc.width,
                         /*.bbHeight = */ swapChainDesc.height,
                         /*.winWidth = */ (uint32_t)w,
                         /*.winHeight = */ (uint32_t)h };
    sample_on_frame(&frameData);

    REI_ASSERT(frameCmd);
    REI_queueSubmit(gfxQueue, 1, &frameCmd, sync.fence, 1, &sync.imageSem, 1, &sync.renderEndSem);
    REI_queuePresent(gfxQueue, swapchain, frameImageIndex, 1, &sync.renderEndSem);

    frameCmd = NULL;
    ++frameIndex;
}

void sample_cmdPrepareBackbuffer(REI_Cmd* cmd, REI_Texture* backbuffer, REI_ResourceState startState)
{
    REI_cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

    REI_TextureBarrier barrier{ backbuffer };

    barrier.startState = startState;
    if (doScreenshot)
    {
        barrier.endState = REI_RESOURCE_STATE_COPY_SOURCE;
        REI_cmdResourceBarrier(cmd, 0, 0, 1, &barrier);

        REI_SubresourceDesc texData{};
        texData.bufferOffset = screenshotSize * setIndex;
        texData.region.w = swapChainDesc.width;
        texData.region.h = swapChainDesc.height;

        REI_cmdCopyTextureToBuffer(cmd, screenshotBuffer, barrier.pTexture, &texData);

        doScreenshot = false;
        screenshotMask |= 1u << setIndex;
        barrier.startState = barrier.endState;
    }

    barrier.endState = REI_RESOURCE_STATE_PRESENT;
    REI_cmdResourceBarrier(cmd, 0, 0, 1, &barrier);
}

void sample_submit(REI_Cmd* pCmd) { frameCmd = pCmd; }

void sample_request_screenshot() { doScreenshot = true; }

static void unpremultiplyAlpha(unsigned char* image, int w, int h, int stride)
{
    int x, y;

    // Unpremultiply
    for (y = 0; y < h; y++)
    {
        unsigned char* row = &image[y * stride];
        for (x = 0; x < w; x++)
        {
            int r = row[0], g = row[1], b = row[2], a = row[3];
            if (a != 0)
            {
                row[0] = REI_min<int>(r * 255 / a, 255);
                row[1] = REI_min<int>(g * 255 / a, 255);
                row[2] = REI_min<int>(b * 255 / a, 255);
            }
            row += 4;
        }
    }

    // Defringe
    for (y = 0; y < h; y++)
    {
        unsigned char* row = &image[y * stride];
        for (x = 0; x < w; x++)
        {
            int r = 0, g = 0, b = 0, a = row[3], n = 0;
            if (a == 0)
            {
                if (x - 1 > 0 && row[-1] != 0)
                {
                    r += row[-4];
                    g += row[-3];
                    b += row[-2];
                    n++;
                }
                if (x + 1 < w && row[7] != 0)
                {
                    r += row[4];
                    g += row[5];
                    b += row[6];
                    n++;
                }
                if (y - 1 > 0 && row[-stride + 3] != 0)
                {
                    r += row[-stride];
                    g += row[-stride + 1];
                    b += row[-stride + 2];
                    n++;
                }
                if (y + 1 < h && row[stride + 3] != 0)
                {
                    r += row[stride];
                    g += row[stride + 1];
                    b += row[stride + 2];
                    n++;
                }
                if (n > 0)
                {
                    row[0] = r / n;
                    row[1] = g / n;
                    row[2] = b / n;
                }
            }
            row += 4;
        }
    }
}

static void setAlpha(unsigned char* image, int w, int h, int stride, unsigned char a)
{
    int x, y;
    for (y = 0; y < h; y++)
    {
        unsigned char* row = &image[y * stride];
        for (x = 0; x < w; x++)
            row[x * 4 + 3] = a;
    }
}

static void swapRB(unsigned char* image, int w, int h, int stride)
{
    int           x, y;
    unsigned char c;
    for (y = 0; y < h; y++)
    {
        unsigned char* row = &image[y * stride];
        for (x = 0; x < w; x++)
        {
            c = row[x * 4];
            row[x * 4] = row[x * 4 + 2];
            row[x * 4 + 2] = c;
        }
    }
}

void sample_save_screenshot(void* data, int w, int h, int premult, int bgra, const char* name)
{
    unsigned char* image = (unsigned char*)malloc(w * h * 4);
    if (image == NULL)
        return;
    memcpy(image, data, w * h * 4);
    if (premult)
        unpremultiplyAlpha(image, w, h, w * 4);
    else
        setAlpha(image, w, h, w * 4, 255);
    if (bgra)
        swapRB(image, w, h, w * 4);
    stbi_write_png(name, w, h, 4, image, w * 4);
    free(image);
}
