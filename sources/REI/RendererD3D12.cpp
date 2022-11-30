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
 * This file contains modified code from The-Forge project source code
 * (see https://github.com/ConfettiFX/The-Forge).
 */

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include "RendererD3D12.h"
#include "Common.h"
#include "Thread.h"
#include <algorithm>
#include <wchar.h>
#include <stdint.h>

#define D3D12MA_IMPLEMENTATION
#define D3D12MA_D3D12_HEADERS_ALREADY_INCLUDED
#if REI_PLATFORM_XBOX
#    define DISABLE_HEAP_FLAG_ALLOW_SHADER_ATOMICS
#endif
#include "3rdParty/D3D12MemoryAllocator/D3D12MemoryAllocator.h"

#ifndef D3D12_GPU_VIRTUAL_ADDRESS_NULL
#   define D3D12_GPU_VIRTUAL_ADDRESS_NULL 0
#endif
#define D3D12_DESCRIPTOR_ID_NONE ((int32_t)-1)
#define D3D12_REQ_CONSTANT_BUFFER_SIZE (D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16u)

#if defined(__cplusplus)
#    define DECLARE_ZERO(type, var) type var = {};
#else
#    define DECLARE_ZERO(type, var) type var = { 0 };
#endif

// Already defined in D3D12MemoryAllocator.h lets keep it just in case
#ifndef SAFE_RELEASE
#    define SAFE_RELEASE(ptr)     \
        do                        \
        {                         \
            if (ptr)              \
            {                     \
                (ptr)->Release(); \
                (ptr) = NULL;     \
            }                     \
        } while (false)
#endif

#define CHECK_HRESULT(exp)                                                          \
    do                                                                              \
    {                                                                               \
        HRESULT hres = (exp);                                                       \
        if (!SUCCEEDED(hres))                                                       \
        {                                                                           \
            REI_ASSERT(false, "%s: FAILED with HRESULT: %u", #exp, (uint32_t)hres); \
        }                                                                           \
    } while (0)

#define CALC_SUBRESOURCE_INDEX(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize) \
    ((MipSlice) + ((ArraySlice) * (MipLevels)) + ((PlaneSlice) * (MipLevels) * (ArraySize)))

#ifdef _DEBUG
#    define VALIDATE_DESCRIPTOR(descriptor, msg, ...)                                                   \
        if (!(descriptor))                                                                              \
        {                                                                                               \
            REI_ASSERT(false, "Descriptor validation failed : %s : " msg, __FUNCTION__, ##__VA_ARGS__); \
            continue;                                                                                   \
        }
#else
#    define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

typedef struct DescriptorHeap
{
    /// DX Heap
    ID3D12DescriptorHeap* pHeap;
    /// Lock for multi-threaded descriptor allocations
    Mutex*        pMutex;
    ID3D12Device* pDevice;
    /// Start position in the heap
    D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandle;
    // Bitmask to track free regions (set bit means occupied)
    uint32_t* pFlags;
    /// Description
    D3D12_DESCRIPTOR_HEAP_TYPE mType;
    uint32_t                   mNumDescriptors;
    /// Descriptor Increment Size
    uint32_t mDescriptorSize;
    // Usage
    uint32_t mUsedDescriptors;
} DescriptorHeap;

//Forward declarations of platform-specific functions
void        d3d12_platform_create_renderer(const REI_AllocatorCallbacks& allocator, REI_Renderer** ppRenderer);
bool        d3d12_platform_add_device(const REI_RendererDescD3D12* pDesc, REI_Renderer* pRenderer);
void        d3d12_platform_remove_device(REI_Renderer* pRenderer);
HMODULE     d3d12_platform_get_d3d12_module_handle();
uint32_t    d3d12_platform_get_texture_row_alignment();
DXGI_FORMAT d3d12_platform_other_rei_formats_to_dxgi(uint32_t fmt);
DXGI_FORMAT d3d12_platform_other_dxgi_formats_to_dxgi_typeless(DXGI_FORMAT fmt);
void        d3d12_platform_create_copy_command_list(
           REI_Renderer* pRenderer, REI_CmdPool* pCmdPool, ID3D12CommandList** ppDxCmdList);
void d3d12_platform_create_copy_command_allocator(REI_Renderer* pRenderer, ID3D12CommandAllocator** ppDxCmdAlloc);
void d3d12_platform_create_copy_command_queue(
    REI_Renderer* pRenderer, REI_QueueDesc* pQDesc, ID3D12CommandQueue** ppDxCmdQueue);
void d3d12_platform_submit_resource_barriers(REI_Cmd* pCmd, uint32_t barriersCount, D3D12_RESOURCE_BARRIER* barriers);

void util_fill_gpu_desc(ID3D12Device* pDxDevice, D3D_FEATURE_LEVEL featureLevel, REI_GpuDesc* pInOutDesc)
{
    // Query the level of support of Shader Model.
    D3D12_FEATURE_DATA_D3D12_OPTIONS  featureData = {};
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 featureData1 = {};
    // Query the level of support of Wave Intrinsics.
    pDxDevice->CheckFeatureSupport(
        (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS, &featureData, sizeof(featureData));
    pDxDevice->CheckFeatureSupport(
        (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &featureData1, sizeof(featureData1));

    REI_GpuDesc&      gpuDesc = *pInOutDesc;
    DXGI_ADAPTER_DESC desc = {};
    gpuDesc.pGpu->GetDesc(&desc);

    gpuDesc.mMaxSupportedFeatureLevel = featureLevel;
    gpuDesc.mDedicatedVideoMemory = desc.DedicatedVideoMemory;
    gpuDesc.mFeatureDataOptions = featureData;
    gpuDesc.mFeatureDataOptions1 = featureData1;

    //save vendor and model Id as string
    //char hexChar[10];
    //convert deviceId and assign it
    sprintf_s(gpuDesc.mDeviceId, "%#x\0", desc.DeviceId);
    //convert modelId and assign it
    sprintf_s(gpuDesc.mVendorId, "%#x\0", desc.VendorId);
    //convert Revision Id
    sprintf_s(gpuDesc.mRevisionId, "%#x\0", desc.Revision);

    //save gpu name (Some situtations this can show description instead of name)
    //char sName[MAX_PATH];
    size_t numConverted = 0;
    wcstombs_s(&numConverted, gpuDesc.mName, desc.Description, REI_MAX_GPU_VENDOR_STRING_LENGTH);
}

D3D12_RESOURCE_STATES util_to_dx12_resource_state(REI_ResourceState state)
{
    D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;

    // These states cannot be combined with other states so we just do an == check
    if (state == REI_RESOURCE_STATE_GENERIC_READ)
        return D3D12_RESOURCE_STATE_GENERIC_READ;
    if (state == REI_RESOURCE_STATE_COMMON || state == REI_RESOURCE_STATE_UNDEFINED)
        return D3D12_RESOURCE_STATE_COMMON;
    if (state == REI_RESOURCE_STATE_PRESENT)
        return D3D12_RESOURCE_STATE_PRESENT;

    if (state & REI_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
        ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    if (state & REI_RESOURCE_STATE_INDEX_BUFFER)
        ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
    if (state & REI_RESOURCE_STATE_RENDER_TARGET)
        ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
    if (state & REI_RESOURCE_STATE_UNORDERED_ACCESS)
        ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    if (state & REI_RESOURCE_STATE_DEPTH_WRITE)
        ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
    if (state & REI_RESOURCE_STATE_DEPTH_READ)
        ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
    if (state & REI_RESOURCE_STATE_STREAM_OUT)
        ret |= D3D12_RESOURCE_STATE_STREAM_OUT;
    if (state & REI_RESOURCE_STATE_INDIRECT_ARGUMENT)
        ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
    if (state & REI_RESOURCE_STATE_COPY_DEST)
        ret |= D3D12_RESOURCE_STATE_COPY_DEST;
    if (state & REI_RESOURCE_STATE_COPY_SOURCE)
        ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
    if (state & REI_RESOURCE_STATE_RESOLVE_DEST)
        ret |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
    if (state & REI_RESOURCE_STATE_RESOLVE_SOURCE)
        ret |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
    if (state & REI_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
        ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    if (state & REI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        ret |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    return ret;
}

DXGI_FORMAT util_to_dx12_dsv_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
            // 32-bit Z w/ Stencil
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

            // No Stencil
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
            return DXGI_FORMAT_D32_FLOAT;

            // 24-bit Z
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return DXGI_FORMAT_D24_UNORM_S8_UINT;

            // 16-bit Z w/o Stencil
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM: return DXGI_FORMAT_D16_UNORM;

        default: return defaultFormat;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE util_to_dx12_primitive_topology_type(uint32_t topology)
{
    REI_ASSERT(topology < REI_PRIMITIVE_TOPO_COUNT, "Invalid topology");
    switch (topology)
    {
        case REI_PRIMITIVE_TOPO_POINT_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        case REI_PRIMITIVE_TOPO_LINE_LIST:
        case REI_PRIMITIVE_TOPO_LINE_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case REI_PRIMITIVE_TOPO_TRI_LIST:
        case REI_PRIMITIVE_TOPO_TRI_STRIP: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case REI_PRIMITIVE_TOPO_PATCH_LIST: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        default: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
}

D3D12_SHADER_VISIBILITY util_to_dx12_shader_visibility(REI_ShaderStage stages)
{
    D3D12_SHADER_VISIBILITY res = D3D12_SHADER_VISIBILITY_ALL;
    uint32_t                stageCount = 0;

    if (stages == REI_SHADER_STAGE_COMP)
    {
        return D3D12_SHADER_VISIBILITY_ALL;
    }
    if (stages & REI_SHADER_STAGE_VERT)
    {
        res = D3D12_SHADER_VISIBILITY_VERTEX;
        ++stageCount;
    }
    if (stages & REI_SHADER_STAGE_GEOM)
    {
        res = D3D12_SHADER_VISIBILITY_GEOMETRY;
        ++stageCount;
    }
    if (stages & REI_SHADER_STAGE_TESC)
    {
        res = D3D12_SHADER_VISIBILITY_HULL;
        ++stageCount;
    }
    if (stages & REI_SHADER_STAGE_TESE)
    {
        res = D3D12_SHADER_VISIBILITY_DOMAIN;
        ++stageCount;
    }
    if (stages & REI_SHADER_STAGE_FRAG)
    {
        res = D3D12_SHADER_VISIBILITY_PIXEL;
        ++stageCount;
    }

    REI_ASSERT(stageCount > 0);
    return stageCount > 1 ? D3D12_SHADER_VISIBILITY_ALL : res;
}

DXGI_FORMAT REI_Format_ToDXGI_FORMAT(uint32_t fmt)
{
    REI_ASSERT(fmt < REI_FMT_COUNT, "Invalid format");
    switch (fmt)
    {
        case REI_FMT_R1_UNORM: return DXGI_FORMAT_R1_UNORM;
        case REI_FMT_R5G6B5_UNORM: return DXGI_FORMAT_B5G6R5_UNORM;
        case REI_FMT_B5G6R5_UNORM: return DXGI_FORMAT_B5G6R5_UNORM;
        case REI_FMT_B5G5R5A1_UNORM: return DXGI_FORMAT_B5G5R5A1_UNORM;
        case REI_FMT_R8_UNORM: return DXGI_FORMAT_R8_UNORM;
        case REI_FMT_R8_SNORM: return DXGI_FORMAT_R8_SNORM;
        case REI_FMT_R8_UINT: return DXGI_FORMAT_R8_UINT;
        case REI_FMT_R8_SINT: return DXGI_FORMAT_R8_SINT;
        case REI_FMT_R8G8_UNORM: return DXGI_FORMAT_R8G8_UNORM;
        case REI_FMT_R8G8_SNORM: return DXGI_FORMAT_R8G8_SNORM;
        case REI_FMT_R8G8_UINT: return DXGI_FORMAT_R8G8_UINT;
        case REI_FMT_R8G8_SINT: return DXGI_FORMAT_R8G8_SINT;
        case REI_FMT_B4G4R4A4_UNORM: return DXGI_FORMAT_B4G4R4A4_UNORM;

        case REI_FMT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case REI_FMT_R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
        case REI_FMT_R8G8B8A8_UINT: return DXGI_FORMAT_R8G8B8A8_UINT;
        case REI_FMT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_SINT;
        case REI_FMT_R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case REI_FMT_B8G8R8A8_UNORM: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case REI_FMT_B8G8R8X8_UNORM: return DXGI_FORMAT_B8G8R8X8_UNORM;
        case REI_FMT_B8G8R8A8_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        case REI_FMT_R10G10B10A2_UNORM: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case REI_FMT_R10G10B10A2_UINT: return DXGI_FORMAT_R10G10B10A2_UINT;

        case REI_FMT_R16_UNORM: return DXGI_FORMAT_R16_UNORM;
        case REI_FMT_R16_SNORM: return DXGI_FORMAT_R16_SNORM;
        case REI_FMT_R16_UINT: return DXGI_FORMAT_R16_UINT;
        case REI_FMT_R16_SINT: return DXGI_FORMAT_R16_SINT;
        case REI_FMT_R16_SFLOAT: return DXGI_FORMAT_R16_FLOAT;
        case REI_FMT_R16G16_UNORM: return DXGI_FORMAT_R16G16_UNORM;
        case REI_FMT_R16G16_SNORM: return DXGI_FORMAT_R16G16_SNORM;
        case REI_FMT_R16G16_UINT: return DXGI_FORMAT_R16G16_UINT;
        case REI_FMT_R16G16_SINT: return DXGI_FORMAT_R16G16_SINT;
        case REI_FMT_R16G16_SFLOAT: return DXGI_FORMAT_R16G16_FLOAT;
        case REI_FMT_R16G16B16A16_UNORM: return DXGI_FORMAT_R16G16B16A16_UNORM;
        case REI_FMT_R16G16B16A16_SNORM: return DXGI_FORMAT_R16G16B16A16_SNORM;
        case REI_FMT_R16G16B16A16_UINT: return DXGI_FORMAT_R16G16B16A16_UINT;
        case REI_FMT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_SINT;
        case REI_FMT_R16G16B16A16_SFLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case REI_FMT_R32_UINT: return DXGI_FORMAT_R32_UINT;
        case REI_FMT_R32_SINT: return DXGI_FORMAT_R32_SINT;
        case REI_FMT_R32_SFLOAT: return DXGI_FORMAT_R32_FLOAT;
        case REI_FMT_R32G32_UINT: return DXGI_FORMAT_R32G32_UINT;
        case REI_FMT_R32G32_SINT: return DXGI_FORMAT_R32G32_SINT;
        case REI_FMT_R32G32_SFLOAT: return DXGI_FORMAT_R32G32_FLOAT;
        case REI_FMT_R32G32B32_UINT: return DXGI_FORMAT_R32G32B32_UINT;
        case REI_FMT_R32G32B32_SINT: return DXGI_FORMAT_R32G32B32_SINT;
        case REI_FMT_R32G32B32_SFLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
        case REI_FMT_R32G32B32A32_UINT: return DXGI_FORMAT_R32G32B32A32_UINT;
        case REI_FMT_R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_SINT;
        case REI_FMT_R32G32B32A32_SFLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case REI_FMT_B10G11R11_UFLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;
        case REI_FMT_E5B9G9R9_UFLOAT: return DXGI_FORMAT_R9G9B9E5_SHAREDEXP;
        case REI_FMT_D16_UNORM: return DXGI_FORMAT_D16_UNORM;
        case REI_FMT_X8_D24_UNORM: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case REI_FMT_D32_SFLOAT: return DXGI_FORMAT_D32_FLOAT;
        case REI_FMT_D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case REI_FMT_D32_SFLOAT_S8_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case REI_FMT_DXBC1_RGB_UNORM: return DXGI_FORMAT_BC1_UNORM;
        case REI_FMT_DXBC1_RGB_SRGB: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case REI_FMT_DXBC1_RGBA_UNORM: return DXGI_FORMAT_BC1_UNORM;
        case REI_FMT_DXBC1_RGBA_SRGB: return DXGI_FORMAT_BC1_UNORM_SRGB;
        case REI_FMT_DXBC2_UNORM: return DXGI_FORMAT_BC2_UNORM;
        case REI_FMT_DXBC2_SRGB: return DXGI_FORMAT_BC2_UNORM_SRGB;
        case REI_FMT_DXBC3_UNORM: return DXGI_FORMAT_BC3_UNORM;
        case REI_FMT_DXBC3_SRGB: return DXGI_FORMAT_BC3_UNORM_SRGB;
        case REI_FMT_DXBC4_UNORM: return DXGI_FORMAT_BC4_UNORM;
        case REI_FMT_DXBC4_SNORM: return DXGI_FORMAT_BC4_SNORM;
        case REI_FMT_DXBC5_UNORM: return DXGI_FORMAT_BC5_UNORM;
        case REI_FMT_DXBC5_SNORM: return DXGI_FORMAT_BC5_SNORM;
        case REI_FMT_DXBC6H_UFLOAT: return DXGI_FORMAT_BC6H_UF16;
        case REI_FMT_DXBC6H_SFLOAT: return DXGI_FORMAT_BC6H_SF16;
        case REI_FMT_DXBC7_UNORM: return DXGI_FORMAT_BC7_UNORM;
        case REI_FMT_DXBC7_SRGB: return DXGI_FORMAT_BC7_UNORM_SRGB;
        default: return d3d12_platform_other_rei_formats_to_dxgi(fmt);
    }
}

inline DXGI_FORMAT DXGI_Format_ToDXGI_FORMAT_Typeless(DXGI_FORMAT fmt)
{
    switch (fmt)
    {
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT: return DXGI_FORMAT_R32G32B32A32_TYPELESS;
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT: return DXGI_FORMAT_R32G32B32_TYPELESS;

        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT: return DXGI_FORMAT_R16G16B16A16_TYPELESS;

        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT: return DXGI_FORMAT_R32G32_TYPELESS;

        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT: return DXGI_FORMAT_R10G10B10A2_TYPELESS;

        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT: return DXGI_FORMAT_R16G16_TYPELESS;

        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT: return DXGI_FORMAT_R32_TYPELESS;
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT: return DXGI_FORMAT_R8G8_TYPELESS;
        case DXGI_FORMAT_B4G4R4A4_UNORM: return DXGI_FORMAT_B4G4R4A4_UNORM;    // Can't be R16Typless for sample view
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT: return DXGI_FORMAT_R16_TYPELESS;
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT: return DXGI_FORMAT_R8_TYPELESS;
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_TYPELESS;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB: return DXGI_FORMAT_BC2_TYPELESS;
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_TYPELESS;
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM: return DXGI_FORMAT_BC4_TYPELESS;
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM: return DXGI_FORMAT_BC5_TYPELESS;
        case DXGI_FORMAT_B5G6R5_UNORM: return DXGI_FORMAT_B5G6R5_UNORM;    // Can't be R16Typless for sample view
        case DXGI_FORMAT_B5G5R5A1_UNORM: return DXGI_FORMAT_R16_TYPELESS;

        case DXGI_FORMAT_R11G11B10_FLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;

        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_TYPELESS;

        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_TYPELESS;

        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16: return DXGI_FORMAT_BC6H_TYPELESS;

        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_TYPELESS;

        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return DXGI_FORMAT_R24G8_TYPELESS;

            // typeless just return the input format
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC7_TYPELESS: return fmt;

        case DXGI_FORMAT_R1_UNORM:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_YUY2:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
        case DXGI_FORMAT_NV11:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
        case DXGI_FORMAT_A8P8:
        case DXGI_FORMAT_P208:
        case DXGI_FORMAT_V208:
        case DXGI_FORMAT_V408:
        case DXGI_FORMAT_UNKNOWN: return DXGI_FORMAT_UNKNOWN;
        default: return d3d12_platform_other_dxgi_formats_to_dxgi_typeless(fmt);
    }
}

D3D12_TEXTURE_ADDRESS_MODE util_to_dx12_texture_address_mode(uint32_t addressMode)
{
    REI_ASSERT(addressMode < REI_ADDRESS_MODE_COUNT, "Invalid REI_AddressMode value"); 
    switch (addressMode)
    {
        case REI_ADDRESS_MODE_MIRROR: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case REI_ADDRESS_MODE_REPEAT: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case REI_ADDRESS_MODE_CLAMP_TO_EDGE: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case REI_ADDRESS_MODE_CLAMP_TO_BORDER: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}

DXGI_FORMAT util_to_dx12_srv_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
            // 32-bit Z w/ Stencil
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

            // No Stencil
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
            return DXGI_FORMAT_R32_FLOAT;

            // 24-bit Z
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

            // 16-bit Z w/o Stencil
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM: return DXGI_FORMAT_R16_UNORM;

        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;

        default: return defaultFormat;
    }
}

DXGI_FORMAT util_to_dx12_uav_format(DXGI_FORMAT defaultFormat)
{
    switch (defaultFormat)
    {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;

        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;

        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8X8_UNORM;

        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;

#if defined(_DEBUG)
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_D16_UNORM: REI_ASSERT(false, "Requested a UAV format for a depth stencil format");
#endif

        default: return defaultFormat;
    }
}

D3D12_FILTER
util_to_dx12_filter(
    uint32_t minFilter, uint32_t magFilter, uint32_t mipMapMode, bool aniso,
    bool comparisonFilterEnabled)
{
    if (aniso)
        return (comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_ANISOTROPIC : D3D12_FILTER_ANISOTROPIC);

    REI_ASSERT(minFilter < REI_FILTER_TYPE_COUNT, "Invalid minFilter");
    REI_ASSERT(magFilter < REI_FILTER_TYPE_COUNT, "Invalid magFilter");
    REI_ASSERT(mipMapMode < REI_MIPMAP_MODE_COUNT, "Invalid mipMapMode");

    // control bit : minFilter  magFilter   mipMapMode
    //   point   :   00	  00	   00
    //   linear  :   01	  01	   01
    // ex : trilinear == 010101
    int filter = (minFilter << 4) | (magFilter << 2) | mipMapMode;
    int baseFilter =
        comparisonFilterEnabled ? D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT : D3D12_FILTER_MIN_MAG_MIP_POINT;
    return (D3D12_FILTER)(baseFilter + filter);
}

static inline constexpr D3D12_GPU_DESCRIPTOR_HANDLE
    descriptor_id_to_gpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
    return { pHeap->mStartGpuHandle.ptr + id * pHeap->mDescriptorSize };
}

static inline bool REI_Format_IsDepthOnly(uint32_t const fmt)
{
    REI_ASSERT(fmt < REI_FMT_COUNT, "Invalid format");
    switch (fmt)
    {
        case REI_FMT_D16_UNORM: return true;
        case REI_FMT_X8_D24_UNORM: return true;
        case REI_FMT_D32_SFLOAT: return true;
        default: return false;
    }
}

static inline bool REI_Format_IsDepthAndStencil(uint32_t const fmt)
{
    REI_ASSERT(fmt < REI_FMT_COUNT, "Invalid format");
    switch (fmt)
    {
        case REI_FMT_D16_UNORM_S8_UINT: return true;
        case REI_FMT_D24_UNORM_S8_UINT: return true;
        case REI_FMT_D32_SFLOAT_S8_UINT: return true;
        default: return false;
    }
}

static inline bool REI_Format_HasDepth(uint32_t const fmt)
{
    return REI_Format_IsDepthOnly(fmt) || REI_Format_IsDepthAndStencil(fmt);
}

static REI_DescriptorType REI_sD3D12_TO_DESCRIPTOR[] = {
    REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER,    //D3D_SIT_CBUFFER
    REI_DESCRIPTOR_TYPE_BUFFER,            //D3D_SIT_TBUFFER
    REI_DESCRIPTOR_TYPE_TEXTURE,           //D3D_SIT_TEXTURE
    REI_DESCRIPTOR_TYPE_SAMPLER,           //D3D_SIT_SAMPLER
    REI_DESCRIPTOR_TYPE_RW_TEXTURE,        //D3D_SIT_UAV_RWTYPED
    REI_DESCRIPTOR_TYPE_BUFFER,            //D3D_SIT_STRUCTURED
    REI_DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_RWSTRUCTURED
    REI_DESCRIPTOR_TYPE_BUFFER,            //D3D_SIT_BYTEADDRESS
    REI_DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_RWBYTEADDRESS
    REI_DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_APPEND_STRUCTURED
    REI_DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_CONSUME_STRUCTURED
    REI_DESCRIPTOR_TYPE_RW_BUFFER,         //D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER
                                           // REI_DESCRIPTOR_TYPE_RAY_TRACING,       //D3D_SIT_RTACCELERATIONSTRUCTURE
};

typedef enum GpuVendor
{
    GPU_VENDOR_NVIDIA,
    GPU_VENDOR_AMD,
    GPU_VENDOR_INTEL,
    GPU_VENDOR_UNKNOWN,
    GPU_VENDOR_COUNT,
} GpuVendor;

#define VENDOR_ID_NVIDIA 0x10DE
#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_AMD_1 0x1022
#define VENDOR_ID_INTEL 0x163C
#define VENDOR_ID_INTEL_1 0x8086
#define VENDOR_ID_INTEL_2 0x8087

DescriptorHeapProperties gCpuDescriptorHeapProperties[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES + 1] = {
    { 1024 * 256, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },    // CBV SRV UAV
    { 2048, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },          // Sampler
    { 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },           // RTV
    { 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },           // DSV
    { 512, D3D12_DESCRIPTOR_HEAP_FLAG_NONE },           // SAMPLER_UNBOUNDED
};

D3D12_BLEND gDx12BlendConstantTranslator[REI_MAX_BLEND_CONSTANTS] = {
    D3D12_BLEND_ZERO,
    D3D12_BLEND_ONE,
    D3D12_BLEND_SRC_COLOR,
    D3D12_BLEND_INV_SRC_COLOR,
    D3D12_BLEND_DEST_COLOR,
    D3D12_BLEND_INV_DEST_COLOR,
    D3D12_BLEND_SRC_ALPHA,
    D3D12_BLEND_INV_SRC_ALPHA,
    D3D12_BLEND_DEST_ALPHA,
    D3D12_BLEND_INV_DEST_ALPHA,
    D3D12_BLEND_SRC_ALPHA_SAT,
    D3D12_BLEND_BLEND_FACTOR,
    D3D12_BLEND_INV_BLEND_FACTOR,
};

D3D12_BLEND_OP gDx12BlendOpTranslator[REI_MAX_BLEND_MODES] = {
    D3D12_BLEND_OP_ADD, D3D12_BLEND_OP_SUBTRACT, D3D12_BLEND_OP_REV_SUBTRACT, D3D12_BLEND_OP_MIN, D3D12_BLEND_OP_MAX,
};

D3D12_COMPARISON_FUNC gDx12ComparisonFuncTranslator[REI_MAX_COMPARE_MODES] = {
    D3D12_COMPARISON_FUNC_NEVER,         D3D12_COMPARISON_FUNC_LESS,    D3D12_COMPARISON_FUNC_EQUAL,
    D3D12_COMPARISON_FUNC_LESS_EQUAL,    D3D12_COMPARISON_FUNC_GREATER, D3D12_COMPARISON_FUNC_NOT_EQUAL,
    D3D12_COMPARISON_FUNC_GREATER_EQUAL, D3D12_COMPARISON_FUNC_ALWAYS,
};

D3D12_STENCIL_OP gDx12StencilOpTranslator[REI_MAX_STENCIL_OPS] = {
    D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_ZERO, D3D12_STENCIL_OP_REPLACE,  D3D12_STENCIL_OP_INVERT,
    D3D12_STENCIL_OP_INCR, D3D12_STENCIL_OP_DECR, D3D12_STENCIL_OP_INCR_SAT, D3D12_STENCIL_OP_DECR_SAT,
};

D3D12_CULL_MODE gDx12CullModeTranslator[REI_MAX_CULL_MODES] = {
    D3D12_CULL_MODE_NONE,
    D3D12_CULL_MODE_BACK,
    D3D12_CULL_MODE_FRONT,
};

D3D12_FILL_MODE gDx12FillModeTranslator[REI_MAX_FILL_MODES] = {
    D3D12_FILL_MODE_SOLID,
    D3D12_FILL_MODE_WIREFRAME,
};

const D3D12_COMMAND_LIST_TYPE gDx12CmdTypeTranslator[REI_MAX_QUEUE_FLAG] = { D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                                             D3D12_COMMAND_LIST_TYPE_COMPUTE,
                                                                             D3D12_COMMAND_LIST_TYPE_COPY };

const D3D12_COMMAND_QUEUE_PRIORITY gDx12QueuePriorityTranslator[REI_MAX_QUEUE_PRIORITY]{
    D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
    D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
    D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME,
};

D3D12_DESCRIPTOR_RANGE_TYPE util_to_dx12_descriptor_range(REI_DescriptorType type)
{
    switch (type)
    {
        case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case REI_DESCRIPTOR_TYPE_ROOT_CONSTANT: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case REI_DESCRIPTOR_TYPE_RW_BUFFER:
        case REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW:
        case REI_DESCRIPTOR_TYPE_RW_TEXTURE: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        case REI_DESCRIPTOR_TYPE_SAMPLER: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
        case REI_DESCRIPTOR_TYPE_TEXTURE:
        case REI_DESCRIPTOR_TYPE_BUFFER:
        case REI_DESCRIPTOR_TYPE_BUFFER_RAW: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

        default: REI_ASSERT(false && "Invalid DescriptorInfo Type"); return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    }
}

D3D12_QUERY_HEAP_TYPE util_to_dx12_query_heap_type(REI_QueryType type)
{
    REI_ASSERT(type < REI_QUERY_TYPE_COUNT, "Invalid query heap type");
    switch (type)
    {
        case REI_QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        case REI_QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
        case REI_QUERY_TYPE_OCCLUSION: return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
        default: REI_ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_HEAP_TYPE_OCCLUSION;
    }
}

D3D12_QUERY_TYPE util_to_dx12_query_type(REI_QueryType type)
{
    REI_ASSERT(type < REI_QUERY_TYPE_COUNT, "Invalid query heap type");
    switch (type)
    {
        case REI_QUERY_TYPE_TIMESTAMP: return D3D12_QUERY_TYPE_TIMESTAMP;
        case REI_QUERY_TYPE_PIPELINE_STATISTICS: return D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
        case REI_QUERY_TYPE_OCCLUSION: return D3D12_QUERY_TYPE_OCCLUSION;
        default: REI_ASSERT(false && "Invalid query heap type"); return D3D12_QUERY_TYPE_OCCLUSION;
    }
}

void util_to_GpuSettings(const REI_GpuDesc* pGpuDesc, REI_DeviceProperties* pOutDeviceProperties)
{
    pOutDeviceProperties->capabilities.uniformBufferAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    pOutDeviceProperties->capabilities.uploadBufferTextureAlignment = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT;
    pOutDeviceProperties->capabilities.uploadBufferTextureRowAlignment = d3d12_platform_get_texture_row_alignment();

    pOutDeviceProperties->capabilities.multiDrawIndirect = true;
    pOutDeviceProperties->capabilities.maxVertexInputBindings = 32U;

    //assign device ID
    strncpy_s(pOutDeviceProperties->modelId, pGpuDesc->mDeviceId, REI_MAX_GPU_VENDOR_STRING_LENGTH);
    //assign vendor ID
    strncpy_s(pOutDeviceProperties->vendorId, pGpuDesc->mVendorId, REI_MAX_GPU_VENDOR_STRING_LENGTH);
    //assign Revision ID
    strncpy_s(pOutDeviceProperties->revisionId, pGpuDesc->mRevisionId, REI_MAX_GPU_VENDOR_STRING_LENGTH);
    //get name from api
    strncpy_s(pOutDeviceProperties->deviceName, pGpuDesc->mName, REI_MAX_GPU_VENDOR_STRING_LENGTH);
    //get wave lane count
    pOutDeviceProperties->capabilities.waveLaneCount = pGpuDesc->mFeatureDataOptions1.WaveLaneCountMin;
    pOutDeviceProperties->capabilities.ROVsSupported = pGpuDesc->mFeatureDataOptions.ROVsSupported ? true : false;

    pOutDeviceProperties->capabilities.maxRootSignatureDWORDS = 64;
}

void return_descriptor_handles_unlocked(DescriptorHeap* pHeap, DxDescriptorID handle, uint32_t count)
{
    if (D3D12_DESCRIPTOR_ID_NONE == handle || !count)
    {
        return;
    }

    for (uint32_t id = handle; id < handle + count; ++id)
    {
        const uint32_t i = id / 32;
        const uint32_t mask = ~(1 << (id % 32));
        pHeap->pFlags[i] &= mask;
    }

    pHeap->mUsedDescriptors -= count;
}

void return_descriptor_handles(DescriptorHeap* pHeap, DxDescriptorID handle, uint32_t count)
{
    MutexLock lock(*pHeap->pMutex);
    return_descriptor_handles_unlocked(pHeap, handle, count);
}

static DxDescriptorID consume_descriptor_handles(DescriptorHeap* pHeap, uint32_t descriptorCount)
{
    if (!descriptorCount)
    {
        return D3D12_DESCRIPTOR_ID_NONE;
    }

    MutexLock lock(*pHeap->pMutex);

    DxDescriptorID result = D3D12_DESCRIPTOR_ID_NONE;
    DxDescriptorID firstResult = D3D12_DESCRIPTOR_ID_NONE;
    uint32_t       foundCount = 0;

    for (uint32_t i = 0; i < pHeap->mNumDescriptors / 32; ++i)
    {
        const uint32_t flag = pHeap->pFlags[i];
        if (UINT32_MAX == flag)
        {
            return_descriptor_handles_unlocked(pHeap, firstResult, foundCount);
            foundCount = 0;
            result = D3D12_DESCRIPTOR_ID_NONE;
            firstResult = D3D12_DESCRIPTOR_ID_NONE;
            continue;
        }

        for (int32_t j = 0, mask = 1; j < 32; ++j, mask <<= 1)
        {
            if (!(flag & mask))
            {
                pHeap->pFlags[i] |= mask;
                result = i * 32 + j;

                REI_ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");

                if (D3D12_DESCRIPTOR_ID_NONE == firstResult)
                {
                    firstResult = result;
                }

                ++foundCount;
                ++pHeap->mUsedDescriptors;

                if (foundCount == descriptorCount)
                {
                    return firstResult;
                }
            }
            // Non contiguous. Start scanning again from this point
            else if (foundCount)
            {
                return_descriptor_handles_unlocked(pHeap, firstResult, foundCount);
                foundCount = 0;
                result = D3D12_DESCRIPTOR_ID_NONE;
                firstResult = D3D12_DESCRIPTOR_ID_NONE;
            }
        }
    }

    REI_ASSERT(result != D3D12_DESCRIPTOR_ID_NONE && "Out of descriptors");
    return firstResult;
}

static inline constexpr D3D12_CPU_DESCRIPTOR_HANDLE
    descriptor_id_to_cpu_handle(DescriptorHeap* pHeap, DxDescriptorID id)
{
    return { pHeap->mStartCpuHandle.ptr + id * pHeap->mDescriptorSize };
}

static void copy_descriptor_handle(
    DescriptorHeap* pSrcHeap, DxDescriptorID srcId, DescriptorHeap* pDstHeap, DxDescriptorID dstId)
{
    REI_ASSERT(pSrcHeap->mType == pDstHeap->mType);
    D3D12_CPU_DESCRIPTOR_HANDLE srcHandle = descriptor_id_to_cpu_handle(pSrcHeap, srcId);
    D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = descriptor_id_to_cpu_handle(pDstHeap, dstId);
    pSrcHeap->pDevice->CopyDescriptorsSimple(1, dstHandle, srcHandle, pSrcHeap->mType);
}

static void add_sampler(REI_Renderer* pRenderer, const D3D12_SAMPLER_DESC* pSamplerDesc, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->pDxDevice->CreateSampler(pSamplerDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

void add_srv(
    REI_Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc,
    DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->pDxDevice->CreateShaderResourceView(pResource, pSrvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_uav(
    REI_Renderer* pRenderer, ID3D12Resource* pResource, ID3D12Resource* pCounterResource,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->pDxDevice->CreateUnorderedAccessView(
        pResource, pCounterResource, pUavDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_cbv(REI_Renderer* pRenderer, const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc, DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->pDxDevice->CreateConstantBufferView(pCbvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

static void add_rtv(
    REI_Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc,
    DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->pDxDevice->CreateRenderTargetView(pResource, pRtvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

void add_dsv(
    REI_Renderer* pRenderer, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc,
    DxDescriptorID* pInOutId)
{
    DescriptorHeap* heap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV];
    if (D3D12_DESCRIPTOR_ID_NONE == *pInOutId)
    {
        *pInOutId = consume_descriptor_handles(heap, 1);
    }
    pRenderer->pDxDevice->CreateDepthStencilView(pResource, pDsvDesc, descriptor_id_to_cpu_handle(heap, *pInOutId));
}

constexpr D3D12_BLEND_DESC util_to_blend_desc(const REI_BlendStateDesc* pDesc)
{
    int blendDescIndex = 0;
#if defined(_DEBUG)

    for (int i = 0; i < REI_MAX_RENDER_TARGET_ATTACHMENTS; ++i)
    {
        if (pDesc->renderTargetMask & (1 << i))
        {
            REI_ASSERT(pDesc->srcFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
            REI_ASSERT(pDesc->dstFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
            REI_ASSERT(pDesc->srcAlphaFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
            REI_ASSERT(pDesc->dstAlphaFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
            REI_ASSERT(pDesc->blendModes[blendDescIndex] < REI_MAX_BLEND_MODES);
            REI_ASSERT(pDesc->blendAlphaModes[blendDescIndex] < REI_MAX_BLEND_MODES);
        }

        if (pDesc->independentBlend)
            ++blendDescIndex;
    }

    blendDescIndex = 0;
#endif

    D3D12_BLEND_DESC ret = {};
    ret.AlphaToCoverageEnable = (BOOL)pDesc->alphaToCoverage;
    ret.IndependentBlendEnable = TRUE;
    for (int i = 0; i < REI_MAX_RENDER_TARGET_ATTACHMENTS; i++)
    {
        if (pDesc->renderTargetMask & (1 << i))
        {
            BOOL blendEnable =
                (gDx12BlendConstantTranslator[pDesc->srcFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
                 gDx12BlendConstantTranslator[pDesc->dstFactors[blendDescIndex]] != D3D12_BLEND_ZERO ||
                 gDx12BlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]] != D3D12_BLEND_ONE ||
                 gDx12BlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]] != D3D12_BLEND_ZERO);

            ret.RenderTarget[i].BlendEnable = blendEnable;
            ret.RenderTarget[i].RenderTargetWriteMask = (UINT8)pDesc->masks[blendDescIndex];
            ret.RenderTarget[i].BlendOp = gDx12BlendOpTranslator[pDesc->blendModes[blendDescIndex]];
            ret.RenderTarget[i].SrcBlend = gDx12BlendConstantTranslator[pDesc->srcFactors[blendDescIndex]];
            ret.RenderTarget[i].DestBlend = gDx12BlendConstantTranslator[pDesc->dstFactors[blendDescIndex]];
            ret.RenderTarget[i].BlendOpAlpha = gDx12BlendOpTranslator[pDesc->blendAlphaModes[blendDescIndex]];
            ret.RenderTarget[i].SrcBlendAlpha = gDx12BlendConstantTranslator[pDesc->srcAlphaFactors[blendDescIndex]];
            ret.RenderTarget[i].DestBlendAlpha = gDx12BlendConstantTranslator[pDesc->dstAlphaFactors[blendDescIndex]];
        }

        if (pDesc->independentBlend)
            ++blendDescIndex;
    }

    return ret;
}

constexpr D3D12_DEPTH_STENCIL_DESC util_to_depth_desc(const REI_DepthStateDesc* pDesc)
{
    REI_ASSERT(pDesc->depthCmpFunc < REI_MAX_COMPARE_MODES);
    REI_ASSERT(pDesc->stencilFrontFunc < REI_MAX_COMPARE_MODES);
    REI_ASSERT(pDesc->stencilFrontFail < REI_MAX_STENCIL_OPS);
    REI_ASSERT(pDesc->depthFrontFail < REI_MAX_STENCIL_OPS);
    REI_ASSERT(pDesc->stencilFrontPass < REI_MAX_STENCIL_OPS);
    REI_ASSERT(pDesc->stencilBackFunc < REI_MAX_COMPARE_MODES);
    REI_ASSERT(pDesc->stencilBackFail < REI_MAX_STENCIL_OPS);
    REI_ASSERT(pDesc->depthBackFail < REI_MAX_STENCIL_OPS);
    REI_ASSERT(pDesc->stencilBackPass < REI_MAX_STENCIL_OPS);

    D3D12_DEPTH_STENCIL_DESC ret = {};
    ret.DepthEnable = (BOOL)pDesc->depthTestEnable;
    ret.DepthWriteMask = pDesc->depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    ret.DepthFunc = gDx12ComparisonFuncTranslator[pDesc->depthCmpFunc];
    ret.StencilEnable = (BOOL)pDesc->stencilTestEnable;
    ret.StencilReadMask = pDesc->stencilReadMask;
    ret.StencilWriteMask = pDesc->stencilWriteMask;
    ret.BackFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->stencilBackFunc];
    ret.FrontFace.StencilFunc = gDx12ComparisonFuncTranslator[pDesc->stencilFrontFunc];
    ret.BackFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->depthBackFail];
    ret.FrontFace.StencilDepthFailOp = gDx12StencilOpTranslator[pDesc->depthFrontFail];
    ret.BackFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->stencilBackFail];
    ret.FrontFace.StencilFailOp = gDx12StencilOpTranslator[pDesc->stencilFrontFail];
    ret.BackFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->stencilBackPass];
    ret.FrontFace.StencilPassOp = gDx12StencilOpTranslator[pDesc->stencilFrontPass];

    return ret;
}

constexpr D3D12_RASTERIZER_DESC util_to_rasterizer_desc(const REI_RasterizerStateDesc* pDesc)
{
    REI_ASSERT(pDesc->fillMode < REI_MAX_FILL_MODES);
    REI_ASSERT(pDesc->cullMode < REI_MAX_CULL_MODES);
    REI_ASSERT(pDesc->frontFace == REI_FRONT_FACE_CCW || pDesc->frontFace == REI_FRONT_FACE_CW);

    D3D12_RASTERIZER_DESC ret = {};
    ret.FillMode = gDx12FillModeTranslator[pDesc->fillMode];
    ret.CullMode = gDx12CullModeTranslator[pDesc->cullMode];
    ret.FrontCounterClockwise = pDesc->frontFace == REI_FRONT_FACE_CCW;
    ret.DepthBias = (INT)pDesc->depthBiasConstantFactor;
    ret.DepthBiasClamp = 0.0f;
    ret.SlopeScaledDepthBias = pDesc->depthBiasSlopeFactor;
    ret.DepthClipEnable = !pDesc->scissor;
    ret.MultisampleEnable = pDesc->multiSample ? TRUE : FALSE;
    ret.AntialiasedLineEnable = FALSE;
    ret.ForcedSampleCount = 0;
    ret.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    return ret;
}

D3D12_DEPTH_STENCIL_DESC gDefaultDepthDesc = {};
D3D12_BLEND_DESC         gDefaultBlendDesc = {};
D3D12_RASTERIZER_DESC    gDefaultRasterizerDesc = {};
static void              add_default_resources(REI_Renderer* pRenderer)
{
    REI_BlendStateDesc blendStateDesc = {};
    blendStateDesc.dstAlphaFactors[0] = REI_BC_ZERO;
    blendStateDesc.dstFactors[0] = REI_BC_ZERO;
    blendStateDesc.srcAlphaFactors[0] = REI_BC_ONE;
    blendStateDesc.srcFactors[0] = REI_BC_ONE;
    blendStateDesc.masks[0] = REI_COLOR_MASK_ALL;
    blendStateDesc.renderTargetMask = REI_BLEND_STATE_TARGET_ALL;
    blendStateDesc.independentBlend = false;
    gDefaultBlendDesc = util_to_blend_desc(&blendStateDesc);

    REI_DepthStateDesc depthStateDesc = {};
    depthStateDesc.depthCmpFunc = REI_CMP_LEQUAL;
    depthStateDesc.depthTestEnable = false;
    depthStateDesc.depthWriteEnable = false;
    depthStateDesc.stencilBackFunc = REI_CMP_ALWAYS;
    depthStateDesc.stencilFrontFunc = REI_CMP_ALWAYS;
    depthStateDesc.stencilReadMask = 0xFF;
    depthStateDesc.stencilWriteMask = 0xFF;
    gDefaultDepthDesc = util_to_depth_desc(&depthStateDesc);

    REI_RasterizerStateDesc rasterizerStateDesc = {};
    rasterizerStateDesc.cullMode = REI_CULL_MODE_BACK;
    gDefaultRasterizerDesc = util_to_rasterizer_desc(&rasterizerStateDesc);
}

static void add_descriptor_heap(
    const REI_AllocatorCallbacks& allocator, REI_LogPtr pLog, ID3D12Device* pDevice, const D3D12_DESCRIPTOR_HEAP_DESC* pDesc,
    DescriptorHeap** ppDescHeap)
{
    uint32_t numDescriptors = pDesc->NumDescriptors;

    // Keep 32 aligned for easy remove
    numDescriptors = REI_align_up(numDescriptors, 32u);

    const size_t flagsSize = (numDescriptors / 32);

    REI_StackAllocator<false> persistentAlloc = { 0 };
    persistentAlloc.reserve<DescriptorHeap>().reserve<uint32_t>(flagsSize).reserve<Mutex>();

    if (!persistentAlloc.done(allocator))
    {
        pLog(
            REI_LOG_TYPE_ERROR, "add_descriptor_heap wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        *ppDescHeap = nullptr;
        return;
    }

    DescriptorHeap* pHeap = persistentAlloc.allocZeroed<DescriptorHeap>();
    pHeap->pFlags = persistentAlloc.allocZeroed<uint32_t>(flagsSize);
    pHeap->pDevice = pDevice;

    pHeap->pMutex = persistentAlloc.construct<Mutex>();

    D3D12_DESCRIPTOR_HEAP_DESC desc = *pDesc;
    desc.NumDescriptors = numDescriptors;

    CHECK_HRESULT(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap->pHeap)));

    pHeap->mStartCpuHandle = pHeap->pHeap->GetCPUDescriptorHandleForHeapStart();
    if (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        pHeap->mStartGpuHandle = pHeap->pHeap->GetGPUDescriptorHandleForHeapStart();
    }
    pHeap->mNumDescriptors = desc.NumDescriptors;
    pHeap->mType = desc.Type;
    pHeap->mDescriptorSize = pDevice->GetDescriptorHandleIncrementSize(pHeap->mType);

    *ppDescHeap = pHeap;
}

void InitCommon(const REI_RendererDescD3D12* pDesc, REI_Renderer* pRenderer)
{
#if defined(_DEBUG)
    //add debug layer if in debug mode
    ID3D12Debug*  pDXDebug = nullptr;
    ID3D12Debug1* pDXDebug1 = nullptr;

    if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(pDXDebug), (void**)&(pDXDebug))))
    {
        pDXDebug->EnableDebugLayer();

        if (SUCCEEDED(pDXDebug->QueryInterface(__uuidof(pDXDebug1), (void**)&(pDXDebug1))))
        {
            pDXDebug1->SetEnableGPUBasedValidation(pDesc->desc.enableGPUBasedValidation);
        }
    }

    SAFE_RELEASE(pDXDebug1);
    SAFE_RELEASE(pDXDebug);
#endif
}

void REI_initRenderer(const REI_RendererDesc* pDesc, REI_Renderer** ppRenderer)
{
    REI_RendererDescD3D12 descD3D12;
    descD3D12.desc = *pDesc;
    REI_initRendererD3D12(&descD3D12, ppRenderer);
}

void REI_initRendererD3D12(const REI_RendererDescD3D12* pDescD3D12, REI_Renderer** ppRenderer)
{
    REI_AllocatorCallbacks allocatorCallbacks;
    REI_setupAllocatorCallbacks(pDescD3D12->desc.pAllocator, allocatorCallbacks);

    d3d12_platform_create_renderer(allocatorCallbacks, ppRenderer);

    REI_Renderer* pRenderer = *ppRenderer;

    pRenderer->pLog = pDescD3D12->desc.pLog ? pDescD3D12->desc.pLog : REI_Log;

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    // Initialize the D3D12 bits
    {
        InitCommon(pDescD3D12, pRenderer);

        if (!d3d12_platform_add_device(pDescD3D12, pRenderer))
        {
            *ppRenderer = NULL;
            return;
        }

        //Load functions
        {
            HMODULE module = d3d12_platform_get_d3d12_module_handle();

            pRenderer->fnD3D12CreateRootSignatureDeserializer =
                (PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(
                    module, "D3D12SerializeVersionedRootSignature");

            pRenderer->fnD3D12SerializeVersionedRootSignature =
                (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)GetProcAddress(
                    module, "D3D12SerializeVersionedRootSignature");

            pRenderer->fnD3D12CreateVersionedRootSignatureDeserializer =
                (PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER)GetProcAddress(
                    module, "D3D12CreateVersionedRootSignatureDeserializer");
        }

        //        /************************************************************************/
        //        // Descriptor heaps
        //        /************************************************************************/
        pRenderer->pCPUDescriptorHeaps = (DescriptorHeap**)allocator.pMalloc(
            allocator.pUserData, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES * sizeof(DescriptorHeap*), 0);

        for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Flags = gCpuDescriptorHeapProperties[i].mFlags;
            desc.NumDescriptors = gCpuDescriptorHeapProperties[i].mMaxDescriptors;
            desc.Type = (D3D12_DESCRIPTOR_HEAP_TYPE)i;
            add_descriptor_heap(allocator, pRenderer->pLog, pRenderer->pDxDevice, &desc, &pRenderer->pCPUDescriptorHeaps[i]);
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

            desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            add_descriptor_heap(allocator, pRenderer->pLog, pRenderer->pDxDevice, &desc, &pRenderer->pCbvSrvUavHeaps);

            // Max sampler descriptor count
            desc.NumDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;
            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            add_descriptor_heap(allocator, pRenderer->pLog, pRenderer->pDxDevice, &desc, &pRenderer->pSamplerHeaps);
        }

        /************************************************************************/
        // Memory allocator
        /************************************************************************/
        D3D12MA::ALLOCATOR_DESC desc = {};
        desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
        desc.pDevice = pRenderer->pDxDevice;
        desc.pAdapter = pRenderer->pDxActiveGPU;

        D3D12MA::ALLOCATION_CALLBACKS allocationCallbacks = {};

        allocationCallbacks.pPrivateData = &pRenderer->allocator;

        allocationCallbacks.pAllocate = [](size_t size, size_t alignment, void* userData)
        {
            const REI_AllocatorCallbacks* allocator = (const REI_AllocatorCallbacks*)userData;
            return allocator->pMalloc(allocator->pUserData, size, 0);
        };

        allocationCallbacks.pFree = [](void* ptr, void* userData)
        {
            const REI_AllocatorCallbacks* allocator = (const REI_AllocatorCallbacks*)userData;
            return allocator->pFree(allocator->pUserData, ptr);
        };

        desc.pAllocationCallbacks = &allocationCallbacks;
        CHECK_HRESULT(D3D12MA::CreateAllocator(&desc, &pRenderer->pResourceAllocator));
    }

    D3D12_FEATURE_DATA_SHADER_CACHE shaderCacheFeature = {};
    CHECK_HRESULT(pRenderer->pDxDevice->CheckFeatureSupport(
        D3D12_FEATURE_SHADER_CACHE, &shaderCacheFeature, sizeof(shaderCacheFeature)));
    pRenderer->mShaderCacheFlags = shaderCacheFeature.SupportFlags;

    if (!(shaderCacheFeature.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY))
        pRenderer->pLog(
            REI_LOG_TYPE_INFO, "Pipeline Cache Library feature is not present. Pipeline Cache will be disabled");
    /************************************************************************/
    /************************************************************************/
    add_default_resources(pRenderer);

    D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSigFeatureData;
    rootSigFeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    CHECK_HRESULT(pRenderer->pDxDevice->CheckFeatureSupport(
        D3D12_FEATURE::D3D12_FEATURE_ROOT_SIGNATURE, &rootSigFeatureData, sizeof(rootSigFeatureData)));

    pRenderer->mHighestRootSignatureVersion = rootSigFeatureData.HighestVersion;

    // Renderer is good!
    *ppRenderer = pRenderer;
}

static void remove_descriptor_heap(const REI_AllocatorCallbacks& allocator, DescriptorHeap* pHeap)
{
    SAFE_RELEASE(pHeap->pHeap);
    pHeap->pMutex->~Mutex();
    allocator.pFree(allocator.pUserData, pHeap);
}

void REI_removeRenderer(REI_Renderer* pRenderer)
{
    REI_ASSERT(pRenderer);
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    // Destroy the Direct3D12 bits
    for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        remove_descriptor_heap(allocator, pRenderer->pCPUDescriptorHeaps[i]);
    }

    remove_descriptor_heap(allocator, pRenderer->pCbvSrvUavHeaps);
    remove_descriptor_heap(allocator, pRenderer->pSamplerHeaps);

    SAFE_RELEASE(pRenderer->pResourceAllocator);

    d3d12_platform_remove_device(pRenderer);

    // Free all the renderer components
    allocator.pFree(allocator.pUserData, pRenderer->pCPUDescriptorHeaps);
    allocator.pFree(allocator.pUserData, pRenderer->pActiveGpuSettings);
    allocator.pFree(allocator.pUserData, pRenderer->pName);
    allocator.pFree(allocator.pUserData, pRenderer);
}

void REI_getDeviceProperties(REI_Renderer* pRenderer, REI_DeviceProperties* outProperties)
{
    memset(&outProperties->capabilities, 0, sizeof(outProperties->capabilities));
    memcpy(outProperties, pRenderer->pActiveGpuSettings, sizeof(*pRenderer->pActiveGpuSettings));
}

void REI_addFence(REI_Renderer* pRenderer, REI_Fence** pp_fence)
{
    //ASSERT that renderer is valid
    REI_ASSERT(pRenderer);
    REI_ASSERT(pp_fence);

    //create a Fence and ASSERT that it is valid
    REI_Fence* pFence = (REI_Fence*)pRenderer->allocator.pMalloc(pRenderer->allocator.pUserData, sizeof(REI_Fence), 0);
    REI_ASSERT(pFence);

    CHECK_HRESULT(pRenderer->pDxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence->pDxFence)));
    pFence->mFenceValue = 1;

    pFence->pDxWaitIdleFenceEvent = CreateEventEx(NULL, FALSE, FALSE, EVENT_ALL_ACCESS);

    *pp_fence = pFence;
}

void REI_removeFence(REI_Renderer* pRenderer, REI_Fence* p_fence)
{
    //ASSERT that renderer is valid
    REI_ASSERT(pRenderer);
    //ASSERT that given fence to remove is valid
    REI_ASSERT(p_fence);

    SAFE_RELEASE(p_fence->pDxFence);
    CloseHandle(p_fence->pDxWaitIdleFenceEvent);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, p_fence);
}

void REI_getFenceStatus(REI_Renderer* pRenderer, REI_Fence* p_fence, REI_FenceStatus* p_fence_status)
{
    UINT64 completedValue = p_fence->pDxFence->GetCompletedValue();
    if (completedValue < p_fence->mFenceValue - 1)
        *p_fence_status = REI_FENCE_STATUS_INCOMPLETE;
    else
        *p_fence_status = REI_FENCE_STATUS_COMPLETE;
}

void REI_waitForFences(REI_Renderer* pRenderer, uint32_t fence_count, REI_Fence** pp_fences)
{
    // Wait for fence completion
    for (uint32_t i = 0; i < fence_count; ++i)
    {
        REI_FenceStatus fenceStatus;
        REI_getFenceStatus(pRenderer, pp_fences[i], &fenceStatus);
        if (fenceStatus == REI_FENCE_STATUS_INCOMPLETE)
        {
            uint64_t fenceValue = pp_fences[i]->mFenceValue - 1;
            pp_fences[i]->pDxFence->SetEventOnCompletion(fenceValue, pp_fences[i]->pDxWaitIdleFenceEvent);
            WaitForSingleObject(pp_fences[i]->pDxWaitIdleFenceEvent, INFINITE);
        }
    }
}

void REI_addSemaphore(REI_Renderer* pRenderer, REI_Semaphore** pp_semaphore)
{
    REI_addFence(pRenderer, (REI_Fence**)pp_semaphore);
}

void REI_removeSemaphore(REI_Renderer* pRenderer, REI_Semaphore* p_semaphore)
{
    //ASSERT that renderer and given semaphore are valid
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_semaphore);

    REI_removeFence(pRenderer, (REI_Fence*)p_semaphore);    //-V1027
}

void REI_addQueue(REI_Renderer* pRenderer, REI_QueueDesc* pQDesc, REI_Queue** ppQueue)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pQDesc);
    REI_ASSERT(ppQueue);

    REI_Queue* pQueue = (REI_Queue*)pRenderer->allocator.pMalloc(pRenderer->allocator.pUserData, sizeof(REI_Queue), 0);
    REI_ASSERT(pQueue);

    if (REI_CMD_POOL_COPY == pQDesc->type)
    {
        d3d12_platform_create_copy_command_queue(pRenderer, pQDesc, &pQueue->pDxQueue);
    }
    else
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        if (pQDesc->flag & REI_QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
            queueDesc.Flags |= D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
        queueDesc.Type = gDx12CmdTypeTranslator[pQDesc->type];
        queueDesc.Priority = gDx12QueuePriorityTranslator[pQDesc->priority];

        CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&pQueue->pDxQueue)));
    }

#ifdef _DEBUG
    wchar_t        queueTypeBuffer[32] = {};
    const wchar_t* queueType = NULL;
    switch (pQDesc->type)
    {
        case REI_CMD_POOL_DIRECT: queueType = L"GRAPHICS QUEUE"; break;
        case REI_CMD_POOL_COMPUTE: queueType = L"COMPUTE QUEUE"; break;
        case REI_CMD_POOL_COPY: queueType = L"COPY QUEUE"; break;
        default: queueType = L"UNKNOWN QUEUE"; break;
    }

    swprintf(queueTypeBuffer, L"%ls %u", queueType, 0);
    pQueue->pDxQueue->SetName(queueTypeBuffer);
#endif

    pQueue->mType = pQDesc->type;

    // Add queue fence. This fence will make sure we finish all GPU works before releasing the queue
    REI_addFence(pRenderer, &pQueue->pFence);
    pQueue->pRenderer = pRenderer;
    *ppQueue = pQueue;
}

void REI_removeQueue(REI_Queue* pQueue)
{
    REI_ASSERT(pQueue);
    // Make sure we finished all GPU works before we remove the queue
    REI_waitQueueIdle(pQueue);

    REI_removeFence(pQueue->pRenderer, pQueue->pFence);

    SAFE_RELEASE(pQueue->pDxQueue);

    pQueue->pRenderer->allocator.pFree(pQueue->pRenderer->allocator.pUserData, pQueue);
}

void REI_queueSubmit(
    REI_Queue* p_queue, uint32_t cmd_count, REI_Cmd** pp_cmds, REI_Fence* pFence, uint32_t wait_semaphore_count,
    REI_Semaphore** pp_wait_semaphores, uint32_t signal_semaphore_count, REI_Semaphore** pp_signal_semaphores)
{
    //ASSERT that given cmd list and given params are valid
    REI_ASSERT(p_queue);
    REI_ASSERT(cmd_count > 0);
    REI_ASSERT(pp_cmds);
    if (wait_semaphore_count > 0)
    {
        REI_ASSERT(pp_wait_semaphores);
    }
    if (signal_semaphore_count > 0)
    {
        REI_ASSERT(pp_signal_semaphores);
    }

    //execute given command list
    REI_ASSERT(p_queue->pDxQueue);

    ID3D12CommandList** cmds = (ID3D12CommandList**)alloca(cmd_count * sizeof(ID3D12CommandList*));
    for (uint32_t i = 0; i < cmd_count; ++i)
    {
        cmds[i] = pp_cmds[i]->pDxCmdList;
    }

    for (uint32_t i = 0; i < wait_semaphore_count; ++i)
        p_queue->pDxQueue->Wait(pp_wait_semaphores[i]->pDxFence, pp_wait_semaphores[i]->mFenceValue - 1);

    p_queue->pDxQueue->ExecuteCommandLists(cmd_count, cmds);

    if (pFence)
        p_queue->pDxQueue->Signal(pFence->pDxFence, pFence->mFenceValue++);

    for (uint32_t i = 0; i < signal_semaphore_count; ++i)
        p_queue->pDxQueue->Signal(pp_signal_semaphores[i]->pDxFence, pp_signal_semaphores[i]->mFenceValue++);
}

void REI_waitQueueIdle(REI_Queue* p_queue)
{
    auto& pQueue = p_queue->pDxQueue;
    auto& pFence = p_queue->pFence;
    auto& pDxFence = pFence->pDxFence;
    pQueue->Signal(pDxFence, pFence->mFenceValue++);

    uint64_t fenceValue = pFence->mFenceValue - 1;
    if (pDxFence->GetCompletedValue() < fenceValue)
    {
        pDxFence->SetEventOnCompletion(fenceValue, pFence->pDxWaitIdleFenceEvent);
        WaitForSingleObject(pFence->pDxWaitIdleFenceEvent, INFINITE);
    }
}

void REI_getQueueProperties(REI_Queue* pQueue, REI_QueueProperties* outProperties)
{
    REI_ASSERT(pQueue);
    REI_ASSERT(outProperties);
    UINT64 timestampFreq;
    CHECK_HRESULT(pQueue->pDxQueue->GetTimestampFrequency(&timestampFreq));

    outProperties->gpuTimestampFreq = (double)timestampFreq;
    outProperties->uploadGranularity = { 1, 1, 1 };
}
//void REI_getTimestampFrequency(REI_Queue* pQueue, double* pFrequency);

void REI_addSampler(REI_Renderer* pRenderer, const REI_SamplerDesc* pDesc, REI_Sampler** pp_sampler)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pRenderer->pDxDevice);
    REI_ASSERT(pp_sampler);
    REI_ASSERT(pDesc->compareFunc < REI_MAX_COMPARE_MODES);

    // initialize to zero
    REI_Sampler* pSampler = REI_new<REI_Sampler>(pRenderer->allocator);
    pSampler->mDescriptor = D3D12_DESCRIPTOR_ID_NONE;
    REI_ASSERT(pSampler);

    D3D12_SAMPLER_DESC desc = {};
    //add sampler to gpu
    desc.Filter = util_to_dx12_filter(
        pDesc->minFilter, pDesc->magFilter, pDesc->mipmapMode, pDesc->maxAnisotropy > 0.0f,
        (pDesc->compareFunc != REI_CMP_NEVER ? true : false));
    desc.AddressU = util_to_dx12_texture_address_mode(pDesc->addressU);
    desc.AddressV = util_to_dx12_texture_address_mode(pDesc->addressV);
    desc.AddressW = util_to_dx12_texture_address_mode(pDesc->addressW);
    desc.MipLODBias = pDesc->mipLodBias;
    desc.MaxAnisotropy = REI_max((UINT)pDesc->maxAnisotropy, 1U);
    desc.ComparisonFunc = gDx12ComparisonFuncTranslator[pDesc->compareFunc];
    desc.BorderColor[0] = 0.0f;
    desc.BorderColor[1] = 0.0f;
    desc.BorderColor[2] = 0.0f;
    desc.BorderColor[3] = 0.0f;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D12_FLOAT32_MAX;

    pSampler->mDesc = desc;
    add_sampler(pRenderer, &pSampler->mDesc, &pSampler->mDescriptor);

    *pp_sampler = pSampler;
}

void REI_removeSampler(REI_Renderer* pRenderer, REI_Sampler* p_sampler)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_sampler);

    // Nop op
    return_descriptor_handles(
        pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER], p_sampler->mDescriptor, 1);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, p_sampler);
}

void REI_addBuffer(REI_Renderer* pRenderer, const REI_BufferDesc* pDesc, REI_Buffer** pp_buffer)
{
    //verify renderer validity
    REI_ASSERT(pRenderer);
    //verify adding at least 1 buffer
    REI_ASSERT(pDesc);
    REI_ASSERT(pp_buffer);
    REI_ASSERT(pDesc->size > 0);

    // initialize to zero
    REI_Buffer* pBuffer = REI_new<REI_Buffer>(pRenderer->allocator);
    REI_ASSERT(pBuffer);
    pBuffer->mDescriptors = D3D12_DESCRIPTOR_ID_NONE;

    uint64_t allocationSize = pDesc->size;
    // Align the buffer size to multiples of 256
    if ((pDesc->descriptors & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER))
    {
        uint64_t minAlignment = pRenderer->pActiveGpuSettings->capabilities.uniformBufferAlignment;
        allocationSize = REI_align_up(allocationSize, minAlignment);
    }

    DECLARE_ZERO(D3D12_RESOURCE_DESC, desc);
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    //Alignment must be 64KB (D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT) or 0, which is effectively 64KB.
    //https://msdn.microsoft.com/en-us/library/windows/desktop/dn903813(v=vs.85).aspx
    desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
    desc.Width = allocationSize;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;    // concrete format is used for views
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER)
    {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // Adjust for padding
    UINT64 padded_size = 0;
    pRenderer->pDxDevice->GetCopyableFootprints(&desc, 0, 1, 0, NULL, NULL, NULL, &padded_size);
    allocationSize = padded_size;
    desc.Width = padded_size;

    REI_ResourceState start_state = pDesc->startState;
    if (pDesc->memoryUsage == REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU ||
        pDesc->memoryUsage == REI_RESOURCE_MEMORY_USAGE_CPU_ONLY)
    {
        start_state = REI_RESOURCE_STATE_GENERIC_READ;
    }
    else if (pDesc->memoryUsage == REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
    {
        start_state = REI_RESOURCE_STATE_COPY_DEST;
    }

    D3D12_RESOURCE_STATES res_states = util_to_dx12_resource_state(start_state);

    D3D12MA::ALLOCATION_DESC alloc_desc = {};

    if (REI_RESOURCE_MEMORY_USAGE_CPU_ONLY == pDesc->memoryUsage ||
        REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU == pDesc->memoryUsage)
        alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
    else if (REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU == pDesc->memoryUsage)
    {
        alloc_desc.HeapType = D3D12_HEAP_TYPE_READBACK;
        desc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
    }
    else
        alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    if (pDesc->flags & REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
        alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;

    // Create resource
    if (D3D12_HEAP_TYPE_DEFAULT != alloc_desc.HeapType && (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
    {
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
        CHECK_HRESULT(pRenderer->pDxDevice->CreateCommittedResource(
            &heapProps, alloc_desc.ExtraHeapFlags, &desc, res_states, NULL, IID_PPV_ARGS(&pBuffer->pDxResource)));
    }
    else
    {
        pRenderer->pResourceAllocator->CreateResource(
            &alloc_desc, &desc, res_states, NULL, &pBuffer->pDxAllocation, IID_PPV_ARGS(&pBuffer->pDxResource));

        // Set name
#if defined(_DEBUG)
        pBuffer->pDxAllocation->SetName(pDesc->pDebugName);
#endif
    }

    if (pDesc->memoryUsage != REI_RESOURCE_MEMORY_USAGE_GPU_ONLY &&
        pDesc->flags & REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
        pBuffer->pDxResource->Map(0, NULL, &pBuffer->pCpuMappedAddress);

    pBuffer->mDxGpuAddress = pBuffer->pDxResource->GetGPUVirtualAddress();

    if (!(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) &&
        !(pDesc->flags & REI_BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION))
    {
        DescriptorHeap* pHeap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];
        uint32_t        handleCount = ((pDesc->descriptors & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
                               ((pDesc->descriptors & REI_DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
                               ((pDesc->descriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
        pBuffer->mDescriptors = consume_descriptor_handles(pHeap, handleCount);

        if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            pBuffer->mSrvDescriptorOffset = 1;

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = pBuffer->mDxGpuAddress;
            cbvDesc.SizeInBytes = (UINT)allocationSize;
            add_cbv(pRenderer, &cbvDesc, &pBuffer->mDescriptors);
        }

        if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_BUFFER)
        {
            REI_ASSERT(pDesc->elementCount > 0);
            REI_ASSERT(
                (pDesc->format != REI_FMT_UNDEFINED) !=
                (pDesc->structStride != 0));    // either typed or structured buffer

            DxDescriptorID srv = pBuffer->mDescriptors + pBuffer->mSrvDescriptorOffset;
            pBuffer->mUavDescriptorOffset = pBuffer->mSrvDescriptorOffset + 1;

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Buffer.FirstElement = pDesc->firstElement;
            srvDesc.Buffer.NumElements = (UINT)pDesc->elementCount;
            srvDesc.Buffer.StructureByteStride = (UINT)(pDesc->structStride);
            srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            srvDesc.Format = REI_Format_ToDXGI_FORMAT(pDesc->format);

            if (REI_DESCRIPTOR_TYPE_BUFFER_RAW == (pDesc->descriptors & REI_DESCRIPTOR_TYPE_BUFFER_RAW))
            {
                REI_ASSERT(DXGI_Format_ToDXGI_FORMAT_Typeless(srvDesc.Format) == DXGI_FORMAT_R32_TYPELESS);
                srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                srvDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
                srvDesc.Buffer.NumElements = (UINT)(allocationSize / 4);
            }

            add_srv(pRenderer, pBuffer->pDxResource, &srvDesc, &srv);
        }

        if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER)
        {
            REI_ASSERT(pDesc->elementCount > 0);
            REI_ASSERT(
                (pDesc->format != REI_FMT_UNDEFINED) !=
                (pDesc->structStride != 0));    // either typed or structured buffer

            DxDescriptorID uav = pBuffer->mDescriptors + pBuffer->mUavDescriptorOffset;

            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.FirstElement = pDesc->firstElement;
            uavDesc.Buffer.NumElements = (UINT)pDesc->elementCount;
            uavDesc.Buffer.StructureByteStride = (UINT)(pDesc->structStride);
            uavDesc.Buffer.CounterOffsetInBytes = 0;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
            uavDesc.Format = REI_Format_ToDXGI_FORMAT(pDesc->format);

            if (REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW == (pDesc->descriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW))
            {
                REI_ASSERT(DXGI_Format_ToDXGI_FORMAT_Typeless(uavDesc.Format) == DXGI_FORMAT_R32_TYPELESS);
                uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
                uavDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
                uavDesc.Buffer.NumElements = (UINT)(allocationSize / 4);
            }
            else if (pDesc->format != REI_FMT_UNDEFINED)
            {
                D3D12_FEATURE_DATA_FORMAT_SUPPORT FormatSupport = { uavDesc.Format, D3D12_FORMAT_SUPPORT1_NONE,
                                                                    D3D12_FORMAT_SUPPORT2_NONE };

                HRESULT hr = pRenderer->pDxDevice->CheckFeatureSupport(
                    D3D12_FEATURE_FORMAT_SUPPORT, &FormatSupport, sizeof(FormatSupport));

                if (!SUCCEEDED(hr) || !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) ||
                    !(FormatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
                {
                    // Format does not support UAV Typed Load
                    pRenderer->pLog(
                        REI_LOG_TYPE_WARNING, "Cannot use Typed UAV for buffer format %u", (uint32_t)pDesc->format);
                    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
                }
            }

            ID3D12Resource* pCounterResource = pDesc->pCounterBuffer ? pDesc->pCounterBuffer->pDxResource : NULL;
            add_uav(pRenderer, pBuffer->pDxResource, pCounterResource, &uavDesc, &uav);
        }
    }

#if defined(_DEBUG)
    if (pDesc->pDebugName)
    {
        // Set name
        pBuffer->pDxResource->SetName(pDesc->pDebugName);
    }
#endif

    pBuffer->mSize = (uint32_t)pDesc->size;
    pBuffer->mMemoryUsage = pDesc->memoryUsage;
    pBuffer->desc = *pDesc;
    *pp_buffer = pBuffer;
}

void REI_removeBuffer(REI_Renderer* pRenderer, REI_Buffer* p_buffer)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_buffer);

    if (p_buffer->mDescriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        uint32_t handleCount = ((p_buffer->mDescriptors & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ? 1 : 0) +
                               ((p_buffer->mDescriptors & REI_DESCRIPTOR_TYPE_BUFFER) ? 1 : 0) +
                               ((p_buffer->mDescriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER) ? 1 : 0);
        return_descriptor_handles(
            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], p_buffer->mDescriptors,
            handleCount);
    }

    SAFE_RELEASE(p_buffer->pDxAllocation);
    SAFE_RELEASE(p_buffer->pDxResource);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, p_buffer);
}

void REI_mapBuffer(REI_Renderer* pRenderer, REI_Buffer* pBuffer, void** pMappedMem)
{
    REI_ASSERT(
        pBuffer->mMemoryUsage != REI_RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");
    CHECK_HRESULT(pBuffer->pDxResource->Map(0, nullptr, pMappedMem));
}

void REI_unmapBuffer(REI_Renderer* pRenderer, REI_Buffer* pBuffer)
{
    REI_ASSERT(
        pBuffer->mMemoryUsage != REI_RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to unmap non-cpu accessible resource");

    pBuffer->pDxResource->Unmap(0, NULL);
    pBuffer->pCpuMappedAddress = NULL;
}

void REI_setBufferName(REI_Renderer* pRenderer, REI_Buffer* pBuffer, const char* pName)
{
#if defined(_DEBUG)
    REI_ASSERT(pRenderer);
    REI_ASSERT(pBuffer);
    REI_ASSERT(pName);

    wchar_t wName[128] = {};
    size_t  numConverted = 0;
    mbstowcs_s(&numConverted, wName, pName, 128);
    pBuffer->pDxResource->SetName(wName);
#endif
}

static uint32_t REI_mapSwizzle(REI_COMPONENT_MAPPING mapping[4])
{
    uint32_t rgbaMapping = 0;

    for (uint32_t i = 0; i < 4; ++i)
    {
        switch (mapping[i])
        {
            case REI_COMPONENT_MAPPING_DEFAULT: rgbaMapping |= i << (i * 3); break;
            case REI_COMPONENT_MAPPING_R: rgbaMapping |= 0 << (i * 3); break;
            case REI_COMPONENT_MAPPING_G: rgbaMapping |= 1 << (i * 3); break;
            case REI_COMPONENT_MAPPING_B: rgbaMapping |= 2 << (i * 3); break;
            case REI_COMPONENT_MAPPING_A: rgbaMapping |= 3 << (i * 3); break;
            case REI_COMPONENT_MAPPING_0: rgbaMapping |= 4 << (i * 3); break;
            case REI_COMPONENT_MAPPING_1: rgbaMapping |= 5 << (i * 3); break;
            default: REI_ASSERT(false);
        }
    }

    return rgbaMapping | 1 << 12;
}

void REI_addTexture(REI_Renderer* pRenderer, const REI_TextureDesc* in_pDesc, REI_Texture** pp_texture)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(in_pDesc && in_pDesc->width && in_pDesc->height);
    REI_TextureDesc* pDesc = const_cast<REI_TextureDesc*>(in_pDesc);

    pDesc->arraySize = pDesc->arraySize ? pDesc->arraySize : 1;
    pDesc->depth = pDesc->depth ? pDesc->depth : 1;
    pDesc->mipLevels = pDesc->mipLevels ? pDesc->mipLevels : 1;
    pDesc->sampleCount = pDesc->sampleCount ? pDesc->sampleCount : REI_SAMPLE_COUNT_1;

    if (pDesc->sampleCount > REI_SAMPLE_COUNT_1 && pDesc->mipLevels > 1)
    {
        REI_ASSERT(false, "Multi-Sampled textures cannot have mip maps");
        return;
    }

    bool isRT = uint32_t(pDesc->descriptors) &
                (REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES);
    const bool isDepth = REI_Format_HasDepth(pDesc->format);
    if (isDepth)
    {
        REI_ASSERT(
            !((isDepth) && (pDesc->descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");
        isRT = false;
    }

    //allocate new texture
    REI_Texture* pTexture = REI_new<REI_Texture>(pRenderer->allocator);
    REI_ASSERT(pTexture);

    pTexture->mDescriptors = D3D12_DESCRIPTOR_ID_NONE;
    pTexture->mRTVDescriptors = D3D12_DESCRIPTOR_ID_NONE;
    pTexture->mDSVDescriptors = D3D12_DESCRIPTOR_ID_NONE;

    if (pDesc->pNativeHandle)
    {
        pTexture->mOwnsImage = false;
        pTexture->pDxResource = (ID3D12Resource*)pDesc->pNativeHandle;
    }
    else
    {
        pTexture->mOwnsImage = true;
    }

    //add to gpu
    D3D12_RESOURCE_DESC desc = {};

    DXGI_FORMAT dxFormat = REI_Format_ToDXGI_FORMAT(pDesc->format);

    REI_DescriptorType descriptors = (REI_DescriptorType)pDesc->descriptors;

    REI_ASSERT(DXGI_FORMAT_UNKNOWN != dxFormat);

    if (NULL == pTexture->pDxResource)
    {
        D3D12_RESOURCE_DIMENSION res_dim = D3D12_RESOURCE_DIMENSION_UNKNOWN;
        if (pDesc->flags & REI_TEXTURE_CREATION_FLAG_FORCE_2D)
        {
            REI_ASSERT(pDesc->depth == 1);
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        }
        else if (pDesc->flags & REI_TEXTURE_CREATION_FLAG_FORCE_3D)
        {
            res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        }
        else
        {
            if (pDesc->depth > 1)
                res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
            else if (pDesc->height > 1)
                res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            else
                res_dim = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
        }

        desc.Dimension = res_dim;
        //On PC, If Alignment is set to 0, the runtime will use 4MB for MSAA textures and 64KB for everything else.
        //On XBox, We have to explicitlly assign D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT if MSAA is used
        desc.Alignment = (UINT)pDesc->sampleCount > 1 ? D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT : 0;
        desc.Width = pDesc->width;
        desc.Height = pDesc->height;
        desc.DepthOrArraySize = (UINT16)(pDesc->arraySize != 1 ? pDesc->arraySize : pDesc->depth);
        desc.MipLevels = (UINT16)pDesc->mipLevels;
        desc.Format = DXGI_Format_ToDXGI_FORMAT_Typeless(dxFormat);
        desc.SampleDesc.Count = (UINT)pDesc->sampleCount;
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS data;
        data.Format = desc.Format;
        data.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        data.SampleCount = desc.SampleDesc.Count;
        pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
        while (data.NumQualityLevels == 0 && data.SampleCount > 0)
        {
            pRenderer->pLog(
                REI_LOG_TYPE_WARNING, "Sample Count (%u) not supported. Trying a lower sample count (%u)",
                data.SampleCount, data.SampleCount / 2);
            data.SampleCount = desc.SampleDesc.Count / 2;
            pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &data, sizeof(data));
        }
        desc.SampleDesc.Count = data.SampleCount;

        // Decide UAV flags
        if (descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)
        {
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        }

        // Decide render target flags
        if (isRT)
        {
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }
        else if (isDepth)
        {
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        }

        // Decide sharing flags
        if (pDesc->flags & REI_TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT)
        {
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
            desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        }
        else if (pDesc->flags & REI_TEXTURE_CREATION_FLAG_EXPORT_BIT)
        {
            if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL || desc.SampleDesc.Count != 1)
            {
                REI_ASSERT(
                    false,
                    "Flag D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS can't be used "
                    "with MSAA textures or with D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL");
            };
            desc.Flags |= D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;
        }

        DECLARE_ZERO(D3D12_CLEAR_VALUE, clearValue);
        clearValue.Format = dxFormat;
        if (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
        {
            clearValue.DepthStencil.Depth = pDesc->clearValue.ds.depth;
            clearValue.DepthStencil.Stencil = (UINT8)pDesc->clearValue.ds.stencil;
        }
        else
        {
            clearValue.Color[0] = pDesc->clearValue.rt.r;
            clearValue.Color[1] = pDesc->clearValue.rt.g;
            clearValue.Color[2] = pDesc->clearValue.rt.b;
            clearValue.Color[3] = pDesc->clearValue.rt.a;
        }

        D3D12_CLEAR_VALUE*    pClearValue = NULL;

        if ((desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ||
            (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
        {
            pClearValue = &clearValue;
        }

        D3D12MA::ALLOCATION_DESC alloc_desc = {};
        alloc_desc.HeapType = D3D12_HEAP_TYPE_DEFAULT;
        if (pDesc->flags & REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
            alloc_desc.Flags |= D3D12MA::ALLOCATION_FLAG_COMMITTED;

        if (pDesc->flags & REI_TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET)
        {
            alloc_desc.ExtraHeapFlags |= D3D12_HEAP_FLAG_ALLOW_DISPLAY;
            desc.Format = dxFormat;
        }

        CHECK_HRESULT(pRenderer->pResourceAllocator->CreateResource(
            &alloc_desc, &desc, D3D12_RESOURCE_STATE_COMMON, pClearValue, &pTexture->pDxAllocation,
            IID_PPV_ARGS(&pTexture->pDxResource)));
#if defined(_DEBUG)
        // Set name
        pTexture->pDxAllocation->SetName(pDesc->pDebugName);
#endif
    }
    else
    {
        desc = pTexture->pDxResource->GetDesc();
        dxFormat = desc.Format;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC  srvDesc = {};
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    D3D12_DEPTH_STENCIL_VIEW_DESC    dsvDesc = {};
    D3D12_RENDER_TARGET_VIEW_DESC    rtvDesc = {};

    switch (desc.Dimension)
    {
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
        {
            if (desc.DepthOrArraySize > 1)
            {
                // SRV
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                srvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
                srvDesc.Texture1DArray.FirstArraySlice = 0;
                srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
                srvDesc.Texture1DArray.MostDetailedMip = 0;
                // UAV
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
                uavDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
                uavDesc.Texture1DArray.FirstArraySlice = 0;
                uavDesc.Texture1DArray.MipSlice = 0;
                // DSV
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
                dsvDesc.Texture1DArray.MipSlice = 0;    //mipSlice;
                // RTV
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
                rtvDesc.Texture1DArray.ArraySize = desc.DepthOrArraySize;
                rtvDesc.Texture1DArray.FirstArraySlice = 0;
                rtvDesc.Texture1DArray.MipSlice = 0;
            }
            else
            {
                // SRV
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                srvDesc.Texture1D.MipLevels = desc.MipLevels;
                srvDesc.Texture1D.MostDetailedMip = 0;
                // UAV
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
                uavDesc.Texture1D.MipSlice = 0;
                // DSV
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
                dsvDesc.Texture1D.MipSlice = 0;    //desc.mipSlice;
                // RTV
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
                rtvDesc.Texture1D.MipSlice = 0;
            }
            break;
        }
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        {
            if (REI_DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & REI_DESCRIPTOR_TYPE_TEXTURE_CUBE))
            {
                REI_ASSERT(desc.DepthOrArraySize % 6 == 0);

                if (desc.DepthOrArraySize > 6)
                {
                    // SRV
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                    srvDesc.TextureCubeArray.First2DArrayFace = 0;
                    srvDesc.TextureCubeArray.MipLevels = desc.MipLevels;
                    srvDesc.TextureCubeArray.MostDetailedMip = 0;
                    srvDesc.TextureCubeArray.NumCubes = desc.DepthOrArraySize / 6;
                }
                else
                {
                    // SRV
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                    srvDesc.TextureCube.MipLevels = desc.MipLevels;
                    srvDesc.TextureCube.MostDetailedMip = 0;
                }

                // UAV
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                uavDesc.Texture2DArray.FirstArraySlice = 0;
                uavDesc.Texture2DArray.MipSlice = 0;
                uavDesc.Texture2DArray.PlaneSlice = 0;
                //DSV
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                dsvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                dsvDesc.Texture2DArray.FirstArraySlice = 0;
                dsvDesc.Texture2DArray.MipSlice = 0;
                //RTV
                rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                rtvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                rtvDesc.Texture2DArray.FirstArraySlice = 0;
                rtvDesc.Texture2DArray.MipSlice = 0;
                rtvDesc.Texture2DArray.PlaneSlice = 0;
            }
            else
            {
                if (desc.DepthOrArraySize > 1)
                {
                    if (desc.SampleDesc.Count > REI_SAMPLE_COUNT_1)
                    {
                        // Cannot create a multisampled uav
                        // SRV
                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                        srvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                        srvDesc.Texture2DMSArray.FirstArraySlice = 0;
                        // No UAV
                        // DSV
                        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                        dsvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                        dsvDesc.Texture2DMSArray.FirstArraySlice = 0;
                        // RTV
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                        rtvDesc.Texture2DMSArray.ArraySize = desc.DepthOrArraySize;
                        rtvDesc.Texture2DMSArray.FirstArraySlice = 0;
                    }
                    else
                    {
                        // SRV
                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                        srvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                        srvDesc.Texture2DArray.FirstArraySlice = 0;
                        srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
                        srvDesc.Texture2DArray.MostDetailedMip = 0;
                        srvDesc.Texture2DArray.PlaneSlice = 0;
                        // UAV
                        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                        uavDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                        uavDesc.Texture2DArray.FirstArraySlice = 0;
                        uavDesc.Texture2DArray.MipSlice = 0;
                        uavDesc.Texture2DArray.PlaneSlice = 0;
                        // DSV
                        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                        dsvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                        dsvDesc.Texture2DArray.FirstArraySlice = 0;
                        dsvDesc.Texture2DArray.MipSlice = 0;
                        // RTV
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                        rtvDesc.Texture2DArray.ArraySize = desc.DepthOrArraySize;
                        rtvDesc.Texture2DArray.FirstArraySlice = 0;
                        rtvDesc.Texture2DArray.MipSlice = 0;
                        rtvDesc.Texture2DArray.PlaneSlice = 0;
                    }
                }
                else
                {
                    if (desc.SampleDesc.Count > REI_SAMPLE_COUNT_1)
                    {
                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
                        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
                        // Cannot create a multisampled uav
                    }
                    else
                    {
                        // SRV
                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        srvDesc.Texture2D.MipLevels = desc.MipLevels;
                        srvDesc.Texture2D.MostDetailedMip = 0;
                        srvDesc.Texture2D.PlaneSlice = 0;
                        // UAV
                        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                        uavDesc.Texture2D.MipSlice = 0;
                        uavDesc.Texture2D.PlaneSlice = 0;
                        // DSV
                        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                        dsvDesc.Texture2D.MipSlice = 0;
                        // RTV
                        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                        rtvDesc.Texture2D.MipSlice = 0;
                        rtvDesc.Texture2D.PlaneSlice = 0;
                    }
                }
            }
            break;
        }
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
        {
            //Cannot create 3D DSV
            // SRV
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels = desc.MipLevels;
            srvDesc.Texture3D.MostDetailedMip = 0;
            // UAV
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            uavDesc.Texture3D.MipSlice = 0;
            uavDesc.Texture3D.FirstWSlice = 0;
            uavDesc.Texture3D.WSize = desc.DepthOrArraySize;
            // NO DSV
            //RTV
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
            rtvDesc.Texture3D.WSize = desc.DepthOrArraySize;
            rtvDesc.Texture3D.MipSlice = 0;
            rtvDesc.Texture3D.FirstWSlice = 0;
            break;
        }
        default: break;
    }

    DescriptorHeap* pHeap = pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV];

    uint32_t handleCount = (descriptors & REI_DESCRIPTOR_TYPE_TEXTURE) ? 1 : 0;
    handleCount += (descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE) ? pDesc->mipLevels : 0;
    pTexture->mDescriptors = consume_descriptor_handles(pHeap, handleCount);
    pTexture->m_SRV_UAV_HandleCount = handleCount;

    if (descriptors & REI_DESCRIPTOR_TYPE_TEXTURE)
    {
        REI_ASSERT(srvDesc.ViewDimension != D3D12_SRV_DIMENSION_UNKNOWN);

        srvDesc.Shader4ComponentMapping = REI_mapSwizzle(pDesc->componentMapping);

        srvDesc.Format = util_to_dx12_srv_format(dxFormat);
        add_srv(pRenderer, pTexture->pDxResource, &srvDesc, &pTexture->mDescriptors);
        ++pTexture->mUavStartIndex;
    }

    if (descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        uavDesc.Format = util_to_dx12_uav_format(dxFormat);
        for (uint32_t i = 0; i < pDesc->mipLevels; ++i)
        {
            DxDescriptorID handle = pTexture->mDescriptors + i + pTexture->mUavStartIndex;

            uavDesc.Texture1DArray.MipSlice = i;
            if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                uavDesc.Texture3D.WSize = desc.DepthOrArraySize / (UINT)pow(2.0, int(i));
            add_uav(pRenderer, pTexture->pDxResource, NULL, &uavDesc, &handle);
        }
    }

    if (descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET)
    {
        handleCount = desc.MipLevels * desc.DepthOrArraySize;
        pTexture->m_RTV_DSV_HandleCount = handleCount;

        pHeap = isDepth ? pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]
                        : pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV];

        if (isDepth)
        {
            pTexture->mDSVDescriptors = consume_descriptor_handles(pHeap, handleCount);
            dsvDesc.Format = util_to_dx12_dsv_format(dxFormat);
            for (uint32_t i = 0; i < desc.MipLevels; ++i)
            {
                if ((descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
                    (descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
                {
                    for (uint32_t j = 0; j < desc.DepthOrArraySize; ++j)
                    {
                        DxDescriptorID handle = pTexture->mDSVDescriptors + (i * desc.DepthOrArraySize + j);

                        add_dsv(pRenderer, pTexture->pDxResource, &dsvDesc, &handle);
                    }
                }
                else
                {
                    DxDescriptorID handle = pTexture->mDSVDescriptors + i;

                    add_dsv(pRenderer, pTexture->pDxResource, &dsvDesc, &handle);
                }
            }
        }
        if (isRT)
        {
            pTexture->mRTVDescriptors = consume_descriptor_handles(pHeap, handleCount);
            rtvDesc.Format = dxFormat;
            for (uint32_t i = 0; i < desc.MipLevels; ++i)
            {
                if ((descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
                    (descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
                {
                    for (uint32_t j = 0; j < desc.DepthOrArraySize; ++j)
                    {
                        DxDescriptorID handle = pTexture->mRTVDescriptors + (i * desc.DepthOrArraySize + j);

                        add_rtv(pRenderer, pTexture->pDxResource, &rtvDesc, &handle);
                    }
                }
                else
                {
                    DxDescriptorID handle = pTexture->mRTVDescriptors + i;

                    add_rtv(pRenderer, pTexture->pDxResource, &rtvDesc, &handle);
                }
            }
        }
    }

#if defined(_DEBUG)
    if (pDesc->pDebugName)
    {
        // Set name
        pTexture->pDxResource->SetName(pDesc->pDebugName);
    }
#endif

    pTexture->mMipLevels = pDesc->mipLevels;
    pTexture->mWidth = pDesc->width;
    pTexture->mHeight = pDesc->height;
    pTexture->mDepth = pDesc->depth;
    pTexture->mUav = pDesc->descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE;
    pTexture->mFormat = pDesc->format;
    pTexture->mArraySizeMinusOne = pDesc->arraySize - 1;
    pTexture->mSampleCount = pDesc->sampleCount;

    *pp_texture = pTexture;
}

void REI_removeTexture(REI_Renderer* pRenderer, REI_Texture* p_texture)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_texture);

    // return texture descriptors
    if (p_texture->mDescriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(
            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], p_texture->mDescriptors,
            p_texture->m_SRV_UAV_HandleCount);
    }
    if (p_texture->mRTVDescriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(
            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV], p_texture->mRTVDescriptors,
            p_texture->m_RTV_DSV_HandleCount);
    }
    if (p_texture->mDSVDescriptors != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(
            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV], p_texture->mDSVDescriptors,
            p_texture->m_RTV_DSV_HandleCount);
    }

    if (p_texture->mOwnsImage)
    {
        SAFE_RELEASE(p_texture->pDxAllocation);
        SAFE_RELEASE(p_texture->pDxResource);
    }

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, p_texture);
}

void REI_setTextureName(REI_Renderer* pRenderer, REI_Texture* pTexture, const char* pName)
{
#if defined(_DEBUG)
    REI_ASSERT(pRenderer);
    REI_ASSERT(pTexture);
    REI_ASSERT(pName);

    wchar_t wName[128] = {};
    size_t  numConverted = 0;
    mbstowcs_s(&numConverted, wName, pName, 128);
    pTexture->pDxResource->SetName(wName);
#endif
}

void REI_addShaders(REI_Renderer* pRenderer, const REI_ShaderDesc* p_descs, uint32_t shaderCount, REI_Shader** pp_shader_programs)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(!shaderCount || p_descs);
    REI_ASSERT(!shaderCount || pp_shader_programs);

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    for (uint32_t i = 0; i < shaderCount; ++i)
    {
        const REI_ShaderDesc& desc = p_descs[i];

        REI_StackAllocator<false> persistentAlloc = {};
        persistentAlloc.reserve<REI_Shader>().reserve<uint8_t>(desc.byteCodeSize);

        if (!persistentAlloc.done(allocator))
        {
            pLog(REI_LOG_TYPE_ERROR, "REI_addShader wasn't able to allocate enough memory for persistentAlloc");
            REI_ASSERT(false);
            pp_shader_programs[i] = nullptr;
            continue;
        }

        REI_Shader* pShaderProgram = persistentAlloc.alloc<REI_Shader>();
        pShaderProgram->pShaderBytecode = persistentAlloc.alloc<uint8_t>(desc.byteCodeSize);
        pShaderProgram->bytecodeSize = desc.byteCodeSize;
        memcpy(pShaderProgram->pShaderBytecode, desc.pByteCode, desc.byteCodeSize);

        pShaderProgram->stage = desc.stage;

        pp_shader_programs[i] = pShaderProgram;
    }
}

void REI_removeShaders(REI_Renderer* pRenderer, uint32_t shaderCount, REI_Shader** pp_shader_programs)
{
    //remove given shader
    for (uint32_t i = 0; i < shaderCount; ++i)
    {
        pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pp_shader_programs[i]);
    }
}

constexpr uint32_t kMaxResourceTableSize = 32;

template<D3D_ROOT_SIGNATURE_VERSION VERSION>
struct RootSigTypes;

template<>
struct RootSigTypes<D3D_ROOT_SIGNATURE_VERSION_1_0>
{
    typedef D3D12_ROOT_PARAMETER ROOT_PARAMETER;
    typedef D3D12_DESCRIPTOR_RANGE DESCRIPTOR_RANGE;

    static inline void setRangeFlags(DESCRIPTOR_RANGE& range, D3D12_DESCRIPTOR_RANGE_FLAGS flags){};

    static inline void setRootSignatureDesc(
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc, ROOT_PARAMETER* pParams, uint32_t numParameters,
        D3D12_STATIC_SAMPLER_DESC* pStaticSamplers, uint32_t numStaticSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags)
    {
        desc.Desc_1_0.NumParameters = numParameters;
        desc.Desc_1_0.pParameters = pParams;
        desc.Desc_1_0.NumStaticSamplers = numStaticSamplers;
        desc.Desc_1_0.pStaticSamplers = pStaticSamplers;
        desc.Desc_1_0.Flags = flags;
    };
};

template<>
struct RootSigTypes<D3D_ROOT_SIGNATURE_VERSION_1_1>
{
    typedef D3D12_ROOT_PARAMETER1    ROOT_PARAMETER;
    typedef D3D12_DESCRIPTOR_RANGE1 DESCRIPTOR_RANGE;

    static inline void setRangeFlags(DESCRIPTOR_RANGE& range, D3D12_DESCRIPTOR_RANGE_FLAGS flags) { range.Flags = flags; };

    static inline void setRootSignatureDesc(
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc, ROOT_PARAMETER* pParams, uint32_t numParameters,
        D3D12_STATIC_SAMPLER_DESC* pStaticSamplers, uint32_t numStaticSamplers, D3D12_ROOT_SIGNATURE_FLAGS flags)
    {
        desc.Desc_1_1.NumParameters = numParameters;
        desc.Desc_1_1.pParameters = pParams;
        desc.Desc_1_1.NumStaticSamplers = numStaticSamplers;
        desc.Desc_1_1.pStaticSamplers = pStaticSamplers;
        desc.Desc_1_1.Flags = flags;
    };
};

template<D3D_ROOT_SIGNATURE_VERSION VERSION>
HRESULT root_signature(REI_StackAllocator<true>& stackAlloc,
    REI_Renderer* pRenderer, const REI_RootSignatureDesc* pRootSignatureDesc, REI_RootSignature* pRootSignature,
    ID3DBlob*& rootSignatureString, uint32_t rootParamCount, uint32_t totalStaticSamplerCount)
{
    using ROOT_PARAMETER = typename RootSigTypes<VERSION>::ROOT_PARAMETER;
    using DESCRIPTOR_RANGE = typename RootSigTypes<VERSION>::DESCRIPTOR_RANGE;

    REI_LogPtr pLog = pRenderer->pLog;

    uint32_t                   rootSignatureDwords = 0;
    D3D12_STATIC_SAMPLER_DESC* staticSamplers = stackAlloc.alloc<D3D12_STATIC_SAMPLER_DESC>(totalStaticSamplerCount);
    ROOT_PARAMETER*            rootParams = stackAlloc.alloc<ROOT_PARAMETER>(rootParamCount);

    DESCRIPTOR_RANGE* cbvSrvUavRange =
        stackAlloc.alloc<DESCRIPTOR_RANGE>(REI_DESCRIPTOR_TABLE_SLOT_COUNT * kMaxResourceTableSize);
    DESCRIPTOR_RANGE* samplerRange =
        stackAlloc.alloc<DESCRIPTOR_RANGE>(REI_DESCRIPTOR_TABLE_SLOT_COUNT * kMaxResourceTableSize);

    REI_ShaderStage rootSigShaderStages = REI_SHADER_STAGE_NONE;
    uint32_t        staticSamplerSlot = -1;
    if (pRootSignatureDesc->staticSamplerBindingCount)
    {
        REI_ASSERT(pRootSignatureDesc->pStaticSamplerBindings);
        staticSamplerSlot = pRootSignatureDesc->staticSamplerSlot;
        uint32_t                staticSamplerOffset = 0;
        D3D12_SHADER_VISIBILITY staticSamplerShaderVisibility =
            util_to_dx12_shader_visibility((REI_ShaderStage)pRootSignatureDesc->staticSamplerStageFlags);

        (uint32_t&)rootSigShaderStages |= pRootSignatureDesc->staticSamplerStageFlags;

        for (uint32_t i = 0; i < pRootSignatureDesc->staticSamplerBindingCount; ++i)
        {
            REI_StaticSamplerBinding& setLayoutBinding = pRootSignatureDesc->pStaticSamplerBindings[i];

            pLog(
                REI_LOG_TYPE_INFO, "User specified static sampler: set = %u, reg = %u, arr size = %u",
                staticSamplerSlot, setLayoutBinding.reg, setLayoutBinding.descriptorCount);

            for (uint32_t ssampler = 0; ssampler < setLayoutBinding.descriptorCount; ++ssampler)
            {
                const D3D12_SAMPLER_DESC& desc = setLayoutBinding.ppStaticSamplers[ssampler]->mDesc;
                uint32_t                  index = ssampler + staticSamplerOffset;
                staticSamplers[index].Filter = desc.Filter;
                staticSamplers[index].AddressU = desc.AddressU;
                staticSamplers[index].AddressV = desc.AddressV;
                staticSamplers[index].AddressW = desc.AddressW;
                staticSamplers[index].MipLODBias = desc.MipLODBias;
                staticSamplers[index].MaxAnisotropy = desc.MaxAnisotropy;
                staticSamplers[index].ComparisonFunc = desc.ComparisonFunc;
                staticSamplers[index].MinLOD = desc.MinLOD;
                staticSamplers[index].MaxLOD = desc.MaxLOD;
                staticSamplers[index].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;

                staticSamplers[index].RegisterSpace = staticSamplerSlot;
                staticSamplers[index].ShaderRegister = setLayoutBinding.reg;
                staticSamplers[index].ShaderVisibility = staticSamplerShaderVisibility;
            }
            staticSamplerOffset += setLayoutBinding.descriptorCount;
        }
    }

    rootParamCount = 0;
    for (uint32_t i = 0; i < pRootSignatureDesc->tableLayoutCount; ++i)
    {
        REI_DescriptorTableLayout& setLayout = pRootSignatureDesc->pTableLayouts[i];
        REI_DescriptorTableSlot    slot = setLayout.slot;

        if (slot == staticSamplerSlot)
        {
            pLog(REI_LOG_TYPE_ERROR, "All static samplers must be in a separate set");
            REI_ASSERT(false);
        }

        D3D12_SHADER_VISIBILITY setLayoutShaderVisibiliry =
            util_to_dx12_shader_visibility((REI_ShaderStage)setLayout.stageFlags);
        (uint32_t&)rootSigShaderStages |= setLayout.stageFlags;

        uint32_t samplerRangeIndex = 0;
        uint32_t cbvSrvUavRangeIndex = 0;
        for (uint32_t descriptorIndex = 0; descriptorIndex < setLayout.bindingCount; ++descriptorIndex)
        {
            REI_DescriptorBinding& setLayoutBinding = setLayout.pBindings[descriptorIndex];

            // Find the D3D12 type of the descriptors
            if (setLayoutBinding.descriptorType == REI_DESCRIPTOR_TYPE_SAMPLER)
            {
                DESCRIPTOR_RANGE& range = samplerRange[slot * kMaxResourceTableSize + samplerRangeIndex];
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
                range.BaseShaderRegister = setLayoutBinding.reg;
                range.RegisterSpace = slot;
                range.NumDescriptors = setLayoutBinding.descriptorCount;
                range.OffsetInDescriptorsFromTableStart = descriptorIndex;
                RootSigTypes<VERSION>::setRangeFlags(range, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

                // Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
                // This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
                pRootSignature->mDxCumulativeSamplerDescriptorCounts[slot] += setLayoutBinding.descriptorCount;

                ++samplerRangeIndex;
            }
            else
            {
                DESCRIPTOR_RANGE& range = cbvSrvUavRange[slot * kMaxResourceTableSize + cbvSrvUavRangeIndex];
                range.RangeType = util_to_dx12_descriptor_range(setLayoutBinding.descriptorType);
                range.BaseShaderRegister = setLayoutBinding.reg;
                range.RegisterSpace = slot;
                range.NumDescriptors = setLayoutBinding.descriptorCount;
                range.OffsetInDescriptorsFromTableStart = descriptorIndex;
                RootSigTypes<VERSION>::setRangeFlags(range, D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

                // Store the cumulative descriptor count so we can just fetch this value later when allocating descriptor handles
                // This avoids unnecessary loops in the future to find the unfolded number of descriptors (includes shader resource arrays) in the descriptor table
                pRootSignature->mDxCumulativeViewDescriptorCounts[slot] += setLayoutBinding.descriptorCount;
                ++cbvSrvUavRangeIndex;
            }
        }

        if (cbvSrvUavRangeIndex)
        {
            ROOT_PARAMETER& param = rootParams[rootParamCount];
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = setLayoutShaderVisibiliry;
            param.DescriptorTable.NumDescriptorRanges = cbvSrvUavRangeIndex;
            param.DescriptorTable.pDescriptorRanges = cbvSrvUavRange + slot * kMaxResourceTableSize;

            // Store some of the binding info which will be required later when binding the descriptor table
            // We need the root index when calling SetRootDescriptorTable
            pRootSignature->mDxViewDescriptorTableRootIndices[slot] = rootParamCount;

            ++rootParamCount;

            rootSignatureDwords += 1;
        }

        if (samplerRangeIndex)
        {
            ROOT_PARAMETER& param = rootParams[rootParamCount];
            param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            param.ShaderVisibility = setLayoutShaderVisibiliry;
            param.DescriptorTable.NumDescriptorRanges = samplerRangeIndex;
            param.DescriptorTable.pDescriptorRanges = samplerRange + slot * kMaxResourceTableSize;

            // Store some of the binding info which will be required later when binding the descriptor table
            // We need the root index when calling SetRootDescriptorTable
            pRootSignature->mDxSamplerDescriptorTableRootIndices[slot] = rootParamCount;

            ++rootParamCount;

            rootSignatureDwords += 1;
        }
    }

    for (uint32_t i = 0; i < pRootSignatureDesc->pushConstantRangeCount; ++i)
    {
        REI_PushConstantRange& constDesc = pRootSignatureDesc->pPushConstantRanges[i];
        uint32_t               sizeInDwords = constDesc.size / sizeof(uint32_t);

        (uint32_t&)rootSigShaderStages |= constDesc.stageFlags;

        ROOT_PARAMETER& constParam = rootParams[rootParamCount];
        constParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        constParam.ShaderVisibility = util_to_dx12_shader_visibility((REI_ShaderStage)constDesc.stageFlags);
        constParam.Constants.Num32BitValues = sizeInDwords;
        constParam.Constants.RegisterSpace = constDesc.slot;
        constParam.Constants.ShaderRegister = constDesc.reg;

        for (uint32_t i = 0; i < REI_SHADER_STAGE_COUNT; ++i)
        {
            if (constDesc.stageFlags & REI_ShaderStage(1 << i))
                pRootSignature->mPushConstantRootParamIndex[i] = rootParamCount;
        }

        rootSignatureDwords += sizeInDwords;
        ++rootParamCount;
    }

    if (rootSignatureDwords > pRenderer->pActiveGpuSettings->capabilities.maxRootSignatureDWORDS)
    {
        pLog(
            REI_LOG_TYPE_ERROR, "Root signature size in DWORD (%u) exceeds maxRootSignatureDWORDS (%u)",
            rootSignatureDwords, pRenderer->pActiveGpuSettings->capabilities.maxRootSignatureDWORDS);

        REI_ASSERT(false);
    }
    // Specify the deny flags to avoid unnecessary shader stages being notified about descriptor modifications
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    if (!(rootSigShaderStages & REI_SHADER_STAGE_VERT))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
    if (!(rootSigShaderStages & REI_SHADER_STAGE_TESC))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
    if (!(rootSigShaderStages & REI_SHADER_STAGE_TESE))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
    if (!(rootSigShaderStages & REI_SHADER_STAGE_GEOM))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    if (!(rootSigShaderStages & REI_SHADER_STAGE_FRAG))
        rootSignatureFlags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    ID3DBlob*                            error = NULL;
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc = *stackAlloc.alloc<D3D12_VERSIONED_ROOT_SIGNATURE_DESC>();
    desc.Version = VERSION;
    RootSigTypes<VERSION>::setRootSignatureDesc(
        desc, rootParams, rootParamCount, staticSamplers, totalStaticSamplerCount, rootSignatureFlags);

    HRESULT hres = D3D12SerializeVersionedRootSignature(&desc, &rootSignatureString, &error);

    if (!SUCCEEDED(hres))
    {
        pLog(
            REI_LOG_TYPE_ERROR, "Failed to serialize root signature with error (%s)", (char*)error->GetBufferPointer());
    }

    SAFE_RELEASE(error);

    return hres;
}

void REI_addRootSignature(
    REI_Renderer* pRenderer, const REI_RootSignatureDesc* pRootSignatureDesc, REI_RootSignature** ppRootSignature)
{
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    if (pRootSignatureDesc->tableLayoutCount > REI_DESCRIPTOR_TABLE_SLOT_COUNT)
    {
        pLog(
            REI_LOG_TYPE_ERROR,
            "pRootSignatureDesc->tableLayoutCount can not be greater than REI_DESCRIPTOR_TABLE_SLOT_COUNT");
        REI_ASSERT(false);
        ppRootSignature = nullptr;
        return;
    }

    uint32_t rootParamCount = pRootSignatureDesc->pushConstantRangeCount + 2 * pRootSignatureDesc->tableLayoutCount;
    uint32_t totalStaticSamplerCount = 0;
    for (uint32_t i = 0; i < pRootSignatureDesc->staticSamplerBindingCount; ++i)
    {
        REI_StaticSamplerBinding& setLayoutBinding = pRootSignatureDesc->pStaticSamplerBindings[i];
        totalStaticSamplerCount += setLayoutBinding.descriptorCount;
    }

    REI_StackAllocator<true> stackAlloc = { 0 };
    stackAlloc.reserve<D3D12_STATIC_SAMPLER_DESC>(totalStaticSamplerCount)
        .reserve<D3D12_VERSIONED_ROOT_SIGNATURE_DESC>();
    if (pRenderer->mHighestRootSignatureVersion == D3D_ROOT_SIGNATURE_VERSION_1_1)
    {
        stackAlloc.reserve<D3D12_ROOT_PARAMETER1>(rootParamCount)
            .reserve<D3D12_DESCRIPTOR_RANGE1>(REI_DESCRIPTOR_TABLE_SLOT_COUNT * kMaxResourceTableSize * 2);
    }
    else 
    {
        stackAlloc.reserve<D3D12_ROOT_PARAMETER>(rootParamCount)
            .reserve<D3D12_DESCRIPTOR_RANGE>(REI_DESCRIPTOR_TABLE_SLOT_COUNT * kMaxResourceTableSize * 2);
    }

    if (!stackAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "REI_addRootSignature wasn't able to allocate enough memory for stackAlloc");
        REI_ASSERT(false);
        *ppRootSignature = nullptr;
        return;
    }

    REI_RootSignature* pRootSignature = (REI_RootSignature*)REI_calloc(allocator, sizeof(REI_RootSignature));

    REI_ASSERT(
        pRootSignatureDesc->pipelineType == REI_PIPELINE_TYPE_GRAPHICS ||
        pRootSignatureDesc->pipelineType == REI_PIPELINE_TYPE_COMPUTE);
    pRootSignature->mPipelineType = pRootSignatureDesc->pipelineType;

    ID3DBlob* rootSignatureString = NULL;
    if (pRenderer->mHighestRootSignatureVersion == D3D_ROOT_SIGNATURE_VERSION_1_1)
        CHECK_HRESULT(root_signature<D3D_ROOT_SIGNATURE_VERSION_1_1>(
            stackAlloc, pRenderer, pRootSignatureDesc, pRootSignature, rootSignatureString, rootParamCount,
            totalStaticSamplerCount));
    else
        CHECK_HRESULT(root_signature<D3D_ROOT_SIGNATURE_VERSION_1_0>(
            stackAlloc, pRenderer, pRootSignatureDesc, pRootSignature, rootSignatureString, rootParamCount,
            totalStaticSamplerCount));

    // If running Linked Mode (SLI) create root signature for all nodes
    // #NOTE : In non SLI mode, mNodeCount will be 0 which sets nodeMask to default value
    CHECK_HRESULT(pRenderer->pDxDevice->CreateRootSignature(
        0, rootSignatureString->GetBufferPointer(), rootSignatureString->GetBufferSize(),
        IID_PPV_ARGS(&pRootSignature->pDxRootSignature)));

    SAFE_RELEASE(rootSignatureString);

    *ppRootSignature = pRootSignature;
}

void REI_removeRootSignature(REI_Renderer* pRenderer, REI_RootSignature* pRootSignature)
{
    SAFE_RELEASE(pRootSignature->pDxRootSignature);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pRootSignature);
}

void addGraphicsPipeline(REI_Renderer* pRenderer, const REI_PipelineDesc* pMainDesc, REI_Pipeline** ppPipeline)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(ppPipeline);
    REI_ASSERT(pMainDesc);

    const REI_GraphicsPipelineDesc* pDesc = &pMainDesc->graphicsDesc;

    REI_ASSERT(pDesc->ppShaderPrograms);
    REI_ASSERT(pDesc->shaderProgramCount);
    REI_ASSERT(pDesc->pRootSignature);

    uint64_t psoShaderHash[2] = { 0 };
    uint64_t psoRenderHash[2] = { 0 };

    WCHAR                 hashName[65];
    hashName[0] = L'\0';
    
    //allocate new pipeline
    REI_Pipeline* pPipeline = REI_new<REI_Pipeline>(pRenderer->allocator);
    REI_ASSERT(pPipeline);

    REI_Shader** ppShaderPrograms = pDesc->ppShaderPrograms;
#if defined(REI_PLATFORM_WINDOWS)
    ID3D12PipelineLibrary* psoCache = pDesc->pCache ? pDesc->pCache->pLibrary : NULL;
#endif 

    pPipeline->mType = REI_PIPELINE_TYPE_GRAPHICS;
    pPipeline->pRootSignature = pDesc->pRootSignature->pDxRootSignature;

    //add to gpu
    D3D12_SHADER_BYTECODE shaderBytecodes[REI_SHADER_STAGE_COUNT - 1] = {};
    uint32_t numShaders = pDesc->shaderProgramCount;
    for (uint32_t i = 0; i < numShaders; ++i)
    {
        REI_Shader* pShader = ppShaderPrograms[i];
        if (pShader->stage == 0) 
        {
            REI_ASSERT(false, "Invalid shader stage");
            continue;
        }
        uint32_t index = 0;
        REI_ctz32(&index, pShader->stage);

        REI_ASSERT(!shaderBytecodes[index].pShaderBytecode, "Duplicate shader stage %u", pShader->stage);
        shaderBytecodes[index].BytecodeLength = (SIZE_T)pShader->bytecodeSize;
        shaderBytecodes[index].pShaderBytecode = pShader->pShaderBytecode;
    }

    DECLARE_ZERO(D3D12_STREAM_OUTPUT_DESC, stream_output_desc);
    stream_output_desc.pSODeclaration = NULL;
    stream_output_desc.NumEntries = 0;
    stream_output_desc.pBufferStrides = NULL;
    stream_output_desc.NumStrides = 0;
    stream_output_desc.RasterizedStream = 0;

    DECLARE_ZERO(D3D12_DEPTH_STENCILOP_DESC, depth_stencilop_desc);
    depth_stencilop_desc.StencilFailOp = D3D12_STENCIL_OP_KEEP;
    depth_stencilop_desc.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
    depth_stencilop_desc.StencilPassOp = D3D12_STENCIL_OP_KEEP;
    depth_stencilop_desc.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

    DECLARE_ZERO(D3D12_INPUT_ELEMENT_DESC, input_elements[REI_MAX_VERTEX_ATTRIBS]);
    uint32_t input_elements_count =
        pDesc->vertexAttribCount > REI_MAX_VERTEX_ATTRIBS ? REI_MAX_VERTEX_ATTRIBS : pDesc->vertexAttribCount;

    // Make sure there's attributes
    if (input_elements_count)
    {
        for (uint32_t attrib_index = 0; attrib_index < input_elements_count; ++attrib_index)
        {
            const REI_VertexAttrib& attrib = pDesc->pVertexAttribs[attrib_index];

            UINT semantic_index = 0;
            const char* semantic_name = nullptr;

            switch (attrib.semantic)
            {
                case REI_SEMANTIC_POSITION0:
                case REI_SEMANTIC_POSITION1:
                case REI_SEMANTIC_POSITION2:
                case REI_SEMANTIC_POSITION3:
                    semantic_name = "POSITION";
                    semantic_index = attrib.semantic - REI_SEMANTIC_POSITION0;
                    break;
                case REI_SEMANTIC_NORMAL:
                    semantic_name = "NORMAL";
                    semantic_index = 0;
                    break;
                case REI_SEMANTIC_TANGENT:
                    semantic_name = "TANGENT";
                    semantic_index = 0;
                    break;
                case REI_SEMANTIC_BITANGENT:
                    semantic_name = "BITANGENT";
                    semantic_index = 0;
                    break;
                case REI_SEMANTIC_COLOR0:
                case REI_SEMANTIC_COLOR1:
                case REI_SEMANTIC_COLOR2:
                case REI_SEMANTIC_COLOR3:
                    semantic_name = "COLOR";
                    semantic_index = attrib.semantic - REI_SEMANTIC_COLOR0;
                    break;
                case REI_SEMANTIC_TEXCOORD0:
                case REI_SEMANTIC_TEXCOORD1:
                case REI_SEMANTIC_TEXCOORD2:
                case REI_SEMANTIC_TEXCOORD3:
                case REI_SEMANTIC_TEXCOORD4:
                case REI_SEMANTIC_TEXCOORD5:
                case REI_SEMANTIC_TEXCOORD6:
                case REI_SEMANTIC_TEXCOORD7:
                    semantic_name = "TEXCOORD";
                    semantic_index = attrib.semantic - REI_SEMANTIC_TEXCOORD0;
                    break;
                case REI_SEMANTIC_USERDEFINED0:
                case REI_SEMANTIC_USERDEFINED1:
                case REI_SEMANTIC_USERDEFINED2:
                case REI_SEMANTIC_USERDEFINED3:
                case REI_SEMANTIC_USERDEFINED4:
                case REI_SEMANTIC_USERDEFINED5:
                case REI_SEMANTIC_USERDEFINED6:
                case REI_SEMANTIC_USERDEFINED7:
                case REI_SEMANTIC_USERDEFINED8:
                case REI_SEMANTIC_USERDEFINED9:
                case REI_SEMANTIC_USERDEFINED10:
                case REI_SEMANTIC_USERDEFINED11:
                case REI_SEMANTIC_USERDEFINED12:
                case REI_SEMANTIC_USERDEFINED13:
                case REI_SEMANTIC_USERDEFINED14:
                case REI_SEMANTIC_USERDEFINED15:
                    semantic_name = "USERDEFINED";
                    semantic_index = attrib.semantic - REI_SEMANTIC_USERDEFINED0;
                    break;
                default: REI_ASSERT(false, "Unknown semantic");
            }

            input_elements[attrib_index].SemanticName = semantic_name;
            input_elements[attrib_index].SemanticIndex = semantic_index;

            input_elements[attrib_index].Format = REI_Format_ToDXGI_FORMAT(attrib.format);
            input_elements[attrib_index].InputSlot = attrib.binding;
            input_elements[attrib_index].AlignedByteOffset = attrib.offset;
            if (attrib.rate == REI_VERTEX_ATTRIB_RATE_INSTANCE)
            {
                input_elements[attrib_index].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                input_elements[attrib_index].InstanceDataStepRate = 1;
            }
            else
            {
                input_elements[attrib_index].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                input_elements[attrib_index].InstanceDataStepRate = 0;
            }

#if defined(REI_PLATFORM_WINDOWS)
            if (psoCache)
            {
                REI_murmurHash3_128((uint8_t*)&attrib, sizeof(attrib), (uintptr_t)psoRenderHash[0], psoRenderHash);
            }
#endif
        }
    }

    DECLARE_ZERO(D3D12_INPUT_LAYOUT_DESC, input_layout_desc);
    input_layout_desc.pInputElementDescs = input_elements_count ? input_elements : NULL;
    input_layout_desc.NumElements = input_elements_count;

    uint32_t render_target_count = REI_min(pDesc->renderTargetCount, (uint32_t)REI_MAX_RENDER_TARGET_ATTACHMENTS);
    render_target_count = REI_min(render_target_count, (uint32_t)D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT);

    DECLARE_ZERO(DXGI_SAMPLE_DESC, sample_desc);
    sample_desc.Count = pDesc->sampleCount ? pDesc->sampleCount : 1;
    sample_desc.Quality = 0;

    DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
    cached_pso_desc.pCachedBlob = NULL;
    cached_pso_desc.CachedBlobSizeInBytes = 0;

    DECLARE_ZERO(D3D12_GRAPHICS_PIPELINE_STATE_DESC, pipeline_state_desc);
    pipeline_state_desc.pRootSignature = pDesc->pRootSignature->pDxRootSignature;
    pipeline_state_desc.VS = shaderBytecodes[0];
    pipeline_state_desc.HS = shaderBytecodes[1];
    pipeline_state_desc.GS = shaderBytecodes[2];
    pipeline_state_desc.DS = shaderBytecodes[3];
    pipeline_state_desc.PS = shaderBytecodes[4];
    pipeline_state_desc.StreamOutput = stream_output_desc;
    pipeline_state_desc.BlendState = pDesc->pBlendState ? util_to_blend_desc(pDesc->pBlendState) : gDefaultBlendDesc;
    pipeline_state_desc.SampleMask = UINT_MAX;
    pipeline_state_desc.RasterizerState =
        pDesc->pRasterizerState ? util_to_rasterizer_desc(pDesc->pRasterizerState) : gDefaultRasterizerDesc;
    pipeline_state_desc.DepthStencilState =
        pDesc->pDepthState ? util_to_depth_desc(pDesc->pDepthState) : gDefaultDepthDesc;
    pipeline_state_desc.InputLayout = input_layout_desc;
    pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipeline_state_desc.PrimitiveTopologyType = util_to_dx12_primitive_topology_type(pDesc->primitiveTopo);
    pipeline_state_desc.NumRenderTargets = render_target_count;
    pipeline_state_desc.DSVFormat = REI_Format_ToDXGI_FORMAT(pDesc->depthStencilFormat);
    pipeline_state_desc.SampleDesc = sample_desc;
    pipeline_state_desc.CachedPSO = cached_pso_desc;
    pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    for (uint32_t attrib_index = 0; attrib_index < render_target_count; ++attrib_index)
    {
        pipeline_state_desc.RTVFormats[attrib_index] = REI_Format_ToDXGI_FORMAT(pDesc->pColorFormats[attrib_index]);
    }

    HRESULT result = E_FAIL;
#if defined(REI_PLATFORM_WINDOWS)
    if (psoCache)
    {
        for (uint32_t i = 0; i < REI_SHADER_STAGE_COUNT - 1; ++i)
        {
            REI_murmurHash3_128(
                (uint8_t*)shaderBytecodes[i].pShaderBytecode, (int)shaderBytecodes[i].BytecodeLength,
                (uintptr_t)psoShaderHash[0], psoShaderHash);
        }

        REI_murmurHash3_128(
            (uint8_t*)&pipeline_state_desc.BlendState, sizeof(D3D12_BLEND_DESC), (uintptr_t)psoRenderHash[0],
            psoRenderHash);
        REI_murmurHash3_128(
            (uint8_t*)&pipeline_state_desc.DepthStencilState, sizeof(D3D12_DEPTH_STENCIL_DESC),
            (uintptr_t)psoRenderHash[0], psoRenderHash);
        REI_murmurHash3_128(
            (uint8_t*)&pipeline_state_desc.RasterizerState, sizeof(D3D12_RASTERIZER_DESC), (uintptr_t)psoRenderHash[0],
            psoRenderHash);
        REI_murmurHash3_128(
            (uint8_t*)pipeline_state_desc.RTVFormats, render_target_count * sizeof(DXGI_FORMAT),
            (uintptr_t)psoRenderHash[0], psoRenderHash);
        REI_murmurHash3_128(
            (uint8_t*)&pipeline_state_desc.DSVFormat, sizeof(DXGI_FORMAT), (uintptr_t)psoRenderHash[0], psoRenderHash);
        REI_murmurHash3_128(
            (uint8_t*)&pipeline_state_desc.PrimitiveTopologyType, sizeof(D3D12_PRIMITIVE_TOPOLOGY_TYPE),
            (uintptr_t)psoRenderHash[0], psoRenderHash);
        REI_murmurHash3_128(
            (uint8_t*)&pipeline_state_desc.SampleDesc, sizeof(DXGI_SAMPLE_DESC), (uintptr_t)psoRenderHash[0],
            psoRenderHash);
        REI_murmurHash3_128(
            (uint8_t*)&pipeline_state_desc.NodeMask, sizeof(UINT), (uintptr_t)psoRenderHash[0], psoRenderHash);

        swprintf_s(
            hashName, 17, L"%016llX%016llX%016llX%016llX", psoShaderHash[0], psoShaderHash[1], psoRenderHash[0],
            psoRenderHash[1]);

        result = psoCache->LoadGraphicsPipeline(
           hashName, &pipeline_state_desc, IID_PPV_ARGS(&pPipeline->pDxPipelineState));
    }
#endif

    if (!SUCCEEDED(result))
    {
        CHECK_HRESULT(pRenderer->pDxDevice->CreateGraphicsPipelineState(
            &pipeline_state_desc, IID_PPV_ARGS(&pPipeline->pDxPipelineState)));

#if defined(REI_PLATFORM_WINDOWS)
        if (psoCache)
        {
            CHECK_HRESULT(psoCache->StorePipeline(hashName, pPipeline->pDxPipelineState));
        }
#endif
    }

    D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    switch (pDesc->primitiveTopo)
    {
        case REI_PRIMITIVE_TOPO_POINT_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST; break;
        case REI_PRIMITIVE_TOPO_LINE_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST; break;
        case REI_PRIMITIVE_TOPO_LINE_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
        case REI_PRIMITIVE_TOPO_TRI_LIST: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
        case REI_PRIMITIVE_TOPO_TRI_STRIP: topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
        case REI_PRIMITIVE_TOPO_PATCH_LIST:
        {
            topology = (D3D_PRIMITIVE_TOPOLOGY)(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (pDesc->patchControlPoints - 1));
        }
        break;

        default: break;
    }

    REI_ASSERT(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED != topology);
    pPipeline->mDxPrimitiveTopology = topology;

    *ppPipeline = pPipeline;
}

void addComputePipeline(REI_Renderer* pRenderer, const REI_PipelineDesc* pMainDesc, REI_Pipeline** ppPipeline)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(ppPipeline);
    REI_ASSERT(pMainDesc);

    const REI_ComputePipelineDesc* pDesc = &pMainDesc->computeDesc;

    REI_ASSERT(pDesc->pShaderProgram);
    REI_ASSERT(pDesc->pRootSignature);
    REI_ASSERT(pDesc->pShaderProgram->pShaderBytecode);

    //allocate new pipeline
    REI_Pipeline* pPipeline = REI_new<REI_Pipeline>(pRenderer->allocator);
    REI_ASSERT(pPipeline);

    pPipeline->mType = REI_PIPELINE_TYPE_COMPUTE;
    pPipeline->pRootSignature = pDesc->pRootSignature->pDxRootSignature;

    //add pipeline specifying its for compute purposes
    DECLARE_ZERO(D3D12_SHADER_BYTECODE, CS);
    CS.BytecodeLength = (SIZE_T)pDesc->pShaderProgram->bytecodeSize;
    CS.pShaderBytecode = pDesc->pShaderProgram->pShaderBytecode;

    DECLARE_ZERO(D3D12_CACHED_PIPELINE_STATE, cached_pso_desc);
    cached_pso_desc.pCachedBlob = NULL;
    cached_pso_desc.CachedBlobSizeInBytes = 0;

    DECLARE_ZERO(D3D12_COMPUTE_PIPELINE_STATE_DESC, pipeline_state_desc);
    pipeline_state_desc.pRootSignature = pDesc->pRootSignature->pDxRootSignature;
    pipeline_state_desc.CS = CS;
    pipeline_state_desc.CachedPSO = cached_pso_desc;
    pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

    HRESULT result = E_FAIL;

#if defined(REI_PLATFORM_WINDOWS)
    ID3D12PipelineLibrary* psoCache = pDesc->pCache ? pDesc->pCache->pLibrary : NULL;
    uint64_t               psoShaderHash = 0;

    WCHAR hashName[17];
    hashName[0] = L'\0';

    if (psoCache)
    {
        psoShaderHash = REI_murmurHash2_x64_64((uint8_t*)CS.pShaderBytecode, (int)CS.BytecodeLength, psoShaderHash);
        swprintf_s(hashName, 17, L"%016llX", psoShaderHash);

        result = psoCache->LoadComputePipeline(hashName, &pipeline_state_desc, IID_PPV_ARGS(&pPipeline->pDxPipelineState));
    }
#endif

    if (!SUCCEEDED(result))
    {
        CHECK_HRESULT(pRenderer->pDxDevice->CreateComputePipelineState(
            &pipeline_state_desc, IID_PPV_ARGS(&pPipeline->pDxPipelineState)));
#if defined(REI_PLATFORM_WINDOWS)
        if (psoCache)
        {
            CHECK_HRESULT(psoCache->StorePipeline(hashName, pPipeline->pDxPipelineState));
        }
#endif
    }

    *ppPipeline = pPipeline;
}

void REI_addPipeline(REI_Renderer* pRenderer, const REI_PipelineDesc* p_pipeline_settings, REI_Pipeline** pp_pipeline)
{
    switch (p_pipeline_settings->type)
    {
        case (REI_PIPELINE_TYPE_COMPUTE):
        {
            addComputePipeline(pRenderer, p_pipeline_settings, pp_pipeline);
            break;
        }
        case (REI_PIPELINE_TYPE_GRAPHICS):
        {
            addGraphicsPipeline(pRenderer, p_pipeline_settings, pp_pipeline);
            break;
        }
        default:
        {
            REI_ASSERT(false);
            *pp_pipeline = {};
            break;
        }
    }
}

void REI_removePipeline(REI_Renderer* pRenderer, REI_Pipeline* p_pipeline)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_pipeline);

    //delete pipeline from device
    SAFE_RELEASE(p_pipeline->pDxPipelineState);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, p_pipeline);
}

void REI_addDescriptorTableArray(
    REI_Renderer* pRenderer, const REI_DescriptorTableArrayDesc* pDesc, REI_DescriptorTableArray** ppDescriptorTableArr)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(ppDescriptorTableArr);

    const REI_RootSignature* pRootSignature = pDesc->pRootSignature;
    const uint8_t            slot = pDesc->slot;
    const uint32_t           cbvSrvUavDescCount = pRootSignature->mDxCumulativeViewDescriptorCounts[slot];
    const uint32_t           samplerDescCount = pRootSignature->mDxCumulativeSamplerDescriptorCounts[slot];

    REI_DescriptorTableArray* pDescriptorTableArr = REI_new<REI_DescriptorTableArray>(pRenderer->allocator);
    REI_ASSERT(pDescriptorTableArr);

    pDescriptorTableArr->pRootSignature = pRootSignature;
    pDescriptorTableArr->slot = slot;
    pDescriptorTableArr->maxTables = pDesc->maxTables;
    pDescriptorTableArr->mCbvSrvUavRootIndex = pRootSignature->mDxViewDescriptorTableRootIndices[slot];
    pDescriptorTableArr->mSamplerRootIndex = pRootSignature->mDxSamplerDescriptorTableRootIndices[slot];
    pDescriptorTableArr->mCbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorTableArr->mSamplerHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorTableArr->mPipelineType = pRootSignature->mPipelineType;

    if (cbvSrvUavDescCount || samplerDescCount)
    {
        if (cbvSrvUavDescCount)
        {
            DescriptorHeap* pHeap = pRenderer->pCbvSrvUavHeaps;
            pDescriptorTableArr->mCbvSrvUavHandle =
                consume_descriptor_handles(pHeap, cbvSrvUavDescCount * pDesc->maxTables);
            pDescriptorTableArr->mCbvSrvUavStride = cbvSrvUavDescCount;
        }
        if (samplerDescCount)
        {
            DescriptorHeap* pHeap = pRenderer->pSamplerHeaps;
            pDescriptorTableArr->mSamplerHandle =
                consume_descriptor_handles(pHeap, samplerDescCount * pDesc->maxTables);
            pDescriptorTableArr->mSamplerStride = samplerDescCount;
        }
    }

    *ppDescriptorTableArr = pDescriptorTableArr;
}

void REI_removeDescriptorTableArray(REI_Renderer* pRenderer, REI_DescriptorTableArray* pDescriptorTableArr)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDescriptorTableArr);

    if (pDescriptorTableArr->mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(
            pRenderer->pCbvSrvUavHeaps, pDescriptorTableArr->mCbvSrvUavHandle,
            pDescriptorTableArr->mCbvSrvUavStride * pDescriptorTableArr->maxTables);
    }

    if (pDescriptorTableArr->mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
    {
        return_descriptor_handles(
            pRenderer->pSamplerHeaps, pDescriptorTableArr->mSamplerHandle,
            pDescriptorTableArr->mSamplerStride * pDescriptorTableArr->maxTables);
    }

    pDescriptorTableArr->mCbvSrvUavHandle = D3D12_DESCRIPTOR_ID_NONE;
    pDescriptorTableArr->mSamplerHandle = D3D12_DESCRIPTOR_ID_NONE;

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pDescriptorTableArr);
}

void REI_updateDescriptorTableArray(
    REI_Renderer* pRenderer, REI_DescriptorTableArray* pDescriptorTableArr, uint32_t count,
    const REI_DescriptorData* pParams)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDescriptorTableArr);

    const REI_RootSignature* pRootSignature = pDescriptorTableArr->pRootSignature;
    const uint8_t            slot = pDescriptorTableArr->slot;

    for (uint32_t i = 0; i < count; ++i)
    {
        const REI_DescriptorData* pParam = pParams + i;
        uint32_t                  tableIndex = pParam->tableIndex;
        REI_ASSERT(tableIndex < pDescriptorTableArr->maxTables);

        uint32_t descriptorIndex = pParam->descriptorIndex;
        VALIDATE_DESCRIPTOR(descriptorIndex != -1, "REI_DescriptorData has invalid descriptorIndex");

        const REI_DescriptorType type = pParam->descriptorType;
        const uint32_t           arrayCount = REI_max(1U, pParam->count);

        if (type == REI_DESCRIPTOR_TYPE_SAMPLER)
        {
            VALIDATE_DESCRIPTOR(
                descriptorIndex < pDescriptorTableArr->mSamplerStride,
                "REI_DescriptorData has invalid descriptorIndex");

            VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL Sampler");

            for (uint32_t arr = 0; arr < arrayCount; ++arr)
            {
                VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL Sampler [%u]", arr);

                copy_descriptor_handle(
                    pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER],
                    pParam->ppSamplers[arr]->mDescriptor, pRenderer->pSamplerHeaps,
                    pDescriptorTableArr->mSamplerHandle + tableIndex * pDescriptorTableArr->mSamplerStride +
                        descriptorIndex + arr);
            }
        }
        else
        {
            VALIDATE_DESCRIPTOR(
                descriptorIndex < pDescriptorTableArr->mCbvSrvUavStride,
                "REI_DescriptorData has invalid descriptorIndex");

            switch (type)
            {
                case REI_DESCRIPTOR_TYPE_TEXTURE:
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL Texture (%s)");

                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL Texture [%u]", arr);

                        copy_descriptor_handle(
                            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
                            pParam->ppTextures[arr]->mDescriptors, pRenderer->pCbvSrvUavHeaps,
                            pDescriptorTableArr->mCbvSrvUavHandle + tableIndex * pDescriptorTableArr->mCbvSrvUavStride +
                                descriptorIndex + arr);
                    }
                    break;
                }
                case REI_DESCRIPTOR_TYPE_RW_TEXTURE:
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW Texture");

                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW Texture [%u]", arr);

                        DxDescriptorID srcId = pParam->ppTextures[arr]->mDescriptors + pParam->UAVMipSlice +
                                               pParam->ppTextures[arr]->mUavStartIndex;

                        copy_descriptor_handle(
                            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                            pRenderer->pCbvSrvUavHeaps,
                            pDescriptorTableArr->mCbvSrvUavHandle + tableIndex * pDescriptorTableArr->mCbvSrvUavStride +
                                descriptorIndex + arr);
                    }
                    break;
                }
                case REI_DESCRIPTOR_TYPE_BUFFER:
                case REI_DESCRIPTOR_TYPE_BUFFER_RAW:
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Buffer");

                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Buffer [%u]", arr);

                        DxDescriptorID srcId =
                            pParam->ppBuffers[arr]->mDescriptors + pParam->ppBuffers[arr]->mSrvDescriptorOffset;

                        copy_descriptor_handle(
                            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                            pRenderer->pCbvSrvUavHeaps,
                            pDescriptorTableArr->mCbvSrvUavHandle + tableIndex * pDescriptorTableArr->mCbvSrvUavStride +
                                descriptorIndex + arr);
                    }
                    break;
                }
                case REI_DESCRIPTOR_TYPE_RW_BUFFER:
                case REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Buffer ");

                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Buffer [%u]", arr);

                        DxDescriptorID srcId =
                            pParam->ppBuffers[arr]->mDescriptors + pParam->ppBuffers[arr]->mUavDescriptorOffset;

                        copy_descriptor_handle(
                            pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV], srcId,
                            pRenderer->pCbvSrvUavHeaps,
                            pDescriptorTableArr->mCbvSrvUavHandle + tableIndex * pDescriptorTableArr->mCbvSrvUavStride +
                                descriptorIndex + arr);
                    }
                    break;
                }
                case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform Buffer");

                    if (pParam->pOffsets && pParam->pSizes)
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                        {
                            uint64_t offset = pParam->pOffsets[arr];
                            uint64_t size = pParam->pSizes[arr];
                            VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer [%u]", arr);
                            VALIDATE_DESCRIPTOR(size > 0, "Descriptor - pRanges[%u].mSize is zero", arr);
                            VALIDATE_DESCRIPTOR(
                                size <= D3D12_REQ_CONSTANT_BUFFER_SIZE,
                                "Descriptor - pRanges[%u].mSize is %u which exceeds max size %u", arr, size,
                                D3D12_REQ_CONSTANT_BUFFER_SIZE);

                            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                            cbvDesc.BufferLocation = pParam->ppBuffers[arr]->mDxGpuAddress + offset;
                            cbvDesc.SizeInBytes = (UINT)size;
                            uint32_t       setStart = tableIndex * pDescriptorTableArr->mCbvSrvUavStride;
                            DxDescriptorID cbv =
                                pDescriptorTableArr->mCbvSrvUavHandle + setStart + (descriptorIndex + arr);
                            pRenderer->pDxDevice->CreateConstantBufferView(
                                &cbvDesc, descriptor_id_to_cpu_handle(&pRenderer->pCbvSrvUavHeaps[0], cbv));
                        }
                    }
                    else
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                        {
                            VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Uniform Buffer [%u]", arr);
                            VALIDATE_DESCRIPTOR(
                                pParam->ppBuffers[arr]->mSize <= D3D12_REQ_CONSTANT_BUFFER_SIZE,
                                "Descriptor - pParam->ppBuffers[%u]->mSize is %u which exceeds max size %u", arr,
                                pParam->ppBuffers[arr]->mSize, D3D12_REQ_CONSTANT_BUFFER_SIZE);

                            copy_descriptor_handle(
                                pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
                                pParam->ppBuffers[arr]->mDescriptors, pRenderer->pCbvSrvUavHeaps,
                                pDescriptorTableArr->mCbvSrvUavHandle +
                                    tableIndex * pDescriptorTableArr->mCbvSrvUavStride + descriptorIndex + arr);
                        }
                    }
                    break;
                }

                default: break;
            }
        }
    }
}

void REI_addIndirectCommandSignature(
    REI_Renderer* pRenderer, const REI_CommandSignatureDesc* pDesc, REI_CommandSignature** ppCommandSignature)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(pDesc->pArgDescs);
    REI_ASSERT(ppCommandSignature);
    REI_ASSERT(false);
#if 0
    REI_CommandSignature* pCommandSignature = (REI_CommandSignature*)pRenderer->allocator.pMalloc(
        pRenderer->allocator.pUserData, sizeof(REI_CommandSignature), 0);
    REI_ASSERT(pCommandSignature);

    bool needRootSignature = false;
    // calculate size through arguement types
    uint32_t commandStride = 0;
    int      drawType = -1;    // REI_INDIRECT_ARG_INVALID;

    D3D12_INDIRECT_ARGUMENT_DESC* argumentDescs =
        (D3D12_INDIRECT_ARGUMENT_DESC*)alloca((pDesc->indirectArgCount + 1) * sizeof(D3D12_INDIRECT_ARGUMENT_DESC));

    for (uint32_t i = 0; i < pDesc->indirectArgCount; ++i)
    {

        const REI_DescriptorInfo* desc = NULL;
        if (pDesc->pArgDescs[i].type > REI_INDIRECT_DISPATCH)
        {
            //TODO: implement

            REI_ASSERT(pDesc->pArgDescs[i].rootParameterIndex < pDesc->pRootSignature->mDescriptorCount);
            desc = &pDesc->pRootSignature->pDescriptors[pDesc->pArgDescs[i].rootParameterIndex];

            REI_ASSERT(false);
        }


        switch (pDesc->pArgDescs[i].type)
        {
            case REI_INDIRECT_CONSTANT:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
                argumentDescs[i].Constant.RootParameterIndex = desc->mHandleIndex;    //-V522
                argumentDescs[i].Constant.DestOffsetIn32BitValues = 0;
                argumentDescs[i].Constant.Num32BitValuesToSet = desc->mSize;
                commandStride += sizeof(UINT) * argumentDescs[i].Constant.Num32BitValuesToSet;
                needRootSignature = true;
                break;
            case REI_INDIRECT_UNORDERED_ACCESS_VIEW:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW;
                argumentDescs[i].UnorderedAccessView.RootParameterIndex = desc->mHandleIndex;
                commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
                needRootSignature = true;
                break;
            case REI_INDIRECT_SHADER_RESOURCE_VIEW:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW;
                argumentDescs[i].ShaderResourceView.RootParameterIndex = desc->mHandleIndex;
                commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
                needRootSignature = true;
                break;
            case REI_INDIRECT_CONSTANT_BUFFER_VIEW:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
                argumentDescs[i].ConstantBufferView.RootParameterIndex = desc->mHandleIndex;
                commandStride += sizeof(D3D12_GPU_VIRTUAL_ADDRESS);
                needRootSignature = true;
                break;
            case REI_INDIRECT_VERTEX_BUFFER:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
                argumentDescs[i].VertexBuffer.Slot = desc->mHandleIndex;
                commandStride += sizeof(D3D12_VERTEX_BUFFER_VIEW);
                needRootSignature = true;
                break;
            case REI_INDIRECT_INDEX_BUFFER:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
                argumentDescs[i].VertexBuffer.Slot = desc->mHandleIndex;
                commandStride += sizeof(D3D12_INDEX_BUFFER_VIEW);
                needRootSignature = true;
                break;
            case REI_INDIRECT_DRAW:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
                commandStride += sizeof(REI_IndirectDrawArguments);
                // Only one draw command allowed. So make sure no other draw command args in the list
                REI_ASSERT(-1 == drawType);
                drawType = REI_INDIRECT_DRAW;
                break;
            case REI_INDIRECT_DRAW_INDEX:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
                commandStride += sizeof(REI_IndirectDrawIndexArguments);
                REI_ASSERT(-1 == drawType);
                drawType = REI_INDIRECT_DRAW_INDEX;
                break;
            case REI_INDIRECT_DISPATCH:
                argumentDescs[i].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
                commandStride += sizeof(REI_IndirectDispatchArguments);
                REI_ASSERT(-1 == drawType);
                drawType = REI_INDIRECT_DISPATCH;
                break;
            default: REI_ASSERT(false); break;
        }
    }

    if (needRootSignature)
    {
        REI_ASSERT(pDesc->pRootSignature);
    }

    D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
    commandSignatureDesc.pArgumentDescs = argumentDescs;
    commandSignatureDesc.NumArgumentDescs = pDesc->indirectArgCount;
    commandSignatureDesc.ByteStride = commandStride;

    uint32_t alignedStride = REI_align_up(commandStride, 16u);
    //TODO:
    if (/*!pDesc->mPacked &&*/ alignedStride != commandStride)
    {
        commandSignatureDesc.ByteStride += alignedStride - commandStride;
    }

    CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandSignature(
        &commandSignatureDesc, needRootSignature ? pDesc->pRootSignature->pDxRootSignature : NULL,
        IID_PPV_ARGS(&pCommandSignature->pDxHandle)));
    pCommandSignature->mStride = commandSignatureDesc.ByteStride;
    REI_ASSERT(drawType != -1);
    pCommandSignature->mDrawType = (REI_IndirectArgumentType)drawType;

    *ppCommandSignature = pCommandSignature;
#endif
}

void REI_removeIndirectCommandSignature(REI_Renderer* pRenderer, REI_CommandSignature* pCommandSignature)
{
    SAFE_RELEASE(pCommandSignature->pDxHandle);
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pCommandSignature);
}

void REI_addQueryPool(REI_Renderer* pRenderer, const REI_QueryPoolDesc* pDesc, REI_QueryPool** ppQueryPool)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(ppQueryPool);

    REI_QueryPool* pQueryPool =
        (REI_QueryPool*)pRenderer->allocator.pMalloc(pRenderer->allocator.pUserData, sizeof(REI_QueryPool), 0);
    REI_ASSERT(pQueryPool);

    pQueryPool->mType = util_to_dx12_query_type(pDesc->type);
    pQueryPool->mCount = pDesc->queryCount;

    D3D12_QUERY_HEAP_DESC desc = {};
    desc.Count = pDesc->queryCount;
    desc.Type = util_to_dx12_query_heap_type(pDesc->type);
    pRenderer->pDxDevice->CreateQueryHeap(&desc, IID_PPV_ARGS(&pQueryPool->pDxQueryHeap));

    *ppQueryPool = pQueryPool;
}

void REI_removeQueryPool(REI_Renderer* pRenderer, REI_QueryPool* pQueryPool)
{
    SAFE_RELEASE(pQueryPool->pDxQueryHeap);
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pQueryPool);
}

void REI_addCmdPool(REI_Renderer* pRenderer, REI_Queue* p_queue, bool transient, REI_CmdPool** pp_CmdPool)
{
    //ASSERT that renderer is valid
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_queue);
    REI_ASSERT(pp_CmdPool);

    //create one new CmdPool and add to renderer
    REI_CmdPool* pCmdPool = REI_new<REI_CmdPool>(pRenderer->allocator);
    REI_ASSERT(pCmdPool);

    if (REI_CMD_POOL_COPY == p_queue->mType)
    {
        d3d12_platform_create_copy_command_allocator(pRenderer, &pCmdPool->pDxCmdAlloc);
    }
    else
    {
        CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandAllocator(
            gDx12CmdTypeTranslator[p_queue->mType], IID_PPV_ARGS(&pCmdPool->pDxCmdAlloc)));
    }

    pCmdPool->pQueue = p_queue;

    *pp_CmdPool = pCmdPool;
}

void REI_resetCmdPool(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pCmdPool);
    CHECK_HRESULT(pCmdPool->pDxCmdAlloc->Reset());
}

void REI_removeCmdPool(REI_Renderer* pRenderer, REI_CmdPool* p_CmdPool)
{
    //check validity of given renderer and command pool
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_CmdPool);

    SAFE_RELEASE(p_CmdPool->pDxCmdAlloc);
    REI_delete(pRenderer->allocator, p_CmdPool);
}

void REI_addCmd(REI_Renderer* pRenderer, REI_CmdPool* p_CmdPool, bool secondary, REI_Cmd** pp_cmd)
{
    //verify that given pool is valid
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_CmdPool);
    REI_ASSERT(pp_cmd);

    // initialize to zero
    REI_Cmd* pCmd = REI_new<REI_Cmd>(pRenderer->allocator);
    REI_ASSERT(pCmd);

    //set command pool of new command
    pCmd->mType = p_CmdPool->pQueue->mType;
    pCmd->pQueue = p_CmdPool->pQueue;
    pCmd->pRenderer = pRenderer;

    pCmd->pBoundHeaps[0] = pRenderer->pCbvSrvUavHeaps;
    pCmd->pBoundHeaps[1] = pRenderer->pSamplerHeaps;

    pCmd->pCmdPool = p_CmdPool;

    if (REI_CMD_POOL_COPY == p_CmdPool->pQueue->mType)
    {
        d3d12_platform_create_copy_command_list(pRenderer, p_CmdPool, &pCmd->pDxCmdList);
    }
    else
    {
        ID3D12PipelineState*       initialState = NULL;
        ID3D12GraphicsCommandList* pTempCmd;
        CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandList(
            0, gDx12CmdTypeTranslator[pCmd->mType], p_CmdPool->pDxCmdAlloc, initialState, IID_PPV_ARGS(&pTempCmd)));
        //CHECK_HRESULT(pTempCmd->Close());
        pCmd->pDxCmdList = pTempCmd;
    }
    CHECK_HRESULT(((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)->Close());
    // Command lists are addd in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.

    *pp_cmd = pCmd;
}

void REI_removeCmd(REI_Renderer* pRenderer, REI_CmdPool* p_CmdPool, REI_Cmd* p_cmd)
{
    //verify that given command and pool are valid
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_cmd);
    SAFE_RELEASE(p_cmd->pDxCmdList);

    REI_delete(pRenderer->allocator, p_cmd);
}

void REI_beginCmd(REI_Cmd* p_cmd)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_cmd->pCmdPool);
    ID3D12GraphicsCommandList* pDxCmdList = (ID3D12GraphicsCommandList*)p_cmd->pDxCmdList;
    CHECK_HRESULT(pDxCmdList->Reset(p_cmd->pCmdPool->pDxCmdAlloc, NULL));

    if (p_cmd->mType != REI_CMD_POOL_COPY)
    {
        ID3D12DescriptorHeap* heaps[] = {
            p_cmd->pBoundHeaps[0]->pHeap,
            p_cmd->pBoundHeaps[1]->pHeap,
        };
        pDxCmdList->SetDescriptorHeaps(2, heaps);

        p_cmd->mBoundHeapStartHandles[0] = p_cmd->pBoundHeaps[0]->pHeap->GetGPUDescriptorHandleForHeapStart();
        p_cmd->mBoundHeapStartHandles[1] = p_cmd->pBoundHeaps[1]->pHeap->GetGPUDescriptorHandleForHeapStart();
    }

    // Reset CPU side data
    p_cmd->pBoundRootSignature = NULL;
}

void REI_endCmd(REI_Cmd* p_cmd)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_cmd->pDxCmdList);
    CHECK_HRESULT(((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)->Close());
}

void REI_cmdBindRenderTargets(
    REI_Cmd* p_cmd, uint32_t render_target_count, REI_Texture** pp_render_targets, REI_Texture* p_depth_stencil,
    const REI_LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices,
    uint32_t depthArraySlice, uint32_t depthMipSlice)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_cmd->pDxCmdList);
    REI_ASSERT(p_cmd->mType == REI_CMD_POOL_DIRECT);
    ID3D12GraphicsCommandList* pDxCmdList = (ID3D12GraphicsCommandList*)p_cmd->pDxCmdList;
    if (!render_target_count && !p_depth_stencil)
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = {};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvs[REI_MAX_RENDER_TARGET_ATTACHMENTS] = {};

    for (uint32_t i = 0; i < render_target_count; ++i)
    {
        if (!pColorMipSlices && !pColorArraySlices)
        {
            rtvs[i] = descriptor_id_to_cpu_handle(
                p_cmd->pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                pp_render_targets[i]->mRTVDescriptors);
        }
        else
        {
            uint32_t handle = 0;
            if (pColorMipSlices)
            {
                if (pColorArraySlices)
                    handle = 1 + pColorMipSlices[i] * ((uint32_t)pp_render_targets[i]->mArraySizeMinusOne + 1) +
                             pColorArraySlices[i];
                else
                    handle = 1 + pColorMipSlices[i];
            }
            else if (pColorArraySlices)
            {
                handle = 1 + pColorArraySlices[i];
            }

            rtvs[i] = descriptor_id_to_cpu_handle(
                p_cmd->pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_RTV],
                pp_render_targets[i]->mRTVDescriptors + handle);
        }
    }

    if (p_depth_stencil)
    {
        if (depthMipSlice <= 0 && depthArraySlice <= 0)
        {
            dsv = descriptor_id_to_cpu_handle(
                p_cmd->pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                p_depth_stencil->mDSVDescriptors);
        }
        else
        {
            uint32_t handle = 0;
            if (depthMipSlice != -1)
            {
                if (depthArraySlice != -1)
                    handle = 1 + depthMipSlice * ((uint32_t)p_depth_stencil->mArraySizeMinusOne + 1) + depthArraySlice;
                else
                    handle = 1 + depthMipSlice;
            }
            else if (depthArraySlice != -1)
            {
                handle = 1 + depthArraySlice;
            }

            dsv = descriptor_id_to_cpu_handle(
                p_cmd->pRenderer->pCPUDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_DSV],
                p_depth_stencil->mDSVDescriptors + handle);
        }
    }

    pDxCmdList->OMSetRenderTargets(
        render_target_count, rtvs, FALSE, dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL ? &dsv : NULL);

    // process clear actions (clear color/depth)
    if (loadActions)
    {
        for (uint32_t i = 0; i < render_target_count; ++i)
        {
            if (loadActions->loadActionsColor[i] == REI_LOAD_ACTION_CLEAR)
            {
                float color_rgba[4] = {
                    loadActions->clearColorValues[i].rt.r,
                    loadActions->clearColorValues[i].rt.g,
                    loadActions->clearColorValues[i].rt.b,
                    loadActions->clearColorValues[i].rt.a,
                };

                pDxCmdList->ClearRenderTargetView(rtvs[i], color_rgba, 0, NULL);
            }
        }
        if (loadActions->loadActionDepth == REI_LOAD_ACTION_CLEAR ||
            loadActions->loadActionStencil == REI_LOAD_ACTION_CLEAR)
        {
            REI_ASSERT(dsv.ptr != D3D12_GPU_VIRTUAL_ADDRESS_NULL);

            D3D12_CLEAR_FLAGS flags = (D3D12_CLEAR_FLAGS)0;
            if (loadActions->loadActionDepth == REI_LOAD_ACTION_CLEAR)
                flags |= D3D12_CLEAR_FLAG_DEPTH;
            if (loadActions->loadActionStencil == REI_LOAD_ACTION_CLEAR)
                flags |= D3D12_CLEAR_FLAG_STENCIL;
            REI_ASSERT(flags > 0);
            pDxCmdList->ClearDepthStencilView(
                dsv, flags, loadActions->clearDepth.ds.depth, (UINT8)loadActions->clearDepth.ds.stencil, 0, NULL);
        }
    }
}

void REI_cmdSetViewport(REI_Cmd* p_cmd, float x, float y, float width, float height, float min_depth, float max_depth)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_cmd->mType == REI_CMD_POOL_DIRECT);
    //set new viewport
    REI_ASSERT(p_cmd->pDxCmdList);

    D3D12_VIEWPORT viewport;
    viewport.TopLeftX = x;
    viewport.TopLeftY = y;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = min_depth;
    viewport.MaxDepth = max_depth;

    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)->RSSetViewports(1, &viewport);
}

void REI_cmdSetScissor(REI_Cmd* p_cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_cmd->mType == REI_CMD_POOL_DIRECT);
    //set new scissor values
    REI_ASSERT(p_cmd->pDxCmdList);

    D3D12_RECT scissor;
    scissor.left = x;
    scissor.top = y;
    scissor.right = x + width;
    scissor.bottom = y + height;

    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)->RSSetScissorRects(1, &scissor);
}

void REI_cmdSetStencilRef(REI_Cmd* p_cmd, REI_StencilFaceMask face, uint32_t ref)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_cmd->pDxCmdList);

    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)->OMSetStencilRef(ref);
}

bool reset_root_signature(REI_Cmd* pCmd, REI_PipelineType type, ID3D12RootSignature* pRootSignature)
{
    // Set root signature if the current one differs from pRootSignature
    if (pCmd->pBoundRootSignature != pRootSignature)
    {
        pCmd->pBoundRootSignature = pRootSignature;
        if (type == REI_PIPELINE_TYPE_GRAPHICS)
            ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)->SetGraphicsRootSignature(pRootSignature);
        else
            ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)->SetComputeRootSignature(pRootSignature);
    }

    return false;
}

void REI_cmdBindPipeline(REI_Cmd* p_cmd, REI_Pipeline* p_pipeline)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_pipeline);
    ID3D12GraphicsCommandList* pDxCmdList = (ID3D12GraphicsCommandList*)p_cmd->pDxCmdList;
    //bind given pipeline
    REI_ASSERT(p_cmd->pDxCmdList);

    if (p_pipeline->mType == REI_PIPELINE_TYPE_GRAPHICS)
    {
        REI_ASSERT(p_pipeline->pDxPipelineState);
        reset_root_signature(p_cmd, p_pipeline->mType, p_pipeline->pRootSignature);
        pDxCmdList->IASetPrimitiveTopology(p_pipeline->mDxPrimitiveTopology);
        pDxCmdList->SetPipelineState(p_pipeline->pDxPipelineState);
    }
    else
    {
        REI_ASSERT(p_pipeline->pDxPipelineState);
        reset_root_signature(p_cmd, p_pipeline->mType, p_pipeline->pRootSignature);
        pDxCmdList->SetPipelineState(p_pipeline->pDxPipelineState);
    }
}

void REI_cmdBindDescriptorTable(REI_Cmd* pCmd, uint32_t tableIndex, REI_DescriptorTableArray* pDescriptorTableArr)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pDescriptorTableArr);
    REI_ASSERT(tableIndex < pDescriptorTableArr->maxTables);
    ID3D12GraphicsCommandList* pDxCmdList = (ID3D12GraphicsCommandList*)pCmd->pDxCmdList;
    const uint8_t              slot = pDescriptorTableArr->slot;

    // Set root signature if the current one differs from pRootSignature
    reset_root_signature(
        pCmd, (REI_PipelineType)pDescriptorTableArr->mPipelineType,
        pDescriptorTableArr->pRootSignature->pDxRootSignature);

    // Bind the descriptor tables associated with this DescriptorSet
    if (pDescriptorTableArr->mPipelineType == REI_PIPELINE_TYPE_GRAPHICS)
    {
        if (pDescriptorTableArr->mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
        {
            pDxCmdList->SetGraphicsRootDescriptorTable(
                pDescriptorTableArr->mCbvSrvUavRootIndex,
                descriptor_id_to_gpu_handle(
                    pCmd->pBoundHeaps[0],
                    pDescriptorTableArr->mCbvSrvUavHandle + tableIndex * pDescriptorTableArr->mCbvSrvUavStride));
        }

        if (pDescriptorTableArr->mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
        {
            pDxCmdList->SetGraphicsRootDescriptorTable(
                pDescriptorTableArr->mSamplerRootIndex,
                descriptor_id_to_gpu_handle(
                    pCmd->pBoundHeaps[1],
                    pDescriptorTableArr->mSamplerHandle + tableIndex * pDescriptorTableArr->mSamplerStride));
        }
    }
    else
    {
        if (pDescriptorTableArr->mCbvSrvUavHandle != D3D12_DESCRIPTOR_ID_NONE)
        {
            pDxCmdList->SetComputeRootDescriptorTable(
                pDescriptorTableArr->mCbvSrvUavRootIndex,
                descriptor_id_to_gpu_handle(
                    pCmd->pBoundHeaps[0],
                    pDescriptorTableArr->mCbvSrvUavHandle + tableIndex * pDescriptorTableArr->mCbvSrvUavStride));
        }

        if (pDescriptorTableArr->mSamplerHandle != D3D12_DESCRIPTOR_ID_NONE)
        {
            pDxCmdList->SetComputeRootDescriptorTable(
                pDescriptorTableArr->mSamplerRootIndex,
                descriptor_id_to_gpu_handle(
                    pCmd->pBoundHeaps[1],
                    pDescriptorTableArr->mSamplerHandle + tableIndex * pDescriptorTableArr->mSamplerStride));
        }
    }
}

void REI_cmdBindPushConstants(
    REI_Cmd* pCmd, REI_RootSignature* pRootSignature, REI_ShaderStage stages, uint32_t offset, uint32_t size,
    const void* pConstants)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pConstants);
    REI_ASSERT(pRootSignature);
    // Set root signature if the current one differs from pRootSignature
    reset_root_signature(pCmd, pRootSignature->mPipelineType, pRootSignature->pDxRootSignature);

    if (pRootSignature->mPipelineType == REI_PIPELINE_TYPE_GRAPHICS)
    {
        uint32_t index;
        for (index = 0; index < REI_SHADER_STAGE_COUNT - 1; ++index)
        {
            if (stages & REI_ShaderStage(1 << index))
                break;
        }
        ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
            ->SetGraphicsRoot32BitConstants(
                pRootSignature->mPushConstantRootParamIndex[index], size / sizeof(uint32_t), pConstants, 0);
    }
    else
        ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
            ->SetComputeRoot32BitConstants(
                pRootSignature->mPushConstantRootParamIndex[REI_SHADER_STAGE_COUNT - 1], size / sizeof(uint32_t),
                pConstants, 0);
}

void REI_cmdBindIndexBuffer(REI_Cmd* p_cmd, REI_Buffer* p_buffer, uint64_t offset)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(p_buffer);
    REI_ASSERT(p_cmd->pDxCmdList);
    REI_ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != p_buffer->mDxGpuAddress);

    D3D12_INDEX_BUFFER_VIEW ibView = {};
    ibView.BufferLocation = p_buffer->mDxGpuAddress + offset;

    ibView.Format = (REI_INDEX_TYPE_UINT16 == p_buffer->desc.indexType) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
    ibView.SizeInBytes = (UINT)(p_buffer->mSize - offset);

    //bind given index buffer
    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)->IASetIndexBuffer(&ibView);
}

void REI_cmdBindVertexBuffer(REI_Cmd* p_cmd, uint32_t buffer_count, REI_Buffer** pp_buffers, uint64_t* pOffsets)
{
    REI_ASSERT(p_cmd);
    REI_ASSERT(0 != buffer_count);
    REI_ASSERT(pp_buffers);
    REI_ASSERT(p_cmd->pDxCmdList);
    //bind given vertex buffer

    DECLARE_ZERO(D3D12_VERTEX_BUFFER_VIEW, views[REI_MAX_VERTEX_ATTRIBS]);
    for (uint32_t i = 0; i < buffer_count; ++i)
    {
        REI_ASSERT(D3D12_GPU_VIRTUAL_ADDRESS_NULL != pp_buffers[i]->mDxGpuAddress);

        views[i].BufferLocation = (pp_buffers[i]->mDxGpuAddress + (pOffsets ? pOffsets[i] : 0));
        views[i].SizeInBytes = (UINT)(pp_buffers[i]->mSize - (pOffsets ? pOffsets[i] : 0));
        views[i].StrideInBytes = pp_buffers[i]->desc.vertexStride;
    }

    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)->IASetVertexBuffers(0, buffer_count, views);
}

void REI_cmdDraw(REI_Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex)
{
    REI_ASSERT(p_cmd);

    //draw given vertices
    REI_ASSERT(p_cmd->pDxCmdList);

    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)
        ->DrawInstanced((UINT)vertex_count, (UINT)1, (UINT)first_vertex, (UINT)0);
}

void REI_cmdDrawInstanced(
    REI_Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    REI_ASSERT(pCmd);

    //draw given vertices
    REI_ASSERT(pCmd->pDxCmdList);

    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
        ->DrawInstanced((UINT)vertexCount, (UINT)instanceCount, (UINT)firstVertex, (UINT)firstInstance);
}

void REI_cmdDrawIndexed(REI_Cmd* p_cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
    REI_ASSERT(p_cmd);

    //draw indexed mesh
    REI_ASSERT(p_cmd->pDxCmdList);

    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)
        ->DrawIndexedInstanced((UINT)index_count, (UINT)1, (UINT)first_index, (UINT)first_vertex, (UINT)0);
}

void REI_cmdDrawIndexedInstanced(
    REI_Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
    uint32_t firstInstance)
{
    REI_ASSERT(pCmd);

    //draw indexed mesh
    REI_ASSERT(pCmd->pDxCmdList);

    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
        ->DrawIndexedInstanced(
            (UINT)indexCount, (UINT)instanceCount, (UINT)firstIndex, (UINT)firstVertex, (UINT)firstInstance);
}

void REI_cmdDispatch(REI_Cmd* p_cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z)
{
    REI_ASSERT(p_cmd);

    //dispatch given command
    REI_ASSERT(p_cmd->pDxCmdList != NULL);

    ((ID3D12GraphicsCommandList*)p_cmd->pDxCmdList)->Dispatch(group_count_x, group_count_y, group_count_z);
}

void REI_cmdResolveTexture(
    REI_Cmd* pCmd, REI_Texture* pDstTexture, REI_Texture* pSrcTexture, REI_ResolveDesc* pResolveDesc)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pCmd->pDxCmdList);
    REI_ASSERT(pDstTexture);
    REI_ASSERT(pDstTexture->pDxResource);
    REI_ASSERT(pSrcTexture);
    REI_ASSERT(pSrcTexture->pDxResource);
    REI_ASSERT(pCmd->mType == REI_CMD_POOL_DIRECT);

    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
        ->ResolveSubresource(
            pDstTexture->pDxResource, 0, pSrcTexture->pDxResource, 0, REI_Format_ToDXGI_FORMAT(pDstTexture->mFormat));
}

void REI_cmdResourceBarrier(
    REI_Cmd* p_cmd, uint32_t buffer_barrier_count, REI_BufferBarrier* p_buffer_barriers, uint32_t texture_barrier_count,
    REI_TextureBarrier* p_texture_barriers)
{
#if REI_PLATFORM_WINDOWS
    if (p_cmd->mType == REI_CMD_POOL_COPY)
        return;
#endif
    D3D12_RESOURCE_BARRIER* barriers = (D3D12_RESOURCE_BARRIER*)alloca(
        size_t(buffer_barrier_count + texture_barrier_count) * sizeof(D3D12_RESOURCE_BARRIER));
    uint32_t transitionCount = 0;

    for (uint32_t i = 0; i < buffer_barrier_count; ++i)
    {
        REI_BufferBarrier*      pTransBarrier = &p_buffer_barriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        REI_Buffer*             pBuffer = pTransBarrier->pBuffer;

        // Only transition GPU visible resources.
        // Note: General CPU_TO_GPU resources have to stay in generic read state. They are created in upload heap.
        // There is one corner case: CPU_TO_GPU resources with UAV usage can have state transition. And they are created in custom heap.
        if (pBuffer->mMemoryUsage == REI_RESOURCE_MEMORY_USAGE_GPU_ONLY ||
            pBuffer->mMemoryUsage == REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU ||
            (pBuffer->mMemoryUsage == REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU &&
             (pBuffer->mDescriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER)))
        {
            //if (!(pBuffer->mCurrentState & pTransBarrier->mNewState) && pBuffer->mCurrentState != pTransBarrier->mNewState)
            if (REI_RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->startState &&
                REI_RESOURCE_STATE_UNORDERED_ACCESS == pTransBarrier->endState)
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                pBarrier->UAV.pResource = pBuffer->pDxResource;
                ++transitionCount;
            }
            else
            {
                pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;    // Split barriers are not implemented
                pBarrier->Transition.pResource = pBuffer->pDxResource;
                pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTransBarrier->startState);
                pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTransBarrier->endState);

                ++transitionCount;
            }
        }
    }

    for (uint32_t i = 0; i < texture_barrier_count; ++i)
    {
        REI_TextureBarrier*     pTrans = &p_texture_barriers[i];
        D3D12_RESOURCE_BARRIER* pBarrier = &barriers[transitionCount];
        REI_Texture*            pTexture = pTrans->pTexture;

        if (REI_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->startState &&
            REI_RESOURCE_STATE_UNORDERED_ACCESS == pTrans->endState)
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            pBarrier->UAV.pResource = pTexture->pDxResource;
            ++transitionCount;
        }
        else
        {
            pBarrier->Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            pBarrier->Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

            pBarrier->Transition.pResource = pTexture->pDxResource;
            pBarrier->Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

            pBarrier->Transition.StateBefore = util_to_dx12_resource_state(pTrans->startState);
            pBarrier->Transition.StateAfter = util_to_dx12_resource_state(pTrans->endState);

            ++transitionCount;
        }
    }

    if (transitionCount)
    {
        d3d12_platform_submit_resource_barriers(p_cmd, transitionCount, barriers);
    }
}

void REI_cmdExecuteIndirect(
    REI_Cmd* pCmd, REI_CommandSignature* pCommandSignature, uint32_t maxCommandCount, REI_Buffer* pIndirectBuffer,
    uint64_t bufferOffset, REI_Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    REI_ASSERT(pCommandSignature);
    REI_ASSERT(pIndirectBuffer);

    if (!pCounterBuffer)
        ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
            ->ExecuteIndirect(
                pCommandSignature->pDxHandle, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset, NULL, 0);
    else
        ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
            ->ExecuteIndirect(
                pCommandSignature->pDxHandle, maxCommandCount, pIndirectBuffer->pDxResource, bufferOffset,
                pCounterBuffer->pDxResource, counterBufferOffset);
}

void REI_cmdResetQueryPool(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    // No op
}

void REI_cmdBeginQuery(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t index)
{
    D3D12_QUERY_TYPE           type = pQueryPool->mType;
    ID3D12GraphicsCommandList* d3dCmd = (ID3D12GraphicsCommandList*)pCmd->pDxCmdList;

    switch (type)
    {
        case D3D12_QUERY_TYPE_OCCLUSION:
        case D3D12_QUERY_TYPE_BINARY_OCCLUSION: d3dCmd->BeginQuery(pQueryPool->pDxQueryHeap, type, index); break;

        case D3D12_QUERY_TYPE_TIMESTAMP:
            REI_ASSERT(false && "Illegal call to BeginQuery for QUERY_TYPE_TIMESTAMP");
            break;

        //case D3D12_QUERY_TYPE_PIPELINE_STATISTICS: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3: break;
        default: REI_ASSERT(false, "Specified QUERY_TYPE is not implemented"); break;
    }
}

void REI_cmdEndQuery(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t index)
{
    D3D12_QUERY_TYPE           type = pQueryPool->mType;
    ID3D12GraphicsCommandList* d3dCmd = (ID3D12GraphicsCommandList*)pCmd->pDxCmdList;

    switch (type)
    {
        case D3D12_QUERY_TYPE_OCCLUSION:
        case D3D12_QUERY_TYPE_BINARY_OCCLUSION: d3dCmd->EndQuery(pQueryPool->pDxQueryHeap, type, index); break;

        case D3D12_QUERY_TYPE_TIMESTAMP: d3dCmd->EndQuery(pQueryPool->pDxQueryHeap, type, index); break;

        //case D3D12_QUERY_TYPE_PIPELINE_STATISTICS: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM1: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM2: break;
        //case D3D12_QUERY_TYPE_SO_STATISTICS_STREAM3: break;
        default: REI_ASSERT(false, "Specified QUERY_TYPE is not implemented"); break;
    }
}

void REI_cmdResolveQuery(
    REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t bufferOffset, REI_QueryPool* pQueryPool, uint32_t startQuery,
    uint32_t queryCount)
{
    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
        ->ResolveQueryData(
            pQueryPool->pDxQueryHeap, pQueryPool->mType, startQuery, queryCount, pBuffer->pDxResource, startQuery * 8);
}

void REI_cmdAddDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if REI_ENABLE_PIX
    //color is in B8G8R8X8 format where X is padding
    PIXSetMarker(
        (ID3D12GraphicsCommandList*)pCmd->pDxCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)),
        pName);
#endif
#if defined(ENABLE_NSIGHT_AFTERMATH)
    SetAftermathMarker(&pCmd->pRenderer->mAftermathTracker, pCmd->pDxCmdList, pName);
#endif
}

void REI_getSwapchainTextures(REI_Swapchain* pSwapchain, uint32_t* count, REI_Texture** ppTextures)
{
    if (!count)
        return;

    uint32_t copyCount = REI_min(*count, pSwapchain->mImageCount);

    if (ppTextures)
    {
        REI_vector<REI_Texture*> ppSwapchainTextures{ copyCount, REI_allocator<REI_Texture>(*pSwapchain->pAllocator) };

        for (uint32_t i = 0; i < copyCount; ++i)
        {
            ppSwapchainTextures[i] = pSwapchain->ppRenderTargets[i];
        }
        memcpy(ppTextures, ppSwapchainTextures.data(), sizeof(REI_Texture*) * copyCount);
    }
    *count = pSwapchain->mImageCount;
}

REI_Format REI_getRecommendedSwapchainFormat(bool hintHDR) { return REI_FMT_B8G8R8A8_UNORM; }