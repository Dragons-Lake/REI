#ifndef SAMPLE_TEST
#    include "REI_Sample/sample.h"
#endif

static REI_CmdPool*       cmdPool;
static REI_Cmd*           pCmds[FRAME_COUNT];
static REI_Texture*       depthBuffer;
static REI_Shader*        shader;
static REI_RootSignature* rootSignature;
static REI_Pipeline*      pipeline;

int sample_on_init()
{
    REI_addCmdPool(renderer, gfxQueue, false, &cmdPool);
    for (size_t i = 0; i < FRAME_COUNT; ++i)
        REI_addCmd(renderer, cmdPool, false, &pCmds[i]);

    return 1;
}

void sample_on_fini()
{
    for (size_t i = 0; i < FRAME_COUNT; ++i)
        REI_removeCmd(renderer, cmdPool, pCmds[i]);
    REI_removeCmdPool(renderer, cmdPool);
}

// shader.vert, compiled with:
// # glslangValidator -V -x -o shader.vert.u32.h shader.vert
// see sources/shaders/build.ninja
static uint32_t vert_spv[] = {
#include "spv/shader.vert.u32.h"
};

// shader.frag, compiled with:
// # glslangValidator -V -x -o shader.frag.u32.h shader.frag
// see sources/shaders/build.ninja
static uint32_t frag_spv[] = {
#include "spv/shader.frag.u32.h"
};

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

    REI_ShaderDesc basicShader = {};
    basicShader.stages = REI_SHADER_STAGE_VERT | REI_SHADER_STAGE_FRAG;
    basicShader.vert = { (uint8_t*)vert_spv, sizeof(vert_spv) };
    basicShader.frag = { (uint8_t*)frag_spv, sizeof(frag_spv) };
    REI_addShader(renderer, &basicShader, &shader);

    REI_Shader*           shaders[]{ shader };
    REI_RootSignatureDesc rootDesc{};
    rootDesc.shaderCount = 1;
    rootDesc.ppShaders = shaders;
    REI_addRootSignature(renderer, &rootDesc, &rootSignature);

    REI_RasterizerStateDesc rasterizerStateDesc{};
    rasterizerStateDesc.cullMode = REI_CULL_MODE_NONE;

    REI_DepthStateDesc depthStateDesc{};
    depthStateDesc.depthTestEnable = true;
    depthStateDesc.depthWriteEnable = true;
    depthStateDesc.depthCmpFunc = REI_CMP_LEQUAL;

    REI_VertexLayout vertexLayout{};
    vertexLayout.attribCount = 0;

    REI_PipelineDesc pipelineDesc{};
    pipelineDesc.type = REI_PIPELINE_TYPE_GRAPHICS;

    REI_Format                colorFormat = swapchainDesc->colorFormat;
    REI_GraphicsPipelineDesc& pipelineSettings = pipelineDesc.graphicsDesc;
    pipelineSettings.primitiveTopo = REI_PRIMITIVE_TOPO_TRI_STRIP;
    pipelineSettings.renderTargetCount = 1;
    pipelineSettings.pDepthState = &depthStateDesc;
    pipelineSettings.pColorFormats = &colorFormat;
    pipelineSettings.sampleCount = swapchainDesc->sampleCount;
    pipelineSettings.depthStencilFormat = depthRTDesc.format;
    pipelineSettings.pRootSignature = rootSignature;
    pipelineSettings.pShaderProgram = shader;
    pipelineSettings.pVertexLayout = &vertexLayout;
    pipelineSettings.pRasterizerState = &rasterizerStateDesc;
    REI_addPipeline(renderer, &pipelineDesc, &pipeline);
}

void sample_on_swapchain_fini()
{
    REI_removeShader(renderer, shader);
    REI_removeRootSignature(renderer, rootSignature);
    REI_removePipeline(renderer, pipeline);
    REI_removeTexture(renderer, depthBuffer);
}

void sample_on_event(SDL_Event* evt) {}

void sample_on_frame(const FrameData* frameData)
{
    REI_Texture* renderTarget = frameData->backBuffer;
    REI_Cmd*     cmd = pCmds[frameData->setIndex];

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

    REI_endCmd(cmd);

    sample_submit(cmd);
}
