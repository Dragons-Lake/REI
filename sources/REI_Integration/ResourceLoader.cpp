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

#include "ResourceLoader.h"

#include "REI/Common.h"
#include "REI/Thread.h"

struct REI_RL_MappedMemoryRange
{
    uint8_t* pData;
    uint64_t offset;
};

struct REI_RL_ResourceSet
{
    REI_Fence*   pFence;
    REI_CmdPool* pCmdPool;
    REI_Cmd*     pCmd;
    REI_Buffer*  pBuffer;
    void*        pMappedAddress;
};

enum
{
    DEFAULT_BUFFER_SIZE = 16ull << 20,
    DEFAULT_BUFFER_COUNT = 2u,
    DEFAULT_TIMESLICE_MS = 4u,
    MAX_BUFFER_COUNT = 8u,
};

typedef enum REI_RL_UpdateRequestType
{
    REI_RL_UPDATE_REQUEST_UPDATE_BUFFER,
    REI_RL_UPDATE_REQUEST_UPDATE_TEXTURE,
    REI_RL_UPDATE_REQUEST_INVALID,
} REI_RL_UpdateRequestType;

struct REI_RL_UpdateRequest
{
    REI_RL_UpdateRequest(): type(REI_RL_UPDATE_REQUEST_INVALID) {}
    REI_RL_UpdateRequest(REI_RL_BufferUpdateDesc& buffer):
        type(REI_RL_UPDATE_REQUEST_UPDATE_BUFFER), bufUpdateDesc(buffer)
    {
    }
    REI_RL_UpdateRequest(REI_RL_TextureUpdateDesc& texture):
        type(REI_RL_UPDATE_REQUEST_UPDATE_TEXTURE), texUpdateDesc(texture)
    {
    }
    REI_RL_UpdateRequestType type;
    union
    {
        REI_RL_BufferUpdateDesc  bufUpdateDesc;
        REI_RL_TextureUpdateDesc texUpdateDesc;
    };
};

struct REI_RL_State
{
    REI_RL_State(const REI_AllocatorCallbacks& inAllocator):
        allocator(inAllocator), requestQueue(REI_allocator<REI_RL_UpdateRequest>(allocator))
    {
    }


    REI_Renderer*             pRenderer;
    REI_AllocatorCallbacks    allocator;
    REI_RL_ResourceLoaderDesc desc;

    volatile int run;
    ThreadDesc   threadDesc;
    ThreadHandle thread;

    Mutex                            queueMutex;
    ConditionVariable                queueCond;
    Mutex                            tokenMutex;
    ConditionVariable                tokenCond;
    REI_deque<REI_RL_UpdateRequest>  requestQueue;

    REI_atomicptr_t requestsCompleted;
    REI_atomicptr_t requestsSubmitted;

    REI_Queue*          pQueue;
    REI_RL_ResourceSet* resourceSets;
    uint64_t            uniformBufferAlignment;
    uint64_t            allocatedSpace;
    REI_Extent3D        uploadGranularity;
    uint32_t            uploadBufferTextureAlignment;
    uint32_t            uploadBufferTextureRowAlignment;
};

/// Return memory from pre-allocated staging buffer or create a temporary buffer if the streamer ran out of memory
static REI_RL_MappedMemoryRange REI_RL_allocateStagingMemory(
    REI_RL_State* pRMState, size_t activeSet, uint64_t memoryRequirement, uint32_t alignment)
{
    uint64_t offset = pRMState->allocatedSpace;
    if (alignment != 0)
    {
        offset = REI_align_up<uint64_t>(offset, alignment);
    }

    REI_RL_ResourceSet* pResourceSet = &pRMState->resourceSets[activeSet];
    uint64_t            size = pRMState->desc.bufferSize;
    bool                memoryAvailable = (offset < size) && (memoryRequirement <= size - offset);
    if (memoryAvailable)
    {
        uint8_t* pDstData = (uint8_t*)pResourceSet->pMappedAddress + offset;
        pRMState->allocatedSpace = offset + memoryRequirement;
        return { pDstData, offset };
    }

    return { nullptr, 0 };
}

struct uint3
{
    uint32_t x, y, z;
};

static inline uint3 operator+(const uint3& a, const uint3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
static inline uint3 operator-(const uint3& a, const uint3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
static inline uint3 operator*(const uint3& a, const uint3& b) { return { a.x * b.x, a.y * b.y, a.z * b.z }; }
static inline uint3 operator/(const uint3& a, const uint3& b) { return { a.x / b.x, a.y / b.y, a.z / b.z }; }

template<>
inline uint3 REI_min(uint3 a, uint3 b)
{
    return { REI_min(a.x, b.x), REI_min(a.y, b.y), REI_min(a.z, b.z) };
}

struct REI_RL_UpdateState
{
    REI_RL_UpdateState(): REI_RL_UpdateState(REI_RL_UpdateRequest()) {}
    REI_RL_UpdateState(const REI_RL_UpdateRequest& request): request(request), offset({ 0, 0, 0 }), size(0) {}

    REI_RL_UpdateRequest request;
    uint3                offset;
    uint64_t             size;
};

static inline uint32_t REI_RL_SplitBitsWith0(uint32_t x)
{
    x &= 0x0000ffff;
    x = (x ^ (x << 8)) & 0x00ff00ff;
    x = (x ^ (x << 4)) & 0x0f0f0f0f;
    x = (x ^ (x << 2)) & 0x33333333;
    x = (x ^ (x << 1)) & 0x55555555;
    return x;
}

static inline uint32_t REI_RL_EncodeMorton(uint32_t x, uint32_t y)
{
    return (REI_RL_SplitBitsWith0(y) << 1) + REI_RL_SplitBitsWith0(x);
}

// TODO: test and fix
static void REI_RL_copyUploadRectZCurve(
    uint8_t* pDstData, uint8_t* pSrcData, REI_Region3D uploadRegion, uint3 srcPitches, uint3 dstPitches)
{
    uint32_t offset = 0;
    for (uint32_t z = uploadRegion.z; z < uploadRegion.z + uploadRegion.d; ++z)
    {
        for (uint32_t y = uploadRegion.y; y < uploadRegion.y + uploadRegion.h; ++y)
        {
            for (uint32_t x = uploadRegion.x; x < uploadRegion.x + uploadRegion.w; ++x)
            {
                uint32_t blockOffset = REI_RL_EncodeMorton(y, x);
                memcpy(pDstData + offset, pSrcData + blockOffset * srcPitches.x, srcPitches.x);
                offset += dstPitches.x;
            }
            offset = REI_align_up(offset, dstPitches.y);
        }
        pSrcData += srcPitches.z;
    }
}

static void REI_RL_copyUploadRect(
    uint8_t* pDstData, uint8_t* pSrcData, REI_Region3D uploadRegion, uint3 srcPitches, uint3 dstPitches)
{
    uint32_t srcOffset = uploadRegion.z * srcPitches.z + uploadRegion.y * srcPitches.y + uploadRegion.x * srcPitches.x;
    uint32_t numSlices = uploadRegion.h * uploadRegion.d;
    uint32_t pitch = uploadRegion.w * srcPitches.x;
    pSrcData += srcOffset;
    for (uint32_t s = 0; s < numSlices; ++s)
    {
        memcpy(pDstData, pSrcData, pitch);
        pSrcData += srcPitches.y;
        pDstData += dstPitches.y;
    }
}

static uint3 REI_RL_calculateUploadRect(uint64_t mem, uint3 pitches, uint3 offset, uint3 extent, uint3 granularity)
{
    uint3 scaler{ granularity.x * granularity.y * granularity.z, granularity.y * granularity.z, granularity.z };
    pitches = pitches * scaler;
    uint3    leftover = extent - offset;
    uint32_t numSlices = (uint32_t)REI_min<uint64_t>((mem / pitches.z) * granularity.z, leftover.z);
    // advance by slices
    if (offset.x == 0 && offset.y == 0 && numSlices > 0)
    {
        return { extent.x, extent.y, numSlices };
    }

    // advance by strides
    numSlices = REI_min(leftover.z, granularity.z);
    uint32_t numStrides = (uint32_t)REI_min<uint64_t>((mem / pitches.y) * granularity.y, leftover.y);
    if (offset.x == 0 && numStrides > 0)
    {
        return { extent.x, numStrides, numSlices };
    }

    numStrides = REI_min(leftover.y, granularity.y);
    // advance by blocks
    uint32_t numBlocks = (uint32_t)REI_min<uint64_t>((mem / pitches.x) * granularity.x, leftover.x);
    return { numBlocks, numStrides, numSlices };
}

static REI_Region3D REI_RL_calculateUploadRegion(uint3 offset, uint3 extent, uint3 uploadBlock, uint3 pxImageDim)
{
    uint3 regionOffset = offset * uploadBlock;
    uint3 regionSize = REI_min<uint3>(extent * uploadBlock, pxImageDim);
    return { regionOffset.x, regionOffset.y, regionOffset.z, regionSize.x, regionSize.y, regionSize.z };
}

static bool REI_RL_isLinearLayout(REI_Format format)
{
    switch (format)
    {
        case REI_FMT_PVRTC1_2BPP_UNORM:
        case REI_FMT_PVRTC1_2BPP_SRGB:
        case REI_FMT_PVRTC1_4BPP_UNORM:
        case REI_FMT_PVRTC1_4BPP_SRGB: return false;
        default: return true;
    }
}

static uint32_t REI_RL_util_bitSizeOfBlock(REI_Format fmt)
{
    switch (fmt)
    {
        case REI_FMT_UNDEFINED: return 0;
        case REI_FMT_R1_UNORM: return 8;
        case REI_FMT_R2_UNORM: return 8;
        case REI_FMT_R4_UNORM: return 8;
        case REI_FMT_R4G4_UNORM: return 8;
        case REI_FMT_G4R4_UNORM: return 8;
        case REI_FMT_A8_UNORM: return 8;
        case REI_FMT_R8_UNORM: return 8;
        case REI_FMT_R8_SNORM: return 8;
        case REI_FMT_R8_UINT: return 8;
        case REI_FMT_R8_SINT: return 8;
        case REI_FMT_R8_SRGB: return 8;
        case REI_FMT_B2G3R3_UNORM: return 8;
        case REI_FMT_R4G4B4A4_UNORM: return 16;
        case REI_FMT_R4G4B4X4_UNORM: return 16;
        case REI_FMT_B4G4R4A4_UNORM: return 16;
        case REI_FMT_B4G4R4X4_UNORM: return 16;
        case REI_FMT_A4R4G4B4_UNORM: return 16;
        case REI_FMT_X4R4G4B4_UNORM: return 16;
        case REI_FMT_A4B4G4R4_UNORM: return 16;
        case REI_FMT_X4B4G4R4_UNORM: return 16;
        case REI_FMT_R5G6B5_UNORM: return 16;
        case REI_FMT_B5G6R5_UNORM: return 16;
        case REI_FMT_R5G5B5A1_UNORM: return 16;
        case REI_FMT_B5G5R5A1_UNORM: return 16;
        case REI_FMT_A1B5G5R5_UNORM: return 16;
        case REI_FMT_A1R5G5B5_UNORM: return 16;
        case REI_FMT_R5G5B5X1_UNORM: return 16;
        case REI_FMT_B5G5R5X1_UNORM: return 16;
        case REI_FMT_X1R5G5B5_UNORM: return 16;
        case REI_FMT_X1B5G5R5_UNORM: return 16;
        case REI_FMT_B2G3R3A8_UNORM: return 16;
        case REI_FMT_R8G8_UNORM: return 16;
        case REI_FMT_R8G8_SNORM: return 16;
        case REI_FMT_G8R8_UNORM: return 16;
        case REI_FMT_G8R8_SNORM: return 16;
        case REI_FMT_R8G8_UINT: return 16;
        case REI_FMT_R8G8_SINT: return 16;
        case REI_FMT_R8G8_SRGB: return 16;
        case REI_FMT_R16_UNORM: return 16;
        case REI_FMT_R16_SNORM: return 16;
        case REI_FMT_R16_UINT: return 16;
        case REI_FMT_R16_SINT: return 16;
        case REI_FMT_R16_SFLOAT: return 16;
        case REI_FMT_R16_SBFLOAT: return 16;
        case REI_FMT_R8G8B8_UNORM: return 24;
        case REI_FMT_R8G8B8_SNORM: return 24;
        case REI_FMT_R8G8B8_UINT: return 24;
        case REI_FMT_R8G8B8_SINT: return 24;
        case REI_FMT_R8G8B8_SRGB: return 24;
        case REI_FMT_B8G8R8_UNORM: return 24;
        case REI_FMT_B8G8R8_SNORM: return 24;
        case REI_FMT_B8G8R8_UINT: return 24;
        case REI_FMT_B8G8R8_SINT: return 24;
        case REI_FMT_B8G8R8_SRGB: return 24;
        case REI_FMT_R16G16B16_UNORM: return 48;
        case REI_FMT_R16G16B16_SNORM: return 48;
        case REI_FMT_R16G16B16_UINT: return 48;
        case REI_FMT_R16G16B16_SINT: return 48;
        case REI_FMT_R16G16B16_SFLOAT: return 48;
        case REI_FMT_R16G16B16_SBFLOAT: return 48;
        case REI_FMT_R16G16B16A16_UNORM: return 64;
        case REI_FMT_R16G16B16A16_SNORM: return 64;
        case REI_FMT_R16G16B16A16_UINT: return 64;
        case REI_FMT_R16G16B16A16_SINT: return 64;
        case REI_FMT_R16G16B16A16_SFLOAT: return 64;
        case REI_FMT_R16G16B16A16_SBFLOAT: return 64;
        case REI_FMT_R32G32_UINT: return 64;
        case REI_FMT_R32G32_SINT: return 64;
        case REI_FMT_R32G32_SFLOAT: return 64;
        case REI_FMT_R32G32B32_UINT: return 96;
        case REI_FMT_R32G32B32_SINT: return 96;
        case REI_FMT_R32G32B32_SFLOAT: return 96;
        case REI_FMT_R32G32B32A32_UINT: return 128;
        case REI_FMT_R32G32B32A32_SINT: return 128;
        case REI_FMT_R32G32B32A32_SFLOAT: return 128;
        case REI_FMT_R64_UINT: return 64;
        case REI_FMT_R64_SINT: return 64;
        case REI_FMT_R64_SFLOAT: return 64;
        case REI_FMT_R64G64_UINT: return 128;
        case REI_FMT_R64G64_SINT: return 128;
        case REI_FMT_R64G64_SFLOAT: return 128;
        case REI_FMT_R64G64B64_UINT: return 192;
        case REI_FMT_R64G64B64_SINT: return 192;
        case REI_FMT_R64G64B64_SFLOAT: return 192;
        case REI_FMT_R64G64B64A64_UINT: return 256;
        case REI_FMT_R64G64B64A64_SINT: return 256;
        case REI_FMT_R64G64B64A64_SFLOAT: return 256;
        case REI_FMT_D16_UNORM: return 16;
        case REI_FMT_S8_UINT: return 8;
        case REI_FMT_D32_SFLOAT_S8_UINT: return 64;
        case REI_FMT_DXBC1_RGB_UNORM: return 64;
        case REI_FMT_DXBC1_RGB_SRGB: return 64;
        case REI_FMT_DXBC1_RGBA_UNORM: return 64;
        case REI_FMT_DXBC1_RGBA_SRGB: return 64;
        case REI_FMT_DXBC2_UNORM: return 128;
        case REI_FMT_DXBC2_SRGB: return 128;
        case REI_FMT_DXBC3_UNORM: return 128;
        case REI_FMT_DXBC3_SRGB: return 128;
        case REI_FMT_DXBC4_UNORM: return 64;
        case REI_FMT_DXBC4_SNORM: return 64;
        case REI_FMT_DXBC5_UNORM: return 128;
        case REI_FMT_DXBC5_SNORM: return 128;
        case REI_FMT_DXBC6H_UFLOAT: return 128;
        case REI_FMT_DXBC6H_SFLOAT: return 128;
        case REI_FMT_DXBC7_UNORM: return 128;
        case REI_FMT_DXBC7_SRGB: return 128;
        case REI_FMT_PVRTC1_2BPP_UNORM: return 64;
        case REI_FMT_PVRTC1_4BPP_UNORM: return 64;
        case REI_FMT_PVRTC2_2BPP_UNORM: return 64;
        case REI_FMT_PVRTC2_4BPP_UNORM: return 64;
        case REI_FMT_PVRTC1_2BPP_SRGB: return 64;
        case REI_FMT_PVRTC1_4BPP_SRGB: return 64;
        case REI_FMT_PVRTC2_2BPP_SRGB: return 64;
        case REI_FMT_PVRTC2_4BPP_SRGB: return 64;
        case REI_FMT_ETC2_R8G8B8_UNORM: return 64;
        case REI_FMT_ETC2_R8G8B8_SRGB: return 64;
        case REI_FMT_ETC2_R8G8B8A1_UNORM: return 64;
        case REI_FMT_ETC2_R8G8B8A1_SRGB: return 64;
        case REI_FMT_ETC2_R8G8B8A8_UNORM: return 64;
        case REI_FMT_ETC2_R8G8B8A8_SRGB: return 64;
        case REI_FMT_ETC2_EAC_R11_UNORM: return 64;
        case REI_FMT_ETC2_EAC_R11_SNORM: return 64;
        case REI_FMT_ETC2_EAC_R11G11_UNORM: return 64;
        case REI_FMT_ETC2_EAC_R11G11_SNORM: return 64;
        case REI_FMT_ASTC_4x4_UNORM: return 128;
        case REI_FMT_ASTC_4x4_SRGB: return 128;
        case REI_FMT_ASTC_5x4_UNORM: return 128;
        case REI_FMT_ASTC_5x4_SRGB: return 128;
        case REI_FMT_ASTC_5x5_UNORM: return 128;
        case REI_FMT_ASTC_5x5_SRGB: return 128;
        case REI_FMT_ASTC_6x5_UNORM: return 128;
        case REI_FMT_ASTC_6x5_SRGB: return 128;
        case REI_FMT_ASTC_6x6_UNORM: return 128;
        case REI_FMT_ASTC_6x6_SRGB: return 128;
        case REI_FMT_ASTC_8x5_UNORM: return 128;
        case REI_FMT_ASTC_8x5_SRGB: return 128;
        case REI_FMT_ASTC_8x6_UNORM: return 128;
        case REI_FMT_ASTC_8x6_SRGB: return 128;
        case REI_FMT_ASTC_8x8_UNORM: return 128;
        case REI_FMT_ASTC_8x8_SRGB: return 128;
        case REI_FMT_ASTC_10x5_UNORM: return 128;
        case REI_FMT_ASTC_10x5_SRGB: return 128;
        case REI_FMT_ASTC_10x6_UNORM: return 128;
        case REI_FMT_ASTC_10x6_SRGB: return 128;
        case REI_FMT_ASTC_10x8_UNORM: return 128;
        case REI_FMT_ASTC_10x8_SRGB: return 128;
        case REI_FMT_ASTC_10x10_UNORM: return 128;
        case REI_FMT_ASTC_10x10_SRGB: return 128;
        case REI_FMT_ASTC_12x10_UNORM: return 128;
        case REI_FMT_ASTC_12x10_SRGB: return 128;
        case REI_FMT_ASTC_12x12_UNORM: return 128;
        case REI_FMT_ASTC_12x12_SRGB: return 128;
        case REI_FMT_CLUT_P4: return 8;
        case REI_FMT_CLUT_P4A4: return 8;
        case REI_FMT_CLUT_P8: return 8;
        case REI_FMT_CLUT_P8A8: return 16;
        default: return 32;
    }
}

static inline uint32_t REI_RL_util_widthOfBlock(REI_Format fmt)
{
    switch (fmt)
    {
        case REI_FMT_UNDEFINED: return 0;
        case REI_FMT_R1_UNORM: return 8;
        case REI_FMT_R2_UNORM: return 4;
        case REI_FMT_R4_UNORM: return 2;
        case REI_FMT_DXBC1_RGB_UNORM: return 4;
        case REI_FMT_DXBC1_RGB_SRGB: return 4;
        case REI_FMT_DXBC1_RGBA_UNORM: return 4;
        case REI_FMT_DXBC1_RGBA_SRGB: return 4;
        case REI_FMT_DXBC2_UNORM: return 4;
        case REI_FMT_DXBC2_SRGB: return 4;
        case REI_FMT_DXBC3_UNORM: return 4;
        case REI_FMT_DXBC3_SRGB: return 4;
        case REI_FMT_DXBC4_UNORM: return 4;
        case REI_FMT_DXBC4_SNORM: return 4;
        case REI_FMT_DXBC5_UNORM: return 4;
        case REI_FMT_DXBC5_SNORM: return 4;
        case REI_FMT_DXBC6H_UFLOAT: return 4;
        case REI_FMT_DXBC6H_SFLOAT: return 4;
        case REI_FMT_DXBC7_UNORM: return 4;
        case REI_FMT_DXBC7_SRGB: return 4;
        case REI_FMT_PVRTC1_2BPP_UNORM: return 8;
        case REI_FMT_PVRTC1_4BPP_UNORM: return 4;
        case REI_FMT_PVRTC2_2BPP_UNORM: return 8;
        case REI_FMT_PVRTC2_4BPP_UNORM: return 4;
        case REI_FMT_PVRTC1_2BPP_SRGB: return 8;
        case REI_FMT_PVRTC1_4BPP_SRGB: return 4;
        case REI_FMT_PVRTC2_2BPP_SRGB: return 8;
        case REI_FMT_PVRTC2_4BPP_SRGB: return 4;
        case REI_FMT_ETC2_R8G8B8_UNORM: return 4;
        case REI_FMT_ETC2_R8G8B8_SRGB: return 4;
        case REI_FMT_ETC2_R8G8B8A1_UNORM: return 4;
        case REI_FMT_ETC2_R8G8B8A1_SRGB: return 4;
        case REI_FMT_ETC2_R8G8B8A8_UNORM: return 4;
        case REI_FMT_ETC2_R8G8B8A8_SRGB: return 4;
        case REI_FMT_ETC2_EAC_R11_UNORM: return 4;
        case REI_FMT_ETC2_EAC_R11_SNORM: return 4;
        case REI_FMT_ETC2_EAC_R11G11_UNORM: return 4;
        case REI_FMT_ETC2_EAC_R11G11_SNORM: return 4;
        case REI_FMT_ASTC_4x4_UNORM: return 4;
        case REI_FMT_ASTC_4x4_SRGB: return 4;
        case REI_FMT_ASTC_5x4_UNORM: return 5;
        case REI_FMT_ASTC_5x4_SRGB: return 5;
        case REI_FMT_ASTC_5x5_UNORM: return 5;
        case REI_FMT_ASTC_5x5_SRGB: return 5;
        case REI_FMT_ASTC_6x5_UNORM: return 6;
        case REI_FMT_ASTC_6x5_SRGB: return 6;
        case REI_FMT_ASTC_6x6_UNORM: return 6;
        case REI_FMT_ASTC_6x6_SRGB: return 6;
        case REI_FMT_ASTC_8x5_UNORM: return 8;
        case REI_FMT_ASTC_8x5_SRGB: return 8;
        case REI_FMT_ASTC_8x6_UNORM: return 8;
        case REI_FMT_ASTC_8x6_SRGB: return 8;
        case REI_FMT_ASTC_8x8_UNORM: return 8;
        case REI_FMT_ASTC_8x8_SRGB: return 8;
        case REI_FMT_ASTC_10x5_UNORM: return 10;
        case REI_FMT_ASTC_10x5_SRGB: return 10;
        case REI_FMT_ASTC_10x6_UNORM: return 10;
        case REI_FMT_ASTC_10x6_SRGB: return 10;
        case REI_FMT_ASTC_10x8_UNORM: return 10;
        case REI_FMT_ASTC_10x8_SRGB: return 10;
        case REI_FMT_ASTC_10x10_UNORM: return 10;
        case REI_FMT_ASTC_10x10_SRGB: return 10;
        case REI_FMT_ASTC_12x10_UNORM: return 12;
        case REI_FMT_ASTC_12x10_SRGB: return 12;
        case REI_FMT_ASTC_12x12_UNORM: return 12;
        case REI_FMT_ASTC_12x12_SRGB: return 12;
        case REI_FMT_CLUT_P4: return 2;
        default: return 1;
    }
}

static inline uint32_t REI_RL_util_heightOfBlock(REI_Format fmt)
{
    switch (fmt)
    {
        case REI_FMT_UNDEFINED: return 0;
        case REI_FMT_DXBC1_RGB_UNORM: return 4;
        case REI_FMT_DXBC1_RGB_SRGB: return 4;
        case REI_FMT_DXBC1_RGBA_UNORM: return 4;
        case REI_FMT_DXBC1_RGBA_SRGB: return 4;
        case REI_FMT_DXBC2_UNORM: return 4;
        case REI_FMT_DXBC2_SRGB: return 4;
        case REI_FMT_DXBC3_UNORM: return 4;
        case REI_FMT_DXBC3_SRGB: return 4;
        case REI_FMT_DXBC4_UNORM: return 4;
        case REI_FMT_DXBC4_SNORM: return 4;
        case REI_FMT_DXBC5_UNORM: return 4;
        case REI_FMT_DXBC5_SNORM: return 4;
        case REI_FMT_DXBC6H_UFLOAT: return 4;
        case REI_FMT_DXBC6H_SFLOAT: return 4;
        case REI_FMT_DXBC7_UNORM: return 4;
        case REI_FMT_DXBC7_SRGB: return 4;
        case REI_FMT_PVRTC1_2BPP_UNORM: return 4;
        case REI_FMT_PVRTC1_4BPP_UNORM: return 4;
        case REI_FMT_PVRTC2_2BPP_UNORM: return 4;
        case REI_FMT_PVRTC2_4BPP_UNORM: return 4;
        case REI_FMT_PVRTC1_2BPP_SRGB: return 4;
        case REI_FMT_PVRTC1_4BPP_SRGB: return 4;
        case REI_FMT_PVRTC2_2BPP_SRGB: return 4;
        case REI_FMT_PVRTC2_4BPP_SRGB: return 4;
        case REI_FMT_ETC2_R8G8B8_UNORM: return 4;
        case REI_FMT_ETC2_R8G8B8_SRGB: return 4;
        case REI_FMT_ETC2_R8G8B8A1_UNORM: return 4;
        case REI_FMT_ETC2_R8G8B8A1_SRGB: return 4;
        case REI_FMT_ETC2_R8G8B8A8_UNORM: return 4;
        case REI_FMT_ETC2_R8G8B8A8_SRGB: return 4;
        case REI_FMT_ETC2_EAC_R11_UNORM: return 4;
        case REI_FMT_ETC2_EAC_R11_SNORM: return 4;
        case REI_FMT_ETC2_EAC_R11G11_UNORM: return 4;
        case REI_FMT_ETC2_EAC_R11G11_SNORM: return 4;
        case REI_FMT_ASTC_4x4_UNORM: return 4;
        case REI_FMT_ASTC_4x4_SRGB: return 4;
        case REI_FMT_ASTC_5x4_UNORM: return 4;
        case REI_FMT_ASTC_5x4_SRGB: return 4;
        case REI_FMT_ASTC_5x5_UNORM: return 5;
        case REI_FMT_ASTC_5x5_SRGB: return 5;
        case REI_FMT_ASTC_6x5_UNORM: return 5;
        case REI_FMT_ASTC_6x5_SRGB: return 5;
        case REI_FMT_ASTC_6x6_UNORM: return 6;
        case REI_FMT_ASTC_6x6_SRGB: return 6;
        case REI_FMT_ASTC_8x5_UNORM: return 5;
        case REI_FMT_ASTC_8x5_SRGB: return 5;
        case REI_FMT_ASTC_8x6_UNORM: return 6;
        case REI_FMT_ASTC_8x6_SRGB: return 6;
        case REI_FMT_ASTC_8x8_UNORM: return 8;
        case REI_FMT_ASTC_8x8_SRGB: return 8;
        case REI_FMT_ASTC_10x5_UNORM: return 5;
        case REI_FMT_ASTC_10x5_SRGB: return 5;
        case REI_FMT_ASTC_10x6_UNORM: return 6;
        case REI_FMT_ASTC_10x6_SRGB: return 6;
        case REI_FMT_ASTC_10x8_UNORM: return 8;
        case REI_FMT_ASTC_10x8_SRGB: return 8;
        case REI_FMT_ASTC_10x10_UNORM: return 10;
        case REI_FMT_ASTC_10x10_SRGB: return 10;
        case REI_FMT_ASTC_12x10_UNORM: return 10;
        case REI_FMT_ASTC_12x10_SRGB: return 10;
        case REI_FMT_ASTC_12x12_UNORM: return 12;
        case REI_FMT_ASTC_12x12_SRGB: return 12;
        default: return 1;
    }
}

static bool REI_RL_updateTexture(REI_RL_State* pRMState, size_t activeSet, REI_RL_UpdateState& pTextureUpdate)
{
    REI_RL_TextureUpdateDesc& texUpdateDesc = pTextureUpdate.request.texUpdateDesc;
    REI_Texture*              pTexture = texUpdateDesc.pTexture;

    REI_Cmd* pCmd = pRMState->resourceSets[activeSet].pCmd;

    REI_ASSERT(pTexture);

    uint32_t textureAlignment = pRMState->uploadBufferTextureAlignment;
    uint32_t textureRowAlignment = pRMState->uploadBufferTextureRowAlignment;

    uint3 uploadOffset = pTextureUpdate.offset;

    // Only need transition for vulkan and durango since resource will auto promote to copy dest on copy queue in PC dx12
    if ((uploadOffset.x == 0) && (uploadOffset.y == 0) && (uploadOffset.z == 0))
    {
        REI_TextureBarrier preCopyBarrier = { pTexture, REI_RESOURCE_STATE_UNDEFINED, REI_RESOURCE_STATE_COPY_DEST };
        REI_cmdResourceBarrier(pCmd, 0, NULL, 1, &preCopyBarrier);
    }
    REI_Extent3D uploadGran = pRMState->uploadGranularity;

    uint32_t blockSize;
    uint3    pxBlockDim;

    blockSize = REI_RL_util_bitSizeOfBlock(texUpdateDesc.format) / 8;
    pxBlockDim = { REI_RL_util_widthOfBlock(texUpdateDesc.format), REI_RL_util_heightOfBlock(texUpdateDesc.format),
                   1u };

    const uint32_t pxPerRow =
        REI_max<uint32_t>(REI_align_down(textureRowAlignment / blockSize, uploadGran.width), uploadGran.width);
    const uint3 queueGranularity = { pxPerRow, uploadGran.height, uploadGran.depth };
    const uint3 imageDim = { texUpdateDesc.width, texUpdateDesc.height, texUpdateDesc.depth };

    uint3    uploadExtent{ (imageDim + pxBlockDim - uint3{ 1u, 1u, 1u }) / pxBlockDim };
    uint3    granularity{ REI_min<uint3>(queueGranularity, uploadExtent) };
    uint32_t srcPitchY{ blockSize * uploadExtent.x };
    uint32_t dstPitchY{ REI_align_up(srcPitchY, textureRowAlignment) };
    uint3    srcPitches{ blockSize, srcPitchY, srcPitchY * uploadExtent.y };
    uint3    dstPitches{ blockSize, dstPitchY, dstPitchY * uploadExtent.y };

    REI_ASSERT(uploadOffset.x < uploadExtent.x || uploadOffset.y < uploadExtent.y || uploadOffset.z < uploadExtent.z);

    uint64_t spaceAvailable{ REI_align_down<uint64_t>(
        pRMState->desc.bufferSize - pRMState->allocatedSpace, textureRowAlignment) };
    uint3    uploadRectExtent{ REI_RL_calculateUploadRect(
        spaceAvailable, dstPitches, uploadOffset, uploadExtent, granularity) };
    uint32_t uploadPitchY{ REI_align_up(uploadRectExtent.x * dstPitches.x, textureRowAlignment) };
    uint3    uploadPitches{ blockSize, uploadPitchY, uploadPitchY * uploadRectExtent.y };

    REI_ASSERT(
        uploadOffset.x + uploadRectExtent.x <= uploadExtent.x ||
        uploadOffset.y + uploadRectExtent.y <= uploadExtent.y || uploadOffset.z + uploadRectExtent.z <= uploadExtent.z);

    if (uploadRectExtent.x == 0)
    {
        pTextureUpdate.offset = uploadOffset;
        return false;
    }

    REI_RL_MappedMemoryRange range =
        REI_RL_allocateStagingMemory(pRMState, activeSet, uploadRectExtent.z * uploadPitches.z, textureAlignment);
    // TODO: should not happed, resolve, simplify
    //REI_ASSERT(range.pData);
    if (!range.pData)
    {
        pTextureUpdate.offset = uploadOffset;
        return false;
    }

    REI_SubresourceDesc texData;
    texData.arrayLayer = texUpdateDesc.arrayLayer /*n * nSlices + k*/;
    texData.mipLevel = texUpdateDesc.mipLevel;
    texData.bufferOffset = range.offset;
    texData.rowPitch = uploadPitches.y;
    texData.slicePitch = uploadPitches.z;
    texData.region = REI_RL_calculateUploadRegion(uploadOffset, uploadRectExtent, pxBlockDim, imageDim);
    texData.region.x += texUpdateDesc.x;
    texData.region.y += texUpdateDesc.y;
    texData.region.z += texUpdateDesc.z;

    REI_Region3D uploadRegion{ uploadOffset.x,     uploadOffset.y,     uploadOffset.z,
                               uploadRectExtent.x, uploadRectExtent.y, uploadRectExtent.z };

    if (REI_RL_isLinearLayout(texUpdateDesc.format))
        REI_RL_copyUploadRect(range.pData, texUpdateDesc.pRawData, uploadRegion, srcPitches, uploadPitches);
    else
        REI_RL_copyUploadRectZCurve(range.pData, texUpdateDesc.pRawData, uploadRegion, srcPitches, uploadPitches);

    REI_cmdCopyBufferToTexture(pCmd, pTexture, pRMState->resourceSets[activeSet].pBuffer, &texData);

    uploadOffset.x += uploadRectExtent.x;
    uploadOffset.y += (uploadOffset.x < uploadExtent.x) ? 0 : uploadRectExtent.y;
    uploadOffset.z += (uploadOffset.y < uploadExtent.y) ? 0 : uploadRectExtent.z;

    uploadOffset.x = uploadOffset.x % uploadExtent.x;
    uploadOffset.y = uploadOffset.y % uploadExtent.y;
    uploadOffset.z = uploadOffset.z % uploadExtent.z;

    if (uploadOffset.x != 0 || uploadOffset.y != 0 || uploadOffset.z != 0)
    {
        pTextureUpdate.offset = uploadOffset;
        return false;
    }
    REI_ASSERT(uploadOffset.x == 0 && uploadOffset.y == 0 && uploadOffset.z == 0);

    // Only need transition for vulkan and durango since resource will decay to srv on graphics queue in PC dx12
    REI_TextureBarrier postCopyBarrier = { pTexture, REI_RESOURCE_STATE_COPY_DEST, texUpdateDesc.endState };
    REI_cmdResourceBarrier(pCmd, 0, NULL, 1, &postCopyBarrier);

    return true;
}

static bool REI_RL_updateBuffer(REI_RL_State* pRMState, size_t activeSet, REI_RL_UpdateState& pBufferUpdate)
{
    REI_RL_BufferUpdateDesc& bufUpdateDesc = pBufferUpdate.request.bufUpdateDesc;
    REI_Buffer*              pBuffer = bufUpdateDesc.pBuffer;

    const uint64_t bufferSize = bufUpdateDesc.size;
    uint64_t       spaceAvailable =
        REI_align_down<uint64_t>(pRMState->desc.bufferSize - pRMState->allocatedSpace, REI_RESOURCE_BUFFER_ALIGNMENT);

    if (spaceAvailable < REI_RESOURCE_BUFFER_ALIGNMENT)
        return false;

    uint64_t dataToCopy = REI_min(spaceAvailable, bufferSize - pBufferUpdate.size);

    REI_Cmd* pCmd = pRMState->resourceSets[activeSet].pCmd;

    REI_RL_MappedMemoryRange range =
        REI_RL_allocateStagingMemory(pRMState, activeSet, dataToCopy, REI_RESOURCE_BUFFER_ALIGNMENT);

    // TODO: should not happed, resolve, simplify
    //REI_ASSERT(range.pData);
    if (!range.pData)
        return false;

    void* pSrcBufferAddress = NULL;
    if (bufUpdateDesc.pData)
        pSrcBufferAddress = (uint8_t*)(bufUpdateDesc.pData) + (bufUpdateDesc.srcOffset + pBufferUpdate.size);

    if (pSrcBufferAddress)
        memcpy(range.pData, pSrcBufferAddress, (size_t)dataToCopy);
    else
        memset(range.pData, 0, (size_t)dataToCopy);

    REI_cmdCopyBuffer(
        pCmd, pBuffer, bufUpdateDesc.dstOffset, pRMState->resourceSets[activeSet].pBuffer, range.offset, dataToCopy);

    pBufferUpdate.size += dataToCopy;

    if (pBufferUpdate.size != bufferSize)
    {
        return false;
    }

    return true;
}

static void streamerThreadFunc(void* pThreadData)
{
    REI_RL_State* pRMState = (REI_RL_State*)pThreadData;
    REI_ASSERT(pRMState);

    bool requestCompleted = true;

    REI_RL_RequestId   lastRequestIdInSet[MAX_BUFFER_COUNT] = { 0 };
    uintptr_t          requestsProcessed = 0;
    size_t             activeSet = 0;
    REI_RL_UpdateState updateState;
    while (1)
    {
        pRMState->queueMutex.Acquire();
        while (pRMState->run && requestsProcessed == REI_atomicptr_load_relaxed(&pRMState->requestsCompleted) &&
               pRMState->requestQueue.empty())
        {
            pRMState->queueCond.Wait(pRMState->queueMutex);
        }
        pRMState->queueMutex.Release();

        if (!pRMState->run)
        {
            break;
        }

        // Wait for commands to complete
        {
            activeSet = (activeSet + 1) % pRMState->desc.bufferCount;
            REI_RL_ResourceSet& resourceSet = pRMState->resourceSets[activeSet];
            REI_waitForFences(pRMState->pRenderer, 1, &resourceSet.pFence);
            REI_atomicptr_store_release(&pRMState->requestsCompleted, lastRequestIdInSet[activeSet]);
            pRMState->tokenCond.WakeAll();
        }

        // check if we have new requests
        pRMState->queueMutex.Acquire();
        bool isQueueEmpty = pRMState->requestQueue.empty();
        pRMState->queueMutex.Release();
        if (!isQueueEmpty)
        {
            REI_RL_ResourceSet& resourceSet = pRMState->resourceSets[activeSet];
            pRMState->allocatedSpace = 0;
            REI_resetCmdPool(pRMState->pRenderer, resourceSet.pCmdPool);
            REI_beginCmd(resourceSet.pCmd);
        }
        else
        {
            continue;
        }

        // Record commands
        while (1)
        {
            if (requestCompleted)
            {
                pRMState->queueMutex.Acquire();
                if (!pRMState->requestQueue.empty())
                {
                    updateState = pRMState->requestQueue.front();
                    pRMState->requestQueue.pop_front();
                }
                else
                {
                    pRMState->queueMutex.Release();
                    break;
                }
                pRMState->queueMutex.Release();
            }

            switch (updateState.request.type)
            {
                case REI_RL_UPDATE_REQUEST_UPDATE_BUFFER:
                    requestCompleted = REI_RL_updateBuffer(pRMState, activeSet, updateState);
                    break;
                case REI_RL_UPDATE_REQUEST_UPDATE_TEXTURE:
                    requestCompleted = REI_RL_updateTexture(pRMState, activeSet, updateState);
                    break;
                default:
                    requestCompleted = true;
                    REI_ASSERT(false, "Should not happen");
                    break;
            }

            if (requestCompleted)
            {
                lastRequestIdInSet[activeSet] = ++requestsProcessed;
            }
            else
            {
                break;
            }
        }

        // Submit work
        {
            REI_RL_ResourceSet& resourceSet = pRMState->resourceSets[activeSet];
            REI_endCmd(resourceSet.pCmd);
            REI_queueSubmit(pRMState->pQueue, 1, &resourceSet.pCmd, resourceSet.pFence, 0, 0, 0, 0);
        }
    }

    REI_waitQueueIdle(pRMState->pQueue);
}

void REI_RL_addResourceLoader(REI_Renderer* pRenderer, REI_RL_ResourceLoaderDesc* pDesc, REI_RL_State** ppRMState)
{
    REI_AllocatorCallbacks allocatorCallbacks;
    REI_setupAllocatorCallbacks(pDesc ? pDesc->pAllocator : nullptr, allocatorCallbacks);

    REI_RL_State* pRMState = REI_new<REI_RL_State>(allocatorCallbacks, allocatorCallbacks);

    const REI_AllocatorCallbacks& allocator = pRMState->allocator;

    pRMState->pRenderer = pRenderer;
    pRMState->run = true;
    pRMState->desc =
        pDesc ? *pDesc : REI_RL_ResourceLoaderDesc{ DEFAULT_BUFFER_SIZE, DEFAULT_BUFFER_COUNT, DEFAULT_TIMESLICE_MS };

    REI_QueueDesc desc = { REI_QUEUE_FLAG_NONE, REI_QUEUE_PRIORITY_NORMAL, REI_CMD_POOL_COPY };
    REI_addQueue(pRenderer, &desc, &pRMState->pQueue);

    REI_QueueProperties queueProps;

    REI_getQueueProperties(pRMState->pQueue, &queueProps);

    const uint32_t maxBlockSize = 32;
    uint64_t       minUploadSize = queueProps.uploadGranularity.width * queueProps.uploadGranularity.height *
                             queueProps.uploadGranularity.depth * maxBlockSize;
    uint64_t size = REI_max(pRMState->desc.bufferSize, minUploadSize);

    pRMState->resourceSets = (REI_RL_ResourceSet*)allocator.pMalloc(allocator.pUserData, sizeof(REI_RL_ResourceSet) * pRMState->desc.bufferCount, 0);
    for (uint32_t i = 0; i < pRMState->desc.bufferCount; ++i)
    {
        REI_RL_ResourceSet& resourceSet = pRMState->resourceSets[i];
        REI_addFence(pRenderer, &resourceSet.pFence);

        REI_addCmdPool(pRenderer, pRMState->pQueue, false, &resourceSet.pCmdPool);

        REI_addCmd(pRenderer, resourceSet.pCmdPool, false, &resourceSet.pCmd);

        REI_BufferDesc bufferDesc = {};
        bufferDesc.size = size;
        bufferDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_CPU_ONLY;
        bufferDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
        REI_addBuffer(pRenderer, &bufferDesc, &resourceSet.pBuffer);
        REI_mapBuffer(pRenderer, resourceSet.pBuffer, &resourceSet.pMappedAddress);
    }

    pRMState->allocatedSpace = 0;

    REI_DeviceProperties deviceProperties{};
    REI_getDeviceProperties(pRenderer, &deviceProperties);

    pRMState->uniformBufferAlignment = deviceProperties.capabilities.uniformBufferAlignment;
    pRMState->uploadBufferTextureAlignment = deviceProperties.capabilities.uploadBufferTextureAlignment;
    pRMState->uploadBufferTextureRowAlignment = deviceProperties.capabilities.uploadBufferTextureRowAlignment;
    pRMState->uploadGranularity = queueProps.uploadGranularity;

    pRMState->threadDesc.pFunc = streamerThreadFunc;
    pRMState->threadDesc.pData = pRMState;

    pRMState->thread = create_thread(&pRMState->threadDesc);

    *ppRMState = pRMState;
}

void REI_RL_removeResourceLoader(REI_RL_State* pRMState)
{
    pRMState->run = false;
    pRMState->queueCond.WakeOne();
    destroy_thread(pRMState->thread);

    for (uint32_t i = 0; i < pRMState->desc.bufferCount; ++i)
    {
        REI_RL_ResourceSet& resourceSet = pRMState->resourceSets[i];
        REI_removeBuffer(pRMState->pRenderer, resourceSet.pBuffer);

        REI_removeCmd(pRMState->pRenderer, resourceSet.pCmdPool, resourceSet.pCmd);

        REI_removeCmdPool(pRMState->pRenderer, resourceSet.pCmdPool);

        REI_removeFence(pRMState->pRenderer, resourceSet.pFence);
    }

    pRMState->allocator.pFree(pRMState->allocator.pUserData, pRMState->resourceSets);

    REI_removeQueue(pRMState->pQueue);

    REI_delete(pRMState->allocator, pRMState);
}

static void
    REI_RL_queueResourceUpdate(REI_RL_State* pRMState, REI_RL_BufferUpdateDesc* pBufferUpdate, REI_RL_RequestId* token)
{
    pRMState->queueMutex.Acquire();
    REI_RL_RequestId t = REI_atomicptr_add_relaxed(&pRMState->requestsSubmitted, 1) + 1;
    pRMState->requestQueue.emplace_back(REI_RL_UpdateRequest(*pBufferUpdate));
    pRMState->queueMutex.Release();
    pRMState->queueCond.WakeOne();
    if (token)
        *token = t;
}

static void REI_RL_queueResourceUpdate(
    REI_RL_State* pRMState, REI_RL_TextureUpdateDesc* pTextureUpdate, REI_RL_RequestId* token)
{
    pRMState->queueMutex.Acquire();
    REI_RL_RequestId t = REI_atomicptr_add_relaxed(&pRMState->requestsSubmitted, 1) + 1;
    pRMState->requestQueue.emplace_back(REI_RL_UpdateRequest(*pTextureUpdate));
    pRMState->queueMutex.Release();
    pRMState->queueCond.WakeOne();
    if (token)
        *token = t;
}

bool REI_RL_isTokenCompleted(REI_RL_State* pRMState, REI_RL_RequestId token)
{
    bool completed = REI_atomicptr_load_acquire(&pRMState->requestsCompleted) >= token;
    return completed;
}

void REI_RL_waitTokenCompleted(REI_RL_State* pRMState, REI_RL_RequestId token)
{
    pRMState->tokenMutex.Acquire();
    while (!REI_RL_isTokenCompleted(pRMState, token))
    {
        pRMState->tokenCond.Wait(pRMState->tokenMutex);
    }
    pRMState->tokenMutex.Release();
}

void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_BufferUpdateDesc* pBufferUpdate, bool batch)
{
    REI_RL_RequestId token = 0;
    REI_RL_updateResource(pRMState, pBufferUpdate, &token);
    if (!batch)
        REI_RL_waitTokenCompleted(pRMState, token);
}

void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_TextureUpdateDesc* pTextureUpdate, bool batch)
{
    REI_RL_RequestId token = 0;
    REI_RL_updateResource(pRMState, pTextureUpdate, &token);
    if (!batch)
        REI_RL_waitTokenCompleted(pRMState, token);
}

void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_BufferUpdateDesc* pBufferUpdate, REI_RL_RequestId* token)
{
    REI_RL_RequestId updateToken;
    REI_RL_queueResourceUpdate(pRMState, pBufferUpdate, &updateToken);
    if (token)
        *token = updateToken;
}

void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_TextureUpdateDesc* pTextureUpdate, REI_RL_RequestId* token)
{
    REI_RL_RequestId updateToken;
    REI_RL_queueResourceUpdate(pRMState, pTextureUpdate, &updateToken);
    if (token)
        *token = updateToken;
}

bool REI_RL_isBatchCompleted(REI_RL_State* pRMState)
{
    REI_RL_RequestId token = REI_atomicptr_load_relaxed(&pRMState->requestsSubmitted);
    return REI_RL_isTokenCompleted(pRMState, token);
}

void REI_RL_waitBatchCompleted(REI_RL_State* pRMState)
{
    REI_RL_RequestId token = REI_atomicptr_load_relaxed(&pRMState->requestsSubmitted);
    REI_RL_waitTokenCompleted(pRMState, token);
}
