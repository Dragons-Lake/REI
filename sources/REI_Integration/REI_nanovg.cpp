#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <vector>
#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include "windows.h"
#endif

#ifdef _WIN32
#    define strcpy strcpy_s
#endif

#include "ResourceLoader.h"
#include "REI_nanovg.h"

enum REI_NanoVG_textureflags
{
    NVGL_TEXTURE_FLIP_Y = 0x01,
    NVGL_TEXTURE_NODELETE = 0x02,
    NVGL_TEXTURE_PREMULTIPLIED = 0x04,
    NVGL_TEXTURE_LUMINANCE = 0x08,
};

enum REI_NanoVG_shaderType
{
    NSVG_SHADER_FILLGRAD,
    NSVG_SHADER_FILLIMG,
    NSVG_SHADER_SIMPLE,
    NSVG_SHADER_IMG
};

struct REI_NanoVG_texture
{
    //TODO: add generations and upload state tracking
    REI_Texture* tex;
    uint32_t     width;
    uint32_t     height;
    uint32_t     flags;
};

enum REI_NanoVG_callType
{
    GLNVG_NONE = 0,
    GLNVG_FILL,
    GLNVG_CONVEXFILL,
    GLNVG_STROKE,
    GLNVG_TRIANGLES,
};

struct REI_NanoVG_call
{
    int      type;
    int      image;
    uint32_t pathOffset, pathCount;
    uint32_t fillOffset, fillCount;
    uint32_t strokeOffset, strokeCount;
    uint32_t triangleOffset, triangleCount;
    uint32_t uniformIndex;
};

struct REI_NanoVG_path
{
    uint32_t fillOffset;
    uint32_t fillCount;
    uint32_t strokeOffset;
    uint32_t strokeCount;
};

struct REI_NanoVG_fragUniforms
{
    float           scissorMat[8];
    float           paintMat[8];
    struct NVGcolor innerCol;
    struct NVGcolor outerCol;
    float           scissorExt[2];
    float           scissorScale[2];
    float           extent[2];
    float           radius;
    float           feather;
    float           strokeMult;
    float           strokeThr;
    int             texType;
    int             type;
};

struct REI_NanoVG_State
{
    REI_NanoVG_Desc    desc;
    REI_Renderer*      renderer;
    REI_Queue*         queue;
    REI_RL_State*      loader;
    REI_RootSignature* rootSignature;
    REI_DescriptorSet* uniDescriptorSet;
    REI_DescriptorSet* texDescriptorSet;
    REI_Pipeline*      triPipeline;
    REI_Pipeline*      fillPipeline;
    REI_Pipeline*      maskPipeline;
    REI_Pipeline*      drawPipeline;
    REI_Sampler*       sampler;
    REI_Shader*        shader;
    REI_Buffer**       vtxBuffers;
    void**             vtxBuffersAddr;
    REI_Buffer**       uniBuffers;
    void**             uniBuffersAddr;
    REI_Cmd*           cmd;
    uint32_t           setIndex;
    uint32_t           width;
    uint32_t           height;
    uint32_t           vtxCount;
    uint32_t           uniCount;

    std::vector<REI_NanoVG_call>    calls;
    std::vector<REI_NanoVG_texture> textures;
};

// fontstash.vert, compiled with:
// # glslangValidator -V -x -o fontstash.vert.u32.h fontstash.vert
// see sources/shaders/build.ninja
static uint32_t vert_spv[] = {
#include "spv/nanovg.vert.u32.h"
};

// fontstash.frag, compiled with:
// # glslangValidator -V -x -o fontstash.frag.u32.h fontstash.frag
// see sources/shaders/build.ninja
static uint32_t frag_spv[] = {
#include "spv/nanovg.frag.u32.h"
};

#define OFFSETOF(type, mem) ((size_t)(&(((type*)0)->mem)))

NVGvertex* allocVertices(REI_NanoVG_State* state, uint32_t count)
{
    bool enoughSpace = state->desc.maxVerts - state->vtxCount >= count;
    if (enoughSpace)
    {
        NVGvertex* vtx = ((NVGvertex*)state->vtxBuffersAddr[state->setIndex]) + state->vtxCount;
        state->vtxCount += count;
        return vtx;
    }
    return 0;
}

REI_NanoVG_fragUniforms* allocUniformData(REI_NanoVG_State* state)
{
    bool enoughSpace = state->desc.maxDraws > state->uniCount;
    if (enoughSpace)
    {
        REI_NanoVG_fragUniforms* uni =
            ((REI_NanoVG_fragUniforms*)state->uniBuffersAddr[state->setIndex]) + state->uniCount;
        ++state->uniCount;
        return uni;
    }
    return 0;
}

static int REI_NanoVG_renderCreate(void* userPtr)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)userPtr;
    REI_ShaderDesc    shaderDesc{};
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
    REI_addSampler(state->renderer, &samplerDesc, &state->sampler);

    const char*           staticSampNames[] = { "uSampler" };
    REI_RootSignatureDesc rootSigDesc = {};
    rootSigDesc.ppShaders = &state->shader;
    rootSigDesc.shaderCount = 1;
    rootSigDesc.ppStaticSamplerNames = staticSampNames;
    rootSigDesc.ppStaticSamplers = &state->sampler;
    rootSigDesc.staticSamplerCount = 1;
    REI_addRootSignature(state->renderer, &rootSigDesc, &state->rootSignature);

    REI_DescriptorSetDesc descriptorSetDesc = {};
    descriptorSetDesc.pRootSignature = state->rootSignature;
    descriptorSetDesc.maxSets = state->desc.resourceSetCount;
    descriptorSetDesc.setIndex = REI_DESCRIPTOR_SET_INDEX_1;
    REI_addDescriptorSet(state->renderer, &descriptorSetDesc, &state->uniDescriptorSet);

    descriptorSetDesc.maxSets = state->desc.maxTextures;
    descriptorSetDesc.setIndex = REI_DESCRIPTOR_SET_INDEX_2;
    REI_addDescriptorSet(state->renderer, &descriptorSetDesc, &state->texDescriptorSet);

    REI_VertexLayout vertexLayout{};
    vertexLayout.attribCount = 2;
    vertexLayout.attribs[0].semanticNameLength = 4;
    strcpy(vertexLayout.attribs[0].semanticName, "aPos");
    vertexLayout.attribs[0].offset = OFFSETOF(NVGvertex, x);
    vertexLayout.attribs[0].location = 0;
    vertexLayout.attribs[0].format = REI_FMT_R32G32_SFLOAT;
    vertexLayout.attribs[1].semanticNameLength = 3;
    strcpy(vertexLayout.attribs[1].semanticName, "aUV");
    vertexLayout.attribs[1].offset = OFFSETOF(NVGvertex, u);
    vertexLayout.attribs[1].location = 1;
    vertexLayout.attribs[1].format = REI_FMT_R32G32_SFLOAT;

    REI_RasterizerStateDesc rasterizerState{};
    rasterizerState.cullMode = REI_CULL_MODE_BACK;
    rasterizerState.frontFace = REI_FRONT_FACE_CCW;

    REI_BlendStateDesc blendState{};
    blendState.renderTargetMask = REI_BLEND_STATE_TARGET_0;
    blendState.srcFactors[0] = REI_BC_ONE;
    blendState.dstFactors[0] = REI_BC_ONE_MINUS_SRC_ALPHA;
    blendState.blendModes[0] = REI_BM_ADD;
    blendState.srcAlphaFactors[0] = REI_BC_ONE;
    blendState.dstAlphaFactors[0] = REI_BC_ONE_MINUS_SRC_ALPHA;
    blendState.blendAlphaModes[0] = REI_BM_ADD;
    blendState.masks[0] = REI_COLOR_MASK_ALL;

    REI_DepthStateDesc depthState{};
    depthState.depthTestEnable = false;
    depthState.stencilTestEnable = false;
    depthState.stencilFrontFunc = REI_CMP_ALWAYS;
    depthState.depthFrontFail = REI_STENCIL_OP_KEEP;
    depthState.stencilFrontFail = REI_STENCIL_OP_KEEP;
    depthState.stencilFrontPass = REI_STENCIL_OP_KEEP;
    depthState.stencilBackFunc = REI_CMP_ALWAYS;
    depthState.depthBackFail = REI_STENCIL_OP_KEEP;
    depthState.stencilBackFail = REI_STENCIL_OP_KEEP;
    depthState.stencilBackPass = REI_STENCIL_OP_KEEP;
    depthState.stencilReadMask = 0xFF;
    depthState.stencilWriteMask = 0xFF;

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
    graphicsDesc.pDepthState = &depthState;
    graphicsDesc.pRasterizerState = &rasterizerState;
    graphicsDesc.pBlendState = &blendState;
    REI_addPipeline(state->renderer, &pipelineDesc, &state->triPipeline);

    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_STRIP;

    REI_addPipeline(state->renderer, &pipelineDesc, &state->fillPipeline);

    rasterizerState.cullMode = REI_CULL_MODE_NONE;

    depthState.stencilTestEnable = true;
    depthState.stencilFrontPass = REI_STENCIL_OP_INCR;
    depthState.stencilBackPass = REI_STENCIL_OP_DECR;

    blendState.masks[0] = REI_COLOR_MASK_NONE;

    REI_addPipeline(state->renderer, &pipelineDesc, &state->maskPipeline);

    rasterizerState.cullMode = REI_CULL_MODE_BACK;

    depthState.stencilFrontFunc = REI_CMP_NOTEQUAL;
    depthState.stencilFrontFail = REI_STENCIL_OP_SET_ZERO;
    depthState.depthFrontFail = REI_STENCIL_OP_SET_ZERO;
    depthState.stencilFrontPass = REI_STENCIL_OP_SET_ZERO;
    depthState.stencilBackFunc = REI_CMP_NOTEQUAL;
    depthState.stencilBackFail = REI_STENCIL_OP_SET_ZERO;
    depthState.depthBackFail = REI_STENCIL_OP_SET_ZERO;
    depthState.stencilBackPass = REI_STENCIL_OP_SET_ZERO;

    blendState.masks[0] = REI_COLOR_MASK_ALL;

    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;

    REI_addPipeline(state->renderer, &pipelineDesc, &state->drawPipeline);

    REI_BufferDesc vbDesc = {};
    vbDesc.descriptors = REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
    vbDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    vbDesc.vertexStride = sizeof(NVGvertex);
    vbDesc.size = state->desc.maxVerts * sizeof(NVGvertex);
    vbDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;

    state->vtxBuffers = (REI_Buffer**)REI_calloc(state->desc.resourceSetCount, sizeof(REI_Buffer*));
    state->vtxBuffersAddr = (void**)REI_calloc(state->desc.resourceSetCount, sizeof(void*));
    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &vbDesc, &state->vtxBuffers[i]);
        REI_mapBuffer(state->renderer, state->vtxBuffers[i], &state->vtxBuffersAddr[i]);
    }

    REI_NanoVG_fragUniforms frag{};
    frag.strokeThr = -1.0f;
    frag.type = NSVG_SHADER_SIMPLE;
    state->uniBuffers = (REI_Buffer**)REI_calloc(state->desc.resourceSetCount, sizeof(REI_Buffer*));
    state->uniBuffersAddr = (void**)REI_calloc(state->desc.resourceSetCount, sizeof(void*));
    REI_BufferDesc ubiDesc = {};
    ubiDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER;
    ubiDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    ubiDesc.size = state->desc.maxDraws * sizeof(REI_NanoVG_fragUniforms);
    ubiDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT | REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT;
    REI_DescriptorData descrUpdateDesc{};
    descrUpdateDesc.pName = "uPaints";
    descrUpdateDesc.count = 1;
    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &ubiDesc, &state->uniBuffers[i]);
        REI_mapBuffer(state->renderer, state->uniBuffers[i], &state->uniBuffersAddr[i]);
        // Simple uniforms for stencil
        memcpy(state->uniBuffersAddr[i], &frag, sizeof(REI_NanoVG_fragUniforms));

        descrUpdateDesc.ppBuffers = &state->uniBuffers[i];
        REI_updateDescriptorSet(state->renderer, state->uniDescriptorSet, i, 1, &descrUpdateDesc);
    }

    REI_resetDescriptorSet(state->renderer, state->texDescriptorSet, 0);

    state->textures.resize(1);

    return 1;
}

static void REI_NanoVG_renderDelete(void* userPtr)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)userPtr;

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->vtxBuffers[i]);
    }
    REI_free(state->vtxBuffers);
    REI_free(state->vtxBuffersAddr);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->uniBuffers[i]);
    }
    REI_free(state->uniBuffers);
    REI_free(state->uniBuffersAddr);

    if (state->sampler)
    {
        REI_removeSampler(state->renderer, state->sampler);
        state->sampler = NULL;
    }
    if (state->rootSignature)
    {
        REI_removeRootSignature(state->renderer, state->rootSignature);
        state->rootSignature = NULL;
    }
    if (state->triPipeline)
    {
        REI_removePipeline(state->renderer, state->triPipeline);
        state->triPipeline = NULL;
    }
    if (state->maskPipeline)
    {
        REI_removePipeline(state->renderer, state->maskPipeline);
        state->maskPipeline = NULL;
    }
    if (state->drawPipeline)
    {
        REI_removePipeline(state->renderer, state->drawPipeline);
        state->drawPipeline = NULL;
    }
    if (state->fillPipeline)
    {
        REI_removePipeline(state->renderer, state->fillPipeline);
        state->fillPipeline = NULL;
    }
    if (state->shader)
    {
        REI_removeShader(state->renderer, state->shader);
        state->shader = NULL;
    }
    if (state->uniDescriptorSet)
    {
        REI_removeDescriptorSet(state->renderer, state->uniDescriptorSet);
        state->uniDescriptorSet = NULL;
    }
    if (state->texDescriptorSet)
    {
        REI_removeDescriptorSet(state->renderer, state->texDescriptorSet);
        state->texDescriptorSet = NULL;
    }

    for (auto& tex: state->textures)
    {
        if (tex.tex != 0 && (tex.flags & NVGL_TEXTURE_NODELETE) == 0)
            REI_removeTexture(state->renderer, tex.tex);
    }

    state->textures.~vector();
    state->calls.~vector();

    REI_free(state);
}

static int REI_NanoVG_renderCreateTexture(void* uptr, int type, int w, int h, int imageFlags, const unsigned char* data)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;

    auto it = std::find_if(
        state->textures.begin() + 1, state->textures.end(), [](REI_NanoVG_texture& tex) { return tex.tex == 0; });
    size_t idx = it - state->textures.begin();
    if (idx == state->textures.size())
        state->textures.emplace_back();
    REI_NanoVG_texture& tex = state->textures[idx];

    memset(&tex, 0, sizeof(REI_NanoVG_texture));
    tex.flags = imageFlags;
    tex.width = w;
    tex.height = h;

    const bool formatR8 = type == NVG_TEXTURE_ALPHA;
    //const bool useMipmaps = (imageFlags & NVG_IMAGE_GENERATE_MIPMAPS) != 0;

    if (formatR8)
        tex.flags |= NVGL_TEXTURE_LUMINANCE;

    REI_TextureDesc desc{};
    desc.width = w;
    desc.height = h;
    desc.mipLevels = /*useMipmaps ? bit_fls(core::max<uint32_t>(w, h)) :*/ 1;
    desc.format = formatR8 ? REI_FMT_R8_UNORM : REI_FMT_R8G8B8A8_UNORM;
    desc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(state->renderer, &desc, &tex.tex);

    if (data)
    {
        REI_RL_TextureUpdateDesc updateDesc{};
        updateDesc.pTexture = tex.tex;
        updateDesc.pRawData = (uint8_t*)data;
        updateDesc.format = desc.format;
        updateDesc.width = desc.width;
        updateDesc.height = desc.height;
        updateDesc.depth = 1;
        updateDesc.arrayLayer = 0;
        updateDesc.mipLevel = 0;// for desc.mipLevels;
        updateDesc.x = 0;
        updateDesc.y = 0;
        updateDesc.z = 0;
        updateDesc.endState = REI_RESOURCE_STATE_SHADER_RESOURCE;
        REI_RL_updateResource(state->loader, &updateDesc);
    }

    REI_DescriptorData descrUpdateDesc{};
    descrUpdateDesc.pName = "uTexture";
    descrUpdateDesc.ppTextures = &tex.tex;
    descrUpdateDesc.count = 1;
    REI_updateDescriptorSet(state->renderer, state->texDescriptorSet, (uint32_t)idx, 1, &descrUpdateDesc);

    //TODO support mips and mips generation

    return (int)idx;
}

static int REI_NanoVG_renderDeleteTexture(void* uptr, int image)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;
    if (image < 0 && (size_t)image >= state->textures.size())
        return 0;
    REI_NanoVG_texture& tex = state->textures[image];

    if (tex.tex != 0 && (tex.flags & NVGL_TEXTURE_NODELETE) == 0)
    {
        REI_waitQueueIdle(state->queue);
        REI_removeTexture(state->renderer, tex.tex);
    }
    memset(&tex, 0, sizeof(REI_NanoVG_texture));

    return 1;
}

static int REI_NanoVG_renderUpdateTexture(void* uptr, int image, int x, int y, int w, int h, const unsigned char* data)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;
    if (image < 0 && (size_t)image >= state->textures.size())
        return 0;
    REI_NanoVG_texture& tex = state->textures[image];

    bool isL8 = tex.flags & NVGL_TEXTURE_LUMINANCE;

    if (isL8)
        data += y * tex.width;
    else
        data += y * tex.width * 4;

    x = 0;
    w = tex.width;

    REI_RL_TextureUpdateDesc updateDesc{};
    updateDesc.pTexture = tex.tex;
    updateDesc.pRawData = (uint8_t*)data;
    updateDesc.format = isL8 ? REI_FMT_R8_UNORM : REI_FMT_R8G8B8A8_UNORM;
    updateDesc.width = w;
    updateDesc.height = h;
    updateDesc.depth = 1;
    updateDesc.arrayLayer = 0;
    updateDesc.mipLevel = 0;    //tex.mipLevels;
    updateDesc.x = x;
    updateDesc.y = y;
    updateDesc.z = 0;
    updateDesc.endState = REI_RESOURCE_STATE_SHADER_RESOURCE;
    REI_RL_updateResource(state->loader, &updateDesc);

    // Handled in resource loader?
    //REI_TextureBarrier texBarrier{ tex.tex, REI_RESOURCE_STATE_SHADER_RESOURCE, false };
    //REI_cmdResourceBarrier(state->cmd, 0, 0, 1, &texBarrier);

    return 1;
}

static int REI_NanoVG_renderGetTextureSize(void* uptr, int image, int* w, int* h)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;
    if (image < 0 && (size_t)image >= state->textures.size())
        return 0;
    REI_NanoVG_texture& tex = state->textures[image];

    *w = tex.width;
    *h = tex.height;

    return 1;
}

static void REI_NanoVG_xformToshmat(float* shm, float* t)
{
    shm[0] = t[0];
    shm[1] = t[2];
    shm[2] = t[4];
    shm[4] = t[1];
    shm[5] = t[3];
    shm[6] = t[5];
}

static NVGcolor REI_NanoVG_premulColor(NVGcolor c)
{
    c.r *= c.a;
    c.g *= c.a;
    c.b *= c.a;
    return c;
}

static int REI_NanoVG_convertPaint(
    REI_NanoVG_State* state, REI_NanoVG_fragUniforms* frag, NVGpaint* paint, NVGscissor* scissor, float width,
    float fringe, float strokeThr)
{
    float invxform[6];

    memset(frag, 0, sizeof(REI_NanoVG_fragUniforms));

    frag->innerCol = REI_NanoVG_premulColor(paint->innerColor);
    frag->outerCol = REI_NanoVG_premulColor(paint->outerColor);

    if (scissor->extent[0] < -0.5f || scissor->extent[1] < -0.5f)
    {
        frag->scissorExt[0] = 1.0f;
        frag->scissorExt[1] = 1.0f;
        frag->scissorScale[0] = 1.0f;
        frag->scissorScale[1] = 1.0f;
    }
    else
    {
        nvgTransformInverse(invxform, scissor->xform);
        REI_NanoVG_xformToshmat(frag->scissorMat, invxform);
        frag->scissorExt[0] = scissor->extent[0];
        frag->scissorExt[1] = scissor->extent[1];
        frag->scissorScale[0] =
            sqrtf(scissor->xform[0] * scissor->xform[0] + scissor->xform[2] * scissor->xform[2]) / fringe;
        frag->scissorScale[1] =
            sqrtf(scissor->xform[1] * scissor->xform[1] + scissor->xform[3] * scissor->xform[3]) / fringe;
    }

    memcpy(frag->extent, paint->extent, sizeof(frag->extent));
    frag->strokeMult = (width * 0.5f + fringe * 0.5f) / fringe;
    frag->strokeThr = strokeThr;

    if (paint->image != 0)
    {
        int image = paint->image;
        if (image < 0 && (size_t)image >= state->textures.size())
            return 0;
        REI_NanoVG_texture& tex = state->textures[image];
        if ((tex.flags & NVGL_TEXTURE_FLIP_Y) != 0)
        {
            float flipped[6];
            nvgTransformScale(flipped, 1.0f, -1.0f);
            nvgTransformMultiply(flipped, paint->xform);
            nvgTransformInverse(invxform, flipped);
        }
        else
        {
            nvgTransformInverse(invxform, paint->xform);
        }
        frag->type = NSVG_SHADER_FILLIMG;

        if (tex.flags & NVGL_TEXTURE_LUMINANCE)
            frag->texType = 2;
        else
            frag->texType = (tex.flags & NVGL_TEXTURE_PREMULTIPLIED) ? 0 : 1;
    }
    else
    {
        frag->type = NSVG_SHADER_FILLGRAD;
        frag->radius = paint->radius;
        frag->feather = paint->feather;
        nvgTransformInverse(invxform, paint->xform);
    }

    REI_NanoVG_xformToshmat(frag->paintMat, invxform);

    return 1;
}

static void REI_NanoVG_setUniforms(REI_NanoVG_State* state, uint32_t uniformIndex, int imageIndex)
{
    uint64_t vertex_offset = 0;
    float    scaleTranslate[4];

    scaleTranslate[0] = 2.0f / state->width;
    scaleTranslate[1] = -2.0f / state->height;
    scaleTranslate[2] = -1.0f;
    scaleTranslate[3] = 1.0f;

    REI_cmdBindDescriptorSet(state->cmd, state->setIndex, state->uniDescriptorSet);
    REI_cmdBindDescriptorSet(state->cmd, (uint32_t)imageIndex, state->texDescriptorSet);
    REI_cmdBindPushConstants(state->cmd, state->rootSignature, "vpc", scaleTranslate);
    REI_cmdBindPushConstants(state->cmd, state->rootSignature, "fpc", &uniformIndex);
    REI_cmdBindVertexBuffer(state->cmd, 1, &state->vtxBuffers[state->setIndex], &vertex_offset);
    REI_cmdSetViewport(state->cmd, 0.0f, 0.0f, (float)state->width, (float)state->height, 0.0f, 1.0f);
}

static void REI_NanoVG_renderViewport(void* uptr, int width, int height, float devicePixelRatio)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;
    state->width = (uint32_t)(width);
    state->height = (uint32_t)(height);
}

static void REI_NanoVG_fill(REI_NanoVG_State* state, REI_NanoVG_call& call)
{
    // Draw shapes
    REI_cmdBindPipeline(state->cmd, state->maskPipeline);
    REI_cmdSetStencilRef(state->cmd, REI_STENCIL_FACE_FRONT_AND_BACK, 0);
    REI_NanoVG_setUniforms(state, 0, 0);
    REI_cmdDraw(state->cmd, call.fillCount, call.fillOffset);

    // Draw fill
    REI_cmdBindPipeline(state->cmd, state->drawPipeline);
    REI_cmdSetStencilRef(state->cmd, REI_STENCIL_FACE_FRONT_AND_BACK, 0);
    REI_NanoVG_setUniforms(state, call.uniformIndex, call.image);
    REI_cmdDraw(state->cmd, call.triangleCount, call.triangleOffset);
}

static void REI_NanoVG_convexFill(REI_NanoVG_State* state, REI_NanoVG_call& call)
{
    REI_cmdBindPipeline(state->cmd, state->fillPipeline);
    REI_NanoVG_setUniforms(state, call.uniformIndex, call.image);
    REI_cmdDraw(state->cmd, call.fillCount, call.fillOffset);
}

static void REI_NanoVG_stroke(REI_NanoVG_State* state, REI_NanoVG_call& call)
{
    // Draw Strokes
    REI_cmdBindPipeline(state->cmd, state->fillPipeline);
    REI_NanoVG_setUniforms(state, call.uniformIndex, call.image);
    REI_cmdDraw(state->cmd, call.strokeCount, call.strokeOffset);
}

static void REI_NanoVG_triangles(REI_NanoVG_State* state, REI_NanoVG_call& call)
{
    REI_cmdBindPipeline(state->cmd, state->triPipeline);
    REI_NanoVG_setUniforms(state, call.uniformIndex, call.image);
    REI_cmdDraw(state->cmd, call.triangleCount, call.triangleOffset);
}

static void REI_NanoVG_renderCancel(void* uptr)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;
    state->calls.clear();
}

static void REI_NanoVG_renderFlush(void* uptr, NVGcompositeOperationState compositeOperation)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;

    if (!state->calls.empty())
    {
        for (REI_NanoVG_call& call: state->calls)
        {
            if (call.type == GLNVG_FILL)
                REI_NanoVG_fill(state, call);
            else if (call.type == GLNVG_CONVEXFILL)
                REI_NanoVG_convexFill(state, call);
            else if (call.type == GLNVG_STROKE)
                REI_NanoVG_stroke(state, call);
            else if (call.type == GLNVG_TRIANGLES)
                REI_NanoVG_triangles(state, call);
        }
    }

    // Reset calls
    state->calls.clear();
}

static int REI_NanoVG_uploadeStrokes(REI_NanoVG_State* state, REI_NanoVG_call& call, const NVGpath* paths, int npaths)
{
    // Allocate vertices for all the paths.
    uint32_t maxverts = 0;

    for (int i = 0; i < npaths; i++)
    {
        uint32_t nstroke = paths[i].nstroke;
        maxverts += nstroke > 2 ? nstroke + 1 + (i > 0) : 0;
    }

    call.strokeCount = maxverts;
    call.strokeOffset = state->vtxCount;

    NVGvertex* vtx = allocVertices(state, maxverts);
    if (!vtx)
        return 0;

    for (int i = 0; i < npaths; i++)
    {
        const NVGpath* path = &paths[i];
        uint32_t       nstroke = path->nstroke;
        if (nstroke > 2)
        {
            NVGvertex* stroke = path->stroke;
            if (i > 0)
                *vtx++ = stroke[0];
            memcpy(vtx, stroke, sizeof(NVGvertex) * nstroke);
            vtx += nstroke;
            *vtx++ = stroke[nstroke - 1];
        }
    }

    return 1;
}

static int REI_NanoVG_uploadeFills(REI_NanoVG_State* state, REI_NanoVG_call& call, const NVGpath* paths, int npaths)
{
    // Allocate vertices for all the paths.
    uint32_t maxverts = 0;

    for (int i = 0; i < npaths; i++)
    {
        uint32_t nfill = paths[i].nfill;
        maxverts += nfill > 2 ? nfill + 1 + (i > 0) : 0;
    }

    if (!maxverts)
        return 0;

    call.fillCount = maxverts;
    call.fillOffset = state->vtxCount;

    NVGvertex* vtx = allocVertices(state, maxverts);
    if (!vtx)
        return 0;

    for (int i = 0; i < npaths; i++)
    {
        const NVGpath* path = &paths[i];
        uint32_t       nfill = path->nfill;
        if (nfill > 2)
        {
            NVGvertex* fill = path->fill;
            if (i > 0)
                *vtx++ = fill[0];
            *vtx++ = fill[0];
            int avtx = nfill & 1;
            int halfvcount = (nfill - 1) / 2;
            int j = 1;
            for (; j <= halfvcount; ++j)
            {
                *vtx++ = fill[j];
                *vtx++ = fill[nfill - j];
            }
            if (avtx == 0)
            {
                *vtx++ = fill[j];
            }
            *vtx++ = fill[j - avtx];
        }
    }

    return 1;
}

static void REI_NanoVG_renderFill(
    void* uptr, NVGpaint* paint, NVGscissor* scissor, float fringe, const float* bounds, const NVGpath* paths,
    int npaths)
{
    REI_NanoVG_State*        state = (REI_NanoVG_State*)uptr;
    REI_NanoVG_fragUniforms* frag;
    NVGvertex*               vtx;

    state->calls.emplace_back();
    REI_NanoVG_call& call = state->calls.back();

    call.type = (npaths == 1 && paths[0].convex) ? GLNVG_CONVEXFILL : GLNVG_FILL;
    call.image = paint->image;

    if (!REI_NanoVG_uploadeFills(state, call, paths, npaths) || !REI_NanoVG_uploadeStrokes(state, call, paths, npaths))
        goto error;

    call.triangleOffset = state->vtxCount;
    call.triangleCount = 6;
    call.uniformIndex = state->uniCount;

    // Quad
    vtx = allocVertices(state, 6);
    if (!vtx)
        goto error;

    *vtx++ = { bounds[0], bounds[3], 0.5f, 1.0f };
    *vtx++ = { bounds[2], bounds[3], 0.5f, 1.0f };
    *vtx++ = { bounds[2], bounds[1], 0.5f, 1.0f };

    *vtx++ = { bounds[0], bounds[3], 0.5f, 1.0f };
    *vtx++ = { bounds[2], bounds[1], 0.5f, 1.0f };
    *vtx++ = { bounds[0], bounds[1], 0.5f, 1.0f };

    // Fill shader
    frag = allocUniformData(state);
    if (!frag)
        goto error;
    REI_NanoVG_convertPaint(state, frag, paint, scissor, fringe, fringe, -1.0f);

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if (!state->calls.empty())
        state->calls.pop_back();
}

static void REI_NanoVG_renderStroke(
    void* uptr, NVGpaint* paint, NVGscissor* scissor, float fringe, float strokeWidth, const NVGpath* paths, int npaths)
{
    REI_NanoVG_State*        state = (REI_NanoVG_State*)uptr;
    REI_NanoVG_fragUniforms* frag;
    state->calls.emplace_back();
    REI_NanoVG_call& call = state->calls.back();

    call.type = GLNVG_STROKE;
    call.image = paint->image;
    call.uniformIndex = state->uniCount;

    if (!REI_NanoVG_uploadeStrokes(state, call, paths, npaths))
        goto error;

    // Fill shader
    frag = allocUniformData(state);
    if (!frag)
        goto error;
    REI_NanoVG_convertPaint(state, frag, paint, scissor, strokeWidth, fringe, -1.0f);

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if (!state->calls.empty())
        state->calls.pop_back();
}

static void
    REI_NanoVG_renderTriangles(void* uptr, NVGpaint* paint, NVGscissor* scissor, const NVGvertex* verts, int nverts)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)uptr;

    state->calls.emplace_back();
    REI_NanoVG_call& call = state->calls.back();

    if (nverts < 3)
        return;

    call.type = GLNVG_TRIANGLES;
    call.image = paint->image;
    call.triangleCount = nverts;
    call.triangleOffset = state->vtxCount;
    call.uniformIndex = state->uniCount;

    // Allocate vertices for all the paths.
    NVGvertex*               vtx = allocVertices(state, nverts);
    REI_NanoVG_fragUniforms* frag = allocUniformData(state);
    if (!vtx || !frag)
        goto error;

    memcpy(vtx, verts, sizeof(NVGvertex) * nverts);

    // Fill shader
    REI_NanoVG_convertPaint(state, frag, paint, scissor, 1.0f, 1.0f, -1.0f);
    frag->type = NSVG_SHADER_IMG;

    return;

error:
    // We get here if call alloc was ok, but something else is not.
    // Roll back the last call to prevent drawing it.
    if (!state->calls.empty())
        state->calls.pop_back();
}

NVGcontext* REI_NanoVG_Init(REI_Renderer* renderer, REI_Queue* queue, REI_RL_State* loader, REI_NanoVG_Desc* info)
{
    NVGparams         params;
    REI_NanoVG_State* state = (REI_NanoVG_State*)REI_malloc(sizeof(REI_NanoVG_State));
    if (!state)
        return NULL;
    memset(state, 0, sizeof(REI_NanoVG_State));
    state->desc = *info;
    state->renderer = renderer;
    state->queue = queue;
    state->loader = loader;

    memset(&params, 0, sizeof(params));
    params.renderCreate = REI_NanoVG_renderCreate;
    params.renderCreateTexture = REI_NanoVG_renderCreateTexture;
    params.renderDeleteTexture = REI_NanoVG_renderDeleteTexture;
    params.renderUpdateTexture = REI_NanoVG_renderUpdateTexture;
    params.renderGetTextureSize = REI_NanoVG_renderGetTextureSize;
    params.renderViewport = REI_NanoVG_renderViewport;
    params.renderCancel = REI_NanoVG_renderCancel;
    params.renderFlush = REI_NanoVG_renderFlush;
    params.renderFill = REI_NanoVG_renderFill;
    params.renderStroke = REI_NanoVG_renderStroke;
    params.renderTriangles = REI_NanoVG_renderTriangles;
    params.renderDelete = REI_NanoVG_renderDelete;
    params.userPtr = state;
    params.edgeAntiAlias = 0;

    new (&state->textures) std::vector<REI_NanoVG_texture>();
    new (&state->calls) std::vector<REI_NanoVG_call>();

    return nvgCreateInternal(&params);
}

void REI_NanoVG_SetupRender(NVGcontext* ctx, REI_Cmd* pCmd, uint32_t set_index)
{
    REI_NanoVG_State* state = (REI_NanoVG_State*)nvgInternalParams(ctx)->userPtr;
    state->cmd = pCmd;
    state->vtxCount = 0;
    state->uniCount = 1;
    state->setIndex = set_index;
}

void REI_NanoVG_Shutdown(NVGcontext* ctx) { nvgDeleteInternal(ctx); }
