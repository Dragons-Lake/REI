/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#pragma once

static const uint32_t MAX_SHADER_STAGE_COUNT = 5;

typedef enum REI_TextureDimension
{
    REI_TEXTURE_DIM_UNDEFINED = 0,
    REI_TEXTURE_DIM_1D,
    REI_TEXTURE_DIM_2D,
    REI_TEXTURE_DIM_2DMS,
    REI_TEXTURE_DIM_3D,
    REI_TEXTURE_DIM_CUBE,
    REI_TEXTURE_DIM_1D_ARRAY,
    REI_TEXTURE_DIM_2D_ARRAY,
    REI_TEXTURE_DIM_2DMS_ARRAY,
    REI_TEXTURE_DIM_CUBE_ARRAY,
    REI_TEXTURE_DIM_COUNT
} REI_TextureDimension;

struct REI_VertexInput
{
    // The size of the attribute
    uint32_t size;

    // resource name
    const char* name;

    // name size
    uint32_t name_size;
};

struct REI_ShaderResource
{
    // resource Type
    REI_DescriptorType type;

    // The resource set for binding frequency
    uint32_t set;

    // The resource binding location
    uint32_t reg;

    // The size of the resource. This will be the DescriptorInfo array size for textures
    uint32_t size;

    // what stages use this resource
    uint32_t used_stages;

    // resource name
    const char* name;

    // name size
    uint32_t name_size;

    // 1D / 2D / Array / MSAA / ...
    REI_TextureDimension dim;
};

struct REI_ShaderReflection
{
    REI_ShaderStage shaderStage;

    // single large allocation for names to reduce number of allocations
    char*    pNamePool;
    uint32_t namePoolSize;

    REI_VertexInput* pVertexInputs;
    uint32_t         vertexInputsCount;

    REI_ShaderResource* pShaderResources;
    uint32_t            shaderResourceCount;

    // Thread group size for compute shader
    uint32_t numThreadsPerGroup[3];

    //number of tessellation control point
    uint32_t numControlPoint;

    char* pEntryPoint;
};

struct REI_PipelineReflection
{
    REI_ShaderStage mShaderStages;
    // the individual stages reflection data.
    REI_ShaderReflection mStageReflections[MAX_SHADER_STAGE_COUNT];
    uint32_t             mStageReflectionCount;

    uint32_t mVertexStageIndex;
    uint32_t mHullStageIndex;
    uint32_t mDomainStageIndex;
    uint32_t mGeometryStageIndex;
    uint32_t mPixelStageIndex;

    REI_ShaderResource* pShaderResources;
    uint32_t            mShaderResourceCount;
};

void REI_destroyShaderReflection(REI_ShaderReflection* pReflection);

void REI_createPipelineReflection(
    REI_ShaderReflection* pReflection, uint32_t stageCount, REI_PipelineReflection* pOutReflection);
void REI_destroyPipelineReflection(REI_PipelineReflection* pReflection);
