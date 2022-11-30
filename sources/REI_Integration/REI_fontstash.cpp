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
#ifdef _WIN32
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN 1
#    endif
#    include "windows.h"
#endif
#define FONTSTASH_IMPLEMENTATION

#include "REI_fontstash.h"

#ifdef _WIN32
#    define strcpy strcpy_s
#endif

static const uint32_t MAX_SHADER_COUNT = 2;

struct REI_Fontstash_State
{
    REI_Fontstash_Desc        desc;
    REI_Renderer*             renderer;
    REI_AllocatorCallbacks    allocator;
    REI_Queue*                queue;
    REI_RL_State*             loader;
    REI_RootSignature*        rootSignature;
    REI_DescriptorTableArray* descriptorSet;
    REI_Pipeline*             pipeline;
    REI_Sampler*              fontSampler;
    REI_Texture*              fontTexture;
    REI_Buffer**              buffers;
    void**                    buffersAddr;
    uint32_t                  setIndex;
    uint32_t                  vertexCount;
    uint32_t                  fontTextureWidth;
};


#include "shaderbin/fontstash_vs.bin.h"

#include "shaderbin/fontstash_ps.bin.h"

struct FontVert
{
    float    pos[2];
    float    uv[2];
    uint32_t col;
};

#define OFFSETOF(type, mem) ((size_t)(&(((type*)0)->mem)))

static int REI_Fontstash_renderResize(void* userPtr, int width, int height)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)userPtr;
    REI_waitQueueIdle(state->queue);

    if (state->fontTexture)
        REI_removeTexture(state->renderer, state->fontTexture);

    REI_TextureDesc textureDesc{};
    textureDesc.flags = REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
    textureDesc.width = (uint32_t)width;
    textureDesc.height = (uint32_t)height;
    textureDesc.format = REI_FMT_R8_UNORM;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(state->renderer, &textureDesc, &state->fontTexture);

    state->fontTextureWidth = width;

    REI_DescriptorData params[1] = {};
    params[0].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
    params[0].descriptorIndex = 0;    //uTexture;
    params[0].ppTextures = &state->fontTexture;
    params[0].count = 1;
    params[0].tableIndex = 0;
    REI_updateDescriptorTableArray(state->renderer, state->descriptorSet, 1, params);

    return 1;
}

static int REI_Fontstash_renderCreate(void* userPtr, int width, int height)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)userPtr;
    REI_ShaderDesc       shaderDesc[MAX_SHADER_COUNT] = {
        { REI_SHADER_STAGE_VERT, (uint8_t*)fontstash_vs_bytecode, sizeof(fontstash_vs_bytecode) },
        { REI_SHADER_STAGE_FRAG, (uint8_t*)fontstash_ps_bytecode, sizeof(fontstash_ps_bytecode) }
    };
    REI_Shader* shaders[MAX_SHADER_COUNT] = {};
    REI_addShaders(state->renderer, shaderDesc, MAX_SHADER_COUNT, shaders);

    REI_SamplerDesc samplerDesc = {
        REI_FILTER_LINEAR,
        REI_FILTER_LINEAR,
        REI_MIPMAP_MODE_LINEAR,
        REI_ADDRESS_MODE_CLAMP_TO_EDGE,
        REI_ADDRESS_MODE_CLAMP_TO_EDGE,
        REI_ADDRESS_MODE_CLAMP_TO_EDGE,
        REI_CMP_NEVER,
        0.0f,
        1.0f,
    };
    REI_addSampler(state->renderer, &samplerDesc, &state->fontSampler);

    REI_RootSignatureDesc rootSigDesc = {};

    REI_PushConstantRange pConst = {};
    pConst.offset = 0;
    pConst.size = sizeof(float[4]);
    pConst.stageFlags = REI_SHADER_STAGE_VERT;

    REI_DescriptorBinding binding = {};

    binding.descriptorCount = 1;
    binding.descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
    binding.reg = 0;
    binding.binding = 0;

    REI_DescriptorTableLayout setLayout = {};
    setLayout.bindingCount = 1;
    setLayout.pBindings = &binding;
    setLayout.slot = REI_DESCRIPTOR_TABLE_SLOT_1;
    setLayout.stageFlags = REI_SHADER_STAGE_FRAG;

    REI_StaticSamplerBinding staticSamplerBinding = {};
    staticSamplerBinding.descriptorCount = 1;
    staticSamplerBinding.reg = 0;
    staticSamplerBinding.binding = 0;
    staticSamplerBinding.ppStaticSamplers = &state->fontSampler;

    rootSigDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
    rootSigDesc.pushConstantRangeCount = 1;
    rootSigDesc.pPushConstantRanges = &pConst;
    rootSigDesc.tableLayoutCount = 1;
    rootSigDesc.pTableLayouts = &setLayout;
    rootSigDesc.staticSamplerBindingCount = 1;
    rootSigDesc.staticSamplerSlot = REI_DESCRIPTOR_TABLE_SLOT_0;
    rootSigDesc.staticSamplerStageFlags = REI_SHADER_STAGE_FRAG;
    rootSigDesc.pStaticSamplerBindings = &staticSamplerBinding;

    REI_addRootSignature(state->renderer, &rootSigDesc, &state->rootSignature);

    REI_DescriptorTableArrayDesc descriptorSetDesc = {};
    descriptorSetDesc.pRootSignature = state->rootSignature;
    descriptorSetDesc.maxTables = 1;
    descriptorSetDesc.slot = REI_DESCRIPTOR_TABLE_SLOT_1;
    REI_addDescriptorTableArray(state->renderer, &descriptorSetDesc, &state->descriptorSet);

    const size_t     vertexAttribCount = 3;
    REI_VertexAttrib vertexAttribs[vertexAttribCount] = {};
    vertexAttribs[0].semantic = REI_SEMANTIC_POSITION0;
    vertexAttribs[0].offset = OFFSETOF(FontVert, pos);
    vertexAttribs[0].location = 0;
    vertexAttribs[0].format = REI_FMT_R32G32_SFLOAT;
    vertexAttribs[1].semantic = REI_SEMANTIC_TEXCOORD0;
    vertexAttribs[1].offset = OFFSETOF(FontVert, uv);
    vertexAttribs[1].location = 1;
    vertexAttribs[1].format = REI_FMT_R32G32_SFLOAT;
    vertexAttribs[2].semantic = REI_SEMANTIC_COLOR0;
    vertexAttribs[2].offset = OFFSETOF(FontVert, col);
    vertexAttribs[2].location = 2;
    vertexAttribs[2].format = REI_FMT_R8G8B8A8_UNORM;

    REI_RasterizerStateDesc rasterizerStateDesc{};
    rasterizerStateDesc.cullMode = REI_CULL_MODE_NONE;

    REI_BlendStateDesc blendState{};
    blendState.renderTargetMask = REI_BLEND_STATE_TARGET_0;
    blendState.srcFactors[0] = REI_BC_SRC_ALPHA;
    blendState.dstFactors[0] = REI_BC_ONE_MINUS_SRC_ALPHA;
    blendState.blendModes[0] = REI_BM_ADD;
    blendState.srcAlphaFactors[0] = REI_BC_SRC_ALPHA;
    blendState.dstAlphaFactors[0] = REI_BC_ONE_MINUS_SRC_ALPHA;
    blendState.blendAlphaModes[0] = REI_BM_ADD;
    blendState.masks[0] = REI_COLOR_MASK_ALL;

    REI_Format       colorFormat = (REI_Format)state->desc.colorFormat;
    REI_PipelineDesc pipelineDesc = {};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
    REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;
    graphicsDesc.renderTargetCount = 1;
    graphicsDesc.pColorFormats = &colorFormat;
    graphicsDesc.sampleCount = state->desc.sampleCount;
    graphicsDesc.depthStencilFormat = state->desc.depthStencilFormat;
    graphicsDesc.pRootSignature = state->rootSignature;
    graphicsDesc.ppShaderPrograms = shaders;
    graphicsDesc.shaderProgramCount = MAX_SHADER_COUNT;
    graphicsDesc.pVertexAttribs = vertexAttribs;
    graphicsDesc.vertexAttribCount = vertexAttribCount;
    graphicsDesc.pRasterizerState = &rasterizerStateDesc;
    graphicsDesc.pBlendState = &blendState;
    REI_addPipeline(state->renderer, &pipelineDesc, &state->pipeline);

    REI_removeShaders(state->renderer, MAX_SHADER_COUNT, shaders);

    REI_BufferDesc vbDesc = {};
    vbDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER | REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.vertexStride = sizeof(FontVert);
    vbDesc.structStride = sizeof(FontVert);
    vbDesc.size = state->desc.vertexBufferSize;
    vbDesc.elementCount = vbDesc.size / vbDesc.structStride;
    vbDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

    state->buffers = (REI_Buffer**)REI_calloc(state->allocator, state->desc.resourceSetCount * sizeof(REI_Buffer*));
    state->buffersAddr = (void**)REI_calloc(state->allocator, state->desc.resourceSetCount * sizeof(void*));
    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &vbDesc, &state->buffers[i]);
        REI_mapBuffer(state->renderer, state->buffers[i], &state->buffersAddr[i]);
    }

    REI_Fontstash_renderResize(userPtr, width, height);

    return 1;
}

static void REI_Fontstash_renderUpdate(void* userPtr, int* rect, const unsigned char* data)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)userPtr;

    uint32_t                 width = state->fontTextureWidth;
    REI_RL_TextureUpdateDesc updateDesc = { state->fontTexture,
                                            (uint8_t*)data + rect[1] * width,
                                            REI_FMT_R8_UNORM,
                                            0,
                                            (uint32_t)rect[1],
                                            0,
                                            width,
                                            (uint32_t)(rect[3] - rect[1]),
                                            1,
                                            0,
                                            0,
                                            REI_RESOURCE_STATE_SHADER_RESOURCE };
    REI_RL_updateResource(state->loader, &updateDesc);
}

static void REI_Fontstash_renderDraw(
    void* userPtr, const float* verts, const float* tcoords, const unsigned int* colors, int nverts)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)userPtr;

    FontVert* vptr = ((FontVert*)state->buffersAddr[state->setIndex]) + state->vertexCount;
    for (int i = 0; i < nverts; ++i)
    {
        vptr[i].pos[0] = verts[i * 2];
        vptr[i].pos[1] = verts[i * 2 + 1];
        vptr[i].uv[0] = tcoords[i * 2];
        vptr[i].uv[1] = tcoords[i * 2 + 1];
        vptr[i].col = colors[i];
    }
    state->vertexCount += nverts;
}

static void REI_Fontstash_renderDelete(void* userPtr)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)userPtr;

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->buffers[i]);
    }
    state->allocator.pFree(state->allocator.pUserData, state->buffers);
    state->allocator.pFree(state->allocator.pUserData, state->buffersAddr);

    if (state->fontTexture)
    {
        REI_removeTexture(state->renderer, state->fontTexture);
        state->fontTexture = NULL;
    }
    if (state->fontSampler)
    {
        REI_removeSampler(state->renderer, state->fontSampler);
        state->fontSampler = NULL;
    }
    if (state->rootSignature)
    {
        REI_removeRootSignature(state->renderer, state->rootSignature);
        state->rootSignature = NULL;
    }
    if (state->pipeline)
    {
        REI_removePipeline(state->renderer, state->pipeline);
        state->pipeline = NULL;
    }
    if (state->descriptorSet)
    {
        REI_removeDescriptorTableArray(state->renderer, state->descriptorSet);
        state->descriptorSet = NULL;
    }

    state->allocator.pFree(state->allocator.pUserData, state);
}

FONScontext*
    REI_Fontstash_Init(REI_Renderer* renderer, REI_Queue* queue, REI_RL_State* loader, REI_Fontstash_Desc* info)
{
    REI_AllocatorCallbacks allocatorCallbacks;
    REI_setupAllocatorCallbacks(info->pAllocator, allocatorCallbacks);

    REI_Fontstash_State* state;

    state =
        (REI_Fontstash_State*)allocatorCallbacks.pMalloc(allocatorCallbacks.pUserData, sizeof(REI_Fontstash_State), 0);
    if (!state)
        return NULL;

    memset(state, 0, sizeof(REI_Fontstash_State));
    state->desc = *info;
    state->renderer = renderer;
    state->allocator = allocatorCallbacks;
    state->queue = queue;
    state->loader = loader;

    FONSparams params;
    memset(&params, 0, sizeof(params));
    params.width = info->texWidth;
    params.height = info->texHeight;
    params.flags = FONS_ZERO_TOPLEFT;
    params.renderCreate = REI_Fontstash_renderCreate;
    params.renderResize = REI_Fontstash_renderResize;
    params.renderUpdate = REI_Fontstash_renderUpdate;
    params.renderDraw = REI_Fontstash_renderDraw;
    params.renderDelete = REI_Fontstash_renderDelete;
    params.userPtr = state;

    return fonsCreateInternal(&params);
}

void REI_Fontstash_SetupRender(FONScontext* ctx, uint32_t set_index)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)ctx->params.userPtr;
    state->vertexCount = 0;
    state->setIndex = set_index;
}

void REI_Fontstash_Render(FONScontext* ctx, REI_Cmd* pCmd)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)ctx->params.userPtr;

    // Bind pipeline and descriptor sets:
    {
        REI_cmdBindPipeline(pCmd, state->pipeline);
        REI_cmdBindDescriptorTable(pCmd, 0, state->descriptorSet);
    }

    // Bind Vertex And Index Buffer:
    {
        uint64_t vertex_offset = 0;
        REI_cmdBindVertexBuffer(pCmd, 1, &state->buffers[state->setIndex], &vertex_offset);
    }

    // Setup viewport:
    {
        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
    }

    // Setup scale and translation:
    {
        float scaleTranslate[4];
        scaleTranslate[0] = 2.0f / state->desc.fbWidth;
        scaleTranslate[1] = -2.0f / state->desc.fbHeight;
        scaleTranslate[2] = -1.0f;
        scaleTranslate[3] = 1.0f;
        REI_cmdBindPushConstants(
            pCmd, state->rootSignature, REI_SHADER_STAGE_VERT, 0, sizeof(scaleTranslate), scaleTranslate);
    }

    // Draw
    REI_cmdDraw(pCmd, state->vertexCount, 0);
}

void REI_Fontstash_Shutdown(FONScontext* ctx) { fonsDeleteInternal(ctx); }
