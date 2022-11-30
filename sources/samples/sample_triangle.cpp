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
#endif

static const uint32_t     MAX_SHADER_COUNT = 2;
static REI_CmdPool*       cmdPool[FRAME_COUNT];
static REI_Cmd*           pCmds[FRAME_COUNT];
static REI_Texture*       depthBuffer;
static REI_RootSignature* rootSignature;
static REI_Pipeline*      pipeline;

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

#include "shaderbin/shader_vs.bin.h"

#include "shaderbin/shader_ps.bin.h"


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

    REI_ShaderDesc basicShader[MAX_SHADER_COUNT] = {
        { REI_SHADER_STAGE_VERT, (uint8_t*)triangle_vs_bytecode, sizeof(triangle_vs_bytecode) },
        { REI_SHADER_STAGE_FRAG, (uint8_t*)triangle_ps_bytecode, sizeof(triangle_ps_bytecode) }
    };
    REI_Shader* shaders[MAX_SHADER_COUNT] = {};
    REI_addShaders(renderer, basicShader, 2, shaders);

    REI_RootSignatureDesc rootDesc{};
    rootDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
    REI_addRootSignature(renderer, &rootDesc, &rootSignature);

    REI_RasterizerStateDesc rasterizerStateDesc{};
    rasterizerStateDesc.cullMode = REI_CULL_MODE_NONE;

    REI_DepthStateDesc depthStateDesc{};
    depthStateDesc.depthTestEnable = true;
    depthStateDesc.depthWriteEnable = true;
    depthStateDesc.depthCmpFunc = REI_CMP_LEQUAL;

    REI_PipelineDesc pipelineDesc{};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;

    REI_Format                colorFormat = (REI_Format)swapchainDesc->colorFormat;
    REI_GraphicsPipelineDesc& pipelineSettings = pipelineDesc.graphicsDesc;
    pipelineSettings.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_STRIP;
    pipelineSettings.renderTargetCount = 1;
    pipelineSettings.pDepthState = &depthStateDesc;
    pipelineSettings.pColorFormats = &colorFormat;
    pipelineSettings.sampleCount = swapchainDesc->sampleCount;
    pipelineSettings.depthStencilFormat = depthRTDesc.format;
    pipelineSettings.pRootSignature = rootSignature;
    pipelineSettings.ppShaderPrograms = shaders;
    pipelineSettings.shaderProgramCount = MAX_SHADER_COUNT;
    pipelineSettings.vertexAttribCount = 0;
    pipelineSettings.pVertexAttribs = nullptr;
    pipelineSettings.pRasterizerState = &rasterizerStateDesc;
    REI_addPipeline(renderer, &pipelineDesc, &pipeline);

    REI_removeShaders(renderer, MAX_SHADER_COUNT, shaders);
}

void sample_on_swapchain_fini()
{
    REI_removeRootSignature(renderer, rootSignature);
    REI_removePipeline(renderer, pipeline);
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
    loadActions.clearColorValues[0].rt.r = 1.0f;
    loadActions.clearColorValues[0].rt.g = 1.0f;
    loadActions.clearColorValues[0].rt.b = 0.0f;
    loadActions.clearColorValues[0].rt.a = 1.0f;
    loadActions.loadActionDepth = REI_LOAD_ACTION_CLEAR;
    loadActions.clearDepth.ds.depth = 1.0f;
    loadActions.clearDepth.ds.stencil = 0;
    REI_cmdBindRenderTargets(cmd, 1, &renderTarget, depthBuffer, &loadActions, NULL, NULL, 0, 0);
    REI_cmdSetViewport(cmd, 0.0f, 0.0f, (float)frameData->bbWidth, (float)frameData->bbHeight, 0.0f, 1.0f);
    REI_cmdSetScissor(cmd, 0, 0, frameData->bbWidth, frameData->bbHeight);

    REI_cmdBindPipeline(cmd, pipeline);
    REI_cmdDraw(cmd, 3, 0);

    sample_cmdPrepareBackbuffer(cmd, renderTarget, REI_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = { depthBuffer, REI_RESOURCE_STATE_DEPTH_WRITE, REI_RESOURCE_STATE_COMMON };
    REI_cmdResourceBarrier(cmd, 0, nullptr, 1, &barriers[1]);
    REI_endCmd(cmd);

    sample_submit(cmd);
}
