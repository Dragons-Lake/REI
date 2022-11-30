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

#include "BasicDraw.h"

#ifdef _WIN32
#    define strcpy strcpy_s
#endif

static const uint32_t MAX_SHADER_COUNT = 2;

struct REI_BasicDraw_Uniforms
{
    float    uMVP[16];
    float    uPxScalers[3];
    uint32_t uColor;
};

static_assert(sizeof(REI_BasicDraw_Uniforms)==16*5, "REI_BasicDraw_Uniforms will mismatch shader");

struct PipelineData
{
    REI_RootSignature* rootSignature;
    REI_Pipeline*      pipeline;
};

struct REI_BasicDraw
{
    REI_BasicDraw_Desc        desc;
    REI_Renderer*             renderer;
    REI_AllocatorCallbacks    allocator;
    PipelineData              meshPipelineData;
    PipelineData              pointPipelineData;
    PipelineData              linePipelineData;
    REI_Buffer**              dataBuffers;
    void**                    dataBuffersAddr;
    REI_Buffer**              uniBuffers;
    void**                    uniBuffersAddr;
    REI_Pipeline*             currentPipeline;
    REI_DescriptorTableArray* meshDescriptorSet;
    REI_DescriptorTableArray* lineDescriptorSet;
    REI_DescriptorTableArray* pointUniDescriptorSet;
    REI_DescriptorTableArray* pointDataDescriptorSet;
    uint32_t                  setIndex;
    uint64_t                  dataUsed;
    uint32_t                  uniformDataCount;
};


#include "shaderbin/basicdraw_point_vs.bin.h"

#include "shaderbin/basicdraw_line_vs.bin.h"

#include "shaderbin/basicdraw_mesh_vs.bin.h"

#include "shaderbin/basicdraw_ps.bin.h"

void createPipelineData(
    REI_BasicDraw* state, uint32_t vertexAttribCount, REI_VertexAttrib* vertexAttribs, REI_ShaderDesc* shaderDesc, uint32_t shaderCount, REI_RootSignatureDesc* pRootSigDesc, PipelineData* pipelineData)
{
    REI_Shader* shaders[MAX_SHADER_COUNT] = {};
    REI_addShaders(state->renderer, shaderDesc, shaderCount, shaders);

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

    REI_addRootSignature(state->renderer, pRootSigDesc, &pipelineData->rootSignature);
    REI_Format colorFormat = (REI_Format)state->desc.colorFormat;
    REI_PipelineDesc pipelineDesc = {};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
    REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;
    graphicsDesc.renderTargetCount = 1;
    graphicsDesc.pColorFormats = &colorFormat;
    graphicsDesc.sampleCount = state->desc.sampleCount;
    graphicsDesc.depthStencilFormat = state->desc.depthStencilFormat;
    graphicsDesc.pRootSignature = pipelineData->rootSignature;
    graphicsDesc.ppShaderPrograms = shaders;
    graphicsDesc.shaderProgramCount = shaderCount;
    graphicsDesc.pVertexAttribs = vertexAttribs;
    graphicsDesc.vertexAttribCount = vertexAttribCount;
    graphicsDesc.pRasterizerState = &rasterizerStateDesc;
    graphicsDesc.pBlendState = &blendState;
    REI_addPipeline(state->renderer, &pipelineDesc, &pipelineData->pipeline);

    REI_removeShaders(state->renderer, shaderCount, shaders);
}

void destroyPipelineData(REI_BasicDraw* state, PipelineData* pipelineData)
{
    if (pipelineData->rootSignature)
    {
        REI_removeRootSignature(state->renderer, pipelineData->rootSignature);
        pipelineData->rootSignature = NULL;
    }
    if (pipelineData->pipeline)
    {
        REI_removePipeline(state->renderer, pipelineData->pipeline);
        pipelineData->pipeline = NULL;
    }
}

bool allocateData(REI_BasicDraw* state, uint32_t count, uint32_t size, uint64_t* offset)
{
    uint64_t allocSize = (uint64_t)size * count;
    if (state->desc.maxDataSize - state->dataUsed < allocSize + size)
        return false;

    uint64_t rem = state->dataUsed % size;
    *offset = state->dataUsed + (rem ? size - rem : 0);
    state->dataUsed = *offset + allocSize;
    return true;
}

void REI_RegisterPointBufferSet(
    REI_BasicDraw* state, uint32_t idx, REI_Buffer* interleaved, REI_Buffer* positions, REI_Buffer* colors)
{
    if (idx >= state->desc.maxBufferSets)
        return;

    if (!interleaved || !positions)
        return;

    uint32_t           numDescrUpdates = 1;
    REI_DescriptorData descrUpdate[2]{};
    if (interleaved)
    {
        descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER_RAW;
        descrUpdate[0].descriptorIndex = 0;    //uStream0
        descrUpdate[0].count = 1;
        descrUpdate[0].ppBuffers = &interleaved;
        descrUpdate[0].tableIndex = idx + state->desc.resourceSetCount;
    }
    else
    {
        descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER_RAW;
        descrUpdate[0].descriptorIndex = 0;    //uStream0
        descrUpdate[0].count = 1;
        descrUpdate[0].ppBuffers = &positions;
        descrUpdate[0].tableIndex = idx + state->desc.resourceSetCount;

        if (colors)
        {
            descrUpdate[1].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
            descrUpdate[1].descriptorIndex = 1;    //uStream1
            descrUpdate[1].count = 1;
            descrUpdate[1].ppBuffers = &colors;
            descrUpdate[1].tableIndex = idx + state->desc.resourceSetCount;
            ++numDescrUpdates;
        }
    }

    REI_updateDescriptorTableArray(state->renderer, state->pointDataDescriptorSet, numDescrUpdates, descrUpdate);
}

REI_BasicDraw* REI_BasicDraw_Init(REI_Renderer* renderer, REI_BasicDraw_Desc* info)
{
    REI_AllocatorCallbacks allocatorCallbacks;
    REI_setupAllocatorCallbacks(info->pAllocator, allocatorCallbacks);

    REI_BasicDraw* state =
        (REI_BasicDraw*)allocatorCallbacks.pMalloc(allocatorCallbacks.pUserData, sizeof(REI_BasicDraw), 0);
    if (!state)
        return NULL;

    memset(state, 0, sizeof(REI_BasicDraw));
    state->desc = *info;
    state->renderer = renderer;
    state->allocator = allocatorCallbacks;

    const REI_AllocatorCallbacks& allocator = state->allocator;

    const size_t     vertexAttribCount = 2;
    REI_VertexAttrib vertexAttribs[vertexAttribCount] = {};
    vertexAttribs[0].semantic = REI_SEMANTIC_POSITION0;
    vertexAttribs[0].offset = REI_OFFSETOF(REI_BasicDraw_V_P3C, p);
    vertexAttribs[0].location = 0;
    vertexAttribs[0].format = REI_FMT_R32G32B32_SFLOAT;
    vertexAttribs[1].semantic = REI_SEMANTIC_COLOR0;
    vertexAttribs[1].offset = REI_OFFSETOF(REI_BasicDraw_V_P3C, c);
    vertexAttribs[1].location = 1;
    vertexAttribs[1].format = REI_FMT_R8G8B8A8_UNORM;

    REI_ShaderDesc meshShaderDesc[MAX_SHADER_COUNT] = {
        { REI_SHADER_STAGE_VERT, (uint8_t*)basicdraw_mesh_vs_bytecode, sizeof(basicdraw_mesh_vs_bytecode) },
        { REI_SHADER_STAGE_FRAG, (uint8_t*)basicdraw_ps_bytecode, sizeof(basicdraw_ps_bytecode) }
    };

    {
        REI_DescriptorBinding binding = {};
        binding.descriptorCount = 1;
        binding.descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
        binding.reg = 0;
        binding.binding = 0;

        REI_DescriptorTableLayout setLayout = {};
        setLayout.bindingCount = 1;
        setLayout.pBindings = &binding;
        setLayout.slot = REI_DESCRIPTOR_TABLE_SLOT_0;
        setLayout.stageFlags = REI_SHADER_STAGE_VERT;

        REI_PushConstantRange pRange = {};
        pRange.size = sizeof(uint32_t);
        pRange.offset = 0;
        pRange.stageFlags = REI_SHADER_STAGE_VERT;

        REI_RootSignatureDesc rootDesc = {};
        rootDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
        rootDesc.tableLayoutCount = 1;
        rootDesc.pTableLayouts = &setLayout;
        rootDesc.pushConstantRangeCount = 1;
        rootDesc.pPushConstantRanges = &pRange;

        createPipelineData(state, vertexAttribCount, vertexAttribs, meshShaderDesc, MAX_SHADER_COUNT, &rootDesc, &state->meshPipelineData);
    }

    REI_ShaderDesc pointShaderDesc[MAX_SHADER_COUNT] = {
        { REI_SHADER_STAGE_VERT, (uint8_t*)basicdraw_point_vs_bytecode, sizeof(basicdraw_point_vs_bytecode) },
        { REI_SHADER_STAGE_FRAG, (uint8_t*)basicdraw_ps_bytecode, sizeof(basicdraw_ps_bytecode) }
    };

    {
        REI_DescriptorBinding binding[3] = {};
        binding[0].descriptorCount = 1;
        binding[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
        binding[0].reg = 0;
        binding[0].binding = 0;

        binding[1].descriptorCount = 1;
        binding[1].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER_RAW;
        binding[1].reg = 0;
        binding[1].binding = 0;

        binding[2].descriptorCount = 1;
        binding[2].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
        binding[2].reg = 1;
        binding[2].binding = 1;

        REI_DescriptorTableLayout setLayout[2] = {};
        setLayout[0].bindingCount = 1;
        setLayout[0].pBindings = binding;
        setLayout[0].slot = REI_DESCRIPTOR_TABLE_SLOT_0;
        setLayout[0].stageFlags = REI_SHADER_STAGE_VERT;

        setLayout[1].bindingCount = 2;
        setLayout[1].pBindings = binding + 1;
        setLayout[1].slot = REI_DESCRIPTOR_TABLE_SLOT_1;
        setLayout[1].stageFlags = REI_SHADER_STAGE_VERT;

        REI_PushConstantRange pRange = {};
        pRange.size = sizeof(uint32_t[3]);
        pRange.offset = 0;
        pRange.stageFlags = REI_SHADER_STAGE_VERT;

        REI_RootSignatureDesc rootDesc = {};
        rootDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
        rootDesc.tableLayoutCount = 2;
        rootDesc.pTableLayouts = setLayout;
        rootDesc.pushConstantRangeCount = 1;
        rootDesc.pPushConstantRanges = &pRange;

        createPipelineData(state, 0, nullptr, pointShaderDesc, MAX_SHADER_COUNT, &rootDesc, &state->pointPipelineData);
    }

    REI_ShaderDesc lineShaderDesc[MAX_SHADER_COUNT] = {
        { REI_SHADER_STAGE_VERT, (uint8_t*)basicdraw_line_vs_bytecode, sizeof(basicdraw_line_vs_bytecode) },
        { REI_SHADER_STAGE_FRAG, (uint8_t*)basicdraw_ps_bytecode, sizeof(basicdraw_ps_bytecode) }
    };

    {
        REI_DescriptorBinding binding[2] = {};
        binding[0].descriptorCount = 1;
        binding[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
        binding[0].reg = 0;
        binding[0].binding = 0;

        binding[1].descriptorCount = 1;
        binding[1].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER_RAW;
        binding[1].reg = 1;
        binding[1].binding = 1;

        REI_DescriptorTableLayout setLayout = {};
        setLayout.bindingCount = 2;
        setLayout.pBindings = binding;
        setLayout.slot = REI_DESCRIPTOR_TABLE_SLOT_0;
        setLayout.stageFlags = REI_SHADER_STAGE_VERT;

        REI_PushConstantRange pRange = {};
        pRange.size = sizeof(uint32_t[2]);
        pRange.offset = 0;
        pRange.stageFlags = REI_SHADER_STAGE_VERT;

        REI_RootSignatureDesc rootDesc = {};
        rootDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
        rootDesc.tableLayoutCount = 1;
        rootDesc.pTableLayouts = &setLayout;
        rootDesc.pushConstantRangeCount = 1;
        rootDesc.pPushConstantRanges = &pRange;

        createPipelineData(state, 0, nullptr, lineShaderDesc, MAX_SHADER_COUNT, &rootDesc, &state->linePipelineData);
    }



    REI_BufferDesc transformBufDesc = {};
    transformBufDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER;
    transformBufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    transformBufDesc.size = state->desc.maxDrawCount * sizeof(REI_BasicDraw_Uniforms);
    transformBufDesc.elementCount = state->desc.maxDrawCount;
    transformBufDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    state->uniBuffers = (REI_Buffer**)REI_calloc(allocator, state->desc.resourceSetCount * sizeof(REI_Buffer*));
    state->uniBuffersAddr = (void**)REI_calloc(allocator, state->desc.resourceSetCount * sizeof(void*));
    transformBufDesc.structStride = sizeof(REI_BasicDraw_Uniforms);
    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &transformBufDesc, &state->uniBuffers[i]);
        REI_mapBuffer(state->renderer, state->uniBuffers[i], &state->uniBuffersAddr[i]);
    }

    REI_DescriptorTableArrayDesc meshDescriptorSetDesc = {};
    meshDescriptorSetDesc.pRootSignature = state->meshPipelineData.rootSignature;
    meshDescriptorSetDesc.maxTables = state->desc.resourceSetCount;
    meshDescriptorSetDesc.slot = REI_DESCRIPTOR_TABLE_SLOT_0;
    REI_addDescriptorTableArray(state->renderer, &meshDescriptorSetDesc, &state->meshDescriptorSet);

    REI_DescriptorTableArrayDesc pointDataDescriptorSetDesc = {};
    pointDataDescriptorSetDesc.pRootSignature = state->pointPipelineData.rootSignature;
    pointDataDescriptorSetDesc.maxTables = state->desc.resourceSetCount + state->desc.maxBufferSets;
    pointDataDescriptorSetDesc.slot = REI_DESCRIPTOR_TABLE_SLOT_1;
    REI_addDescriptorTableArray(state->renderer, &pointDataDescriptorSetDesc, &state->pointDataDescriptorSet);

    REI_BufferDesc dataBufDesc = {};
    dataBufDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER_RAW | REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
    dataBufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    dataBufDesc.size = state->desc.maxDataSize;
    dataBufDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    dataBufDesc.format = REI_FMT_R32_UINT;
    dataBufDesc.vertexStride = 16;
    dataBufDesc.elementCount = dataBufDesc.size / dataBufDesc.vertexStride;
    state->dataBuffers = (REI_Buffer**)REI_calloc(allocator, state->desc.resourceSetCount * sizeof(REI_Buffer*));
    state->dataBuffersAddr = (void**)REI_calloc(allocator, state->desc.resourceSetCount * sizeof(void*));

    REI_DescriptorData descrUpdate[2] = {};
    descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER_RAW;
    descrUpdate[0].descriptorIndex = 0;
    descrUpdate[0].count = 1;

    descrUpdate[1].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
    descrUpdate[1].count = 1;
    descrUpdate[1].descriptorIndex = 1;
    
    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &dataBufDesc, &state->dataBuffers[i]);
        REI_mapBuffer(state->renderer, state->dataBuffers[i], &state->dataBuffersAddr[i]);
        descrUpdate[0].ppBuffers = &state->dataBuffers[i];
        descrUpdate[0].tableIndex = i;
        descrUpdate[1].ppBuffers = &state->dataBuffers[i];
        descrUpdate[1].tableIndex = i;

        REI_updateDescriptorTableArray(state->renderer, state->pointDataDescriptorSet, 2, descrUpdate);
    }

    descrUpdate[0].descriptorIndex = 0;
    descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        descrUpdate[0].ppBuffers = &state->uniBuffers[i];
        descrUpdate[0].tableIndex = i;
        REI_updateDescriptorTableArray(state->renderer, state->meshDescriptorSet, 1, descrUpdate);
    }

    REI_DescriptorTableArrayDesc pointUniDescriptorSetDesc = {};
    pointUniDescriptorSetDesc.pRootSignature = state->pointPipelineData.rootSignature;
    pointUniDescriptorSetDesc.maxTables = state->desc.resourceSetCount;
    pointUniDescriptorSetDesc.slot = REI_DESCRIPTOR_TABLE_SLOT_0;
    REI_addDescriptorTableArray(state->renderer, &pointUniDescriptorSetDesc, &state->pointUniDescriptorSet);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        descrUpdate[0].ppBuffers = &state->uniBuffers[i];
        descrUpdate[0].tableIndex = i;
        REI_updateDescriptorTableArray(state->renderer, state->pointUniDescriptorSet, 1, descrUpdate);
    }

    REI_DescriptorTableArrayDesc lineDescriptorSetDesc = {};
    lineDescriptorSetDesc.pRootSignature = state->linePipelineData.rootSignature;
    lineDescriptorSetDesc.maxTables = state->desc.resourceSetCount;
    lineDescriptorSetDesc.slot = REI_DESCRIPTOR_TABLE_SLOT_0;
    REI_addDescriptorTableArray(state->renderer, &lineDescriptorSetDesc, &state->lineDescriptorSet);

    descrUpdate[1].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER_RAW;
    descrUpdate[1].descriptorIndex = 1;
    descrUpdate[1].count = 1;

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        descrUpdate[0].ppBuffers = &state->uniBuffers[i];
        descrUpdate[0].tableIndex = i;
        descrUpdate[1].ppBuffers = &state->dataBuffers[i];
        descrUpdate[1].tableIndex = i;
        REI_updateDescriptorTableArray(state->renderer, state->lineDescriptorSet, 2, descrUpdate);
    }

    return state;
}

void REI_BasicDraw_SetupRender(REI_BasicDraw* state, uint32_t set_index)
{
    state->dataUsed = 0;
    state->uniformDataCount = 0;
    state->setIndex = set_index;
    state->currentPipeline = NULL;
}

void REI_BasicDraw_RenderMesh(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], uint32_t count, REI_BasicDraw_V_P3C** verts)
{
    if (state->uniformDataCount >= state->desc.maxDrawCount)
        return;

    uint64_t offset;
    if (!allocateData(state, count, sizeof(REI_BasicDraw_V_P3C), &offset))
        return;
    uint8_t* ptr = ((uint8_t*)state->dataBuffersAddr[state->setIndex]) + offset;
    *verts = (REI_BasicDraw_V_P3C*)ptr;

    if (state->currentPipeline != state->meshPipelineData.pipeline)
    {
        state->currentPipeline = state->meshPipelineData.pipeline;
        // Bind pipeline and descriptor sets:
        REI_cmdBindPipeline(pCmd, state->meshPipelineData.pipeline);
        uint64_t vertex_offset = 0;
        REI_cmdBindVertexBuffer(pCmd, 1, &state->dataBuffers[state->setIndex], &vertex_offset);
        REI_cmdBindDescriptorTable(pCmd, state->setIndex, state->meshDescriptorSet);
        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
    }

    REI_cmdBindPushConstants(
        pCmd, state->meshPipelineData.rootSignature, REI_SHADER_STAGE_VERT, 0, sizeof(uint32_t), &state->uniformDataCount);
    REI_cmdDraw(pCmd, count, (uint32_t)(offset) / sizeof(REI_BasicDraw_V_P3C));

    REI_BasicDraw_Uniforms* uniform_ptr =
        ((REI_BasicDraw_Uniforms*)state->uniBuffersAddr[state->setIndex]) + state->uniformDataCount;

    memcpy(&uniform_ptr->uMVP, mvp, sizeof(uniform_ptr->uMVP));
    uniform_ptr->uPxScalers[0] = 0.0f;
    uniform_ptr->uPxScalers[1] = 0.0f;
    uniform_ptr->uPxScalers[2] = 0.0f;
    uniform_ptr->uColor = 0;
    ++state->uniformDataCount;
}

void REI_BasicDraw_RenderPoints(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], float ptsize, uint32_t type, uint32_t color, uint32_t bufferSet,
    uint32_t pointCount, uint32_t firstPoint)
{
    if (state->currentPipeline != state->pointPipelineData.pipeline)
    {
        state->currentPipeline = state->pointPipelineData.pipeline;
        REI_cmdBindPipeline(pCmd, state->pointPipelineData.pipeline);
        REI_cmdBindDescriptorTable(pCmd, state->setIndex, state->pointUniDescriptorSet);
        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
    }

    REI_cmdBindDescriptorTable(pCmd, bufferSet, state->pointDataDescriptorSet);    // TODO: cache
    uint32_t baseVertex = 6 * firstPoint;
    uint32_t pushConstants[3]{ state->uniformDataCount, type, baseVertex };
    REI_cmdBindPushConstants(
        pCmd, state->pointPipelineData.rootSignature, REI_SHADER_STAGE_VERT, 0, sizeof(pushConstants), pushConstants);
    REI_cmdDraw(pCmd, 6 * pointCount, baseVertex);

    REI_BasicDraw_Uniforms* uniform_ptr =
        ((REI_BasicDraw_Uniforms*)state->uniBuffersAddr[state->setIndex]) + state->uniformDataCount;
    memcpy(&uniform_ptr->uMVP, mvp, sizeof(float) * 16);
    uniform_ptr->uPxScalers[0] = ptsize / (float)state->desc.fbWidth;
    uniform_ptr->uPxScalers[1] = ptsize / (float)state->desc.fbHeight;
    uniform_ptr->uPxScalers[2] = 0.0f;
    uniform_ptr->uColor = color;
    ++state->uniformDataCount;
}

void REI_BasicDraw_RenderPoints(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], float ptsize, uint32_t count, REI_BasicDraw_V_P3C** point_data)
{
    if (state->uniformDataCount >= state->desc.maxDrawCount)
        return;

    uint64_t offset;
    if (!allocateData(state, count, sizeof(REI_BasicDraw_V_P3C), &offset))
        return;
    *point_data = (REI_BasicDraw_V_P3C*)(((uint8_t*)state->dataBuffersAddr[state->setIndex]) + offset);

    REI_BasicDraw_RenderPoints(
        state, pCmd, mvp, ptsize, 0, 0, state->setIndex, count, (uint32_t)(offset / sizeof(REI_BasicDraw_V_P3C)));
}

void REI_BasicDraw_RenderPoints(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], float ptsize, uint32_t count, REI_BasicDraw_V_P3** positions,
    uint32_t color)
{
    if (state->uniformDataCount >= state->desc.maxDrawCount)
        return;

    uint64_t offset;
    if (!allocateData(state, count, sizeof(REI_BasicDraw_V_P3), &offset))
        return;
    *positions = (REI_BasicDraw_V_P3*)(((uint8_t*)state->dataBuffersAddr[state->setIndex]) + offset);

    REI_BasicDraw_RenderPoints(
        state, pCmd, mvp, ptsize, 2, color, state->setIndex, count, (uint32_t)(offset / sizeof(REI_BasicDraw_V_P3)));
}

void REI_BasicDraw_RenderLines(
    REI_BasicDraw* state, REI_Cmd* pCmd, float const mvp[16], uint32_t count, REI_BasicDraw_Line** line_data)
{
    if (state->uniformDataCount >= state->desc.maxDrawCount)
        return;

    uint64_t offset;
    if (!allocateData(state, count, sizeof(REI_BasicDraw_Line), &offset))
        return;
    uint8_t* ptr = ((uint8_t*)state->dataBuffersAddr[state->setIndex]) + offset;
    *line_data = (REI_BasicDraw_Line*)ptr;

    if (state->currentPipeline != state->linePipelineData.pipeline)
    {
        state->currentPipeline = state->linePipelineData.pipeline;
        REI_cmdBindPipeline(pCmd, state->linePipelineData.pipeline);
        REI_cmdBindDescriptorTable(pCmd, state->setIndex, state->lineDescriptorSet);
        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
    }
    uint32_t baseVertex = 6 * (uint32_t)(offset / sizeof(REI_BasicDraw_Line));
    uint32_t pushConstants[2]{ state->uniformDataCount, baseVertex };
    REI_cmdBindPushConstants(
        pCmd, state->linePipelineData.rootSignature, REI_SHADER_STAGE_VERT, 0, sizeof(pushConstants), pushConstants);
    REI_cmdDraw(pCmd, 6 * count, baseVertex);

    REI_BasicDraw_Uniforms* uniforms_ptr =
        ((REI_BasicDraw_Uniforms*)state->uniBuffersAddr[state->setIndex]) + state->uniformDataCount;

    memcpy(&uniforms_ptr->uMVP, mvp, sizeof(float) * 16);
    uniforms_ptr->uPxScalers[0] = 1.0f / (float)state->desc.fbWidth;
    uniforms_ptr->uPxScalers[1] = 1.0f / (float)state->desc.fbHeight;
    uniforms_ptr->uPxScalers[2] = (float)state->desc.fbWidth / (float)state->desc.fbHeight;
    uniforms_ptr->uColor = 0;
    ++state->uniformDataCount;
}

void REI_BasicDraw_Shutdown(REI_BasicDraw* state)
{
    REI_removeDescriptorTableArray(state->renderer, state->meshDescriptorSet);
    REI_removeDescriptorTableArray(state->renderer, state->pointUniDescriptorSet);
    REI_removeDescriptorTableArray(state->renderer, state->pointDataDescriptorSet);
    REI_removeDescriptorTableArray(state->renderer, state->lineDescriptorSet);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->dataBuffers[i]);
    }
    state->allocator.pFree(state->allocator.pUserData, state->dataBuffers);
    state->allocator.pFree(state->allocator.pUserData, state->dataBuffersAddr);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->uniBuffers[i]);
    }
    state->allocator.pFree(state->allocator.pUserData, state->uniBuffers);
    state->allocator.pFree(state->allocator.pUserData, state->uniBuffersAddr);

    destroyPipelineData(state, &state->meshPipelineData);
    destroyPipelineData(state, &state->pointPipelineData);
    destroyPipelineData(state, &state->linePipelineData);

    state->allocator.pFree(state->allocator.pUserData, state);
}
