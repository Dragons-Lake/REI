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

#pragma once
#include "Renderer.h"
#include <d3d12.h>
#if REI_PLATFORM_WINDOWS
#include <dxgi1_6.h>
#endif
//WinPixEventRuntime should be installed using the NuGet package for the Windows platform
#define REI_ENABLE_PIX 0

#if REI_ENABLE_PIX
#    if defined(_WIN64) || defined(REI_PLATFORM_XBOX)
#        include <pix3.h>
#    else
#        error PIX on Windows does not support directly analyzing x86 applications
#    endif
#endif

// Forward declare memory allocator classes
namespace D3D12MA
{
class Allocator;
class Allocation;
}    // namespace D3D12MA

struct IDxcBlobEncoding;
typedef int32_t DxDescriptorID;

typedef struct REI_RendererDescD3D12
{
    REI_RendererDesc  desc;
    D3D_FEATURE_LEVEL mDxFeatureLevel;
} REI_RendererDescD3D12;

typedef struct REI_GpuDesc
{
    IDXGIAdapter*                     pGpu = NULL;
    D3D_FEATURE_LEVEL                 mMaxSupportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
    D3D12_FEATURE_DATA_D3D12_OPTIONS  mFeatureDataOptions = {};
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 mFeatureDataOptions1 = {};
    SIZE_T                            mDedicatedVideoMemory = 0;
    char                              mVendorId[REI_MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char                              mDeviceId[REI_MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char                              mRevisionId[REI_MAX_GPU_VENDOR_STRING_LENGTH] = {};
    char                              mName[REI_MAX_GPU_VENDOR_STRING_LENGTH] = {};
} REI_GpuDesc;

/************************************************************************/
// Descriptor Heap Defines
/************************************************************************/
typedef struct DescriptorHeapProperties
{
    uint32_t                    mMaxDescriptors;
    D3D12_DESCRIPTOR_HEAP_FLAGS mFlags;
} DescriptorHeapProperties;

struct DescriptorHeap;

typedef struct REI_Renderer
{
    // API specific descriptor heap and memory allocator
    struct DescriptorHeap**    pCPUDescriptorHeaps;
    struct DescriptorHeap*     pCbvSrvUavHeaps;
    struct DescriptorHeap*     pSamplerHeaps;
    REI_AllocatorCallbacks     allocator;
    REI_LogPtr                 pLog;
    class D3D12MA::Allocator*  pResourceAllocator;
    IDXGIAdapter*              pDxActiveGPU;
    ID3D12Device*              pDxDevice;
    char*                      pName;
    REI_DeviceProperties*      pActiveGpuSettings;
    uint32_t                   mShaderTarget;
    uint32_t                   mEnableGpuBasedValidation;
    D3D_ROOT_SIGNATURE_VERSION mHighestRootSignatureVersion;
    D3D12_SHADER_CACHE_SUPPORT_FLAGS mShaderCacheFlags;
    // Functions points for functions that need to be loaded
    PFN_D3D12_CREATE_ROOT_SIGNATURE_DESERIALIZER           fnD3D12CreateRootSignatureDeserializer = NULL;
    PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE           fnD3D12SerializeVersionedRootSignature = NULL;
    PFN_D3D12_CREATE_VERSIONED_ROOT_SIGNATURE_DESERIALIZER fnD3D12CreateVersionedRootSignatureDeserializer = NULL;
} REI_Renderer;

typedef struct REI_Queue
{
    ID3D12CommandQueue* pDxQueue;
    REI_Fence*          pFence;
    REI_Renderer*       pRenderer;
    uint32_t            mType;
} REI_Queue;

typedef struct REI_CmdPool
{
    ID3D12CommandAllocator* pDxCmdAlloc;
    REI_Queue*              pQueue;
} REI_CmdPool;

typedef struct REI_Cmd
{
    ID3D12CommandList* pDxCmdList;

    // Cached in beginCmd to avoid fetching them during rendering
    struct DescriptorHeap*      pBoundHeaps[2];
    D3D12_GPU_DESCRIPTOR_HANDLE mBoundHeapStartHandles[2];

    // Command buffer state
    const ID3D12RootSignature* pBoundRootSignature;
    uint32_t                   mType;
    REI_CmdPool*               pCmdPool;

    REI_Renderer* pRenderer;
    REI_Queue*    pQueue;
} REI_Cmd;

typedef struct REI_CommandSignature
{
    ID3D12CommandSignature*  pDxHandle;
    REI_IndirectArgumentType mDrawType;
    uint32_t                 mStride;
} REI_CommandSignature;

typedef struct REI_Buffer
{
    /// CPU address of the mapped buffer (applicable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
    void* pCpuMappedAddress;
    /// GPU Address - Cache to avoid calls to ID3D12Resource::GetGpuVirtualAddress
    D3D12_GPU_VIRTUAL_ADDRESS mDxGpuAddress;
    REI_BufferDesc            desc;
    /// Descriptor handle of the CBV in a CPU visible descriptor heap (applicable to BUFFER_USAGE_UNIFORM)
    DxDescriptorID mDescriptors;
    /// Offset from mDxDescriptors for srv descriptor handle
    uint8_t mSrvDescriptorOffset;
    /// Offset from mDxDescriptors for uav descriptor handle
    uint8_t mUavDescriptorOffset;
    /// Native handle of the underlying resource
    ID3D12Resource* pDxResource;
    /// Contains resource allocation info such as parent heap, offset in heap
    D3D12MA::Allocation* pDxAllocation;

    uint32_t mSize;
    uint64_t mMemoryUsage;
} REI_Buffer;

typedef struct REI_Texture
{
    /// Descriptor handle of the SRV in a CPU visible descriptor heap (applicable to TEXTURE_USAGE_SAMPLED_IMAGE)
    DxDescriptorID mDescriptors;
    //Decriptor handle of the RTV in a CPU visible descriptor heap
    DxDescriptorID mRTVDescriptors;
    //Decriptor handle of the DSV in a CPU visible descriptor heap
    DxDescriptorID mDSVDescriptors;
    /// Native handle of the underlying resource
    ID3D12Resource* pDxResource;
    /// Contains resource allocation info such as parent heap, offset in heap
    D3D12MA::Allocation* pDxAllocation;
    uint32_t             m_SRV_UAV_HandleCount;
    uint32_t             m_RTV_DSV_HandleCount;
    uint32_t             mUavStartIndex;
    /// Current state of the buffer
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mDepth;
    uint32_t mMipLevels;
    uint32_t mArraySizeMinusOne;
    uint32_t mFormat;

    uint32_t mSampleCount;
    uint32_t mUav;
    /// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
    uint32_t mOwnsImage;
} REI_Texture;

typedef struct REI_Sampler
{
    /// Description for creating the Sampler descriptor for this sampler
    D3D12_SAMPLER_DESC mDesc;
    /// Descriptor handle of the Sampler in a CPU visible descriptor heap
    DxDescriptorID mDescriptor;
} REI_Sampler;

typedef struct REI_DescriptorTableArray
{
    /// Start handle to cbv srv uav descriptor table
    DxDescriptorID mCbvSrvUavHandle;
    /// Start handle to sampler descriptor table
    DxDescriptorID mSamplerHandle;
    /// Stride of the cbv srv uav descriptor table (number of descriptors * descriptor size)
    uint32_t mCbvSrvUavStride;
    /// Stride of the sampler descriptor table (number of descriptors * descriptor size)
    uint32_t                 mSamplerStride;
    const REI_RootSignature* pRootSignature;
    uint32_t                 maxTables;
    uint32_t                 slot;
    uint32_t                 mCbvSrvUavRootIndex;
    uint32_t                 mSamplerRootIndex;
    uint32_t                 mPipelineType;
} REI_DescriptorTableArray;

typedef struct REI_Shader
{
    REI_ShaderStage stage;
    void*           pShaderBytecode;
    uint32_t        bytecodeSize;
} REI_Shader;

typedef struct REI_PipelineCache
{
#if REI_PLATFORM_WINDOWS
    ID3D12PipelineLibrary* pLibrary;
    void*                  pData;
#endif
} REI_PipelineCache;

typedef struct REI_Pipeline
{
    ID3D12PipelineState*   pDxPipelineState;
    ID3D12RootSignature*   pRootSignature;
    REI_PipelineType       mType;
    D3D_PRIMITIVE_TOPOLOGY mDxPrimitiveTopology;
} REI_Pipeline;

typedef REI_unordered_map<REI_string, uint32_t> REI_DescriptorIndexMap;

typedef struct REI_RootSignature
{
    /// Graphics or Compute
    REI_PipelineType mPipelineType;

    ID3D12RootSignature* pDxRootSignature;
    uint8_t              mDxViewDescriptorTableRootIndices[REI_DESCRIPTOR_TABLE_SLOT_COUNT];
    uint8_t              mDxSamplerDescriptorTableRootIndices[REI_DESCRIPTOR_TABLE_SLOT_COUNT];
    uint32_t             mDxCumulativeViewDescriptorCounts[REI_DESCRIPTOR_TABLE_SLOT_COUNT];
    uint32_t             mDxCumulativeSamplerDescriptorCounts[REI_DESCRIPTOR_TABLE_SLOT_COUNT];
    uint32_t             mPushConstantRootParamIndex[REI_SHADER_STAGE_COUNT];
} REI_RootSignature;

typedef struct REI_QueryPool
{
    ID3D12QueryHeap* pDxQueryHeap;
    D3D12_QUERY_TYPE mType;
    uint32_t         mCount;
} REI_QueryPool;

typedef struct REI_Fence
{
    ID3D12Fence* pDxFence;
    HANDLE       pDxWaitIdleFenceEvent;
    uint64_t     mFenceValue;
} REI_Fence;

typedef struct REI_Semaphore
{
    ID3D12Fence* pDxFence;
    HANDLE       pDxWaitIdleFenceEvent;
    uint64_t     mFenceValue;
} REI_Semaphore;

typedef struct REI_Swapchain
{
    const REI_AllocatorCallbacks* pAllocator;
    /// Render targets created from the swapchain back buffers
    REI_Texture** ppRenderTargets;
    /// Sync interval to specify how interval for vsync
    uint32_t mDxSyncInterval;
    uint32_t mImageCount;
    uint32_t mEnableVsync;
    uint32_t mFlags;
} REI_Swapchain;

enum
{
    REI_TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET = 0x200
};

void REI_initRendererD3D12(const REI_RendererDescD3D12* pDescD3D12, REI_Renderer** ppRenderer);