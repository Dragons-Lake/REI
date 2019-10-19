#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include "windows.h"
#endif
#define FONTSTASH_IMPLEMENTATION

#include "REI_fontstash.h"

#ifdef _WIN32
#    define strcpy strcpy_s
#endif

struct REI_Fontstash_State
{
    REI_Fontstash_Desc  desc;
    REI_Renderer*       renderer;
    REI_Queue*          queue;
    REI_RL_State*       loader;
    REI_RootSignature*  rootSignature;
    REI_DescriptorSet*  descriptorSet;
    REI_Pipeline*       pipeline;
    REI_Sampler*        fontSampler;
    REI_Texture*        fontTexture;
    REI_Shader*         shader;
    REI_Buffer**        buffers;
    void**              buffersAddr;
    uint32_t            setIndex;
    uint32_t            vertexCount;
    uint32_t            fontTextureWidth;
};

// fontstash.vert, compiled with:
// # glslangValidator -V -x -o fontstash.vert.u32.h fontstash.vert
// see sources/shaders/build.ninja
static uint32_t vert_spv[] = {
#include "spv/fontstash.vert.u32.h"
};

// fontstash.frag, compiled with:
// # glslangValidator -V -x -o fontstash.frag.u32.h fontstash.frag
// see sources/shaders/build.ninja
static uint32_t frag_spv[] = {
#include "spv/fontstash.frag.u32.h"
};

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
    textureDesc.depth = 1;
    textureDesc.arraySize = 1;
    textureDesc.mipLevels = 1;
    textureDesc.format = REI_FMT_R8_UNORM;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(state->renderer, &textureDesc, &state->fontTexture);

    state->fontTextureWidth = width;

    REI_DescriptorData params[1] = {};
    params[0].pName = "uTexture";
    params[0].ppTextures = &state->fontTexture;
    REI_updateDescriptorSet(state->renderer, state->descriptorSet, 0, 1, params);

    return 1;
}

static int REI_Fontstash_renderCreate(void* userPtr, int width, int height)
{
    REI_Fontstash_State* state = (REI_Fontstash_State*)userPtr;
    REI_ShaderDesc       shaderDesc{};
    shaderDesc.stages = REI_SHADER_STAGE_VERT | REI_SHADER_STAGE_FRAG;
    shaderDesc.vert = { (uint8_t*)vert_spv, sizeof(vert_spv) };
    shaderDesc.frag = { (uint8_t*)frag_spv, sizeof(frag_spv) };
    REI_addShader(state->renderer, &shaderDesc, &state->shader);

    REI_SamplerDesc samplerDesc = { REI_FILTER_LINEAR,
                                    REI_FILTER_LINEAR,
                                    REI_MIPMAP_MODE_LINEAR,
                                    REI_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    REI_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    REI_ADDRESS_MODE_CLAMP_TO_EDGE,
                                    0.0f,
                                    1.0f,
                                    REI_CMP_NEVER };
    REI_addSampler(state->renderer, &samplerDesc, &state->fontSampler);

    const char*           staticSampNames[] = { "uSampler" };
    REI_RootSignatureDesc rootSigDesc = {};
    rootSigDesc.ppShaders = &state->shader;
    rootSigDesc.shaderCount = 1;
    rootSigDesc.ppStaticSamplerNames = staticSampNames;
    rootSigDesc.ppStaticSamplers = &state->fontSampler;
    rootSigDesc.staticSamplerCount = 1;
    REI_addRootSignature(state->renderer, &rootSigDesc, &state->rootSignature);

    REI_DescriptorSetDesc descriptorSetDesc = {};
    descriptorSetDesc.pRootSignature = state->rootSignature;
    descriptorSetDesc.maxSets = 1;
    descriptorSetDesc.setIndex = REI_DESCRIPTOR_SET_INDEX_0;
    REI_addDescriptorSet(state->renderer, &descriptorSetDesc, &state->descriptorSet);

    REI_VertexLayout vertexLayout{};
    vertexLayout.attribCount = 3;
    vertexLayout.attribs[0].semanticNameLength = 4;
    strcpy(vertexLayout.attribs[0].semanticName, "aPos");
    vertexLayout.attribs[0].offset = OFFSETOF(FontVert, pos);
    vertexLayout.attribs[0].location = 0;
    vertexLayout.attribs[0].format = REI_FMT_R32G32_SFLOAT;
    vertexLayout.attribs[1].semanticNameLength = 3;
    strcpy(vertexLayout.attribs[1].semanticName, "aUV");
    vertexLayout.attribs[1].offset = OFFSETOF(FontVert, uv);
    vertexLayout.attribs[1].location = 1;
    vertexLayout.attribs[1].format = REI_FMT_R32G32_SFLOAT;
    vertexLayout.attribs[2].semanticNameLength = 6;
    strcpy(vertexLayout.attribs[2].semanticName, "aColor");
    vertexLayout.attribs[2].offset = OFFSETOF(FontVert, col);
    vertexLayout.attribs[2].location = 2;
    vertexLayout.attribs[2].format = REI_FMT_R8G8B8A8_UNORM;

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

    REI_PipelineDesc pipelineDesc = {};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
    REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;
    graphicsDesc.renderTargetCount = 1;
    graphicsDesc.pColorFormats = &state->desc.colorFormat;
    graphicsDesc.sampleCount = state->desc.sampleCount;
    graphicsDesc.depthStencilFormat = state->desc.depthStencilFormat;
    graphicsDesc.pRootSignature = state->rootSignature;
    graphicsDesc.pShaderProgram = state->shader;
    graphicsDesc.pVertexLayout = &vertexLayout;
    graphicsDesc.pRasterizerState = &rasterizerStateDesc;
    graphicsDesc.pBlendState = &blendState;
    REI_addPipeline(state->renderer, &pipelineDesc, &state->pipeline);

    REI_BufferDesc vbDesc = {};
    vbDesc.descriptors = REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.vertexStride = sizeof(FontVert);
    vbDesc.size = state->desc.vertexBufferSize;
    vbDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

    state->buffers = (REI_Buffer**)REI_calloc(state->desc.resourceSetCount, sizeof(REI_Buffer*));
    state->buffersAddr = (void**)REI_calloc(state->desc.resourceSetCount, sizeof(void*));
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
    REI_free(state->buffers);
    REI_free(state->buffersAddr);

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
    if (state->shader)
    {
        REI_removeShader(state->renderer, state->shader);
        state->shader = NULL;
    }
    if (state->descriptorSet)
    {
        REI_removeDescriptorSet(state->renderer, state->descriptorSet);
        state->descriptorSet = NULL;
    }

    REI_free(state);
}

FONScontext*
    REI_Fontstash_Init(REI_Renderer* renderer, REI_Queue* queue, REI_RL_State* loader, REI_Fontstash_Desc* info)
{
    FONSparams           params;
    REI_Fontstash_State* state;

    state = (REI_Fontstash_State*)REI_malloc(sizeof(REI_Fontstash_State));
    if (!state)
        return NULL;
    memset(state, 0, sizeof(REI_Fontstash_State));
    state->desc = *info;
    state->renderer = renderer;
    state->queue = queue;
    state->loader = loader;

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
        REI_cmdBindDescriptorSet(pCmd, 0, state->descriptorSet);
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
        REI_cmdBindPushConstants(pCmd, state->rootSignature, "pc", scaleTranslate);
    }

    // Draw
    REI_cmdDraw(pCmd, state->vertexCount, 0);
}

void REI_Fontstash_Shutdown(FONScontext* ctx) { fonsDeleteInternal(ctx); }
