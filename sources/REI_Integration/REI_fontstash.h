#pragma once

#include "3rdParty/fontstash/fontstash.h"

#include "REI/Renderer/Renderer.h"

#include "ResourceLoader.h"

struct REI_Fontstash_Desc
{
    uint64_t        vertexBufferSize;
    REI_Format      colorFormat;
    REI_Format      depthStencilFormat;
    REI_SampleCount sampleCount;
    uint32_t        resourceSetCount;
    uint32_t        fbWidth;
    uint32_t        fbHeight;
    uint32_t        texWidth;
    uint32_t        texHeight;
};

struct FONScontext;

FONScontext*
     REI_Fontstash_Init(REI_Renderer* Renderer, REI_Queue* queue, REI_RL_State* loader, REI_Fontstash_Desc* info);
void REI_Fontstash_SetupRender(FONScontext* ctx, uint32_t set_index);
void REI_Fontstash_Render(FONScontext* ctx, REI_Cmd* pCmd);
void REI_Fontstash_Shutdown(FONScontext* ctx);
