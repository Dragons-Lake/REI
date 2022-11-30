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

#include "3rdParty/imgui/imgui.h"

#include "REI/Renderer.h"

#include "ResourceLoader.h"

struct REI_ImGui_Desc
{
    uint64_t                      vertexBufferSize;
    uint64_t                      indexBufferSize;
    uint32_t                      colorFormat : REI_FORMAT_BIT_COUNT;
    uint32_t                      depthStencilFormat : REI_FORMAT_BIT_COUNT;
    uint32_t                      sampleCount : REI_SAMPLE_COUNT_BIT_COUNT;
    uint32_t                      resourceSetCount;
    const REI_AllocatorCallbacks* pAllocator;
};

struct ImDrawData;

bool REI_ImGui_Init(REI_Renderer* Renderer, REI_ImGui_Desc* info);
void REI_ImGui_Shutdown();
void REI_ImGui_Render(ImDrawData* draw_data, REI_Cmd* command_buffer, uint32_t resource_set_index);
// if token is nullptr wait use batch wait
bool REI_ImGui_CreateFontsTexture(REI_RL_State* loader, REI_RL_RequestId* token = nullptr);
