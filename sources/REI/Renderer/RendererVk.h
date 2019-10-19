#pragma once

#include "Renderer.h"
#include "../Interface/Thread.h"

#include <string>
#include <vector>
#include <unordered_map>

#if defined(_WIN32)
#    define VK_USE_PLATFORM_WIN32_KHR
#endif
#if defined(__ANDROID__)
#    ifndef VK_USE_PLATFORM_ANDROID_KHR
#        define VK_USE_PLATFORM_ANDROID_KHR
#    endif
#elif defined(__linux__)
#    define VK_USE_PLATFORM_XLIB_KHR    //Use Xlib or Xcb as display server, defaults to Xlib
#endif
#include <vulkan/vulkan.h>

enum
{
    REI_VK_MAX_QUEUE_FAMILY_COUNT = 16,
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
} REI_RendererDescVk;

typedef struct REI_RenderPassDesc
{
    REI_Format*               pColorFormats;
    const REI_LoadActionType* pLoadActionsColor;
    uint32_t                  renderTargetCount;
    REI_SampleCount           sampleCount;
    REI_Format                depthStencilFormat;
    REI_LoadActionType        loadActionDepth;
    REI_LoadActionType        loadActionStencil;
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

using RenderPassMap = std::unordered_map<uint64_t, VkRenderPass>;
using RenderPassMapNode = RenderPassMap::value_type;
using FrameBufferMap = std::unordered_map<uint64_t, struct REI_FrameBuffer*>;
using FrameBufferMapNode = FrameBufferMap::value_type;

using DescriptorNameToIndexMap = std::unordered_map<std::string, uint32_t>;

union DescriptorUpdateData
{
    VkDescriptorImageInfo  mImageInfo;
    VkDescriptorBufferInfo mBufferInfo;
    VkBufferView           mBuferView;
};

typedef struct REI_DescriptorSet
{
    VkDescriptorSet*         pHandles;
    const REI_RootSignature* pRootSignature;
    DescriptorUpdateData**   ppUpdateData;
    uint32_t**               pDynamicOffsets;
    uint32_t**               pDynamicSizes;
    uint32_t                 maxSets;
    uint8_t                  dynamicOffsetCount;
    uint8_t                  setIndex;
    uint8_t                  pad[2];
} REI_DescriptorSet;

typedef struct REI_Renderer
{
    uint32_t                    numOfDevices;
    VkInstance                  pVkInstance;
    VkPhysicalDevice            pVkPhysicalDevice;
    VkPhysicalDeviceProperties2 vkDeviceProperties;
    uint32_t                    vkQueueFamilyCount;
    VkQueueFamilyProperties     vkQueueFamilyProperties[REI_VK_MAX_QUEUE_FAMILY_COUNT];
    VkDevice                    pVkDevice;

    bool hasDebugUtilsExtension : 1;
    bool hasRenderDocLayerEnabled : 1;
    bool hasDedicatedAllocationExtension : 1;
    bool hasExternalMemoryExtension : 1;
    bool hasDrawIndirectCountExtension : 1;
    bool hasDescriptorIndexingExtension : 1;
    bool hasDebugMarkerExtension : 1;

    // TODO: make runtime configurable
#ifdef USE_DEBUG_UTILS_EXTENSION
    VkDebugUtilsMessengerEXT pVkDebugUtilsMessenger;
#else
    VkDebugReportCallbackEXT pVkDebugReport;
#endif
    PFN_vkGetPhysicalDeviceFeatures2KHR pfn_vkGetPhysicalDeviceFeatures2KHR;

    PFN_vkCreateDescriptorUpdateTemplateKHR  pfn_vkCreateDescriptorUpdateTemplateKHR;
    PFN_vkDestroyDescriptorUpdateTemplateKHR pfn_vkDestroyDescriptorUpdateTemplateKHR;
    PFN_vkUpdateDescriptorSetWithTemplateKHR pfn_vkUpdateDescriptorSetWithTemplateKHR;
#ifndef USE_DEBUG_UTILS_EXTENSION
    PFN_vkDebugMarkerSetObjectNameEXT pfn_vkDebugMarkerSetObjectNameEXT;
    PFN_vkCmdDebugMarkerBeginEXT      pfn_vkCmdDebugMarkerBeginEXT;
    PFN_vkCmdDebugMarkerEndEXT        pfn_vkCmdDebugMarkerEndEXT;
    PFN_vkCmdDebugMarkerInsertEXT     pfn_vkCmdDebugMarkerInsertEXT;
#endif
    PFN_vkCmdDrawIndirectCountKHR        pfn_VkCmdDrawIndirectCountKHR = NULL;
    PFN_vkCmdDrawIndexedIndirectCountKHR pfn_VkCmdDrawIndexedIndirectCountKHR = NULL;

    REI_Texture* pDefaultTextureSRV[REI_TEXTURE_DIM_COUNT];
    REI_Texture* pDefaultTextureUAV[REI_TEXTURE_DIM_COUNT];
    REI_Buffer*  pDefaultBufferSRV;
    REI_Buffer*  pDefaultBufferUAV;
    REI_Sampler* pDefaultSampler;

    struct REI_DescriptorPool* pDescriptorPool;
    struct VmaAllocator_T*     pVmaAllocator;

    REI_atomicptr_t textureIds = 0;

    // REI_RenderPass map per thread (this will make lookups lock free and we only need a lock when inserting a REI_RenderPass Map for the first time)
    std::unordered_map<ThreadID, RenderPassMap> renderPassMap;
    // REI_FrameBuffer map per thread (this will make lookups lock free and we only need a lock when inserting a REI_FrameBuffer map for the first time)
    std::unordered_map<ThreadID, FrameBufferMap> frameBufferMap;
    Mutex                                        renderPassMutex;

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
    uint32_t               stages;
    REI_PipelineReflection reflection;
    VkShaderModule*        pShaderModules;
} REI_Shader;

typedef struct REI_Sampler
{
    /// Native handle of the underlying resource
    VkSampler pVkSampler;
    /// Description for creating the descriptor for this sampler
    VkDescriptorImageInfo vkSamplerView;
} REI_Sampler;

/// Data structure holding the layout for a descriptor
typedef struct REI_DescriptorInfo
{
    /// Binding information generated from the shader reflection
    REI_ShaderResource desc;
    /// Index in the descriptor set
    uint32_t indexInParent;
    /// Update frequency of this descriptor
    REI_DescriptorSetIndex setIndex;
    uint32_t               handleIndex;
    VkDescriptorType       vkType;
    VkShaderStageFlags     vkStages;
    uint32_t               dynamicUniformIndex;
} REI_DescriptorInfo;

typedef struct REI_RootSignature
{
    /// Number of descriptors declared in the root signature layout
    uint32_t descriptorCount;
    /// Array of all descriptors declared in the root signature layout
    REI_DescriptorInfo* pDescriptors;
    /// Translates hash of descriptor name to descriptor index
    // TODO: rework
    std::unordered_map<std::string, uint32_t> pDescriptorNameToIndexMap;

    REI_PipelineType           pipelineType;
    VkDescriptorSetLayout      vkDescriptorSetLayouts[REI_DESCRIPTOR_SET_COUNT];
    uint32_t                   vkDescriptorCounts[REI_DESCRIPTOR_SET_COUNT];
    uint32_t                   vkDynamicDescriptorCounts[REI_DESCRIPTOR_SET_COUNT];
    uint32_t                   vkCumulativeDescriptorCounts[REI_DESCRIPTOR_SET_COUNT];
    VkPushConstantRange*       pVkPushConstantRanges;
    VkPipelineLayout           pPipelineLayout;
    VkDescriptorUpdateTemplate updateTemplates[REI_DESCRIPTOR_SET_COUNT];
    VkDescriptorSet            vkEmptyDescriptorSets[REI_DESCRIPTOR_SET_COUNT];
    void*                      pUpdateTemplateData[REI_DESCRIPTOR_SET_COUNT];
    uint32_t                   vkPushConstantCount;
} REI_RootSignature;

typedef struct REI_Pipeline
{
    REI_PipelineType type;
    VkPipeline       pVkPipeline;
} REI_Pipeline;

typedef struct REI_Buffer
{
    /// Position of dynamic buffer memory in the mapped resource
    uint64_t positionInHeap;
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
    REI_Renderer* pRenderer;
    REI_CmdPool*  pCmdPool;

    const REI_RootSignature* pBoundRootSignature;
    VkCommandBuffer          pVkCmdBuf;
    VkRenderPass             pVkActiveRenderPass;
} REI_Cmd;

typedef struct REI_CommandSignature
{
    uint32_t                 indirectArgDescCounts;
    uint32_t                 drawCommandStride;
    REI_IndirectArgumentType drawType;
} REI_CommandSignature;

typedef struct REI_Swapchain
{
    REI_SwapchainDesc desc;
    /// Render targets created from the swapchain back buffers
    REI_Texture** ppSwapchainTextures;
    /// Present queue if one exists (queuePresent will use this queue if the hardware has a dedicated present queue)
    VkQueue        pPresentQueue;
    VkSwapchainKHR pSwapchain;
    VkSurfaceKHR   pVkSurface;
    VkImage*       ppVkSwapchainImages;
} REI_Swapchain;

void REI_initRendererVk(const REI_RendererDescVk* pDescVk, REI_Renderer** ppRenderer);
