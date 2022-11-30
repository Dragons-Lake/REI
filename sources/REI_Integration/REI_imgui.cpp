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

#include <stdio.h>

#include "ResourceLoader.h"
#include "REI_imgui.h"

#ifdef _WIN32
#    define strcpy strcpy_s
#endif

static const uint32_t MAX_SHADER_COUNT = 2;

struct REI_ImGui_State
{
    REI_ImGui_Desc            desc;
    REI_Renderer*             renderer;
    REI_RootSignature*        rootSignature;
    REI_DescriptorTableArray* descriptorSet;
    REI_Pipeline*             pipeline;
    REI_Sampler*              fontSampler;
    REI_Texture*              fontTexture;
    REI_Buffer**              buffers;
    void**                    buffersAddr;
    REI_AllocatorCallbacks    allocator;
};

static REI_ImGui_State g_State = {};

#include "shaderbin/imgui_vs.bin.h"

#include "shaderbin/imgui_ps.bin.h"

static void REI_ImGui_SetupRenderState(
    ImDrawData* draw_data, REI_Cmd* command_buffer, REI_Buffer* vertex_buffer, REI_Buffer* index_buffer, int fb_width,
    int fb_height)
{
    // Bind pipeline and descriptor sets:
    {
        REI_cmdBindPipeline(command_buffer, g_State.pipeline);
        REI_cmdBindDescriptorTable(command_buffer, 0, g_State.descriptorSet);
    }

    // Bind Vertex And Index Buffer:
    {
        uint64_t vertex_offset = 0;
        REI_cmdBindVertexBuffer(command_buffer, 1, &vertex_buffer, &vertex_offset);
        REI_cmdBindIndexBuffer(command_buffer, index_buffer, 0);
    }

    // Setup viewport:
    {
        REI_cmdSetViewport(command_buffer, 0.0f, 0.0f, (float)fb_width, (float)fb_height, 0.0f, 1.0f);
    }

    // Setup scale and translation:
    // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    {
        float scaleTranslate[4];
        scaleTranslate[0] = 2.0f / draw_data->DisplaySize.x;
        scaleTranslate[1] = -2.0f / draw_data->DisplaySize.y;
        scaleTranslate[2] = -1.0f - draw_data->DisplayPos.x * scaleTranslate[0];
        scaleTranslate[3] = 1.0f + draw_data->DisplayPos.y * scaleTranslate[1];
        REI_cmdBindPushConstants(
            command_buffer, g_State.rootSignature, REI_SHADER_STAGE_VERT, 0, sizeof(scaleTranslate), scaleTranslate);
    }
}

// Render function
// (this used to be set in io.RenderDrawListsFn and called by ImGui::Render(), but you can now call this directly from your main loop)
void REI_ImGui_Render(ImDrawData* draw_data, REI_Cmd* command_buffer, uint32_t resource_set)
{
    if (!draw_data)
    {
        return;
    }

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0 || draw_data->TotalVtxCount == 0)
        return;

    // Create or resize the vertex/index buffers
    uint64_t vertex_size = 0;
    uint64_t index_size = 0;
    int      cmdListNum = 0;
    // Upload vertex/index data into a single contiguous GPU buffer
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        vertex_size += cmd_list->VtxBuffer.Size * sizeof(ImDrawVert);
        index_size += cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx);
        if (vertex_size > g_State.desc.vertexBufferSize || index_size > g_State.desc.indexBufferSize)
            break;
        ++cmdListNum;
    }

    REI_Buffer* vertex_buffer = g_State.buffers[resource_set * 2];
    REI_Buffer* index_buffer = g_State.buffers[resource_set * 2 + 1];

    REI_ImGui_SetupRenderState(draw_data, command_buffer, vertex_buffer, index_buffer, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;            // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale;    // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    int         global_vtx_offset = 0;
    int         global_idx_offset = 0;
    ImDrawVert* vtx_dst = (ImDrawVert*)g_State.buffersAddr[resource_set * 2];
    ImDrawIdx*  idx_dst = (ImDrawIdx*)g_State.buffersAddr[resource_set * 2 + 1];
    for (int n = 0; n < cmdListNum; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtx_dst += cmd_list->VtxBuffer.Size;
        idx_dst += cmd_list->IdxBuffer.Size;

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    REI_ImGui_SetupRenderState(
                        draw_data, command_buffer, vertex_buffer, index_buffer, fb_width, fb_height);
                }
                else
                {
                    pcmd->UserCallback(cmd_list, pcmd);
                }
            }
            else
            {
                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
                {
                    if (clip_rect.x < 0.0f)
                        clip_rect.x = 0.0f;
                    if (clip_rect.y < 0.0f)
                        clip_rect.y = 0.0f;

                    REI_cmdSetScissor(
                        command_buffer, (int32_t)(clip_rect.x), (int32_t)(clip_rect.y),
                        (uint32_t)(clip_rect.z - clip_rect.x), (uint32_t)(clip_rect.w - clip_rect.y));

                    // Draw
                    REI_cmdDrawIndexed(
                        command_buffer, pcmd->ElemCount, pcmd->IdxOffset + global_idx_offset,
                        pcmd->VtxOffset + global_vtx_offset);
                }
            }
        }
        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

bool REI_ImGui_CreateFontsTexture(REI_RL_State* loader, REI_RL_RequestId* token)
{
    ImGuiIO& io = ImGui::GetIO();

    unsigned char* pixels;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    //size_t upload_size = width * height * 4 * sizeof(char);

    REI_TextureDesc textureDesc{};
    textureDesc.flags = REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
    textureDesc.width = (uint32_t)width;
    textureDesc.height = (uint32_t)height;
    textureDesc.format = REI_FMT_R8G8B8A8_UNORM;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(g_State.renderer, &textureDesc, &g_State.fontTexture);
    REI_RL_TextureUpdateDesc updateDesc = { g_State.fontTexture,
                                            pixels,
                                            REI_FMT_R8G8B8A8_UNORM,
                                            0,
                                            0,
                                            0,
                                            (uint32_t)width,
                                            (uint32_t)height,
                                            1,
                                            0,
                                            0,
                                            REI_RESOURCE_STATE_SHADER_RESOURCE };
    REI_RL_updateResource(loader, &updateDesc, token);
    io.Fonts->TexID = (void*)g_State.fontTexture;

    REI_DescriptorData params[1] = {};
    params[0].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
    params[0].descriptorIndex = 0;    //uTexture
    params[0].ppTextures = &g_State.fontTexture;
    params[0].tableIndex = 0;
    REI_updateDescriptorTableArray(g_State.renderer, g_State.descriptorSet, 1, params);

    // Store our identifier
    io.Fonts->TexID = (ImTextureID)(intptr_t)g_State.fontTexture;

    return true;
}

bool REI_ImGui_CreateDeviceObjects()
{
    REI_ShaderDesc shaderDesc[MAX_SHADER_COUNT] = {
        shaderDesc[0] = { REI_SHADER_STAGE_VERT, (uint8_t*)imgui_vs_bytecode, sizeof(imgui_vs_bytecode) },
        shaderDesc[1] = { REI_SHADER_STAGE_FRAG, (uint8_t*)imgui_ps_bytecode, sizeof(imgui_ps_bytecode) }
    };
    REI_Shader* shaders[MAX_SHADER_COUNT] = {};
    REI_addShaders(g_State.renderer, shaderDesc, MAX_SHADER_COUNT, shaders);

    REI_SamplerDesc samplerDesc = {
        REI_FILTER_LINEAR,
        REI_FILTER_LINEAR,
        REI_MIPMAP_MODE_LINEAR,
        REI_ADDRESS_MODE_REPEAT,
        REI_ADDRESS_MODE_REPEAT,
        REI_ADDRESS_MODE_REPEAT,
        REI_CMP_NEVER,
        0.0f,
        1.0f,
    };
    REI_addSampler(g_State.renderer, &samplerDesc, &g_State.fontSampler);

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
    staticSamplerBinding.ppStaticSamplers = &g_State.fontSampler;

    rootSigDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
    rootSigDesc.pushConstantRangeCount = 1;
    rootSigDesc.pPushConstantRanges = &pConst;
    rootSigDesc.tableLayoutCount = 1;
    rootSigDesc.pTableLayouts = &setLayout;
    rootSigDesc.staticSamplerBindingCount = 1;
    rootSigDesc.staticSamplerSlot = REI_DESCRIPTOR_TABLE_SLOT_0;
    rootSigDesc.staticSamplerStageFlags = REI_SHADER_STAGE_FRAG;
    rootSigDesc.pStaticSamplerBindings = &staticSamplerBinding;

    REI_addRootSignature(g_State.renderer, &rootSigDesc, &g_State.rootSignature);

    REI_DescriptorTableArrayDesc descriptorSetDesc = {};
    descriptorSetDesc.pRootSignature = g_State.rootSignature;
    descriptorSetDesc.maxTables = 1;
    descriptorSetDesc.slot = REI_DESCRIPTOR_TABLE_SLOT_1;
    REI_addDescriptorTableArray(g_State.renderer, &descriptorSetDesc, &g_State.descriptorSet);


    const size_t     vertexAttribCount = 3;
    REI_VertexAttrib vertexAttribs[vertexAttribCount] = {};
    vertexAttribs[0].semantic = REI_SEMANTIC_POSITION0;
    vertexAttribs[0].offset = IM_OFFSETOF(ImDrawVert, pos);
    vertexAttribs[0].location = 0;
    vertexAttribs[0].format = REI_FMT_R32G32_SFLOAT;
    vertexAttribs[1].semantic = REI_SEMANTIC_TEXCOORD0;
    vertexAttribs[1].offset = IM_OFFSETOF(ImDrawVert, uv);
    vertexAttribs[1].location = 1;
    vertexAttribs[1].format = REI_FMT_R32G32_SFLOAT;
    vertexAttribs[2].semantic = REI_SEMANTIC_COLOR0;
    vertexAttribs[2].offset = IM_OFFSETOF(ImDrawVert, col);
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

    REI_Format       colorFormat = (REI_Format)g_State.desc.colorFormat;
    REI_PipelineDesc pipelineDesc = {};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
    REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;
    graphicsDesc.renderTargetCount = 1;
    graphicsDesc.pColorFormats = &colorFormat;
    graphicsDesc.sampleCount = g_State.desc.sampleCount;
    graphicsDesc.depthStencilFormat = g_State.desc.depthStencilFormat;
    graphicsDesc.pRootSignature = g_State.rootSignature;
    graphicsDesc.ppShaderPrograms = shaders;
    graphicsDesc.shaderProgramCount = MAX_SHADER_COUNT;
    graphicsDesc.pVertexAttribs = vertexAttribs;
    graphicsDesc.vertexAttribCount = vertexAttribCount;
    graphicsDesc.pRasterizerState = &rasterizerStateDesc;
    graphicsDesc.pBlendState = &blendState;
    REI_addPipeline(g_State.renderer, &pipelineDesc, &g_State.pipeline);

    REI_removeShaders(g_State.renderer, MAX_SHADER_COUNT, shaders);

    REI_BufferDesc vbDesc = {};
    vbDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER | REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.vertexStride = sizeof(ImDrawVert);
    vbDesc.structStride = sizeof(ImDrawVert);
    vbDesc.size = g_State.desc.vertexBufferSize;
    vbDesc.elementCount = vbDesc.size / vbDesc.structStride;
    vbDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

    REI_BufferDesc ibDesc = vbDesc;
    ibDesc.descriptors = REI_DESCRIPTOR_TYPE_INDEX_BUFFER;
    ibDesc.indexType = REI_INDEX_TYPE_UINT16;
    ibDesc.size = g_State.desc.indexBufferSize;

    g_State.buffers = (REI_Buffer**)REI_calloc(g_State.allocator, g_State.desc.resourceSetCount * 2 * sizeof(REI_Buffer*));
    g_State.buffersAddr = (void**)REI_calloc(g_State.allocator, g_State.desc.resourceSetCount * 2 * sizeof(void*));
    for (uint32_t i = 0; i < g_State.desc.resourceSetCount; ++i)
    {
        REI_addBuffer(g_State.renderer, &vbDesc, &g_State.buffers[2 * i]);
        REI_mapBuffer(g_State.renderer, g_State.buffers[2 * i], &g_State.buffersAddr[2 * i]);
        REI_addBuffer(g_State.renderer, &ibDesc, &g_State.buffers[2 * i + 1]);
        REI_mapBuffer(g_State.renderer, g_State.buffers[2 * i + 1], &g_State.buffersAddr[2 * i + 1]);
    }

    return true;
}

bool REI_ImGui_Init(REI_Renderer* renderer, REI_ImGui_Desc* info)
{
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_REI";
    // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    IM_ASSERT(renderer);

    REI_setupAllocatorCallbacks(info->pAllocator, g_State.allocator);

    g_State.renderer = renderer;
    g_State.desc = *info;
    REI_ImGui_CreateDeviceObjects();

    return true;
}

void REI_ImGui_Shutdown()
{
    for (uint32_t i = 0; i < g_State.desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(g_State.renderer, g_State.buffers[2 * i]);
        REI_removeBuffer(g_State.renderer, g_State.buffers[2 * i + 1]);
    }
    g_State.allocator.pFree(g_State.allocator.pUserData, g_State.buffers);
    g_State.allocator.pFree(g_State.allocator.pUserData, g_State.buffersAddr);

    if (g_State.fontTexture)
    {
        REI_removeTexture(g_State.renderer, g_State.fontTexture);
        g_State.fontTexture = NULL;
    }
    if (g_State.fontSampler)
    {
        REI_removeSampler(g_State.renderer, g_State.fontSampler);
        g_State.fontSampler = NULL;
    }
    if (g_State.rootSignature)
    {
        REI_removeRootSignature(g_State.renderer, g_State.rootSignature);
        g_State.rootSignature = NULL;
    }
    if (g_State.pipeline)
    {
        REI_removePipeline(g_State.renderer, g_State.pipeline);
        g_State.pipeline = NULL;
    }
    if (g_State.descriptorSet)
    {
        REI_removeDescriptorTableArray(g_State.renderer, g_State.descriptorSet);
        g_State.descriptorSet = NULL;
    }
}
