/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
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

#ifndef SAMPLE_TEST
#    include <stdio.h>
#    include <stdlib.h>
#    include <string>
#    include <vector>
#    include "REI_Sample/sample.h"
#    include "REI_Integration/SimpleCamera.h"
#    include "REI_Integration/rm_math.h"
#    include "REI/Renderer.h"
#    include "REI_Integration/ResourceLoader.h"
#    include "REI_Integration/3rdParty/stb/stb_image.h"
#    define CGLTF_IMPLEMENTATION
#    include "REI_Integration/3rdParty/cgltf/cgltf.h"
#endif

#ifdef _WIN32
#    define strcpy strcpy_s
#endif
static const REI_DescriptorTableSlot modelDescriptorSetIndex = REI_DESCRIPTOR_TABLE_SLOT_1;
static const REI_DescriptorTableSlot meshDescriptorSetIndex = REI_DESCRIPTOR_TABLE_SLOT_2;
static const uint32_t                MAX_SHADER_COUNT = 2;

struct GLTF_Mesh
{
    uint32_t indicesLength;
    uint32_t firstIndex;
    uint32_t firstVertex;
    uint32_t  descriptorIndex;
};

struct GLTF_Model
{
    REI_Buffer* vertexBuffer;
    void*       vertexBufferAddr;
    REI_Buffer* indexBuffer;
    void*       indexBufferAddr;
    std::vector<GLTF_Mesh>          meshes;
    std::vector<REI_Texture*>       textures;
    std::vector<REI_DescriptorTableArray*> meshDescriptorSets;
    REI_DescriptorTableArray*              descriptorSet;
    REI_Buffer*                     uniBuffer;
    void*                           uniBufferAddr;
};

struct GLTF_Vertex
{
    rm_vec3 position;
    rm_vec3 color;
    rm_vec3 normal;
    rm_vec2 texUV;
};

void gltf_destroy_model(REI_Renderer* renderer, GLTF_Model& model);

GLTF_Model gltf_load_model(
    const char* file, REI_Renderer* renderer, REI_RL_State* loader, REI_RootSignature* rootSignature);

struct GLTF_StateDesc
{
    uint32_t        colorFormat : REI_FORMAT_BIT_COUNT;
    uint32_t        depthStencilFormat : REI_FORMAT_BIT_COUNT;
    uint32_t        sampleCount : REI_SAMPLE_COUNT_BIT_COUNT;

    uint32_t fbWidth;
    uint32_t fbHeight;
    uint32_t resourceSetCount;
};
struct PipelineData
{
    REI_RootSignature* rootSignature;
    REI_Pipeline*      pipeline;
};

struct GLTF_State
{
    GLTF_StateDesc desc;
    REI_Renderer*      renderer;
    REI_RL_State*      loader;
    PipelineData       meshPipelineData;
    REI_Buffer**       sceneUniBuffers;
    void**             sceneUniBuffersAddr;
    REI_Pipeline*      currentPipeline;
    REI_DescriptorTableArray* meshDescriptorSet;
    REI_DescriptorTableArray* sceneDescriptorSet;
    REI_Sampler*       sampler;
    REI_Texture*       defaultTexture;
    uint32_t           setIndex;
    int32_t            lastMeshDescriptor;
};
struct Scene_Uniforms
{
    rm_mat4 uVP;
};
struct Model_Uniforms
{
    rm_mat4 uModel;
};

static REI_CmdPool*   cmdPool[FRAME_COUNT];
static REI_Cmd*       pCmds[FRAME_COUNT];
static REI_Texture*   depthBuffer;
static GLTF_State* state;
static SimpleCamera   camera;
static GLTF_Model model;
static REI_RL_State*  resourceLoader;

#include "shaderbin/gltf_mesh_vs.bin.h"
#include "shaderbin/gltf_mesh_ps.bin.h"

static void create_pipeline_data(
    GLTF_State* state, uint32_t vertexAttribCount, REI_VertexAttrib* vertexAttribs, REI_ShaderDesc* shaderDesc, uint32_t shaderCount, PipelineData* pipelineData)
{
    REI_Shader* shaders[MAX_SHADER_COUNT] = {};
    REI_addShaders(state->renderer, shaderDesc, shaderCount, shaders);

    //create sampler

    REI_SamplerDesc samplerDesc = { REI_FILTER_LINEAR,
                                    REI_FILTER_LINEAR,
                                    REI_MIPMAP_MODE_LINEAR,
                                    REI_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    REI_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    REI_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    REI_CMP_NEVER,
                                    0.0f,
                                    1.0f };
    REI_addSampler(state->renderer, &samplerDesc, &state->sampler);

    //create root signature

    REI_RootSignatureDesc rootSigDesc = {};

    REI_DescriptorBinding binding[4] = {};
    binding[0].descriptorCount = 1;
    binding[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
    binding[0].reg = 0;
    binding[0].binding = 0;

    binding[1].descriptorCount = 1;
    binding[1].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
    binding[1].reg = 0;
    binding[1].binding = 0;

    binding[2].descriptorCount = 1;
    binding[2].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
    binding[2].reg = 0;
    binding[2].binding = 0;

    binding[3].descriptorCount = 1;
    binding[3].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
    binding[3].reg = 1;
    binding[3].binding = 1;

    REI_DescriptorTableLayout setLayout[3] = {};
    setLayout[0].slot = REI_DESCRIPTOR_TABLE_SLOT_0;
    setLayout[0].bindingCount = 1;
    setLayout[0].pBindings = binding;
    setLayout[0].stageFlags = REI_SHADER_STAGE_VERT;

    setLayout[1].slot = REI_DESCRIPTOR_TABLE_SLOT_1;
    setLayout[1].bindingCount = 1;
    setLayout[1].pBindings = binding + 1;
    setLayout[1].stageFlags = REI_SHADER_STAGE_VERT;

    setLayout[2].slot = REI_DESCRIPTOR_TABLE_SLOT_2;
    setLayout[2].bindingCount = 2;
    setLayout[2].pBindings = binding + 2;
    setLayout[2].stageFlags = REI_SHADER_STAGE_FRAG;

    REI_PushConstantRange pConst = {};
    pConst.offset = 0;
    pConst.size = sizeof(uint32_t);
    pConst.stageFlags = REI_SHADER_STAGE_VERT;

    REI_StaticSamplerBinding staticSamplerBinding = {};
    staticSamplerBinding.descriptorCount = 1;
    staticSamplerBinding.reg = 0;
    staticSamplerBinding.binding = 0;
    staticSamplerBinding.ppStaticSamplers = &state->sampler;

    rootSigDesc.pipelineType = REI_PIPELINE_TYPE_GRAPHICS;
    rootSigDesc.tableLayoutCount = 3;
    rootSigDesc.pTableLayouts = setLayout;
    rootSigDesc.pushConstantRangeCount = 1;
    rootSigDesc.pPushConstantRanges = &pConst;
    rootSigDesc.staticSamplerBindingCount = 1;
    rootSigDesc.staticSamplerSlot = REI_DESCRIPTOR_TABLE_SLOT_3;
    rootSigDesc.staticSamplerStageFlags = REI_SHADER_STAGE_FRAG;
    rootSigDesc.pStaticSamplerBindings = &staticSamplerBinding;

    REI_addRootSignature(state->renderer, &rootSigDesc, &pipelineData->rootSignature);

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

    REI_DepthStateDesc depthStateDesc{};
    depthStateDesc.depthTestEnable = true;
    depthStateDesc.depthWriteEnable = true;
    depthStateDesc.depthCmpFunc = REI_CMP_LEQUAL;

    REI_Format       colorFormat = (REI_Format)state->desc.colorFormat;
    REI_PipelineDesc pipelineDesc = {};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
    REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;
    graphicsDesc.renderTargetCount = 1;
    graphicsDesc.pColorFormats = &colorFormat;
    graphicsDesc.sampleCount = state->desc.sampleCount;
    graphicsDesc.depthStencilFormat = state->desc.depthStencilFormat;
    graphicsDesc.pDepthState = &depthStateDesc;
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
static void destroy_pipeline_data(GLTF_State* state, PipelineData* pipelineData)
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

static GLTF_State* gltf_init(REI_Renderer* renderer, REI_RL_State* loader, GLTF_StateDesc* info)
{
    GLTF_State* state = (GLTF_State*)malloc(sizeof(GLTF_State));

    if (!state)
        return NULL;
    memset(state, 0, sizeof(GLTF_State));
    state->desc = *info;
    state->renderer = renderer;
    state->loader = loader;

    //create pipeline data

    const size_t     vertexAttribCount = 4;
    REI_VertexAttrib vertexAttribs[vertexAttribCount] = {};

    vertexAttribs[0].semantic = REI_SEMANTIC_POSITION0;
    vertexAttribs[0].offset = REI_OFFSETOF(GLTF_Vertex, position);
    vertexAttribs[0].location = 0;
    vertexAttribs[0].format = REI_FMT_R32G32B32_SFLOAT;

    vertexAttribs[1].semantic = REI_SEMANTIC_COLOR0;
    vertexAttribs[1].offset = REI_OFFSETOF(GLTF_Vertex, color);
    vertexAttribs[1].location = 1;
    vertexAttribs[1].format = REI_FMT_R32G32B32_SFLOAT;

    vertexAttribs[2].semantic = REI_SEMANTIC_NORMAL;
    vertexAttribs[2].offset = REI_OFFSETOF(GLTF_Vertex, normal);
    vertexAttribs[2].location = 2;
    vertexAttribs[2].format = REI_FMT_R32G32B32_SFLOAT;

    vertexAttribs[3].semantic = REI_SEMANTIC_TEXCOORD0;
    vertexAttribs[3].offset = REI_OFFSETOF(GLTF_Vertex, texUV);
    vertexAttribs[3].location = 3;
    vertexAttribs[3].format = REI_FMT_R32G32_SFLOAT;

    REI_ShaderDesc meshShaderDesc[MAX_SHADER_COUNT] = {
        { REI_SHADER_STAGE_VERT, (uint8_t*)gltf_vs_bytecode, sizeof(gltf_vs_bytecode) },
        { REI_SHADER_STAGE_FRAG, (uint8_t*)gltf_ps_bytecode, sizeof(gltf_ps_bytecode) }
    };
    create_pipeline_data(state, vertexAttribCount, vertexAttribs, meshShaderDesc, 2, &state->meshPipelineData);

    //create uniform buffer

    REI_BufferDesc sceneUniBuffersDesc = {};
    sceneUniBuffersDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER;
    sceneUniBuffersDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    sceneUniBuffersDesc.size = sizeof(Scene_Uniforms);
    sceneUniBuffersDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    state->sceneUniBuffers = (REI_Buffer**)calloc(state->desc.resourceSetCount, sizeof(REI_Buffer*));
    state->sceneUniBuffersAddr = (void**)calloc(state->desc.resourceSetCount, sizeof(void*));
    sceneUniBuffersDesc.structStride = sizeof(Scene_Uniforms);
    sceneUniBuffersDesc.elementCount = sceneUniBuffersDesc.size / sceneUniBuffersDesc.structStride;
    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &sceneUniBuffersDesc, &state->sceneUniBuffers[i]);
        REI_mapBuffer(state->renderer, state->sceneUniBuffers[i], &state->sceneUniBuffersAddr[i]);
    }

    //create default texture

    REI_TextureDesc textureDesc = {};
    textureDesc.flags = REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
    textureDesc.width = 2;
    textureDesc.height = 2;
    textureDesc.format = REI_FMT_R8G8B8A8_UNORM;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;

    REI_addTexture(state->renderer, &textureDesc, &state->defaultTexture);

    //update default texture

    uint8_t data[16] = { 255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 255, 0, 255, 255, 255, 0 };

    REI_RL_TextureUpdateDesc updateDesc{};
    updateDesc.pTexture = state->defaultTexture;
    updateDesc.pRawData = data;
    updateDesc.format = REI_FMT_R8G8B8A8_UNORM;
    updateDesc.width = 2;
    updateDesc.height = 2;
    updateDesc.depth = 1;
    updateDesc.arrayLayer = 0;
    updateDesc.mipLevel = 0;    // for desc.mipLevels;
    updateDesc.x = 0;
    updateDesc.y = 0;
    updateDesc.z = 0;
    updateDesc.endState = REI_RESOURCE_STATE_SHADER_RESOURCE;
    REI_RL_updateResource(state->loader, &updateDesc);

    //create default mesh descriptor

    REI_DescriptorTableArrayDesc meshDescriptorSetDesc = {};
    meshDescriptorSetDesc.pRootSignature = state->meshPipelineData.rootSignature;
    meshDescriptorSetDesc.maxTables = 1;
    meshDescriptorSetDesc.slot = meshDescriptorSetIndex;
    REI_addDescriptorTableArray(renderer, &meshDescriptorSetDesc, &state->meshDescriptorSet);

    //update default mesh descriptor

    {
        REI_DescriptorData descrUpdate[2] = {};
        descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
        descrUpdate[0].count = 1;
        descrUpdate[0].descriptorIndex = 0;    //uBaseColorTexture;
        descrUpdate[0].ppTextures = &state->defaultTexture;
        descrUpdate[0].tableIndex = 0;

        descrUpdate[1].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
        descrUpdate[1].count = 1;
        descrUpdate[1].descriptorIndex = 1;    //uMetallicRoughnessTexture;
        descrUpdate[1].ppTextures = &state->defaultTexture;
        descrUpdate[1].tableIndex = 0;

        REI_updateDescriptorTableArray(renderer, state->meshDescriptorSet, 2, descrUpdate);
    }

    //create scene descriptor set

    REI_DescriptorTableArrayDesc sceneDescriptorSetDesc = {};
    sceneDescriptorSetDesc.pRootSignature = state->meshPipelineData.rootSignature;
    sceneDescriptorSetDesc.maxTables = state->desc.resourceSetCount;
    REI_addDescriptorTableArray(state->renderer, &sceneDescriptorSetDesc, &state->sceneDescriptorSet);

    //update scene descriptor set

    {
        REI_DescriptorData descrUpdate[1] = {};
        descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
        descrUpdate[0].count = 1;
        descrUpdate[0].descriptorIndex = 0;    //uSceneUniforms;
        
        for (uint32_t i = 0; i < state->desc.resourceSetCount; i++)
        {
            descrUpdate[0].ppBuffers = &state->sceneUniBuffers[i];
            descrUpdate[0].tableIndex = i;
            REI_updateDescriptorTableArray(
                state->renderer, state->sceneDescriptorSet, 1, descrUpdate);
        }
        
    }

    return state;
}

static void gltf_shutdown(GLTF_State* state)
{
    REI_removeDescriptorTableArray(state->renderer, state->meshDescriptorSet);
    REI_removeDescriptorTableArray(state->renderer, state->sceneDescriptorSet);
    REI_removeSampler(state->renderer, state->sampler);
    REI_removeTexture(state->renderer, state->defaultTexture);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->sceneUniBuffers[i]);
    }
    free(state->sceneUniBuffers);
    free(state->sceneUniBuffersAddr);

    destroy_pipeline_data(state, &state->meshPipelineData);

    free(state);
}
static void gltf_draw_model(GLTF_State* state, REI_Cmd* pCmd, const rm_mat4& vp, GLTF_Model& model)
{
    if (state->currentPipeline != state->meshPipelineData.pipeline)
    {
        state->currentPipeline = state->meshPipelineData.pipeline;

        REI_cmdBindPipeline(pCmd, state->meshPipelineData.pipeline);

        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
        REI_cmdBindDescriptorTable(pCmd, state->setIndex, state->sceneDescriptorSet);
        REI_cmdBindDescriptorTable(pCmd, 0, state->meshDescriptorSet);

        state->lastMeshDescriptor = -1;
    }

    //setup camera

    Scene_Uniforms* uniform_ptr = ((Scene_Uniforms*)state->sceneUniBuffersAddr[state->setIndex]);
    uniform_ptr->uVP = vp;

    //start draw meshes

    if (model.meshes.empty())
    {
        return;
    }

    //bind buffers

    uint64_t vertex_offset = 0;
    REI_cmdBindVertexBuffer(pCmd, 1, &model.vertexBuffer, &vertex_offset);
    REI_cmdBindIndexBuffer(pCmd, model.indexBuffer, 0);


    //draw meshes
    REI_cmdBindDescriptorTable(pCmd, 0, model.descriptorSet);
    for (size_t i = 0; i < model.meshes.size(); i++)
    {
        GLTF_Mesh& mesh = model.meshes[i];
        if (state->lastMeshDescriptor != mesh.descriptorIndex)
        {
            if (mesh.descriptorIndex >= 0)
            {
                state->lastMeshDescriptor = mesh.descriptorIndex;
                REI_cmdBindDescriptorTable(pCmd, 0, model.meshDescriptorSets[mesh.descriptorIndex]);
            }
        }

        REI_cmdBindPushConstants(
            pCmd, state->meshPipelineData.rootSignature, REI_SHADER_STAGE_VERT, 0, sizeof(uint32_t), &i);

        REI_cmdDrawIndexed(pCmd, mesh.indicesLength, mesh.firstIndex, mesh.firstVertex);
    }
}

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

void sample_on_swapchain_init(const REI_SwapchainDesc* swapchainDesc)
{
    REI_TextureDesc depthRTDesc{};
    depthRTDesc.clearValue.ds.depth = 1.0f;
    depthRTDesc.clearValue.ds.stencil = 0;
    depthRTDesc.format = REI_FMT_D32_SFLOAT;
    depthRTDesc.width = swapchainDesc->width;
    depthRTDesc.height = swapchainDesc->height;
    depthRTDesc.sampleCount = swapchainDesc->sampleCount;
    depthRTDesc.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET;
    REI_addTexture(renderer, &depthRTDesc, &depthBuffer);
    
    REI_RL_addResourceLoader(renderer, nullptr, &resourceLoader);

    GLTF_StateDesc srInfo = { swapchainDesc->colorFormat,
                              depthRTDesc.format,
                              swapchainDesc->sampleCount,
                              swapchainDesc->width,
                              swapchainDesc->height,
                              FRAME_COUNT };

    state = gltf_init(renderer, resourceLoader, &srInfo);
    
    const char* modelList[] = { "gltf/bunny/scene.gltf", "gltf/map/scene.gltf", "gltf/2CylinderEngine.glb" };

    model = gltf_load_model(modelList[2], renderer, resourceLoader, state->meshPipelineData.rootSignature);

    SimpleCameraProjDesc projDesc = {};
    projDesc.proj_type = SimpleCameraProjInfiniteVulkan;
    projDesc.y_fov = RM_PI_2;
    projDesc.aspect = (float)swapchainDesc->width / swapchainDesc->height;
    projDesc.z_near = 0.1f;

    SimpleCamera_init(&camera, projDesc);
    SimpleCamera_rotate(&camera, 0, 0);
}
void sample_on_swapchain_fini()
{
    gltf_destroy_model(renderer,model);
    gltf_shutdown(state);
    REI_RL_removeResourceLoader(resourceLoader);
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
    loadActions.clearColorValues[0].rt.r = 0.0f;
    loadActions.clearColorValues[0].rt.g = 0.1f;
    loadActions.clearColorValues[0].rt.b = 0.3f;
    loadActions.clearColorValues[0].rt.a = 1.0f;
    loadActions.loadActionDepth = REI_LOAD_ACTION_CLEAR;
    loadActions.clearDepth.ds.depth = 1.0f;
    loadActions.clearDepth.ds.stencil = 0;
    REI_cmdBindRenderTargets(cmd, 1, &renderTarget, depthBuffer, &loadActions, NULL, NULL, 0, 0);
    REI_cmdSetViewport(cmd, 0.0f, 0.0f, (float)frameData->bbWidth, (float)frameData->bbHeight, 0.0f, 1.0f);
    REI_cmdSetScissor(cmd, 0, 0, frameData->bbWidth, frameData->bbHeight);

    //setup Render

    state->setIndex = frameData->setIndex;
    state->currentPipeline = NULL;

    //setup camera

    camera.p = RM_VALUE(rm_vec3, 0, 0, 0);
    SimpleCamera_rotate(&camera, 1.5f * frameData->dt / 1000000000.0f, 0.0f);
   
    SimpleCamera_move(&camera, 0, 5, -1500);

    rm_mat4 camMVP = SimpleCamera_buildViewProj(&camera);

    //draw

    gltf_draw_model(state, cmd, camMVP, model);
 
    //end draw

    sample_cmdPrepareBackbuffer(cmd, renderTarget, REI_RESOURCE_STATE_RENDER_TARGET);
    barriers[1] = { depthBuffer, REI_RESOURCE_STATE_DEPTH_WRITE, REI_RESOURCE_STATE_COMMON };
    REI_cmdResourceBarrier(cmd, 0, nullptr, 1, &barriers[1]);
    REI_endCmd(cmd);
    
    sample_submit(cmd);
}

using GLTF_Accessors = cgltf_accessor * [(uint32_t)10];
static void gltf_traverse_node(
    const cgltf_data* gltfData, const cgltf_node* node, std::vector<Model_Uniforms>& meshMatrices,
    const rm_mat4 matrix = rm_mat4_identity())
{
    if (node == NULL)
    {
        return;
    }

    // Get translation if it exists

    const rm_vec3 translation =
        node->has_translation ? RM_VALUE(rm_vec3, node->translation[0], node->translation[1], node->translation[2])
                              : RM_VALUE(rm_vec3, 0.0f, 0.0f, 0.0f);

    // Get quaternion if it exists
    const rm_quat rotation =
        node->has_rotation
            ? RM_VALUE(rm_quat, node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3])
            : rm_quat_axis_rotation(RM_VALUE(rm_vec3, 0.0f, 0.0f, 1.0f), 0.0f);

    // Get scale if it exists
    const rm_vec3 scale = node->has_scale ? RM_VALUE(rm_vec3, node->scale[0], node->scale[1], node->scale[2])
                                          : RM_VALUE(rm_vec3, 1.0f, 1.0f, 1.0f);
    // Get matrix if it exists
    rm_mat4 model_mat;
    if (node->has_matrix)
    {
        const cgltf_float* m = node->matrix;

        model_mat = RM_VALUE(
            rm_mat4, m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8], m[9], m[10], m[11], m[12], m[13], m[14],
            m[15]);
    }
    else
    {
        model_mat = RM_VALUE(
            rm_mat4, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Multiply all matrices together
    rm_mat4 matNextNode = rm_mat4_mul(matrix, model_mat);
    matNextNode = rm_mat4_mul(matNextNode, rm_mat4_translation(translation));
    matNextNode = rm_mat4_mul(matNextNode, rm_mat4_from_quat(rotation));
    matNextNode = rm_mat4_mul(matNextNode, rm_mat4_scaling(scale));

    // Check if the node contains a mesh and if it does load it
    const cgltf_mesh* mesh = node->mesh;
    if (mesh != NULL)
    {
        meshMatrices[mesh - gltfData->meshes].uModel = matNextNode;
    }
    cgltf_node** childrens = node->children;
    cgltf_size   children_count = node->children_count;
    // Check if the node has children, and if it does, apply this function to them with the matNextNode
    if (childrens != NULL)
    {
        for (size_t i = 0; i < children_count; i++)
        {
            gltf_traverse_node(gltfData, childrens[i], meshMatrices, matNextNode);
        }
    }
}

static REI_Texture* gltf_model_get_texture(GLTF_Model& model, cgltf_data* gltfData, cgltf_texture* texture)
{
    if (texture == NULL)
    {
        return model.textures[0];
    }

    if (texture->image == NULL)
    {
        return model.textures[0];
    }

    return model.textures[texture->image - gltfData->images];
}

GLTF_Model
    gltf_load_model(const char* file, REI_Renderer* renderer, REI_RL_State* loader, REI_RootSignature* rootSignature)
{
    GLTF_Model model = {};

    std::string path(256, '0');

    sample_get_path(DIRECTORY_DATA, file, (char*)path.c_str(), path.size());

    size_t vertCount = 0;
    size_t indicesCount = 0;
    size_t firstVertex = 0;
    size_t firstIndex = 0;

    cgltf_options options = {};

    memset(&options, 0, sizeof(cgltf_options));

    cgltf_data*  gltfData = NULL;
    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &gltfData);
    if (result != cgltf_result_success)
    {
        printf("Failed to parse GLTF_ file.\n");
        return model;
    }

    result = cgltf_load_buffers(&options, gltfData, path.c_str());

    if (result != cgltf_result_success)
    {
        cgltf_free(gltfData);

        printf("Failed to load GLTF_ buffers.\n");
        return model;
    }

    //get file directory

    std::size_t lastSlashPos = path.rfind('/');
    std::string fileDirectory = lastSlashPos != std::string::npos ? path.substr(0, lastSlashPos + 1) : "";

    //create REI_Buffers

    for (size_t i = 0; i < gltfData->meshes_count; i++)
    {
        const cgltf_mesh&      mesh = gltfData->meshes[i];
        const cgltf_primitive& primitive = mesh.primitives[0];
        if (primitive.attributes_count <= 0)
        {
            continue;
        }
        vertCount += primitive.attributes[0].data->count;
        indicesCount += primitive.indices->count;
    }

    REI_BufferDesc vertexBufDesc = {};
    vertexBufDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER_RAW | REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vertexBufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vertexBufDesc.size = vertCount * sizeof(GLTF_Vertex);
    vertexBufDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    vertexBufDesc.vertexStride = sizeof(GLTF_Vertex);
    vertexBufDesc.elementCount = vertCount;
    vertexBufDesc.structStride = 0;
    vertexBufDesc.format = REI_Format::REI_FMT_R32_SFLOAT;

    REI_BufferDesc indexBufDesc = {};
    indexBufDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER_RAW | REI_DESCRIPTOR_TYPE_INDEX_BUFFER;
    indexBufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    indexBufDesc.size = indicesCount * sizeof(uint32_t);
    indexBufDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    indexBufDesc.elementCount = indicesCount;
    indexBufDesc.structStride = 0;
    indexBufDesc.format = REI_Format::REI_FMT_R32_UINT;

    REI_addBuffer(renderer, &vertexBufDesc, &model.vertexBuffer);
    REI_mapBuffer(renderer, model.vertexBuffer, &model.vertexBufferAddr);

    REI_addBuffer(renderer, &indexBufDesc, &model.indexBuffer);
    REI_mapBuffer(renderer, model.indexBuffer, &model.indexBufferAddr);

    //load textures

    for (size_t i = 0; i < gltfData->images_count; i++)
    {
        char* texturePath = gltfData->images[i].uri;

        std::string fullPath = fileDirectory + texturePath;

        int      width, height, channels;
        stbi_uc* data = stbi_load(fullPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);

        if (data == NULL)
        {
            continue;
        }

        REI_Texture* rei_tex;

        REI_TextureDesc textureDesc = {};
        textureDesc.flags = REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT;
        textureDesc.width = width;
        textureDesc.height = height;
        textureDesc.format = REI_FMT_R8G8B8A8_UNORM;
        textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;

        REI_addTexture(renderer, &textureDesc, &rei_tex);

        REI_RL_TextureUpdateDesc updateDesc{};
        updateDesc.pTexture = rei_tex;
        updateDesc.pRawData = data;

        updateDesc.format = REI_FMT_R8G8B8A8_UNORM;
        updateDesc.width = width;
        updateDesc.height = height;
        updateDesc.depth = 1;
        updateDesc.arrayLayer = 0;
        updateDesc.mipLevel = 0;    // for desc.mipLevels;
        updateDesc.x = 0;
        updateDesc.y = 0;
        updateDesc.z = 0;
        updateDesc.endState = REI_RESOURCE_STATE_SHADER_RESOURCE;
        REI_RL_updateResource(loader, &updateDesc);

        stbi_image_free(data);

        model.textures.push_back(rei_tex);
    }

    //make materials / descriptor sets

    if (!model.textures.empty())
    {
        for (size_t i = 0; i < gltfData->materials_count; i++)
        {
            cgltf_material& mat = gltfData->materials[i];

            REI_Texture* bTex =
                gltf_model_get_texture(model, gltfData, mat.pbr_metallic_roughness.base_color_texture.texture);

            REI_Texture* mrTex =
                gltf_model_get_texture(
                model, gltfData, mat.pbr_metallic_roughness.metallic_roughness_texture.texture);

            REI_DescriptorTableArray* descriptorSet;

            REI_DescriptorTableArrayDesc meshDescriptorSetDesc = {};
            meshDescriptorSetDesc.pRootSignature = rootSignature;
            meshDescriptorSetDesc.maxTables = 1;
            meshDescriptorSetDesc.slot = meshDescriptorSetIndex;
            REI_addDescriptorTableArray(renderer, &meshDescriptorSetDesc, &descriptorSet);

            REI_DescriptorData descrUpdate[2] = {};
            descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
            descrUpdate[0].count = 1;
            descrUpdate[0].descriptorIndex = 0;    //uBaseColorTexture
            descrUpdate[0].ppTextures = &bTex;
            descrUpdate[0].tableIndex = 0;

            descrUpdate[1].descriptorType = REI_DESCRIPTOR_TYPE_TEXTURE;
            descrUpdate[1].count = 1;
            descrUpdate[1].descriptorIndex = 1;    //uMetallicRoughnessTexture
            descrUpdate[1].ppTextures = &mrTex;
            descrUpdate[1].tableIndex = 0;

            REI_updateDescriptorTableArray(renderer, descriptorSet, 2, descrUpdate);

            model.meshDescriptorSets.push_back(descriptorSet);
        }
    }

    //load meshes

    for (size_t i = 0; i < gltfData->meshes_count; i++)
    {
        GLTF_Mesh newMesh = {};

        const cgltf_primitive& primitive = gltfData->meshes[i].primitives[0];

        if (primitive.attributes_count <= 0)
        {
            continue;
        }

        //Find Mesh Attributes

        GLTF_Accessors   acc = {};
        const cgltf_size count = primitive.attributes_count;

        for (size_t i = 0; i < count; i++)
        {
            const cgltf_attribute& attr = primitive.attributes[i];

            acc[attr.type] = attr.data;
        }

        const cgltf_accessor* positionAccessor = acc[cgltf_attribute_type::cgltf_attribute_type_position];
        const cgltf_accessor* colorAccessor = acc[cgltf_attribute_type::cgltf_attribute_type_color];
        const cgltf_accessor* normalAccessor = acc[cgltf_attribute_type::cgltf_attribute_type_normal];
        const cgltf_accessor* texCoordAccessor = acc[cgltf_attribute_type::cgltf_attribute_type_texcoord];
        const cgltf_accessor* indexAccessor = primitive.indices;

        if (positionAccessor == NULL || indexAccessor == NULL)
        {
            continue;
        }

        //update first indices verts

        size_t meshVertCount = positionAccessor->count;
        size_t meshIndicesCount = primitive.indices->count;

        REI_ASSERT(firstIndex < indicesCount);
        REI_ASSERT(firstVertex < vertCount);

        newMesh.firstIndex = (uint32_t)firstIndex;
        newMesh.firstVertex = (uint32_t)firstVertex;
        newMesh.indicesLength = (uint32_t)meshIndicesCount;

        uint32_t*    indexDataPtr = ((uint32_t*)model.indexBufferAddr) + firstIndex;
        GLTF_Vertex* vertexDataPtr = ((GLTF_Vertex*)model.vertexBufferAddr) + firstVertex;

        firstIndex += meshIndicesCount;
        firstVertex += meshVertCount;

        //copy vertex index to rei buffers

        for (uint32_t i = 0; i < newMesh.indicesLength; i++)
        {
            indexDataPtr[i] = (uint32_t)cgltf_accessor_read_index(indexAccessor, i);
        }

        cgltf_float bufFloat[4];
        for (size_t j = 0; j < meshVertCount; ++j)
        {
            GLTF_Vertex& vertexRef = vertexDataPtr[j];
            vertexRef = {};

            cgltf_accessor_read_float(positionAccessor, j, bufFloat, 3);
            vertexRef.position.x = bufFloat[0];
            vertexRef.position.y = bufFloat[1];
            vertexRef.position.z = bufFloat[2];

            if (colorAccessor)
            {
                cgltf_accessor_read_float(colorAccessor, j, bufFloat, 3);
                vertexRef.color.x = bufFloat[0];
                vertexRef.color.y = bufFloat[1];
                vertexRef.color.z = bufFloat[2];
            }

            if (normalAccessor)
            {
                cgltf_accessor_read_float(normalAccessor, j, bufFloat, 3);
                vertexRef.normal.x = bufFloat[0];
                vertexRef.normal.y = bufFloat[1];
                vertexRef.normal.z = bufFloat[2];
            }

            if (texCoordAccessor)
            {
                cgltf_accessor_read_float(texCoordAccessor, j, bufFloat, 2);
                vertexRef.texUV.x = bufFloat[0];
                vertexRef.texUV.y = bufFloat[1];
            }
        }

        //set textures

        newMesh.descriptorIndex = UINT32_MAX;
        if (!model.meshDescriptorSets.empty() && primitive.material != NULL)
        {
            newMesh.descriptorIndex = (uint32_t)(primitive.material - gltfData->materials);
        }

        model.meshes.push_back(newMesh);
    }

    //TraverseNode

    const cgltf_node* rootNode = gltfData->nodes;
    std::vector<Model_Uniforms> meshMatrices;
    meshMatrices.resize(model.meshes.size());

    gltf_traverse_node(gltfData, rootNode, meshMatrices);

    //Create matrices uniform baffer

    REI_BufferDesc modelUniBufDesc = {};
    modelUniBufDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER;
    modelUniBufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    modelUniBufDesc.size = meshMatrices.size() * sizeof(Model_Uniforms);
    modelUniBufDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    modelUniBufDesc.structStride = sizeof(Model_Uniforms);
    modelUniBufDesc.elementCount = modelUniBufDesc.size / modelUniBufDesc.structStride;

    REI_addBuffer(renderer, &modelUniBufDesc, &model.uniBuffer);
    REI_mapBuffer(renderer, model.uniBuffer, &model.uniBufferAddr);
    memcpy(model.uniBufferAddr, meshMatrices.data(), meshMatrices.size() * sizeof(Model_Uniforms));

    //Create model descriptor set

    REI_DescriptorTableArrayDesc modelDescriptorSetDesc = {};
    modelDescriptorSetDesc.pRootSignature = rootSignature;
    modelDescriptorSetDesc.maxTables = 1;
    modelDescriptorSetDesc.slot = modelDescriptorSetIndex;
    REI_addDescriptorTableArray(renderer, &modelDescriptorSetDesc, &model.descriptorSet);

    REI_DescriptorData descrUpdate[1] = {};
    descrUpdate[0].descriptorType = REI_DESCRIPTOR_TYPE_BUFFER;
    descrUpdate[0].count = 1;
    descrUpdate[0].descriptorIndex = 0;    //uModelUniforms;
    descrUpdate[0].ppBuffers = &model.uniBuffer;
    descrUpdate[0].tableIndex = 0;

    REI_updateDescriptorTableArray(renderer, model.descriptorSet, 1, descrUpdate);
    cgltf_free(gltfData);
    return model;
}

void gltf_destroy_model(REI_Renderer* renderer, GLTF_Model& model)
{
    REI_unmapBuffer(renderer, model.vertexBuffer);
    REI_removeBuffer(renderer, model.vertexBuffer);
    REI_unmapBuffer(renderer, model.indexBuffer);
    REI_removeBuffer(renderer, model.indexBuffer);
    REI_unmapBuffer(renderer, model.uniBuffer);
    REI_removeBuffer(renderer, model.uniBuffer);
    REI_removeDescriptorTableArray(renderer, model.descriptorSet);

    for (size_t i = 0; i < model.textures.size(); i++)
    {
        REI_removeTexture(renderer, model.textures[i]);
    }

    for (size_t i = 0; i < model.meshDescriptorSets.size(); i++)
    {
        REI_removeDescriptorTableArray(renderer, model.meshDescriptorSets[i]);
    }
}