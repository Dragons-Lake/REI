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

#include "Renderer.h"
#include "Thread.h"

#if defined(_WIN32)
#    define VK_USE_PLATFORM_WIN32_KHR
#endif
#if defined(__ANDROID__)
#    ifndef VK_USE_PLATFORM_ANDROID_KHR
#        define VK_USE_PLATFORM_ANDROID_KHR
#    endif
#elif defined(__linux__)
#    define VK_USE_PLATFORM_XLIB_KHR    //Use Xlib or Xcb as display server, defaults to Xlib
#elif defined(REI_PLATFORM_SWITCH)
#    ifndef VK_USE_PLATFORM_VI_NN
#        define VK_USE_PLATFORM_VI_NN
#    endif
#endif
#include <vulkan/vulkan.h>
//#define USE_DEBUG_UTILS_EXTENSION
/************************************************************************/
// Debugging Macros
/************************************************************************/
// Render doc capture support
#define USE_RENDER_DOC 0

#define USE_DEBUG_UTILS_EXTENSION 0
/************************************************************************/
/************************************************************************/

//if enabled, a warning will be displayed, otherwise ASSERT(false) when the user uses REI_cmdResourceBarrier inside a renderpass
#define REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS 1

enum
{
    REI_VK_MAX_QUEUE_FAMILY_COUNT = 16,
    REI_VK_MAX_QUEUES_PER_FAMILY = 64,
    REI_VK_MAX_VERTEX_BUFFERS = 64,
    REI_VK_CMD_SCRATCH_MEM_SIZE = 4 * 1024,    // 4KB
    REI_VK_MAX_EXTENSIONS_COUNT = 4000,
    REI_VK_MAX_DESCRIPTORS_SETS_IN_POOL = 8192,
    REI_VK_DESCRIPTOR_TYPE_SAMPLER_COUNT = 1024,
    REI_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_COUNT = 1,
    REI_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE_COUNT = 8192,
    REI_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE_COUNT = 1024,
    REI_VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER_COUNT = 1024,
    REI_VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER_COUNT = 1024,
    REI_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_COUNT = 8192,
    REI_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_COUNT = 1024,
    REI_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC_COUNT = 1024,
    REI_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC_COUNT = 1,
    REI_VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT_COUNT = 1,
};

typedef struct REI_RendererDescVk
{
    REI_RendererDesc desc;
    const char**     ppInstanceLayers;
    uint32_t         instanceLayerCount;
    const char**     ppInstanceExtensions;
    uint32_t         instanceExtensionCount;
    const char**     ppDeviceExtensions;
    uint32_t         deviceExtensionCount;
    uint32_t         vulkanApiVersion;
} REI_RendererDescVk;

typedef struct REI_RenderPassDesc
{
    REI_Format*               pColorFormats;
    const REI_LoadActionType* pLoadActionsColor;
    uint32_t                  renderTargetCount;
    uint32_t                  sampleCount: REI_SAMPLE_COUNT_BIT_COUNT;              //REI_SampleCount
    uint32_t                  depthStencilFormat: REI_FORMAT_BIT_COUNT;             //REI_Format
    uint32_t                  loadActionDepth: REI_LOAD_ACTION_TYPE_BIT_COUNT;      //REI_LoadActionType
    uint32_t                  loadActionStencil: REI_LOAD_ACTION_TYPE_BIT_COUNT;    //REI_LoadActionType
} REI_RenderPassDesc;

typedef struct REI_FrameBufferDesc
{
    VkRenderPass  renderPass;
    REI_Texture** ppRenderTargets;
    REI_Texture*  pDepthStencil;
    uint32_t*     pColorArraySlices;
    uint32_t*     pColorMipSlices;
    uint32_t      depthArraySlice;
    uint32_t      depthMipSlice;
    uint32_t      renderTargetCount;
} FrameBufferDesc;

typedef struct REI_FrameBuffer
{
    VkFramebuffer pFramebuffer;
    uint32_t      width;
    uint32_t      height;
    uint32_t      arraySize;
} REI_FrameBuffer;

#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
using RenderPassMap = REI_unordered_map<uint64_t, std::pair<VkRenderPass, VkRenderPass>>;
#else
using RenderPassMap = REI_unordered_map<uint64_t, VkRenderPass>;
#endif
using RenderPassMapNode = RenderPassMap::value_type;
using FrameBufferMap = REI_unordered_map<uint64_t, struct REI_FrameBuffer*>;
using FrameBufferMapNode = FrameBufferMap::value_type;

using DescriptorNameToIndexMap = REI_unordered_map<REI_string, uint32_t>;
typedef struct REI_BindingInfo REI_BindingInfo;

typedef struct REI_DescriptorTableArray
{
    void*                    scratchMem;
    size_t                   scratchMemSize;
    VkDescriptorSet*         pHandles;
    REI_BindingInfo*         pDescriptorBindings;
    VkWriteDescriptorSet*    pWriteDescriptorSets;
    uint32_t                 numDescriptors;
    uint32_t                 maxTables;
    size_t                   poolIndex;
    uint8_t                  slot;
} REI_DescriptorTableArray;

typedef struct REI_Renderer
{
    inline REI_Renderer(const REI_AllocatorCallbacks& inAllocatorCallbacks): allocator(inAllocatorCallbacks) {}

    uint32_t                    numOfDevices;
    VkInstance                  pVkInstance;
    VkPhysicalDevice            pVkPhysicalDevice;
    VkPhysicalDeviceProperties2 vkDeviceProperties;
    uint32_t                    vkQueueFamilyCount;
    VkQueueFamilyProperties     vkQueueFamilyProperties[REI_VK_MAX_QUEUE_FAMILY_COUNT];
    VkDevice                    pVkDevice;
    VkDescriptorSetLayout       pVkEmptyDescriptorSetLayout;
    VkDescriptorSet             pVkEmptyDescriptorSet;

    uint32_t hasDebugUtilsExtension : 1;
    uint32_t hasRenderDocLayerEnabled : 1;
    uint32_t hasDedicatedAllocationExtension : 1;
    uint32_t hasExternalMemoryExtension : 1;
    uint32_t hasDrawIndirectCountExtension : 1;
    uint32_t hasDescriptorIndexingExtension : 1;
    uint32_t hasDebugMarkerExtension : 1;
    uint32_t has4444FormatsExtension : 1;
    uint32_t hasPipelineCreationCacheControlExtension : 1;

    // TODO: make runtime configurable
#if USE_DEBUG_UTILS_EXTENSION
    VkDebugUtilsMessengerEXT pVkDebugUtilsMessenger;
#else
    VkDebugReportCallbackEXT            pVkDebugReport;
#endif
    PFN_vkGetPhysicalDeviceFeatures2KHR pfn_vkGetPhysicalDeviceFeatures2KHR;

    PFN_vkCmdBindVertexBuffers2EXT pfn_vkCmdBindVertexBuffers2EXT;
#if !USE_DEBUG_UTILS_EXTENSION
    PFN_vkDebugMarkerSetObjectNameEXT pfn_vkDebugMarkerSetObjectNameEXT;
    PFN_vkCmdDebugMarkerBeginEXT      pfn_vkCmdDebugMarkerBeginEXT;
    PFN_vkCmdDebugMarkerEndEXT        pfn_vkCmdDebugMarkerEndEXT;
    PFN_vkCmdDebugMarkerInsertEXT     pfn_vkCmdDebugMarkerInsertEXT;
#else
    PFN_vkCreateDebugUtilsMessengerEXT  pfn_vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT pfn_vkDestroyDebugUtilsMessengerEXT;
    PFN_vkSetDebugUtilsObjectNameEXT    pfn_vkSetDebugUtilsObjectNameEXT;
    PFN_vkCmdBeginDebugUtilsLabelEXT    pfn_vkCmdBeginDebugUtilsLabelEXT;
    PFN_vkCmdEndDebugUtilsLabelEXT      pfn_vkCmdEndDebugUtilsLabelEXT;
    PFN_vkCmdInsertDebugUtilsLabelEXT   pfn_vkCmdInsertDebugUtilsLabelEXT;
#endif
    PFN_vkCmdDrawIndirectCountKHR        pfn_VkCmdDrawIndirectCountKHR = NULL;
    PFN_vkCmdDrawIndexedIndirectCountKHR pfn_VkCmdDrawIndexedIndirectCountKHR = NULL;

    struct REI_DescriptorPool* pDescriptorPool;
    struct VmaAllocator_T*     pVmaAllocator;
    REI_AllocatorCallbacks     allocator;
    REI_LogPtr                 pLog;

    REI_atomicptr_t textureIds = 0;

    uint32_t vkUsedQueueCount[REI_VK_MAX_QUEUE_FAMILY_COUNT];
} REI_Renderer;

typedef struct REI_Queue
{
    REI_Renderer* pRenderer;
    VkQueue       pVkQueue;
    uint32_t      vkQueueFamilyIndex;
    uint32_t      vkQueueIndex;
    REI_QueueDesc queueDesc;
} REI_Queue;

typedef struct REI_QueryPool
{
    REI_QueryPoolDesc desc;
    VkQueryPool       pVkQueryPool;
} REI_QueryPool;

typedef struct REI_Fence
{
    VkFence pVkFence;
    bool    submitted;
} REI_Fence;

typedef struct REI_Semaphore
{
    VkSemaphore pVkSemaphore;
    bool        signaled;
} REI_Semaphore;

typedef struct REI_Shader
{
    VkShaderStageFlagBits  stage;
    char*                  pEntryPoint;
    VkShaderModule         shaderModule;
} REI_Shader;

typedef struct REI_PipelineCache
{
    VkPipelineCache pCache;
} REI_PipelineCache;

typedef struct REI_Sampler
{
    /// Native handle of the underlying resource
    VkSampler pVkSampler;
    /// Description for creating the descriptor for this sampler
    VkDescriptorImageInfo vkSamplerView;
} REI_Sampler;

typedef struct REI_RootSignature
{
    /// Number of descriptors declared in the root signature layout
    uint32_t              descriptorCount;
    REI_BindingInfo*      pDescriptorBindings;
    VkPipelineBindPoint   pipelineType;
    uint32_t              mDescriptorIndexToBindingOffset[REI_DESCRIPTOR_TABLE_SLOT_COUNT];
    VkDescriptorSetLayout vkDescriptorSetLayouts[REI_DESCRIPTOR_TABLE_SLOT_COUNT];
    uint32_t              vkCumulativeDescriptorCounts[REI_DESCRIPTOR_TABLE_SLOT_COUNT];
    VkPipelineLayout      pPipelineLayout;
    uint32_t              vkPushConstantCount;
    uint32_t              mStaticSamplerSlot;
    VkDescriptorSet       vkStaticSamplerSet;
    size_t                mStaticSamplerSetPoolIndex;
    uint32_t              mMaxUsedSlots;
} REI_RootSignature;

typedef struct REI_Pipeline
{
    VkPipelineBindPoint      type;
    VkPipeline               pVkPipeline;
    const REI_RootSignature* pRootSignature;
} REI_Pipeline;

typedef struct REI_Buffer
{
    /// CPU address of the mapped buffer (appliacable to buffers created in CPU accessible heaps (CPU, CPU_TO_GPU, GPU_TO_CPU)
    void* pCpuMappedAddress;
    /// Native handle of the underlying resource
    VkBuffer pVkBuffer;
    /// REI_Buffer view
    VkBufferView pVkStorageTexelView;
    VkBufferView pVkUniformTexelView;
    /// Contains resource allocation info such as parent heap, offset in heap
    struct VmaAllocation_T* pVkAllocation;
    /// Description for creating the descriptor for this buffer (applicable to REI_BUFFER_USAGE_UNIFORM,REI_BUFFER_USAGE_STORAGE_SRV, REI_BUFFER_USAGE_STORAGE_UAV)
    VkDescriptorBufferInfo vkBufferInfo;
    /// REI_Buffer creation info
    REI_BufferDesc desc;
} REI_Buffer;

typedef struct REI_Texture
{
    /// Opaque handle used by shaders for doing read/write operations on the texture
    VkImageView pVkSRVDescriptor;
    /// Opaque handle used by shaders for doing read/write operations on the texture
    VkImageView* pVkUAVDescriptors;
    /// Opaque handle used by shaders for doing read/write operations on the texture
    VkImageView* pVkSRVStencilDescriptor;
    VkImageView* pVkRTDescriptors;
    /// Native handle of the underlying resource
    VkImage pVkImage;
    /// Contains resource allocation info such as parent heap, offset in heap
    struct VmaAllocation_T* pVkAllocation;
    uint64_t                textureId;
    /// Flags specifying which aspects (COLOR,DEPTH,STENCIL) are included in the pVkImageView
    VkImageAspectFlags vkAspectMask;
    /// REI_Texture creation info
    REI_TextureDesc desc;    //88
    /// This value will be false if the underlying resource is not owned by the texture (swapchain textures,...)
    bool ownsImage;
} REI_Texture;

typedef struct REI_CmdPool
{
    REI_CmdPoolType cmdPoolType;
    VkCommandPool   pVkCmdPool;
} REI_CmdPool;

typedef struct REI_Cmd
{
    inline REI_Cmd(const REI_AllocatorCallbacks& inAllocatorCallbacks):
        renderPassMap(REI_allocator<RenderPassMap>(inAllocatorCallbacks)),
        frameBufferMap(REI_allocator<FrameBufferMap>(inAllocatorCallbacks))
    {
    }

    REI_Renderer* pRenderer;
    REI_CmdPool*  pCmdPool;

    const REI_RootSignature* pBoundRootSignature;
    VkCommandBuffer          pVkCmdBuf;

    RenderPassMap  renderPassMap;
    FrameBufferMap frameBufferMap;

    struct DirtyState
    {
        VkRenderPass pVkActiveRenderPass;
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
        VkRenderPass pVkRestoreRenderPass;
#endif
        VkRenderPassBeginInfo mRenderPassBeginInfo;
        VkClearValue          clearValues[REI_MAX_RENDER_TARGET_ATTACHMENTS + 1];

        uint32_t beginRenderPassDirty : 1;
    } mDirtyState;
} REI_Cmd;

typedef struct REI_CommandSignature
{
    uint32_t                 indirectArgDescCounts;
    uint32_t                 drawCommandStride;
    REI_IndirectArgumentType drawType;
} REI_CommandSignature;

typedef struct REI_Swapchain
{
    REI_SwapchainDesc             desc;
    const REI_AllocatorCallbacks* pAllocator;
    /// Render targets created from the swapchain back buffers
    REI_Texture** ppSwapchainTextures;
    /// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
    VkQueue        pPresentQueue;
    VkSwapchainKHR pSwapchain;
    VkSurfaceKHR   pVkSurface;
    VkImage*       ppVkSwapchainImages;
} REI_Swapchain;

void REI_initRendererVk(const REI_RendererDescVk* pDescVk, REI_Renderer** ppRenderer);
void REI_cmdBindDescriptorTableVK(
    REI_Cmd* pCmd, uint32_t tableIndex, REI_DescriptorTableArray* pDescriptorTableArr, uint32_t dynamicOffsetCount,
    const uint32_t* pDynamicOffsets);