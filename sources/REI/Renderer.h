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

#include "Common.h"

#if !defined(REI_RENDERER_CUSTOM_MAX)
enum
{
    REI_MAX_GPUS = 10,
    REI_MAX_RENDER_TARGET_ATTACHMENTS = 8,
    REI_MAX_SUBMIT_CMDS = 20,    // max number of command lists / command buffers
    REI_MAX_SUBMIT_WAIT_SEMAPHORES = 8,
    REI_MAX_SUBMIT_SIGNAL_SEMAPHORES = 8,
    REI_MAX_PRESENT_WAIT_SEMAPHORES = 8,
    REI_MAX_VERTEX_BINDINGS = 15,
    REI_MAX_VERTEX_ATTRIBS = 15,
    REI_MAX_RESOURCE_NAME_LENGTH = 256,
    REI_MAX_SEMANTIC_NAME_LENGTH = 128,
    REI_MAX_MIP_LEVELS = 0xFFFFFFFF,
    REI_MAX_GPU_VENDOR_STRING_LENGTH = 64,    //max size for GPUVendorPreset strings
};
#endif

typedef enum REI_BitCount
{
    REI_MAX_RENDER_TARGET_ATTACHMENTS_BIT_COUNT = 4,
} REI_BitCount;
static_assert(REI_MAX_RENDER_TARGET_ATTACHMENTS < (1 << REI_MAX_RENDER_TARGET_ATTACHMENTS_BIT_COUNT), "");

typedef enum REI_Format
{
    REI_FMT_UNDEFINED = 0,
    REI_FMT_R1_UNORM = 1,
    REI_FMT_R2_UNORM = 2,
    REI_FMT_R4_UNORM = 3,
    REI_FMT_R4G4_UNORM = 4,
    REI_FMT_G4R4_UNORM = 5,
    REI_FMT_A8_UNORM = 6,
    REI_FMT_R8_UNORM = 7,
    REI_FMT_R8_SNORM = 8,
    REI_FMT_R8_UINT = 9,
    REI_FMT_R8_SINT = 10,
    REI_FMT_R8_SRGB = 11,
    REI_FMT_B2G3R3_UNORM = 12,
    REI_FMT_R4G4B4A4_UNORM = 13,
    REI_FMT_R4G4B4X4_UNORM = 14,
    REI_FMT_B4G4R4A4_UNORM = 15,
    REI_FMT_B4G4R4X4_UNORM = 16,
    REI_FMT_A4R4G4B4_UNORM = 17,
    REI_FMT_X4R4G4B4_UNORM = 18,
    REI_FMT_A4B4G4R4_UNORM = 19,
    REI_FMT_X4B4G4R4_UNORM = 20,
    REI_FMT_R5G6B5_UNORM = 21,
    REI_FMT_B5G6R5_UNORM = 22,
    REI_FMT_R5G5B5A1_UNORM = 23,
    REI_FMT_B5G5R5A1_UNORM = 24,
    REI_FMT_A1B5G5R5_UNORM = 25,
    REI_FMT_A1R5G5B5_UNORM = 26,
    REI_FMT_R5G5B5X1_UNORM = 27,
    REI_FMT_B5G5R5X1_UNORM = 28,
    REI_FMT_X1R5G5B5_UNORM = 29,
    REI_FMT_X1B5G5R5_UNORM = 30,
    REI_FMT_B2G3R3A8_UNORM = 31,
    REI_FMT_R8G8_UNORM = 32,
    REI_FMT_R8G8_SNORM = 33,
    REI_FMT_G8R8_UNORM = 34,
    REI_FMT_G8R8_SNORM = 35,
    REI_FMT_R8G8_UINT = 36,
    REI_FMT_R8G8_SINT = 37,
    REI_FMT_R8G8_SRGB = 38,
    REI_FMT_R16_UNORM = 39,
    REI_FMT_R16_SNORM = 40,
    REI_FMT_R16_UINT = 41,
    REI_FMT_R16_SINT = 42,
    REI_FMT_R16_SFLOAT = 43,
    REI_FMT_R16_SBFLOAT = 44,
    REI_FMT_R8G8B8_UNORM = 45,
    REI_FMT_R8G8B8_SNORM = 46,
    REI_FMT_R8G8B8_UINT = 47,
    REI_FMT_R8G8B8_SINT = 48,
    REI_FMT_R8G8B8_SRGB = 49,
    REI_FMT_B8G8R8_UNORM = 50,
    REI_FMT_B8G8R8_SNORM = 51,
    REI_FMT_B8G8R8_UINT = 52,
    REI_FMT_B8G8R8_SINT = 53,
    REI_FMT_B8G8R8_SRGB = 54,
    REI_FMT_R8G8B8A8_UNORM = 55,
    REI_FMT_R8G8B8A8_SNORM = 56,
    REI_FMT_R8G8B8A8_UINT = 57,
    REI_FMT_R8G8B8A8_SINT = 58,
    REI_FMT_R8G8B8A8_SRGB = 59,
    REI_FMT_B8G8R8A8_UNORM = 60,
    REI_FMT_B8G8R8A8_SNORM = 61,
    REI_FMT_B8G8R8A8_UINT = 62,
    REI_FMT_B8G8R8A8_SINT = 63,
    REI_FMT_B8G8R8A8_SRGB = 64,
    REI_FMT_R8G8B8X8_UNORM = 65,
    REI_FMT_B8G8R8X8_UNORM = 66,
    REI_FMT_R16G16_UNORM = 67,
    REI_FMT_G16R16_UNORM = 68,
    REI_FMT_R16G16_SNORM = 69,
    REI_FMT_G16R16_SNORM = 70,
    REI_FMT_R16G16_UINT = 71,
    REI_FMT_R16G16_SINT = 72,
    REI_FMT_R16G16_SFLOAT = 73,
    REI_FMT_R16G16_SBFLOAT = 74,
    REI_FMT_R32_UINT = 75,
    REI_FMT_R32_SINT = 76,
    REI_FMT_R32_SFLOAT = 77,
    REI_FMT_A2R10G10B10_UNORM = 78,
    REI_FMT_A2R10G10B10_UINT = 79,
    REI_FMT_A2R10G10B10_SNORM = 80,
    REI_FMT_A2R10G10B10_SINT = 81,
    REI_FMT_A2B10G10R10_UNORM = 82,
    REI_FMT_A2B10G10R10_UINT = 83,
    REI_FMT_A2B10G10R10_SNORM = 84,
    REI_FMT_A2B10G10R10_SINT = 85,
    REI_FMT_R10G10B10A2_UNORM = 86,
    REI_FMT_R10G10B10A2_UINT = 87,
    REI_FMT_R10G10B10A2_SNORM = 88,
    REI_FMT_R10G10B10A2_SINT = 89,
    REI_FMT_B10G10R10A2_UNORM = 90,
    REI_FMT_B10G10R10A2_UINT = 91,
    REI_FMT_B10G10R10A2_SNORM = 92,
    REI_FMT_B10G10R10A2_SINT = 93,
    REI_FMT_B10G11R11_UFLOAT = 94,
    REI_FMT_E5B9G9R9_UFLOAT = 95,
    REI_FMT_R16G16B16_UNORM = 96,
    REI_FMT_R16G16B16_SNORM = 97,
    REI_FMT_R16G16B16_UINT = 98,
    REI_FMT_R16G16B16_SINT = 99,
    REI_FMT_R16G16B16_SFLOAT = 100,
    REI_FMT_R16G16B16_SBFLOAT = 101,
    REI_FMT_R16G16B16A16_UNORM = 102,
    REI_FMT_R16G16B16A16_SNORM = 103,
    REI_FMT_R16G16B16A16_UINT = 104,
    REI_FMT_R16G16B16A16_SINT = 105,
    REI_FMT_R16G16B16A16_SFLOAT = 106,
    REI_FMT_R16G16B16A16_SBFLOAT = 107,
    REI_FMT_R32G32_UINT = 108,
    REI_FMT_R32G32_SINT = 109,
    REI_FMT_R32G32_SFLOAT = 110,
    REI_FMT_R32G32B32_UINT = 111,
    REI_FMT_R32G32B32_SINT = 112,
    REI_FMT_R32G32B32_SFLOAT = 113,
    REI_FMT_R32G32B32A32_UINT = 114,
    REI_FMT_R32G32B32A32_SINT = 115,
    REI_FMT_R32G32B32A32_SFLOAT = 116,
    REI_FMT_R64_UINT = 117,
    REI_FMT_R64_SINT = 118,
    REI_FMT_R64_SFLOAT = 119,
    REI_FMT_R64G64_UINT = 120,
    REI_FMT_R64G64_SINT = 121,
    REI_FMT_R64G64_SFLOAT = 122,
    REI_FMT_R64G64B64_UINT = 123,
    REI_FMT_R64G64B64_SINT = 124,
    REI_FMT_R64G64B64_SFLOAT = 125,
    REI_FMT_R64G64B64A64_UINT = 126,
    REI_FMT_R64G64B64A64_SINT = 127,
    REI_FMT_R64G64B64A64_SFLOAT = 128,
    REI_FMT_D16_UNORM = 129,
    REI_FMT_X8_D24_UNORM = 130,
    REI_FMT_D32_SFLOAT = 131,
    REI_FMT_S8_UINT = 132,
    REI_FMT_D16_UNORM_S8_UINT = 133,
    REI_FMT_D24_UNORM_S8_UINT = 134,
    REI_FMT_D32_SFLOAT_S8_UINT = 135,
    REI_FMT_DXBC1_RGB_UNORM = 136,
    REI_FMT_DXBC1_RGB_SRGB = 137,
    REI_FMT_DXBC1_RGBA_UNORM = 138,
    REI_FMT_DXBC1_RGBA_SRGB = 139,
    REI_FMT_DXBC2_UNORM = 140,
    REI_FMT_DXBC2_SRGB = 141,
    REI_FMT_DXBC3_UNORM = 142,
    REI_FMT_DXBC3_SRGB = 143,
    REI_FMT_DXBC4_UNORM = 144,
    REI_FMT_DXBC4_SNORM = 145,
    REI_FMT_DXBC5_UNORM = 146,
    REI_FMT_DXBC5_SNORM = 147,
    REI_FMT_DXBC6H_UFLOAT = 148,
    REI_FMT_DXBC6H_SFLOAT = 149,
    REI_FMT_DXBC7_UNORM = 150,
    REI_FMT_DXBC7_SRGB = 151,
    REI_FMT_PVRTC1_2BPP_UNORM = 152,
    REI_FMT_PVRTC1_4BPP_UNORM = 153,
    REI_FMT_PVRTC2_2BPP_UNORM = 154,
    REI_FMT_PVRTC2_4BPP_UNORM = 155,
    REI_FMT_PVRTC1_2BPP_SRGB = 156,
    REI_FMT_PVRTC1_4BPP_SRGB = 157,
    REI_FMT_PVRTC2_2BPP_SRGB = 158,
    REI_FMT_PVRTC2_4BPP_SRGB = 159,
    REI_FMT_ETC2_R8G8B8_UNORM = 160,
    REI_FMT_ETC2_R8G8B8_SRGB = 161,
    REI_FMT_ETC2_R8G8B8A1_UNORM = 162,
    REI_FMT_ETC2_R8G8B8A1_SRGB = 163,
    REI_FMT_ETC2_R8G8B8A8_UNORM = 164,
    REI_FMT_ETC2_R8G8B8A8_SRGB = 165,
    REI_FMT_ETC2_EAC_R11_UNORM = 166,
    REI_FMT_ETC2_EAC_R11_SNORM = 167,
    REI_FMT_ETC2_EAC_R11G11_UNORM = 168,
    REI_FMT_ETC2_EAC_R11G11_SNORM = 169,
    REI_FMT_ASTC_4x4_UNORM = 170,
    REI_FMT_ASTC_4x4_SRGB = 171,
    REI_FMT_ASTC_5x4_UNORM = 172,
    REI_FMT_ASTC_5x4_SRGB = 173,
    REI_FMT_ASTC_5x5_UNORM = 174,
    REI_FMT_ASTC_5x5_SRGB = 175,
    REI_FMT_ASTC_6x5_UNORM = 176,
    REI_FMT_ASTC_6x5_SRGB = 177,
    REI_FMT_ASTC_6x6_UNORM = 178,
    REI_FMT_ASTC_6x6_SRGB = 179,
    REI_FMT_ASTC_8x5_UNORM = 180,
    REI_FMT_ASTC_8x5_SRGB = 181,
    REI_FMT_ASTC_8x6_UNORM = 182,
    REI_FMT_ASTC_8x6_SRGB = 183,
    REI_FMT_ASTC_8x8_UNORM = 184,
    REI_FMT_ASTC_8x8_SRGB = 185,
    REI_FMT_ASTC_10x5_UNORM = 186,
    REI_FMT_ASTC_10x5_SRGB = 187,
    REI_FMT_ASTC_10x6_UNORM = 188,
    REI_FMT_ASTC_10x6_SRGB = 189,
    REI_FMT_ASTC_10x8_UNORM = 190,
    REI_FMT_ASTC_10x8_SRGB = 191,
    REI_FMT_ASTC_10x10_UNORM = 192,
    REI_FMT_ASTC_10x10_SRGB = 193,
    REI_FMT_ASTC_12x10_UNORM = 194,
    REI_FMT_ASTC_12x10_SRGB = 195,
    REI_FMT_ASTC_12x12_UNORM = 196,
    REI_FMT_ASTC_12x12_SRGB = 197,
    REI_FMT_CLUT_P4 = 198,
    REI_FMT_CLUT_P4A4 = 199,
    REI_FMT_CLUT_P8 = 200,
    REI_FMT_CLUT_P8A8 = 201,
    REI_FMT_COUNT = 202,
    REI_FORMAT_BIT_COUNT = 8,
} REI_Format;
static_assert(REI_FMT_COUNT <= (1 << REI_FORMAT_BIT_COUNT), "");

typedef enum REI_CmdPoolType
{
    REI_CMD_POOL_DIRECT,
    REI_CMD_POOL_BUNDLE,
    REI_CMD_POOL_COPY,
    REI_CMD_POOL_COMPUTE,
    REI_MAX_CMD_TYPE,
    REI_CMD_POOL_TYPE_BIT_COUNT = 2
} REI_CmdPoolType;
static_assert(REI_MAX_CMD_TYPE <= (1 << REI_CMD_POOL_TYPE_BIT_COUNT), "");

typedef enum REI_QueueFlag
{
    REI_QUEUE_FLAG_NONE,
    REI_QUEUE_FLAG_DISABLE_GPU_TIMEOUT,
    REI_QUEUE_FLAG_INIT_MICROPROFILE,
    REI_MAX_QUEUE_FLAG
} REI_QueueFlag;

typedef enum REI_QueuePriority
{
    REI_QUEUE_PRIORITY_NORMAL,
    REI_QUEUE_PRIORITY_HIGH,
    REI_QUEUE_PRIORITY_GLOBAL_REALTIME,
    REI_MAX_QUEUE_PRIORITY,
    REI_QUEUE_PRIORITY_BIT_COUNT = 2
} REI_QueuePriority;
static_assert(REI_MAX_QUEUE_PRIORITY <= (1 << REI_QUEUE_PRIORITY_BIT_COUNT), "");

typedef enum REI_LoadActionType
{
    REI_LOAD_ACTION_DONTCARE,
    REI_LOAD_ACTION_LOAD,
    REI_LOAD_ACTION_CLEAR,
    REI_MAX_LOAD_ACTION,
    REI_LOAD_ACTION_TYPE_BIT_COUNT = 2
} REI_LoadActionType;
static_assert(REI_MAX_LOAD_ACTION <= (1 << REI_LOAD_ACTION_TYPE_BIT_COUNT), "");

typedef enum REI_ResourceState
{
    REI_RESOURCE_STATE_UNDEFINED = 0,
    REI_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER = 0x1,
    REI_RESOURCE_STATE_INDEX_BUFFER = 0x2,
    REI_RESOURCE_STATE_RENDER_TARGET = 0x4,
    REI_RESOURCE_STATE_UNORDERED_ACCESS = 0x8,
    REI_RESOURCE_STATE_DEPTH_WRITE = 0x10,
    REI_RESOURCE_STATE_DEPTH_READ = 0x20,
    REI_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE = 0x40,
    REI_RESOURCE_STATE_PIXEL_SHADER_RESOURCE = 0x80,
    REI_RESOURCE_STATE_SHADER_RESOURCE = 0x40 | 0x80,
    REI_RESOURCE_STATE_STREAM_OUT = 0x100,
    REI_RESOURCE_STATE_INDIRECT_ARGUMENT = 0x200,
    REI_RESOURCE_STATE_COPY_DEST = 0x400,
    REI_RESOURCE_STATE_COPY_SOURCE = 0x800,
    REI_RESOURCE_STATE_RESOLVE_DEST = 0x1000,
    REI_RESOURCE_STATE_RESOLVE_SOURCE = 0x2000,
    REI_RESOURCE_STATE_GENERIC_READ = (((((0x1 | 0x2) | 0x40) | 0x80) | 0x200) | 0x800),
    REI_RESOURCE_STATE_PRESENT = 0x4000,
    REI_RESOURCE_STATE_COMMON = 0x8000,
} REI_ResourceState;

/// Choosing Memory Type
typedef enum REI_ResourceMemoryUsage
{
    /// No intended memory usage specified.
    REI_RESOURCE_MEMORY_USAGE_UNKNOWN = 0,
    /// Memory will be used on device only, no need to be mapped on host.
    REI_RESOURCE_MEMORY_USAGE_GPU_ONLY = 1,
    /// Memory will be mapped on host. Could be used for transfer to device.
    REI_RESOURCE_MEMORY_USAGE_CPU_ONLY = 2,
    /// Memory will be used for frequent (dynamic) updates from host and reads on device.
    REI_RESOURCE_MEMORY_USAGE_CPU_TO_GPU = 3,
    /// Memory will be used for writing on device and readback on host.
    REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU = 4,
    REI_RESOURCE_MEMORY_USAGE_MAX_ENUM = 0x7FFFFFFF
} REI_ResourceMemoryUsage;

typedef struct REI_IndirectDrawArguments
{
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t startVertex;
    uint32_t startInstance;
} REI_IndirectDrawArguments;

typedef struct REI_IndirectDrawIndexArguments
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t startIndex;
    uint32_t vertexOffset;
    uint32_t startInstance;
} REI_IndirectDrawIndexArguments;

typedef struct REI_IndirectDispatchArguments
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
} REI_IndirectDispatchArguments;

typedef enum REI_IndirectArgumentType
{
    REI_INDIRECT_DRAW,
    REI_INDIRECT_DRAW_INDEX,
    REI_INDIRECT_DISPATCH,
    REI_INDIRECT_VERTEX_BUFFER,
    REI_INDIRECT_INDEX_BUFFER,
    REI_INDIRECT_CONSTANT,
    REI_INDIRECT_DESCRIPTOR_TABLE,         // only for vulkan
    REI_INDIRECT_PIPELINE,                 // only for vulkan now, probally will add to dx when it comes to xbox
    REI_INDIRECT_CONSTANT_BUFFER_VIEW,     // only for dx
    REI_INDIRECT_SHADER_RESOURCE_VIEW,     // only for dx
    REI_INDIRECT_UNORDERED_ACCESS_VIEW,    // only for dx
    REI_INDIRECT_ARGUMENT_TYPE_COUNT,
    REI_INDIRECT_ARGUMENT_TYPE_BIT_COUNT = 4
} REI_IndirectArgumentType;
static_assert(REI_INDIRECT_ARGUMENT_TYPE_COUNT <= (1 << REI_INDIRECT_ARGUMENT_TYPE_BIT_COUNT), "");
/************************************************/

typedef enum REI_DescriptorType
{
    REI_DESCRIPTOR_TYPE_UNDEFINED = 0,
    REI_DESCRIPTOR_TYPE_SAMPLER = 0x01,
    // SRV Read only texture
    REI_DESCRIPTOR_TYPE_TEXTURE = (REI_DESCRIPTOR_TYPE_SAMPLER << 1),
    /// UAV REI_Texture
    REI_DESCRIPTOR_TYPE_RW_TEXTURE = (REI_DESCRIPTOR_TYPE_TEXTURE << 1),
    // SRV Read only buffer
    REI_DESCRIPTOR_TYPE_BUFFER = (REI_DESCRIPTOR_TYPE_RW_TEXTURE << 1),
    REI_DESCRIPTOR_TYPE_BUFFER_RAW = (REI_DESCRIPTOR_TYPE_BUFFER | (REI_DESCRIPTOR_TYPE_BUFFER << 1)),
    /// UAV REI_Buffer
    REI_DESCRIPTOR_TYPE_RW_BUFFER = (REI_DESCRIPTOR_TYPE_BUFFER << 2),
    REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW = (REI_DESCRIPTOR_TYPE_RW_BUFFER | (REI_DESCRIPTOR_TYPE_RW_BUFFER << 1)),
    /// Uniform buffer
    REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER = (REI_DESCRIPTOR_TYPE_RW_BUFFER << 2),
    REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC =
        (REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER | (REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER << 1)),
    REI_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC = (REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER << 2),
    REI_DESCRIPTOR_TYPE_VERTEX_BUFFER = (REI_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC << 1),
    REI_DESCRIPTOR_TYPE_INDEX_BUFFER = (REI_DESCRIPTOR_TYPE_VERTEX_BUFFER << 1),
    REI_DESCRIPTOR_TYPE_INDIRECT_BUFFER = (REI_DESCRIPTOR_TYPE_INDEX_BUFFER << 1),
    /// Push constant / Root constant
    REI_DESCRIPTOR_TYPE_ROOT_CONSTANT = (REI_DESCRIPTOR_TYPE_INDIRECT_BUFFER << 1),
    /// Cubemap SRV
    REI_DESCRIPTOR_TYPE_TEXTURE_CUBE = (REI_DESCRIPTOR_TYPE_TEXTURE | (REI_DESCRIPTOR_TYPE_ROOT_CONSTANT << 1)),
    // Render target
    REI_DESCRIPTOR_TYPE_RENDER_TARGET = (REI_DESCRIPTOR_TYPE_ROOT_CONSTANT << 2),
    /// RTV / DSV per array slice
    REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES = (REI_DESCRIPTOR_TYPE_RENDER_TARGET << 1),
    /// RTV / DSV per depth slice
    REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES = (REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES << 1),
    /// Usage as DST/SRC in copy operations
    REI_DESCRIPTOR_TYPE_COPY_DST = (REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
    REI_DESCRIPTOR_TYPE_COPY_SRC = (REI_DESCRIPTOR_TYPE_COPY_DST << 1),
    //#if defined(VULKAN)
    /// Subpass input (descriptor type only available in Vulkan)
    REI_DESCRIPTOR_TYPE_INPUT_ATTACHMENT = (REI_DESCRIPTOR_TYPE_COPY_SRC << 1),
    REI_DESCRIPTOR_TYPE_TEXEL_BUFFER = (REI_DESCRIPTOR_TYPE_INPUT_ATTACHMENT << 1),
    REI_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER = (REI_DESCRIPTOR_TYPE_TEXEL_BUFFER << 1),
    //#endif
    //#if defined(METAL)
    REI_DESCRIPTOR_TYPE_ARGUMENT_BUFFER = (REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES << 1),
    //#endif
} REI_DescriptorType;

typedef enum REI_SampleCount
{
    REI_SAMPLE_COUNT_1 = 1,
    REI_SAMPLE_COUNT_2 = 2,
    REI_SAMPLE_COUNT_4 = 4,
    REI_SAMPLE_COUNT_8 = 8,
    REI_SAMPLE_COUNT_16 = 16,
    REI_MAX_SAMPLE_COUNT_ENUM_VALUE = REI_SAMPLE_COUNT_16,
    REI_SAMPLE_COUNT_BIT_COUNT = 5
} REI_SampleCount;
static_assert(REI_MAX_SAMPLE_COUNT_ENUM_VALUE < (1 << REI_SAMPLE_COUNT_BIT_COUNT), "");

typedef enum REI_ShaderStage
{
    REI_SHADER_STAGE_NONE = 0,
    REI_SHADER_STAGE_VERT = 0X00000001,
    REI_SHADER_STAGE_TESC = 0X00000002,
    REI_SHADER_STAGE_TESE = 0X00000004,
    REI_SHADER_STAGE_GEOM = 0X00000008,
    REI_SHADER_STAGE_FRAG = 0X00000010,
    REI_SHADER_STAGE_COMP = 0X00000020,
    REI_SHADER_STAGE_ALL_GRAPHICS =
        ((uint32_t)REI_SHADER_STAGE_VERT | (uint32_t)REI_SHADER_STAGE_TESC | (uint32_t)REI_SHADER_STAGE_TESE |
         (uint32_t)REI_SHADER_STAGE_GEOM | (uint32_t)REI_SHADER_STAGE_FRAG),
    REI_SHADER_STAGE_COUNT = 6,
} REI_ShaderStage;

typedef enum REI_PrimitiveTopology
{
    REI_PRIMITIVE_TOPO_POINT_LIST = 0,
    REI_PRIMITIVE_TOPO_LINE_LIST,
    REI_PRIMITIVE_TOPO_LINE_STRIP,
    REI_PRIMITIVE_TOPO_TRI_LIST,
    REI_PRIMITIVE_TOPO_TRI_STRIP,
    REI_PRIMITIVE_TOPO_PATCH_LIST,
    REI_PRIMITIVE_TOPO_COUNT,
    REI_PRIMITIVE_TOPOLIGY_BIT_COUNT = 3
} REI_PrimitiveTopology;
static_assert(REI_PRIMITIVE_TOPO_COUNT <= (1 << REI_PRIMITIVE_TOPOLIGY_BIT_COUNT), "");

typedef enum REI_IndexType
{
    REI_INDEX_TYPE_UINT32 = 0,
    REI_INDEX_TYPE_UINT16,
    REI_INDEX_TYPE_COUNT,
    REI_INDEX_TYPE_BIT_COUNT = 1
} REI_IndexType;
static_assert(REI_INDEX_TYPE_COUNT <= (1 << REI_INDEX_TYPE_BIT_COUNT), "");

typedef enum REI_ShaderSemantic
{
    REI_SEMANTIC_POSITION0,
    REI_SEMANTIC_POSITION1,
    REI_SEMANTIC_POSITION2,
    REI_SEMANTIC_POSITION3,
    REI_SEMANTIC_NORMAL,
    REI_SEMANTIC_TANGENT,
    REI_SEMANTIC_BITANGENT,
    REI_SEMANTIC_COLOR0,
    REI_SEMANTIC_COLOR1,
    REI_SEMANTIC_COLOR2,
    REI_SEMANTIC_COLOR3,
    REI_SEMANTIC_TEXCOORD0,
    REI_SEMANTIC_TEXCOORD1,
    REI_SEMANTIC_TEXCOORD2,
    REI_SEMANTIC_TEXCOORD3,
    REI_SEMANTIC_TEXCOORD4,
    REI_SEMANTIC_TEXCOORD5,
    REI_SEMANTIC_TEXCOORD6,
    REI_SEMANTIC_TEXCOORD7,
    REI_SEMANTIC_USERDEFINED0,
    REI_SEMANTIC_USERDEFINED1,
    REI_SEMANTIC_USERDEFINED2,
    REI_SEMANTIC_USERDEFINED3,
    REI_SEMANTIC_USERDEFINED4,
    REI_SEMANTIC_USERDEFINED5,
    REI_SEMANTIC_USERDEFINED6,
    REI_SEMANTIC_USERDEFINED7,
    REI_SEMANTIC_USERDEFINED8,
    REI_SEMANTIC_USERDEFINED9,
    REI_SEMANTIC_USERDEFINED10,
    REI_SEMANTIC_USERDEFINED11,
    REI_SEMANTIC_USERDEFINED12,
    REI_SEMANTIC_USERDEFINED13,
    REI_SEMANTIC_USERDEFINED14,
    REI_SEMANTIC_USERDEFINED15,
    REI_SEMANTIC_COUNT,
    REI_SHADER_SEMANTIC_BIT_COUNT = 6
} REI_ShaderSemantic;
static_assert(REI_SEMANTIC_COUNT <= (1 << REI_SHADER_SEMANTIC_BIT_COUNT), "");

typedef enum REI_BlendConstant
{
    REI_BC_ZERO = 0,
    REI_BC_ONE,
    REI_BC_SRC_COLOR,
    REI_BC_ONE_MINUS_SRC_COLOR,
    REI_BC_DST_COLOR,
    REI_BC_ONE_MINUS_DST_COLOR,
    REI_BC_SRC_ALPHA,
    REI_BC_ONE_MINUS_SRC_ALPHA,
    REI_BC_DST_ALPHA,
    REI_BC_ONE_MINUS_DST_ALPHA,
    REI_BC_SRC_ALPHA_SATURATE,
    REI_BC_BLEND_FACTOR,
    REI_BC_INV_BLEND_FACTOR,
    REI_MAX_BLEND_CONSTANTS,
    REI_BLEND_CONSTANT_BIT_COUNT = 4
} REI_BlendConstant;
static_assert(REI_MAX_BLEND_CONSTANTS <= (1 << REI_BLEND_CONSTANT_BIT_COUNT), "");

typedef enum REI_BlendMode
{
    REI_BM_ADD,
    REI_BM_SUBTRACT,
    REI_BM_REVERSE_SUBTRACT,
    REI_BM_MIN,
    REI_BM_MAX,
    REI_MAX_BLEND_MODES,
    REI_BLEND_MODE_BIT_COUNT = 3
} REI_BlendMode;
static_assert(REI_MAX_BLEND_MODES <= (1 << REI_BLEND_MODE_BIT_COUNT), "");

typedef enum REI_CompareMode
{
    REI_CMP_NEVER,
    REI_CMP_LESS,
    REI_CMP_EQUAL,
    REI_CMP_LEQUAL,
    REI_CMP_GREATER,
    REI_CMP_NOTEQUAL,
    REI_CMP_GEQUAL,
    REI_CMP_ALWAYS,
    REI_MAX_COMPARE_MODES,
    REI_COMPARE_MODE_BIT_COUNT = 3
} REI_CompareMode;
static_assert(REI_MAX_COMPARE_MODES <= (1 << REI_COMPARE_MODE_BIT_COUNT), "");

typedef enum REI_StencilOp
{
    REI_STENCIL_OP_KEEP,
    REI_STENCIL_OP_SET_ZERO,
    REI_STENCIL_OP_REPLACE,
    REI_STENCIL_OP_INVERT,
    REI_STENCIL_OP_INCR,
    REI_STENCIL_OP_DECR,
    REI_STENCIL_OP_INCR_SAT,
    REI_STENCIL_OP_DECR_SAT,
    REI_MAX_STENCIL_OPS,
    REI_STENCIL_OP_BIT_COUNT = 3
} REI_StencilOp;
static_assert(REI_MAX_STENCIL_OPS <= (1 << REI_STENCIL_OP_BIT_COUNT), "");

enum
{
    REI_COLOR_MASK_RED = 0x1,
    REI_COLOR_MASK_GREEN = 0x2,
    REI_COLOR_MASK_BLUE = 0x4,
    REI_COLOR_MASK_ALPHA = 0x8,
    REI_COLOR_MASK_ALL = 0xF,
    REI_COLOR_MASK_NONE = 0x0,

    REI_BS_NONE = -1,
    REI_DS_NONE = -1,
    REI_RS_NONE = -1,
};

// Blend states are always attached to one of the eight or more render targets that
// are in a MRT
// Mask constants
typedef enum REI_BlendStateTargets
{
    REI_BLEND_STATE_TARGET_0 = 0x1,
    REI_BLEND_STATE_TARGET_1 = 0x2,
    REI_BLEND_STATE_TARGET_2 = 0x4,
    REI_BLEND_STATE_TARGET_3 = 0x8,
    REI_BLEND_STATE_TARGET_4 = 0x10,
    REI_BLEND_STATE_TARGET_5 = 0x20,
    REI_BLEND_STATE_TARGET_6 = 0x40,
    REI_BLEND_STATE_TARGET_7 = 0x80,
    REI_BLEND_STATE_TARGET_ALL = 0xFF,
    REI_MAX_BLEND_STATE_TARGET_ENUM_VALUE = REI_BLEND_STATE_TARGET_ALL,
    REI_BLEND_STATE_TARGETS_BIT_COUNT = 8
} REI_BlendStateTargets;
static_assert(REI_MAX_BLEND_STATE_TARGET_ENUM_VALUE < (1 << REI_BLEND_STATE_TARGETS_BIT_COUNT), "");

typedef enum REI_CullMode
{
    REI_CULL_MODE_NONE = 0,
    REI_CULL_MODE_BACK,
    REI_CULL_MODE_FRONT,
    REI_CULL_MODE_BOTH,
    REI_MAX_CULL_MODES,
    REI_CULL_MODE_BIT_COUNT = 2
} REI_CullMode;
static_assert(REI_MAX_CULL_MODES <= (1 << REI_CULL_MODE_BIT_COUNT), "");

typedef enum REI_FrontFace
{
    REI_FRONT_FACE_CCW = 0,
    REI_FRONT_FACE_CW,
    REI_FRONT_FACE_COUNT,
    REI_FRONT_FACE_BIT_COUNT = 1
} REI_FrontFace;
static_assert(REI_FRONT_FACE_COUNT <= (1 << REI_FRONT_FACE_BIT_COUNT), "");

typedef enum REI_FillMode
{
    REI_FILL_MODE_SOLID,
    REI_FILL_MODE_WIREFRAME,
    REI_MAX_FILL_MODES,
    REI_FILL_MODE_BIT_COUNT = 1
} REI_FillMode;
static_assert(REI_MAX_FILL_MODES <= (1 << REI_FILL_MODE_BIT_COUNT), "");

typedef enum REI_StencilFaceMask
{
    REI_STENCIL_FACE_FRONT,
    REI_STENCIL_FACE_BACK,
    REI_STENCIL_FACE_FRONT_AND_BACK,
    REI_STENCIL_FACE_MASK_COUNT,
    REI_STENCIL_FACE_MASK_BIT_COUNT = 2
} REI_StencilFaceMask;
static_assert(REI_STENCIL_FACE_MASK_COUNT <= (1 << REI_STENCIL_FACE_MASK_BIT_COUNT), "");

typedef enum REI_PipelineType
{
    REI_PIPELINE_TYPE_UNDEFINED = 0,
    REI_PIPELINE_TYPE_COMPUTE,
    REI_PIPELINE_TYPE_GRAPHICS,
    REI_PIPELINE_TYPE_COUNT,
    REI_PIPELINE_TYPE_BIT_COUNT = 2
} REI_PipelineType;
static_assert(REI_PIPELINE_TYPE_COUNT <= (1 << REI_PIPELINE_TYPE_BIT_COUNT), "");

typedef enum REI_FilterType
{
    REI_FILTER_NEAREST = 0,
    REI_FILTER_LINEAR,
    REI_FILTER_TYPE_COUNT,
    REI_FILTER_TYPE_BIT_COUNT = 1
} REI_FilterType;
static_assert(REI_FILTER_TYPE_COUNT <= (1 << REI_FILTER_TYPE_BIT_COUNT), "");

typedef enum REI_AddressMode
{
    REI_ADDRESS_MODE_MIRROR,
    REI_ADDRESS_MODE_REPEAT,
    REI_ADDRESS_MODE_CLAMP_TO_EDGE,
    REI_ADDRESS_MODE_CLAMP_TO_BORDER,
    REI_ADDRESS_MODE_COUNT,
    REI_ADDRESS_MODE_BIT_COUNT = 2
} REI_AddressMode;
static_assert(REI_ADDRESS_MODE_COUNT <= (1 << REI_ADDRESS_MODE_BIT_COUNT), "");

typedef enum REI_MipmapMode
{
    REI_MIPMAP_MODE_NEAREST = 0,
    REI_MIPMAP_MODE_LINEAR,
    REI_MIPMAP_MODE_COUNT,
    REI_MIPMAP_MODE_BIT_COUNT = 1
} REI_MipmapMode;
static_assert(REI_MIPMAP_MODE_COUNT <= (1 << REI_MIPMAP_MODE_BIT_COUNT), "");

typedef enum REI_BufferCreationFlags
{
    /// Default flag (REI_Buffer will use aliased memory, buffer will not be cpu accessible until mapBuffer is called)
    REI_BUFFER_CREATION_FLAG_NONE = 0x01,
    /// REI_Buffer will allocate its own memory (COMMITTED resource)
    REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT = 0x02,
    /// REI_Buffer will be persistently mapped
    REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT = 0x04,
    /// Use ESRAM to store this buffer
    REI_BUFFER_CREATION_FLAG_ESRAM = 0x08,
    /// Flag to specify not to allocate descriptors for the resource
    REI_BUFFER_CREATION_FLAG_NO_DESCRIPTOR_VIEW_CREATION = 0x10,
} REI_BufferCreationFlags;

typedef enum REI_TextureCreationFlags
{
    /// Default flag (REI_Texture will use default allocation strategy decided by the api specific allocator)
    REI_TEXTURE_CREATION_FLAG_NONE = 0,
    /// REI_Texture will allocate its own memory (COMMITTED resource)
    REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT = 0x01,
    /// REI_Texture will be allocated in memory which can be shared among multiple processes
    REI_TEXTURE_CREATION_FLAG_EXPORT_BIT = 0x02,
    /// REI_Texture will be allocated in memory which can be shared among multiple gpus
    REI_TEXTURE_CREATION_FLAG_EXPORT_ADAPTER_BIT = 0x04,
    /// REI_Texture will be imported from a handle created in another process
    REI_TEXTURE_CREATION_FLAG_IMPORT_BIT = 0x08,
    /// Use ESRAM to store this texture
    REI_TEXTURE_CREATION_FLAG_ESRAM = 0x10,
    /// Use on-tile memory to store this texture
    REI_TEXTURE_CREATION_FLAG_ON_TILE = 0x20,
    /// Prevent compression meta data from generating (XBox)
    REI_TEXTURE_CREATION_FLAG_NO_COMPRESSION = 0x40,
    /// Force 2D instead of automatically determining dimension based on width, height, depth
    REI_TEXTURE_CREATION_FLAG_FORCE_2D = 0x80,
    /// Force 3D instead of automatically determining dimension based on width, height, depth
    REI_TEXTURE_CREATION_FLAG_FORCE_3D = 0x100,
} REI_TextureCreationFlags;

typedef enum REI_QueryType
{
    REI_QUERY_TYPE_TIMESTAMP = 0,
    REI_QUERY_TYPE_PIPELINE_STATISTICS,
    REI_QUERY_TYPE_OCCLUSION,
    REI_QUERY_TYPE_BINARY_OCCLUSION,
    REI_QUERY_TYPE_COUNT,
    REI_QUERY_TYPE_BIT_COUNT = 2
} REI_QueryType;
static_assert(REI_QUERY_TYPE_COUNT <= (1 << REI_QUERY_TYPE_BIT_COUNT), "");

typedef enum REI_DescriptorTableSlot
{
    REI_DESCRIPTOR_TABLE_SLOT_0,
    REI_DESCRIPTOR_TABLE_SLOT_1,
    REI_DESCRIPTOR_TABLE_SLOT_2,
    REI_DESCRIPTOR_TABLE_SLOT_3,
    REI_DESCRIPTOR_TABLE_SLOT_4,
    REI_DESCRIPTOR_TABLE_SLOT_5,
    REI_DESCRIPTOR_TABLE_SLOT_6,
    REI_DESCRIPTOR_TABLE_SLOT_7,
    REI_DESCRIPTOR_TABLE_SLOT_COUNT,
    REI_DESCRIPTOR_TABLE_SLOT_BIT_COUNT = 3
} REI_DescriptorTableSlot;
static_assert(REI_DESCRIPTOR_TABLE_SLOT_COUNT <= (1 << REI_DESCRIPTOR_TABLE_SLOT_BIT_COUNT), "");

typedef enum REI_FenceStatus
{
    REI_FENCE_STATUS_COMPLETE = 0,
    REI_FENCE_STATUS_INCOMPLETE,
    REI_FENCE_STATUS_NOTSUBMITTED,
    REI_FENCE_STATUS_COUNT,
    REI_FENCE_STATUS_BIT_COUNT = 2
} REI_FenceStatus;
static_assert(REI_FENCE_STATUS_COUNT <= (1 << REI_FENCE_STATUS_BIT_COUNT), "");

typedef enum REI_VertexAttribRate
{
    REI_VERTEX_ATTRIB_RATE_VERTEX = 0,
    REI_VERTEX_ATTRIB_RATE_INSTANCE = 1,
    REI_VERTEX_ATTRIB_RATE_COUNT,
    REI_VERTEX_ATTRIB_RATE_BIT_COUNT = 1
} REI_VertexAttribRate;
static_assert(REI_VERTEX_ATTRIB_RATE_COUNT <= (1 << REI_VERTEX_ATTRIB_RATE_BIT_COUNT), "");

typedef enum REI_ShaderTarget
{
    REI_SHADER_TARGET_5_1,
    REI_SHADER_TARGET_6_0,
    REI_SHADER_TARGET_6_1,
    REI_SHADER_TARGET_6_2,
    REI_SHADER_TARGET_6_3,
    REI_SHADER_TARGET_COUNT,
    REI_SHADER_TARGET_BIT_COUNT = 3
} REI_ShaderTarget;
static_assert(REI_SHADER_TARGET_COUNT <= (1 << REI_SHADER_TARGET_BIT_COUNT), "");

typedef enum REI_DefaultResourceAlignment
{
    REI_RESOURCE_BUFFER_ALIGNMENT = 4U,
} REI_DefaultResourceAlignment;

enum REI_COMPONENT_MAPPING    // matches Vulkan mapping
{
    REI_COMPONENT_MAPPING_DEFAULT = 0,
    REI_COMPONENT_MAPPING_0 = 1,
    REI_COMPONENT_MAPPING_1 = 2,
    REI_COMPONENT_MAPPING_R = 3,
    REI_COMPONENT_MAPPING_G = 4,
    REI_COMPONENT_MAPPING_B = 5,
    REI_COMPONENT_MAPPING_A = 6,
    REI_COMPONENT_MAPPING_COUNT,
    REI_COMPONENT_MAPPING_BIT_COUNT = 3
};
static_assert(REI_COMPONENT_MAPPING_COUNT <= (1 << REI_COMPONENT_MAPPING_BIT_COUNT), "");

// Forward declarations
typedef struct REI_Renderer             REI_Renderer;
typedef struct REI_Queue                REI_Queue;
typedef struct REI_CmdPool              REI_CmdPool;
typedef struct REI_Cmd                  REI_Cmd;
typedef struct REI_CommandSignature     REI_CommandSignature;
typedef struct REI_Buffer               REI_Buffer;
typedef struct REI_Texture              REI_Texture;
typedef struct REI_Sampler              REI_Sampler;
typedef struct REI_DescriptorTableArray REI_DescriptorTableArray;
typedef struct REI_RootSignature        REI_RootSignature;
typedef struct REI_Shader               REI_Shader;
typedef struct REI_Pipeline             REI_Pipeline;
typedef struct REI_PipelineCache        REI_PipelineCache;
typedef struct REI_QueryPool            REI_QueryPool;
typedef struct REI_Fence                REI_Fence;
typedef struct REI_Semaphore            REI_Semaphore;
typedef struct REI_Swapchain            REI_Swapchain;

typedef struct REI_ReadRange
{
    uint64_t offset;
    uint64_t size;
} REI_ReadRange;

typedef struct REI_Region3D
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
    uint32_t w;
    uint32_t h;
    uint32_t d;
} REI_Region3D;

typedef struct REI_Extent3D
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
} REI_Extent3D;

typedef struct REI_ClearValue
{
    union
    {
        struct
        {
            float r;
            float g;
            float b;
            float a;
        } rt;
        struct
        {
            float    depth;
            uint32_t stencil;
        } ds;
    };
} REI_ClearValue;

typedef struct REI_LoadActionsDesc
{
    REI_ClearValue     clearColorValues[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    REI_LoadActionType loadActionsColor[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    REI_ClearValue     clearDepth;
    uint32_t           loadActionDepth: REI_LOAD_ACTION_TYPE_BIT_COUNT;      //REI_LoadActionType
    uint32_t           loadActionStencil: REI_LOAD_ACTION_TYPE_BIT_COUNT;    //REI_LoadActionType
} REI_LoadActionsDesc;

typedef struct REI_BufferBarrier
{
    REI_Buffer*       pBuffer;
    REI_ResourceState startState;
    REI_ResourceState endState;
} REI_BufferBarrier;

typedef struct REI_TextureBarrier
{
    REI_Texture*      pTexture;
    REI_ResourceState startState;
    REI_ResourceState endState;
} REI_TextureBarrier;

typedef struct REI_BlendStateDesc
{
    /// Source blend factor per render target.
    REI_BlendConstant srcFactors[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    /// Destination blend factor per render target.
    REI_BlendConstant dstFactors[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    /// Source alpha blend factor per render target.
    REI_BlendConstant srcAlphaFactors[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    /// Destination alpha blend factor per render target.
    REI_BlendConstant dstAlphaFactors[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    /// Blend mode per render target.
    REI_BlendMode blendModes[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    /// Alpha blend mode per render target.
    REI_BlendMode blendAlphaModes[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    /// Write mask per render target.
    int32_t masks[REI_MAX_RENDER_TARGET_ATTACHMENTS];
    /// Mask that identifies the render targets affected by the blend state.
    uint32_t renderTargetMask : REI_BLEND_STATE_TARGETS_BIT_COUNT; //REI_BlendStateTargets
    /// Set whether alpha to coverage should be enabled.
    uint32_t alphaToCoverage : 1;
    /// Set whether each render target has an unique blend function. When false the blend function in slot 0 will be used for all render targets.
    uint32_t independentBlend : 1;
} REI_BlendStateDesc;

typedef struct REI_DepthStateDesc
{
    uint32_t depthTestEnable : 1;
    uint32_t depthWriteEnable : 1;
    uint32_t stencilTestEnable : 1;
    uint32_t depthCmpFunc: REI_COMPARE_MODE_BIT_COUNT;        // REI_CompareMode
    uint32_t stencilFrontFunc: REI_COMPARE_MODE_BIT_COUNT;    // REI_CompareMode
    uint32_t stencilFrontFail: REI_STENCIL_OP_BIT_COUNT;      // REI_StencilOp
    uint32_t depthFrontFail: REI_STENCIL_OP_BIT_COUNT;        // REI_StencilOp
    uint32_t stencilFrontPass: REI_STENCIL_OP_BIT_COUNT;      // REI_StencilOp
    uint32_t stencilBackFunc: REI_COMPARE_MODE_BIT_COUNT;     // REI_CompareMode
    uint32_t stencilBackFail: REI_STENCIL_OP_BIT_COUNT;       // REI_StencilOp
    uint32_t depthBackFail: REI_STENCIL_OP_BIT_COUNT;         // REI_StencilOp
    uint32_t stencilBackPass: REI_STENCIL_OP_BIT_COUNT;       // REI_StencilOp
    uint32_t stencilReadMask : 8;
    uint32_t stencilWriteMask : 8;
} REI_DepthStateDesc;

static_assert(sizeof(REI_DepthStateDesc) == 8, "");

typedef struct REI_RasterizerStateDesc
{
    uint32_t cullMode: REI_CULL_MODE_BIT_COUNT;      //REI_CullMode
    uint32_t fillMode: REI_FILL_MODE_BIT_COUNT;      //REI_FillMode
    uint32_t frontFace: REI_FRONT_FACE_BIT_COUNT;    //REI_FrontFace
    uint32_t multiSample : 1;
    uint32_t scissor : 1;
    float    depthBiasConstantFactor;
    float    depthBiasSlopeFactor;
} REI_RasterizerStateDesc;

static_assert(sizeof(REI_RasterizerStateDesc) == 12, "");

typedef struct REI_VertexAttrib
{
    uint32_t semantic: REI_SHADER_SEMANTIC_BIT_COUNT;    //REI_ShaderSemantic
    uint32_t format: REI_FORMAT_BIT_COUNT;               //REI_Format
    uint32_t rate: REI_VERTEX_ATTRIB_RATE_BIT_COUNT;     //REI_VertexAttribRate
    uint32_t binding;
    uint32_t location;
    uint32_t offset;
} REI_VertexAttrib;

static_assert(sizeof(REI_VertexAttrib) == 16, "");

typedef struct REI_SubresourceDesc
{
    uint64_t     bufferOffset;
    uint32_t     rowPitch;
    uint32_t     slicePitch;
    uint32_t     arrayLayer;
    uint32_t     mipLevel;
    REI_Region3D region;
} REI_SubresourceDesc;

typedef struct REI_ResolveDesc
{
    uint32_t arrayLayer;
    uint32_t mipLevel;
} REI_ResolveDesc;

typedef struct REI_DescriptorData
{
    union
    {
        struct
        {
            /// Offset to bind the buffer descriptor
            const uint64_t* pOffsets;
            const uint64_t* pSizes;
        };
        uint32_t UAVMipSlice;
        bool     bindStencilResource;
    };
    /// Array of resources containing descriptor handles or constant to be used in ring buffer memory - DescriptorRange can hold only one resource type array
    union
    {
        /// Array of texture descriptors (srv and uav textures)
        REI_Texture** ppTextures;
        /// Array of sampler descriptors
        REI_Sampler** ppSamplers;
        /// Array of buffer descriptors (srv, uav and cbv buffers)
        REI_Buffer** ppBuffers;
    };
    uint32_t           tableIndex;
    REI_DescriptorType descriptorType;
    uint32_t           descriptorIndex = (uint32_t)-1;

    /// Number of resources in the descriptor(applies to array of textures, buffers,...)
    uint32_t count;
} REI_DescriptorData;

typedef struct REI_DeviceCapabilities
{
    uint32_t uniformBufferAlignment;
    uint32_t uploadBufferTextureAlignment;
    uint32_t uploadBufferTextureRowAlignment;
    uint32_t maxVertexInputBindings;
    uint32_t maxRootSignatureDWORDS;
    uint32_t waveLaneCount;
    bool     canShaderReadFrom[REI_FMT_COUNT];
    bool     canShaderWriteTo[REI_FMT_COUNT];
    bool     canRenderTargetWriteTo[REI_FMT_COUNT];
    uint32_t multiDrawIndirect : 1;
    uint32_t ROVsSupported : 1;
    uint32_t partialUpdateConstantBufferSupported : 1;
} REI_DeviceCapabilities;

typedef struct REI_DeviceProperties
{
    char vendorId[REI_MAX_GPU_VENDOR_STRING_LENGTH];
    char modelId[REI_MAX_GPU_VENDOR_STRING_LENGTH];
    char revisionId[REI_MAX_GPU_VENDOR_STRING_LENGTH];    // OPtional as not all gpu's have that. Default is : 0x00
    char deviceName[REI_MAX_GPU_VENDOR_STRING_LENGTH];    //If GPU Name is missing then value will be empty string
    REI_DeviceCapabilities capabilities;
} REI_DeviceProperties;

typedef struct REI_QueueProperties
{
    double       gpuTimestampFreq;
    REI_Extent3D uploadGranularity;
} REI_QueueProperties;

// Indirect command sturcture define
typedef struct REI_IndirectArgumentDescriptor
{
    REI_IndirectArgumentType type;
    uint32_t                 rootParameterIndex;
    uint32_t                 count;
    uint32_t                 divisor;

} REI_IndirectArgumentDescriptor;

typedef struct REI_RendererDesc
{
    const char*      app_name;
    const char*      gpu_name;
    REI_ShaderTarget shaderTarget;
    /// This results in new validation not possible during API calls on the CPU, by creating patched shaders that have validation added directly to the shader.
    /// However, it can slow things down a lot, especially for applications with numerous PSOs. Time to see the first render frame may take several minutes
    bool                          enableGPUBasedValidation;
    const REI_AllocatorCallbacks* pAllocator;
    REI_LogPtr                    pLog;
} REI_RendererDesc;

typedef struct REI_QueueDesc
{
    REI_QueueFlag     flag;
    REI_QueuePriority priority;
    REI_CmdPoolType   type;
} REI_QueueDesc;

typedef struct REI_CommandSignatureDesc
{
    REI_CmdPool*                    pCmdPool;
    REI_RootSignature*              pRootSignature;
    uint32_t                        indirectArgCount;
    REI_IndirectArgumentDescriptor* pArgDescs;
} REI_CommandSignatureDesc;

typedef struct REI_BufferDesc
{
    /// Size of the buffer (in bytes)
    uint64_t size;
    /// Decides which memory heap buffer will use (default, upload, readback)
    REI_ResourceMemoryUsage memoryUsage;
    /// Creation flags of the buffer
    uint32_t flags;
    /// What state will the buffer get created in
    REI_ResourceState startState;
    /// Specifies whether the buffer will have 32 bit or 16 bit indices (applicable to BUFFER_USAGE_INDEX)
    REI_IndexType indexType;
    /// Vertex stride of the buffer (applicable to BUFFER_USAGE_VERTEX)
    uint32_t vertexStride;
    /// Index of the first element accessible by the SRV/UAV (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint64_t firstElement;
    /// Number of elements in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint64_t elementCount;
    /// Size of each element (in bytes) in the buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    uint64_t structStride;
    /// Set this to specify a counter buffer for this buffer (applicable to BUFFER_USAGE_STORAGE_SRV, BUFFER_USAGE_STORAGE_UAV)
    struct REI_Buffer* pCounterBuffer;
    /// Format of the buffer (applicable to typed storage buffers (Buffer<T>)
    REI_Format format;
    /// Flags specifying the suitable usage of this buffer (Uniform buffer, Vertex Buffer, Index Buffer,...)
    uint32_t descriptors;
    /// Debug name used in gpu profile
    const wchar_t* pDebugName;
} REI_BufferDesc;

typedef struct REI_TextureDesc
{
    /// REI_Texture creation flags (decides memory allocation strategy, sharing access,...)
    REI_TextureCreationFlags flags;
    /// Width
    uint32_t width;
    /// Height
    uint32_t height;
    /// Depth (Should be 1 if not a mType is not TEXTURE_TYPE_3D)
    uint32_t depth;
    /// REI_Texture array size (Should be 1 if texture is not a texture array or cubemap)
    uint32_t arraySize;
    /// Number of mip levels
    uint32_t mipLevels;
    /// REI_SampleCount. Number of multisamples per pixel (currently Textures created with mUsage TEXTURE_USAGE_SAMPLED_IMAGE only support SAMPLE_COUNT_1)
    uint32_t sampleCount : REI_SAMPLE_COUNT_BIT_COUNT;
    /// REI_Format. Image format
    uint32_t format : REI_FORMAT_BIT_COUNT;
    /// Optimized clear value (recommended to use this same value when clearing the rendertarget)
    REI_ClearValue clearValue;
    /// Descriptor creation
    uint32_t descriptors;
    /// Pointer to native texture handle if the texture does not own underlying resource
    uint64_t pNativeHandle;
    /// Debug name used in gpu profile
    const wchar_t* pDebugName;
    /// Is the texture CPU accessible (applicable on hardware supporting CPU mapped textures (UMA))
    bool hostVisible;
    /// Mapping for component/channel swizzling
    REI_COMPONENT_MAPPING componentMapping[4];
} REI_TextureDesc;

typedef struct REI_SamplerDesc
{
    uint32_t minFilter: REI_FILTER_TYPE_BIT_COUNT;       //REI_FilterType
    uint32_t magFilter: REI_FILTER_TYPE_BIT_COUNT;       //REI_FilterType
    uint32_t mipmapMode: REI_MIPMAP_MODE_BIT_COUNT;      //REI_MipmapMode
    uint32_t addressU: REI_ADDRESS_MODE_BIT_COUNT;       //REI_AddressMode
    uint32_t addressV: REI_ADDRESS_MODE_BIT_COUNT;       //REI_AddressMode
    uint32_t addressW: REI_ADDRESS_MODE_BIT_COUNT;       //REI_AddressMode
    uint32_t compareFunc: REI_COMPARE_MODE_BIT_COUNT;    //REI_CompareMode
    float    mipLodBias;
    float    maxAnisotropy;
} REI_SamplerDesc;

static_assert(sizeof(REI_SamplerDesc) == 12, "");

typedef struct REI_DescriptorSetDesc
{
    REI_RootSignature*      pRootSignature;
    REI_DescriptorTableSlot slot;
    uint32_t                maxTables;
} REI_DescriptorTableArrayDesc;

typedef struct REI_DescriptorBinding
{
    REI_DescriptorType descriptorType;
    uint32_t           binding;
    uint32_t           reg;
    uint32_t           descriptorCount;
} REI_DescriptorBinding;

typedef struct REI_DescriptorTableLayout
{
    REI_DescriptorTableSlot slot;
    uint32_t                stageFlags;
    uint32_t                bindingCount;
    REI_DescriptorBinding*  pBindings;
} REI_DescriptorTableLayout;

typedef struct REI_PushConstantRange
{
    uint32_t slot;
    uint32_t stageFlags;
    uint32_t offset;
    uint32_t size;
    uint32_t reg;
} REI_PushConstantRange;

typedef struct REI_StaticSamplerBinding
{
    uint32_t      binding;
    uint32_t      reg;
    uint32_t      descriptorCount;
    REI_Sampler** ppStaticSamplers;
} REI_StaticSamplerBinding;

typedef struct REI_RootSignatureDesc
{
    REI_PipelineType           pipelineType;
    uint32_t                   tableLayoutCount;
    REI_DescriptorTableLayout* pTableLayouts;
    uint32_t                   pushConstantRangeCount;
    REI_PushConstantRange*     pPushConstantRanges;
    REI_DescriptorTableSlot    staticSamplerSlot;
    uint32_t                   staticSamplerStageFlags;
    uint32_t                   staticSamplerBindingCount;
    REI_StaticSamplerBinding*  pStaticSamplerBindings;
} REI_RootSignatureDesc;

typedef struct REI_ShaderDesc
{
    REI_ShaderStage stage;
    uint8_t*        pByteCode;
    uint32_t        byteCodeSize;
    const char*     pEntryPoint;
} REI_ShaderDesc;

typedef enum REI_PipelineCacheFlags
{
    PIPELINE_CACHE_FLAG_NONE = 0x0,
    PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED = 0x1,
} REI_PipelineCacheFlags;

typedef struct REI_PipelineCacheDesc
{
    /// Initial pipeline cache data (can be NULL which means empty pipeline cache)
    void*                   pData;
    /// Initial pipeline cache size
    size_t                  mSize;
    REI_PipelineCacheFlags  mFlags;
} REI_PipelineCacheDesc;

typedef struct REI_GraphicsPipelineDesc
{
    REI_Shader**             ppShaderPrograms;
    REI_RootSignature*       pRootSignature;
    REI_VertexAttrib*        pVertexAttribs;
    REI_RasterizerStateDesc* pRasterizerState;
    REI_DepthStateDesc*      pDepthState;
    REI_BlendStateDesc*      pBlendState;
    REI_Format*              pColorFormats;
    uint32_t                 vertexAttribCount;
    uint32_t                 primitiveTopo: REI_PRIMITIVE_TOPOLIGY_BIT_COUNT;    //REI_PrimitiveTopology
    uint32_t                 sampleCount: REI_SAMPLE_COUNT_BIT_COUNT;            //REI_SampleCount
    uint32_t                 depthStencilFormat: REI_FORMAT_BIT_COUNT;           //REI_Format
    uint32_t                 renderTargetCount: REI_MAX_RENDER_TARGET_ATTACHMENTS_BIT_COUNT;
    uint32_t                 patchControlPoints;
    uint32_t                 shaderProgramCount;
    REI_PipelineCache*       pCache;
} REI_GraphicsPipelineDesc;
#if REI_PTRSIZE == 8
static_assert(sizeof(REI_GraphicsPipelineDesc) == 80, "");
#elif REI_PTRSIZE == 4
static_assert(sizeof(REI_GraphicsPipelineDesc) == 48, "");
#endif

typedef struct REI_ComputePipelineDesc
{
    REI_Shader*        pShaderProgram;
    REI_RootSignature* pRootSignature;
    uint32_t           numThreadsPerGroup[3];
    REI_PipelineCache* pCache;
} REI_ComputePipelineDesc;

typedef struct REI_PipelineDesc
{
    REI_PipelineType type;
    union
    {
        REI_ComputePipelineDesc  computeDesc;
        REI_GraphicsPipelineDesc graphicsDesc;
    };
} REI_PipelineDesc;

typedef struct REI_QueryPoolDesc
{
    REI_QueryType type;
    uint32_t      queryCount;
} REI_QueryPoolDesc;

typedef struct REI_WindowHandle
{
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    Display* display;
    Window   window;
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    xcb_connection_t*        connection;
    xcb_window_t             window;
#else
    void* window;    //hWnd
#endif
} REI_WindowHandle;

typedef struct REI_SwapchainDesc
{
    /// Window handle
    REI_WindowHandle windowHandle;
    /// Queues which should be allowed to present
    REI_Queue** ppPresentQueues;
    /// Number of present queues
    uint32_t presentQueueCount;
    /// Number of backbuffers in this swapchain
    uint32_t imageCount;
    /// Width of the swapchain
    uint32_t width;
    /// Height of the swapchain
    uint32_t height;
    /// Sample count
    uint32_t sampleCount: REI_SAMPLE_COUNT_BIT_COUNT;
    /// Color format of the swapchain
    uint32_t colorFormat: REI_FORMAT_BIT_COUNT;
    /// Clear value
    REI_ClearValue colorClearValue;
    /// Set whether swap chain will be presented using vsync
    bool enableVsync;
} REI_SwapchainDesc;

// API functions

// allocates memory and initializes the renderer -> returns pRenderer
void REI_initRenderer(const REI_RendererDesc* pDesc, REI_Renderer** ppRenderer);
void REI_removeRenderer(REI_Renderer* pRenderer);
void REI_getDeviceProperties(REI_Renderer* pRenderer, REI_DeviceProperties* outProperties);

void REI_addFence(REI_Renderer* pRenderer, REI_Fence** pp_fence);
void REI_removeFence(REI_Renderer* pRenderer, REI_Fence* p_fence);
void REI_getFenceStatus(REI_Renderer* pRenderer, REI_Fence* p_fence, REI_FenceStatus* p_fence_status);
void REI_waitForFences(REI_Renderer* pRenderer, uint32_t fence_count, REI_Fence** pp_fences);

void REI_addSemaphore(REI_Renderer* pRenderer, REI_Semaphore** pp_semaphore);
void REI_removeSemaphore(REI_Renderer* pRenderer, REI_Semaphore* p_semaphore);

void REI_addQueue(REI_Renderer* pRenderer, REI_QueueDesc* pQDesc, REI_Queue** ppQueue);
void REI_removeQueue(REI_Queue* pQueue);
void REI_queueSubmit(
    REI_Queue* p_queue, uint32_t cmd_count, REI_Cmd** pp_cmds, REI_Fence* pFence, uint32_t wait_semaphore_count,
    REI_Semaphore** pp_wait_semaphores, uint32_t signal_semaphore_count, REI_Semaphore** pp_signal_semaphores);
void REI_queuePresent(
    REI_Queue* p_queue, REI_Swapchain* p_swap_chain, uint32_t swap_chain_image_index, uint32_t wait_semaphore_count,
    REI_Semaphore** pp_wait_semaphores);
void REI_waitQueueIdle(REI_Queue* p_queue);
void REI_getQueueProperties(REI_Queue* pQueue, REI_QueueProperties* outProperties);
//void REI_getTimestampFrequency(REI_Queue* pQueue, double* pFrequency);

void REI_addSampler(REI_Renderer* pRenderer, const REI_SamplerDesc* pDesc, REI_Sampler** pp_sampler);
void REI_removeSampler(REI_Renderer* pRenderer, REI_Sampler* p_sampler);

void REI_addBuffer(REI_Renderer* pRenderer, const REI_BufferDesc* desc, REI_Buffer** pp_buffer);
void REI_removeBuffer(REI_Renderer* pRenderer, REI_Buffer* p_buffer);
void REI_mapBuffer(REI_Renderer* pRenderer, REI_Buffer* pBuffer, void** pMappedMem);
void REI_unmapBuffer(REI_Renderer* pRenderer, REI_Buffer* pBuffer);
void REI_setBufferName(REI_Renderer* pRenderer, REI_Buffer* pBuffer, const char* pName);

void REI_addTexture(REI_Renderer* pRenderer, const REI_TextureDesc* pDesc, REI_Texture** pp_texture);
void REI_removeTexture(REI_Renderer* pRenderer, REI_Texture* p_texture);
void REI_setTextureName(REI_Renderer* pRenderer, REI_Texture* pTexture, const char* pName);

void REI_addShaders(REI_Renderer* pRenderer, const REI_ShaderDesc* p_descs, uint32_t shaderCount, REI_Shader** pp_shader_programs);
void REI_removeShaders(REI_Renderer* pRenderer, uint32_t shaderCount, REI_Shader** pp_shader_programs);

void REI_addRootSignature(
    REI_Renderer* pRenderer, const REI_RootSignatureDesc* pRootDesc, REI_RootSignature** pp_root_signature);
void REI_removeRootSignature(REI_Renderer* pRenderer, REI_RootSignature* pRootSignature);

void REI_addPipeline(REI_Renderer* pRenderer, const REI_PipelineDesc* p_pipeline_settings, REI_Pipeline** pp_pipeline);
void REI_removePipeline(REI_Renderer* pRenderer, REI_Pipeline* p_pipeline);

void REI_addPipelineCache(
    REI_Renderer* pRenderer, const REI_PipelineCacheDesc* pDesc, REI_PipelineCache** ppPipelineCache);
void REI_removePipelineCache(REI_Renderer* pRenderer, REI_PipelineCache* pPipelineCache);
void REI_getPipelineCacheData(REI_Renderer* pRenderer, REI_PipelineCache* pPipelineCache, size_t* pSize, void* pData);

void REI_addDescriptorTableArray(
    REI_Renderer* pRenderer, const REI_DescriptorTableArrayDesc* pDesc, REI_DescriptorTableArray** ppDescriptorTableArr);
void REI_removeDescriptorTableArray(REI_Renderer* pRenderer, REI_DescriptorTableArray* pDescriptorTableArr);
void REI_updateDescriptorTableArray(
    REI_Renderer* pRenderer, REI_DescriptorTableArray* pDescriptorTableArr, uint32_t count,
    const REI_DescriptorData* pParams);

void REI_addIndirectCommandSignature(
    REI_Renderer* pRenderer, const REI_CommandSignatureDesc* p_desc, REI_CommandSignature** ppCommandSignature);
void REI_removeIndirectCommandSignature(REI_Renderer* pRenderer, REI_CommandSignature* pCommandSignature);

void REI_addQueryPool(REI_Renderer* pRenderer, const REI_QueryPoolDesc* pDesc, REI_QueryPool** ppQueryPool);
void REI_removeQueryPool(REI_Renderer* pRenderer, REI_QueryPool* pQueryPool);

void REI_addCmdPool(REI_Renderer* pRenderer, REI_Queue* p_queue, bool transient, REI_CmdPool** pp_CmdPool);
void REI_resetCmdPool(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool);
void REI_removeCmdPool(REI_Renderer* pRenderer, REI_CmdPool* p_CmdPool);

void REI_addCmd(REI_Renderer* pRenderer, REI_CmdPool* p_CmdPool, bool secondary, REI_Cmd** pp_cmd);
void REI_removeCmd(REI_Renderer* pRenderer, REI_CmdPool* p_CmdPool, REI_Cmd* p_cmd);
void REI_beginCmd(REI_Cmd* p_cmd);
void REI_endCmd(REI_Cmd* p_cmd);
void REI_cmdBindRenderTargets(
    REI_Cmd* p_cmd, uint32_t render_target_count, REI_Texture** pp_render_targets, REI_Texture* p_depth_stencil,
    const REI_LoadActionsDesc* loadActions, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices,
    uint32_t depthArraySlice, uint32_t depthMipSlice);
void REI_cmdSetViewport(REI_Cmd* p_cmd, float x, float y, float width, float height, float min_depth, float max_depth);
void REI_cmdSetScissor(REI_Cmd* p_cmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void REI_cmdSetStencilRef(REI_Cmd* p_cmd, REI_StencilFaceMask face, uint32_t ref);
void REI_cmdBindPipeline(REI_Cmd* p_cmd, REI_Pipeline* p_pipeline);
void REI_cmdBindDescriptorTable(REI_Cmd* pCmd, uint32_t tableIndex, REI_DescriptorTableArray* pDescriptorTableArr);
void REI_cmdBindPushConstants(
    REI_Cmd* pCmd, REI_RootSignature* pRootSignature, REI_ShaderStage stages, uint32_t offset, uint32_t size,
    const void* pConstants);
void REI_cmdBindIndexBuffer(REI_Cmd* p_cmd, REI_Buffer* p_buffer, uint64_t offset);
void REI_cmdBindVertexBuffer(REI_Cmd* p_cmd, uint32_t buffer_count, REI_Buffer** pp_buffers, uint64_t* pOffsets);
void REI_cmdDraw(REI_Cmd* p_cmd, uint32_t vertex_count, uint32_t first_vertex);
void REI_cmdDrawInstanced(
    REI_Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance);
void REI_cmdDrawIndexed(REI_Cmd* p_cmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex);
void REI_cmdDrawIndexedInstanced(
    REI_Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
    uint32_t firstInstance);
void REI_cmdDispatch(REI_Cmd* p_cmd, uint32_t group_count_x, uint32_t group_count_y, uint32_t group_count_z);
void REI_cmdCopyBuffer(
    REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t dstOffset, REI_Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size);
void REI_cmdCopyBufferToTexture(
    REI_Cmd* pCmd, REI_Texture* pTexture, REI_Buffer* pSrcBuffer, REI_SubresourceDesc* pSubresourceDesc);
void REI_cmdCopyTextureToBuffer(
    REI_Cmd* pCmd, REI_Buffer* pDstBuffer, REI_Texture* pTexture, REI_SubresourceDesc* pSubresourceDesc);
void REI_cmdResolveTexture(
    REI_Cmd* pCmd, REI_Texture* pDstTexture, REI_Texture* pSrcTexture, REI_ResolveDesc* pResolveDesc);
void REI_cmdResourceBarrier(
    REI_Cmd* p_cmd, uint32_t buffer_barrier_count, REI_BufferBarrier* p_buffer_barriers, uint32_t texture_barrier_count,
    REI_TextureBarrier* p_texture_barriers);
void REI_cmdExecuteIndirect(
    REI_Cmd* pCmd, REI_CommandSignature* pCommandSignature, uint32_t maxCommandCount, REI_Buffer* pIndirectBuffer,
    uint64_t bufferOffset, REI_Buffer* pCounterBuffer, uint64_t counterBufferOffset);
void REI_cmdResetQueryPool(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount);
void REI_cmdBeginQuery(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t index);
void REI_cmdEndQuery(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t index);
void REI_cmdResolveQuery(
    REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t bufferOffset, REI_QueryPool* pQueryPool, uint32_t startQuery,
    uint32_t queryCount);
void REI_cmdBeginDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName);
void REI_cmdEndDebugMarker(REI_Cmd* pCmd);
void REI_cmdAddDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName);

void REI_addSwapchain(REI_Renderer* pRenderer, const REI_SwapchainDesc* p_desc, REI_Swapchain** pp_swap_chain);
void REI_removeSwapchain(REI_Renderer* pRenderer, REI_Swapchain* p_swap_chain);
void REI_getSwapchainTextures(REI_Swapchain* pSwapchain, uint32_t* count, REI_Texture** ppTextures);
void REI_acquireNextImage(
    REI_Renderer* pRenderer, REI_Swapchain* p_swap_chain, REI_Semaphore* p_signal_semaphore, REI_Fence* p_fence,
    uint32_t* p_image_index);
REI_Format REI_getRecommendedSwapchainFormat(bool hintHDR);
