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

#pragma once

#include "REI/Renderer.h"

struct REI_BasicDraw_Desc
{
    uint32_t                      colorFormat : REI_FORMAT_BIT_COUNT;
    uint32_t                      depthStencilFormat : REI_FORMAT_BIT_COUNT;
    uint32_t                      sampleCount : REI_SAMPLE_COUNT_BIT_COUNT;
    uint32_t                      fbWidth;
    uint32_t                      fbHeight;
    uint32_t                      resourceSetCount;
    uint32_t                      maxDrawCount;
    uint32_t                      maxBufferSets;
    uint64_t                      maxDataSize;
    const REI_AllocatorCallbacks* pAllocator;
};

struct REI_BasicDraw_V_P3C
{
    float    p[3];
    uint32_t c;
};

struct REI_BasicDraw_V_P3
{
    float p[3];
};

struct REI_BasicDraw_Line
{
    float    p0[3];
    float    w;
    float    p1[3];
    uint32_t c;
};

struct REI_BasicDraw;

REI_BasicDraw* REI_BasicDraw_Init(REI_Renderer* Renderer, REI_BasicDraw_Desc* info);
void           REI_BasicDraw_SetupRender(REI_BasicDraw* state, uint32_t set_index);
void           REI_BasicDraw_Shutdown(REI_BasicDraw* state);

void REI_RegisterPointBufferSet(
    REI_BasicDraw* state, uint32_t idx, REI_Buffer* interleaved, REI_Buffer* positions, REI_Buffer* colors);

void REI_BasicDraw_RenderPoints(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], float ptsize, uint32_t count, REI_BasicDraw_V_P3C** point_data);
void REI_BasicDraw_RenderPoints(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], float ptsize, uint32_t count, REI_BasicDraw_V_P3** positions,
    uint32_t color);

void REI_BasicDraw_RenderLines(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], uint32_t count, REI_BasicDraw_Line** line_data);
void REI_BasicDraw_RenderMesh(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], uint32_t count, REI_BasicDraw_V_P3C** verts);
