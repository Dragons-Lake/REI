/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 */

#pragma once

#include <array>

#include "REI/Common.h"
#include "REI/Renderer.h"
#include "REI_Integration/ResourceLoader.h"
#include "REI_Sample/Log.h"

#define TEST(b)                  \
    if (testSuccess)             \
    {                            \
        if (!(b))                \
        {                        \
            testSuccess = false; \
        }                        \
        REI_ASSERT(b);           \
    }

#include "shaderbin/test_sample_texture_vs.bin.h"
#include "shaderbin/test_sample_texture_ps.bin.h"

struct Graphics
{
    REI_Texture*       colorRT;
    REI_RootSignature* signature;
    REI_Sampler*       sampler;
    REI_Pipeline*      pipeline;
    REI_DescriptorTableArray* descriptors;
};

bool test_copyBuffer(
    REI_Renderer* renderer, REI_RL_State* loader, REI_Queue* queue, REI_Cmd* cmd, REI_CmdPool* cmdPool,
    REI_Fence* fence)
{
    bool testSuccess = true;

    REI_waitQueueIdle(queue);

    const int BUFFER_ITEMS = 1024 * 8;
    const int BUFFER_SIZE = BUFFER_ITEMS * sizeof(int);

    REI_Buffer* uploadBuffer;
    REI_Buffer* gpuBuffer1;
    REI_Buffer* gpuBuffer2;
    REI_Buffer* downloadBuffer;

    // init
    {
        REI_BufferDesc vbDesc = {};
        vbDesc.descriptors = REI_DESCRIPTOR_TYPE_UNDEFINED;
        vbDesc.size = BUFFER_SIZE;

        vbDesc.startState = REI_RESOURCE_STATE_COPY_SOURCE;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &uploadBuffer);

        vbDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_ONLY;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &gpuBuffer1);

        vbDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_ONLY;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &gpuBuffer2);

        vbDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &downloadBuffer);

        int* data = nullptr;
        REI_mapBuffer(renderer, uploadBuffer, (void**)&data);
        for (int i = 0; i < BUFFER_ITEMS; ++i)
        {
            data[i] = i + 1;
        }
        REI_unmapBuffer(renderer, uploadBuffer);
    }

    // commands
    {
        REI_resetCmdPool(renderer, cmdPool);
        REI_beginCmd(cmd);

        //The problem with the non-working resource barrier on the PC | VULKAN 
        //when we use overlapping ranges in REI_cmdCopyBuffer
        #if 1
        const uint64_t copyOffset[] = { 0, 1, 256 + 1 };
        const uint64_t copyNumber[] = { 1, 256, BUFFER_ITEMS - 256 - 1 };
        #else
        const uint64_t copyOffset[] = { 0, 1, 256 };
        const uint64_t copyNumber[] = { 2, 256, BUFFER_ITEMS - 256 };
        #endif

        for (int i = 0; i < 3; ++i)
        {
            REI_cmdCopyBuffer(
                cmd, gpuBuffer1, copyOffset[i] * sizeof(uint32_t), uploadBuffer, copyOffset[i] * sizeof(uint32_t),
                copyNumber[i] * sizeof(uint32_t));
        }

        REI_BufferBarrier barrier;
        barrier.startState = REI_RESOURCE_STATE_COPY_DEST;
        barrier.endState = REI_RESOURCE_STATE_COPY_SOURCE;
        barrier.pBuffer = gpuBuffer1;
        REI_cmdResourceBarrier(cmd, 1, &barrier, 0, nullptr);

        for (int i = 0; i < 3; ++i)
        {
            REI_cmdCopyBuffer(
                cmd, gpuBuffer2, copyOffset[i] * sizeof(uint32_t), gpuBuffer1, copyOffset[i] * sizeof(uint32_t),
                copyNumber[i] * sizeof(uint32_t));
        }

        barrier.startState = REI_RESOURCE_STATE_COPY_DEST;
        barrier.endState = REI_RESOURCE_STATE_COPY_SOURCE;
        barrier.pBuffer = gpuBuffer2;
        REI_cmdResourceBarrier(cmd, 1, &barrier, 0, nullptr);

        for (int i = 0; i < 3; ++i)
        {
            REI_cmdCopyBuffer(
                cmd, downloadBuffer, copyOffset[i] * sizeof(uint32_t), gpuBuffer2, copyOffset[i] * sizeof(uint32_t),
                copyNumber[i] * sizeof(uint32_t));
        }

        REI_endCmd(cmd);
        REI_queueSubmit(queue, 1, &cmd, fence, 0, 0, 0, 0);
        REI_waitForFences(renderer, 1, &fence);
    }

    // test
    {
        int* result = nullptr;
        REI_mapBuffer(renderer, downloadBuffer, (void**)&result);

        for (int i = 0; i < BUFFER_ITEMS; ++i)
        {
            TEST(result[i] == i + 1);
        }

        REI_unmapBuffer(renderer, downloadBuffer);
    }

    // deinit
    {
        REI_removeBuffer(renderer, uploadBuffer);
        REI_removeBuffer(renderer, gpuBuffer1);
        REI_removeBuffer(renderer, gpuBuffer2);
        REI_removeBuffer(renderer, downloadBuffer);
    }

    return testSuccess;
}

// Tests: initializing texture content, basic rendering into RT, sampling SRV with swizzling, copying texture to buffer
bool test_render_srv_swizzling(
    REI_Renderer* renderer, REI_RL_State* loader, REI_Queue* queue, REI_Cmd* cmd, REI_CmdPool* cmdPool,
    REI_Fence* fence)
{
    bool testSuccess = true;

    const int  ROW_TEXELS = 4;
    const int  TEXEL_CHANNELS = 4;
    const int  TEXEL_SIZE = sizeof(uint8_t) * TEXEL_CHANNELS;
    const int  ROW_SIZE = ROW_TEXELS * TEXEL_SIZE;
    const int  BUFFER_SIZE = ROW_SIZE;

    Graphics      graphics;
    REI_Texture*  texture;
    REI_Buffer*   downloadBuffer;

    // init
    {
        REI_BufferDesc vbDesc = {};
        vbDesc.descriptors = REI_DESCRIPTOR_TYPE_UNDEFINED;
        vbDesc.size = BUFFER_SIZE;
        vbDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &downloadBuffer);

        REI_TextureDesc textureDesc{};
        textureDesc.flags =
            REI_TextureCreationFlags(REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | REI_TEXTURE_CREATION_FLAG_FORCE_2D);
        textureDesc.width = ROW_TEXELS;
        textureDesc.height = 1;
        textureDesc.depth = 1;
        textureDesc.arraySize = 1;
        textureDesc.mipLevels = 1;
        textureDesc.sampleCount = REI_SAMPLE_COUNT_1;
        textureDesc.format = REI_FMT_R8G8B8A8_UNORM;
        textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
        textureDesc.componentMapping[0] = REI_COMPONENT_MAPPING_B;
        textureDesc.componentMapping[1] = REI_COMPONENT_MAPPING_B;
        textureDesc.componentMapping[2] = REI_COMPONENT_MAPPING_R;
        textureDesc.componentMapping[3] = REI_COMPONENT_MAPPING_R;
        REI_addTexture(renderer, &textureDesc, &texture);

        REI_TextureDesc colorRTDesc{};
        colorRTDesc.flags =
            REI_TextureCreationFlags(REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | REI_TEXTURE_CREATION_FLAG_FORCE_2D);
        colorRTDesc.width = ROW_TEXELS;
        colorRTDesc.height = 1;
        colorRTDesc.depth = 1;
        colorRTDesc.arraySize = 1;
        colorRTDesc.mipLevels = 1;
        colorRTDesc.sampleCount = REI_SAMPLE_COUNT_1;
        colorRTDesc.format = REI_FMT_B8G8R8A8_UNORM;
        colorRTDesc.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_COPY_SRC;
        REI_addTexture(renderer, &colorRTDesc, &graphics.colorRT);

        REI_ShaderDesc shaderDesc[2] = {
            { REI_SHADER_STAGE_VERT, (uint8_t*)test_sample_texture_vs_bytecode, sizeof(test_sample_texture_vs_bytecode) },
            { REI_SHADER_STAGE_FRAG, (uint8_t*)test_sample_texture_ps_bytecode, sizeof(test_sample_texture_ps_bytecode) }
        };
        REI_Shader*    shaders[2];
        REI_addShaders(renderer, shaderDesc, 2, shaders);

        REI_SamplerDesc samplerDesc = {};
        samplerDesc.minFilter = REI_FILTER_NEAREST;
        samplerDesc.magFilter = REI_FILTER_NEAREST;
        samplerDesc.mipmapMode = REI_MIPMAP_MODE_NEAREST;
        samplerDesc.addressU = REI_ADDRESS_MODE_REPEAT;
        samplerDesc.addressV = REI_ADDRESS_MODE_REPEAT;
        samplerDesc.addressW = REI_ADDRESS_MODE_REPEAT;
        samplerDesc.compareFunc = REI_CMP_NEVER;
        REI_addSampler(renderer, &samplerDesc, &graphics.sampler);

        REI_DescriptorBinding descriptor;
        descriptor.reg = 0;
        descriptor.binding = 0;
        descriptor.descriptorCount = 1;
        descriptor.descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;

        REI_DescriptorTableLayout setLayout;
        setLayout.slot = REI_DESCRIPTOR_TABLE_SLOT_0;
        setLayout.bindingCount = 1;
        setLayout.pBindings = &descriptor;
        setLayout.stageFlags = REI_SHADER_STAGE_FRAG;

        REI_RootSignatureDesc rootDesc = {};
        rootDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
        rootDesc.tableLayoutCount = 1;
        rootDesc.pTableLayouts = &setLayout;
        
        REI_addRootSignature(renderer, &rootDesc, &graphics.signature);

        REI_DescriptorTableArrayDesc descriptorSetDesc = {};
        descriptorSetDesc.pRootSignature = graphics.signature;
        descriptorSetDesc.maxTables = 1;
        descriptorSetDesc.slot = REI_DESCRIPTOR_TABLE_SLOT_0;
        REI_addDescriptorTableArray(renderer, &descriptorSetDesc, &graphics.descriptors);

        REI_DescriptorData params[1] = {};
        params[0].descriptorIndex = 0;
        params[0].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
        params[0].ppTextures = &texture;
        params[0].count = 1;
        params[0].tableIndex = 0;
        REI_updateDescriptorTableArray(renderer, graphics.descriptors, 1, params);

        REI_RasterizerStateDesc rasterDesc = {};
        rasterDesc.cullMode = REI_CULL_MODE_NONE;

        REI_Format rtFormat = REI_FMT_B8G8R8A8_UNORM;
        REI_PipelineDesc pipelineDesc{};
        pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
        REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
        graphicsDesc.ppShaderPrograms = shaders;
        graphicsDesc.shaderProgramCount = 2;
        graphicsDesc.pRootSignature = graphics.signature;
        graphicsDesc.vertexAttribCount = 0;
        graphicsDesc.pVertexAttribs = nullptr;
        graphicsDesc.pBlendState = nullptr;
        graphicsDesc.pDepthState = nullptr;
        graphicsDesc.pRasterizerState = &rasterDesc;
        graphicsDesc.pColorFormats = &rtFormat;
        graphicsDesc.renderTargetCount = 1;
        graphicsDesc.sampleCount = REI_SAMPLE_COUNT_1;
        graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_STRIP;
        REI_addPipeline(renderer, &pipelineDesc, &graphics.pipeline);

        REI_removeShaders(renderer, 2, shaders);

        uint8_t data[ROW_TEXELS * TEXEL_CHANNELS];
        for (int i = 0; i < ROW_TEXELS; ++i)
        {
            data[i * 4 + 0] = ((i + 1) * 10 + 1);
            data[i * 4 + 1] = ((i + 1) * 10 + 2);
            data[i * 4 + 2] = ((i + 1) * 10 + 3);
            data[i * 4 + 3] = ((i + 1) * 10 + 4);
        }

        REI_RL_TextureUpdateDesc updateDesc = {};
        updateDesc.pTexture = texture;
        updateDesc.pRawData = (uint8_t*)data;
        updateDesc.format = REI_FMT_R8G8B8A8_UNORM;
        updateDesc.x = updateDesc.y = updateDesc.z = 0;
        updateDesc.width = ROW_TEXELS;
        updateDesc.height = 1;
        updateDesc.depth = 1;
        updateDesc.arrayLayer = 0;
        updateDesc.mipLevel = 0;
        updateDesc.endState = REI_RESOURCE_STATE_SHADER_RESOURCE;

        REI_RL_updateResource(loader, &updateDesc, nullptr);
        REI_RL_waitBatchCompleted(loader);
    }

    // commands
    {
        REI_resetCmdPool(renderer, cmdPool);
        REI_beginCmd(cmd);

        {
            REI_TextureBarrier barriers[2];

            barriers[0].startState = REI_RESOURCE_STATE_UNDEFINED;
            barriers[0].endState = REI_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].pTexture = graphics.colorRT;

            barriers[1].startState = REI_RESOURCE_STATE_UNDEFINED;
            barriers[1].endState = REI_RESOURCE_STATE_SHADER_RESOURCE;
            barriers[1].pTexture = texture;

            REI_cmdResourceBarrier(cmd, 0, nullptr, 2, barriers);
        }

        REI_LoadActionsDesc loadActions{};
        loadActions.loadActionsColor[0] = REI_LOAD_ACTION_CLEAR;
        loadActions.clearColorValues[0].rt.r = 103 / 255.f;
        loadActions.clearColorValues[0].rt.g = 102 / 255.f;
        loadActions.clearColorValues[0].rt.b = 101 / 255.f;
        loadActions.clearColorValues[0].rt.a = 104 / 255.f;


        REI_cmdBindRenderTargets(cmd, 1, &graphics.colorRT, nullptr, &loadActions, nullptr, nullptr, 0, 0);

        REI_cmdBindPipeline(cmd, graphics.pipeline);
        REI_cmdBindDescriptorTable(cmd, 0, graphics.descriptors);

        REI_cmdSetViewport(cmd, 0.0f, 0.0f, ROW_TEXELS, 1.0f, 0.0f, 1.0f);
        REI_cmdSetScissor(cmd, 0, 0, ROW_TEXELS, 1);

        REI_cmdDraw(cmd, 3, 0);    // only rasterizes pixel at position (1, 0)

        REI_endCmd(cmd);
        REI_queueSubmit(queue, 1, &cmd, fence, 0, 0, 0, 0);
        REI_waitForFences(renderer, 1, &fence);
        
        REI_resetCmdPool(renderer, cmdPool);
        REI_beginCmd(cmd);

        {
            REI_TextureBarrier barrier;
            barrier.startState = REI_RESOURCE_STATE_RENDER_TARGET;
            barrier.endState = REI_RESOURCE_STATE_COPY_SOURCE;
            barrier.pTexture = graphics.colorRT;
            REI_cmdResourceBarrier(cmd, 0, nullptr, 1, &barrier);
        }

        REI_SubresourceDesc copyDesc;
        copyDesc.bufferOffset = 0;
        copyDesc.rowPitch = ROW_SIZE;
        copyDesc.slicePitch = ROW_SIZE;
        copyDesc.arrayLayer = 0;
        copyDesc.mipLevel = 0;
        copyDesc.region = { 0, 0, 0, ROW_TEXELS, 1, 1 };
        REI_cmdCopyTextureToBuffer(cmd, downloadBuffer, graphics.colorRT, &copyDesc);

        REI_endCmd(cmd);
        REI_queueSubmit(queue, 1, &cmd, fence, 0, 0, 0, 0);
        REI_waitForFences(renderer, 1, &fence);
    }

    // test
    {
        uint8_t* result = nullptr;
        REI_mapBuffer(renderer, downloadBuffer, (void**)&result);

        TEST(
            result[4 * 0 + 0] == 101 && result[4 * 0 + 1] == 102 && result[4 * 0 + 2] == 103 &&
            result[4 * 0 + 3] == 104);

        // only pixel (1, 0) was rasterized
        TEST(
            result[4 * 1 + 0] == 21 && result[4 * 1 + 1] == 23 && result[4 * 1 + 2] == 23 &&
            result[4 * 1 + 3] == 21);

        TEST(
            result[4 * 2 + 0] == 101 && result[4 * 2 + 1] == 102 && result[4 * 2 + 2] == 103 &&
            result[4 * 2 + 3] == 104);

        TEST(
            result[4 * 3 + 0] == 101 && result[4 * 3 + 1] == 102 && result[4 * 3 + 2] == 103 &&
            result[4 * 3 + 3] == 104);

        REI_unmapBuffer(renderer, downloadBuffer);

    }

    // deinit
    {
        REI_removeTexture(renderer, texture);
        REI_removeBuffer(renderer, downloadBuffer);

        REI_removeTexture(renderer, graphics.colorRT);
        REI_removeRootSignature(renderer, graphics.signature);
        REI_removeSampler(renderer, graphics.sampler);
        REI_removePipeline(renderer, graphics.pipeline);
        REI_removeDescriptorTableArray(renderer, graphics.descriptors);
    }

    return testSuccess;
}

#include "shaderbin/test_write_uint_vs.bin.h"
#include "shaderbin/test_write_uint_ps.bin.h"

// Tests: rendering into RT with depth test and occlusion query
bool test_render_depth_query(
    REI_Renderer* renderer, REI_RL_State* loader, REI_Queue* queue, REI_Cmd* cmd, REI_CmdPool* cmdPool,
    REI_Fence* fence)
{
    bool testSuccess = true;

    const uint32_t TEXELS_PER_DIM = 4;

    REI_DeviceProperties deviceProperties = {};
    REI_getDeviceProperties(renderer, &deviceProperties);
    REI_DeviceCapabilities& deviceCaps = deviceProperties.capabilities;

    const uint32_t COLOR_ROW_BYTES = std::max(deviceCaps.uploadBufferTextureRowAlignment, uint32_t(TEXELS_PER_DIM * sizeof(uint8_t)));
    const uint32_t DEPTH_ROW_BYTES =
        std::max(deviceCaps.uploadBufferTextureRowAlignment, uint32_t(TEXELS_PER_DIM * sizeof(float)));

    const uint32_t COLOR_BUFFER_BYTES = COLOR_ROW_BYTES * TEXELS_PER_DIM;
    const uint32_t DEPTH_BUFFER_BYTES = DEPTH_ROW_BYTES * TEXELS_PER_DIM;

    REI_Shader*        shaders[2];
    REI_RootSignature* signature;
    REI_Pipeline*      pipeline;
    REI_Texture*       colorRT;
    REI_Texture*       depthRT;
    REI_Buffer*        vertexBuffer;
    REI_Buffer*        colorDownloadBuffer;
    REI_Buffer*        depthDownloadBuffer;
    REI_QueryPool*     queryPool;
    REI_Buffer*        queryBuffer;

    struct Vertex
    {
        std::array<float, 4> pos;
        uint32_t col;
    };

    // init
    {
        REI_Format rtFormat = REI_FMT_R8_UINT;
        REI_Format depthFormat = REI_FMT_D32_SFLOAT;

        REI_BufferDesc vbDesc = {};
        vbDesc.descriptors = REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
        vbDesc.size = sizeof(Vertex) * 6;
        vbDesc.vertexStride = sizeof(Vertex);
        vbDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &vertexBuffer);

        {
            Vertex* data;
            REI_mapBuffer(renderer, vertexBuffer, (void**)&data);

            data[0].pos = { -1.f, -1.f, 0.5f, 1.f };
            data[1].pos = { 1.f + 0.01f, -1.f, 0.5f, 1.f };
            data[2].pos = { -1.f, 1.f + 0.01f, 0.5f, 1.f };
            data[0].col = data[1].col = data[2].col = 27;

            data[3].pos = { 1.f, -1.f, 0.75f, 1.f };
            data[4].pos = { -1.f - 0.01f, -1.f, 0.75f, 1.f };
            data[5].pos = { 1.f, 1.f + 0.01f, 0.75f, 1.f };
            data[3].col = data[4].col = data[5].col = 42;

            REI_unmapBuffer(renderer, vertexBuffer);
        }

        vbDesc = {};
        vbDesc.descriptors = REI_DESCRIPTOR_TYPE_UNDEFINED;
        vbDesc.size = COLOR_BUFFER_BYTES;    
        vbDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &colorDownloadBuffer);

        vbDesc = {};
        vbDesc.descriptors = REI_DESCRIPTOR_TYPE_UNDEFINED;
        vbDesc.size = DEPTH_BUFFER_BYTES;    
        vbDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &depthDownloadBuffer);

        REI_TextureDesc colorRTDesc{};
        colorRTDesc.flags =
            REI_TextureCreationFlags(REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | REI_TEXTURE_CREATION_FLAG_FORCE_2D);
        colorRTDesc.width = TEXELS_PER_DIM;
        colorRTDesc.height = TEXELS_PER_DIM;
        colorRTDesc.depth = 1;
        colorRTDesc.arraySize = 1;
        colorRTDesc.mipLevels = 1;
        colorRTDesc.sampleCount = REI_SAMPLE_COUNT_1;
        colorRTDesc.format = rtFormat;
        colorRTDesc.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_COPY_SRC;
        REI_addTexture(renderer, &colorRTDesc, &colorRT);

        REI_TextureDesc depthRTDesc{};
        depthRTDesc.flags =
            REI_TextureCreationFlags(REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT | REI_TEXTURE_CREATION_FLAG_FORCE_2D);
        depthRTDesc.width = TEXELS_PER_DIM;
        depthRTDesc.height = TEXELS_PER_DIM;
        depthRTDesc.depth = 1;
        depthRTDesc.arraySize = 1;
        depthRTDesc.mipLevels = 1;
        depthRTDesc.sampleCount = REI_SAMPLE_COUNT_1;
        depthRTDesc.format = depthFormat;
        depthRTDesc.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_COPY_SRC;
        REI_addTexture(renderer, &depthRTDesc, &depthRT);

        REI_ShaderDesc shaderDesc[2] = {
            { REI_SHADER_STAGE_VERT, (uint8_t*)test_write_uint_vs_bytecode, sizeof(test_write_uint_vs_bytecode) },
            { REI_SHADER_STAGE_FRAG, (uint8_t*)test_write_uint_ps_bytecode, sizeof(test_write_uint_ps_bytecode) }
        };
        REI_addShaders(renderer, shaderDesc, 2, shaders);

        REI_RootSignatureDesc rootDesc = {};
        rootDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
        REI_addRootSignature(renderer, &rootDesc, &signature);

        REI_RasterizerStateDesc rasterDesc = {};
        rasterDesc.cullMode = REI_CULL_MODE_NONE;

        REI_DepthStateDesc depthDesc = {};
        depthDesc.depthWriteEnable = true;
        depthDesc.depthTestEnable = true;
        depthDesc.depthCmpFunc = REI_CompareMode::REI_CMP_LESS;

        const size_t     vertexAttribCount = 2;
        REI_VertexAttrib vertexAttribs[vertexAttribCount] = {};
        vertexAttribs[0].semantic= REI_SEMANTIC_POSITION0;
        vertexAttribs[0].offset = offsetof(Vertex, pos);
        vertexAttribs[0].location = 0;
        vertexAttribs[0].format = REI_FMT_R32G32B32A32_SFLOAT;
        vertexAttribs[1].semantic = REI_SEMANTIC_COLOR0;
        vertexAttribs[1].offset = offsetof(Vertex, col);
        vertexAttribs[1].location = 1;
        vertexAttribs[1].format = REI_FMT_R32_UINT;

        REI_PipelineDesc pipelineDesc{};
        pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
        REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
        graphicsDesc.ppShaderPrograms = shaders;
        graphicsDesc.shaderProgramCount = 2;
        graphicsDesc.pRootSignature = signature;
        graphicsDesc.vertexAttribCount = vertexAttribCount;
        graphicsDesc.pVertexAttribs = vertexAttribs;
        graphicsDesc.pBlendState = nullptr;
        graphicsDesc.pDepthState = &depthDesc;
        graphicsDesc.pRasterizerState = &rasterDesc;
        graphicsDesc.pColorFormats = &rtFormat;
        graphicsDesc.depthStencilFormat = depthFormat;
        graphicsDesc.renderTargetCount = 1;
        graphicsDesc.sampleCount = REI_SAMPLE_COUNT_1;
        graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;
        REI_addPipeline(renderer, &pipelineDesc, &pipeline);

        REI_QueryPoolDesc queryPoolDesc = {};
        queryPoolDesc.type = REI_QUERY_TYPE_OCCLUSION;
        queryPoolDesc.queryCount = 2;
        REI_addQueryPool(renderer, &queryPoolDesc, &queryPool);

        vbDesc = {};
        vbDesc.descriptors = REI_DESCRIPTOR_TYPE_UNDEFINED;
        vbDesc.size = 2 * sizeof(uint64_t);
        vbDesc.startState = REI_RESOURCE_STATE_COPY_SOURCE;
        vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
        vbDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
        REI_addBuffer(renderer, &vbDesc, &queryBuffer);
    }

    // commands
    {
        REI_resetCmdPool(renderer, cmdPool);
        REI_beginCmd(cmd);

        REI_cmdResetQueryPool(cmd, queryPool, 0, 2);

        {
            REI_TextureBarrier barriers[2];

            barriers[0].startState = REI_RESOURCE_STATE_UNDEFINED;
            barriers[0].endState = REI_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].pTexture = colorRT;

            barriers[1].startState = REI_RESOURCE_STATE_UNDEFINED;
            barriers[1].endState = REI_RESOURCE_STATE_DEPTH_WRITE;
            barriers[1].pTexture = depthRT;

            REI_cmdResourceBarrier(cmd, 0, nullptr, 2, barriers);
        }

        REI_LoadActionsDesc loadActions{};
        loadActions.loadActionsColor[0] = REI_LOAD_ACTION_CLEAR;
        loadActions.loadActionDepth = REI_LOAD_ACTION_CLEAR;

        loadActions.clearColorValues[0].rt.r = 0;
        loadActions.clearColorValues[0].rt.g = 0;
        loadActions.clearColorValues[0].rt.b = 0;
        loadActions.clearColorValues[0].rt.a = 0;

        loadActions.clearDepth.ds.depth = 1.f;

        REI_cmdBindRenderTargets(cmd, 1, &colorRT, depthRT, &loadActions, nullptr, nullptr, 0, 0);

        REI_cmdBindPipeline(cmd, pipeline);
        REI_cmdBindVertexBuffer(cmd, 1, &vertexBuffer, nullptr);

        REI_cmdSetViewport(cmd, 0.0f, 0.0f, TEXELS_PER_DIM, TEXELS_PER_DIM, 0.0f, 1.0f);
        REI_cmdSetScissor(cmd, 0, 0, TEXELS_PER_DIM, TEXELS_PER_DIM);

        REI_cmdBeginQuery(cmd, queryPool, 0);
        REI_cmdDraw(cmd, 3, 0);
        REI_cmdEndQuery(cmd, queryPool, 0);

        REI_cmdBeginQuery(cmd, queryPool, 1);
        REI_cmdDraw(cmd, 3, 3);
        REI_cmdEndQuery(cmd, queryPool, 1);

        REI_endCmd(cmd);
        REI_queueSubmit(queue, 1, &cmd, fence, 0, 0, 0, 0);
        REI_waitForFences(renderer, 1, &fence);

        REI_resetCmdPool(renderer, cmdPool);
        REI_beginCmd(cmd);

        REI_cmdResolveQuery(cmd, queryBuffer, 0, queryPool, 0, 2);

        {
            REI_TextureBarrier barriers[2];

            barriers[0].startState = REI_RESOURCE_STATE_RENDER_TARGET;
            barriers[0].endState = REI_RESOURCE_STATE_COPY_SOURCE;
            barriers[0].pTexture = colorRT;

            barriers[1].startState = REI_RESOURCE_STATE_DEPTH_WRITE;
            barriers[1].endState = REI_RESOURCE_STATE_COPY_SOURCE;
            barriers[1].pTexture = depthRT;

            REI_cmdResourceBarrier(cmd, 0, nullptr, 2, barriers);
        }

        REI_SubresourceDesc copyDesc = {};
        copyDesc.region = { 0, 0, 0, TEXELS_PER_DIM, TEXELS_PER_DIM, 1 };
        REI_cmdCopyTextureToBuffer(cmd, colorDownloadBuffer, colorRT, &copyDesc);
        REI_cmdCopyTextureToBuffer(cmd, depthDownloadBuffer, depthRT, &copyDesc);

        REI_endCmd(cmd);
        REI_queueSubmit(queue, 1, &cmd, fence, 0, 0, 0, 0);
        REI_waitForFences(renderer, 1, &fence);
    }

    // test
    {
        uint8_t* color = nullptr;
        REI_mapBuffer(renderer, colorDownloadBuffer, (void**)&color);

        TEST(color[0] == 27 && color[1] == 0 && color[2] == 0 && color[3] == 42);
        color += COLOR_ROW_BYTES / sizeof(uint8_t);

        TEST(color[0] == 27 && color[1] == 27 && color[2] == 42 && color[3] == 42);
        color += COLOR_ROW_BYTES / sizeof(uint8_t);

        TEST(color[0] == 27 && color[1] == 27 && color[2] == 27 && color[3] == 42);
        color += COLOR_ROW_BYTES / sizeof(uint8_t);

        TEST(color[0] == 27 && color[1] == 27 && color[2] == 27 && color[3] == 27);
        color += COLOR_ROW_BYTES / sizeof(uint8_t);

        REI_unmapBuffer(renderer, colorDownloadBuffer);

        float* depth = nullptr;
        REI_mapBuffer(renderer, depthDownloadBuffer, (void**)&depth);

        TEST(depth[0] == 0.5f && depth[1] == 1.f && depth[2] == 1.f && depth[3] == 0.75f);
        depth += DEPTH_ROW_BYTES / sizeof(float);

        TEST(depth[0] == 0.5f && depth[1] == 0.5f && depth[2] == 0.75f && depth[3] == 0.75f);
        depth += DEPTH_ROW_BYTES / sizeof(float);

        TEST(depth[0] == 0.5f && depth[1] == 0.5f && depth[2] == 0.5f && depth[3] == 0.75f);
        depth += DEPTH_ROW_BYTES / sizeof(float);

        TEST(depth[0] == 0.5f && depth[1] == 0.5f && depth[2] == 0.5f && depth[3] == 0.5f);
        depth += DEPTH_ROW_BYTES / sizeof(float);

        REI_unmapBuffer(renderer, depthDownloadBuffer);

        uint64_t* queryResult = nullptr;
        REI_mapBuffer(renderer, queryBuffer, (void**)&queryResult);

        TEST(queryResult[0] == 10);
        TEST(queryResult[1] == 4);

        REI_unmapBuffer(renderer, queryBuffer);
    }

    // deinit
    {
        REI_removeShaders(renderer, 2, shaders);
        REI_removeRootSignature(renderer, signature);
        REI_removePipeline(renderer, pipeline);
        REI_removeTexture(renderer, colorRT);
        REI_removeTexture(renderer, depthRT);
        REI_removeBuffer(renderer, vertexBuffer);
        REI_removeBuffer(renderer, colorDownloadBuffer);
        REI_removeBuffer(renderer, depthDownloadBuffer);
        REI_removeQueryPool(renderer, queryPool);
        REI_removeBuffer(renderer, queryBuffer);
    }

    return testSuccess;
}

#define RUN_TEST(name)                                                               \
    {                                                                                \
        testTotal += 1;                                                              \
        if (name(renderer, loader, queue, cmd, cmdPool, fence))                               \
        {                                                                            \
            testPassed += 1;                                                         \
            sample_log(REI_LOG_TYPE_INFO, "TEST PASSED " #name); \
        }                                                                            \
        else                                                                         \
            sample_log(REI_LOG_TYPE_INFO, "TEST FAILED " #name); \
    }

void perform_tests(REI_Renderer* renderer, REI_Queue* queue, REI_Cmd* cmd, REI_CmdPool* cmdPool)
{
    REI_Fence* fence;
    REI_addFence(renderer, &fence);

    REI_RL_State* loader;
    REI_RL_addResourceLoader(renderer, nullptr, &loader);

    uint32_t testPassed = 0;
    uint32_t testTotal = 0;

    sample_log(REI_LOG_TYPE_INFO, "TESTS STARTED");

    RUN_TEST(test_copyBuffer);
    RUN_TEST(test_render_srv_swizzling);
    RUN_TEST(test_render_depth_query);

    sample_log(REI_LOG_TYPE_INFO, "TESTS FINISHED, %i/%i", testPassed, testTotal);

    REI_removeFence(renderer, fence);
    REI_RL_removeResourceLoader(loader);
}