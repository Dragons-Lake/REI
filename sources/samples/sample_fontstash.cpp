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

#ifndef SAMPLE_TEST
#    include "REI_Sample/sample.h"

#    include "REI_Integration/REI_fontstash.h"
#    include "REI_Integration/BasicDraw.h"
#    include <math.h>
#endif
#include <math.h>
enum
{
    BUFFER_SIZE = 1 << 20,
};

static REI_RL_State* resourceLoader;
static REI_CmdPool*  cmdPool[FRAME_COUNT];
static REI_Cmd*      pCmds[FRAME_COUNT];
static REI_Texture*  depthBuffer;

static REI_BasicDraw* basicDraw;

static FONScontext* fonsCtx;
static int          fontNormal = FONS_INVALID;
static int          fontItalic = FONS_INVALID;
static int          fontBold = FONS_INVALID;
static int          fontJapanese = FONS_INVALID;

static bool debug = false;

static float mvp[16] = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
};

int sample_on_init()
{
    REI_RL_addResourceLoader(renderer, nullptr, &resourceLoader);

    for (size_t i = 0; i < FRAME_COUNT; ++i)
    {
        REI_addCmdPool(renderer, gfxQueue, false, &cmdPool[i]);
        REI_addCmd(renderer, cmdPool[i], false, &pCmds[i]);
    }

    return 1;
}

void sample_on_fini()
{
    REI_RL_waitBatchCompleted(resourceLoader);

    for (size_t i = 0; i < FRAME_COUNT; ++i)
    {
        REI_removeCmd(renderer, cmdPool[i], pCmds[i]);
        REI_removeCmdPool(renderer, cmdPool[i]);
    }

    REI_RL_removeResourceLoader(resourceLoader);
}

void sample_on_swapchain_init(const REI_SwapchainDesc* swapchainDesc)
{
    // Add depth buffer
    REI_TextureDesc depthRTDesc{};
    depthRTDesc.clearValue.ds.depth = 1.0f;
    depthRTDesc.clearValue.ds.stencil = 0;
    depthRTDesc.format = REI_FMT_D32_SFLOAT;
    depthRTDesc.width = swapchainDesc->width;
    depthRTDesc.height = swapchainDesc->height;
    depthRTDesc.sampleCount = swapchainDesc->sampleCount;
    depthRTDesc.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET;
    REI_addTexture(renderer, &depthRTDesc, &depthBuffer);

    REI_BasicDraw_Desc srInfo = { swapchainDesc->colorFormat,
                                  depthRTDesc.format,
                                  swapchainDesc->sampleCount,
                                  swapchainDesc->width,
                                  swapchainDesc->height,
                                  FRAME_COUNT,
                                  512,
                                  128,
                                  BUFFER_SIZE };

    basicDraw = REI_BasicDraw_Init(renderer, &srInfo);

    REI_Fontstash_Desc fsInfo = { BUFFER_SIZE,
                                  swapchainDesc->colorFormat,
                                  depthRTDesc.format,
                                  swapchainDesc->sampleCount,
                                  FRAME_COUNT,
                                  swapchainDesc->width,
                                  swapchainDesc->height,
                                  1024,
                                  1024 };

    char path[256];

    fonsCtx = REI_Fontstash_Init(renderer, gfxQueue, resourceLoader, &fsInfo);

    fontNormal = fonsAddFont(
        fonsCtx, "sans", sample_get_path(DIRECTORY_DATA, "fonts/DroidSerif-Regular.ttf", path, sizeof(path)));
    fontItalic = fonsAddFont(
        fonsCtx, "sans-italic", sample_get_path(DIRECTORY_DATA, "fonts/DroidSerif-Italic.ttf", path, sizeof(path)));
    fontBold = fonsAddFont(
        fonsCtx, "sans-bold", sample_get_path(DIRECTORY_DATA, "fonts/DroidSerif-Bold.ttf", path, sizeof(path)));
    fontJapanese = fonsAddFont(
        fonsCtx, "sans-jp", sample_get_path(DIRECTORY_DATA, "fonts/DroidSansJapanese.ttf", path, sizeof(path)));

    REI_ASSERT(fontNormal != FONS_INVALID);
    REI_ASSERT(fontItalic != FONS_INVALID);
    REI_ASSERT(fontBold != FONS_INVALID);
    REI_ASSERT(fontJapanese != FONS_INVALID);

    mvp[0] = 2.0f / swapchainDesc->width;
    mvp[12] = -1.0f;
    mvp[5] = -2.0f / swapchainDesc->height;
    mvp[13] = 1.0f;
}

void sample_on_swapchain_fini()
{
    REI_Fontstash_Shutdown(fonsCtx);
    REI_BasicDraw_Shutdown(basicDraw);
    REI_removeTexture(renderer, depthBuffer);
}

void sample_on_event(SDL_Event* evt)
{
#ifndef __ANDROID__
    if (evt->type == SDL_KEYDOWN && evt->key.keysym.sym == SDLK_SPACE)
    {
        debug = !debug;
    }
#endif
}

static void dash(REI_Cmd* pCmd, float dx, float dy)
{
    REI_BasicDraw_V_P3C* lines;
    REI_BasicDraw_RenderMesh(basicDraw, pCmd, mvp, 6, &lines);

    lines[0] = { { dx - 5, dy, 0.0f }, 0x80000000 };
    lines[1] = { { dx - 10, dy, 0.0f }, 0x80000000 };
    lines[2] = { { dx - 5, dy + 1, 0.0f }, 0x80000000 };
    lines[3] = { { dx - 5, dy + 1, 0.0f }, 0x80000000 };
    lines[4] = { { dx - 10, dy, 0.0f }, 0x80000000 };
    lines[5] = { { dx - 10, dy + 1, 0.0f }, 0x80000000 };
}

static void line(REI_Cmd* pCmd, float sx, float sy, float ex, float ey)
{
    REI_BasicDraw_V_P3C* lines;
    REI_BasicDraw_RenderMesh(basicDraw, pCmd, mvp, 6, &lines);

    float dx = (ey - sy);
    float dy = -(ex - sx);

    float n = 1.0f / sqrtf(dx * dx + dy * dy);
    n = n < 0.0001f ? 1.0f : n;

    dx *= n;
    dy *= n;

    lines[0] = { { sx, sy, 0.0f }, 0x80000000 };
    lines[1] = { { ex, ey, 0.0f }, 0x80000000 };
    lines[2] = { { sx + dx, sy + dy, 0.0f }, 0x80000000 };
    lines[3] = { { sx + dx, sy + dy, 0.0f }, 0x80000000 };
    lines[4] = { { ex, ey, 0.0f }, 0x80000000 };
    lines[5] = { { ex + dx, ey + dy, 0.0f }, 0x80000000 };
}

static void text_update(REI_Cmd* pCmd)
{
    int white = 0xFFFFFFFF;
    int brown = 0x800080C0;
    int blue = 0xFFFFC000;
    int black = 0xFF000000;

    float sx = 50.0f;
    float sy = 50.0f;
    float dx = sx;
    float dy = sy;
    float lh = 0.0f;

    //dash(pCmd, dx,dy);

    fonsClearState(fonsCtx);

    fonsSetSize(fonsCtx, 124.0f);
    fonsSetFont(fonsCtx, fontNormal);
    fonsVertMetrics(fonsCtx, NULL, NULL, &lh);
    dx = sx;
    dy += lh;
    //dash(pCmd, dx,dy);

    fonsSetSize(fonsCtx, 124.0f);
    fonsSetFont(fonsCtx, fontNormal);
    fonsSetColor(fonsCtx, white);
    dx = fonsDrawText(fonsCtx, dx, dy, "The quick ", NULL);

    fonsSetSize(fonsCtx, 48.0f);
    fonsSetFont(fonsCtx, fontItalic);
    fonsSetColor(fonsCtx, brown);
    dx = fonsDrawText(fonsCtx, dx, dy, "brown ", NULL);

    fonsSetSize(fonsCtx, 24.0f);
    fonsSetFont(fonsCtx, fontNormal);
    fonsSetColor(fonsCtx, white);
    dx = fonsDrawText(fonsCtx, dx, dy, "fox ", NULL);

    fonsVertMetrics(fonsCtx, NULL, NULL, &lh);
    dx = sx;
    dy += lh * 1.2f;
    dash(pCmd, dx, dy);
    fonsSetFont(fonsCtx, fontItalic);
    dx = fonsDrawText(fonsCtx, dx, dy, "jumps over ", NULL);
    fonsSetFont(fonsCtx, fontBold);
    dx = fonsDrawText(fonsCtx, dx, dy, "the lazy ", NULL);
    fonsSetFont(fonsCtx, fontNormal);
    dx = fonsDrawText(fonsCtx, dx, dy, "dog.", NULL);

    dx = sx;
    dy += lh * 1.2f;
    dash(pCmd, dx, dy);
    fonsSetSize(fonsCtx, 12.0f);
    fonsSetFont(fonsCtx, fontNormal);
    fonsSetColor(fonsCtx, blue);
    fonsDrawText(fonsCtx, dx, dy, "Now is the time for all good men to come to the aid of the party.", NULL);

    fonsVertMetrics(fonsCtx, NULL, NULL, &lh);
    dx = sx;
    dy += lh * 1.2f * 2;
    dash(pCmd, dx, dy);
    fonsSetSize(fonsCtx, 18.0f);
    fonsSetFont(fonsCtx, fontItalic);
    fonsSetColor(fonsCtx, white);
    fonsDrawText(fonsCtx, dx, dy, "Ég get etið gler án þess að meiða mig.", NULL);

    fonsVertMetrics(fonsCtx, NULL, NULL, &lh);
    dx = sx;
    dy += lh * 1.2f;
    dash(pCmd, dx, dy);
    fonsSetFont(fonsCtx, fontJapanese);
    fonsDrawText(fonsCtx, dx, dy, "私はガラスを食べられます。それは私を傷つけません。", NULL);

    // Font alignment
    fonsSetSize(fonsCtx, 18.0f);
    fonsSetFont(fonsCtx, fontNormal);
    fonsSetColor(fonsCtx, white);

    dx = 50;
    dy = 350;
    line(pCmd, dx - 10, dy, dx + 250, dy);
    fonsSetAlign(fonsCtx, FONS_ALIGN_LEFT | FONS_ALIGN_TOP);
    dx = fonsDrawText(fonsCtx, dx, dy, "Top", NULL);
    dx += 10;
    fonsSetAlign(fonsCtx, FONS_ALIGN_LEFT | FONS_ALIGN_MIDDLE);
    dx = fonsDrawText(fonsCtx, dx, dy, "Middle", NULL);
    dx += 10;
    fonsSetAlign(fonsCtx, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
    dx = fonsDrawText(fonsCtx, dx, dy, "Baseline", NULL);
    dx += 10;
    fonsSetAlign(fonsCtx, FONS_ALIGN_LEFT | FONS_ALIGN_BOTTOM);
    fonsDrawText(fonsCtx, dx, dy, "Bottom", NULL);

    dx = 150;
    dy = 400;
    line(pCmd, dx, dy - 30, dx, dy + 80.0f);
    fonsSetAlign(fonsCtx, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);
    fonsDrawText(fonsCtx, dx, dy, "Left", NULL);
    dy += 30;
    fonsSetAlign(fonsCtx, FONS_ALIGN_CENTER | FONS_ALIGN_BASELINE);
    fonsDrawText(fonsCtx, dx, dy, "Center", NULL);
    dy += 30;
    fonsSetAlign(fonsCtx, FONS_ALIGN_RIGHT | FONS_ALIGN_BASELINE);
    fonsDrawText(fonsCtx, dx, dy, "Right", NULL);

    // Blur
    dx = 500;
    dy = 350;
    fonsSetAlign(fonsCtx, FONS_ALIGN_LEFT | FONS_ALIGN_BASELINE);

    fonsSetSize(fonsCtx, 60.0f);
    fonsSetFont(fonsCtx, fontItalic);
    fonsSetColor(fonsCtx, white);
    fonsSetSpacing(fonsCtx, 5.0f);
    fonsSetBlur(fonsCtx, 10.0f);
    fonsDrawText(fonsCtx, dx, dy, "Blurry...", NULL);

    dy += 50.0f;

    fonsSetSize(fonsCtx, 18.0f);
    fonsSetFont(fonsCtx, fontBold);
    fonsSetColor(fonsCtx, black);
    fonsSetSpacing(fonsCtx, 0.0f);
    fonsSetBlur(fonsCtx, 3.0f);
    fonsDrawText(fonsCtx, dx, dy + 2, "DROP THAT SHADOW", NULL);

    fonsSetColor(fonsCtx, white);
    fonsSetBlur(fonsCtx, 0);
    fonsDrawText(fonsCtx, dx, dy, "DROP THAT SHADOW", NULL);

    if (debug)
        fonsDrawDebug(fonsCtx, 800.0, 50.0);
}

void sample_on_frame(const FrameData* frameData)
{
    REI_Texture* renderTarget = frameData->backBuffer;
    REI_Cmd*     cmd = pCmds[frameData->setIndex];
    REI_CmdPool* pCmdPool = cmdPool[frameData->setIndex];

    REI_resetCmdPool(renderer, pCmdPool);
    REI_beginCmd(cmd);

    REI_TextureBarrier barriers[] = {
        { renderTarget, REI_RESOURCE_STATE_UNDEFINED, REI_RESOURCE_STATE_RENDER_TARGET },
        { depthBuffer, REI_RESOURCE_STATE_UNDEFINED, REI_RESOURCE_STATE_DEPTH_WRITE },
    };
    REI_cmdResourceBarrier(cmd, 0, nullptr, 2, barriers);

    REI_LoadActionsDesc loadActions{};
    loadActions.loadActionsColor[0] = REI_LOAD_ACTION_CLEAR;
    loadActions.clearColorValues[0].rt.r = 0.4f;
    loadActions.clearColorValues[0].rt.g = 0.4f;
    loadActions.clearColorValues[0].rt.b = 0.48f;
    loadActions.clearColorValues[0].rt.a = 1.0f;
    loadActions.loadActionDepth = REI_LOAD_ACTION_CLEAR;
    loadActions.clearDepth.ds.depth = 1.0f;
    loadActions.clearDepth.ds.stencil = 0;
    REI_cmdBindRenderTargets(cmd, 1, &renderTarget, depthBuffer, &loadActions, NULL, NULL, 0, 0);
    REI_cmdSetViewport(cmd, 0.0f, 0.0f, (float)frameData->bbWidth, (float)frameData->bbHeight, 0.0f, 1.0f);
    REI_cmdSetScissor(cmd, 0, 0, frameData->bbWidth, frameData->bbHeight);

    REI_Fontstash_SetupRender(fonsCtx, frameData->setIndex);
    REI_BasicDraw_SetupRender(basicDraw, frameData->setIndex);

    text_update(cmd);

    REI_Fontstash_Render(fonsCtx, cmd);

    sample_cmdPrepareBackbuffer(cmd, renderTarget, REI_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = { depthBuffer, REI_RESOURCE_STATE_DEPTH_WRITE, REI_RESOURCE_STATE_COMMON };
    REI_cmdResourceBarrier(cmd, 0, nullptr, 1, &barriers[1]);
    REI_endCmd(cmd);

    sample_submit(cmd);
}
