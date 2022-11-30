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

#include "RendererVk.h"
#include "3rdParty/spirv_reflect/spirv_reflect.h"
#include <cassert>

static REI_DescriptorType to_descriptor_type(SpvReflectDescriptorType type)
{
    switch (type)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return REI_DESCRIPTOR_TYPE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return REI_DESCRIPTOR_TYPE_TEXTURE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return REI_DESCRIPTOR_TYPE_RW_TEXTURE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return REI_DESCRIPTOR_TYPE_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return REI_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return REI_DESCRIPTOR_TYPE_RW_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return REI_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        //case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        default: return REI_DESCRIPTOR_TYPE_UNDEFINED;
    }
}

static uint32_t calc_array_size(SpvReflectBindingArrayTraits& array)
{
    uint32_t size = 1;
    for (uint32_t i = 0; i < array.dims_count; ++i)
        size *= array.dims[i];
    return size;
}

static REI_TextureDimension to_resource_dim(SpvReflectImageTraits& image)
{
    switch (image.dim)
    {
        case SpvDim1D: return image.arrayed ? REI_TEXTURE_DIM_1D_ARRAY : REI_TEXTURE_DIM_1D;
        case SpvDim2D:
            if (image.ms)
                return image.arrayed ? REI_TEXTURE_DIM_2DMS_ARRAY : REI_TEXTURE_DIM_2DMS;
            else
                return image.arrayed ? REI_TEXTURE_DIM_2D_ARRAY : REI_TEXTURE_DIM_2D;
        case SpvDim3D: return REI_TEXTURE_DIM_3D;
        case SpvDimCube: return image.arrayed ? REI_TEXTURE_DIM_CUBE_ARRAY : REI_TEXTURE_DIM_CUBE;
        default: return REI_TEXTURE_DIM_UNDEFINED;
    }
}

static uint32_t calc_var_size(SpvReflectInterfaceVariable& var)
{
    switch (var.type_description->op)
    {
        case SpvOpTypeBool:
        {
            return 4;
        }
        break;

        case SpvOpTypeInt:
        case SpvOpTypeFloat:
        {
            return var.numeric.scalar.width / 8;
        }
        break;

        case SpvOpTypeVector:
        {
            return var.numeric.vector.component_count * (var.numeric.scalar.width / 8);
        }
        break;

        case SpvOpTypeMatrix:
        {
            if (var.decoration_flags & SPV_REFLECT_DECORATION_COLUMN_MAJOR)
            {
                return var.numeric.matrix.column_count * var.numeric.matrix.stride;
            }
            else if (var.decoration_flags & SPV_REFLECT_DECORATION_ROW_MAJOR)
            {
                return var.numeric.matrix.row_count * var.numeric.matrix.stride;
            }
        }
        break;

        case SpvOpTypeArray:
        {
            // If array of structs, parse members first...
            bool is_struct =
                (var.type_description->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) == SPV_REFLECT_TYPE_FLAG_STRUCT;
            if (is_struct)
            {
                assert(false);
                break;
            }
            // ...then array
            uint32_t element_count = (var.array.dims_count > 0 ? 1 : 0);
            for (uint32_t i = 0; i < var.array.dims_count; ++i)
            {
                element_count *= var.array.dims[i];
            }
            return element_count * var.array.stride;
        }
        break;
        default: assert(false);
    }
    return 0;
}

static void vk_createShaderReflection(
    const REI_AllocatorCallbacks& allocator, REI_LogPtr pLog, const uint8_t* shaderCode, uint32_t shaderSize,
    REI_ShaderStage shaderStage, REI_ShaderReflection* pOutReflection)
{
    if (pOutReflection == NULL)
    {
        pLog(REI_LOG_TYPE_ERROR, "Create Shader Refection failed. Invalid reflection output!");
        return;    // TODO: error msg
    }

    SpvReflectShaderModule module;

    if (spvReflectCreateShaderModule(shaderSize, shaderCode, &module) != SPV_REFLECT_RESULT_SUCCESS)
    {
        pLog(REI_LOG_TYPE_ERROR, "Create Shader Refection failed. Invalid input!");
        return;    // TODO: error msg
    }

    SpvReflectEntryPoint& entryPoint = module.entry_points[0];

    if (entryPoint.shader_stage == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
    {
        pOutReflection->numThreadsPerGroup[0] = entryPoint.local_size.x;
        pOutReflection->numThreadsPerGroup[1] = entryPoint.local_size.y;
        pOutReflection->numThreadsPerGroup[2] = entryPoint.local_size.z;
    }
    else if (entryPoint.shader_stage == SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
    {
        pOutReflection->numControlPoint = entryPoint.output_vertices;
    }

    // lets find out the size of the name pool we need
    // also get number of resources while we are at it
    uint32_t resouceCount = 0;
    size_t   entryPointSize = strlen(entryPoint.name);
    size_t   namePoolSize = 0;
    namePoolSize += entryPointSize + 1;

    // vertex inputs
    if (entryPoint.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
    {
        for (uint32_t i = 0; i < entryPoint.input_variable_count; ++i)
        {
            SpvReflectInterfaceVariable& v = entryPoint.input_variables[i];
            if (v.built_in != -1)
                continue;

            if (v.name)
                namePoolSize += strlen(v.name) + 1;
        }
    }

    // descriptors
    for (uint32_t j = 0; j < entryPoint.descriptor_set_count; ++j)
    {
        SpvReflectDescriptorSet& ds = entryPoint.descriptor_sets[j];
        resouceCount += ds.binding_count;
        for (uint32_t i = 0; i < ds.binding_count; ++i)
        {
            SpvReflectDescriptorBinding* b = ds.bindings[i];

            // filter out what we don't use
            namePoolSize += strlen(b->name) + 1;
        }
    }

    // push constants
    for (uint32_t i = 0; i < entryPoint.used_push_constant_count; ++i)
    {
        uint32_t id = entryPoint.used_push_constants[i];
        for (uint32_t j = 0; j < module.push_constant_block_count; ++j)
        {
            SpvReflectBlockVariable& b = module.push_constant_blocks[j];
            if (b.spirv_id == id)
            {
                namePoolSize += strlen(b.name) + 1;
                ++resouceCount;
            }
        }
    }

    REI_StackAllocator<false> persistentAlloc = { 0 };
    persistentAlloc.reserve<REI_RootSignature>()
        .reserve<char>(namePoolSize)
        .reserve<REI_VertexInput>(entryPoint.input_variable_count)
        .reserve<REI_ShaderResource>(resouceCount);

    if (persistentAlloc.size != 0 && !persistentAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "vk_createShaderReflection wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        memset(pOutReflection, 0, sizeof(*pOutReflection));
        return;
    }

    pOutReflection->pMemChunk = persistentAlloc.ptr;

    // we now have the size of the memory pool and number of resources
    char* namePool = NULL;
    if (namePoolSize)
        namePool = persistentAlloc.allocZeroed<char>(namePoolSize);    // should be zeroed
    char* pCurrentName = namePool;

    pOutReflection->pEntryPoint = pCurrentName;
    memcpy(pCurrentName, entryPoint.name, entryPointSize);
    pCurrentName += entryPointSize + 1;

    uint32_t         vertexInputCount = 0;
    REI_VertexInput* pVertexInputs = NULL;
    // start with the vertex input
    if (shaderStage == REI_SHADER_STAGE_VERT && entryPoint.input_variable_count > 0)
    {
        pVertexInputs = persistentAlloc.alloc<REI_VertexInput>(entryPoint.input_variable_count);

        for (uint32_t i = 0; i < entryPoint.input_variable_count; ++i)
        {
            SpvReflectInterfaceVariable& v = entryPoint.input_variables[i];
            if (v.built_in != -1)
                continue;

            pVertexInputs[i].size = calc_var_size(v);
            pVertexInputs[i].name = pCurrentName;
            if (v.name)
            {
                uint32_t name_len = (uint32_t)strlen(v.name);
                pVertexInputs[i].name_size = name_len;
                // we dont own the names memory we need to copy it to the name pool
                memcpy(pCurrentName, v.name, name_len);
                pCurrentName += name_len + 1;
            }

            ++vertexInputCount;
        }
    }

    REI_ShaderResource* pResources = NULL;
    // continue with resources
    if (resouceCount)
    {
        pResources = persistentAlloc.alloc<REI_ShaderResource>(resouceCount);

        uint32_t r = 0;
        for (uint32_t i = 0; i < entryPoint.descriptor_set_count; ++i)
        {
            SpvReflectDescriptorSet& ds = entryPoint.descriptor_sets[i];
            for (uint32_t j = 0; j < ds.binding_count; ++j)
            {
                SpvReflectDescriptorBinding* b = ds.bindings[j];
                pResources[r].type = to_descriptor_type(b->descriptor_type);
                pResources[r].set = ds.set;
                pResources[r].reg = b->binding;
                pResources[r].size = calc_array_size(b->array);    //resource->size;
                pResources[r].used_stages = shaderStage;

                pResources[r].name = pCurrentName;
                size_t name_len = strlen(b->name);
                pResources[r].name_size = (uint32_t)name_len;
                pResources[r].dim = to_resource_dim(b->image);
                // we dont own the names memory we need to copy it to the name pool
                memcpy(pCurrentName, b->name, name_len);
                pCurrentName += name_len + 1;
                ++r;
            }
        }

        for (uint32_t i = 0; i < entryPoint.used_push_constant_count; ++i)
        {
            uint32_t id = entryPoint.used_push_constants[i];
            for (uint32_t j = 0; j < module.push_constant_block_count; ++j)
            {
                SpvReflectBlockVariable& b = module.push_constant_blocks[j];
                if (b.spirv_id == id)
                {
                    pResources[r].type = REI_DESCRIPTOR_TYPE_ROOT_CONSTANT;
                    pResources[r].set = UINT32_MAX;
                    pResources[r].reg = b.offset;
                    pResources[r].size = b.size;
                    pResources[r].used_stages = shaderStage;

                    pResources[r].name = pCurrentName;
                    size_t name_len = strlen(b.name);
                    pResources[r].name_size = (uint32_t)name_len;
                    pResources[r].dim = REI_TEXTURE_DIM_UNDEFINED;
                    // we dont own the names memory we need to copy it to the name pool
                    memcpy(pCurrentName, b.name, name_len);
                    pCurrentName += name_len + 1;
                    ++r;
                }
            }
        }

        assert(r == resouceCount);
    }

    // all refection structs should be built now
    pOutReflection->shaderStage = shaderStage;

    pOutReflection->pNamePool = namePool;
    pOutReflection->namePoolSize = (uint32_t)namePoolSize;

    pOutReflection->pVertexInputs = pVertexInputs;
    pOutReflection->vertexInputsCount = vertexInputCount;

    pOutReflection->pShaderResources = pResources;
    pOutReflection->shaderResourceCount = resouceCount;

    spvReflectDestroyShaderModule(&module);
}