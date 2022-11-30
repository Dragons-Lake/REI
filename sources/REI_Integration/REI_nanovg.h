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

#include "3rdParty/nanovg/nanovg.h"

#include "REI/Renderer.h"

struct REI_NanoVG_Desc
{
    uint32_t                      colorFormat : REI_FORMAT_BIT_COUNT;
    uint32_t                      depthStencilFormat : REI_FORMAT_BIT_COUNT;
    uint32_t                      sampleCount : REI_SAMPLE_COUNT_BIT_COUNT;
    uint32_t                      resourceSetCount;
    uint32_t                      maxVerts;
    uint32_t                      maxDraws;
    uint32_t                      maxTextures;
    const REI_AllocatorCallbacks* pAllocator;
};

struct NVGcontext;
struct REI_ResourceLoader;

NVGcontext* REI_NanoVG_Init(REI_Renderer* Renderer, REI_Queue* queue, REI_RL_State* loader, REI_NanoVG_Desc* info);
void        REI_NanoVG_SetupRender(NVGcontext* ctx, REI_Cmd* pCmd, uint32_t set_index);
void        REI_NanoVG_Shutdown(NVGcontext* ctx);
