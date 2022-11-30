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
 * This file contains modified code from The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
 */

#include "RendererD3D12.h"
#include <dxcapi.h>
#include <d3d12shader.h>

#ifndef CHECK_HRESULT
#    define CHECK_HRESULT(exp)                                                          \
        do                                                                              \
        {                                                                               \
            HRESULT hres = (exp);                                                       \
            if (!SUCCEEDED(hres))                                                       \
            {                                                                           \
                REI_ASSERT(false, "%s: FAILED with HRESULT: %u", #exp, (uint32_t)hres); \
            }                                                                           \
        } while (0)
#endif

template<typename ID3D12ReflectionT, typename D3D12_SHADER_DESC_T>
void calculate_bound_resource_count(
    ID3D12ReflectionT* d3d12reflection, const D3D12_SHADER_DESC_T& shaderDesc, REI_ShaderReflection& reflection)
{
    //Get the number of bound resources
    reflection.shaderResourceCount = shaderDesc.BoundResources;

    //Count string sizes of the bound resources for the name pool
    for (UINT i = 0; i < shaderDesc.BoundResources; ++i)
    {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc;
        d3d12reflection->GetResourceBindingDesc(i, &bindDesc);
        reflection.namePoolSize += (uint32_t)strlen(bindDesc.Name) + 1;
    }

    //Count the number of variables and add to the size of the string pool
    for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
    {
        ID3D12ShaderReflectionConstantBuffer* buffer = d3d12reflection->GetConstantBufferByIndex(i);

        D3D12_SHADER_BUFFER_DESC bufferDesc;
        buffer->GetDesc(&bufferDesc);

        //We only care about constant buffers
        if (bufferDesc.Type != D3D_CT_CBUFFER)
            continue;

        for (UINT v = 0; v < bufferDesc.Variables; ++v)
        {
            ID3D12ShaderReflectionVariable* variable = buffer->GetVariableByIndex(v);

            D3D12_SHADER_VARIABLE_DESC varDesc;
            variable->GetDesc(&varDesc);

            //Only count used variables
            if ((varDesc.uFlags | D3D_SVF_USED) != 0)
            {
                reflection.namePoolSize += (uint32_t)strlen(varDesc.Name) + 1;
                reflection.variableCount++;
            }
        }
    }
}

template<typename ID3D12ReflectionT, typename D3D12_SHADER_DESC_T>
static void fill_shader_resources(
    const REI_AllocatorCallbacks& allocator, ID3D12ReflectionT* d3d12reflection, const D3D12_SHADER_DESC_T& shaderDesc,
    REI_ShaderStage shaderStage, char* pCurrentName, REI_ShaderReflection& reflection)
{
    reflection.pShaderResources = NULL;
    if (reflection.shaderResourceCount > 0)
    {
        reflection.pShaderResources = (REI_ShaderResource*)allocator.pMalloc(
            allocator.pUserData, sizeof(REI_ShaderResource) * reflection.shaderResourceCount, 0);

        for (uint32_t i = 0; i < reflection.shaderResourceCount; ++i)
        {
            D3D12_SHADER_INPUT_BIND_DESC bindDesc;
            d3d12reflection->GetResourceBindingDesc(i, &bindDesc);
            uint32_t len = (uint32_t)strlen(bindDesc.Name);

            reflection.pShaderResources[i].type = REI_sD3D12_TO_DESCRIPTOR[bindDesc.Type];
            reflection.pShaderResources[i].set = bindDesc.Space;
            reflection.pShaderResources[i].reg = bindDesc.BindPoint;
            reflection.pShaderResources[i].size = bindDesc.BindCount;
            reflection.pShaderResources[i].used_stages = shaderStage;
            reflection.pShaderResources[i].name = pCurrentName;
            reflection.pShaderResources[i].name_size = len;
            reflection.pShaderResources[i].dim = REI_sD3D12_TO_RESOURCE_DIM[bindDesc.Dimension];

            // RWTyped is considered as DESCRIPTOR_TYPE_TEXTURE by default so we handle the case for RWBuffer here
            if (bindDesc.Type == D3D_SHADER_INPUT_TYPE::D3D_SIT_UAV_RWTYPED &&
                bindDesc.Dimension == D3D_SRV_DIMENSION_BUFFER)
            {
                reflection.pShaderResources[i].type = REI_DESCRIPTOR_TYPE_RW_BUFFER;
            }
            // Buffer<> is considered as DESCRIPTOR_TYPE_TEXTURE by default so we handle the case for Buffer<> here
            if (bindDesc.Type == D3D_SHADER_INPUT_TYPE::D3D_SIT_TEXTURE &&
                bindDesc.Dimension == D3D_SRV_DIMENSION_BUFFER)
            {
                reflection.pShaderResources[i].type = REI_DESCRIPTOR_TYPE_BUFFER;
            }

            //Copy over the name
            memcpy(pCurrentName, bindDesc.Name, len);
            pCurrentName[len] = '\0';
            pCurrentName += len + 1;
        }
    }

    if (reflection.variableCount > 0)
    {
        reflection.pVariables = (REI_ShaderVariable*)allocator.pMalloc(
            allocator.pUserData, sizeof(REI_ShaderVariable) * reflection.variableCount, 0);

        UINT v = 0;
        for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i)
        {
            //Get the constant buffer
            ID3D12ShaderReflectionConstantBuffer* buffer = d3d12reflection->GetConstantBufferByIndex(i);

            //Get the constant buffer description
            D3D12_SHADER_BUFFER_DESC bufferDesc;
            buffer->GetDesc(&bufferDesc);

            //We only care about constant buffers
            if (bufferDesc.Type != D3D_CT_CBUFFER)
                continue;

            //Find the resource index for the constant buffer
            uint32_t resourceIndex = ~0u;
            for (UINT r = 0; r < shaderDesc.BoundResources; ++r)
            {
                D3D12_SHADER_INPUT_BIND_DESC inputDesc;
                d3d12reflection->GetResourceBindingDesc(r, &inputDesc);

                if (inputDesc.Type == D3D_SIT_CBUFFER && strcmp(inputDesc.Name, bufferDesc.Name) == 0)
                {
                    resourceIndex = r;
                    break;
                }
            }
            REI_ASSERT(resourceIndex != ~0u);

            //Go through all the variables in the constant buffer
            for (UINT j = 0; j < bufferDesc.Variables; ++j)
            {
                //Get the variable
                ID3D12ShaderReflectionVariable* variable = buffer->GetVariableByIndex(j);

                //Get the variable description
                D3D12_SHADER_VARIABLE_DESC varDesc;
                variable->GetDesc(&varDesc);

                //If the variable is used in the shader
                if ((varDesc.uFlags | D3D_SVF_USED) != 0)
                {
                    uint32_t len = (uint32_t)strlen(varDesc.Name);

                    reflection.pVariables[v].parent_index = resourceIndex;
                    reflection.pVariables[v].offset = varDesc.StartOffset;
                    reflection.pVariables[v].size = varDesc.Size;
                    reflection.pVariables[v].name = pCurrentName;
                    reflection.pVariables[v].name_size = len;

                    //Copy over the name
                    memcpy(pCurrentName, varDesc.Name, len);
                    pCurrentName[len] = '\0';
                    pCurrentName += len + 1;

                    ++v;
                }
            }
        }
    }
}

static void d3d12_createShaderReflection(
    const REI_AllocatorCallbacks& allocator, ID3D12ShaderReflection* d3d12reflection, REI_ShaderStage shaderStage,
    REI_ShaderReflection& reflection)
{
    //Get a description of this shader
    D3D12_SHADER_DESC shaderDesc;
    d3d12reflection->GetDesc(&shaderDesc);

    calculate_bound_resource_count(d3d12reflection, shaderDesc, reflection);

    //Get the number of input parameters
    reflection.vertexInputsCount = 0;

    if (shaderStage == REI_SHADER_STAGE_VERT)
    {
        reflection.vertexInputsCount = shaderDesc.InputParameters;

        //Count the string sizes of the vertex inputs for the name pool
        for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
        {
            D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
            d3d12reflection->GetInputParameterDesc(i, &paramDesc);
            reflection.namePoolSize += (uint32_t)strlen(paramDesc.SemanticName) + 2;
        }
    }
    //Get the number of threads per group
    else if (shaderStage == REI_SHADER_STAGE_COMP)
    {
        d3d12reflection->GetThreadGroupSize(
            &reflection.numThreadsPerGroup[0], &reflection.numThreadsPerGroup[1], &reflection.numThreadsPerGroup[2]);
    }
    //Get the number of cnotrol point
    else if (shaderStage == REI_SHADER_STAGE_TESC)
    {
        reflection.numControlPoint = shaderDesc.cControlPoints;
    }

    //Allocate memory for the name pool
    if (reflection.namePoolSize)
        reflection.pNamePool = (char*)REI_calloc(allocator, reflection.namePoolSize);
    char* pCurrentName = reflection.pNamePool;

    reflection.pVertexInputs = NULL;
    if (shaderStage == REI_SHADER_STAGE_VERT && reflection.vertexInputsCount > 0)
    {
        reflection.pVertexInputs = (REI_VertexInput*)allocator.pMalloc(
            allocator.pUserData, sizeof(REI_VertexInput) * reflection.vertexInputsCount, 0);

        for (UINT i = 0; i < shaderDesc.InputParameters; ++i)
        {
            D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
            d3d12reflection->GetInputParameterDesc(i, &paramDesc);

            //Get the length of the semantic name
            bool     hasParamIndex = paramDesc.SemanticIndex > 0 || !strcmp(paramDesc.SemanticName, "TEXCOORD");
            uint32_t len = (uint32_t)strlen(paramDesc.SemanticName) + (hasParamIndex ? 1 : 0);

            if (hasParamIndex)
            {
                sprintf(pCurrentName, "%s%u", paramDesc.SemanticName, paramDesc.SemanticIndex);
            }
            else
            {
                sprintf(pCurrentName, "%s", paramDesc.SemanticName);
            }

            reflection.pVertexInputs[i].name = pCurrentName;
            reflection.pVertexInputs[i].name_size = len;
            reflection.pVertexInputs[i].size = (uint32_t)log2(paramDesc.Mask + 1) * sizeof(uint8_t[4]);

            //Copy over the name into the name pool
            pCurrentName += len + 1;    //move the name pointer through the name pool
        }
    }

    fill_shader_resources(allocator, d3d12reflection, shaderDesc, shaderStage, pCurrentName, reflection);
}

static void d3d12_createShaderReflection(
    const REI_AllocatorCallbacks& allocator, REI_LogPtr pLog, const uint8_t* shaderCode, uint32_t shaderSize,
    REI_ShaderStage shaderStage, REI_ShaderReflection* pOutReflection)
{
    //Check to see if parameters are valid
    if (shaderCode == NULL)
    {
        pLog(REI_LOG_TYPE_ERROR, "Parameter 'shaderCode' was NULL.");
        return;
    }
    if (shaderSize == 0)
    {
        pLog(REI_LOG_TYPE_ERROR, "Parameter 'shaderSize' was 0.");
        return;
    }
    if (pOutReflection == NULL)
    {
        pLog(REI_LOG_TYPE_ERROR, "Paramater 'pOutReflection' was NULL.");
        return;
    }

    //Run the D3D12 shader reflection on the compiled shader
    ID3D12LibraryReflection* d3d12LibReflection = NULL;
    ID3D12ShaderReflection*  d3d12reflection = NULL;

    IDxcLibrary* pLibrary = NULL;
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pLibrary));
    IDxcBlobEncoding* pBlob = NULL;
    pLibrary->CreateBlobWithEncodingFromPinned((LPBYTE)shaderCode, (UINT32)shaderSize, 0, &pBlob);
#define DXIL_FOURCC(ch0, ch1, ch2, ch3)                                                          \
    ((uint32_t)(uint8_t)(ch0) | (uint32_t)(uint8_t)(ch1) << 8 | (uint32_t)(uint8_t)(ch2) << 16 | \
     (uint32_t)(uint8_t)(ch3) << 24)

    IDxcContainerReflection* pReflection;
    UINT32                   shaderIdx;
    DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(&pReflection));
    CHECK_HRESULT(pReflection->Load(pBlob));
    (pReflection->FindFirstPartKind(DXIL_FOURCC('D', 'X', 'I', 'L'), &shaderIdx));

    CHECK_HRESULT(pReflection->GetPartReflection(shaderIdx, IID_PPV_ARGS(&d3d12reflection)));

    pBlob->Release();
    pLibrary->Release();
    pReflection->Release();

    //Allocate our internal shader reflection structure on the stack
    REI_ShaderReflection reflection = {};    //initialize the struct to 0

    d3d12_createShaderReflection(allocator, d3d12reflection, shaderStage, reflection);
    d3d12reflection->Release();    //-V522

    reflection.shaderStage = shaderStage;

    //Copy the shader reflection data to the output variable
    *pOutReflection = reflection;
}