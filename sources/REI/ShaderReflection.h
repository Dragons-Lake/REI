/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 * Copyright (c) 2019-2022 Mykhailo Parfeniuk, Vladyslav Serhiienko.
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#pragma once
#include <stdio.h>
#include <ctype.h>
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

struct REI_ShaderVariable
{
    // Variable name
    const char* name;

    // parents resource index
    uint32_t parent_index;

    // The offset of the Variable.
    uint32_t offset;

    // The size of the Variable.
    uint32_t size;

    // name size
    uint32_t name_size;
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

    uint32_t variableCount;

    REI_ShaderVariable* pVariables;

    char* pEntryPoint;

    void* pMemChunk; // if not nullptr all memory was allocated in one chunk
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

    uint32_t variableCount;

    REI_ShaderVariable* pVariables;
};

struct REI_PipelineReflectionDesc
{
    REI_ShaderReflection*         pReflection;
    uint32_t                      stageCount;
    const REI_AllocatorCallbacks* pAllocator;
    REI_LogPtr                    pLog;
};

void REI_createPipelineReflection(const REI_PipelineReflectionDesc& desc, REI_PipelineReflection* pOutReflection);
void REI_destroyPipelineReflection(const REI_AllocatorCallbacks& allocator, REI_PipelineReflection* pReflection);

inline bool REI_isDescriptorRootConstant(const char* resourceName)
{
    char     lower[REI_MAX_RESOURCE_NAME_LENGTH] = {};
    uint32_t length = (uint32_t)strlen(resourceName);
    for (uint32_t i = 0; i < length; ++i)
    {
        lower[i] = (char)tolower(resourceName[i]);
    }
    return strstr(lower, "rootconstant") || strstr(lower, "pushconstant");
}

inline bool REI_isDescriptorRootCbv(const char* resourceName)
{
    char     lower[REI_MAX_RESOURCE_NAME_LENGTH] = {};
    uint32_t length = (uint32_t)strlen(resourceName);
    for (uint32_t i = 0; i < length; ++i)
    {
        lower[i] = (char)tolower(resourceName[i]);
    }
    return strstr(lower, "rootcbv");
}
