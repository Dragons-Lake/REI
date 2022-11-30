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
 * This file contains modified code from source code from 
 *
 * This file contains modified code from the REI project source code
 * (see https://github.com/Vi3LM/REI).
 */

#ifndef SAMPLE_TEST
#    include "REI_Sample/sample.h"
#    include "REI_Integration/rm_math.h"
#    include "REI_Integration/SimpleCamera.h"
#    include "REI_Integration/BasicDraw.h"
#endif

REI_CmdPool* cmdPool[FRAME_COUNT];
REI_Cmd*     pCmds[FRAME_COUNT];
REI_Texture* depthBuffer;

static REI_BasicDraw* basicDraw;
static SimpleCamera   cam;

enum
{
    BUFFER_SIZE = 1 << 20,
};

int sample_on_init()
{
    for (size_t i = 0; i < FRAME_COUNT; ++i)
    {
        REI_addCmdPool(renderer, gfxQueue, false, &cmdPool[i]);
        REI_addCmd(renderer, cmdPool[i], false, &pCmds[i]);
    }

    return 1;
}

void sample_on_fini()
{
    for (size_t i = 0; i < FRAME_COUNT; ++i)
    {
        REI_removeCmd(renderer, cmdPool[i], pCmds[i]);
        REI_removeCmdPool(renderer, cmdPool[i]);
    }
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

    SimpleCameraProjDesc projDesc = {};
    projDesc.proj_type = SimpleCameraProjInfiniteVulkan;
    projDesc.y_fov = RM_PI_2;
    projDesc.aspect = (float)swapchainDesc->width / swapchainDesc->height;
    projDesc.z_near = 0.1f;

    SimpleCamera_init(&cam, projDesc);
    SimpleCamera_rotate(&cam, 0.5f, 0.5f);
}

void sample_on_swapchain_fini()
{
    REI_BasicDraw_Shutdown(basicDraw);
    REI_removeTexture(renderer, depthBuffer);
}

void sample_on_event(SDL_Event* evt) {}

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
    loadActions.clearColorValues[0].rt.r = 0.0f;
    loadActions.clearColorValues[0].rt.g = 0.1f;
    loadActions.clearColorValues[0].rt.b = 0.3f;
    loadActions.clearColorValues[0].rt.a = 1.0f;
    loadActions.loadActionDepth = REI_LOAD_ACTION_CLEAR;
    loadActions.clearDepth.ds.depth = 1.0f;
    loadActions.clearDepth.ds.stencil = 0;
    REI_cmdBindRenderTargets(cmd, 1, &renderTarget, depthBuffer, &loadActions, NULL, NULL, 0, 0);
    REI_cmdSetViewport(cmd, 0.0f, 0.0f, (float)frameData->bbWidth, (float)frameData->bbHeight, 0.0f, 1.0f);
    REI_cmdSetScissor(cmd, 0, 0, frameData->bbWidth, frameData->bbHeight);

    REI_BasicDraw_SetupRender(basicDraw, frameData->setIndex);

    cam.p = RM_VALUE(rm_vec3, 0, 0, 0);
    SimpleCamera_rotate(&cam, 1.5f * frameData->dt / 1000000000.0f, 0.0f);
    SimpleCamera_move(&cam, 0, 0, -10);

    rm_mat4 mvp = rm_mat4_identity();
    rm_mat4 cam_mvp = SimpleCamera_buildViewProj(&cam);

    REI_BasicDraw_V_P3C* triVerts;
    REI_BasicDraw_V_P3C* points;
    REI_BasicDraw_Line*  lines;

    REI_BasicDraw_RenderLines(basicDraw, cmd, cam_mvp.a, 21 * 2, &lines);
    for (int i = -10; i <= 10; ++i)
    {
        lines[i + 10] = { { i * 10.0f, 0.0f, -100.0f }, 1.0f, { i * 10.0f, 0.0f, 100.0f }, 0xFFFFFF00 };
        lines[i + 31] = { { -100.0f, 0.0f, i * 10.0f }, 1.0f, { 100.0f, 0.0f, i * 10.0f }, 0xFFFFFF00 };
    }

    REI_BasicDraw_RenderLines(basicDraw, cmd, cam_mvp.a, 3, &lines);
    lines[0] = { { 0.0f, 0.0f, 0.0f }, 2.0f, { 1.0f, 0.0f, 0.0f }, 0xFF0000FF };
    lines[1] = { { 0.0f, 0.0f, 0.0f }, 2.0f, { 0.0f, 1.0f, 0.0f }, 0xFF00FF00 };
    lines[2] = { { 0.0f, 0.0f, 0.0f }, 2.0f, { 0.0f, 0.0f, 1.0f }, 0xFFFF0000 };

    REI_BasicDraw_RenderPoints(basicDraw, cmd, cam_mvp.a, 10.0f, 4, &points);
    points[0] = { { 0.0f, 0.0f, 0.0f }, 0xFFFFFFFF };
    points[1] = { { 1.0f, 0.0f, 0.0f }, 0xFF0000FF };
    points[2] = { { 0.0f, 1.0f, 0.0f }, 0xFF00FF00 };
    points[3] = { { 0.0f, 0.0f, 1.0f }, 0xFFFF0000 };

    REI_BasicDraw_RenderMesh(basicDraw, cmd, cam_mvp.a, 3, &triVerts);
    triVerts[0] = { { 0.0f, 3.5f, 17.0f }, 0xFF0000FF };
    triVerts[1] = { { 2.0f, 0.0f, 17.0f }, 0xFF00FF00 };
    triVerts[2] = { { -2.0f, 0.0f, 17.0f }, 0xFFFF0000 };

    float sx = 2.0f / frameData->bbWidth;
    float sy = 2.0f / frameData->bbHeight;

    REI_BasicDraw_RenderMesh(basicDraw, cmd, mvp.a, 6, &triVerts);
    float d = 10.0f;
    triVerts[0] = { { 1.0f - d * sx, -1.0f / 3.0f, 0.0f }, 0xFF383838 };
    triVerts[1] = { { -1.0f + d * sx, -1.0f + d * sy, 0.0f }, 0xFF383838 };
    triVerts[2] = { { -1.0f + d * sx, -1.0f / 3.0f, 0.0f }, 0xFF383838 };
    triVerts[3] = { { -1.0f + d * sx, -1.0f + d * sy, 0.0f }, 0xFF383838 };
    triVerts[4] = { { 1.0f - d * sx, -1.0f / 3.0f, 0.0f }, 0xFF383838 };
    triVerts[5] = { { 1.0f - d * sx, -1.0f + d * sy, 0.0f }, 0xFF383838 };

    REI_BasicDraw_RenderMesh(basicDraw, cmd, mvp.a, 3, &triVerts);
    float d2 = 10;
    triVerts[0] = { { -2.0f / 3.0f, -1.0f / 3.0f - d2 * sy, 0.0f }, 0xFF0000FF };
    triVerts[1] = { { -2.0f / 3.0f - 0.25f, -1.0f + (d + d2) * sy, 0.0f }, 0xFF00FF00 };
    triVerts[2] = { { -2.0f / 3.0f + 0.25f, -1.0f + (d + d2) * sy, 0.0f }, 0xFFFF0000 };

    float a = (float)frameData->bbWidth / frameData->bbHeight;

    REI_BasicDraw_RenderLines(basicDraw, cmd, mvp.a, 2, &lines);
    lines[0] = { { 0.0f, -2.0f / 3.0f, 0.0f }, 2.0f, { 1.0f / 6.0f / a, -2.0f / 3.0f, 0.0f }, 0xFF0000FF };
    lines[1] = { { 0.0f, -2.0f / 3.0f, 0.0f }, 2.0f, { 0.0f, -0.5f, 0.0f }, 0xFF00FF00 };

    REI_BasicDraw_RenderPoints(basicDraw, cmd, mvp.a, 10.0f, 3, &points);
    points[0] = { { 0.0f, -2.0f / 3.0f, 0.0f }, 0xFFFFFFFF };
    points[1] = { { 1.0f / 6.0f / a, -2.0f / 3.0f, 0.0f }, 0xFF0000FF };
    points[2] = { { 0.0f, -0.5f, 0.0f }, 0xFF00FF00 };

    REI_BasicDraw_RenderLines(basicDraw, cmd, mvp.a, 2, &lines);
    lines[0] = { { 2.0f / 3.0f - 0.25f, -1.0f / 3.0f - d2 * sy, 0.0f },
                 1.0f,
                 { 2.0f / 3.0f + 0.25f, -1.0f / 3.0f - d2 * sy, 0.0f },
                 0xFF0000FF };
    lines[1] = { { 2.0f / 3.0f - 0.25f, -1.0f + (d + d2) * sy, 0.0f },
                 1.0f,
                 { 2.0f / 3.0f + 0.25f, -1.0f + (d + d2) * sy, 0.0f },
                 0xFF0000FF };

    REI_BasicDraw_RenderLines(basicDraw, cmd, mvp.a, 2, &lines);
    lines[0] = { { 2.0f / 3.0f - 0.25f, -1.0f / 3.0f - d2 * sy, 0.0f },
                 1.0f,
                 { 2.0f / 3.0f - 0.25f, -1.0f + (d + d2) * sy, 0.0f },
                 0xFF0000FF };
    lines[1] = { { 2.0f / 3.0f + 0.25f, -1.0f / 3.0f - d2 * sy, 0.0f },
                 1.0f,
                 { 2.0f / 3.0f + 0.25f, -1.0f + (d + d2) * sy, 0.0f },
                 0xFF0000FF };

    REI_BasicDraw_V_P3* positions;
    REI_BasicDraw_RenderPoints(basicDraw, cmd, mvp.a, 10.0f, 4, &positions, 0xFF00FF00);
    positions[0] = { 2.0f / 3.0f - 0.25f, -2.0f / 3.0f, 0.0f };
    positions[1] = { 2.0f / 3.0f, -1.0f / 3.0f - d2 * sy, 0.0f };
    positions[2] = { 2.0f / 3.0f + 0.25f, -2.0f / 3.0f, 0.0f };
    positions[3] = { 2.0f / 3.0f, -1.0f + (d + d2) * sy, 0.0f };

    sample_cmdPrepareBackbuffer(cmd, renderTarget, REI_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = { depthBuffer, REI_RESOURCE_STATE_DEPTH_WRITE, REI_RESOURCE_STATE_COMMON };
    REI_cmdResourceBarrier(cmd, 0, nullptr, 1, &barriers[1]);
    REI_endCmd(cmd);

    sample_submit(cmd);
}
