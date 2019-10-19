#pragma once

#include "3rdParty/nanovg/nanovg.h"

#include "REI/Renderer/Renderer.h"

struct REI_NanoVG_Desc
{
    REI_Format      colorFormat;
    REI_Format      depthStencilFormat;
    REI_SampleCount sampleCount;
    uint32_t        resourceSetCount;
    uint32_t        maxVerts;
    uint32_t        maxDraws;
    uint32_t        maxTextures;
};

struct NVGcontext;
struct REI_ResourceLoader;

NVGcontext* REI_NanoVG_Init(REI_Renderer* Renderer, REI_Queue* queue, REI_RL_State* loader, REI_NanoVG_Desc* info);
void        REI_NanoVG_SetupRender(NVGcontext* ctx, REI_Cmd* pCmd, uint32_t set_index);
void        REI_NanoVG_Shutdown(NVGcontext* ctx);
