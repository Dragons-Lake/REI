#pragma once

#include "3rdParty/imgui/imgui.h"

#include "REI/Renderer/Renderer.h"

#include "ResourceLoader.h"

struct REI_ImGui_Desc
{
    uint64_t        vertexBufferSize;
    uint64_t        indexBufferSize;
    REI_Format      colorFormat;
    REI_Format      depthStencilFormat;
    REI_SampleCount sampleCount;
    uint32_t        resourceSetCount;
};

struct ImDrawData;

bool REI_ImGui_Init(REI_Renderer* Renderer, REI_ImGui_Desc* info);
void REI_ImGui_Shutdown();
void REI_ImGui_Render(ImDrawData* draw_data, REI_Cmd* command_buffer, uint32_t resource_set_index);
// if token is nullptr wait use batch wait
bool REI_ImGui_CreateFontsTexture(REI_RL_State* loader, REI_RL_RequestId* token = nullptr);
