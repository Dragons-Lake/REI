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


#include "Renderer.h"
#include "ShaderReflection.h"
#include "Common.h"

//This file contains shader reflection code that is the same for all platforms.
//We know it's the same for all platforms since it only interacts with the
// platform abstractions we created.

#define RESOURCE_NAME_CHECK
static bool ShaderResourceCmp(REI_ShaderResource* a, REI_ShaderResource* b)
{
    return (a->type == b->type) && (a->set == b->set) && (a->reg == b->reg)
#ifdef RESOURCE_NAME_CHECK
    // we may not need this, the rest is enough but if we want to be super sure we can do this check
    && (a->name_size == b->name_size) 
    && ((a->name_size != 0 && b->name_size != 0) ? strcmp(a->name, b->name) == 0 : true)
#endif
    ;
}

static bool ShaderVariableCmp(REI_ShaderVariable* a, REI_ShaderVariable* b)
{
    return (a->offset == b->offset) && (a->size == b->size) && (a->name_size == b->name_size)
    && (strcmp(a->name, b->name) == 0);
}

void REI_destroyShaderReflection(const REI_AllocatorCallbacks& allocator, REI_ShaderReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    if (pReflection->pMemChunk)
    {
        allocator.pFree(allocator.pUserData, pReflection->pMemChunk);
        return;
    }

    allocator.pFree(allocator.pUserData, pReflection->pNamePool);
    allocator.pFree(allocator.pUserData, pReflection->pVertexInputs);
    allocator.pFree(allocator.pUserData, pReflection->pShaderResources);
    allocator.pFree(allocator.pUserData, pReflection->pVariables);
}

void REI_createPipelineReflection(const REI_PipelineReflectionDesc& desc, REI_PipelineReflection* pOutReflection)
{
    //Parameter checks
    if (desc.pReflection == NULL)
    {
        desc.pLog(REI_LOG_TYPE_ERROR, "Parameter 'pReflection' is NULL.");
        return;
    }
    if (desc.stageCount == 0)
    {
        desc.pLog(REI_LOG_TYPE_ERROR, "Parameter 'stageCount' is 0.");
        return;
    }
    if (pOutReflection == NULL)
    {
        desc.pLog(REI_LOG_TYPE_ERROR, "Parameter 'pOutShaderReflection' is NULL.");
        return;
    }

    // Sanity check to make sure we don't have repeated stages.
    REI_ShaderStage combinedShaderStages = (REI_ShaderStage)0;
    for (uint32_t i = 0; i < desc.stageCount; ++i)
    {
        if ((combinedShaderStages & desc.pReflection[i].shaderStage) != 0)
        {
            desc.pLog(REI_LOG_TYPE_ERROR, "Duplicate shader stage was detected in shader reflection array.");
            return;
        }
        combinedShaderStages = (REI_ShaderStage)(combinedShaderStages | desc.pReflection[i].shaderStage);
    }

    const REI_AllocatorCallbacks& allocator = *desc.pAllocator;

    // Combine all shaders
    // this will have a large amount of looping
    // 1. count number of resources
    uint32_t             vertexStageIndex = ~0u;
    uint32_t             hullStageIndex = ~0u;
    uint32_t             domainStageIndex = ~0u;
    uint32_t             geometryStageIndex = ~0u;
    uint32_t             pixelStageIndex = ~0u;
    REI_ShaderResource*  pResources = NULL;
    uint32_t             resourceCount = 0;
    REI_ShaderVariable*  pVariables = NULL;
    uint32_t             variableCount = 0;

    REI_ShaderVariable** uniqueVariable =
        (REI_ShaderVariable**)REI_calloc(allocator, 512 * sizeof(REI_ShaderVariable*));
    REI_ShaderResource** uniqueVariableParent =
        (REI_ShaderResource**)REI_calloc(allocator, 512 * sizeof(REI_ShaderResource*));
    REI_ShaderResource** uniqueResources =
        (REI_ShaderResource**)REI_calloc(allocator, 512 * sizeof(REI_ShaderResource*));

    uint32_t* shaderUsage = (uint32_t*)allocator.pMalloc(allocator.pUserData, 512 * sizeof(uint32_t), 0);

    for (uint32_t i = 0; i < desc.stageCount; ++i)
    {
        REI_ShaderReflection* pSrcRef = desc.pReflection + i;
        pOutReflection->mStageReflections[i] = *pSrcRef;

        if (pSrcRef->shaderStage == REI_SHADER_STAGE_VERT)
        {
            vertexStageIndex = i;
        }
#if !defined(METAL)
        else if (pSrcRef->shaderStage == REI_SHADER_STAGE_TESC)
        {
            hullStageIndex = i;
        }
        else if (pSrcRef->shaderStage == REI_SHADER_STAGE_TESE)
        {
            domainStageIndex = i;
        }
        else if (pSrcRef->shaderStage == REI_SHADER_STAGE_GEOM)
        {
            geometryStageIndex = i;
        }
#endif
        else if (pSrcRef->shaderStage == REI_SHADER_STAGE_FRAG)
        {
            pixelStageIndex = i;
        }

        //Loop through all shader resources
        for (uint32_t j = 0; j < pSrcRef->shaderResourceCount; ++j)
        {
            bool unique = true;

            //Go through all already added shader resources to see if this shader
            // resource was already added from a different shader stage. If we find a
            // duplicate shader resource, we add the shader stage to the shader stage
            // mask of that resource instead.
            for (uint32_t k = 0; k < resourceCount; ++k)
            {
                unique = !ShaderResourceCmp(&pSrcRef->pShaderResources[j], uniqueResources[k]);
                if (unique == false)
                {
                    // update shader usage
                    // NOT SURE
                    //shaderUsage[k] = (ShaderStage)(shaderUsage[k] & pSrcRef->pShaderResources[j].used_stages);
                    shaderUsage[k] |= pSrcRef->pShaderResources[j].used_stages;
                    break;
                }
            }

            //Loop through all shader variables (constant/uniform buffer members)
            for (uint32_t j = 0; j < pSrcRef->variableCount; ++j)
            {
                bool unique = true;
                //Go through all already added shader variables to see if this shader
                // variable was already added from a different shader stage. If we find a
                // duplicate shader variables, we don't add it.
                for (uint32_t k = 0; k < variableCount; ++k)
                {
                    unique = !ShaderVariableCmp(&pSrcRef->pVariables[j], uniqueVariable[k]);
                    if (unique == false)
                        break;
                }

                //If it's unique we add it to the list of shader variables
                if (unique)
                {
                    uniqueVariableParent[variableCount] = &pSrcRef->pShaderResources[pSrcRef->pVariables[j].parent_index];
                    uniqueVariable[variableCount] = &pSrcRef->pVariables[j];
                    variableCount++;
                }
            }

            //If it's unique, we add it to the list of shader resourceas
            if (unique == true)
            {
                shaderUsage[resourceCount] = pSrcRef->pShaderResources[j].used_stages;
                uniqueResources[resourceCount] = &pSrcRef->pShaderResources[j];
                resourceCount++;
            }
        }
    }

    //Copy over the shader resources in a dynamic array of the correct size
    if (resourceCount)
    {
        pResources = (REI_ShaderResource*)REI_calloc(allocator, resourceCount * sizeof(REI_ShaderResource));

        for (uint32_t i = 0; i < resourceCount; ++i)
        {
            pResources[i] = *uniqueResources[i];
            pResources[i].used_stages = shaderUsage[i];
        }
    }

    //Copy over the shader variables in a dynamic array of the correct size
    if (variableCount)
    {
        pVariables = (REI_ShaderVariable*)allocator.pMalloc(
            allocator.pUserData, sizeof(REI_ShaderVariable) * variableCount, 0);

        for (uint32_t i = 0; i < variableCount; ++i)
        {
            pVariables[i] = *uniqueVariable[i];
            REI_ShaderResource* parentResource = uniqueVariableParent[i];
            // look for parent
            for (uint32_t j = 0; j < resourceCount; ++j)
            {
                if (ShaderResourceCmp(&pResources[j], parentResource))    //-V522
                {
                    pVariables[i].parent_index = j;
                    break;
                }
            }
        }
    }

    // all refection structs should be built now
    pOutReflection->mShaderStages = combinedShaderStages;

    pOutReflection->mStageReflectionCount = desc.stageCount;

    pOutReflection->mVertexStageIndex = vertexStageIndex;
    pOutReflection->mHullStageIndex = hullStageIndex;
    pOutReflection->mDomainStageIndex = domainStageIndex;
    pOutReflection->mGeometryStageIndex = geometryStageIndex;
    pOutReflection->mPixelStageIndex = pixelStageIndex;

    pOutReflection->pShaderResources = pResources;
    pOutReflection->mShaderResourceCount = resourceCount;

    pOutReflection->variableCount = variableCount;
    pOutReflection->pVariables = pVariables;

    allocator.pFree(allocator.pUserData, uniqueVariable);
    allocator.pFree(allocator.pUserData, uniqueVariableParent);
    allocator.pFree(allocator.pUserData, uniqueResources);
    allocator.pFree(allocator.pUserData, shaderUsage);
}

void REI_destroyPipelineReflection(const REI_AllocatorCallbacks& allocator, REI_PipelineReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    for (uint32_t i = 0; i < pReflection->mStageReflectionCount; ++i)
        REI_destroyShaderReflection(allocator, &pReflection->mStageReflections[i]);

    allocator.pFree(allocator.pUserData, pReflection->pShaderResources);
    allocator.pFree(allocator.pUserData, pReflection->pVariables);
}
