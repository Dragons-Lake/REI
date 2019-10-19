#include "BasicDraw.h"

#ifdef _WIN32
#    define strcpy strcpy_s
#endif

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
    REI_Shader*        shader;
};

struct REI_BasicDraw
{
    REI_BasicDraw_Desc desc;
    REI_Renderer*      renderer;
    PipelineData       meshPipelineData;
    PipelineData       pointPipelineData;
    PipelineData       linePipelineData;
    REI_Buffer**       dataBuffers;
    void**             dataBuffersAddr;
    REI_Buffer**       uniBuffers;
    void**             uniBuffersAddr;
    REI_Pipeline*      currentPipeline;
    REI_DescriptorSet* meshDescriptorSet;
    REI_DescriptorSet* lineDescriptorSet;
    REI_DescriptorSet* pointUniDescriptorSet;
    REI_DescriptorSet* pointDataDescriptorSet;
    uint32_t           setIndex;
    uint64_t           dataUsed;
    uint32_t           uniformDataCount;
};

// basicdraw_point.vert, compiled with:
// # glslangValidator -V -x -o fontstash.vert.u32.h fontstash.vert
// see sources/shaders/build.ninja
static uint32_t point_vert_spv[] = {
#include "spv/basicdraw_point.vert.u32.h"
};

// basicdraw_line.vert, compiled with:
// # glslangValidator -V -x -o fontstash.vert.u32.h fontstash.vert
// see sources/shaders/build.ninja
static uint32_t line_vert_spv[] = {
#include "spv/basicdraw_line.vert.u32.h"
};

// fontstash.vert, compiled with:
// # glslangValidator -V -x -o fontstash.vert.u32.h fontstash.vert
// see sources/shaders/build.ninja
static uint32_t mesh_vert_spv[] = {
#include "spv/basicdraw_mesh.vert.u32.h"
};

// fontstash.frag, compiled with:
// # glslangValidator -V -x -o fontstash.frag.u32.h fontstash.frag
// see sources/shaders/build.ninja
static uint32_t frag_spv[] = {
#include "spv/basicdraw.frag.u32.h"
};

void createPipelineData(
    REI_BasicDraw* state, REI_VertexLayout* vertexLayout, REI_ShaderDesc* shaderDesc, PipelineData* pipelineData)
{
    REI_addShader(state->renderer, shaderDesc, &pipelineData->shader);

    REI_RootSignatureDesc rootSigDesc = {};
    rootSigDesc.ppShaders = &pipelineData->shader;
    rootSigDesc.shaderCount = 1;
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

    REI_PipelineDesc pipelineDesc = {};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;
    REI_GraphicsPipelineDesc& graphicsDesc = pipelineDesc.graphicsDesc;
    graphicsDesc.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_LIST;
    graphicsDesc.renderTargetCount = 1;
    graphicsDesc.pColorFormats = &state->desc.colorFormat;
    graphicsDesc.sampleCount = state->desc.sampleCount;
    graphicsDesc.depthStencilFormat = state->desc.depthStencilFormat;
    graphicsDesc.pRootSignature = pipelineData->rootSignature;
    graphicsDesc.pShaderProgram = pipelineData->shader;
    graphicsDesc.pVertexLayout = vertexLayout;
    graphicsDesc.pRasterizerState = &rasterizerStateDesc;
    graphicsDesc.pBlendState = &blendState;
    REI_addPipeline(state->renderer, &pipelineDesc, &pipelineData->pipeline);
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
    if (pipelineData->shader)
    {
        REI_removeShader(state->renderer, pipelineData->shader);
        pipelineData->shader = NULL;
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
        descrUpdate[0].pName = "uStream0";
        descrUpdate[0].count = 1;
        descrUpdate[0].ppBuffers = &interleaved;
    }
    else
    {
        descrUpdate[0].pName = "uStream0";
        descrUpdate[0].count = 1;
        descrUpdate[0].ppBuffers = &positions;

        if (colors)
        {
            descrUpdate[1].pName = "uStream1";
            descrUpdate[1].count = 1;
            descrUpdate[1].ppBuffers = &colors;
            ++numDescrUpdates;
        }
    }

    REI_updateDescriptorSet(
        state->renderer, state->pointDataDescriptorSet, idx + state->desc.resourceSetCount, numDescrUpdates, descrUpdate);
}

REI_BasicDraw* REI_BasicDraw_Init(REI_Renderer* renderer, REI_BasicDraw_Desc* info)
{
    REI_BasicDraw* state = (REI_BasicDraw*)REI_malloc(sizeof(REI_BasicDraw));

    if (!state)
        return NULL;
    memset(state, 0, sizeof(REI_BasicDraw));
    state->desc = *info;
    state->renderer = renderer;

    REI_VertexLayout vertexLayout{};
    vertexLayout.attribCount = 2;
    vertexLayout.attribs[0].semanticNameLength = 4;
    strcpy(vertexLayout.attribs[0].semanticName, "aPos");
    vertexLayout.attribs[0].offset = REI_OFFSETOF(REI_BasicDraw_V_P3C, p);
    vertexLayout.attribs[0].location = 0;
    vertexLayout.attribs[0].format = REI_FMT_R32G32B32_SFLOAT;
    vertexLayout.attribs[1].semanticNameLength = 6;
    strcpy(vertexLayout.attribs[1].semanticName, "aColor");
    vertexLayout.attribs[1].offset = REI_OFFSETOF(REI_BasicDraw_V_P3C, c);
    vertexLayout.attribs[1].location = 1;
    vertexLayout.attribs[1].format = REI_FMT_R8G8B8A8_UNORM;

    REI_ShaderDesc meshShaderDesc{};
    meshShaderDesc.stages = REI_SHADER_STAGE_VERT | REI_SHADER_STAGE_FRAG;
    meshShaderDesc.vert = { (uint8_t*)mesh_vert_spv, sizeof(mesh_vert_spv) };
    meshShaderDesc.frag = { (uint8_t*)frag_spv, sizeof(frag_spv) };
    createPipelineData(state, &vertexLayout, &meshShaderDesc, &state->meshPipelineData);

    REI_ShaderDesc pointShaderDesc{};
    pointShaderDesc.stages = REI_SHADER_STAGE_VERT | REI_SHADER_STAGE_FRAG;
    pointShaderDesc.vert = { (uint8_t*)point_vert_spv, sizeof(point_vert_spv) };
    pointShaderDesc.frag = { (uint8_t*)frag_spv, sizeof(frag_spv) };
    createPipelineData(state, nullptr, &pointShaderDesc, &state->pointPipelineData);

    REI_ShaderDesc lineShaderDesc{};
    lineShaderDesc.stages = REI_SHADER_STAGE_VERT | REI_SHADER_STAGE_FRAG;
    lineShaderDesc.vert = { (uint8_t*)line_vert_spv, sizeof(line_vert_spv) };
    lineShaderDesc.frag = { (uint8_t*)frag_spv, sizeof(frag_spv) };
    createPipelineData(state, nullptr, &lineShaderDesc, &state->linePipelineData);

    REI_BufferDesc transformBufDesc = {};
    transformBufDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER;
    transformBufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    transformBufDesc.size = state->desc.maxDrawCount * sizeof(REI_BasicDraw_Uniforms);
    transformBufDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    state->uniBuffers = (REI_Buffer**)REI_calloc(state->desc.resourceSetCount, sizeof(REI_Buffer*));
    state->uniBuffersAddr = (void**)REI_calloc(state->desc.resourceSetCount, sizeof(void*));

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &transformBufDesc, &state->uniBuffers[i]);
        REI_mapBuffer(state->renderer, state->uniBuffers[i], &state->uniBuffersAddr[i]);
    }

    REI_DescriptorSetDesc meshDescriptorSetDesc = {};
    meshDescriptorSetDesc.pRootSignature = state->meshPipelineData.rootSignature;
    meshDescriptorSetDesc.maxSets = state->desc.resourceSetCount;
    meshDescriptorSetDesc.setIndex = REI_DESCRIPTOR_SET_INDEX_0;
    REI_addDescriptorSet(state->renderer, &meshDescriptorSetDesc, &state->meshDescriptorSet);

    REI_DescriptorData descrUpdate[2] = {};

    descrUpdate[0].pName = "uStream0";
    descrUpdate[0].count = 1;

    REI_DescriptorSetDesc pointDataDescriptorSetDesc = {};
    pointDataDescriptorSetDesc.pRootSignature = state->pointPipelineData.rootSignature;
    pointDataDescriptorSetDesc.maxSets = state->desc.resourceSetCount + state->desc.maxBufferSets;
    pointDataDescriptorSetDesc.setIndex = REI_DESCRIPTOR_SET_INDEX_1;
    REI_addDescriptorSet(state->renderer, &pointDataDescriptorSetDesc, &state->pointDataDescriptorSet);

    REI_BufferDesc dataBufDesc = {};
    dataBufDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER|REI_DESCRIPTOR_TYPE_VERTEX_BUFFER;
    dataBufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU;
    dataBufDesc.size = state->desc.maxDataSize;
    dataBufDesc.flags = REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    state->dataBuffers = (REI_Buffer**)REI_calloc(state->desc.resourceSetCount, sizeof(REI_Buffer*));
    state->dataBuffersAddr = (void**)REI_calloc(state->desc.resourceSetCount, sizeof(void*));
    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_addBuffer(state->renderer, &dataBufDesc, &state->dataBuffers[i]);
        REI_mapBuffer(state->renderer, state->dataBuffers[i], &state->dataBuffersAddr[i]);
        descrUpdate[0].ppBuffers = &state->dataBuffers[i];
        REI_updateDescriptorSet(state->renderer, state->pointDataDescriptorSet, i, 1, descrUpdate);
    }

    descrUpdate[0].pName = "uUniforms";

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        descrUpdate[0].ppBuffers = &state->uniBuffers[i];
        REI_updateDescriptorSet(state->renderer, state->meshDescriptorSet, i, 1, descrUpdate);
    }

    REI_DescriptorSetDesc pointUniDescriptorSetDesc = {};
    pointUniDescriptorSetDesc.pRootSignature = state->pointPipelineData.rootSignature;
    pointUniDescriptorSetDesc.maxSets = state->desc.resourceSetCount;
    pointUniDescriptorSetDesc.setIndex = REI_DESCRIPTOR_SET_INDEX_0;
    REI_addDescriptorSet(state->renderer, &pointUniDescriptorSetDesc, &state->pointUniDescriptorSet);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        descrUpdate[0].ppBuffers = &state->uniBuffers[i];
        REI_updateDescriptorSet(state->renderer, state->pointUniDescriptorSet, i, 1, descrUpdate);
    }

    REI_DescriptorSetDesc lineDescriptorSetDesc = {};
    lineDescriptorSetDesc.pRootSignature = state->linePipelineData.rootSignature;
    lineDescriptorSetDesc.maxSets = state->desc.resourceSetCount;
    lineDescriptorSetDesc.setIndex = REI_DESCRIPTOR_SET_INDEX_0;
    REI_addDescriptorSet(state->renderer, &lineDescriptorSetDesc, &state->lineDescriptorSet);

    descrUpdate[1].pName = "uLines";
    descrUpdate[1].count = 1;

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        descrUpdate[0].ppBuffers = &state->uniBuffers[i];
        descrUpdate[1].ppBuffers = &state->dataBuffers[i];
        REI_updateDescriptorSet(state->renderer, state->lineDescriptorSet, i, 2, descrUpdate);
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
        REI_cmdBindDescriptorSet(pCmd, state->setIndex, state->meshDescriptorSet);
        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
    }

    REI_cmdBindPushConstants(pCmd, state->meshPipelineData.rootSignature, "vpc", &state->uniformDataCount);
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
        REI_cmdBindDescriptorSet(pCmd, state->setIndex, state->pointUniDescriptorSet);
        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
    }

    REI_cmdBindDescriptorSet(pCmd, bufferSet, state->pointDataDescriptorSet);    // TODO: cache
    uint32_t pushConstants[2]{ state->uniformDataCount, type };
    REI_cmdBindPushConstants(pCmd, state->pointPipelineData.rootSignature, "vpc", pushConstants);
    REI_cmdDraw(pCmd, 6 * pointCount, 6 * firstPoint);

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
        REI_cmdBindDescriptorSet(pCmd, state->setIndex, state->lineDescriptorSet);
        REI_cmdSetViewport(pCmd, 0.0f, 0.0f, (float)state->desc.fbWidth, (float)state->desc.fbHeight, 0.0f, 1.0f);
    }

    REI_cmdBindPushConstants(pCmd, state->linePipelineData.rootSignature, "vpc", &state->uniformDataCount);
    REI_cmdDraw(pCmd, 6 * count, 6 * (uint32_t)(offset / sizeof(REI_BasicDraw_Line)));

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
    REI_removeDescriptorSet(state->renderer, state->meshDescriptorSet);
    REI_removeDescriptorSet(state->renderer, state->pointUniDescriptorSet);
    REI_removeDescriptorSet(state->renderer, state->pointDataDescriptorSet);
    REI_removeDescriptorSet(state->renderer, state->lineDescriptorSet);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->dataBuffers[i]);
    }
    REI_free(state->dataBuffers);
    REI_free(state->dataBuffersAddr);

    for (uint32_t i = 0; i < state->desc.resourceSetCount; ++i)
    {
        REI_removeBuffer(state->renderer, state->uniBuffers[i]);
    }
    REI_free(state->uniBuffers);
    REI_free(state->uniBuffersAddr);

    destroyPipelineData(state, &state->meshPipelineData);
    destroyPipelineData(state, &state->pointPipelineData);
    destroyPipelineData(state, &state->linePipelineData);

    REI_free(state);
}
