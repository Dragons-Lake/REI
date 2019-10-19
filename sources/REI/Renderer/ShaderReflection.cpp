/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#include "Renderer.h"
#include "../Interface/Common.h"

//This file contains shader reflection code that is the same for all platforms.
//We know it's the same for all platforms since it only interacts with the
// platform abstractions we created.

#define RESOURCE_NAME_CHECK
static bool ShaderResourceCmp(REI_ShaderResource* a, REI_ShaderResource* b)
{
    bool isSame = true;

    isSame = isSame && (a->type == b->type);
    isSame = isSame && (a->set == b->set);
    isSame = isSame && (a->reg == b->reg);

#ifdef RESOURCE_NAME_CHECK
    // we may not need this, the rest is enough but if we want to be super sure we can do this check
    isSame = isSame && (a->name_size == b->name_size);
    // early exit before string cmp
    if (isSame == false)
        return isSame;

    isSame = isSame && (strcmp(a->name, b->name) == 0);
#endif
    return isSame;
}

void REI_destroyShaderReflection(REI_ShaderReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    REI_free(pReflection->pNamePool);
    REI_free(pReflection->pVertexInputs);
    REI_free(pReflection->pShaderResources);
}

void REI_createPipelineReflection(
    REI_ShaderReflection* pReflection, uint32_t stageCount, REI_PipelineReflection* pOutReflection)
{
    //Parameter checks
    if (pReflection == NULL)
    {
        REI_LOG(ERROR, "Parameter 'pReflection' is NULL.");
        return;
    }
    if (stageCount == 0)
    {
        REI_LOG(ERROR, "Parameter 'stageCount' is 0.");
        return;
    }
    if (pOutReflection == NULL)
    {
        REI_LOG(ERROR, "Parameter 'pOutShaderReflection' is NULL.");
        return;
    }

    // Sanity check to make sure we don't have repeated stages.
    REI_ShaderStage combinedShaderStages = (REI_ShaderStage)0;
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        if ((combinedShaderStages & pReflection[i].shaderStage) != 0)
        {
            REI_LOG(ERROR, "Duplicate shader stage was detected in shader reflection array.");
            return;
        }
        combinedShaderStages = (REI_ShaderStage)(combinedShaderStages | pReflection[i].shaderStage);
    }

    // Combine all shaders
    // this will have a large amount of looping
    // 1. count number of resources
    uint32_t            vertexStageIndex = ~0u;
    uint32_t            hullStageIndex = ~0u;
    uint32_t            domainStageIndex = ~0u;
    uint32_t            geometryStageIndex = ~0u;
    uint32_t            pixelStageIndex = ~0u;
    REI_ShaderResource* pResources = NULL;
    uint32_t            resourceCount = 0;

    //Should we be using dynamic arrays for these? Perhaps we can add std::vector
    // like functionality?
    REI_ShaderResource* uniqueResources[512];
    uint32_t            shaderUsage[512];
    for (uint32_t i = 0; i < stageCount; ++i)
    {
        REI_ShaderReflection* pSrcRef = pReflection + i;
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
        pResources = (REI_ShaderResource*)REI_calloc(resourceCount, sizeof(REI_ShaderResource));

        for (uint32_t i = 0; i < resourceCount; ++i)
        {
            pResources[i] = *uniqueResources[i];
            pResources[i].used_stages = shaderUsage[i];
        }
    }

    // all refection structs should be built now
    pOutReflection->mShaderStages = combinedShaderStages;

    pOutReflection->mStageReflectionCount = stageCount;

    pOutReflection->mVertexStageIndex = vertexStageIndex;
    pOutReflection->mHullStageIndex = hullStageIndex;
    pOutReflection->mDomainStageIndex = domainStageIndex;
    pOutReflection->mGeometryStageIndex = geometryStageIndex;
    pOutReflection->mPixelStageIndex = pixelStageIndex;

    pOutReflection->pShaderResources = pResources;
    pOutReflection->mShaderResourceCount = resourceCount;
}

void REI_destroyPipelineReflection(REI_PipelineReflection* pReflection)
{
    if (pReflection == NULL)
        return;

    for (uint32_t i = 0; i < pReflection->mStageReflectionCount; ++i)
        REI_destroyShaderReflection(&pReflection->mStageReflections[i]);

    REI_free(pReflection->pShaderResources);
}
