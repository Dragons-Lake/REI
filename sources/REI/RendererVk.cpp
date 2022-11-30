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

#define _CRT_SECURE_NO_WARNINGS

#include "RendererVk.h"

#include <algorithm>

#define VMA_IMPLEMENTATION

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wunused-variable"
#    pragma clang diagnostic ignored "-Wswitch"
#    pragma clang diagnostic ignored "-Wunused-private-field"
#endif

#include "3rdParty/VulkanMemoryAllocator/VulkanMemoryAllocator.h"

#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

#include "Common.h"
#include <float.h>

void     vk_platfom_get_wanted_instance_layers(uint32_t* outLayerCount, const char*** outLayerNames);
void     vk_platfom_get_wanted_instance_extensions(uint32_t* outExtensionCount, const char*** outExtensionNames);
void     vk_platfom_get_wanted_device_extensions(uint32_t* outExtensionCount, const char*** outExtensionNames);
VkResult vk_platform_create_surface(
    VkInstance instance, const REI_WindowHandle* pNativeWindow, const VkAllocationCallbacks* pAllocator,
    VkSurfaceKHR* pSurface);

// clang-format off
VkBlendOp gVkBlendOpTranslator[REI_MAX_BLEND_MODES] =
{
	VK_BLEND_OP_ADD,
	VK_BLEND_OP_SUBTRACT,
	VK_BLEND_OP_REVERSE_SUBTRACT,
	VK_BLEND_OP_MIN,
	VK_BLEND_OP_MAX,
};

VkBlendFactor gVkBlendConstantTranslator[REI_MAX_BLEND_CONSTANTS] =
{
	VK_BLEND_FACTOR_ZERO,
	VK_BLEND_FACTOR_ONE,
	VK_BLEND_FACTOR_SRC_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
	VK_BLEND_FACTOR_DST_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
	VK_BLEND_FACTOR_SRC_ALPHA,
	VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	VK_BLEND_FACTOR_DST_ALPHA,
	VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
	VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
	VK_BLEND_FACTOR_CONSTANT_COLOR,
	VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
};

VkCompareOp gVkComparisonFuncTranslator[REI_MAX_COMPARE_MODES] =
{
	VK_COMPARE_OP_NEVER,
	VK_COMPARE_OP_LESS,
	VK_COMPARE_OP_EQUAL,
	VK_COMPARE_OP_LESS_OR_EQUAL,
	VK_COMPARE_OP_GREATER,
	VK_COMPARE_OP_NOT_EQUAL,
	VK_COMPARE_OP_GREATER_OR_EQUAL,
	VK_COMPARE_OP_ALWAYS,
};

VkStencilOp gVkStencilOpTranslator[REI_MAX_STENCIL_OPS] =
{
	VK_STENCIL_OP_KEEP,
	VK_STENCIL_OP_ZERO,
	VK_STENCIL_OP_REPLACE,
	VK_STENCIL_OP_INVERT,
	VK_STENCIL_OP_INCREMENT_AND_WRAP,
	VK_STENCIL_OP_DECREMENT_AND_WRAP,
	VK_STENCIL_OP_INCREMENT_AND_CLAMP,
	VK_STENCIL_OP_DECREMENT_AND_CLAMP,
};

VkCullModeFlagBits gVkCullModeTranslator[REI_MAX_CULL_MODES] =
{
	VK_CULL_MODE_NONE,
	VK_CULL_MODE_BACK_BIT,
	VK_CULL_MODE_FRONT_BIT
};

VkPolygonMode gVkFillModeTranslator[REI_MAX_FILL_MODES] =
{
	VK_POLYGON_MODE_FILL,
	VK_POLYGON_MODE_LINE
};

VkFrontFace gVkFrontFaceTranslator[] =
{
	VK_FRONT_FACE_COUNTER_CLOCKWISE,
	VK_FRONT_FACE_CLOCKWISE
};

VkAttachmentLoadOp gVkAttachmentLoadOpTranslator[REI_MAX_LOAD_ACTION] = 
{
	VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	VK_ATTACHMENT_LOAD_OP_LOAD,
	VK_ATTACHMENT_LOAD_OP_CLEAR,
};

// clang-format on

// TODO: remove
#ifdef _MSC_VER
#    pragma comment(lib, "vulkan-1.lib")
#endif

#if defined(__cplusplus)
#    define DECLARE_ZERO(type, var) type var = {};
#else
#    define DECLARE_ZERO(type, var) type var = { 0 };
#endif

// Internal utility functions (may become external one day)
static VkSampleCountFlagBits util_to_vk_sample_count(uint32_t sampleCount);
static REI_Format            util_from_vk_format(REI_Renderer* pRenderer, VkFormat format);
static VkFormat              util_to_vk_format(REI_Renderer* pRenderer, uint32_t format);

static const uint32_t gDescriptorTypeRangeSize = (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1);
static const char*    gDefaultEntryPointName = "main";

/************************************************************************/
// REI_BindingInfo Structure
/************************************************************************/
typedef struct REI_BindingInfo
{
    uint32_t binding;
    uint32_t firstArrayElement;
} REI_BindingInfo;

/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
typedef struct REI_DescriptorPoolContainer
{
    VkDescriptorPool pVkPool;
    bool             mIsFull;

} REI_DescriptorPoolContainer;

typedef struct REI_DescriptorPool
{
    REI_DescriptorPool(const REI_AllocatorCallbacks& allocator):
        descriptorPools(REI_allocator<VkDescriptorPool>(allocator)), mutex()
    {
    }

    VkDevice                                pDevice;
    VkDescriptorPoolSize*                   pPoolSizes;
    REI_vector<REI_DescriptorPoolContainer> descriptorPools;
    uint32_t                                poolSizeCount;
    uint32_t                                numDescriptorSets;
    VkDescriptorPoolCreateFlags             flags;
    Mutex                                   mutex;
} REI_DescriptorPool;

/************************************************************************/
// Static REI_DescriptorInfo Heap Implementation
/************************************************************************/
static void add_descriptor_pool(
    REI_Renderer* pRenderer, uint32_t numDescriptorSets, VkDescriptorPoolCreateFlags flags,
    VkDescriptorPoolSize* pPoolSizes, uint32_t numPoolSizes, REI_DescriptorPool** ppPool)
{
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    REI_StackAllocator<false> persistentAlloc = { 0 };

    persistentAlloc.reserve<REI_DescriptorPool>().reserve<VkDescriptorPoolSize>(numPoolSizes);

    if (!persistentAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "add_descriptor_pool wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        *ppPool = nullptr;
        return;
    }

    REI_DescriptorPool* pPool = persistentAlloc.constructZeroed<REI_DescriptorPool>(allocator);
    pPool->flags = flags;
    pPool->numDescriptorSets = numDescriptorSets;
    pPool->pDevice = pRenderer->pVkDevice;

    pPool->poolSizeCount = numPoolSizes;
    pPool->pPoolSizes = persistentAlloc.alloc<VkDescriptorPoolSize>(numPoolSizes);
    if (numPoolSizes)
        memcpy(pPool->pPoolSizes, pPoolSizes, numPoolSizes * sizeof(VkDescriptorPoolSize));

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = NULL;
    poolCreateInfo.poolSizeCount = numPoolSizes;
    poolCreateInfo.pPoolSizes = pPoolSizes;
    poolCreateInfo.flags = flags;
    poolCreateInfo.maxSets = numDescriptorSets;

    REI_DescriptorPoolContainer currentPoolContainer = { 0 };

    VkResult res = vkCreateDescriptorPool(pPool->pDevice, &poolCreateInfo, NULL, &currentPoolContainer.pVkPool);
    REI_ASSERT(VK_SUCCESS == res);

    pPool->descriptorPools.emplace_back(currentPoolContainer);

    *ppPool = pPool;
}

static void remove_descriptor_pool(REI_Renderer* pRenderer, REI_DescriptorPool* pPool)
{
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    for (uint32_t i = 0; i < (uint32_t)pPool->descriptorPools.size(); ++i)
        vkDestroyDescriptorPool(pRenderer->pVkDevice, pPool->descriptorPools[i].pVkPool, NULL);

    pPool->descriptorPools.~vector();

    pPool->mutex.~Mutex();
    allocator.pFree(allocator.pUserData, pPool);
}

static void consume_descriptor_sets(
    REI_DescriptorPool* pPool, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet* pSets,
    uint32_t numDescriptorSets, size_t* pOutPoolIndex)
{
    DECLARE_ZERO(VkDescriptorSetAllocateInfo, alloc_info);
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.descriptorSetCount = numDescriptorSets;
    alloc_info.pSetLayouts = pLayouts;

    // Need a lock since vkAllocateDescriptorSets needs to be externally synchronized
    // This is fine since this will only happen during Init time
    MutexLock lock(pPool->mutex);

    for (size_t i = 0; i < pPool->descriptorPools.size(); ++i) 
    {
        REI_DescriptorPoolContainer& currentCont = pPool->descriptorPools[i];
        if (!currentCont.mIsFull) 
        {
            alloc_info.descriptorPool = currentCont.pVkPool;
            VkResult vk_res = vkAllocateDescriptorSets(pPool->pDevice, &alloc_info, pSets);
            if (vk_res == VK_SUCCESS) 
            {
                if (pOutPoolIndex)
                    *pOutPoolIndex = i;
                return;
            }
            else {
                currentCont.mIsFull = true;
            }
        }
    }

    REI_DescriptorPoolContainer descriptorPoolContainer = { 0 };

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = NULL;
    poolCreateInfo.poolSizeCount = pPool->poolSizeCount;
    poolCreateInfo.pPoolSizes = pPool->pPoolSizes;
    poolCreateInfo.flags = pPool->flags;
    poolCreateInfo.maxSets = pPool->numDescriptorSets;

    VkResult res = vkCreateDescriptorPool(pPool->pDevice, &poolCreateInfo, NULL, &descriptorPoolContainer.pVkPool);
    REI_ASSERT(VK_SUCCESS == res);

    alloc_info.descriptorPool = descriptorPoolContainer.pVkPool;
    res = vkAllocateDescriptorSets(pPool->pDevice, &alloc_info, pSets);
    REI_ASSERT(VK_SUCCESS == res);

    pPool->descriptorPools.emplace_back(descriptorPoolContainer);
    if (pOutPoolIndex)
        *pOutPoolIndex = pPool->descriptorPools.size() - 1;
}

static void return_descriptor_sets(REI_DescriptorPool* pPool, size_t poolIndex, uint32_t descriptorSetCount, const VkDescriptorSet* pDescriptorSets)
{ 
    REI_ASSERT(poolIndex < pPool->descriptorPools.size());

    REI_DescriptorPoolContainer& poolCont = pPool->descriptorPools[poolIndex];

    MutexLock lock(pPool->mutex);
    VkResult  res = vkFreeDescriptorSets(pPool->pDevice, poolCont.pVkPool, descriptorSetCount, pDescriptorSets);
    REI_ASSERT(res == VK_SUCCESS);
    poolCont.mIsFull = false;
}

/************************************************************************/
/************************************************************************/

uint64_t fnv_64a(uint8_t* buf, size_t len, uint64_t hval = 0xCBF29CE484222325ull)
{
    while (len--)
    {
        hval ^= (uint64_t)*buf++;
        hval *= 0x100000001B3ull;
    }

    return hval;
}

void REI_getDeviceProperties(REI_Renderer* pRenderer, REI_DeviceProperties* outProperties)
{
    memset(&outProperties->capabilities, 0, sizeof(outProperties->capabilities));

    VkPhysicalDeviceProperties2        vkDeviceProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR };
    VkPhysicalDeviceSubgroupProperties subgroupProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES };

    vkDeviceProperties.pNext = &subgroupProperties;
    vkGetPhysicalDeviceProperties2(pRenderer->pVkPhysicalDevice, &vkDeviceProperties);

    VkPhysicalDeviceFeatures2KHR vkDeviceFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };

#if VK_EXT_fragment_shader_interlock
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT
    };
    vkDeviceFeatures.pNext = &fragmentShaderInterlockFeatures;
#endif
    pRenderer->pfn_vkGetPhysicalDeviceFeatures2KHR(pRenderer->pVkPhysicalDevice, &vkDeviceFeatures);

    outProperties->capabilities.uniformBufferAlignment =
        (uint32_t)vkDeviceProperties.properties.limits.minUniformBufferOffsetAlignment;
    outProperties->capabilities.uploadBufferTextureAlignment =
        (uint32_t)vkDeviceProperties.properties.limits.optimalBufferCopyOffsetAlignment;
    outProperties->capabilities.uploadBufferTextureRowAlignment =
        (uint32_t)vkDeviceProperties.properties.limits.optimalBufferCopyRowPitchAlignment;
    outProperties->capabilities.maxVertexInputBindings = vkDeviceProperties.properties.limits.maxVertexInputBindings;
    outProperties->capabilities.multiDrawIndirect = vkDeviceProperties.properties.limits.maxDrawIndirectCount > 1;
    outProperties->capabilities.waveLaneCount = subgroupProperties.subgroupSize;
#if VK_EXT_fragment_shader_interlock
    outProperties->capabilities.ROVsSupported = (bool)fragmentShaderInterlockFeatures.fragmentShaderPixelInterlock;
#endif

    //save vendor and model Id as string
    sprintf(outProperties->modelId, "%#x", vkDeviceProperties.properties.deviceID);
    sprintf(outProperties->vendorId, "%#x", vkDeviceProperties.properties.vendorID);
    strncpy(outProperties->deviceName, vkDeviceProperties.properties.deviceName, REI_MAX_GPU_VENDOR_STRING_LENGTH);
    //TODO: Fix once vulkan adds support for revision ID
    strncpy(outProperties->revisionId, "0x00", REI_MAX_GPU_VENDOR_STRING_LENGTH);

    for (uint32_t i = 0; i < REI_FMT_COUNT; ++i)
    {
        VkFormatProperties formatSupport;
        VkFormat           fmt = util_to_vk_format(pRenderer, i);
        if (fmt == VK_FORMAT_UNDEFINED)
            continue;

        vkGetPhysicalDeviceFormatProperties(pRenderer->pVkPhysicalDevice, fmt, &formatSupport);
        outProperties->capabilities.canShaderReadFrom[i] =
            (formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
        outProperties->capabilities.canShaderWriteTo[i] =
            (formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
        outProperties->capabilities.canRenderTargetWriteTo[i] =
            (formatSupport.optimalTilingFeatures &
             (VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0;
    }
}

// add_render_pass funtion accepts stack allocator from caller, use this funtion to allocate enough space in that allocator 
constexpr uint32_t util_add_render_pass_stack_size_in_bytes() 
{
    return (
        (REI_MAX_RENDER_TARGET_ATTACHMENTS + 1) * (sizeof(VkAttachmentDescription) + sizeof(VkAttachmentReference)) +
        sizeof(VkSubpassDescription) + sizeof(VkRenderPassCreateInfo));
}

static void add_render_pass(
    REI_StackAllocator<false>& stackAlloc, REI_Renderer* pRenderer,
    const REI_RenderPassDesc* pDesc, VkRenderPass* pRenderPass)
{
    /************************************************************************/
    // Add render pass
    /************************************************************************/
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    uint32_t colorAttachmentCount = pDesc->renderTargetCount;
    uint32_t depthAttachmentCount = (pDesc->depthStencilFormat != REI_FMT_UNDEFINED) ? 1 : 0;

    VkAttachmentDescription* attachments = NULL;
    VkAttachmentReference*   color_attachment_refs = NULL;
    VkAttachmentReference*   depth_stencil_attachment_ref = NULL;

    VkSampleCountFlagBits sample_count = util_to_vk_sample_count(pDesc->sampleCount);

    // Fill out attachment descriptions and references
    {
        attachments = stackAlloc.alloc<VkAttachmentDescription>(colorAttachmentCount + depthAttachmentCount);

        if (colorAttachmentCount > 0)
        {
            color_attachment_refs = stackAlloc.alloc<VkAttachmentReference>(colorAttachmentCount);
        }
        if (depthAttachmentCount > 0)
        {
            depth_stencil_attachment_ref = stackAlloc.alloc<VkAttachmentReference>(depthAttachmentCount);
        }

        // Color
        for (uint32_t i = 0; i < colorAttachmentCount; ++i)
        {
            const uint32_t ssidx = i;

            // descriptions
            attachments[ssidx].flags = 0;
            attachments[ssidx].format = util_to_vk_format(pRenderer, pDesc->pColorFormats[i]);
            attachments[ssidx].samples = sample_count;
            attachments[ssidx].loadOp = pDesc->pLoadActionsColor
                                            ? gVkAttachmentLoadOpTranslator[pDesc->pLoadActionsColor[i]]
                                            : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[ssidx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[ssidx].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[ssidx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[ssidx].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachments[ssidx].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

            // references
            color_attachment_refs[i].attachment = ssidx;
            color_attachment_refs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
    }

    // Depth stencil
    if (depthAttachmentCount > 0)
    {
        uint32_t idx = colorAttachmentCount;
        attachments[idx].flags = 0;
        attachments[idx].format = util_to_vk_format(pRenderer, pDesc->depthStencilFormat);
        attachments[idx].samples = sample_count;
        attachments[idx].loadOp = gVkAttachmentLoadOpTranslator[pDesc->loadActionDepth];
        attachments[idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[idx].stencilLoadOp = gVkAttachmentLoadOpTranslator[pDesc->loadActionStencil];
        attachments[idx].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[idx].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments[idx].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_stencil_attachment_ref[0].attachment = idx;
        depth_stencil_attachment_ref[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription& subpass = *stackAlloc.alloc<VkSubpassDescription>();
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = colorAttachmentCount;
    subpass.pColorAttachments = color_attachment_refs;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = depth_stencil_attachment_ref;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    uint32_t attachment_count = colorAttachmentCount;
    attachment_count += depthAttachmentCount;

    VkRenderPassCreateInfo& create_info = *stackAlloc.alloc<VkRenderPassCreateInfo>();
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.attachmentCount = attachment_count;
    create_info.pAttachments = attachments;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 0;
    create_info.pDependencies = NULL;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkResult     vk_res = vkCreateRenderPass(pRenderer->pVkDevice, &create_info, NULL, &renderPass);
    REI_ASSERT(VK_SUCCESS == vk_res);

    *pRenderPass = renderPass;
}

constexpr uint32_t util_add_framebuffer_stack_size_in_bytes() 
{
    return ((REI_MAX_RENDER_TARGET_ATTACHMENTS + 1) * sizeof(VkImageView) + sizeof(VkFramebufferCreateInfo));
}

static void add_framebuffer(REI_StackAllocator<false>& stackAlloc, REI_Renderer* pRenderer, const REI_FrameBufferDesc* pDesc, REI_FrameBuffer** ppFrameBuffer)
{
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    REI_FrameBuffer* pFrameBuffer = (REI_FrameBuffer*)REI_calloc(allocator, sizeof(*pFrameBuffer));
    REI_ASSERT(pFrameBuffer);

    uint32_t colorAttachmentCount = pDesc->renderTargetCount;
    uint32_t depthAttachmentCount = (pDesc->pDepthStencil) ? 1 : 0;

    if (colorAttachmentCount)
    {
        pFrameBuffer->width = pDesc->ppRenderTargets[0]->desc.width;
        pFrameBuffer->height = pDesc->ppRenderTargets[0]->desc.height;
        if (pDesc->pColorArraySlices)
            pFrameBuffer->arraySize = 1;
        else
            pFrameBuffer->arraySize = pDesc->ppRenderTargets[0]->desc.arraySize;
    }
    else
    {
        pFrameBuffer->width = pDesc->pDepthStencil->desc.width;
        pFrameBuffer->height = pDesc->pDepthStencil->desc.height;
        if (pDesc->depthArraySlice != -1)
            pFrameBuffer->arraySize = 1;
        else
            pFrameBuffer->arraySize = pDesc->pDepthStencil->desc.arraySize;
    }

    /************************************************************************/
    // Add frame buffer
    /************************************************************************/
    uint32_t attachment_count = colorAttachmentCount;
    attachment_count += depthAttachmentCount;

    VkImageView* pImageViews = stackAlloc.alloc<VkImageView>(attachment_count);
    REI_ASSERT(pImageViews);

    VkImageView* pView = pImageViews;
    // Color
    for (uint32_t i = 0; i < pDesc->renderTargetCount; ++i)
    {
        REI_TextureDesc& tdesc = pDesc->ppRenderTargets[i]->desc;
        uint32_t         mipLevel = pDesc->pColorMipSlices ? pDesc->pColorArraySlices[i] : 0;
        uint32_t         arrayLayer = pDesc->pColorArraySlices ? pDesc->pColorArraySlices[i] : 0;
        uint32_t         depthOrArraySize = 1;

        if ((tdesc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (tdesc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            depthOrArraySize *= tdesc.arraySize * tdesc.depth;
        }

        uint32_t handle = mipLevel * depthOrArraySize + arrayLayer;

        *pView++ = pDesc->ppRenderTargets[i]->pVkRTDescriptors[handle];
    }
    // Depth/stencil
    if (pDesc->pDepthStencil)
    {
        REI_TextureDesc& tdesc = pDesc->pDepthStencil->desc;
        uint32_t         depthOrArraySize = 1;

        if ((tdesc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (tdesc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            depthOrArraySize *= tdesc.arraySize * tdesc.depth;
        }

        uint32_t handle = pDesc->depthMipSlice * depthOrArraySize + pDesc->depthArraySlice;

        *pView++ = pDesc->pDepthStencil->pVkRTDescriptors[handle];
    }

    VkFramebufferCreateInfo& add_info = *stackAlloc.alloc<VkFramebufferCreateInfo>();
    add_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.renderPass = pDesc->renderPass;
    add_info.attachmentCount = attachment_count;
    add_info.pAttachments = pImageViews;
    add_info.width = pFrameBuffer->width;
    add_info.height = pFrameBuffer->height;
    add_info.layers = pFrameBuffer->arraySize;
    VkResult vk_res = vkCreateFramebuffer(pRenderer->pVkDevice, &add_info, NULL, &(pFrameBuffer->pFramebuffer));
    REI_ASSERT(VK_SUCCESS == vk_res);
    /************************************************************************/
    /************************************************************************/

    *ppFrameBuffer = pFrameBuffer;
}

static void remove_framebuffer(REI_Renderer* pRenderer, REI_FrameBuffer* pFrameBuffer)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pFrameBuffer);

    vkDestroyFramebuffer(pRenderer->pVkDevice, pFrameBuffer->pFramebuffer, NULL);
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pFrameBuffer);
}

/************************************************************************/
// Logging, Validation layer implementation
/************************************************************************/

#if USE_DEBUG_UTILS_EXTENSION
// Debug callback for Vulkan layers
static VkBool32 VKAPI_PTR internal_debug_report_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    REI_LogPtr pLog = reinterpret_cast<REI_LogPtr>(pUserData);

    const char* pLayerPrefix = pCallbackData->pMessageIdName;
    const char* pMessage = pCallbackData->pMessage;
    int32_t     messageCode = pCallbackData->messageIdNumber;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        pLog(REI_LOG_TYPE_INFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        // Code 64 is vkCmdClearAttachments issued before any draws
        // We ignore this since we dont use load store actions
        // Instead we clear the attachments in the DirectX way
        if (messageCode == 64)
            return VK_FALSE;

        pLog(REI_LOG_TYPE_WARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        pLog(REI_LOG_TYPE_ERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }

    return VK_FALSE;
}
#else
static VKAPI_ATTR VkBool32 VKAPI_CALL internal_debug_report_callback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location,
    int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    REI_LogPtr pLog = reinterpret_cast<REI_LogPtr>(pUserData);

    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        pLog(REI_LOG_TYPE_INFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        // Vulkan SDK 1.0.68 fixes the Dedicated memory binding validation error bugs
#    if VK_HEADER_VERSION < 68
        // Disable warnings for bind memory for dedicated allocations extension
        if (gDedicatedAllocationExtension && messageCode != 11 && messageCode != 12)
#    endif
            pLog(REI_LOG_TYPE_WARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        pLog(REI_LOG_TYPE_WARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & (VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_ERROR_VALIDATION_FAILED_EXT))
    {
        pLog(REI_LOG_TYPE_ERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }

    return VK_FALSE;
}
#endif

/************************************************************************/
// Internal utility functions
/************************************************************************/
VkFilter util_to_vk_filter(uint32_t filter)
{
    REI_ASSERT(filter < REI_FILTER_TYPE_COUNT, "invalid filter type");
    switch (filter)
    {
        case REI_FILTER_NEAREST: return VK_FILTER_NEAREST;
        case REI_FILTER_LINEAR: return VK_FILTER_LINEAR;
        default: return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode util_to_vk_mip_map_mode(uint32_t mipMapMode)
{
    REI_ASSERT(mipMapMode < REI_MIPMAP_MODE_COUNT, "Invalid Mip Map Mode");
    switch (mipMapMode)
    {
        case REI_MIPMAP_MODE_NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case REI_MIPMAP_MODE_LINEAR: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default: REI_ASSERT(false && "Invalid Mip Map Mode"); return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}

VkSamplerAddressMode util_to_vk_address_mode(uint32_t addressMode)
{
    REI_ASSERT(addressMode < REI_ADDRESS_MODE_COUNT, "Invalid address mode"); 
    switch (addressMode)
    {
        case REI_ADDRESS_MODE_MIRROR: return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        case REI_ADDRESS_MODE_REPEAT: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case REI_ADDRESS_MODE_CLAMP_TO_EDGE: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case REI_ADDRESS_MODE_CLAMP_TO_BORDER: return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        default: return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}

VkShaderStageFlags util_to_vk_shader_stages(REI_ShaderStage shader_stages)
{
    VkShaderStageFlags result = 0;
    if (REI_SHADER_STAGE_ALL_GRAPHICS == (shader_stages & REI_SHADER_STAGE_ALL_GRAPHICS))
    {
        result = VK_SHADER_STAGE_ALL_GRAPHICS;
    }
    else
    {
        if (REI_SHADER_STAGE_VERT == (shader_stages & REI_SHADER_STAGE_VERT))
        {
            result |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (REI_SHADER_STAGE_TESC == (shader_stages & REI_SHADER_STAGE_TESC))
        {
            result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        }
        if (REI_SHADER_STAGE_TESE == (shader_stages & REI_SHADER_STAGE_TESE))
        {
            result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        }
        if (REI_SHADER_STAGE_GEOM == (shader_stages & REI_SHADER_STAGE_GEOM))
        {
            result |= VK_SHADER_STAGE_GEOMETRY_BIT;
        }
        if (REI_SHADER_STAGE_FRAG == (shader_stages & REI_SHADER_STAGE_FRAG))
        {
            result |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        if (REI_SHADER_STAGE_COMP == (shader_stages & REI_SHADER_STAGE_COMP))
        {
            result |= VK_SHADER_STAGE_COMPUTE_BIT;
        }
    }
    return result;
}

VkSampleCountFlagBits util_to_vk_sample_count(uint32_t sampleCount)
{
    VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;
    switch (sampleCount)
    {
        case 0:
        case REI_SAMPLE_COUNT_1: result = VK_SAMPLE_COUNT_1_BIT; break;
        case REI_SAMPLE_COUNT_2: result = VK_SAMPLE_COUNT_2_BIT; break;
        case REI_SAMPLE_COUNT_4: result = VK_SAMPLE_COUNT_4_BIT; break;
        case REI_SAMPLE_COUNT_8: result = VK_SAMPLE_COUNT_8_BIT; break;
        case REI_SAMPLE_COUNT_16: result = VK_SAMPLE_COUNT_16_BIT; break;
        default: REI_ASSERT(false, "invalid sampleCount"); break;
    }
    return result;
}

VkBufferUsageFlags util_to_vk_buffer_usage(uint32_t usage, bool typed)
{
    VkBufferUsageFlags result = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (usage & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        result |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (usage & REI_DESCRIPTOR_TYPE_RW_BUFFER)
    {
        result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (typed)
            result |= VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;
    }
    if (usage & REI_DESCRIPTOR_TYPE_BUFFER)
    {
        result |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if (typed)
            result |= VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT;
    }
    if (usage & REI_DESCRIPTOR_TYPE_INDEX_BUFFER)
    {
        result |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (usage & REI_DESCRIPTOR_TYPE_VERTEX_BUFFER)
    {
        result |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (usage & REI_DESCRIPTOR_TYPE_INDIRECT_BUFFER)
    {
        result |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    return result;
}

VkImageUsageFlags util_to_vk_image_usage(uint32_t usage)
{
    VkImageUsageFlags result = 0;
    if (REI_DESCRIPTOR_TYPE_TEXTURE == (usage & REI_DESCRIPTOR_TYPE_TEXTURE))
        result |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (REI_DESCRIPTOR_TYPE_RW_TEXTURE == (usage & REI_DESCRIPTOR_TYPE_RW_TEXTURE))
        result |= VK_IMAGE_USAGE_STORAGE_BIT;
    return result;
}

VkAccessFlags util_to_vk_access_flags(REI_ResourceState state)
{
    VkAccessFlags ret = 0;
    if (state & (REI_RESOURCE_STATE_COPY_SOURCE | REI_RESOURCE_STATE_RESOLVE_SOURCE))
    {
        ret |= VK_ACCESS_TRANSFER_READ_BIT;
    }
    if (state & (REI_RESOURCE_STATE_COPY_DEST | REI_RESOURCE_STATE_RESOLVE_DEST))
    {
        ret |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
    if (state & REI_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
    {
        ret |= VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
    if (state & REI_RESOURCE_STATE_INDEX_BUFFER)
    {
        ret |= VK_ACCESS_INDEX_READ_BIT;
    }
    if (state & REI_RESOURCE_STATE_UNORDERED_ACCESS)
    {
        ret |= VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    }
    if (state & REI_RESOURCE_STATE_INDIRECT_ARGUMENT)
    {
        ret |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }
    if (state & REI_RESOURCE_STATE_RENDER_TARGET)
    {
        ret |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
    if (state & REI_RESOURCE_STATE_DEPTH_WRITE)
    {
        ret |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    if (state & (REI_RESOURCE_STATE_SHADER_RESOURCE | REI_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
    {
        ret |= VK_ACCESS_SHADER_READ_BIT;
    }
    if (state & REI_RESOURCE_STATE_PRESENT)
    {
        ret |= VK_ACCESS_MEMORY_READ_BIT;
    }

    return ret;
}

VkImageLayout util_to_vk_image_layout(REI_ResourceState usage)
{
    if (usage & (REI_RESOURCE_STATE_COPY_SOURCE | REI_RESOURCE_STATE_RESOLVE_SOURCE))
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

    if (usage & (REI_RESOURCE_STATE_COPY_DEST | REI_RESOURCE_STATE_RESOLVE_DEST))
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    if (usage & REI_RESOURCE_STATE_RENDER_TARGET)
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    if (usage & REI_RESOURCE_STATE_DEPTH_WRITE)
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    if (usage & REI_RESOURCE_STATE_UNORDERED_ACCESS)
        return VK_IMAGE_LAYOUT_GENERAL;

    if ((usage & REI_RESOURCE_STATE_SHADER_RESOURCE) || (usage & REI_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (usage & REI_RESOURCE_STATE_PRESENT)
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    if (usage == REI_RESOURCE_STATE_COMMON)
        return VK_IMAGE_LAYOUT_GENERAL;

    return VK_IMAGE_LAYOUT_UNDEFINED;
}

static VkStencilFaceFlags util_to_vk_stencil_face_mask(REI_StencilFaceMask face)
{
    REI_ASSERT(face < REI_STENCIL_FACE_MASK_COUNT, "Invalid stencil face mask");
    switch (face)
    {
        case REI_STENCIL_FACE_FRONT: return VK_STENCIL_FACE_FRONT_BIT;
        case REI_STENCIL_FACE_BACK: return VK_STENCIL_FACE_BACK_BIT;
        case REI_STENCIL_FACE_FRONT_AND_BACK: return VK_STENCIL_FRONT_AND_BACK;
        default: REI_ASSERT(0);
    }
    return VK_STENCIL_FRONT_AND_BACK;
}

// Determines pipeline stages involved for given accesses
static VkPipelineStageFlags util_determine_pipeline_stage_flags(VkAccessFlags accessFlags, REI_CmdPoolType cmdPoolType)
{
    VkPipelineStageFlags flags = 0;

    switch (cmdPoolType)
    {
        case REI_CMD_POOL_DIRECT:
        case REI_CMD_POOL_BUNDLE:
        {
            if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;

            if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) !=
                0)
            {
                flags |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT;
                flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }

            if ((accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0)
                flags |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

            if ((accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            if ((accessFlags &
                 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
                flags |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            break;
        }
        case REI_CMD_POOL_COMPUTE:
        {
            if ((accessFlags & (VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT)) != 0 ||
                (accessFlags & VK_ACCESS_INPUT_ATTACHMENT_READ_BIT) != 0 ||
                (accessFlags & (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) != 0 ||
                (accessFlags &
                 (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) != 0)
                return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

            if ((accessFlags & (VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT)) !=
                0)
                flags |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

            break;
        }
        case REI_CMD_POOL_COPY: return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        default: break;
    }

    // Compatible with both compute and graphics queues
    if ((accessFlags & VK_ACCESS_INDIRECT_COMMAND_READ_BIT) != 0)
        flags |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;

    if ((accessFlags & (VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT)) != 0)
        flags |= VK_PIPELINE_STAGE_TRANSFER_BIT;

    if ((accessFlags & (VK_ACCESS_HOST_READ_BIT | VK_ACCESS_HOST_WRITE_BIT)) != 0)
        flags |= VK_PIPELINE_STAGE_HOST_BIT;

    if (flags == 0)
        flags = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    return flags;
}

inline VkFormat util_to_vk_format(REI_Renderer* pRenderer, uint32_t fmt)
{
    REI_ASSERT(fmt < REI_FMT_COUNT, "Invalid format");
    switch (fmt)
    {
        case REI_FMT_UNDEFINED: return VK_FORMAT_UNDEFINED;
        case REI_FMT_G4R4_UNORM: return VK_FORMAT_R4G4_UNORM_PACK8;
        case REI_FMT_A4B4G4R4_UNORM: return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        case REI_FMT_A4R4G4B4_UNORM: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
#if VK_EXT_4444_formats
        case REI_FMT_B4G4R4A4_UNORM:
            if (pRenderer->has4444FormatsExtension)
                return VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT;
            else
                return VK_FORMAT_UNDEFINED;
#endif
        case REI_FMT_R5G6B5_UNORM: return VK_FORMAT_B5G6R5_UNORM_PACK16;
        case REI_FMT_B5G6R5_UNORM: return VK_FORMAT_R5G6B5_UNORM_PACK16;
        case REI_FMT_A1B5G5R5_UNORM: return VK_FORMAT_R5G5B5A1_UNORM_PACK16;
        case REI_FMT_A1R5G5B5_UNORM: return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
        case REI_FMT_B5G5R5A1_UNORM: return VK_FORMAT_A1R5G5B5_UNORM_PACK16;

        case REI_FMT_R8_UNORM: return VK_FORMAT_R8_UNORM;
        case REI_FMT_R8_SNORM: return VK_FORMAT_R8_SNORM;
        case REI_FMT_R8_UINT: return VK_FORMAT_R8_UINT;
        case REI_FMT_R8_SINT: return VK_FORMAT_R8_SINT;
        case REI_FMT_R8_SRGB: return VK_FORMAT_R8_SRGB;
        case REI_FMT_R8G8_UNORM: return VK_FORMAT_R8G8_UNORM;
        case REI_FMT_R8G8_SNORM: return VK_FORMAT_R8G8_SNORM;
        case REI_FMT_R8G8_UINT: return VK_FORMAT_R8G8_UINT;
        case REI_FMT_R8G8_SINT: return VK_FORMAT_R8G8_SINT;
        case REI_FMT_R8G8_SRGB: return VK_FORMAT_R8G8_SRGB;
        case REI_FMT_R8G8B8_UNORM: return VK_FORMAT_R8G8B8_UNORM;
        case REI_FMT_R8G8B8_SNORM: return VK_FORMAT_R8G8B8_SNORM;
        case REI_FMT_R8G8B8_UINT: return VK_FORMAT_R8G8B8_UINT;
        case REI_FMT_R8G8B8_SINT: return VK_FORMAT_R8G8B8_SINT;
        case REI_FMT_R8G8B8_SRGB: return VK_FORMAT_R8G8B8_SRGB;
        case REI_FMT_B8G8R8_UNORM: return VK_FORMAT_B8G8R8_UNORM;
        case REI_FMT_B8G8R8_SNORM: return VK_FORMAT_B8G8R8_SNORM;
        case REI_FMT_B8G8R8_UINT: return VK_FORMAT_B8G8R8_UINT;
        case REI_FMT_B8G8R8_SINT: return VK_FORMAT_B8G8R8_SINT;
        case REI_FMT_B8G8R8_SRGB: return VK_FORMAT_B8G8R8_SRGB;
        case REI_FMT_R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case REI_FMT_R8G8B8A8_SNORM: return VK_FORMAT_R8G8B8A8_SNORM;
        case REI_FMT_R8G8B8A8_UINT: return VK_FORMAT_R8G8B8A8_UINT;
        case REI_FMT_R8G8B8A8_SINT: return VK_FORMAT_R8G8B8A8_SINT;
        case REI_FMT_R8G8B8A8_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case REI_FMT_B8G8R8A8_UNORM: return VK_FORMAT_B8G8R8A8_UNORM;
        case REI_FMT_B8G8R8A8_SNORM: return VK_FORMAT_B8G8R8A8_SNORM;
        case REI_FMT_B8G8R8A8_UINT: return VK_FORMAT_B8G8R8A8_UINT;
        case REI_FMT_B8G8R8A8_SINT: return VK_FORMAT_B8G8R8A8_SINT;
        case REI_FMT_B8G8R8A8_SRGB: return VK_FORMAT_B8G8R8A8_SRGB;
        case REI_FMT_R16_UNORM: return VK_FORMAT_R16_UNORM;
        case REI_FMT_R16_SNORM: return VK_FORMAT_R16_SNORM;
        case REI_FMT_R16_UINT: return VK_FORMAT_R16_UINT;
        case REI_FMT_R16_SINT: return VK_FORMAT_R16_SINT;
        case REI_FMT_R16_SFLOAT: return VK_FORMAT_R16_SFLOAT;
        case REI_FMT_R16G16_UNORM: return VK_FORMAT_R16G16_UNORM;
        case REI_FMT_R16G16_SNORM: return VK_FORMAT_R16G16_SNORM;
        case REI_FMT_R16G16_UINT: return VK_FORMAT_R16G16_UINT;
        case REI_FMT_R16G16_SINT: return VK_FORMAT_R16G16_SINT;
        case REI_FMT_R16G16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
        case REI_FMT_R16G16B16_UNORM: return VK_FORMAT_R16G16B16_UNORM;
        case REI_FMT_R16G16B16_SNORM: return VK_FORMAT_R16G16B16_SNORM;
        case REI_FMT_R16G16B16_UINT: return VK_FORMAT_R16G16B16_UINT;
        case REI_FMT_R16G16B16_SINT: return VK_FORMAT_R16G16B16_SINT;
        case REI_FMT_R16G16B16_SFLOAT: return VK_FORMAT_R16G16B16_SFLOAT;
        case REI_FMT_R16G16B16A16_UNORM: return VK_FORMAT_R16G16B16A16_UNORM;
        case REI_FMT_R16G16B16A16_SNORM: return VK_FORMAT_R16G16B16A16_SNORM;
        case REI_FMT_R16G16B16A16_UINT: return VK_FORMAT_R16G16B16A16_UINT;
        case REI_FMT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_SINT;
        case REI_FMT_R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case REI_FMT_R32_UINT: return VK_FORMAT_R32_UINT;
        case REI_FMT_R32_SINT: return VK_FORMAT_R32_SINT;
        case REI_FMT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
        case REI_FMT_R32G32_UINT: return VK_FORMAT_R32G32_UINT;
        case REI_FMT_R32G32_SINT: return VK_FORMAT_R32G32_SINT;
        case REI_FMT_R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case REI_FMT_R32G32B32_UINT: return VK_FORMAT_R32G32B32_UINT;
        case REI_FMT_R32G32B32_SINT: return VK_FORMAT_R32G32B32_SINT;
        case REI_FMT_R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case REI_FMT_R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        case REI_FMT_R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_SINT;
        case REI_FMT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case REI_FMT_R64_UINT: return VK_FORMAT_R64_UINT;
        case REI_FMT_R64_SINT: return VK_FORMAT_R64_SINT;
        case REI_FMT_R64_SFLOAT: return VK_FORMAT_R64_SFLOAT;
        case REI_FMT_R64G64_UINT: return VK_FORMAT_R64G64_UINT;
        case REI_FMT_R64G64_SINT: return VK_FORMAT_R64G64_SINT;
        case REI_FMT_R64G64_SFLOAT: return VK_FORMAT_R64G64_SFLOAT;
        case REI_FMT_R64G64B64_UINT: return VK_FORMAT_R64G64B64_UINT;
        case REI_FMT_R64G64B64_SINT: return VK_FORMAT_R64G64B64_SINT;
        case REI_FMT_R64G64B64_SFLOAT: return VK_FORMAT_R64G64B64_SFLOAT;
        case REI_FMT_R64G64B64A64_UINT: return VK_FORMAT_R64G64B64A64_UINT;
        case REI_FMT_R64G64B64A64_SINT: return VK_FORMAT_R64G64B64A64_SINT;
        case REI_FMT_R64G64B64A64_SFLOAT: return VK_FORMAT_R64G64B64A64_SFLOAT;

        case REI_FMT_B10G10R10A2_UNORM: return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
        case REI_FMT_B10G10R10A2_UINT: return VK_FORMAT_A2R10G10B10_UINT_PACK32;
        case REI_FMT_R10G10B10A2_UNORM: return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case REI_FMT_R10G10B10A2_UINT: return VK_FORMAT_A2B10G10R10_UINT_PACK32;

        case REI_FMT_B10G11R11_UFLOAT: return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case REI_FMT_E5B9G9R9_UFLOAT: return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;

        case REI_FMT_D16_UNORM: return VK_FORMAT_D16_UNORM;
        case REI_FMT_X8_D24_UNORM: return VK_FORMAT_X8_D24_UNORM_PACK32;
        case REI_FMT_D32_SFLOAT: return VK_FORMAT_D32_SFLOAT;
        case REI_FMT_S8_UINT: return VK_FORMAT_S8_UINT;
        case REI_FMT_D16_UNORM_S8_UINT: return VK_FORMAT_D16_UNORM_S8_UINT;
        case REI_FMT_D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case REI_FMT_D32_SFLOAT_S8_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case REI_FMT_DXBC1_RGB_UNORM: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case REI_FMT_DXBC1_RGB_SRGB: return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
        case REI_FMT_DXBC1_RGBA_UNORM: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case REI_FMT_DXBC1_RGBA_SRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
        case REI_FMT_DXBC2_UNORM: return VK_FORMAT_BC2_UNORM_BLOCK;
        case REI_FMT_DXBC2_SRGB: return VK_FORMAT_BC2_SRGB_BLOCK;
        case REI_FMT_DXBC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
        case REI_FMT_DXBC3_SRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
        case REI_FMT_DXBC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
        case REI_FMT_DXBC4_SNORM: return VK_FORMAT_BC4_SNORM_BLOCK;
        case REI_FMT_DXBC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
        case REI_FMT_DXBC5_SNORM: return VK_FORMAT_BC5_SNORM_BLOCK;
        case REI_FMT_DXBC6H_UFLOAT: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case REI_FMT_DXBC6H_SFLOAT: return VK_FORMAT_BC6H_SFLOAT_BLOCK;
        case REI_FMT_DXBC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
        case REI_FMT_DXBC7_SRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
        case REI_FMT_PVRTC1_2BPP_UNORM: return VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG;
        case REI_FMT_PVRTC1_4BPP_UNORM: return VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG;
        case REI_FMT_PVRTC1_2BPP_SRGB: return VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG;
        case REI_FMT_PVRTC1_4BPP_SRGB: return VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG;
        case REI_FMT_ETC2_R8G8B8_UNORM: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case REI_FMT_ETC2_R8G8B8A1_UNORM: return VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK;
        case REI_FMT_ETC2_R8G8B8A8_UNORM: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        case REI_FMT_ETC2_R8G8B8_SRGB: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
        case REI_FMT_ETC2_R8G8B8A1_SRGB: return VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK;
        case REI_FMT_ETC2_R8G8B8A8_SRGB: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
        case REI_FMT_ETC2_EAC_R11_UNORM: return VK_FORMAT_EAC_R11_UNORM_BLOCK;
        case REI_FMT_ETC2_EAC_R11G11_UNORM: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
        case REI_FMT_ETC2_EAC_R11_SNORM: return VK_FORMAT_EAC_R11_SNORM_BLOCK;
        case REI_FMT_ETC2_EAC_R11G11_SNORM: return VK_FORMAT_EAC_R11G11_SNORM_BLOCK;
        case REI_FMT_ASTC_4x4_UNORM: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        case REI_FMT_ASTC_4x4_SRGB: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
        case REI_FMT_ASTC_5x4_UNORM: return VK_FORMAT_ASTC_5x4_UNORM_BLOCK;
        case REI_FMT_ASTC_5x4_SRGB: return VK_FORMAT_ASTC_5x4_SRGB_BLOCK;
        case REI_FMT_ASTC_5x5_UNORM: return VK_FORMAT_ASTC_5x5_UNORM_BLOCK;
        case REI_FMT_ASTC_5x5_SRGB: return VK_FORMAT_ASTC_5x5_SRGB_BLOCK;
        case REI_FMT_ASTC_6x5_UNORM: return VK_FORMAT_ASTC_6x5_UNORM_BLOCK;
        case REI_FMT_ASTC_6x5_SRGB: return VK_FORMAT_ASTC_6x5_SRGB_BLOCK;
        case REI_FMT_ASTC_6x6_UNORM: return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        case REI_FMT_ASTC_6x6_SRGB: return VK_FORMAT_ASTC_6x6_SRGB_BLOCK;
        case REI_FMT_ASTC_8x5_UNORM: return VK_FORMAT_ASTC_8x5_UNORM_BLOCK;
        case REI_FMT_ASTC_8x5_SRGB: return VK_FORMAT_ASTC_8x5_SRGB_BLOCK;
        case REI_FMT_ASTC_8x6_UNORM: return VK_FORMAT_ASTC_8x6_UNORM_BLOCK;
        case REI_FMT_ASTC_8x6_SRGB: return VK_FORMAT_ASTC_8x6_SRGB_BLOCK;
        case REI_FMT_ASTC_8x8_UNORM: return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
        case REI_FMT_ASTC_8x8_SRGB: return VK_FORMAT_ASTC_8x8_SRGB_BLOCK;
        case REI_FMT_ASTC_10x5_UNORM: return VK_FORMAT_ASTC_10x5_UNORM_BLOCK;
        case REI_FMT_ASTC_10x5_SRGB: return VK_FORMAT_ASTC_10x5_SRGB_BLOCK;
        case REI_FMT_ASTC_10x6_UNORM: return VK_FORMAT_ASTC_10x6_UNORM_BLOCK;
        case REI_FMT_ASTC_10x6_SRGB: return VK_FORMAT_ASTC_10x6_SRGB_BLOCK;
        case REI_FMT_ASTC_10x8_UNORM: return VK_FORMAT_ASTC_10x8_UNORM_BLOCK;
        case REI_FMT_ASTC_10x8_SRGB: return VK_FORMAT_ASTC_10x8_SRGB_BLOCK;
        case REI_FMT_ASTC_10x10_UNORM: return VK_FORMAT_ASTC_10x10_UNORM_BLOCK;
        case REI_FMT_ASTC_10x10_SRGB: return VK_FORMAT_ASTC_10x10_SRGB_BLOCK;
        case REI_FMT_ASTC_12x10_UNORM: return VK_FORMAT_ASTC_12x10_UNORM_BLOCK;
        case REI_FMT_ASTC_12x10_SRGB: return VK_FORMAT_ASTC_12x10_SRGB_BLOCK;
        case REI_FMT_ASTC_12x12_UNORM: return VK_FORMAT_ASTC_12x12_UNORM_BLOCK;
        case REI_FMT_ASTC_12x12_SRGB: return VK_FORMAT_ASTC_12x12_SRGB_BLOCK;
        case REI_FMT_PVRTC2_2BPP_UNORM: return VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG;
        case REI_FMT_PVRTC2_4BPP_UNORM: return VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG;
        case REI_FMT_PVRTC2_2BPP_SRGB: return VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG;
        case REI_FMT_PVRTC2_4BPP_SRGB: return VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG;

        default: return VK_FORMAT_UNDEFINED;
    }
    return VK_FORMAT_UNDEFINED;
}

inline REI_Format util_from_vk_format(REI_Renderer* pRenderer, VkFormat fmt)
{
    switch (fmt)
    {
        case VK_FORMAT_UNDEFINED: return REI_FMT_UNDEFINED;
        case VK_FORMAT_R4G4_UNORM_PACK8: return REI_FMT_G4R4_UNORM;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return REI_FMT_A4B4G4R4_UNORM;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return REI_FMT_A4R4G4B4_UNORM;
#if VK_EXT_4444_formats
        case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT:
            if (pRenderer->has4444FormatsExtension)
                return REI_FMT_B4G4R4A4_UNORM;
            else
                return REI_FMT_UNDEFINED;
#endif
        case VK_FORMAT_R5G6B5_UNORM_PACK16: return REI_FMT_B5G6R5_UNORM;
        case VK_FORMAT_B5G6R5_UNORM_PACK16: return REI_FMT_R5G6B5_UNORM;
        case VK_FORMAT_R5G5B5A1_UNORM_PACK16: return REI_FMT_A1B5G5R5_UNORM;
        case VK_FORMAT_B5G5R5A1_UNORM_PACK16: return REI_FMT_A1R5G5B5_UNORM;
        case VK_FORMAT_A1R5G5B5_UNORM_PACK16: return REI_FMT_B5G5R5A1_UNORM;
        case VK_FORMAT_R8_UNORM: return REI_FMT_R8_UNORM;
        case VK_FORMAT_R8_SNORM: return REI_FMT_R8_SNORM;
        case VK_FORMAT_R8_UINT: return REI_FMT_R8_UINT;
        case VK_FORMAT_R8_SINT: return REI_FMT_R8_SINT;
        case VK_FORMAT_R8_SRGB: return REI_FMT_R8_SRGB;
        case VK_FORMAT_R8G8_UNORM: return REI_FMT_R8G8_UNORM;
        case VK_FORMAT_R8G8_SNORM: return REI_FMT_R8G8_SNORM;
        case VK_FORMAT_R8G8_UINT: return REI_FMT_R8G8_UINT;
        case VK_FORMAT_R8G8_SINT: return REI_FMT_R8G8_SINT;
        case VK_FORMAT_R8G8_SRGB: return REI_FMT_R8G8_SRGB;
        case VK_FORMAT_R8G8B8_UNORM: return REI_FMT_R8G8B8_UNORM;
        case VK_FORMAT_R8G8B8_SNORM: return REI_FMT_R8G8B8_SNORM;
        case VK_FORMAT_R8G8B8_UINT: return REI_FMT_R8G8B8_UINT;
        case VK_FORMAT_R8G8B8_SINT: return REI_FMT_R8G8B8_SINT;
        case VK_FORMAT_R8G8B8_SRGB: return REI_FMT_R8G8B8_SRGB;
        case VK_FORMAT_B8G8R8_UNORM: return REI_FMT_B8G8R8_UNORM;
        case VK_FORMAT_B8G8R8_SNORM: return REI_FMT_B8G8R8_SNORM;
        case VK_FORMAT_B8G8R8_UINT: return REI_FMT_B8G8R8_UINT;
        case VK_FORMAT_B8G8R8_SINT: return REI_FMT_B8G8R8_SINT;
        case VK_FORMAT_B8G8R8_SRGB: return REI_FMT_B8G8R8_SRGB;
        case VK_FORMAT_R8G8B8A8_UNORM: return REI_FMT_R8G8B8A8_UNORM;
        case VK_FORMAT_R8G8B8A8_SNORM: return REI_FMT_R8G8B8A8_SNORM;
        case VK_FORMAT_R8G8B8A8_UINT: return REI_FMT_R8G8B8A8_UINT;
        case VK_FORMAT_R8G8B8A8_SINT: return REI_FMT_R8G8B8A8_SINT;
        case VK_FORMAT_R8G8B8A8_SRGB: return REI_FMT_R8G8B8A8_SRGB;
        case VK_FORMAT_B8G8R8A8_UNORM: return REI_FMT_B8G8R8A8_UNORM;
        case VK_FORMAT_B8G8R8A8_SNORM: return REI_FMT_B8G8R8A8_SNORM;
        case VK_FORMAT_B8G8R8A8_UINT: return REI_FMT_B8G8R8A8_UINT;
        case VK_FORMAT_B8G8R8A8_SINT: return REI_FMT_B8G8R8A8_SINT;
        case VK_FORMAT_B8G8R8A8_SRGB: return REI_FMT_B8G8R8A8_SRGB;
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32: return REI_FMT_B10G10R10A2_UNORM;
        case VK_FORMAT_A2R10G10B10_UINT_PACK32: return REI_FMT_B10G10R10A2_UINT;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: return REI_FMT_R10G10B10A2_UNORM;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: return REI_FMT_R10G10B10A2_UINT;
        case VK_FORMAT_R16_UNORM: return REI_FMT_R16_UNORM;
        case VK_FORMAT_R16_SNORM: return REI_FMT_R16_SNORM;
        case VK_FORMAT_R16_UINT: return REI_FMT_R16_UINT;
        case VK_FORMAT_R16_SINT: return REI_FMT_R16_SINT;
        case VK_FORMAT_R16_SFLOAT: return REI_FMT_R16_SFLOAT;
        case VK_FORMAT_R16G16_UNORM: return REI_FMT_R16G16_UNORM;
        case VK_FORMAT_R16G16_SNORM: return REI_FMT_R16G16_SNORM;
        case VK_FORMAT_R16G16_UINT: return REI_FMT_R16G16_UINT;
        case VK_FORMAT_R16G16_SINT: return REI_FMT_R16G16_SINT;
        case VK_FORMAT_R16G16_SFLOAT: return REI_FMT_R16G16_SFLOAT;
        case VK_FORMAT_R16G16B16_UNORM: return REI_FMT_R16G16B16_UNORM;
        case VK_FORMAT_R16G16B16_SNORM: return REI_FMT_R16G16B16_SNORM;
        case VK_FORMAT_R16G16B16_UINT: return REI_FMT_R16G16B16_UINT;
        case VK_FORMAT_R16G16B16_SINT: return REI_FMT_R16G16B16_SINT;
        case VK_FORMAT_R16G16B16_SFLOAT: return REI_FMT_R16G16B16_SFLOAT;
        case VK_FORMAT_R16G16B16A16_UNORM: return REI_FMT_R16G16B16A16_UNORM;
        case VK_FORMAT_R16G16B16A16_SNORM: return REI_FMT_R16G16B16A16_SNORM;
        case VK_FORMAT_R16G16B16A16_UINT: return REI_FMT_R16G16B16A16_UINT;
        case VK_FORMAT_R16G16B16A16_SINT: return REI_FMT_R16G16B16A16_SINT;
        case VK_FORMAT_R16G16B16A16_SFLOAT: return REI_FMT_R16G16B16A16_SFLOAT;
        case VK_FORMAT_R32_UINT: return REI_FMT_R32_UINT;
        case VK_FORMAT_R32_SINT: return REI_FMT_R32_SINT;
        case VK_FORMAT_R32_SFLOAT: return REI_FMT_R32_SFLOAT;
        case VK_FORMAT_R32G32_UINT: return REI_FMT_R32G32_UINT;
        case VK_FORMAT_R32G32_SINT: return REI_FMT_R32G32_SINT;
        case VK_FORMAT_R32G32_SFLOAT: return REI_FMT_R32G32_SFLOAT;
        case VK_FORMAT_R32G32B32_UINT: return REI_FMT_R32G32B32_UINT;
        case VK_FORMAT_R32G32B32_SINT: return REI_FMT_R32G32B32_SINT;
        case VK_FORMAT_R32G32B32_SFLOAT: return REI_FMT_R32G32B32_SFLOAT;
        case VK_FORMAT_R32G32B32A32_UINT: return REI_FMT_R32G32B32A32_UINT;
        case VK_FORMAT_R32G32B32A32_SINT: return REI_FMT_R32G32B32A32_SINT;
        case VK_FORMAT_R32G32B32A32_SFLOAT: return REI_FMT_R32G32B32A32_SFLOAT;
        case VK_FORMAT_R64_UINT: return REI_FMT_R64_UINT;
        case VK_FORMAT_R64_SINT: return REI_FMT_R64_SINT;
        case VK_FORMAT_R64_SFLOAT: return REI_FMT_R64_SFLOAT;
        case VK_FORMAT_R64G64_UINT: return REI_FMT_R64G64_UINT;
        case VK_FORMAT_R64G64_SINT: return REI_FMT_R64G64_SINT;
        case VK_FORMAT_R64G64_SFLOAT: return REI_FMT_R64G64_SFLOAT;
        case VK_FORMAT_R64G64B64_UINT: return REI_FMT_R64G64B64_UINT;
        case VK_FORMAT_R64G64B64_SINT: return REI_FMT_R64G64B64_SINT;
        case VK_FORMAT_R64G64B64_SFLOAT: return REI_FMT_R64G64B64_SFLOAT;
        case VK_FORMAT_R64G64B64A64_UINT: return REI_FMT_R64G64B64A64_UINT;
        case VK_FORMAT_R64G64B64A64_SINT: return REI_FMT_R64G64B64A64_SINT;
        case VK_FORMAT_R64G64B64A64_SFLOAT: return REI_FMT_R64G64B64A64_SFLOAT;
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32: return REI_FMT_B10G11R11_UFLOAT;
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32: return REI_FMT_E5B9G9R9_UFLOAT;
        case VK_FORMAT_D16_UNORM: return REI_FMT_D16_UNORM;
        case VK_FORMAT_X8_D24_UNORM_PACK32: return REI_FMT_X8_D24_UNORM;
        case VK_FORMAT_D32_SFLOAT: return REI_FMT_D32_SFLOAT;
        case VK_FORMAT_S8_UINT: return REI_FMT_S8_UINT;
        case VK_FORMAT_D16_UNORM_S8_UINT: return REI_FMT_D16_UNORM_S8_UINT;
        case VK_FORMAT_D24_UNORM_S8_UINT: return REI_FMT_D24_UNORM_S8_UINT;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: return REI_FMT_D32_SFLOAT_S8_UINT;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK: return REI_FMT_DXBC1_RGB_UNORM;
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK: return REI_FMT_DXBC1_RGB_SRGB;
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK: return REI_FMT_DXBC1_RGBA_UNORM;
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK: return REI_FMT_DXBC1_RGBA_SRGB;
        case VK_FORMAT_BC2_UNORM_BLOCK: return REI_FMT_DXBC2_UNORM;
        case VK_FORMAT_BC2_SRGB_BLOCK: return REI_FMT_DXBC2_SRGB;
        case VK_FORMAT_BC3_UNORM_BLOCK: return REI_FMT_DXBC3_UNORM;
        case VK_FORMAT_BC3_SRGB_BLOCK: return REI_FMT_DXBC3_SRGB;
        case VK_FORMAT_BC4_UNORM_BLOCK: return REI_FMT_DXBC4_UNORM;
        case VK_FORMAT_BC4_SNORM_BLOCK: return REI_FMT_DXBC4_SNORM;
        case VK_FORMAT_BC5_UNORM_BLOCK: return REI_FMT_DXBC5_UNORM;
        case VK_FORMAT_BC5_SNORM_BLOCK: return REI_FMT_DXBC5_SNORM;
        case VK_FORMAT_BC6H_UFLOAT_BLOCK: return REI_FMT_DXBC6H_UFLOAT;
        case VK_FORMAT_BC6H_SFLOAT_BLOCK: return REI_FMT_DXBC6H_SFLOAT;
        case VK_FORMAT_BC7_UNORM_BLOCK: return REI_FMT_DXBC7_UNORM;
        case VK_FORMAT_BC7_SRGB_BLOCK: return REI_FMT_DXBC7_SRGB;
        case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK: return REI_FMT_ETC2_R8G8B8_UNORM;
        case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK: return REI_FMT_ETC2_R8G8B8_SRGB;
        case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK: return REI_FMT_ETC2_R8G8B8A1_UNORM;
        case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK: return REI_FMT_ETC2_R8G8B8A1_SRGB;
        case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK: return REI_FMT_ETC2_R8G8B8A8_UNORM;
        case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK: return REI_FMT_ETC2_R8G8B8A8_SRGB;
        case VK_FORMAT_EAC_R11_UNORM_BLOCK: return REI_FMT_ETC2_EAC_R11_UNORM;
        case VK_FORMAT_EAC_R11_SNORM_BLOCK: return REI_FMT_ETC2_EAC_R11_SNORM;
        case VK_FORMAT_EAC_R11G11_UNORM_BLOCK: return REI_FMT_ETC2_EAC_R11G11_UNORM;
        case VK_FORMAT_EAC_R11G11_SNORM_BLOCK: return REI_FMT_ETC2_EAC_R11G11_SNORM;
        case VK_FORMAT_ASTC_4x4_UNORM_BLOCK: return REI_FMT_ASTC_4x4_UNORM;
        case VK_FORMAT_ASTC_4x4_SRGB_BLOCK: return REI_FMT_ASTC_4x4_SRGB;
        case VK_FORMAT_ASTC_5x4_UNORM_BLOCK: return REI_FMT_ASTC_5x4_UNORM;
        case VK_FORMAT_ASTC_5x4_SRGB_BLOCK: return REI_FMT_ASTC_5x4_SRGB;
        case VK_FORMAT_ASTC_5x5_UNORM_BLOCK: return REI_FMT_ASTC_5x5_UNORM;
        case VK_FORMAT_ASTC_5x5_SRGB_BLOCK: return REI_FMT_ASTC_5x5_SRGB;
        case VK_FORMAT_ASTC_6x5_UNORM_BLOCK: return REI_FMT_ASTC_6x5_UNORM;
        case VK_FORMAT_ASTC_6x5_SRGB_BLOCK: return REI_FMT_ASTC_6x5_SRGB;
        case VK_FORMAT_ASTC_6x6_UNORM_BLOCK: return REI_FMT_ASTC_6x6_UNORM;
        case VK_FORMAT_ASTC_6x6_SRGB_BLOCK: return REI_FMT_ASTC_6x6_SRGB;
        case VK_FORMAT_ASTC_8x5_UNORM_BLOCK: return REI_FMT_ASTC_8x5_UNORM;
        case VK_FORMAT_ASTC_8x5_SRGB_BLOCK: return REI_FMT_ASTC_8x5_SRGB;
        case VK_FORMAT_ASTC_8x6_UNORM_BLOCK: return REI_FMT_ASTC_8x6_UNORM;
        case VK_FORMAT_ASTC_8x6_SRGB_BLOCK: return REI_FMT_ASTC_8x6_SRGB;
        case VK_FORMAT_ASTC_8x8_UNORM_BLOCK: return REI_FMT_ASTC_8x8_UNORM;
        case VK_FORMAT_ASTC_8x8_SRGB_BLOCK: return REI_FMT_ASTC_8x8_SRGB;
        case VK_FORMAT_ASTC_10x5_UNORM_BLOCK: return REI_FMT_ASTC_10x5_UNORM;
        case VK_FORMAT_ASTC_10x5_SRGB_BLOCK: return REI_FMT_ASTC_10x5_SRGB;
        case VK_FORMAT_ASTC_10x6_UNORM_BLOCK: return REI_FMT_ASTC_10x6_UNORM;
        case VK_FORMAT_ASTC_10x6_SRGB_BLOCK: return REI_FMT_ASTC_10x6_SRGB;
        case VK_FORMAT_ASTC_10x8_UNORM_BLOCK: return REI_FMT_ASTC_10x8_UNORM;
        case VK_FORMAT_ASTC_10x8_SRGB_BLOCK: return REI_FMT_ASTC_10x8_SRGB;
        case VK_FORMAT_ASTC_10x10_UNORM_BLOCK: return REI_FMT_ASTC_10x10_UNORM;
        case VK_FORMAT_ASTC_10x10_SRGB_BLOCK: return REI_FMT_ASTC_10x10_SRGB;
        case VK_FORMAT_ASTC_12x10_UNORM_BLOCK: return REI_FMT_ASTC_12x10_UNORM;
        case VK_FORMAT_ASTC_12x10_SRGB_BLOCK: return REI_FMT_ASTC_12x10_SRGB;
        case VK_FORMAT_ASTC_12x12_UNORM_BLOCK: return REI_FMT_ASTC_12x12_UNORM;
        case VK_FORMAT_ASTC_12x12_SRGB_BLOCK: return REI_FMT_ASTC_12x12_SRGB;

        case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG: return REI_FMT_PVRTC1_2BPP_UNORM;
        case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG: return REI_FMT_PVRTC1_4BPP_UNORM;
        case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG: return REI_FMT_PVRTC1_2BPP_SRGB;
        case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG: return REI_FMT_PVRTC1_4BPP_SRGB;

        default: return REI_FMT_UNDEFINED;
    }
    return REI_FMT_UNDEFINED;
}

static inline bool util_has_depth_aspect(uint32_t const fmt)
{
    REI_ASSERT(fmt < REI_FMT_COUNT, "Invalid format");
    switch (fmt)
    {
        case REI_FMT_D16_UNORM:
        case REI_FMT_X8_D24_UNORM:
        case REI_FMT_D32_SFLOAT:
        case REI_FMT_D16_UNORM_S8_UINT:
        case REI_FMT_D24_UNORM_S8_UINT:
        case REI_FMT_D32_SFLOAT_S8_UINT: return true;
        default: return false;
    }
}

static inline bool util_has_stencil_aspect(uint32_t const fmt)
{
    REI_ASSERT(fmt < REI_FMT_COUNT, "Invalid format");
    switch (fmt)
    {
        case REI_FMT_D16_UNORM_S8_UINT:
        case REI_FMT_D24_UNORM_S8_UINT:
        case REI_FMT_D32_SFLOAT_S8_UINT:
        case REI_FMT_S8_UINT: return true;
        default: return false;
    }
}

static inline uint32_t util_bit_size_of_block(REI_Format const fmt)
{
    REI_ASSERT(fmt < REI_FMT_COUNT, "Invalid format");
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

static VkImageAspectFlags util_vk_determine_aspect_mask(VkFormat format, bool includeStencilBit)
{
    VkImageAspectFlags result = 0;
    switch (format)
    {
            // Depth
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D32_SFLOAT:
            result = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
            // Stencil
        case VK_FORMAT_S8_UINT:
            result = VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
            // Depth/stencil
        case VK_FORMAT_D16_UNORM_S8_UINT:
        case VK_FORMAT_D24_UNORM_S8_UINT:
        case VK_FORMAT_D32_SFLOAT_S8_UINT:
            result = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (includeStencilBit)
                result |= VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
            // Assume everything else is Color
        default: result = VK_IMAGE_ASPECT_COLOR_BIT; break;
    }
    return result;
}

static VkFormatFeatureFlags util_vk_image_usage_to_format_features(VkImageUsageFlags usage)
{
    VkFormatFeatureFlags result = (VkFormatFeatureFlags)0;
    if (VK_IMAGE_USAGE_SAMPLED_BIT == (usage & VK_IMAGE_USAGE_SAMPLED_BIT))
    {
        result |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    }
    if (VK_IMAGE_USAGE_STORAGE_BIT == (usage & VK_IMAGE_USAGE_STORAGE_BIT))
    {
        result |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    }
    if (VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
    {
        result |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    }
    if (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT == (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT))
    {
        result |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    return result;
}

static VkQueueFlags util_to_vk_queue_flags(uint32_t cmdPoolType)
{
    REI_ASSERT(cmdPoolType < REI_MAX_CMD_TYPE, "Invalid REI_CmdPoolType value");
    switch (cmdPoolType)
    {
        case REI_CMD_POOL_DIRECT: return VK_QUEUE_GRAPHICS_BIT;
        case REI_CMD_POOL_COPY: return VK_QUEUE_TRANSFER_BIT;
        case REI_CMD_POOL_COMPUTE: return VK_QUEUE_COMPUTE_BIT;
        default: REI_ASSERT(false && "Invalid REI_Queue Type"); return VK_QUEUE_FLAG_BITS_MAX_ENUM;
    }
}

VkDescriptorType util_to_vk_descriptor_type(REI_DescriptorType type)
{
    switch (type)
    {
        case REI_DESCRIPTOR_TYPE_UNDEFINED:
            REI_ASSERT("Invalid REI_DescriptorInfo Type");
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        case REI_DESCRIPTOR_TYPE_SAMPLER: return VK_DESCRIPTOR_TYPE_SAMPLER;
        case REI_DESCRIPTOR_TYPE_TEXTURE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case REI_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case REI_DESCRIPTOR_TYPE_RW_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case REI_DESCRIPTOR_TYPE_BUFFER:
        case REI_DESCRIPTOR_TYPE_RW_BUFFER:
        case REI_DESCRIPTOR_TYPE_BUFFER_RAW:
        case REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case REI_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        case REI_DESCRIPTOR_TYPE_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case REI_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        default:
            REI_ASSERT("Invalid REI_DescriptorInfo Type");
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
            break;
    }
}

VkShaderStageFlags util_to_vk_shader_stage_flags(uint32_t stages)
{
    VkShaderStageFlags res = 0;
    if ((stages & REI_SHADER_STAGE_ALL_GRAPHICS) == REI_SHADER_STAGE_ALL_GRAPHICS)
        return VK_SHADER_STAGE_ALL_GRAPHICS;

    if (stages & REI_SHADER_STAGE_VERT)
        res |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stages & REI_SHADER_STAGE_GEOM)
        res |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stages & REI_SHADER_STAGE_TESE)
        res |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (stages & REI_SHADER_STAGE_TESC)
        res |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stages & REI_SHADER_STAGE_FRAG)
        res |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages & REI_SHADER_STAGE_COMP)
        res |= VK_SHADER_STAGE_COMPUTE_BIT;

    REI_ASSERT(res != 0);
    return res;
}

VkShaderStageFlagBits util_to_vk_shader_stage_flag_bit(REI_ShaderStage stage)
{
    switch (stage)
    {
        case REI_SHADER_STAGE_VERT: return VK_SHADER_STAGE_VERTEX_BIT;
        case REI_SHADER_STAGE_TESC: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case REI_SHADER_STAGE_TESE: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case REI_SHADER_STAGE_GEOM: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case REI_SHADER_STAGE_FRAG: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case REI_SHADER_STAGE_COMP: return VK_SHADER_STAGE_COMPUTE_BIT;
        case REI_SHADER_STAGE_ALL_GRAPHICS: return VK_SHADER_STAGE_ALL_GRAPHICS;
        default: REI_ASSERT(false, "Invalid shader stage"); return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }
}

/************************************************************************/
// Internal init functions
/************************************************************************/
static void create_instance(const REI_RendererDescVk* pDescVk, REI_Renderer* pRenderer)
{
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    VkResult vk_res = VK_RESULT_MAX_ENUM;

#if USE_DEBUG_UTILS_EXTENSION
    bool isDebugUtilsExtensionPresent = false;
#endif

    // Instance
    {
        // get desired layers count
        const char** platformRequestedLayers;
        uint32_t     platformRequestedLayersCount;
        vk_platfom_get_wanted_instance_layers(&platformRequestedLayersCount, &platformRequestedLayers);
        uint32_t userRequestedLayerCount = pDescVk->instanceLayerCount;
        uint32_t requestedLayersCount = platformRequestedLayersCount + userRequestedLayerCount;

        // get desired extensions count
        const char** platformRequestedExtensions;
        uint32_t     platformRequestedExtensionsCount;
        vk_platfom_get_wanted_instance_extensions(&platformRequestedExtensionsCount, &platformRequestedExtensions);
        uint32_t userRequestedExtensionsCount = (uint32_t)pDescVk->instanceExtensionCount;
        uint32_t requestedExtensionsCount = platformRequestedExtensionsCount + userRequestedExtensionsCount;

        // get available layers count
        uint32_t availableLayersCount = 0;
        vk_res = vkEnumerateInstanceLayerProperties(&availableLayersCount, NULL);
        REI_ASSERT(VK_SUCCESS == vk_res);

        // layers stack allocator
        REI_StackAllocator<true> stackAlloc = { 0 };
        stackAlloc.reserve<VkLayerProperties>(availableLayersCount)
            .reserve<char*>(requestedLayersCount)
            .reserve<uint32_t>(requestedLayersCount)
            .reserve<char*>(requestedExtensionsCount)
            .reserve<VkExtensionProperties>(REI_VK_MAX_EXTENSIONS_COUNT);

        if (!stackAlloc.done(allocator))
        {
            pRenderer->pLog(
                REI_LOG_TYPE_ERROR, "create_instance wasn't able to allocate enough memory for layersStackAlloc");
            REI_ASSERT(false);
            return;
        }

        // get available layers properties
        VkLayerProperties* availableLayers = stackAlloc.alloc<VkLayerProperties>(availableLayersCount);
        vk_res = vkEnumerateInstanceLayerProperties(&availableLayersCount, availableLayers);
        REI_ASSERT(VK_SUCCESS == vk_res);
        for (uint32_t i = 0; i < availableLayersCount; ++i)
        {
            pLog(REI_LOG_TYPE_INFO, "vkinstance-layer (%s)", availableLayers[i].layerName);
        }

        // copy layers requested by user and platform to one array
        const char** requestedLayers = stackAlloc.alloc<const char*>(requestedLayersCount);
        
        if (platformRequestedLayersCount)
            memcpy(requestedLayers, platformRequestedLayers, platformRequestedLayersCount * sizeof(*requestedLayers));

        if (userRequestedLayerCount)
            memcpy(
                requestedLayers + platformRequestedLayersCount, pDescVk->ppInstanceLayers,
                userRequestedLayerCount * sizeof(*requestedLayers));

        // check to see if the requested layers are present
        uint32_t w = 0;
        for (uint32_t r = 0; r < requestedLayersCount && w < requestedLayersCount; ++r)
        {
            bool layerFound = false;
            for (uint32_t j = 0; j < availableLayersCount; ++j)
            {
                if (strcmp(requestedLayers[r], availableLayers[j].layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (layerFound == false)
            {
                pLog(REI_LOG_TYPE_WARNING, "vkinstance-layer-missing (%s)", requestedLayers[r]);
            }
            else
            {
                requestedLayers[w] = requestedLayers[r];
                ++w;
            }
        }
        requestedLayersCount = w;

        // get count of available extensions per layer 
        uint32_t  layersExtensionsCount = 0;
        uint32_t* perLayerExtensionCounts = stackAlloc.alloc<uint32_t>(requestedLayersCount);
        for (uint32_t i = 0; i < requestedLayersCount; ++i)
        {
            vk_res = vkEnumerateInstanceExtensionProperties(requestedLayers[i], &perLayerExtensionCounts[i], NULL);
            REI_ASSERT(VK_SUCCESS == vk_res);
            layersExtensionsCount += perLayerExtensionCounts[i];
        }

        // get count of standalone available extensions
        uint32_t standaloneExtensionsCount = 0;
        vk_res = vkEnumerateInstanceExtensionProperties(NULL, &standaloneExtensionsCount, NULL);
        REI_ASSERT(VK_SUCCESS == vk_res);
        uint32_t availableExtensionsCount = standaloneExtensionsCount + layersExtensionsCount;

        // copy extensions requested by user and platform to one array
        const char** requestedExtensions = stackAlloc.alloc<const char*>(requestedExtensionsCount);
        if (platformRequestedExtensionsCount)
            memcpy(
                requestedExtensions, platformRequestedExtensions,
                platformRequestedExtensionsCount * sizeof(*requestedExtensions));

        if (userRequestedExtensionsCount)
            memcpy(
                requestedExtensions + platformRequestedExtensionsCount, pDescVk->ppInstanceExtensions,
                userRequestedExtensionsCount * sizeof(*requestedExtensions));

        // get standalone available extensions
        VkExtensionProperties* availableExtensions = stackAlloc.alloc<VkExtensionProperties>(availableExtensionsCount);
        vk_res = vkEnumerateInstanceExtensionProperties(NULL, &standaloneExtensionsCount, availableExtensions);
        REI_ASSERT(VK_SUCCESS == vk_res);
        for (uint32_t i = 0; i < standaloneExtensionsCount; ++i)
        {
            pLog(REI_LOG_TYPE_INFO, "vkinstance-ext (%s)", availableExtensions[i].extensionName);
        }

        // get available extensions per layer 
        VkExtensionProperties* pProperties = availableExtensions + standaloneExtensionsCount;
        for (uint32_t i = 0; i < requestedLayersCount; ++i)
        {
            const char* layer_name = requestedLayers[i];
            uint32_t    count = perLayerExtensionCounts[i];
            if (!count)
                continue;
            vk_res = vkEnumerateInstanceExtensionProperties(layer_name, &count, pProperties);
            REI_ASSERT(VK_SUCCESS == vk_res);
            for (uint32_t j = 0; j < count; ++j)
            {
                pLog(REI_LOG_TYPE_INFO, "vkinstance-ext (%s)", pProperties[j].extensionName);
            }
            pProperties += count;
        }
        REI_ASSERT(pProperties <= availableExtensions + availableExtensionsCount);

        // check to see if the requested extensions are present
        w = 0;
        for (uint32_t r = 0; r < requestedExtensionsCount && w < requestedExtensionsCount; ++r) 
        {
            bool extensionFound = false;
            for (uint32_t j = 0; j < availableExtensionsCount; ++j)
            {
                if (strcmp(requestedExtensions[r], availableExtensions[j].extensionName) == 0)
                {
#if USE_DEBUG_UTILS_EXTENSION
                    if (strcmp(requestedExtensions[r], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                            isDebugUtilsExtensionPresent = true;
#endif
                    extensionFound = true;
                    break;
                }
            }
            if (extensionFound == false)
            {
                pLog(REI_LOG_TYPE_WARNING, "vkinstance-extension-missing (%s)", requestedExtensions[r]);
            }
            else
            {
                requestedExtensions[w] = requestedExtensions[r];
                ++w;
            }
        }
        requestedExtensionsCount = w;

#if VK_HEADER_VERSION >= 108
        VkValidationFeaturesEXT      validationFeaturesExt = { VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT };
        VkValidationFeatureEnableEXT enabledValidationFeatures[] = {
            VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        };

        if (pDescVk->desc.enableGPUBasedValidation)
        {
            validationFeaturesExt.enabledValidationFeatureCount = 1;
            validationFeaturesExt.pEnabledValidationFeatures = enabledValidationFeatures;
        }
#endif
        DECLARE_ZERO(VkApplicationInfo, app_info);
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pNext = NULL;
        app_info.pApplicationName = pDescVk->desc.app_name;
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "REI";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = pDescVk->vulkanApiVersion ? pDescVk->vulkanApiVersion : VK_API_VERSION_1_1;

        // Add more extensions here
        DECLARE_ZERO(VkInstanceCreateInfo, create_info);
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#if VK_HEADER_VERSION >= 108
        create_info.pNext = &validationFeaturesExt;
#endif
        create_info.flags = 0;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledLayerCount = requestedLayersCount;
        create_info.ppEnabledLayerNames = requestedLayers;
        create_info.enabledExtensionCount = requestedExtensionsCount;
        create_info.ppEnabledExtensionNames = requestedExtensions;
        vk_res = vkCreateInstance(&create_info, NULL, &(pRenderer->pVkInstance));
        REI_ASSERT(VK_SUCCESS == vk_res);
    }

    pRenderer->pfn_vkGetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(
        pRenderer->pVkInstance, "vkGetPhysicalDeviceFeatures2KHR");

    // Debug
    {
#if USE_DEBUG_UTILS_EXTENSION
        if (isDebugUtilsExtensionPresent)
        {
            pRenderer->pfn_vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCreateDebugUtilsMessengerEXT"));

            pRenderer->pfn_vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkDestroyDebugUtilsMessengerEXT"));

            pRenderer->pfn_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(
                vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkSetDebugUtilsObjectNameEXT"));

            pRenderer->pfn_vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdBeginDebugUtilsLabelEXT"));

            pRenderer->pfn_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
                vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdEndDebugUtilsLabelEXT"));

            pRenderer->pfn_vkCmdInsertDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
                vkGetInstanceProcAddr(pRenderer->pVkInstance, "vkCmdInsertDebugUtilsLabelEXT"));

            VkDebugUtilsMessengerCreateInfoEXT create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            create_info.pfnUserCallback = internal_debug_report_callback;
            create_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            create_info.flags = 0;
            create_info.pUserData = (void*)pRenderer->pLog;
            VkResult res = pRenderer->pfn_vkCreateDebugUtilsMessengerEXT(
                pRenderer->pVkInstance, &create_info, NULL, &(pRenderer->pVkDebugUtilsMessenger));
            if (VK_SUCCESS != res)
            {
                pLog(
                    REI_LOG_TYPE_ERROR,
                    "internal_vk_init_instance (vkCreateDebugUtilsMessengerEXT failed"
                    " - disabling Vulkan debug callbacks)");
            }
        }
#else
        PFN_vkCreateDebugReportCallbackEXT pfn_vkCreateDebugReportCallbackEXT =
            (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
                pRenderer->pVkInstance, "vkCreateDebugReportCallbackEXT");

        DECLARE_ZERO(VkDebugReportCallbackCreateInfoEXT, create_info);
        create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
        create_info.pNext = NULL;
        create_info.pfnCallback = internal_debug_report_callback;
        create_info.flags =
            VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
            // VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | Performance warnings are not very vaild on desktop
            VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
        create_info.pUserData = (void*)pRenderer->pLog;
        vk_res = pfn_vkCreateDebugReportCallbackEXT(
            pRenderer->pVkInstance, &create_info, NULL, &(pRenderer->pVkDebugReport));
        if (VK_SUCCESS != vk_res)
        {
            pLog(
                REI_LOG_TYPE_ERROR,
                "internal_vk_init_instance (vkCreateDebugReportCallbackEXT failed"
                " - disabling Vulkan debug callbacks)");
        }
#endif
    }
}

static void remove_instance(REI_Renderer* pRenderer)
{
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);

#if USE_DEBUG_UTILS_EXTENSION
    if (pRenderer->pVkDebugUtilsMessenger)
    {
        pRenderer->pfn_vkDestroyDebugUtilsMessengerEXT(pRenderer->pVkInstance, pRenderer->pVkDebugUtilsMessenger, NULL);
        pRenderer->pVkDebugUtilsMessenger = NULL;
    }
#else
    if (pRenderer->pVkDebugReport)
    {
        PFN_vkDestroyDebugReportCallbackEXT pfn_vkDestroyDebugReportCallbackEXT =
            (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
                pRenderer->pVkInstance, "vkDestroyDebugReportCallbackEXT");
        pfn_vkDestroyDebugReportCallbackEXT(pRenderer->pVkInstance, pRenderer->pVkDebugReport, NULL);
        pRenderer->pVkDebugReport = NULL;
    }
#endif

    vkDestroyInstance(pRenderer->pVkInstance, NULL);
}

static void REI_addDevice(const REI_RendererDescVk* pDescVk, REI_Renderer* pRenderer)
{
    REI_ASSERT(pDescVk != nullptr);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);

    VkResult vk_res = vkEnumeratePhysicalDevices(pRenderer->pVkInstance, &(pRenderer->numOfDevices), NULL);
    REI_ASSERT(VK_SUCCESS == vk_res);
    REI_ASSERT(pRenderer->numOfDevices > 0);

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    pRenderer->numOfDevices = REI_min<uint32_t>(REI_MAX_GPUS, pRenderer->numOfDevices);

    VkPhysicalDevice pVkDevices[REI_MAX_GPUS] = {};
    vk_res = vkEnumeratePhysicalDevices(pRenderer->pVkInstance, &(pRenderer->numOfDevices), pVkDevices);
    REI_ASSERT(VK_SUCCESS == vk_res);

    uint32_t maxLayersCount = 0;
    uint32_t maxExtensionsCount = 0;
    for (uint32_t i = 0; i < pRenderer->numOfDevices; ++i)
    {
        uint32_t temp = 0;
        vk_res = vkEnumerateDeviceLayerProperties(pVkDevices[i], &temp, NULL);
        REI_ASSERT(VK_SUCCESS == vk_res);
        maxLayersCount = REI_max(maxLayersCount, temp);

        vk_res = vkEnumerateDeviceExtensionProperties(pVkDevices[i], NULL, &temp, NULL);
        REI_ASSERT(VK_SUCCESS == vk_res);
        maxExtensionsCount = REI_max(maxExtensionsCount, temp);
    }

    const char** platformRequestedExtensions;
    uint32_t     platformRequestedExtensionsCount;
    vk_platfom_get_wanted_device_extensions(&platformRequestedExtensionsCount, &platformRequestedExtensions);
    const uint32_t userRequestedExtensionsCount = (uint32_t)pDescVk->deviceExtensionCount;
    uint32_t       requestedExtensionsCount = platformRequestedExtensionsCount + userRequestedExtensionsCount;

    // Stack allocator
    REI_StackAllocator<true> stackAlloc = { 0 };
    stackAlloc.reserve<VkPhysicalDeviceProperties2>(pRenderer->numOfDevices)
        .reserve<VkPhysicalDeviceMemoryProperties>(pRenderer->numOfDevices)
        .reserve<uint32_t>(pRenderer->numOfDevices)
        .reserve<VkQueueFamilyProperties>(pRenderer->numOfDevices * REI_VK_MAX_QUEUE_FAMILY_COUNT)
        .reserve<const char*>(requestedExtensionsCount + maxExtensionsCount + 2 /*2 is number of extensions requested by REI*/)
        .reserve<VkLayerProperties>(maxLayersCount)
        .reserve<VkExtensionProperties>(maxExtensionsCount)
        .reserve<float>(REI_VK_MAX_QUEUES_PER_FAMILY)
        .reserve<VkDeviceQueueCreateInfo>(REI_VK_MAX_QUEUE_FAMILY_COUNT)
        .reserve<VkDeviceCreateInfo>();

    if (!stackAlloc.done(allocator))
    {
        pRenderer->pLog(REI_LOG_TYPE_ERROR, "REI_addDevice wasn't able to allocate enough memory for stackAlloc");
        REI_ASSERT(false);
        return;
    }

    VkPhysicalDeviceProperties2* vkDeviceProperties =
        stackAlloc.allocZeroed<VkPhysicalDeviceProperties2>(pRenderer->numOfDevices);
    VkPhysicalDeviceMemoryProperties* vkDeviceMemoryProperties =
        stackAlloc.allocZeroed<VkPhysicalDeviceMemoryProperties>(pRenderer->numOfDevices);
    uint32_t*                vkQueueFamilyCount = stackAlloc.allocZeroed<uint32_t>(pRenderer->numOfDevices);
    VkQueueFamilyProperties* vkQueueFamilyProperties =
        stackAlloc.allocZeroed<VkQueueFamilyProperties>(pRenderer->numOfDevices * REI_VK_MAX_QUEUE_FAMILY_COUNT);

    /************************************************************************/
    // Select discrete gpus first
    // If we have multiple discrete gpus prefer with bigger VRAM size
    // To find VRAM in Vulkan, loop through all the heaps and find if the
    // heap has the DEVICE_LOCAL_BIT flag set
    /************************************************************************/
    auto isDeviceBetter = [&](uint32_t testIndex, uint32_t refIndex) -> bool
    {
        VkPhysicalDeviceProperties& testProps = vkDeviceProperties[testIndex].properties;
        VkPhysicalDeviceProperties& refProps = vkDeviceProperties[refIndex].properties;

        if (testProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            refProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            return true;
        }

        if (testProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU &&
            refProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            return false;
        }

        //compare by preset if both gpu's are of same type (integrated vs discrete)
        if (testProps.vendorID == refProps.vendorID && testProps.deviceID == refProps.deviceID)
        {
            VkPhysicalDeviceMemoryProperties& testMemoryProps = vkDeviceMemoryProperties[testIndex];
            VkPhysicalDeviceMemoryProperties& refMemoryProps = vkDeviceMemoryProperties[refIndex];
            //if presets are the same then sort by vram size
            VkDeviceSize totalTestVram = 0;
            VkDeviceSize totalRefVram = 0;
            for (uint32_t i = 0; i < testMemoryProps.memoryHeapCount; ++i)
            {
                if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & testMemoryProps.memoryHeaps[i].flags)
                    totalTestVram += testMemoryProps.memoryHeaps[i].size;
            }
            for (uint32_t i = 0; i < refMemoryProps.memoryHeapCount; ++i)
            {
                if (VK_MEMORY_HEAP_DEVICE_LOCAL_BIT & refMemoryProps.memoryHeaps[i].flags)
                    totalRefVram += refMemoryProps.memoryHeaps[i].size;
            }

            return totalTestVram >= totalRefVram;
        }

        return false;
    };

    for (uint32_t i = 0; i < pRenderer->numOfDevices; ++i)
    {
        // Get memory properties
        vkGetPhysicalDeviceMemoryProperties(pVkDevices[i], &vkDeviceMemoryProperties[i]);

        // Get device properties
        VkPhysicalDeviceSubgroupProperties subgroupProperties;
        subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
        subgroupProperties.pNext = NULL;

        vkDeviceProperties[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
        vkDeviceProperties[i].pNext = &subgroupProperties;
        vkGetPhysicalDeviceProperties2(pVkDevices[i], &vkDeviceProperties[i]);

        // Get queue family properties
        vkGetPhysicalDeviceQueueFamilyProperties(pVkDevices[i], &vkQueueFamilyCount[i], NULL);
        vkQueueFamilyCount[i] = REI_min(vkQueueFamilyCount[i], (uint32_t)REI_VK_MAX_QUEUE_FAMILY_COUNT);
        vkGetPhysicalDeviceQueueFamilyProperties(
            pVkDevices[i], &vkQueueFamilyCount[i], vkQueueFamilyProperties + i * REI_VK_MAX_QUEUE_FAMILY_COUNT);

        pLog(
            REI_LOG_TYPE_INFO, "GPU found: %s (VendorID:0x%04X, DeviceID: 0x%04X)",
            vkDeviceProperties[i].properties.deviceName, vkDeviceProperties[i].properties.vendorID,
            vkDeviceProperties[i].properties.deviceID);
    }

    uint32_t deviceIndex = UINT32_MAX;
    for (uint32_t i = 0; i < pRenderer->numOfDevices; ++i)
    {
        if (pDescVk->desc.gpu_name &&
            strstr(pRenderer->vkDeviceProperties.properties.deviceName, pDescVk->desc.gpu_name) != nullptr)
        {
            deviceIndex = i;
            break;
        }

        // Check that gpu supports at least graphics
        if (deviceIndex == UINT32_MAX || isDeviceBetter(i, deviceIndex))
        {
            uint32_t                 count = vkQueueFamilyCount[i];
            VkQueueFamilyProperties* properties = vkQueueFamilyProperties + i * REI_VK_MAX_QUEUE_FAMILY_COUNT;

            //select if graphics queue is available
            for (uint32_t j = 0; j < count; j++)
            {
                //get graphics queue family
                if (properties[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    deviceIndex = i;
                    break;
                }
            }
        }
    }

    REI_ASSERT(deviceIndex != UINT32_MAX);
    pRenderer->pVkPhysicalDevice = pVkDevices[deviceIndex];
    pRenderer->vkDeviceProperties = vkDeviceProperties[deviceIndex];
    pRenderer->vkQueueFamilyCount = vkQueueFamilyCount[deviceIndex];
    REI_ASSERT(
        sizeof(pRenderer->vkQueueFamilyProperties) == sizeof(VkQueueFamilyProperties) * REI_VK_MAX_QUEUE_FAMILY_COUNT);
    memcpy(
        pRenderer->vkQueueFamilyProperties,
        vkQueueFamilyProperties + deviceIndex * REI_VK_MAX_QUEUE_FAMILY_COUNT,
        sizeof(pRenderer->vkQueueFamilyProperties));
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkPhysicalDevice);

    pLog(
        REI_LOG_TYPE_INFO, "GPU selected: %s (VendorID:0x%04X, DeviceID: 0x%04X)",
        pRenderer->vkDeviceProperties.properties.deviceName, pRenderer->vkDeviceProperties.properties.vendorID,
        pRenderer->vkDeviceProperties.properties.deviceID);

    uint32_t availableLayersCount = maxLayersCount;
    VkLayerProperties* availableLayers = stackAlloc.alloc<VkLayerProperties>(availableLayersCount);
    vk_res = vkEnumerateDeviceLayerProperties(pRenderer->pVkPhysicalDevice, &availableLayersCount, availableLayers);
    REI_ASSERT(VK_SUCCESS == vk_res);
    for (uint32_t i = 0; i < availableLayersCount; ++i)
    {
        pLog(REI_LOG_TYPE_INFO, "vkdevice-layer (%s)", availableLayers[i].layerName);
        if (strcmp(availableLayers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
            pRenderer->hasRenderDocLayerEnabled = true;
    }

    uint32_t availableExtensionsCount = maxExtensionsCount;
    VkExtensionProperties* availableExtensions = stackAlloc.alloc<VkExtensionProperties>(availableExtensionsCount);
    vk_res = vkEnumerateDeviceExtensionProperties(
        pRenderer->pVkPhysicalDevice, NULL, &availableExtensionsCount, availableExtensions);
    REI_ASSERT(VK_SUCCESS == vk_res);

    for (uint32_t i = 0; i < availableExtensionsCount; ++i)
    {
        pLog(REI_LOG_TYPE_INFO, "vkdevice-ext (%s)", availableExtensions[i].extensionName);
    }

    REI_ASSERT(availableExtensionsCount != 0);
    uint32_t     deviceExtensionsCount = 0;
    const char** deviceExtensions = stackAlloc.alloc<const char*>(availableExtensionsCount);

    bool dedicatedAllocationExtension = false;
    bool memoryReq2Extension = false;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    bool externalMemoryExtension = false;
    bool externalMemoryWin32Extension = false;
#endif
    bool hasExtendedDynamicStateExtension = false;
    // Standalone extensions
    const char** wantedDeviceExtensions = stackAlloc.alloc<const char*>(requestedExtensionsCount);
    if (platformRequestedExtensionsCount)
        memcpy(
            wantedDeviceExtensions, platformRequestedExtensions,
            platformRequestedExtensionsCount * sizeof(*wantedDeviceExtensions));
    if (userRequestedExtensionsCount)
        memcpy(
            wantedDeviceExtensions + platformRequestedExtensionsCount, pDescVk->ppDeviceExtensions,
            userRequestedExtensionsCount * sizeof(*wantedDeviceExtensions));

    for (uint32_t j = 0; j < availableExtensionsCount; ++j)
    {
#if VK_EXT_4444_formats
        if (strcmp(availableExtensions[j].extensionName, VK_EXT_4444_FORMATS_EXTENSION_NAME) == 0)
        {
            pRenderer->has4444FormatsExtension = true;
            deviceExtensions[deviceExtensionsCount++] = VK_EXT_4444_FORMATS_EXTENSION_NAME;
            continue;
        }
#endif

#if VK_EXT_extended_dynamic_state
        if (strcmp(availableExtensions[j].extensionName, VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME) == 0)
        {
            hasExtendedDynamicStateExtension = true;
            deviceExtensions[deviceExtensionsCount++] = VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME;
            continue;
        }
#endif
        for (uint32_t k = 0; k < requestedExtensionsCount; ++k)
        {
            if (strcmp(wantedDeviceExtensions[k], availableExtensions[j].extensionName) == 0)
            {
                deviceExtensions[deviceExtensionsCount++] = wantedDeviceExtensions[k];
#if !USE_DEBUG_UTILS_EXTENSION
                if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
                    pRenderer->hasDebugMarkerExtension = true;
#endif
                if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
                    dedicatedAllocationExtension = true;
                if (strcmp(wantedDeviceExtensions[k], VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
                    memoryReq2Extension = true;
#ifdef VK_USE_PLATFORM_WIN32_KHR
                if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0)
                    externalMemoryExtension = true;
                if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
                    externalMemoryWin32Extension = true;
#endif
#if VK_EXT_pipeline_creation_cache_control
                if (strcmp(wantedDeviceExtensions[k], VK_EXT_PIPELINE_CREATION_CACHE_CONTROL_EXTENSION_NAME) == 0)
                    pRenderer->hasPipelineCreationCacheControlExtension = true;
#endif
#if VK_KHR_draw_indirect_count
                if (strcmp(wantedDeviceExtensions[k], VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME) == 0)
                    pRenderer->hasDrawIndirectCountExtension = true;
#endif
#if VK_EXT_descriptor_indexing
                if (strcmp(wantedDeviceExtensions[k], VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME) == 0)
                    pRenderer->hasDescriptorIndexingExtension = true;
                break;
#endif
            }
        }
    }
    REI_ASSERT(deviceExtensionsCount < availableExtensionsCount);

    void* pExtensionList = nullptr;
#if VK_EXT_descriptor_indexing
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, pExtensionList
    };
    pExtensionList = &descriptorIndexingFeatures;
#endif
    // Add more extensions here
#if VK_EXT_fragment_shader_interlock
    VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT fragmentShaderInterlockFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT, pExtensionList
    };
    pExtensionList = &fragmentShaderInterlockFeatures;
#endif    // VK_EXT_fragment_shader_interlock

#if VK_EXT_extended_dynamic_state
    VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT, pExtensionList, true
    };
    pExtensionList = &extendedDynamicStateFeatures;
#endif

    VkPhysicalDeviceFeatures2KHR gpuFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, pExtensionList };
    pRenderer->pfn_vkGetPhysicalDeviceFeatures2KHR(pRenderer->pVkPhysicalDevice, &gpuFeatures2);

    // need a queue_priorite for each queue in the queue family we create
    {
        uint32_t                 queueFamiliesCount = pRenderer->vkQueueFamilyCount;
        VkQueueFamilyProperties* queueFamiliesProperties = pRenderer->vkQueueFamilyProperties;

        float* queue_priorities = stackAlloc.alloc<float>(REI_VK_MAX_QUEUES_PER_FAMILY);
        memset(queue_priorities, 1, REI_VK_MAX_QUEUES_PER_FAMILY * sizeof(float));

        uint32_t queue_create_infos_count = 0;
        VkDeviceQueueCreateInfo* queue_create_infos =
            stackAlloc.allocZeroed<VkDeviceQueueCreateInfo>(REI_VK_MAX_QUEUE_FAMILY_COUNT);

        for (uint32_t i = 0; i < queueFamiliesCount; i++)
        {
            queueFamiliesProperties[i].queueCount =
                queueFamiliesProperties[i].queueCount > REI_VK_MAX_QUEUES_PER_FAMILY
                    ? REI_VK_MAX_QUEUES_PER_FAMILY
                    : queueFamiliesProperties[i].queueCount;

            if (queueFamiliesProperties[i].queueCount > 0)
            {
                queue_create_infos[queue_create_infos_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queue_create_infos[queue_create_infos_count].pNext = NULL;
                queue_create_infos[queue_create_infos_count].flags = 0;
                queue_create_infos[queue_create_infos_count].queueFamilyIndex = i;
                queue_create_infos[queue_create_infos_count].queueCount = queueFamiliesProperties[i].queueCount;
                queue_create_infos[queue_create_infos_count].pQueuePriorities = queue_priorities;
                queue_create_infos_count++;
            }
        }

        VkDeviceCreateInfo& create_info = *stackAlloc.alloc<VkDeviceCreateInfo>();
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.pNext = &gpuFeatures2;
        create_info.flags = 0;
        create_info.queueCreateInfoCount = queue_create_infos_count;
        create_info.pQueueCreateInfos = queue_create_infos;
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = NULL;
        create_info.enabledExtensionCount = deviceExtensionsCount;
        create_info.ppEnabledExtensionNames = deviceExtensionsCount ? deviceExtensions: NULL;
        create_info.pEnabledFeatures = NULL;
        vk_res = vkCreateDevice(pRenderer->pVkPhysicalDevice, &create_info, NULL, &(pRenderer->pVkDevice));
        REI_ASSERT(pRenderer->pVkDevice && VK_SUCCESS == vk_res);
    }

    pRenderer->hasDedicatedAllocationExtension = dedicatedAllocationExtension && memoryReq2Extension;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    pRenderer->hasExternalMemoryExtension = externalMemoryExtension && externalMemoryWin32Extension;
#endif

    if (pRenderer->hasDedicatedAllocationExtension)
    {
        pLog(REI_LOG_TYPE_INFO, "Successfully loaded Dedicated Allocation extension");
    }

    if (pRenderer->hasExternalMemoryExtension)
    {
        pLog(REI_LOG_TYPE_INFO, "Successfully loaded External Memory extension");
    }

#if !USE_DEBUG_UTILS_EXTENSION
    if (pRenderer->hasDebugMarkerExtension)
    {
        pRenderer->pfn_vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)vkGetDeviceProcAddr(
            pRenderer->pVkDevice, "vkDebugMarkerSetObjectNameEXT");
        pRenderer->pfn_vkCmdDebugMarkerBeginEXT =
            (PFN_vkCmdDebugMarkerBeginEXT)vkGetDeviceProcAddr(pRenderer->pVkDevice, "vkCmdDebugMarkerBeginEXT");
        pRenderer->pfn_vkCmdDebugMarkerEndEXT =
            (PFN_vkCmdDebugMarkerEndEXT)vkGetDeviceProcAddr(pRenderer->pVkDevice, "vkCmdDebugMarkerEndEXT");
        pRenderer->pfn_vkCmdDebugMarkerInsertEXT =
            (PFN_vkCmdDebugMarkerInsertEXT)vkGetDeviceProcAddr(pRenderer->pVkDevice, "vkCmdDebugMarkerInsertEXT");
    }
#endif

    if (pRenderer->hasDrawIndirectCountExtension)
    {
        pRenderer->pfn_VkCmdDrawIndirectCountKHR =
            (PFN_vkCmdDrawIndirectCountKHR)vkGetDeviceProcAddr(pRenderer->pVkDevice, "vkCmdDrawIndirectCountKHR");
        pRenderer->pfn_VkCmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR)vkGetDeviceProcAddr(
            pRenderer->pVkDevice, "vkCmdDrawIndexedIndirectCountKHR");
        pLog(REI_LOG_TYPE_INFO, "Successfully loaded Draw Indirect extension");
    }

    if (pRenderer->hasDescriptorIndexingExtension)
    {
        pLog(REI_LOG_TYPE_INFO, "Successfully loaded Descriptor Indexing extension");
    }

    if (pRenderer->has4444FormatsExtension)
    {
        pLog(REI_LOG_TYPE_INFO, "Successfully loaded 4444 Formats extension");
    }
    else
    {
        REI_ASSERT(false, "Failed to load VK_EXT_4444_formats extension");
    }

#if USE_DEBUG_UTILS_EXTENSION
    pRenderer->hasDebugMarkerExtension =
        pRenderer->pfn_vkCmdBeginDebugUtilsLabelEXT && pRenderer->pfn_vkCmdEndDebugUtilsLabelEXT &&
        pRenderer->pfn_vkCmdInsertDebugUtilsLabelEXT && pRenderer->pfn_vkSetDebugUtilsObjectNameEXT;
#endif

    if (hasExtendedDynamicStateExtension)
    {
        pRenderer->pfn_vkCmdBindVertexBuffers2EXT =
            (PFN_vkCmdBindVertexBuffers2EXT)vkGetDeviceProcAddr(pRenderer->pVkDevice, "vkCmdBindVertexBuffers2EXT");
        REI_ASSERT(pRenderer->pfn_vkCmdBindVertexBuffers2EXT);
        pLog(REI_LOG_TYPE_INFO, "Successfully loaded Extended Dynamic State extension");
    }
    else
    {
        REI_ASSERT(
            false,
            "Failed to load VK_EXT_extended_dynamic_state extension; vkCmdBindVertexBuffers2EXT is currently required "
            "to bind vertex buffers correctly");
    }
}

static void REI_removeDevice(REI_Renderer* pRenderer) { vkDestroyDevice(pRenderer->pVkDevice, NULL); }

/************************************************************************/
// Renderer Init Remove
/************************************************************************/
void REI_initRenderer(const REI_RendererDesc* pDesc, REI_Renderer** ppRenderer)
{
    REI_RendererDescVk descVk{ *pDesc };
    REI_initRendererVk(&descVk, ppRenderer);
}

void REI_initRendererVk(const REI_RendererDescVk* pDescVk, REI_Renderer** ppRenderer)
{
    REI_AllocatorCallbacks allocatorCallbacks;
    REI_setupAllocatorCallbacks(pDescVk->desc.pAllocator, allocatorCallbacks);

    REI_Renderer* pRenderer = REI_new<REI_Renderer>(allocatorCallbacks, allocatorCallbacks);
    REI_ASSERT(pRenderer);

    pRenderer->pLog = pDescVk->desc.pLog ? pDescVk->desc.pLog : REI_Log;

    // Initialize the Vulkan internal bits
    {
        create_instance(pDescVk, pRenderer);
        REI_addDevice(pDescVk, pRenderer);

        /************************************************************************/
        /************************************************************************/
        VmaAllocatorCreateInfo createInfo = { 0 };
        createInfo.device = pRenderer->pVkDevice;
        createInfo.physicalDevice = pRenderer->pVkPhysicalDevice;

        // Render Doc Capture currently does not support use of this extension
        if (pRenderer->hasDedicatedAllocationExtension && !pRenderer->hasRenderDocLayerEnabled)
        {
            createInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
        }

        PFN_vkGetImageMemoryRequirements2KHR pfn_vkGetImageMemoryRequirements2KHR =
            (PFN_vkGetImageMemoryRequirements2KHR)vkGetDeviceProcAddr(
                pRenderer->pVkDevice, "vkGetImageMemoryRequirements2KHR");
        PFN_vkGetBufferMemoryRequirements2KHR pfn_vkGetBufferMemoryRequirements2KHR =
            (PFN_vkGetBufferMemoryRequirements2KHR)vkGetDeviceProcAddr(
                pRenderer->pVkDevice, "vkGetBufferMemoryRequirements2KHR");

        VmaVulkanFunctions vulkanFunctions = {};
        vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
        vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
        vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
        vulkanFunctions.vkCreateImage = vkCreateImage;
        vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vulkanFunctions.vkDestroyImage = vkDestroyImage;
        vulkanFunctions.vkFreeMemory = vkFreeMemory;
        vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vulkanFunctions.vkGetBufferMemoryRequirements2KHR = pfn_vkGetBufferMemoryRequirements2KHR;
        vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vulkanFunctions.vkGetImageMemoryRequirements2KHR = pfn_vkGetImageMemoryRequirements2KHR;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vulkanFunctions.vkMapMemory = vkMapMemory;
        vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
        vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;

        createInfo.pVulkanFunctions = &vulkanFunctions;

        vmaCreateAllocator(&createInfo, &pRenderer->pVmaAllocator);
    }

    VkDescriptorPoolSize descriptorPoolSizes[gDescriptorTypeRangeSize] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, REI_VK_DESCRIPTOR_TYPE_SAMPLER_COUNT },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, REI_VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER_COUNT },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, REI_VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE_COUNT },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, REI_VK_DESCRIPTOR_TYPE_STORAGE_IMAGE_COUNT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, REI_VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER_COUNT },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, REI_VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER_COUNT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, REI_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_COUNT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, REI_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_COUNT },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, REI_VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC_COUNT },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, REI_VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC_COUNT },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, REI_VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT_COUNT },
    };

    add_descriptor_pool(
        pRenderer, REI_VK_MAX_DESCRIPTORS_SETS_IN_POOL, VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        descriptorPoolSizes,
        gDescriptorTypeRangeSize, &pRenderer->pDescriptorPool);

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext = NULL;
    layoutInfo.bindingCount = 0;
    layoutInfo.pBindings = nullptr;
    layoutInfo.flags = 0;

    VkResult result = vkCreateDescriptorSetLayout(pRenderer->pVkDevice, &layoutInfo, NULL, &pRenderer->pVkEmptyDescriptorSetLayout);
    REI_ASSERT(result == VK_SUCCESS);

    consume_descriptor_sets(
        pRenderer->pDescriptorPool, &pRenderer->pVkEmptyDescriptorSetLayout, &pRenderer->pVkEmptyDescriptorSet, 1, nullptr);

    // REI_Renderer is good! Assign it to result!
    *(ppRenderer) = pRenderer;
}

void REI_removeRenderer(REI_Renderer* pRenderer)
{
    REI_ASSERT(pRenderer);

    vkDestroyDescriptorSetLayout(pRenderer->pVkDevice, pRenderer->pVkEmptyDescriptorSetLayout, nullptr);
    remove_descriptor_pool(pRenderer, pRenderer->pDescriptorPool);
    // Destroy the Vulkan bits
    vmaDestroyAllocator(pRenderer->pVmaAllocator);

    REI_removeDevice(pRenderer);
    remove_instance(pRenderer);

    // Free all the renderer components!
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void REI_addFence(REI_Renderer* pRenderer, REI_Fence** ppFence)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    REI_Fence* pFence = (REI_Fence*)REI_calloc(pRenderer->allocator, sizeof(*pFence));
    REI_ASSERT(pFence);

    DECLARE_ZERO(VkFenceCreateInfo, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = 0;
    VkResult vk_res = vkCreateFence(pRenderer->pVkDevice, &add_info, NULL, &(pFence->pVkFence));
    REI_ASSERT(VK_SUCCESS == vk_res);

    pFence->submitted = false;

    *ppFence = pFence;
}

void REI_removeFence(REI_Renderer* pRenderer, REI_Fence* pFence)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pFence);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pFence->pVkFence);

    vkDestroyFence(pRenderer->pVkDevice, pFence->pVkFence, NULL);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pFence);
}

void REI_addSemaphore(REI_Renderer* pRenderer, REI_Semaphore** ppSemaphore)
{
    REI_ASSERT(pRenderer);

    REI_Semaphore* pSemaphore = (REI_Semaphore*)REI_calloc(pRenderer->allocator, sizeof(*pSemaphore));
    REI_ASSERT(pSemaphore);

    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    DECLARE_ZERO(VkSemaphoreCreateInfo, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = 0;
    VkResult vk_res = vkCreateSemaphore(pRenderer->pVkDevice, &add_info, NULL, &(pSemaphore->pVkSemaphore));
    REI_ASSERT(VK_SUCCESS == vk_res);
    // Set signal inital state.
    pSemaphore->signaled = false;

    *ppSemaphore = pSemaphore;
}

void REI_removeSemaphore(REI_Renderer* pRenderer, REI_Semaphore* pSemaphore)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pSemaphore);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pSemaphore->pVkSemaphore);

    vkDestroySemaphore(pRenderer->pVkDevice, pSemaphore->pVkSemaphore, NULL);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pSemaphore);
}

void REI_addQueue(REI_Renderer* pRenderer, REI_QueueDesc* pDesc, REI_Queue** ppQueue)
{
    REI_ASSERT(pDesc != NULL);

    uint32_t     queueFamilyIndex = UINT32_MAX;
    VkQueueFlags requiredFlags = util_to_vk_queue_flags(pDesc->type);
    uint32_t     queueIndex = UINT32_MAX;
    bool         found = false;

    // Try to find a dedicated queue of this type
    for (uint32_t index = 0; index < pRenderer->vkQueueFamilyCount; ++index)
    {
        VkQueueFlags queueFlags = pRenderer->vkQueueFamilyProperties[index].queueFlags;
        if ((queueFlags & requiredFlags) == queueFlags &&
            pRenderer->vkUsedQueueCount[index] < pRenderer->vkQueueFamilyProperties[index].queueCount)
        {
            found = true;
            queueFamilyIndex = index;
            queueIndex = pRenderer->vkUsedQueueCount[index];
            break;
        }
    }

    // If hardware doesn't provide a dedicated queue try to find a non-dedicated one
    if (!found)
    {
        for (uint32_t index = 0; index < pRenderer->vkQueueFamilyCount; ++index)
        {
            VkQueueFlags queueFlags = pRenderer->vkQueueFamilyProperties[index].queueFlags;
            if ((queueFlags & requiredFlags) &&
                pRenderer->vkUsedQueueCount[index] < pRenderer->vkQueueFamilyProperties[index].queueCount)
            {
                found = true;
                queueFamilyIndex = index;
                queueIndex = pRenderer->vkUsedQueueCount[index];
                break;
            }
        }
    }

    if (!found)
    {
        found = true;
        queueFamilyIndex = 0;
        queueIndex = 0;

        pRenderer->pLog(
            REI_LOG_TYPE_WARNING, "Could not find queue of type %u. Using default queue", (uint32_t)pDesc->type);
    }

    if (found)
    {
        REI_Queue* pQueueToCreate = (REI_Queue*)REI_calloc(pRenderer->allocator, sizeof(*pQueueToCreate));
        pQueueToCreate->vkQueueFamilyIndex = queueFamilyIndex;
        pQueueToCreate->pRenderer = pRenderer;
        pQueueToCreate->queueDesc = *pDesc;
        pQueueToCreate->vkQueueIndex = queueIndex;

        //get queue handle
        vkGetDeviceQueue(
            pRenderer->pVkDevice, pQueueToCreate->vkQueueFamilyIndex, pQueueToCreate->vkQueueIndex,
            &(pQueueToCreate->pVkQueue));
        REI_ASSERT(VK_NULL_HANDLE != pQueueToCreate->pVkQueue);
        *ppQueue = pQueueToCreate;

        ++pRenderer->vkUsedQueueCount[queueFamilyIndex];
    }
    else
    {
        pRenderer->pLog(REI_LOG_TYPE_ERROR, "Cannot create queue of type (%u)", pDesc->type);
    }
}

void REI_removeQueue(REI_Queue* pQueue)
{
    REI_ASSERT(pQueue != NULL);
    --pQueue->pRenderer->vkUsedQueueCount[pQueue->vkQueueFamilyIndex];
    pQueue->pRenderer->allocator.pFree(pQueue->pRenderer->allocator.pUserData, pQueue);
}

void REI_addCmdPool(REI_Renderer* pRenderer, REI_Queue* pQueue, bool transient, REI_CmdPool** ppCmdPool)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    REI_CmdPool* pCmdPool = (REI_CmdPool*)REI_calloc(pRenderer->allocator, sizeof(*pCmdPool));
    REI_ASSERT(pCmdPool);

    pCmdPool->cmdPoolType = (REI_CmdPoolType)pQueue->queueDesc.type;

    DECLARE_ZERO(VkCommandPoolCreateInfo, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    add_info.queueFamilyIndex = pQueue->vkQueueFamilyIndex;
    if (transient)
    {
        add_info.flags |= VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    }
    VkResult vk_res = vkCreateCommandPool(pRenderer->pVkDevice, &add_info, NULL, &(pCmdPool->pVkCmdPool));
    REI_ASSERT(VK_SUCCESS == vk_res);

    *ppCmdPool = pCmdPool;
}

void REI_resetCmdPool(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pCmdPool);

    VkResult vk_res = vkResetCommandPool(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, 0);
    REI_ASSERT(VK_SUCCESS == vk_res);
}

void REI_removeCmdPool(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pCmdPool);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

    vkDestroyCommandPool(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, NULL);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pCmdPool);
}

// get memChunk of REI_VK_CMD_SCRATCH_MEM_SIZE size allocated after eac REI_Cmd
inline void* util_get_scratch_memory(const REI_Cmd* pCmd) { return (void*)(pCmd + 1); }

void REI_addCmd(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool, bool secondary, REI_Cmd** ppCmd)
{
    REI_ASSERT(pCmdPool);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);
    
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    REI_StackAllocator<false> persistentAlloc = { 0 };

    persistentAlloc.reserve<REI_Cmd>().reserve<uint8_t>(REI_VK_CMD_SCRATCH_MEM_SIZE);

    if (!persistentAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "REI_addCmd wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        *ppCmd = nullptr;
        return;
    }

    REI_Cmd* pCmd = persistentAlloc.constructZeroed<REI_Cmd>(allocator);
    REI_ASSERT(pCmd);

    pCmd->pRenderer = pRenderer;
    pCmd->pCmdPool = pCmdPool;

    DECLARE_ZERO(VkCommandBufferAllocateInfo, alloc_info);
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.commandPool = pCmdPool->pVkCmdPool;
    alloc_info.level = secondary ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkResult vk_res = vkAllocateCommandBuffers(pRenderer->pVkDevice, &alloc_info, &(pCmd->pVkCmdBuf));
    REI_ASSERT(VK_SUCCESS == vk_res);

    *ppCmd = pCmd;
}

void REI_removeCmd(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool, REI_Cmd* pCmd)
{
    REI_ASSERT(pCmdPool);
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    // Remove the renderpasses
    for (RenderPassMapNode& it: pCmd->renderPassMap)
    {
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
        vkDestroyRenderPass(pRenderer->pVkDevice, it.second.first, NULL);
        vkDestroyRenderPass(pRenderer->pVkDevice, it.second.second, NULL);
#else
        vkDestroyRenderPass(pRenderer->pVkDevice, it.second, NULL);
#endif
    }

    for (FrameBufferMapNode& it: pCmd->frameBufferMap)
        remove_framebuffer(pRenderer, it.second);

    vkFreeCommandBuffers(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, 1, &(pCmd->pVkCmdBuf));

    REI_delete(pRenderer->allocator, pCmd);
}

void REI_toggleVSync(REI_Renderer* pRenderer, REI_Swapchain** ppSwapchain)
{
    //toggle vsync on or off
    //for Vulkan we need to remove the REI_Swapchain and recreate it with correct vsync option
    (*ppSwapchain)->desc.enableVsync = !(*ppSwapchain)->desc.enableVsync;
    REI_SwapchainDesc desc = (*ppSwapchain)->desc;
    REI_removeSwapchain(pRenderer, *ppSwapchain);
    REI_addSwapchain(pRenderer, &desc, ppSwapchain);
}

void REI_addSwapchain(REI_Renderer* pRenderer, const REI_SwapchainDesc* pDesc, REI_Swapchain** ppSwapchain)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(ppSwapchain);
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    /************************************************************************/
    // Create surface
    /************************************************************************/
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);
    VkSurfaceKHR surface = NULL;
    VkResult vk_res;
    // Create a WSI surface for the window:
    vk_res = vk_platform_create_surface(pRenderer->pVkInstance, &pDesc->windowHandle, NULL, &surface);
    REI_ASSERT(VK_SUCCESS == vk_res);
    /************************************************************************/
    // Create swap chain
    /************************************************************************/
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkPhysicalDevice);

    // Most GPUs will not go beyond VK_SAMPLE_COUNT_8_BIT
    REI_ASSERT(
        0 != (pRenderer->vkDeviceProperties.properties.limits.framebufferColorSampleCounts & pDesc->sampleCount));

    uint32_t surfaceFormatCount = 0;
    uint32_t surfacePresentModesCount = 0;

    // Get surface formats count
    vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->pVkPhysicalDevice, surface, &surfaceFormatCount, NULL);
    REI_ASSERT(VK_SUCCESS == vk_res);

    // Get present mode count
    vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(
        pRenderer->pVkPhysicalDevice, surface, &surfacePresentModesCount, NULL);
    REI_ASSERT(VK_SUCCESS == vk_res);

    // Stack allocator
    REI_StackAllocator<true> stackAlloc = { 0 };
    stackAlloc.reserve<VkSurfaceFormatKHR>(surfaceFormatCount).reserve<VkPresentModeKHR>(surfacePresentModesCount);

    if (!stackAlloc.done(allocator))
    {
        pRenderer->pLog(REI_LOG_TYPE_ERROR, "REI_addSwapchain wasn't able to allocate enough memory for stackAlloc");
        REI_ASSERT(false);
        vkDestroySurfaceKHR(pRenderer->pVkInstance, surface, NULL);
        *ppSwapchain = nullptr;
        return;
    }

    DECLARE_ZERO(VkSurfaceCapabilitiesKHR, caps);
    vk_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->pVkPhysicalDevice, surface, &caps);
    REI_ASSERT(VK_SUCCESS == vk_res);

    // Image count
    uint32_t image_count = pDesc->imageCount ? pDesc->imageCount : 2;
    image_count = REI_clamp(image_count, caps.minImageCount, caps.maxImageCount ? caps.maxImageCount : UINT_MAX);

    // Persistent allocator
    REI_StackAllocator<false> persistentAlloc = { 0 };
    persistentAlloc.reserve<REI_Swapchain>().reserve<VkImage>(image_count).reserve<REI_Texture*>(image_count);

    if (!persistentAlloc.done(allocator)) 
    {
        pRenderer->pLog(REI_LOG_TYPE_ERROR, "REI_addSwapchain wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        vkDestroySurfaceKHR(pRenderer->pVkInstance, surface, NULL);
        *ppSwapchain = nullptr;
        return;
    }

    REI_Swapchain* pSwapchain = persistentAlloc.allocZeroed<REI_Swapchain>();
    pSwapchain->pAllocator = &allocator;
    pSwapchain->desc = *pDesc;
    pSwapchain->pVkSurface = surface;

    // Surface format
    DECLARE_ZERO(VkSurfaceFormatKHR, surface_format);
    surface_format.format = VK_FORMAT_UNDEFINED;

    // Allocate and get surface formats
    VkSurfaceFormatKHR* formats = stackAlloc.alloc<VkSurfaceFormatKHR>(surfaceFormatCount);
    vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(
        pRenderer->pVkPhysicalDevice, pSwapchain->pVkSurface, &surfaceFormatCount, formats);
    REI_ASSERT(VK_SUCCESS == vk_res);

    if ((1 == surfaceFormatCount) && (VK_FORMAT_UNDEFINED == formats[0].format))
    {
        surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
        surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    }
    else
    {
        VkFormat requested_format = util_to_vk_format(pRenderer, pSwapchain->desc.colorFormat);
        for (uint32_t i = 0; i < surfaceFormatCount; ++i)
        {
            if ((requested_format == formats[i].format) && (VK_COLOR_SPACE_SRGB_NONLINEAR_KHR == formats[i].colorSpace))
            {
                surface_format.format = requested_format;
                surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                break;
            }
        }

        // Default to VK_FORMAT_B8G8R8A8_UNORM if requested format isn't found
        if (VK_FORMAT_UNDEFINED == surface_format.format)
        {
            surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
            surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
    }

    REI_ASSERT(VK_FORMAT_UNDEFINED != surface_format.format);

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR  present_mode = VK_PRESENT_MODE_FIFO_KHR;

    // Allocate and get present modes
    VkPresentModeKHR* modes = stackAlloc.alloc<VkPresentModeKHR>(surfacePresentModesCount);
    vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(
        pRenderer->pVkPhysicalDevice, pSwapchain->pVkSurface, &surfacePresentModesCount, modes);
    REI_ASSERT(VK_SUCCESS == vk_res);

    const uint32_t   preferredModeCount = 4;
    VkPresentModeKHR preferredModeList[preferredModeCount] = { VK_PRESENT_MODE_IMMEDIATE_KHR,
                                                               VK_PRESENT_MODE_MAILBOX_KHR,
                                                               VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                                                               VK_PRESENT_MODE_FIFO_KHR };
    uint32_t         preferredModeStartIndex = pSwapchain->desc.enableVsync ? 2 : 0;

    for (uint32_t j = preferredModeStartIndex; j < preferredModeCount; ++j)
    {
        VkPresentModeKHR mode = preferredModeList[j];
        uint32_t         i = 0;
        for (; i < surfacePresentModesCount; ++i)
        {
            if (modes[i] == mode)
            {
                break;
            }
        }
        if (i < surfacePresentModesCount)
        {
            present_mode = mode;
            break;
        }
    }


    // Swapchain
    VkExtent2D extent = { 0 };
    extent.width = pSwapchain->desc.width;
    extent.height = pSwapchain->desc.height;

    VkSharingMode sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t      queue_family_index_count = 0;
    uint32_t      queue_family_indices[2] = { pDesc->ppPresentQueues[0]->vkQueueFamilyIndex, 0 };
    uint32_t      presentQueueFamilyIndex = -1;

    // Check if hardware provides dedicated present queue
    if (0 != pRenderer->vkQueueFamilyCount)
    {
        for (uint32_t index = 0; index < pRenderer->vkQueueFamilyCount; ++index)
        {
            VkBool32 supports_present = VK_FALSE;
            VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(
                pRenderer->pVkPhysicalDevice, index, pSwapchain->pVkSurface, &supports_present);
            if ((VK_SUCCESS == res) && (VK_TRUE == supports_present) &&
                pSwapchain->desc.ppPresentQueues[0]->vkQueueFamilyIndex != index)
            {
                presentQueueFamilyIndex = index;
                break;
            }
        }

        // If there is no dedicated present queue, just find the first available queue which supports present
        if (presentQueueFamilyIndex == -1)
        {
            for (uint32_t index = 0; index < pRenderer->vkQueueFamilyCount; ++index)
            {
                VkBool32 supports_present = VK_FALSE;
                VkResult res = vkGetPhysicalDeviceSurfaceSupportKHR(
                    pRenderer->pVkPhysicalDevice, index, pSwapchain->pVkSurface, &supports_present);
                if ((VK_SUCCESS == res) && (VK_TRUE == supports_present))
                {
                    presentQueueFamilyIndex = index;
                    break;
                }
                else
                {
                    // No present queue family available. Something goes wrong.
                    REI_ASSERT(0);
                }
            }
        }
    }

    // Find if gpu has a dedicated present queue
    if (presentQueueFamilyIndex != -1 && queue_family_indices[0] != presentQueueFamilyIndex)
    {
        queue_family_indices[1] = presentQueueFamilyIndex;

        vkGetDeviceQueue(pRenderer->pVkDevice, queue_family_indices[1], 0, &pSwapchain->pPresentQueue);
        sharing_mode = VK_SHARING_MODE_CONCURRENT;
        queue_family_index_count = 2;
    }
    else
    {
        pSwapchain->pPresentQueue = VK_NULL_HANDLE;
    }

    VkSurfaceTransformFlagBitsKHR pre_transform;
    if (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    {
        pre_transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    }
    else
    {
        pre_transform = caps.currentTransform;
    }


    DECLARE_ZERO(VkSwapchainCreateInfoKHR, swapChainCreateInfo);
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.pNext = NULL;
    swapChainCreateInfo.flags = 0;
    swapChainCreateInfo.surface = pSwapchain->pVkSurface;
    swapChainCreateInfo.minImageCount = image_count;
    swapChainCreateInfo.imageFormat = surface_format.format;
    swapChainCreateInfo.imageColorSpace = surface_format.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT /*allow copy*/ |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT /*allow resolve*/;
    swapChainCreateInfo.imageSharingMode = sharing_mode;
    swapChainCreateInfo.queueFamilyIndexCount = queue_family_index_count;
    swapChainCreateInfo.pQueueFamilyIndices = queue_family_indices;
    swapChainCreateInfo.preTransform = pre_transform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = present_mode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = 0;
    vk_res = vkCreateSwapchainKHR(pRenderer->pVkDevice, &swapChainCreateInfo, NULL, &(pSwapchain->pSwapchain));
    if (VK_SUCCESS != vk_res)
    {
        // pSwapchain->desc.imageCount should be 0 when this branch is taken
        pRenderer->pLog(
            REI_LOG_TYPE_ERROR, "vkCreateSwapchainKHR failed to create swapchain");
        REI_ASSERT(false);
        REI_removeSwapchain(pRenderer, pSwapchain);
        *ppSwapchain = nullptr;
        return;
    }

    pSwapchain->desc.colorFormat = util_from_vk_format(pRenderer, surface_format.format);

    // Create rendertargets from swapchain
    uint32_t swapchainImagesCount = 0;
    vk_res = vkGetSwapchainImagesKHR(pRenderer->pVkDevice, pSwapchain->pSwapchain, &swapchainImagesCount, NULL);
    REI_ASSERT(VK_SUCCESS == vk_res);

    if (image_count != swapchainImagesCount) 
    {
        // pSwapchain->desc.imageCount should be 0 when this branch is taken
        pRenderer->pLog(
            REI_LOG_TYPE_ERROR, "vkGetSwapchainImagesKHR swapchainImagesCount value not equal to pSwapchain->desc.imageCount");
        REI_ASSERT(false);
        REI_removeSwapchain(pRenderer, pSwapchain);
        *ppSwapchain = nullptr;
        return;
    }

    pSwapchain->desc.imageCount = image_count;
    pSwapchain->ppVkSwapchainImages = persistentAlloc.alloc<VkImage>(image_count);

    vk_res = vkGetSwapchainImagesKHR(
        pRenderer->pVkDevice, pSwapchain->pSwapchain, &image_count, pSwapchain->ppVkSwapchainImages);
    REI_ASSERT(VK_SUCCESS == vk_res);

    REI_TextureDesc descColor = {};
    descColor.width = pSwapchain->desc.width;
    descColor.height = pSwapchain->desc.height;
    descColor.depth = 1;
    descColor.arraySize = 1;
    descColor.format = pSwapchain->desc.colorFormat;
    descColor.clearValue = pSwapchain->desc.colorClearValue;
    descColor.sampleCount = REI_SAMPLE_COUNT_1;
    descColor.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_TEXTURE;

    pSwapchain->ppSwapchainTextures = persistentAlloc.allocZeroed<REI_Texture*>(pSwapchain->desc.imageCount);

    // Populate the vk_image field and add the Vulkan texture objects
    for (uint32_t i = 0; i < pSwapchain->desc.imageCount; ++i)
    {
        descColor.pNativeHandle = (uint64_t)pSwapchain->ppVkSwapchainImages[i];
        REI_addTexture(pRenderer, &descColor, &pSwapchain->ppSwapchainTextures[i]);
    }
    /************************************************************************/
    /************************************************************************/

    *ppSwapchain = pSwapchain;
}

void REI_getSwapchainTextures(REI_Swapchain* pSwapchain, uint32_t* count, REI_Texture** ppTextures)
{
    if (!count)
        return;

    uint32_t copyCount = REI_min(*count, pSwapchain->desc.imageCount);
    if (ppTextures)
        memcpy(ppTextures, pSwapchain->ppSwapchainTextures, sizeof(REI_Texture*) * copyCount);

    *count = pSwapchain->desc.imageCount;
}

void REI_removeSwapchain(REI_Renderer* pRenderer, REI_Swapchain* pSwapchain)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pSwapchain);

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    for (uint32_t i = 0; i < pSwapchain->desc.imageCount; ++i)
    {
        REI_removeTexture(pRenderer, pSwapchain->ppSwapchainTextures[i]);
    }

    vkDestroySwapchainKHR(pRenderer->pVkDevice, pSwapchain->pSwapchain, NULL);
    vkDestroySurfaceKHR(pRenderer->pVkInstance, pSwapchain->pVkSurface, NULL);

    allocator.pFree(allocator.pUserData, pSwapchain);
}

void REI_addBuffer(REI_Renderer* pRenderer, const REI_BufferDesc* pDesc, REI_Buffer** pp_buffer)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(pDesc->size > 0);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    REI_Buffer* pBuffer = (REI_Buffer*)REI_calloc(pRenderer->allocator, sizeof(*pBuffer));
    REI_ASSERT(pBuffer);

    pBuffer->desc = *pDesc;

    uint64_t allocationSize = pBuffer->desc.size;
    // Align the buffer size to multiples of the dynamic uniform buffer minimum size
    if (pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    {
        uint64_t minAlignment = pRenderer->vkDeviceProperties.properties.limits.minUniformBufferOffsetAlignment;
        allocationSize = REI_align_up(allocationSize, minAlignment);
    }

    DECLARE_ZERO(VkBufferCreateInfo, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.size = allocationSize;
    add_info.usage = util_to_vk_buffer_usage(pBuffer->desc.descriptors, pDesc->format != REI_FMT_UNDEFINED);
    add_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    add_info.queueFamilyIndexCount = 0;
    add_info.pQueueFamilyIndices = NULL;

    if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_COPY_DST)
        add_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_COPY_SRC)
        add_info.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    // REI_Buffer can be used as dest in a transfer command (Uploading data to a storage buffer, Readback query data)
    if (pBuffer->desc.memoryUsage == REI_RESOURCE_MEMORY_USAGE_GPU_ONLY ||
        pBuffer->desc.memoryUsage == REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU)
        add_info.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    VmaAllocationCreateInfo vma_mem_reqs = { 0 };
    vma_mem_reqs.usage = (VmaMemoryUsage)pBuffer->desc.memoryUsage;
    vma_mem_reqs.flags = 0;
    if (pBuffer->desc.flags & REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT)
        vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    if (pBuffer->desc.flags & REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT)
        vma_mem_reqs.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo;
    VkResult          vk_res = vmaCreateBuffer(
        pRenderer->pVmaAllocator, &add_info, &vma_mem_reqs, &pBuffer->pVkBuffer, &pBuffer->pVkAllocation, &allocInfo);
    pBuffer->pCpuMappedAddress = allocInfo.pMappedData;
    REI_ASSERT(VK_SUCCESS == vk_res);

    /************************************************************************/
    // Set descriptor data
    /************************************************************************/
    if ((pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER) ||
        (pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
        (pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) ||
        (pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_BUFFER) ||
        (pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER))
    {
        pBuffer->vkBufferInfo.buffer = pBuffer->pVkBuffer;
        pBuffer->vkBufferInfo.range = VK_WHOLE_SIZE;

        if ((pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_BUFFER) ||
            (pBuffer->desc.descriptors & REI_DESCRIPTOR_TYPE_RW_BUFFER))
        {
            pBuffer->vkBufferInfo.offset = pBuffer->desc.structStride * pBuffer->desc.firstElement;
        }
        else
        {
            pBuffer->vkBufferInfo.offset = 0;
        }
    }

    if (add_info.usage & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)
    {
        uint32_t               texelSize = util_bit_size_of_block(pDesc->format) / 8;
        VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
        viewInfo.buffer = pBuffer->pVkBuffer;
        viewInfo.flags = 0;
        viewInfo.format = util_to_vk_format(pRenderer, pDesc->format);
        viewInfo.offset = pDesc->firstElement * texelSize;
        viewInfo.range = pDesc->elementCount * texelSize;
        VkFormatProperties formatProps = {};
        vkGetPhysicalDeviceFormatProperties(pRenderer->pVkPhysicalDevice, viewInfo.format, &formatProps);
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        {
            pRenderer->pLog(
                REI_LOG_TYPE_WARNING, "Failed to create uniform texel buffer view for format %u",
                (uint32_t)pDesc->format);
        }
        else
        {
            vkCreateBufferView(pRenderer->pVkDevice, &viewInfo, NULL, &pBuffer->pVkUniformTexelView);
        }
    }
    if (add_info.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
    {
        uint32_t               texelSize = util_bit_size_of_block(pDesc->format) / 8;
        VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
        viewInfo.buffer = pBuffer->pVkBuffer;
        viewInfo.flags = 0;
        viewInfo.format = util_to_vk_format(pRenderer, pDesc->format);
        viewInfo.offset = pDesc->firstElement * texelSize;
        viewInfo.range = pDesc->elementCount * texelSize;
        VkFormatProperties formatProps = {};
        vkGetPhysicalDeviceFormatProperties(pRenderer->pVkPhysicalDevice, viewInfo.format, &formatProps);
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        {
            pRenderer->pLog(
                REI_LOG_TYPE_WARNING, "Failed to create storage texel buffer view for format %u",
                (uint32_t)pDesc->format);
        }
        else
        {
            vkCreateBufferView(pRenderer->pVkDevice, &viewInfo, NULL, &pBuffer->pVkStorageTexelView);
        }
    }
    /************************************************************************/
    /************************************************************************/
    *pp_buffer = pBuffer;
}

void REI_removeBuffer(REI_Renderer* pRenderer, REI_Buffer* pBuffer)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pBuffer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pBuffer->pVkBuffer);

    if (pBuffer->pVkUniformTexelView)
    {
        vkDestroyBufferView(pRenderer->pVkDevice, pBuffer->pVkUniformTexelView, NULL);
        pBuffer->pVkUniformTexelView = VK_NULL_HANDLE;
    }
    if (pBuffer->pVkStorageTexelView)
    {
        vkDestroyBufferView(pRenderer->pVkDevice, pBuffer->pVkStorageTexelView, NULL);
        pBuffer->pVkStorageTexelView = VK_NULL_HANDLE;
    }

    vmaDestroyBuffer(pRenderer->pVmaAllocator, pBuffer->pVkBuffer, pBuffer->pVkAllocation);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pBuffer);
}

void REI_addTexture(REI_Renderer* pRenderer, const REI_TextureDesc* pDesc, REI_Texture** ppTexture)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc && pDesc->width && pDesc->height);
    if (pDesc->sampleCount > REI_SAMPLE_COUNT_1 && pDesc->mipLevels > 1)
    {
        pRenderer->pLog(REI_LOG_TYPE_ERROR, "Multi-Sampled textures cannot have mip maps");
        REI_ASSERT(false);
        return;
    }

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    REI_StackAllocator<false> persistentAlloc = { 0 };
    persistentAlloc.reserve<REI_Texture>();

    if (((util_has_stencil_aspect(pDesc->format)) && (pDesc->descriptors & REI_DESCRIPTOR_TYPE_TEXTURE))) 
    {
        persistentAlloc.reserve<VkImageView>();
    }

    if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        persistentAlloc.reserve<VkImageView>((pDesc->mipLevels ? pDesc->mipLevels : 1));
    }

     bool isRT =
        pDesc->descriptors & (REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES |
                              REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES);

    if (isRT)
    {
        uint32_t numRTVs = (pDesc->mipLevels ? pDesc->mipLevels : 1);
        if ((pDesc->descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (pDesc->descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            numRTVs *= pDesc->arraySize ? pDesc->arraySize : 1 * pDesc->depth ? pDesc->depth : 1;
        }

        persistentAlloc.reserve<VkImageView>(numRTVs);
    }

    if (pDesc->pDebugName)
    {
        persistentAlloc.reserve<wchar_t>(wcslen(pDesc->pDebugName) + 1);
    }

    if (!persistentAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "REI_addTexture wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        *ppTexture = nullptr;
        return;
    }

    REI_Texture* pTexture = persistentAlloc.allocZeroed<REI_Texture>();
    REI_ASSERT(pTexture);

    pTexture->desc = *pDesc;
    REI_TextureDesc& desc = pTexture->desc;
    desc.depth = pDesc->depth ? pDesc->depth : 1;
    desc.mipLevels = pDesc->mipLevels ? pDesc->mipLevels : 1;
    desc.arraySize = pDesc->arraySize ? pDesc->arraySize : 1;

    // Monotonically increasing thread safe id generation
    pTexture->textureId = REI_atomicptr_add_relaxed(&pRenderer->textureIds, 1);

    if (desc.pNativeHandle && !(desc.flags & REI_TEXTURE_CREATION_FLAG_IMPORT_BIT))
    {
        pTexture->ownsImage = false;
        pTexture->pVkImage = (VkImage)desc.pNativeHandle;
    }
    else
    {
        pTexture->ownsImage = true;
    }

    uint32_t descriptors = pDesc->descriptors;
    bool const isDepth = util_has_depth_aspect(desc.format);
    REI_ASSERT(
        !((isDepth) && (desc.descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

    VkImageUsageFlags additionalFlags =
        isRT ? (isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) : 0;

    if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_COPY_DST)
        additionalFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    if (pDesc->descriptors & REI_DESCRIPTOR_TYPE_COPY_SRC)
        additionalFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkImageType image_type = VK_IMAGE_TYPE_MAX_ENUM;
    if (desc.flags & REI_TEXTURE_CREATION_FLAG_FORCE_2D)
    {
        REI_ASSERT(desc.depth == 1);
        image_type = VK_IMAGE_TYPE_2D;
    }
    else if (desc.flags & REI_TEXTURE_CREATION_FLAG_FORCE_3D)
    {
        image_type = VK_IMAGE_TYPE_3D;
    }
    else
    {
        if (desc.depth > 1)
            image_type = VK_IMAGE_TYPE_3D;
        else if (desc.height > 1)
            image_type = VK_IMAGE_TYPE_2D;
        else
            image_type = VK_IMAGE_TYPE_1D;
    }

    bool cubemapRequired = (REI_DESCRIPTOR_TYPE_TEXTURE_CUBE == (descriptors & REI_DESCRIPTOR_TYPE_TEXTURE_CUBE));
    bool arrayRequired = false;

    if (VK_NULL_HANDLE == pTexture->pVkImage)
    {
        DECLARE_ZERO(VkImageCreateInfo, add_info);
        add_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.imageType = image_type;
        add_info.format = util_to_vk_format(pRenderer, desc.format);
        add_info.extent.width = desc.width;
        add_info.extent.height = desc.height;
        add_info.extent.depth = desc.depth;
        add_info.mipLevels = desc.mipLevels;
        add_info.arrayLayers = desc.arraySize;
        add_info.samples = util_to_vk_sample_count(desc.sampleCount);
        add_info.tiling = (0 != desc.hostVisible) ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
        add_info.usage = util_to_vk_image_usage(descriptors);
        add_info.usage |= additionalFlags;
        add_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        add_info.queueFamilyIndexCount = 0;
        add_info.pQueueFamilyIndices = NULL;
        add_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (cubemapRequired)
            add_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        if (arrayRequired)
            add_info.flags |= VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT_KHR;

        if (VK_IMAGE_USAGE_SAMPLED_BIT & add_info.usage)
        {
            // Make it easy to copy to and from textures
            add_info.usage |= (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        }

        if (add_info.samples != VK_SAMPLE_COUNT_1_BIT)
        {
            // allow resolve for image
            add_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }

        // TODO Deano move hostvisible flag to capbits structure
        // Verify that GPU supports this format
        DECLARE_ZERO(VkFormatProperties, formatProperties);
        vkGetPhysicalDeviceFormatProperties(pRenderer->pVkPhysicalDevice, add_info.format, &formatProperties);
        VkFormatFeatureFlags format_features = util_vk_image_usage_to_format_features(add_info.usage);
        REI_ASSERT(
            (formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0,
            "GPU shader can't' read from this format");

        if (desc.hostVisible)
        {
            VkFormatFeatureFlags flags = formatProperties.linearTilingFeatures & format_features;
            REI_ASSERT((0 != flags), "Format is not supported for host visible images");
        }
        else
        {
            VkFormatFeatureFlags flags = formatProperties.optimalTilingFeatures & format_features;
            REI_ASSERT((0 != flags), "Format is not supported for GPU local images (i.e. not host visible images)");
        }

        VmaAllocationCreateInfo mem_reqs = { 0 };
        if (desc.flags & REI_TEXTURE_CREATION_FLAG_OWN_MEMORY_BIT)
            mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        mem_reqs.usage = (VmaMemoryUsage)VMA_MEMORY_USAGE_GPU_ONLY;

        VkExternalMemoryImageCreateInfoKHR externalInfo = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
                                                            NULL };

#if defined(VK_USE_PLATFORM_WIN32_KHR)
        VkImportMemoryWin32HandleInfoKHR importInfo = { VK_STRUCTURE_TYPE_IMPORT_MEMORY_WIN32_HANDLE_INFO_KHR, NULL };
#endif
        VkExportMemoryAllocateInfoKHR exportMemoryInfo = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR, NULL };

        if (pRenderer->hasExternalMemoryExtension && desc.flags & REI_TEXTURE_CREATION_FLAG_IMPORT_BIT)
        {
            add_info.pNext = &externalInfo;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
            struct ImportHandleInfo
            {
                void*                                 pHandle;
                VkExternalMemoryHandleTypeFlagBitsKHR mHandleType;
            };

            ImportHandleInfo* pHandleInfo = (ImportHandleInfo*)desc.pNativeHandle;
            importInfo.handle = pHandleInfo->pHandle;
            importInfo.handleType = pHandleInfo->mHandleType;

            externalInfo.handleTypes = pHandleInfo->mHandleType;

            mem_reqs.pUserData = &importInfo;
            // Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
            mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
#endif
        }
        else if (pRenderer->hasExternalMemoryExtension && desc.flags & REI_TEXTURE_CREATION_FLAG_EXPORT_BIT)
        {
#if defined(VK_USE_PLATFORM_WIN32_KHR)
            exportMemoryInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
#endif

            mem_reqs.pUserData = &exportMemoryInfo;
            // Allocate external (importable / exportable) memory as dedicated memory to avoid adding unnecessary complexity to the Vulkan Memory Allocator
            mem_reqs.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
        }

        VkResult vk_res = vmaCreateImage(
            pRenderer->pVmaAllocator, &add_info, &mem_reqs, &pTexture->pVkImage, &pTexture->pVkAllocation, NULL);
        REI_ASSERT(VK_SUCCESS == vk_res);
    }
    /************************************************************************/
    // Create image view
    /************************************************************************/
    VkImageViewType view_type = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
    switch (image_type)
    {
        case VK_IMAGE_TYPE_1D:
            view_type = desc.arraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;
            break;
        case VK_IMAGE_TYPE_2D:
            if (cubemapRequired)
                view_type = (desc.arraySize > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
            else
                view_type = desc.arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            break;
        case VK_IMAGE_TYPE_3D:
            if (desc.arraySize > 1)
            {
                pRenderer->pLog(REI_LOG_TYPE_ERROR, "Cannot support 3D REI_Texture Array in Vulkan");
                REI_ASSERT(false);
            }
            view_type = VK_IMAGE_VIEW_TYPE_3D;
            break;
        default: REI_ASSERT(false && "Image Format not supported!"); break;
    }

    REI_ASSERT(view_type != VK_IMAGE_VIEW_TYPE_MAX_ENUM && "Invalid Image View");

    VkImageViewCreateInfo srvDesc = {};
    // SRV
    srvDesc.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    srvDesc.pNext = NULL;
    srvDesc.flags = 0;
    srvDesc.image = pTexture->pVkImage;
    srvDesc.viewType = view_type;
    srvDesc.format = util_to_vk_format(pRenderer, desc.format);
    srvDesc.components.r = VkComponentSwizzle(desc.componentMapping[0]);
    srvDesc.components.g = VkComponentSwizzle(desc.componentMapping[1]);
    srvDesc.components.b = VkComponentSwizzle(desc.componentMapping[2]);
    srvDesc.components.a = VkComponentSwizzle(desc.componentMapping[3]);
    srvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(srvDesc.format, false);
    srvDesc.subresourceRange.baseMipLevel = 0;
    srvDesc.subresourceRange.levelCount = desc.mipLevels;
    srvDesc.subresourceRange.baseArrayLayer = 0;
    srvDesc.subresourceRange.layerCount = desc.arraySize;
    pTexture->vkAspectMask = util_vk_determine_aspect_mask(srvDesc.format, true);
    if (descriptors & REI_DESCRIPTOR_TYPE_TEXTURE)
    {
        VkResult vk_res = vkCreateImageView(pRenderer->pVkDevice, &srvDesc, NULL, &pTexture->pVkSRVDescriptor);
        REI_ASSERT(VK_SUCCESS == vk_res);
    }

    // SRV stencil
    if ((util_has_stencil_aspect(desc.format)) && (descriptors & REI_DESCRIPTOR_TYPE_TEXTURE))
    {
        pTexture->pVkSRVStencilDescriptor = persistentAlloc.alloc<VkImageView>();
        srvDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        VkResult vk_res = vkCreateImageView(pRenderer->pVkDevice, &srvDesc, NULL, pTexture->pVkSRVStencilDescriptor);
        REI_ASSERT(VK_SUCCESS == vk_res);
    }

    // UAV
    if (descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        pTexture->pVkUAVDescriptors = persistentAlloc.alloc<VkImageView>(desc.mipLevels);
        VkImageViewCreateInfo uavDesc = srvDesc;
        // #NOTE : We dont support imageCube, imageCubeArray for consistency with other APIs
        // All cubemaps will be used as image2DArray for Image Load / Store ops
        if (uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE_ARRAY || uavDesc.viewType == VK_IMAGE_VIEW_TYPE_CUBE)
            uavDesc.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        uavDesc.subresourceRange.levelCount = 1;
        for (uint32_t i = 0; i < desc.mipLevels; ++i)
        {
            uavDesc.subresourceRange.baseMipLevel = i;
            VkResult vk_res = vkCreateImageView(pRenderer->pVkDevice, &uavDesc, NULL, &pTexture->pVkUAVDescriptors[i]);
            REI_ASSERT(VK_SUCCESS == vk_res);
        }
    }

    if (isRT)
    {
        VkImageViewCreateInfo rtvDesc = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL };
        rtvDesc.flags = 0;
        rtvDesc.image = pTexture->pVkImage;
        rtvDesc.viewType = view_type;
        rtvDesc.format = util_to_vk_format(pRenderer, pTexture->desc.format);
        rtvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
        rtvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
        rtvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
        rtvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
        rtvDesc.subresourceRange.aspectMask = util_vk_determine_aspect_mask(rtvDesc.format, true);
        rtvDesc.subresourceRange.baseMipLevel = 0;
        rtvDesc.subresourceRange.levelCount = 1;
        rtvDesc.subresourceRange.baseArrayLayer = 0;
        rtvDesc.subresourceRange.layerCount = 1;

        uint32_t depthOrArraySize = 1;
        uint32_t numRTVs = desc.mipLevels;
        if ((desc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (desc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            depthOrArraySize = desc.arraySize * desc.depth;
            numRTVs *= depthOrArraySize;
        }

        pTexture->pVkRTDescriptors = persistentAlloc.alloc<VkImageView>(numRTVs);

        for (uint32_t i = 0; i < desc.mipLevels; ++i)
        {
            rtvDesc.subresourceRange.baseMipLevel = i;
            for (uint32_t j = 0; j < depthOrArraySize; ++j)
            {
                rtvDesc.subresourceRange.baseArrayLayer = j;
                VkResult vkRes = vkCreateImageView(
                    pRenderer->pVkDevice, &rtvDesc, NULL, &pTexture->pVkRTDescriptors[i * depthOrArraySize + j]);
                REI_ASSERT(VK_SUCCESS == vkRes);
            }
        }
    }

    /************************************************************************/
    /************************************************************************/
    // Get memory requirements that covers all mip levels
    DECLARE_ZERO(VkMemoryRequirements, vk_mem_reqs);
    vkGetImageMemoryRequirements(pRenderer->pVkDevice, pTexture->pVkImage, &vk_mem_reqs);

    if (pDesc->pDebugName)
    {
        size_t nameLen = wcslen(pDesc->pDebugName);
        desc.pDebugName = persistentAlloc.alloc<wchar_t>(nameLen + 1);
        wcscpy((wchar_t*)desc.pDebugName, pDesc->pDebugName);
        (wchar_t&)desc.pDebugName[nameLen] = 0;
    }

    *ppTexture = pTexture;
}

void REI_removeTexture(REI_Renderer* pRenderer, REI_Texture* pTexture)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pTexture);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pTexture->pVkImage);

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    if (pTexture->ownsImage)
        vmaDestroyImage(pRenderer->pVmaAllocator, pTexture->pVkImage, pTexture->pVkAllocation);

    if (VK_NULL_HANDLE != pTexture->pVkSRVDescriptor)
        vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkSRVDescriptor, NULL);

    if (VK_NULL_HANDLE != pTexture->pVkSRVStencilDescriptor)
        vkDestroyImageView(pRenderer->pVkDevice, *pTexture->pVkSRVStencilDescriptor, NULL);

    if (pTexture->pVkUAVDescriptors)
    {
        for (uint32_t i = 0; i < pTexture->desc.mipLevels; ++i)
        {
            vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkUAVDescriptors[i], NULL);
        }
    }

    if (pTexture->pVkRTDescriptors)
    {
        uint32_t depthOrArraySize = 1;
        if ((pTexture->desc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES) ||
            (pTexture->desc.descriptors & REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES))
        {
            depthOrArraySize = pTexture->desc.arraySize * pTexture->desc.depth;
        }

        for (uint32_t i = 0; i < pTexture->desc.mipLevels; ++i)
        {
            for (uint32_t j = 0; j < depthOrArraySize; ++j)
            {
                vkDestroyImageView(pRenderer->pVkDevice, pTexture->pVkRTDescriptors[i * depthOrArraySize + j], NULL);
            }
        }
    }

    allocator.pFree(allocator.pUserData, pTexture);
}

void REI_addSampler(REI_Renderer* pRenderer, const REI_SamplerDesc* pDesc, REI_Sampler** pp_sampler)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(pDesc->compareFunc < REI_MAX_COMPARE_MODES);

    REI_Sampler* pSampler = (REI_Sampler*)REI_calloc(pRenderer->allocator, sizeof(*pSampler));
    REI_ASSERT(pSampler);

    DECLARE_ZERO(VkSamplerCreateInfo, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.magFilter = util_to_vk_filter(pDesc->magFilter);
    add_info.minFilter = util_to_vk_filter(pDesc->minFilter);
    add_info.mipmapMode = util_to_vk_mip_map_mode(pDesc->mipmapMode);
    add_info.addressModeU = util_to_vk_address_mode(pDesc->addressU);
    add_info.addressModeV = util_to_vk_address_mode(pDesc->addressV);
    add_info.addressModeW = util_to_vk_address_mode(pDesc->addressW);
    add_info.mipLodBias = pDesc->mipLodBias;
    add_info.anisotropyEnable = (pDesc->maxAnisotropy > 0.0f) ? VK_TRUE : VK_FALSE;
    add_info.maxAnisotropy = pDesc->maxAnisotropy;
    add_info.compareEnable =
        (gVkComparisonFuncTranslator[pDesc->compareFunc] != VK_COMPARE_OP_NEVER) ? VK_TRUE : VK_FALSE;
    add_info.compareOp = gVkComparisonFuncTranslator[pDesc->compareFunc];
    add_info.minLod = 0.0f;
    add_info.maxLod = FLT_MAX;
    add_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    add_info.unnormalizedCoordinates = VK_FALSE;

    VkResult vk_res = vkCreateSampler(pRenderer->pVkDevice, &add_info, NULL, &(pSampler->pVkSampler));
    REI_ASSERT(VK_SUCCESS == vk_res);

    pSampler->vkSamplerView.sampler = pSampler->pVkSampler;

    *pp_sampler = pSampler;
}

void REI_removeSampler(REI_Renderer* pRenderer, REI_Sampler* pSampler)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pSampler);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pSampler->pVkSampler);

    vkDestroySampler(pRenderer->pVkDevice, pSampler->pVkSampler, NULL);

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pSampler);
}
/************************************************************************/
// REI_Buffer Functions
/************************************************************************/
void REI_mapBuffer(REI_Renderer* pRenderer, REI_Buffer* pBuffer, void** pMappedMem)
{
    REI_ASSERT(
        pBuffer->desc.memoryUsage != REI_RESOURCE_MEMORY_USAGE_GPU_ONLY && "Trying to map non-cpu accessible resource");

    if (pBuffer->pCpuMappedAddress)
    {
        *pMappedMem = pBuffer->pCpuMappedAddress;
    }
    else
    {
        VkResult vk_res = vmaMapMemory(pRenderer->pVmaAllocator, pBuffer->pVkAllocation, pMappedMem);
        REI_ASSERT(vk_res == VK_SUCCESS);
    }
}

void REI_unmapBuffer(REI_Renderer* pRenderer, REI_Buffer* pBuffer)
{
    REI_ASSERT(
        pBuffer->desc.memoryUsage != REI_RESOURCE_MEMORY_USAGE_GPU_ONLY &&
        "Trying to unmap non-cpu accessible resource");

    if (!pBuffer->pCpuMappedAddress)
        vmaUnmapMemory(pRenderer->pVmaAllocator, pBuffer->pVkAllocation);
}

/************************************************************************/
// Descriptor Set Functions
/************************************************************************/
void REI_addDescriptorTableArray(
    REI_Renderer* pRenderer, const REI_DescriptorTableArrayDesc* pDesc, REI_DescriptorTableArray** ppDescriptorTableArr)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(ppDescriptorTableArr);

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    const REI_RootSignature*      pRootSignature = pDesc->pRootSignature;
    const REI_DescriptorTableSlot slot = pDesc->slot;
    REI_DescriptorTableArray*     pDescriptorTableArr = nullptr;

    if (pRootSignature->vkDescriptorSetLayouts[slot] == VK_NULL_HANDLE)
    {
        pRenderer->pLog(
            REI_LOG_TYPE_ERROR, "NULL Descriptor Set Layout for update frequency %u. Cannot allocate descriptor set",
            (uint32_t)slot);
        ppDescriptorTableArr = nullptr;
        return;
    }
    uint32_t numDescriptors = pRootSignature->vkCumulativeDescriptorCounts[slot];

    REI_StackAllocator<false> structAlloc = { 0 };
    size_t                    scratchMemSize =
        numDescriptors * pDesc->maxTables *
        std::max(sizeof(VkDescriptorImageInfo), std::max(sizeof(VkDescriptorBufferInfo), sizeof(VkBufferView)));

    structAlloc.reserve<REI_DescriptorTableArray>()
        .reserve<VkDescriptorSet>(pDesc->maxTables)
        .reserve<REI_BindingInfo>(numDescriptors)
        .reserve<VkWriteDescriptorSet>(numDescriptors * pDesc->maxTables)
        .reserve<uint8_t>(scratchMemSize)
        .done(allocator);

    pDescriptorTableArr = structAlloc.alloc<REI_DescriptorTableArray>();

    pDescriptorTableArr->slot = slot;
    pDescriptorTableArr->maxTables = pDesc->maxTables;
    pDescriptorTableArr->numDescriptors = numDescriptors;

    pDescriptorTableArr->pHandles = structAlloc.alloc<VkDescriptorSet>(pDesc->maxTables);
    pDescriptorTableArr->pDescriptorBindings = structAlloc.alloc<REI_BindingInfo>(numDescriptors);
    pDescriptorTableArr->pWriteDescriptorSets = structAlloc.alloc<VkWriteDescriptorSet>(numDescriptors);

    uint32_t firstDescriptorIndex = pRootSignature->mDescriptorIndexToBindingOffset[slot];
    for (uint32_t i = 0; i < numDescriptors; ++i)
    {
        VkWriteDescriptorSet& writeSet = pDescriptorTableArr->pWriteDescriptorSets[i];
        writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeSet.pNext = nullptr;
        writeSet.dstArrayElement = 0;

        pDescriptorTableArr->pDescriptorBindings[i] = pRootSignature->pDescriptorBindings[firstDescriptorIndex + i];
    }

    pDescriptorTableArr->scratchMem = structAlloc.alloc<uint8_t>(scratchMemSize);
    pDescriptorTableArr->scratchMemSize = scratchMemSize;
    REI_ASSERT(scratchMemSize >= sizeof(VkDescriptorSetLayout) * pDesc->maxTables);
    VkDescriptorSetLayout* pLayouts = (VkDescriptorSetLayout*)pDescriptorTableArr->scratchMem;
    for (uint32_t i = 0; i < pDesc->maxTables; ++i)
    {
        pLayouts[i] = pRootSignature->vkDescriptorSetLayouts[slot];
    }

    consume_descriptor_sets(
        pRenderer->pDescriptorPool, pLayouts, pDescriptorTableArr->pHandles, pDesc->maxTables,
        &pDescriptorTableArr->poolIndex);

    *ppDescriptorTableArr = pDescriptorTableArr;
}

void REI_removeDescriptorTableArray(REI_Renderer* pRenderer, REI_DescriptorTableArray* pDescriptorTableArr)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDescriptorTableArr);

    return_descriptor_sets(
        pRenderer->pDescriptorPool, pDescriptorTableArr->poolIndex, pDescriptorTableArr->maxTables,
        pDescriptorTableArr->pHandles);

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    allocator.pFree(allocator.pUserData, pDescriptorTableArr);
}

void REI_updateDescriptorTableArray(
    REI_Renderer* pRenderer, REI_DescriptorTableArray* pDescriptorTableArr, uint32_t count,
    const REI_DescriptorData* pParams)
{
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

    REI_ASSERT(pRenderer);
    REI_ASSERT(pDescriptorTableArr);
    REI_ASSERT(pDescriptorTableArr->pHandles);
    REI_ASSERT(count > 0);

    REI_StackAllocator<false>    scratchAlloc = { pDescriptorTableArr->scratchMem, pDescriptorTableArr->scratchMemSize };
    VkWriteDescriptorSet* writeDescriptorSets = pDescriptorTableArr->pWriteDescriptorSets;

    for (uint32_t i = 0; i < count; ++i)
    {
        const REI_DescriptorData* pParam = pParams + i;

        uint32_t descriptorIndex = pParam->descriptorIndex;
        VALIDATE_DESCRIPTOR(descriptorIndex != -1, "REI_DescriptorData has invalid descriptorIndex");
        VALIDATE_DESCRIPTOR(
            descriptorIndex < pDescriptorTableArr->numDescriptors, "REI_DescriptorData has invalid descriptorIndex");

        const REI_DescriptorType type = pParam->descriptorType;
        const uint32_t           arrayCount = REI_max(1U, pParam->count);
        uint32_t                 tableIndex = pParam->tableIndex;
        REI_ASSERT(tableIndex < pDescriptorTableArr->maxTables);

        VkWriteDescriptorSet& curWrite = writeDescriptorSets[i];
        curWrite.descriptorCount = arrayCount;
        curWrite.dstBinding = pDescriptorTableArr->pDescriptorBindings[descriptorIndex].binding;
        curWrite.dstArrayElement =
            descriptorIndex - pDescriptorTableArr->pDescriptorBindings[descriptorIndex].firstArrayElement;
        curWrite.dstSet = pDescriptorTableArr->pHandles[tableIndex];

        switch (type)
        {
            case REI_DESCRIPTOR_TYPE_SAMPLER:
            {
                VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL REI_Sampler");

                VkDescriptorImageInfo* pImageInfo = scratchAlloc.alloc<VkDescriptorImageInfo>(arrayCount);
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL REI_Sampler ");
                    pImageInfo[arr] = pParam->ppSamplers[arr]->vkSamplerView;
                }

                curWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
                curWrite.pImageInfo = pImageInfo;
                break;
            }
            case REI_DESCRIPTOR_TYPE_TEXTURE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL REI_Texture");

                VkDescriptorImageInfo* pImageInfo = scratchAlloc.alloc<VkDescriptorImageInfo>(arrayCount);
                if (!pParam->bindStencilResource)
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL REI_Texture");

                        pImageInfo[arr] = {
                            VK_NULL_HANDLE,                               // REI_Sampler
                            pParam->ppTextures[arr]->pVkSRVDescriptor,    // Image View
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL      // Image Layout
                        };
                    }
                }
                else
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL REI_Texture");

                        pImageInfo[arr] = {
                            VK_NULL_HANDLE,                                       // REI_Sampler
                            *pParam->ppTextures[arr]->pVkSRVStencilDescriptor,    // Image View
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL              // Image Layout
                        };
                    }
                }
                curWrite.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                curWrite.pImageInfo = pImageInfo;
                break;
            }
            case REI_DESCRIPTOR_TYPE_RW_TEXTURE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW REI_Texture");
                const uint32_t mipSlice = pParam->UAVMipSlice;

                VkDescriptorImageInfo* pImageInfo = scratchAlloc.alloc<VkDescriptorImageInfo>(arrayCount);
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppTextures[arr], "NULL RW REI_Texture (%s [%u] )");
                    VALIDATE_DESCRIPTOR(
                        mipSlice < pParam->ppTextures[arr]->desc.mipLevels,
                        "Descriptor : () Mip Slice (%u) exceeds mip levels (%u)", mipSlice,
                        pParam->ppTextures[arr]->desc.mipLevels);

                    pImageInfo[arr] = {
                        VK_NULL_HANDLE,                                          // REI_Sampler
                        pParam->ppTextures[arr]->pVkUAVDescriptors[mipSlice],    // Image View
                        VK_IMAGE_LAYOUT_GENERAL                                  // Image Layout
                    };
                }
                curWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                curWrite.pImageInfo = pImageInfo;
                break;
            }
            case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER: 
            case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: 
            case REI_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            case REI_DESCRIPTOR_TYPE_BUFFER:
            case REI_DESCRIPTOR_TYPE_BUFFER_RAW:
            case REI_DESCRIPTOR_TYPE_RW_BUFFER:
            case REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL REI_Buffer");

                VkDescriptorBufferInfo* pBufferInfo = scratchAlloc.alloc<VkDescriptorBufferInfo>(arrayCount);
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL REI_Buffer");

                    pBufferInfo[arr] = pParam->ppBuffers[arr]->vkBufferInfo;
                    if (pParam->pOffsets || pParam->pSizes)
                    {
                        VALIDATE_DESCRIPTOR(pParam->pSizes, "Descriptor - pSizes must be provided with pOffsets");
                        VALIDATE_DESCRIPTOR(pParam->pSizes[arr] > 0, "Descriptor - pSizes[%u] is zero", arr);
                        VALIDATE_DESCRIPTOR(
                            (type != REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
                             type != REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                                pParam->pSizes[arr] <=
                                    pRenderer->vkDeviceProperties.properties.limits.maxUniformBufferRange,
                            "Descriptor - pSizes[%u] is %ull which exceeds max size %u", arr, pParam->pSizes[arr],
                            pRenderer->vkDeviceProperties.properties.limits.maxUniformBufferRange);

                        VALIDATE_DESCRIPTOR(
                            (type == REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                             type == REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ||
                                pParam->pSizes[arr] <=
                                    pRenderer->vkDeviceProperties.properties.limits.maxStorageBufferRange,
                            "Descriptor - pSizes[%u] is %ull which exceeds max size %u", arr, pParam->pSizes[arr],
                            pRenderer->vkDeviceProperties.properties.limits.maxStorageBufferRange);

                        pBufferInfo[arr].offset = pParam->pOffsets ? pParam->pOffsets[arr] : 0;
                        pBufferInfo[arr].range = pParam->pSizes[arr];
                    }
                }
                curWrite.descriptorType = util_to_vk_descriptor_type(type);

                curWrite.pBufferInfo = pBufferInfo;
                break;
            }
            case REI_DESCRIPTOR_TYPE_TEXEL_BUFFER:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Texel REI_Buffer");
                VkBufferView* pBufferView = scratchAlloc.alloc<VkBufferView>(arrayCount);
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL Texel REI_Buffer [%u]", arr);
                    pBufferView[arr] = pParam->ppBuffers[arr]->pVkUniformTexelView;
                }
                curWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                curWrite.pTexelBufferView = pBufferView;
                break;
            }
            case REI_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Texel REI_Buffer");
                VkBufferView* pBufferView = scratchAlloc.alloc<VkBufferView>(arrayCount);
                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers[arr], "NULL RW Texel REI_Buffer [%u]", arr);
                    pBufferView[arr] = pParam->ppBuffers[arr]->pVkStorageTexelView;
                }
                curWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
                curWrite.pTexelBufferView = pBufferView;
                break;
            }

            default: REI_ASSERT(false); break;
        }
    }

    vkUpdateDescriptorSets(pRenderer->pVkDevice, count, writeDescriptorSets, 0, nullptr);
}

void REI_cmdBindDescriptorTable(REI_Cmd* pCmd, uint32_t tableIndex, REI_DescriptorTableArray* pDescriptorTableArr)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pDescriptorTableArr);
    REI_ASSERT(pDescriptorTableArr->pHandles);
    REI_ASSERT(tableIndex < pDescriptorTableArr->maxTables);

    const REI_RootSignature* pRootSignature = pCmd->pBoundRootSignature;

    vkCmdBindDescriptorSets(
        pCmd->pVkCmdBuf, pRootSignature->pipelineType, pRootSignature->pPipelineLayout, pDescriptorTableArr->slot, 1,
        &pDescriptorTableArr->pHandles[tableIndex], 0, NULL);
}

void REI_cmdBindDescriptorTableVK(
    REI_Cmd* pCmd, uint32_t tableIndex, REI_DescriptorTableArray* pDescriptorTableArr, uint32_t dynamicOffsetCount,
    const uint32_t* pDynamicOffsets)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pDescriptorTableArr);
    REI_ASSERT(pDescriptorTableArr->pHandles);
    REI_ASSERT(tableIndex < pDescriptorTableArr->maxTables);

    const REI_RootSignature* pRootSignature = pCmd->pBoundRootSignature;

    vkCmdBindDescriptorSets(
        pCmd->pVkCmdBuf, pRootSignature->pipelineType, pRootSignature->pPipelineLayout, pDescriptorTableArr->slot, 1,
        &pDescriptorTableArr->pHandles[tableIndex], dynamicOffsetCount, pDynamicOffsets);
}

void REI_cmdBindPushConstants(
    REI_Cmd* pCmd, REI_RootSignature* pRootSignature, REI_ShaderStage stages, uint32_t offset, uint32_t size,
    const void* pConstants)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pConstants);
    REI_ASSERT(pRootSignature);
    REI_ASSERT(pRootSignature->vkPushConstantCount);

    vkCmdPushConstants(
        pCmd->pVkCmdBuf, pRootSignature->pPipelineLayout, util_to_vk_shader_stage_flags(stages), offset, size,
        pConstants);
}

void REI_addShaders(
    REI_Renderer* pRenderer, const REI_ShaderDesc* pDescs, uint32_t shaderCount, REI_Shader** ppShaderPrograms)
{
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(!shaderCount || pDescs);
    REI_ASSERT(!shaderCount || ppShaderPrograms);
    
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    for (uint32_t i = 0; i < shaderCount; ++i)
    {
        const char* srcEntryPointStr = pDescs[i].pEntryPoint ? pDescs[i].pEntryPoint : gDefaultEntryPointName;
        size_t      srcStrBufferLen = strlen(srcEntryPointStr) + 1;

        REI_StackAllocator<false> persistentAlloc = { 0 };
        persistentAlloc.reserve<REI_Shader>().reserve<char>(srcStrBufferLen);

        if (!persistentAlloc.done(allocator))
        {
            pLog(REI_LOG_TYPE_ERROR, "REI_addShader wasn't able to allocate enough memory for persistentAlloc");
            REI_ASSERT(false);
            ppShaderPrograms[i] = nullptr;
            continue;
        }

        REI_Shader* pShaderProgram = persistentAlloc.allocZeroed<REI_Shader>();
        pShaderProgram->pEntryPoint = persistentAlloc.alloc<char>(srcStrBufferLen);

        memcpy(pShaderProgram->pEntryPoint, srcEntryPointStr, srcStrBufferLen);
        pShaderProgram->stage = util_to_vk_shader_stage_flag_bit(pDescs[i].stage);

        DECLARE_ZERO(VkShaderModuleCreateInfo, create_info);
        create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        create_info.pNext = NULL;
        create_info.flags = 0;
        create_info.codeSize = pDescs[i].byteCodeSize;
        create_info.pCode = (const uint32_t*)pDescs[i].pByteCode;
        VkResult vk_res = vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &pShaderProgram->shaderModule);
        REI_ASSERT(VK_SUCCESS == vk_res);

        ppShaderPrograms[i] = pShaderProgram;
    }
}

void REI_removeShaders(REI_Renderer* pRenderer, uint32_t shaderCount, REI_Shader** ppShaderPrograms)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    for (uint32_t i = 0; i < shaderCount; ++i) 
    {
        vkDestroyShaderModule(pRenderer->pVkDevice, ppShaderPrograms[i]->shaderModule, NULL);
        allocator.pFree(allocator.pUserData, ppShaderPrograms[i]);
    }
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/

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
        *ppRootSignature = nullptr;
        return;
    }

    uint32_t totalDescriptorCount = 0;
    uint32_t totalVkBindingCount = pRootSignatureDesc->staticSamplerBindingCount;
    uint32_t descriptorIndexToBindingOffset[REI_DESCRIPTOR_TABLE_SLOT_COUNT] = { 0 };
    uint32_t lastUsedSlot = 0;

    for (uint32_t set = 0; set < pRootSignatureDesc->tableLayoutCount; ++set)
    {
        REI_DescriptorTableLayout& setLayout = pRootSignatureDesc->pTableLayouts[set];
        REI_ASSERT(setLayout.slot < REI_DESCRIPTOR_TABLE_SLOT_COUNT);

        totalVkBindingCount += setLayout.bindingCount;
        descriptorIndexToBindingOffset[setLayout.slot] = totalDescriptorCount;
        for (uint32_t binding = 0; binding < setLayout.bindingCount; ++binding)
            totalDescriptorCount += setLayout.pBindings[binding].descriptorCount;
    }

    uint32_t totalStaticSamplerCount = 0;
    for (uint32_t binding = 0; binding < pRootSignatureDesc->staticSamplerBindingCount; ++binding) 
    {
        REI_ASSERT(pRootSignatureDesc->pStaticSamplerBindings);
        for (uint32_t i = 0; i < pRootSignatureDesc->staticSamplerBindingCount; ++i) 
        {
            REI_StaticSamplerBinding&     setLayoutBinding = pRootSignatureDesc->pStaticSamplerBindings[i];
            totalStaticSamplerCount += setLayoutBinding.descriptorCount;
        }
    }

    REI_StackAllocator<true> stackAlloc = { 0 };
    stackAlloc.reserve<VkDescriptorSetLayoutBinding>(totalVkBindingCount)
        .reserve<VkSampler>(totalStaticSamplerCount)
        .reserve<VkPushConstantRange>(pRootSignatureDesc->pushConstantRangeCount);

    if (!stackAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "REI_addRootSignature wasn't able to allocate enough memory for stackAlloc");
        REI_ASSERT(false);
        *ppRootSignature = nullptr;
        return;
    }

    REI_StackAllocator<false> persistentAlloc = { 0 };
    persistentAlloc.reserve<REI_RootSignature>().reserve<REI_BindingInfo>(totalDescriptorCount);

    if (!persistentAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "REI_addRootSignature wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        *ppRootSignature = nullptr;
        return;
    }

    REI_RootSignature* pRootSignature = persistentAlloc.allocZeroed<REI_RootSignature>();

    REI_ASSERT(
        pRootSignatureDesc->pipelineType == REI_PIPELINE_TYPE_GRAPHICS ||
        pRootSignatureDesc->pipelineType == REI_PIPELINE_TYPE_COMPUTE);

    if (pRootSignatureDesc->pipelineType == REI_PIPELINE_TYPE_GRAPHICS)
        pRootSignature->pipelineType = VK_PIPELINE_BIND_POINT_GRAPHICS;
    else if (pRootSignatureDesc->pipelineType == REI_PIPELINE_TYPE_COMPUTE)
        pRootSignature->pipelineType = VK_PIPELINE_BIND_POINT_COMPUTE;

    pRootSignature->descriptorCount = totalDescriptorCount;
    pRootSignature->pDescriptorBindings = persistentAlloc.alloc<REI_BindingInfo>(totalDescriptorCount);
    memcpy(
        pRootSignature->mDescriptorIndexToBindingOffset, descriptorIndexToBindingOffset,
        sizeof(descriptorIndexToBindingOffset));

    pRootSignature->mStaticSamplerSlot = UINT32_MAX;
    pRootSignature->mStaticSamplerSetPoolIndex = UINT32_MAX;
    if (pRootSignatureDesc->staticSamplerBindingCount)
    {
        REI_ASSERT(pRootSignatureDesc->pStaticSamplerBindings);
        pRootSignature->mStaticSamplerSlot = pRootSignatureDesc->staticSamplerSlot;

        VkDescriptorSetLayoutBinding* pVkBindings =
            stackAlloc.alloc<VkDescriptorSetLayoutBinding>(pRootSignatureDesc->staticSamplerBindingCount);
        REI_ASSERT(pVkBindings);
        VkSampler*            staticSamplers = stackAlloc.alloc<VkSampler>(totalStaticSamplerCount);
        uint32_t              staticSamplerOffset = 0;
        VkShaderStageFlags    staticSamplerShaderFlags =
            util_to_vk_shader_stage_flags(pRootSignatureDesc->staticSamplerStageFlags);

        for (uint32_t i = 0; i < pRootSignatureDesc->staticSamplerBindingCount; ++i)
        {
            REI_StaticSamplerBinding&     setLayoutBinding = pRootSignatureDesc->pStaticSamplerBindings[i];
            VkDescriptorSetLayoutBinding& curVkBinding = pVkBindings[i];
            curVkBinding.binding = setLayoutBinding.binding;
            curVkBinding.descriptorCount = setLayoutBinding.descriptorCount;
            curVkBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
            curVkBinding.stageFlags = staticSamplerShaderFlags;

            pLog(REI_LOG_TYPE_INFO, "User specified static sampler. binding = %u", setLayoutBinding.binding);

            for (uint32_t ssampler = 0; ssampler < curVkBinding.descriptorCount; ++ssampler)
                staticSamplers[ssampler + staticSamplerOffset] =
                    setLayoutBinding.ppStaticSamplers[ssampler]->pVkSampler;

            curVkBinding.pImmutableSamplers = staticSamplers + staticSamplerOffset;

            staticSamplerOffset += curVkBinding.descriptorCount;
        }
        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = NULL;
        layoutInfo.bindingCount = (uint32_t)pRootSignatureDesc->staticSamplerBindingCount;
        layoutInfo.pBindings = pVkBindings;
        layoutInfo.flags = 0;

        VkResult result = vkCreateDescriptorSetLayout(
            pRenderer->pVkDevice, &layoutInfo, NULL,
            &pRootSignature->vkDescriptorSetLayouts[pRootSignature->mStaticSamplerSlot]);

        REI_ASSERT(result == VK_SUCCESS);

        consume_descriptor_sets(
            pRenderer->pDescriptorPool, &pRootSignature->vkDescriptorSetLayouts[pRootSignature->mStaticSamplerSlot],
            &pRootSignature->vkStaticSamplerSet, 1, &pRootSignature->mStaticSamplerSetPoolIndex);

        lastUsedSlot = pRootSignature->mStaticSamplerSlot;
    }

    for (uint32_t i = 0; i < pRootSignatureDesc->tableLayoutCount; ++i)
    {
        REI_DescriptorTableLayout& setLayout = pRootSignatureDesc->pTableLayouts[i];
        uint32_t                   slot = setLayout.slot;

        if (slot == pRootSignature->mStaticSamplerSlot)
        {
            pLog(REI_LOG_TYPE_ERROR, "All static samplers must be in a separate set");
            REI_ASSERT(false);
        }
        lastUsedSlot = slot > lastUsedSlot ? slot : lastUsedSlot;

        VkDescriptorSetLayoutBinding* pVkBindings =
            stackAlloc.alloc<VkDescriptorSetLayoutBinding>(setLayout.bindingCount);
        REI_ASSERT(pVkBindings);

        VkShaderStageFlags setLayoutStageFlags = util_to_vk_shader_stage_flags(setLayout.stageFlags);
        uint32_t           offsetToBindingArray = pRootSignature->mDescriptorIndexToBindingOffset[slot];
        for (uint32_t handleIndex = 0; handleIndex < setLayout.bindingCount; ++handleIndex)
        {
            REI_DescriptorBinding& setLayoutBinding = setLayout.pBindings[handleIndex];

            VkDescriptorSetLayoutBinding& curVkBinding = pVkBindings[handleIndex];
            curVkBinding.binding = setLayoutBinding.binding;
            curVkBinding.descriptorCount = setLayoutBinding.descriptorCount;
            curVkBinding.descriptorType = util_to_vk_descriptor_type(setLayoutBinding.descriptorType);
            curVkBinding.stageFlags = setLayoutStageFlags;
            curVkBinding.pImmutableSamplers = nullptr;

            for (uint32_t arr = 0; arr < setLayoutBinding.descriptorCount; ++arr)
            {
                pRootSignature->pDescriptorBindings[offsetToBindingArray + arr] = {
                    setLayoutBinding.binding, pRootSignature->vkCumulativeDescriptorCounts[slot]
                };
            }
            pRootSignature->vkCumulativeDescriptorCounts[slot] += setLayoutBinding.descriptorCount;
            offsetToBindingArray += setLayoutBinding.descriptorCount;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.pNext = NULL;
        layoutInfo.bindingCount = (uint32_t)setLayout.bindingCount;
        layoutInfo.pBindings = pVkBindings;
        layoutInfo.flags = 0;

        VkResult result = vkCreateDescriptorSetLayout(
            pRenderer->pVkDevice, &layoutInfo, NULL, &pRootSignature->vkDescriptorSetLayouts[slot]);

        REI_ASSERT(result == VK_SUCCESS);
    }

    pRootSignature->vkPushConstantCount = pRootSignatureDesc->pushConstantRangeCount;
    VkPushConstantRange* pVkPushConstantRanges = nullptr;
    if (pRootSignature->vkPushConstantCount)
        pVkPushConstantRanges = stackAlloc.alloc<VkPushConstantRange>(pRootSignature->vkPushConstantCount);

    // Create push constant ranges
    for (uint32_t i = 0; i < pRootSignature->vkPushConstantCount; ++i)
    {
        VkPushConstantRange* pConst = &pVkPushConstantRanges[i];

        REI_PushConstantRange& constDesc = pRootSignatureDesc->pPushConstantRanges[i];

        pConst->offset = constDesc.offset;
        pConst->size = constDesc.size;
        pConst->stageFlags = util_to_vk_shader_stage_flags(constDesc.stageFlags);
    }

    bool create_empty_sets = false;
    for (uint32_t slot = lastUsedSlot; slot > 0; --slot)
    {
        if (!create_empty_sets && pRootSignature->vkDescriptorSetLayouts[slot])
            create_empty_sets = true;

        if (create_empty_sets && !pRootSignature->vkDescriptorSetLayouts[slot - 1])
        {
            pRootSignature->vkDescriptorSetLayouts[slot - 1] = pRenderer->pVkEmptyDescriptorSetLayout;
        }
    }

    pRootSignature->mMaxUsedSlots =
        pRootSignatureDesc->tableLayoutCount + (pRootSignatureDesc->staticSamplerBindingCount != 0);

    //if user passed at least one table layout we should correct mMaxUsedSlots to add empty sets that are between the actually specified
    if (pRootSignature->mMaxUsedSlots)
        pRootSignature->mMaxUsedSlots = lastUsedSlot + 1;

    /************************************************************************/
    // REI_Pipeline layout
    /************************************************************************/

    DECLARE_ZERO(VkPipelineLayoutCreateInfo, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.setLayoutCount = pRootSignature->mMaxUsedSlots;
    add_info.pSetLayouts = pRootSignature->vkDescriptorSetLayouts;
    add_info.pushConstantRangeCount = pRootSignature->vkPushConstantCount;
    add_info.pPushConstantRanges = pVkPushConstantRanges;
    VkResult vk_res = vkCreatePipelineLayout(pRenderer->pVkDevice, &add_info, NULL, &(pRootSignature->pPipelineLayout));
    REI_ASSERT(VK_SUCCESS == vk_res);

    *ppRootSignature = pRootSignature;
}

void REI_removeRootSignature(REI_Renderer* pRenderer, REI_RootSignature* pRootSignature)
{
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;

    if (pRootSignature->mStaticSamplerSetPoolIndex != UINT32_MAX && pRootSignature->vkStaticSamplerSet)
        return_descriptor_sets(
            pRenderer->pDescriptorPool, pRootSignature->mStaticSamplerSetPoolIndex, 1,
            &pRootSignature->vkStaticSamplerSet);

    for (uint32_t i = 0; i < pRootSignature->mMaxUsedSlots; ++i)
    {
        if (pRootSignature->vkDescriptorSetLayouts[i] != pRenderer->pVkEmptyDescriptorSetLayout)
            vkDestroyDescriptorSetLayout(pRenderer->pVkDevice, pRootSignature->vkDescriptorSetLayouts[i], NULL);
    }

    vkDestroyPipelineLayout(pRenderer->pVkDevice, pRootSignature->pPipelineLayout, NULL);

    allocator.pFree(allocator.pUserData, pRootSignature);
}

/************************************************************************/
// Pipeline State Functions
/************************************************************************/

void REI_addPipelineCache(
    REI_Renderer* pRenderer, const REI_PipelineCacheDesc* pDesc, REI_PipelineCache** ppPipelineCache)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(ppPipelineCache);

    REI_PipelineCache* pPipelineCache = REI_new<REI_PipelineCache>(pRenderer->allocator);
    REI_ASSERT(pPipelineCache);

    VkPipelineCacheCreateInfo psoCacheCreateInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    psoCacheCreateInfo.initialDataSize = pDesc->mSize;
    psoCacheCreateInfo.pInitialData = pDesc->pData;

    VkPipelineCacheCreateFlags flags = 0;

    if (!pRenderer->hasPipelineCreationCacheControlExtension)
    {
        pRenderer->pLog(REI_LOG_TYPE_ERROR, "Device do not has PipelineCreationCacheControlExtension");
        REI_delete(pRenderer->allocator, pPipelineCache);
        *ppPipelineCache = nullptr;
        return;
    }
    if (pDesc->mFlags & PIPELINE_CACHE_FLAG_EXTERNALLY_SYNCHRONIZED)
    {
        flags |= VK_PIPELINE_CACHE_CREATE_EXTERNALLY_SYNCHRONIZED_BIT_EXT;
    }

    psoCacheCreateInfo.flags = flags;
    
    VkResult vk_res = vkCreatePipelineCache(pRenderer->pVkDevice, &psoCacheCreateInfo, NULL, &pPipelineCache->pCache);

    if (VK_SUCCESS != vk_res) 
    {
        REI_ASSERT(false);
        REI_removePipelineCache(pRenderer, pPipelineCache);
        pPipelineCache = nullptr;
    }
    *ppPipelineCache = pPipelineCache;
}

void REI_removePipelineCache(REI_Renderer* pRenderer, REI_PipelineCache* pPipelineCache)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pPipelineCache);

    if (pPipelineCache->pCache)
    {
        vkDestroyPipelineCache(pRenderer->pVkDevice, pPipelineCache->pCache, NULL);
    }

    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pPipelineCache);
}

void REI_getPipelineCacheData(REI_Renderer* pRenderer, REI_PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pPipelineCache);
    REI_ASSERT(pSize);

    if (pPipelineCache->pCache)
    {
        VkResult vk_res = vkGetPipelineCacheData(pRenderer->pVkDevice, pPipelineCache->pCache, pSize, pData);
        REI_ASSERT(VK_SUCCESS == vk_res);
    }
}

static void
    addGraphicsPipelineImpl(REI_Renderer* pRenderer, const REI_GraphicsPipelineDesc* pDesc, REI_Pipeline** ppPipeline)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(pDesc);
    REI_ASSERT(pDesc->ppShaderPrograms);
    REI_ASSERT(pDesc->shaderProgramCount);
    REI_ASSERT(pDesc->pRootSignature);

    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr                    pLog = pRenderer->pLog;

    REI_StackAllocator<true> stackAlloc = { 0 };
    stackAlloc.reserve<VkPipelineShaderStageCreateInfo>(REI_SHADER_STAGE_COUNT - 1)
        .reserve<VkVertexInputBindingDescription>(REI_MAX_VERTEX_BINDINGS)
        .reserve<VkVertexInputAttributeDescription>(REI_MAX_VERTEX_ATTRIBS)
        .reserve<VkPipelineVertexInputStateCreateInfo>()
        .reserve<VkPipelineInputAssemblyStateCreateInfo>()
        .reserve<VkPipelineTessellationStateCreateInfo>()
        .reserve<VkPipelineViewportStateCreateInfo>()
        .reserve<VkPipelineRasterizationStateCreateInfo>()
        .reserve<VkPipelineMultisampleStateCreateInfo>()
        .reserve<VkPipelineDepthStencilStateCreateInfo>()
        .reserve<VkPipelineColorBlendAttachmentState>(REI_MAX_RENDER_TARGET_ATTACHMENTS)
        .reserve<VkPipelineColorBlendStateCreateInfo>()
        .reserve<VkPipelineDynamicStateCreateInfo>()
        .reserve<VkGraphicsPipelineCreateInfo>()
        .reserve<uint8_t>(util_add_render_pass_stack_size_in_bytes());

    if (!stackAlloc.done(allocator))
    {
        pLog(REI_LOG_TYPE_ERROR, "addGraphicsPipelineImpl wasn't able to allocate enough memory for stackAlloc");
        REI_ASSERT(false);
        *ppPipeline = nullptr;
        return;
    }
    
    VkPipelineCache psoCache = pDesc->pCache ? pDesc->pCache->pCache : VK_NULL_HANDLE;

    REI_Pipeline* pPipeline = REI_new<REI_Pipeline>(pRenderer->allocator);
    REI_ASSERT(pPipeline);

    REI_Shader** const     ppShaderPrograms = pDesc->ppShaderPrograms;

    pPipeline->type = VK_PIPELINE_BIND_POINT_GRAPHICS;
    pPipeline->pRootSignature = pDesc->pRootSignature;

    // Create tempporary renderpass for pipeline creation
    VkRenderPass       renderPass = NULL;
    {
        REI_RenderPassDesc renderPassDesc = { 0 };
        renderPassDesc.renderTargetCount = pDesc->renderTargetCount;
        renderPassDesc.pColorFormats = pDesc->pColorFormats;
        renderPassDesc.sampleCount = pDesc->sampleCount;
        renderPassDesc.depthStencilFormat = pDesc->depthStencilFormat;
        REI_StackAllocator<false> tempAlloc = { stackAlloc.alloc<uint8_t>(util_add_render_pass_stack_size_in_bytes()),
                                                util_add_render_pass_stack_size_in_bytes() };
        add_render_pass(tempAlloc, pRenderer, &renderPassDesc, &renderPass);
    }
    uint32_t numShaderModules = pDesc->shaderProgramCount;
    REI_ASSERT(numShaderModules);

    // REI_Pipeline
    {
        VkPipelineShaderStageCreateInfo* stages =
            stackAlloc.allocZeroed<VkPipelineShaderStageCreateInfo>(REI_SHADER_STAGE_COUNT - 1);

        VkShaderStageFlags combinedStage = {};
        for (uint32_t i = 0; i < numShaderModules; ++i)
        {
            REI_ASSERT(ppShaderPrograms[i]->shaderModule != VK_NULL_HANDLE);
            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].pNext = NULL;
            stages[i].flags = 0;
            stages[i].pSpecializationInfo = NULL;
            stages[i].pName = ppShaderPrograms[i]->pEntryPoint;
            stages[i].stage = ppShaderPrograms[i]->stage;
            stages[i].module = ppShaderPrograms[i]->shaderModule;
            combinedStage |= stages[i].stage;
        }
        bool hasFullTessStage =
            (combinedStage &
             (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)) ==
            (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);

        uint32_t                         input_binding_count = 0;
        VkVertexInputBindingDescription* input_bindings =
            stackAlloc.allocZeroed<VkVertexInputBindingDescription>(REI_MAX_VERTEX_BINDINGS);
        uint32_t input_attribute_count =
            pDesc->vertexAttribCount > REI_MAX_VERTEX_ATTRIBS ? REI_MAX_VERTEX_ATTRIBS : pDesc->vertexAttribCount;
        VkVertexInputAttributeDescription* input_attributes =
            stackAlloc.allocZeroed<VkVertexInputAttributeDescription>(REI_MAX_VERTEX_ATTRIBS);

        if (input_attribute_count)
        {
            // Ignore everything that's beyond max_vertex_attribs
            uint32_t binding_value = UINT32_MAX;

            // Initial values
            for (uint32_t i = 0; i < input_attribute_count; ++i)
            {
                const REI_VertexAttrib& attrib = pDesc->pVertexAttribs[i];

                // TODO: fix this - as its based on not documented assumption
                if (binding_value != attrib.binding)
                {
                    binding_value = attrib.binding;
                    ++input_binding_count;
                }

                VkVertexInputBindingDescription& input_binding = input_bindings[input_binding_count - 1];
                input_binding.binding = binding_value;
                input_binding.inputRate = (attrib.rate == REI_VERTEX_ATTRIB_RATE_INSTANCE)
                                              ? VK_VERTEX_INPUT_RATE_INSTANCE
                                              : VK_VERTEX_INPUT_RATE_VERTEX;

                input_attributes[i].location = attrib.location;
                input_attributes[i].binding = attrib.binding;
                input_attributes[i].format = util_to_vk_format(pRenderer, attrib.format);
                input_attributes[i].offset = attrib.offset;
            }
        }

        VkPipelineVertexInputStateCreateInfo& vi = *stackAlloc.alloc<VkPipelineVertexInputStateCreateInfo>();
        vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vi.pNext = NULL;
        vi.flags = 0;
        vi.vertexBindingDescriptionCount = input_binding_count;
        vi.pVertexBindingDescriptions = input_bindings;
        vi.vertexAttributeDescriptionCount = input_attribute_count;
        vi.pVertexAttributeDescriptions = input_attributes;

        VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        switch (pDesc->primitiveTopo)
        {
            case REI_PRIMITIVE_TOPO_POINT_LIST: topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST; break;
            case REI_PRIMITIVE_TOPO_LINE_LIST: topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST; break;
            case REI_PRIMITIVE_TOPO_LINE_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; break;
            case REI_PRIMITIVE_TOPO_TRI_STRIP: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP; break;
            case REI_PRIMITIVE_TOPO_PATCH_LIST: topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST; break;
            case REI_PRIMITIVE_TOPO_TRI_LIST: topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; break;
            default: REI_ASSERT(false && "Primitive Topo not supported!"); break;
        }
        VkPipelineInputAssemblyStateCreateInfo& ia = *stackAlloc.alloc<VkPipelineInputAssemblyStateCreateInfo>();
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext = NULL;
        ia.flags = 0;
        ia.topology = topology;
        ia.primitiveRestartEnable = VK_FALSE;

        VkPipelineTessellationStateCreateInfo& ts = *stackAlloc.alloc<VkPipelineTessellationStateCreateInfo>();
        if (hasFullTessStage)
        {
            ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
            ts.pNext = NULL;
            ts.flags = 0;
            ts.patchControlPoints = pDesc->patchControlPoints;
        }

        VkPipelineViewportStateCreateInfo& vs = *stackAlloc.alloc<VkPipelineViewportStateCreateInfo>();
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.pNext = NULL;
        vs.flags = 0;
        // we are using dynimic viewports but we must set the count to 1
        vs.viewportCount = 1;
        vs.pViewports = NULL;
        vs.scissorCount = 1;
        vs.pScissors = NULL;

        VkPipelineRasterizationStateCreateInfo& rs = *stackAlloc.allocZeroed<VkPipelineRasterizationStateCreateInfo>();
        rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rs.pNext = NULL;
        rs.flags = 0;
        rs.depthClampEnable = VK_TRUE;
        rs.rasterizerDiscardEnable = VK_FALSE;
        rs.lineWidth = 1;
        REI_RasterizerStateDesc* pRasterizerState = pDesc->pRasterizerState;
        if (pRasterizerState)
        {
            REI_ASSERT(pRasterizerState->fillMode < REI_MAX_FILL_MODES);
            REI_ASSERT(pRasterizerState->cullMode < REI_MAX_CULL_MODES);
            REI_ASSERT(
                pRasterizerState->frontFace == REI_FRONT_FACE_CCW || pRasterizerState->frontFace == REI_FRONT_FACE_CW);

            rs.polygonMode = gVkFillModeTranslator[pRasterizerState->fillMode];
            rs.cullMode = gVkCullModeTranslator[pRasterizerState->cullMode];
            rs.frontFace = gVkFrontFaceTranslator[pRasterizerState->frontFace];
            rs.depthBiasEnable = (pRasterizerState->depthBiasConstantFactor != 0.0f) ? VK_TRUE : VK_FALSE;
            rs.depthBiasConstantFactor = pRasterizerState->depthBiasConstantFactor;
            rs.depthBiasSlopeFactor = pRasterizerState->depthBiasSlopeFactor;
        }
        else
        {
            rs.cullMode = VK_CULL_MODE_BACK_BIT;
        }

        VkPipelineMultisampleStateCreateInfo& ms = *stackAlloc.alloc<VkPipelineMultisampleStateCreateInfo>();
        ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.pNext = NULL;
        ms.flags = 0;
        ms.rasterizationSamples = util_to_vk_sample_count(pDesc->sampleCount);
        ms.sampleShadingEnable = VK_FALSE;
        ms.minSampleShading = 0.0f;
        ms.pSampleMask = 0;
        ms.alphaToCoverageEnable = VK_FALSE;
        ms.alphaToOneEnable = VK_FALSE;

        /// TODO: Dont create depth state if no depth stencil bound
        VkPipelineDepthStencilStateCreateInfo& ds = *stackAlloc.allocZeroed<VkPipelineDepthStencilStateCreateInfo>();
        ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        ds.pNext = NULL;
        ds.flags = 0;
        REI_DepthStateDesc* pDepthState = pDesc->pDepthState;
        if (pDepthState)
        {
            REI_ASSERT(pDepthState->depthCmpFunc < REI_MAX_COMPARE_MODES);
            REI_ASSERT(pDepthState->stencilFrontFunc < REI_MAX_COMPARE_MODES);
            REI_ASSERT(pDepthState->stencilFrontFail < REI_MAX_STENCIL_OPS);
            REI_ASSERT(pDepthState->depthFrontFail < REI_MAX_STENCIL_OPS);
            REI_ASSERT(pDepthState->stencilFrontPass < REI_MAX_STENCIL_OPS);
            REI_ASSERT(pDepthState->stencilBackFunc < REI_MAX_COMPARE_MODES);
            REI_ASSERT(pDepthState->stencilBackFail < REI_MAX_STENCIL_OPS);
            REI_ASSERT(pDepthState->depthBackFail < REI_MAX_STENCIL_OPS);
            REI_ASSERT(pDepthState->stencilBackPass < REI_MAX_STENCIL_OPS);

            ds.front.failOp = gVkStencilOpTranslator[pDepthState->stencilFrontFail];
            ds.front.passOp = gVkStencilOpTranslator[pDepthState->stencilFrontPass];
            ds.front.depthFailOp = gVkStencilOpTranslator[pDepthState->depthFrontFail];
            ds.front.compareOp = VkCompareOp(pDepthState->stencilFrontFunc);
            ds.front.compareMask = pDepthState->stencilReadMask;
            ds.front.writeMask = pDepthState->stencilWriteMask;
            ds.front.reference = 0;

            ds.back.failOp = gVkStencilOpTranslator[pDepthState->stencilBackFail];
            ds.back.passOp = gVkStencilOpTranslator[pDepthState->stencilBackPass];
            ds.back.depthFailOp = gVkStencilOpTranslator[pDepthState->depthBackFail];
            ds.back.compareOp = gVkComparisonFuncTranslator[pDepthState->stencilBackFunc];
            ds.back.compareMask = pDepthState->stencilReadMask;
            ds.back.writeMask = pDepthState->stencilWriteMask;    // devsh fixed
            ds.back.reference = 0;

            ds.depthTestEnable = pDepthState->depthTestEnable;
            ds.depthWriteEnable = pDepthState->depthWriteEnable;
            ds.depthCompareOp = gVkComparisonFuncTranslator[pDepthState->depthCmpFunc];
            ds.depthBoundsTestEnable = false;
            ds.stencilTestEnable = pDepthState->stencilTestEnable;
            ds.minDepthBounds = 0;
            ds.maxDepthBounds = 1;
        }
        else
        {
            ds.front.compareOp = VK_COMPARE_OP_ALWAYS;
            ds.front.compareMask = 0xFF;
            ds.front.writeMask = 0xFF;

            ds.back.compareOp = VK_COMPARE_OP_ALWAYS;
            ds.back.compareMask = 0xFF;
            ds.back.writeMask = 0xFF;    // devsh fixed

            ds.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        }

        VkPipelineColorBlendAttachmentState* RTBlendStates =
            stackAlloc.allocZeroed<VkPipelineColorBlendAttachmentState>(REI_MAX_RENDER_TARGET_ATTACHMENTS);
        VkPipelineColorBlendStateCreateInfo& cb = *stackAlloc.alloc<VkPipelineColorBlendStateCreateInfo>();
        cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        cb.pNext = NULL;
        cb.flags = 0;
        cb.blendConstants[0] = 0.0f;
        cb.blendConstants[1] = 0.0f;
        cb.blendConstants[2] = 0.0f;
        cb.blendConstants[3] = 0.0f;
        cb.attachmentCount = pDesc->renderTargetCount;
        cb.logicOpEnable = false;
        cb.logicOp = VK_LOGIC_OP_CLEAR;
        cb.pAttachments = RTBlendStates;
        REI_BlendStateDesc* pBlendState = pDesc->pBlendState;
        if (pBlendState)
        {
            int blendDescIndex = 0;
            for (int i = 0; i < REI_MAX_RENDER_TARGET_ATTACHMENTS; ++i)
            {
                if (pBlendState->renderTargetMask & (1 << i))
                {
                    REI_ASSERT(pBlendState->srcFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
                    REI_ASSERT(pBlendState->dstFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
                    REI_ASSERT(pBlendState->srcAlphaFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
                    REI_ASSERT(pBlendState->dstAlphaFactors[blendDescIndex] < REI_MAX_BLEND_CONSTANTS);
                    REI_ASSERT(pBlendState->blendModes[blendDescIndex] < REI_MAX_BLEND_MODES);
                    REI_ASSERT(pBlendState->blendAlphaModes[blendDescIndex] < REI_MAX_BLEND_MODES);

                    VkBool32 blendEnable =
                        (gVkBlendConstantTranslator[pBlendState->srcFactors[blendDescIndex]] != VK_BLEND_FACTOR_ONE ||
                         gVkBlendConstantTranslator[pBlendState->dstFactors[blendDescIndex]] != VK_BLEND_FACTOR_ZERO ||
                         gVkBlendConstantTranslator[pBlendState->srcAlphaFactors[blendDescIndex]] !=
                             VK_BLEND_FACTOR_ONE ||
                         gVkBlendConstantTranslator[pBlendState->dstAlphaFactors[blendDescIndex]] !=
                             VK_BLEND_FACTOR_ZERO);

                    RTBlendStates[i].blendEnable = blendEnable;
                    RTBlendStates[i].colorWriteMask = pBlendState->masks[blendDescIndex];
                    RTBlendStates[i].srcColorBlendFactor =
                        gVkBlendConstantTranslator[pBlendState->srcFactors[blendDescIndex]];
                    RTBlendStates[i].dstColorBlendFactor =
                        gVkBlendConstantTranslator[pBlendState->dstFactors[blendDescIndex]];
                    RTBlendStates[i].colorBlendOp = gVkBlendOpTranslator[pBlendState->blendModes[blendDescIndex]];
                    RTBlendStates[i].srcAlphaBlendFactor =
                        gVkBlendConstantTranslator[pBlendState->srcAlphaFactors[blendDescIndex]];
                    RTBlendStates[i].dstAlphaBlendFactor =
                        gVkBlendConstantTranslator[pBlendState->dstAlphaFactors[blendDescIndex]];
                    RTBlendStates[i].alphaBlendOp = gVkBlendOpTranslator[pBlendState->blendAlphaModes[blendDescIndex]];
                }

                if (pBlendState->independentBlend)
                    ++blendDescIndex;
            }
        }
        else
        {
            for (int i = 0; i < REI_MAX_RENDER_TARGET_ATTACHMENTS; ++i)
            {
                RTBlendStates[i].blendEnable = VK_FALSE;
                RTBlendStates[i].colorWriteMask = REI_COLOR_MASK_ALL;
                RTBlendStates[i].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                RTBlendStates[i].dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
                RTBlendStates[i].colorBlendOp = VK_BLEND_OP_ADD;
                RTBlendStates[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                RTBlendStates[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                RTBlendStates[i].alphaBlendOp = VK_BLEND_OP_ADD;
            }
        }

        uint32_t dynamicStateCount = 0;
        DECLARE_ZERO(VkDynamicState, dyn_states[7]);
        dyn_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
        dyn_states[1] = VK_DYNAMIC_STATE_SCISSOR;
        dyn_states[2] = VK_DYNAMIC_STATE_DEPTH_BIAS;
        dyn_states[3] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
        dyn_states[4] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
        dyn_states[5] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
        dynamicStateCount = 6;

#if VK_EXT_extended_dynamic_state
        if (input_attribute_count)
        {
            dyn_states[6] = VK_DYNAMIC_STATE_VERTEX_INPUT_BINDING_STRIDE_EXT;
            dynamicStateCount += 1;
        }
#endif

        VkPipelineDynamicStateCreateInfo& dy = *stackAlloc.alloc<VkPipelineDynamicStateCreateInfo>();
        dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dy.pNext = NULL;
        dy.flags = 0;
        dy.dynamicStateCount = dynamicStateCount;
        dy.pDynamicStates = dyn_states;

        VkGraphicsPipelineCreateInfo& add_info = *stackAlloc.alloc<VkGraphicsPipelineCreateInfo>();
        add_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.stageCount = numShaderModules;
        add_info.pStages = stages;
        add_info.pVertexInputState = &vi;
        add_info.pInputAssemblyState = &ia;

        if (hasFullTessStage)
            add_info.pTessellationState = &ts;
        else
            add_info.pTessellationState = NULL;    // set tessellation state to null if we have no tessellation

        add_info.pViewportState = &vs;
        add_info.pRasterizationState = &rs;
        add_info.pMultisampleState = &ms;
        add_info.pDepthStencilState = &ds;
        add_info.pColorBlendState = &cb;
        add_info.pDynamicState = &dy;
        add_info.layout = pDesc->pRootSignature->pPipelineLayout;
        add_info.renderPass = renderPass;
        add_info.subpass = 0;
        add_info.basePipelineHandle = VK_NULL_HANDLE;
        add_info.basePipelineIndex = -1;
        VkResult vk_res = vkCreateGraphicsPipelines(pRenderer->pVkDevice, psoCache, 1, &add_info, NULL, &(pPipeline->pVkPipeline));
        REI_ASSERT(VK_SUCCESS == vk_res);
        
        vkDestroyRenderPass(pRenderer->pVkDevice, renderPass, NULL);
    }

    *ppPipeline = pPipeline;
}

static void
    addComputePipelineImpl(REI_Renderer* pRenderer, const REI_ComputePipelineDesc* pDesc, REI_Pipeline** ppPipeline)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(pDesc->pShaderProgram);
    REI_ASSERT(pDesc->pRootSignature);
    REI_ASSERT(pRenderer->pVkDevice != VK_NULL_HANDLE);
    REI_ASSERT(pDesc->pShaderProgram->shaderModule != VK_NULL_HANDLE);

    VkPipelineCache psoCache = pDesc->pCache ? pDesc->pCache->pCache : VK_NULL_HANDLE;

    REI_Pipeline* pPipeline = REI_new<REI_Pipeline>(pRenderer->allocator);
    REI_ASSERT(pPipeline);

    pPipeline->type = VK_PIPELINE_BIND_POINT_COMPUTE;
    pPipeline->pRootSignature = pDesc->pRootSignature;

    // REI_Pipeline
    {
        DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stage);
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.pNext = NULL;
        stage.flags = 0;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = pDesc->pShaderProgram->shaderModule;
        stage.pName = pDesc->pShaderProgram->pEntryPoint;
        stage.pSpecializationInfo = NULL;

        DECLARE_ZERO(VkComputePipelineCreateInfo, create_info);
        create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        create_info.pNext = NULL;
        create_info.flags = 0;
        create_info.stage = stage;
        create_info.layout = pDesc->pRootSignature->pPipelineLayout;
        create_info.basePipelineHandle = 0;
        create_info.basePipelineIndex = 0;
        VkResult vk_res = vkCreateComputePipelines(pRenderer->pVkDevice, psoCache, 1, &create_info, NULL, &(pPipeline->pVkPipeline));
        REI_ASSERT(VK_SUCCESS == vk_res);
    }

    *ppPipeline = pPipeline;
}

void REI_addPipeline(REI_Renderer* pRenderer, const REI_PipelineDesc* pDesc, REI_Pipeline** ppPipeline)
{
    switch (pDesc->type)
    {
        case (REI_PIPELINE_TYPE_COMPUTE):
        {
            addComputePipelineImpl(pRenderer, &pDesc->computeDesc, ppPipeline);
            break;
        }
        case (REI_PIPELINE_TYPE_GRAPHICS):
        {
            addGraphicsPipelineImpl(pRenderer, &pDesc->graphicsDesc, ppPipeline);
            break;
        }
        default:
        {
            REI_ASSERT(false);
            ppPipeline = NULL;
            break;
        }
    }
}

void REI_removePipeline(REI_Renderer* pRenderer, REI_Pipeline* pPipeline)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pPipeline);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pPipeline->pVkPipeline);

    vkDestroyPipeline(pRenderer->pVkDevice, pPipeline->pVkPipeline, NULL);
    REI_delete(pRenderer->allocator, pPipeline);
}

/************************************************************************/
// Command buffer functions
/************************************************************************/
void REI_beginCmd(REI_Cmd* pCmd)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    VkCommandBufferBeginInfo begin_info{ /*.sType = */ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                         /*.pNext = */ NULL,
                                         /*.flags = */ VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
                                         /*.pInheritanceInfo = */ NULL };

    VkResult vk_res = vkBeginCommandBuffer(pCmd->pVkCmdBuf, &begin_info);
    REI_ASSERT(VK_SUCCESS == vk_res);

    // Reset CPU side data
    pCmd->pBoundRootSignature = NULL;
}

static inline void util_use_dirty_state(REI_Cmd* pCmd)
{
    auto& dirtyState = pCmd->mDirtyState;
    if (dirtyState.beginRenderPassDirty)
    {
        vkCmdBeginRenderPass(pCmd->pVkCmdBuf, &dirtyState.mRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        dirtyState.beginRenderPassDirty = 0;
    }
}

void REI_endCmd(REI_Cmd* pCmd)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    auto& dirtyState = pCmd->mDirtyState;
    if (dirtyState.beginRenderPassDirty)
    {
        vkCmdBeginRenderPass(pCmd->pVkCmdBuf, &dirtyState.mRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        dirtyState.beginRenderPassDirty = 0;
    }
    if (dirtyState.pVkActiveRenderPass)
    {
        vkCmdEndRenderPass(pCmd->pVkCmdBuf);
    }

    dirtyState.pVkActiveRenderPass = VK_NULL_HANDLE;
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
    dirtyState.pVkRestoreRenderPass = VK_NULL_HANDLE;
#endif
    dirtyState.beginRenderPassDirty = 0;

    VkResult vk_res = vkEndCommandBuffer(pCmd->pVkCmdBuf);
    REI_ASSERT(VK_SUCCESS == vk_res);
}

void REI_cmdBindRenderTargets(
    REI_Cmd* pCmd, uint32_t renderTargetCount, REI_Texture** ppRenderTargets, REI_Texture* pDepthStencil,
    const REI_LoadActionsDesc* pLoadActions /* = NULL*/, uint32_t* pColorArraySlices, uint32_t* pColorMipSlices,
    uint32_t depthArraySlice, uint32_t depthMipSlice)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    auto& dirtyState = pCmd->mDirtyState;
    if (dirtyState.pVkActiveRenderPass)
    {
        if (!dirtyState.beginRenderPassDirty)
            vkCmdEndRenderPass(pCmd->pVkCmdBuf);
        dirtyState.pVkActiveRenderPass = VK_NULL_HANDLE;
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
        dirtyState.pVkRestoreRenderPass = VK_NULL_HANDLE;
#endif
        dirtyState.beginRenderPassDirty = 0;
    }

    if (!renderTargetCount && !pDepthStencil)
        return;

    uint64_t renderPassHash = 0;
    uint64_t frameBufferHash = 0;

    // Generate hash for render pass and frame buffer
    // NOTE:
    // Render pass does not care about underlying VkImageView. It only cares about the format and sample count of the attachments.
    // We hash those two values to generate render pass hash
    // Frame buffer is the actual array of all the VkImageViews
    // We hash the texture id associated with the render target to generate frame buffer hash
    for (uint32_t i = 0; i < renderTargetCount; ++i)
    {
        uint32_t hashValues[] = {
            (uint32_t)ppRenderTargets[i]->desc.format,
            (uint32_t)ppRenderTargets[i]->desc.sampleCount,
            pLoadActions ? (uint32_t)pLoadActions->loadActionsColor[i] : 0,
        };
        renderPassHash = fnv_64a((uint8_t*)hashValues, sizeof(hashValues), renderPassHash);
        frameBufferHash = fnv_64a((uint8_t*)&ppRenderTargets[i]->textureId, sizeof(uint64_t), frameBufferHash);
    }
    if (pDepthStencil)
    {
        uint32_t hashValues[] = {
            (uint32_t)pDepthStencil->desc.format,
            (uint32_t)pDepthStencil->desc.sampleCount,
            pLoadActions ? (uint32_t)pLoadActions->loadActionDepth : 0,
            pLoadActions ? (uint32_t)pLoadActions->loadActionStencil : 0,
        };
        renderPassHash = fnv_64a((uint8_t*)hashValues, sizeof(hashValues), renderPassHash);
        frameBufferHash = fnv_64a((uint8_t*)&pDepthStencil->textureId, sizeof(uint64_t), frameBufferHash);
    }
    if (pColorArraySlices)
        frameBufferHash = fnv_64a((uint8_t*)pColorArraySlices, sizeof(uint32_t) * renderTargetCount, frameBufferHash);
    if (pColorMipSlices)
        frameBufferHash = fnv_64a((uint8_t*)pColorMipSlices, sizeof(uint32_t) * renderTargetCount, frameBufferHash);
    frameBufferHash = fnv_64a((uint8_t*)&depthArraySlice, sizeof(uint32_t), frameBufferHash);
    frameBufferHash = fnv_64a((uint8_t*)&depthMipSlice, sizeof(uint32_t), frameBufferHash);

    REI_SampleCount sampleCount = renderTargetCount ? (REI_SampleCount)ppRenderTargets[0]->desc.sampleCount
                                                    : (REI_SampleCount)pDepthStencil->desc.sampleCount;

    RenderPassMap&  renderPassMap = pCmd->renderPassMap;
    FrameBufferMap& frameBufferMap = pCmd->frameBufferMap;

    const RenderPassMap::iterator  pNode = renderPassMap.find(renderPassHash);
    const FrameBufferMap::iterator pFrameBufferNode = frameBufferMap.find(frameBufferHash);

    VkRenderPass renderPass = VK_NULL_HANDLE;
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
    VkRenderPass restoreRenderPass = VK_NULL_HANDLE;
#endif
    REI_FrameBuffer* pFrameBuffer = NULL;

    // If a render pass of this combination already exists just use it or create a new one
    if (pNode != renderPassMap.end())
    {
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
        renderPass = pNode->second.first;
        restoreRenderPass = pNode->second.second;
#else
        renderPass = pNode->second;
#endif
    }
    else
    {
        constexpr uint32_t requiredScratchSpace = 2u * util_add_render_pass_stack_size_in_bytes();
        static_assert(
            requiredScratchSpace <= REI_VK_CMD_SCRATCH_MEM_SIZE, "not enough scratch space to use for allocations");

        REI_StackAllocator<false> scratchAlloc = { util_get_scratch_memory(pCmd), REI_VK_CMD_SCRATCH_MEM_SIZE };

        REI_Format colorFormats[REI_MAX_RENDER_TARGET_ATTACHMENTS] = {};
        REI_Format depthStencilFormat = REI_FMT_UNDEFINED;
        for (uint32_t i = 0; i < renderTargetCount; ++i)
        {
            colorFormats[i] = (REI_Format)ppRenderTargets[i]->desc.format;
        }
        if (pDepthStencil)
        {
            depthStencilFormat = (REI_Format)pDepthStencil->desc.format;
        }

        REI_RenderPassDesc renderPassDesc = {};
        renderPassDesc.renderTargetCount = renderTargetCount;
        renderPassDesc.sampleCount = sampleCount;
        renderPassDesc.pColorFormats = colorFormats;
        renderPassDesc.depthStencilFormat = depthStencilFormat;
        renderPassDesc.pLoadActionsColor = pLoadActions ? pLoadActions->loadActionsColor : NULL;
        renderPassDesc.loadActionDepth = pLoadActions ? pLoadActions->loadActionDepth : REI_LOAD_ACTION_DONTCARE;
        renderPassDesc.loadActionStencil = pLoadActions ? pLoadActions->loadActionStencil : REI_LOAD_ACTION_DONTCARE;
        add_render_pass(scratchAlloc, pCmd->pRenderer, &renderPassDesc, &renderPass);

#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
        REI_LoadActionType restoreLoadActionsColor[REI_MAX_RENDER_TARGET_ATTACHMENTS];
        for (uint32_t i = 0; i < renderTargetCount; ++i)
        {
            restoreLoadActionsColor[i] = REI_LOAD_ACTION_LOAD;
        }
        renderPassDesc.pLoadActionsColor = restoreLoadActionsColor;
        renderPassDesc.loadActionDepth = REI_LOAD_ACTION_LOAD;
        renderPassDesc.loadActionStencil = REI_LOAD_ACTION_LOAD;

        add_render_pass(scratchAlloc, pCmd->pRenderer, &renderPassDesc, &restoreRenderPass);

        // No need of a lock here since this map is per thread
        renderPassMap.insert({ renderPassHash, { renderPass, restoreRenderPass } });
#else
        renderPassMap.insert({ renderPassHash, renderPass });
#endif
    }

    // If a frame buffer of this combination already exists just use it or create a new one
    if (pFrameBufferNode != frameBufferMap.end())
    {
        pFrameBuffer = pFrameBufferNode->second;
    }
    else
    {
        constexpr uint32_t requiredScratchSpace = util_add_framebuffer_stack_size_in_bytes();
        static_assert(
            requiredScratchSpace <= REI_VK_CMD_SCRATCH_MEM_SIZE, "not enough scratch space to use for allocations");

        REI_StackAllocator<false> scratchAlloc = { util_get_scratch_memory(pCmd), REI_VK_CMD_SCRATCH_MEM_SIZE };

        FrameBufferDesc desc{
            /*.renderPass = */ renderPass,
            /*.ppRenderTargets = */ ppRenderTargets,
            /*.pDepthStencil = */ pDepthStencil,
            /*.pColorArraySlices = */ pColorArraySlices,
            /*.pColorMipSlices = */ pColorMipSlices,
            /*.depthArraySlice = */ depthArraySlice,
            /*.depthMipSlice = */ depthMipSlice,
            /*.renderTargetCount = */ renderTargetCount,
        };
        add_framebuffer(scratchAlloc, pCmd->pRenderer, &desc, &pFrameBuffer);

        // No need of a lock here since this map is per thread
        frameBufferMap.insert({ { frameBufferHash, pFrameBuffer } });
    }

    VkRect2D render_area{ { 0, 0 }, { pFrameBuffer->width, pFrameBuffer->height } };

    uint32_t clearValueCount = renderTargetCount;
    if (pLoadActions)
    {
        for (uint32_t i = 0; i < renderTargetCount; ++i)
        {
            REI_ClearValue clearValue = pLoadActions->clearColorValues[i];
            dirtyState.clearValues[i].color = { { clearValue.rt.r, clearValue.rt.g, clearValue.rt.b,
                                                  clearValue.rt.a } };
        }
        if (pDepthStencil)
        {
            dirtyState.clearValues[renderTargetCount].depthStencil = { pLoadActions->clearDepth.ds.depth,
                                                                       pLoadActions->clearDepth.ds.stencil };
            ++clearValueCount;
        }
    }

    VkRenderPassBeginInfo begin_info{ /*.sType = */ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                      /*.pNext = */ NULL,
                                      /*.renderPass = */ renderPass,
                                      /*.framebuffer = */ pFrameBuffer->pFramebuffer,
                                      /*.renderArea = */ render_area,
                                      /*.clearValueCount = */ clearValueCount,
                                      /*.pClearValues = */ dirtyState.clearValues };

    dirtyState.pVkActiveRenderPass = renderPass;
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
    dirtyState.pVkRestoreRenderPass = restoreRenderPass;
#endif
    dirtyState.mRenderPassBeginInfo = begin_info;
    dirtyState.beginRenderPassDirty = 1;
}

void REI_cmdSetViewport(REI_Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    VkViewport viewport{ x, y + height, width, -height, minDepth, maxDepth };
    vkCmdSetViewport(pCmd->pVkCmdBuf, 0, 1, &viewport);
}

void REI_cmdSetScissor(REI_Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    VkRect2D rect{ { (int32_t)x, (int32_t)y }, { width, height } };
    vkCmdSetScissor(pCmd->pVkCmdBuf, 0, 1, &rect);
}

void REI_cmdSetStencilRef(REI_Cmd* pCmd, REI_StencilFaceMask face, uint32_t ref)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    vkCmdSetStencilReference(pCmd->pVkCmdBuf, util_to_vk_stencil_face_mask(face), ref);
}

void REI_cmdBindPipeline(REI_Cmd* pCmd, REI_Pipeline* pPipeline)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pPipeline);
    REI_ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

    vkCmdBindPipeline(pCmd->pVkCmdBuf, pPipeline->type, pPipeline->pVkPipeline);

    const REI_RootSignature* pRootSignature = pPipeline->pRootSignature;
    if (pCmd->pBoundRootSignature != pRootSignature)
    {
        pCmd->pBoundRootSignature = pRootSignature;
        REI_Renderer* pRenderer = pCmd->pRenderer;
        // Vulkan requires to bind all descriptor sets upto the highest set number even if they are empty
        // Example: If shader uses only set 2, we still have to bind empty sets for set=0 and set=1
        for (uint32_t slot = 0; slot < pRootSignature->mMaxUsedSlots; ++slot)
        {
            if (pRootSignature->vkDescriptorSetLayouts[slot] == pRenderer->pVkEmptyDescriptorSetLayout)
            {
                vkCmdBindDescriptorSets(
                    pCmd->pVkCmdBuf, pRootSignature->pipelineType, pRootSignature->pPipelineLayout, slot, 1,
                    &pRenderer->pVkEmptyDescriptorSet, 0, NULL);
            }
        }

        VkDescriptorSet staticSamplerSet = pRootSignature->vkStaticSamplerSet;
        if (staticSamplerSet) 
        {
            vkCmdBindDescriptorSets(
                pCmd->pVkCmdBuf, pRootSignature->pipelineType, pRootSignature->pPipelineLayout,
                pRootSignature->mStaticSamplerSlot, 1, &staticSamplerSet, 0, NULL);
        }
    }
}

void REI_cmdBindIndexBuffer(REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t offset)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pBuffer);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    VkIndexType vk_index_type =
        (REI_INDEX_TYPE_UINT16 == pBuffer->desc.indexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(pCmd->pVkCmdBuf, pBuffer->pVkBuffer, offset, vk_index_type);
}

void REI_cmdBindVertexBuffer(REI_Cmd* pCmd, uint32_t bufferCount, REI_Buffer** ppBuffers, uint64_t* pOffsets)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(0 != bufferCount);
    REI_ASSERT(ppBuffers);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    static constexpr VkDeviceSize s_buffer_offsets[REI_VK_MAX_VERTEX_BUFFERS] = { 0 };

    uint32_t capped_buffer_count = bufferCount > REI_VK_MAX_VERTEX_BUFFERS ? REI_VK_MAX_VERTEX_BUFFERS : bufferCount;

    // No upper bound for this, so use 64 for now
    REI_ASSERT(capped_buffer_count < REI_VK_MAX_VERTEX_BUFFERS);

    REI_StackAllocator<false> scratchAlloc = { util_get_scratch_memory(pCmd), REI_VK_CMD_SCRATCH_MEM_SIZE };

    VkBuffer*     pVertexBuffers = scratchAlloc.alloc<VkBuffer>(capped_buffer_count);
    VkDeviceSize* pVertexStrides = scratchAlloc.alloc<VkDeviceSize>(capped_buffer_count);

    for (uint32_t i = 0; i < capped_buffer_count; ++i)
    {
        pVertexBuffers[i] = ppBuffers[i]->pVkBuffer;
        pVertexStrides[i] = ppBuffers[i]->desc.vertexStride;
    }

    pCmd->pRenderer->pfn_vkCmdBindVertexBuffers2EXT(
        pCmd->pVkCmdBuf, 0, capped_buffer_count, pVertexBuffers,
        (pOffsets ? reinterpret_cast<VkDeviceSize*>(pOffsets) : s_buffer_offsets), NULL, pVertexStrides);
}

void REI_cmdDraw(REI_Cmd* pCmd, uint32_t vertex_count, uint32_t first_vertex)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);
    REI_ASSERT(vertex_count);

    util_use_dirty_state(pCmd);

    vkCmdDraw(pCmd->pVkCmdBuf, vertex_count, 1, first_vertex, 0);
}

void REI_cmdDrawInstanced(
    REI_Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    util_use_dirty_state(pCmd);

    vkCmdDraw(pCmd->pVkCmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void REI_cmdDrawIndexed(REI_Cmd* pCmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    util_use_dirty_state(pCmd);

    vkCmdDrawIndexed(pCmd->pVkCmdBuf, index_count, 1, first_index, first_vertex, 0);
}

void REI_cmdDrawIndexedInstanced(
    REI_Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstVertex,
    uint32_t firstInstance)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    util_use_dirty_state(pCmd);

    vkCmdDrawIndexed(pCmd->pVkCmdBuf, indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void REI_cmdDispatch(REI_Cmd* pCmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pCmd->pVkCmdBuf != VK_NULL_HANDLE);

    vkCmdDispatch(pCmd->pVkCmdBuf, groupCountX, groupCountY, groupCountZ);
}

void REI_cmdResourceBarrier(
    REI_Cmd* pCmd, uint32_t numBufferBarriers, REI_BufferBarrier* pBufferBarriers, uint32_t numTextureBarriers,
    REI_TextureBarrier* pTextureBarriers)
{
    REI_StackAllocator<false> scratchAlloc = { util_get_scratch_memory(pCmd), REI_VK_CMD_SCRATCH_MEM_SIZE };
   
    VkImageMemoryBarrier* imageBarriers =
        numTextureBarriers ? scratchAlloc.alloc<VkImageMemoryBarrier>(numTextureBarriers) : NULL;
    uint32_t imageBarrierCount = 0;

    VkBufferMemoryBarrier* bufferBarriers =
        numBufferBarriers ? scratchAlloc.alloc<VkBufferMemoryBarrier>(numBufferBarriers) : NULL;
    uint32_t bufferBarrierCount = 0;

    VkAccessFlags srcAccessFlags = 0;
    VkAccessFlags dstAccessFlags = 0;

    for (uint32_t i = 0; i < numBufferBarriers; ++i)
    {
        REI_BufferBarrier* pTrans = &pBufferBarriers[i];
        REI_Buffer*        pBuffer = pTrans->pBuffer;

        if (!(pTrans->endState & pTrans->startState))
        {
            VkBufferMemoryBarrier* pBufferBarrier = &bufferBarriers[bufferBarrierCount++];
            pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            pBufferBarrier->pNext = NULL;

            pBufferBarrier->buffer = pBuffer->pVkBuffer;
            pBufferBarrier->size = VK_WHOLE_SIZE;
            pBufferBarrier->offset = 0;

            pBufferBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->startState);
            pBufferBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->endState);

            pBufferBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            pBufferBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            srcAccessFlags |= pBufferBarrier->srcAccessMask;
            dstAccessFlags |= pBufferBarrier->dstAccessMask;
        }
        else if (pTrans->endState == REI_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            VkBufferMemoryBarrier* pBufferBarrier = &bufferBarriers[bufferBarrierCount++];
            pBufferBarrier->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            pBufferBarrier->pNext = NULL;

            pBufferBarrier->buffer = pBuffer->pVkBuffer;
            pBufferBarrier->size = VK_WHOLE_SIZE;
            pBufferBarrier->offset = 0;

            pBufferBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            pBufferBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

            pBufferBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            pBufferBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            srcAccessFlags |= pBufferBarrier->srcAccessMask;
            dstAccessFlags |= pBufferBarrier->dstAccessMask;
        }
    }
    for (uint32_t i = 0; i < numTextureBarriers; ++i)
    {
        REI_TextureBarrier* pTrans = &pTextureBarriers[i];
        REI_Texture*        pTexture = pTrans->pTexture;

        if (!(pTrans->endState & pTrans->startState))
        {
            VkImageMemoryBarrier* pImageBarrier = &imageBarriers[imageBarrierCount++];
            pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            pImageBarrier->pNext = NULL;

            pImageBarrier->image = pTexture->pVkImage;
            pImageBarrier->subresourceRange.aspectMask = pTexture->vkAspectMask;
            pImageBarrier->subresourceRange.baseMipLevel = 0;
            pImageBarrier->subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            pImageBarrier->subresourceRange.baseArrayLayer = 0;
            pImageBarrier->subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            pImageBarrier->srcAccessMask = util_to_vk_access_flags(pTrans->startState);
            pImageBarrier->dstAccessMask = util_to_vk_access_flags(pTrans->endState);
            pImageBarrier->oldLayout = util_to_vk_image_layout(pTrans->startState);
            pImageBarrier->newLayout = util_to_vk_image_layout(pTrans->endState);

            pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            srcAccessFlags |= pImageBarrier->srcAccessMask;
            dstAccessFlags |= pImageBarrier->dstAccessMask;
        }
        else if (pTrans->endState == REI_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            VkImageMemoryBarrier* pImageBarrier = &imageBarriers[imageBarrierCount++];
            pImageBarrier->sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            pImageBarrier->pNext = NULL;

            pImageBarrier->image = pTexture->pVkImage;
            pImageBarrier->subresourceRange.aspectMask = pTexture->vkAspectMask;
            pImageBarrier->subresourceRange.baseMipLevel = 0;
            pImageBarrier->subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
            pImageBarrier->subresourceRange.baseArrayLayer = 0;
            pImageBarrier->subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

            pImageBarrier->srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            pImageBarrier->dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
            pImageBarrier->oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            pImageBarrier->newLayout = VK_IMAGE_LAYOUT_GENERAL;

            pImageBarrier->srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            pImageBarrier->dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

            srcAccessFlags |= pImageBarrier->srcAccessMask;
            dstAccessFlags |= pImageBarrier->dstAccessMask;
        }
    }

    // TODO: fix!!!! should not rely on command pool but only on start and end state
    VkPipelineStageFlags srcStageMask =
        util_determine_pipeline_stage_flags(srcAccessFlags, pCmd->pCmdPool->cmdPoolType);
    VkPipelineStageFlags dstStageMask =
        util_determine_pipeline_stage_flags(dstAccessFlags, pCmd->pCmdPool->cmdPoolType);

    if (bufferBarrierCount || imageBarrierCount)
    {
        auto& dirtyState = pCmd->mDirtyState;
        if (dirtyState.pVkActiveRenderPass)
        {
            if (!dirtyState.beginRenderPassDirty)
            {
#if REI_VK_ALLOW_BARRIER_INSIDE_RENDERPASS
                pCmd->pRenderer->pLog(
                    REI_LOG_TYPE_WARNING,
                    "Using vkCmdPipelineBarrier inside a renderpass. Bad for performance due to closing and opening "
                    "the renderpass");

                vkCmdEndRenderPass(pCmd->pVkCmdBuf);
                dirtyState.pVkActiveRenderPass = dirtyState.pVkRestoreRenderPass;
                dirtyState.mRenderPassBeginInfo.renderPass = dirtyState.pVkActiveRenderPass;
                dirtyState.mRenderPassBeginInfo.clearValueCount = 0;
                dirtyState.mRenderPassBeginInfo.pClearValues = nullptr;
                dirtyState.beginRenderPassDirty = 1;
#else
                REI_ASSERT(false, "Using vkCmdPipelineBarrier inside a renderpass");
#endif
            }
        }

        vkCmdPipelineBarrier(
            pCmd->pVkCmdBuf, srcStageMask, dstStageMask, 0, 0, NULL, bufferBarrierCount, bufferBarriers,
            imageBarrierCount, imageBarriers);
    }
}

void REI_cmdCopyBuffer(
    REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t dstOffset, REI_Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pSrcBuffer);
    REI_ASSERT(pSrcBuffer->pVkBuffer);
    REI_ASSERT(pBuffer);
    REI_ASSERT(pBuffer->pVkBuffer);
    REI_ASSERT(srcOffset + size <= pSrcBuffer->desc.size);
    REI_ASSERT(dstOffset + size <= pBuffer->desc.size);

    VkBufferCopy region{ /*.srcOffset = */ srcOffset,
                         /*.dstOffset = */ dstOffset,
                         /*.size = */ (VkDeviceSize)size };
    vkCmdCopyBuffer(pCmd->pVkCmdBuf, pSrcBuffer->pVkBuffer, pBuffer->pVkBuffer, 1, &region);
}

void REI_cmdCopyBufferToTexture(
    REI_Cmd* pCmd, REI_Texture* pTexture, REI_Buffer* pSrcBuffer, REI_SubresourceDesc* pSubresourceDesc)
{
    VkBufferImageCopy copyData;
    copyData.bufferOffset = pSubresourceDesc->bufferOffset;
    copyData.bufferRowLength = 0;
    copyData.bufferImageHeight = 0;
    copyData.imageSubresource.aspectMask = pTexture->vkAspectMask;
    copyData.imageSubresource.mipLevel = pSubresourceDesc->mipLevel;
    copyData.imageSubresource.baseArrayLayer = pSubresourceDesc->arrayLayer;
    copyData.imageSubresource.layerCount = 1;
    copyData.imageOffset.x = pSubresourceDesc->region.x;
    copyData.imageOffset.y = pSubresourceDesc->region.y;
    copyData.imageOffset.z = pSubresourceDesc->region.z;
    copyData.imageExtent.width = pSubresourceDesc->region.w;
    copyData.imageExtent.height = pSubresourceDesc->region.h;
    copyData.imageExtent.depth = pSubresourceDesc->region.d;

    vkCmdCopyBufferToImage(
        pCmd->pVkCmdBuf, pSrcBuffer->pVkBuffer, pTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyData);
}

void REI_cmdCopyTextureToBuffer(
    REI_Cmd* pCmd, REI_Buffer* pDstBuffer, REI_Texture* pTexture, REI_SubresourceDesc* pSubresourceDesc)
{
    VkBufferImageCopy copyData;
    copyData.bufferOffset = pSubresourceDesc->bufferOffset;
    copyData.bufferRowLength = 0;
    copyData.bufferImageHeight = 0;
    copyData.imageSubresource.aspectMask = pTexture->vkAspectMask;
    copyData.imageSubresource.mipLevel = pSubresourceDesc->mipLevel;
    copyData.imageSubresource.baseArrayLayer = pSubresourceDesc->arrayLayer;
    copyData.imageSubresource.layerCount = 1;
    copyData.imageOffset.x = pSubresourceDesc->region.x;
    copyData.imageOffset.y = pSubresourceDesc->region.y;
    copyData.imageOffset.z = pSubresourceDesc->region.z;
    copyData.imageExtent.width = pSubresourceDesc->region.w;
    copyData.imageExtent.height = pSubresourceDesc->region.h;
    copyData.imageExtent.depth = pSubresourceDesc->region.d ? pSubresourceDesc->region.d : 1;

    vkCmdCopyImageToBuffer(
        pCmd->pVkCmdBuf, pTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstBuffer->pVkBuffer, 1, &copyData);
}

void REI_cmdResolveTexture(
    REI_Cmd* pCmd, REI_Texture* pDstTexture, REI_Texture* pSrcTexture, REI_ResolveDesc* pResolveDesc)
{
    VkImageResolve resolveData{};
    resolveData.srcSubresource.aspectMask = pSrcTexture->vkAspectMask;
    resolveData.srcSubresource.mipLevel = pResolveDesc->mipLevel;
    resolveData.srcSubresource.baseArrayLayer = pResolveDesc->arrayLayer;
    resolveData.srcSubresource.layerCount = 1;
    resolveData.dstSubresource.aspectMask = pDstTexture->vkAspectMask;
    resolveData.dstSubresource.mipLevel = pResolveDesc->mipLevel;
    resolveData.dstSubresource.baseArrayLayer = pResolveDesc->arrayLayer;
    resolveData.dstSubresource.layerCount = 1;
    resolveData.extent = VkExtent3D{ pSrcTexture->desc.width, pSrcTexture->desc.height, 1 };

    vkCmdResolveImage(
        pCmd->pVkCmdBuf, pSrcTexture->pVkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pDstTexture->pVkImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &resolveData);
}

/************************************************************************/
// REI_Queue Fence REI_Semaphore Functions
/************************************************************************/
void REI_acquireNextImage(
    REI_Renderer* pRenderer, REI_Swapchain* pSwapchain, REI_Semaphore* pSignalSemaphore, REI_Fence* pFence,
    uint32_t* pImageIndex)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pSwapchain->pSwapchain);
    REI_ASSERT(pSignalSemaphore || pFence);

    VkResult vk_res = {};

    if (pFence != NULL)
    {
        vk_res = vkAcquireNextImageKHR(
            pRenderer->pVkDevice, pSwapchain->pSwapchain, UINT64_MAX, VK_NULL_HANDLE, pFence->pVkFence, pImageIndex);

        // If swapchain is out of date, let caller know by setting image index to -1
        if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            *pImageIndex = -1;
            vkResetFences(pRenderer->pVkDevice, 1, &pFence->pVkFence);
            pFence->submitted = false;
            return;
        }

        pFence->submitted = true;
    }
    else
    {
        vk_res = vkAcquireNextImageKHR(
            pRenderer->pVkDevice, pSwapchain->pSwapchain, UINT64_MAX, pSignalSemaphore->pVkSemaphore, VK_NULL_HANDLE,
            pImageIndex);

        // If swapchain is out of date, let caller know by setting image index to -1
        if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            *pImageIndex = -1;
            pSignalSemaphore->signaled = false;
            return;
        }

        REI_ASSERT(VK_SUCCESS == vk_res);
        pSignalSemaphore->signaled = true;
    }
}

void REI_queueSubmit(
    REI_Queue* pQueue, uint32_t cmdCount, REI_Cmd** ppCmds, REI_Fence* pFence, uint32_t waitSemaphoreCount,
    REI_Semaphore** ppWaitSemaphores, uint32_t signalSemaphoreCount, REI_Semaphore** ppSignalSemaphores)
{
    REI_ASSERT(pQueue);
    REI_ASSERT(cmdCount > 0);
    REI_ASSERT(ppCmds);
    if (waitSemaphoreCount > 0)
    {
        REI_ASSERT(ppWaitSemaphores);
    }
    if (signalSemaphoreCount > 0)
    {
        REI_ASSERT(ppSignalSemaphores);
    }

    REI_ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);

    cmdCount = cmdCount > REI_MAX_SUBMIT_CMDS ? REI_MAX_SUBMIT_CMDS : cmdCount;
    waitSemaphoreCount =
        waitSemaphoreCount > REI_MAX_SUBMIT_WAIT_SEMAPHORES ? REI_MAX_SUBMIT_WAIT_SEMAPHORES : waitSemaphoreCount;
    signalSemaphoreCount = signalSemaphoreCount > REI_MAX_SUBMIT_SIGNAL_SEMAPHORES ? REI_MAX_SUBMIT_SIGNAL_SEMAPHORES
                                                                                   : signalSemaphoreCount;

    VkCommandBuffer* cmds = (VkCommandBuffer*)alloca(cmdCount * sizeof(VkCommandBuffer));
    for (uint32_t i = 0; i < cmdCount; ++i)
    {
        cmds[i] = ppCmds[i]->pVkCmdBuf;
    }

    VkSemaphore* wait_semaphores =
        waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
    VkPipelineStageFlags* wait_masks = (VkPipelineStageFlags*)alloca(waitSemaphoreCount * sizeof(VkPipelineStageFlags));
    uint32_t              waitCount = 0;
    for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
    {
        if (ppWaitSemaphores[i]->signaled)
        {
            wait_semaphores[waitCount] = ppWaitSemaphores[i]->pVkSemaphore;
            wait_masks[waitCount] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            ++waitCount;

            ppWaitSemaphores[i]->signaled = false;
        }
    }

    VkSemaphore* signal_semaphores =
        signalSemaphoreCount ? (VkSemaphore*)alloca(signalSemaphoreCount * sizeof(VkSemaphore)) : NULL;
    uint32_t signalCount = 0;
    for (uint32_t i = 0; i < signalSemaphoreCount; ++i)
    {
        if (!ppSignalSemaphores[i]->signaled)
        {
            signal_semaphores[signalCount] = ppSignalSemaphores[i]->pVkSemaphore;
            ppSignalSemaphores[signalCount]->signaled = true;
            ++signalCount;
        }
    }

    DECLARE_ZERO(VkSubmitInfo, submit_info);
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = waitCount;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_masks;
    submit_info.commandBufferCount = cmdCount;
    submit_info.pCommandBuffers = cmds;
    submit_info.signalSemaphoreCount = signalCount;
    submit_info.pSignalSemaphores = signal_semaphores;

    VkResult vk_res = vkQueueSubmit(pQueue->pVkQueue, 1, &submit_info, pFence ? pFence->pVkFence : VK_NULL_HANDLE);
    REI_ASSERT(VK_SUCCESS == vk_res);

    if (pFence)
        pFence->submitted = true;
}

void REI_queuePresent(
    REI_Queue* pQueue, REI_Swapchain* pSwapchain, uint32_t swapChainImageIndex, uint32_t waitSemaphoreCount,
    REI_Semaphore** ppWaitSemaphores)
{
    REI_ASSERT(pQueue);
    if (waitSemaphoreCount > 0)
    {
        REI_ASSERT(ppWaitSemaphores);
    }

    REI_ASSERT(VK_NULL_HANDLE != pQueue->pVkQueue);

    VkSemaphore* wait_semaphores =
        waitSemaphoreCount ? (VkSemaphore*)alloca(waitSemaphoreCount * sizeof(VkSemaphore)) : NULL;
    waitSemaphoreCount =
        waitSemaphoreCount > REI_MAX_PRESENT_WAIT_SEMAPHORES ? REI_MAX_PRESENT_WAIT_SEMAPHORES : waitSemaphoreCount;
    uint32_t waitCount = 0;
    for (uint32_t i = 0; i < waitSemaphoreCount; ++i)
    {
        if (ppWaitSemaphores[i]->signaled)
        {
            wait_semaphores[waitCount] = ppWaitSemaphores[i]->pVkSemaphore;
            ppWaitSemaphores[i]->signaled = false;
            ++waitCount;
        }
    }

    DECLARE_ZERO(VkPresentInfoKHR, present_info);
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.waitSemaphoreCount = waitCount;
    present_info.pWaitSemaphores = wait_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &(pSwapchain->pSwapchain);
    present_info.pImageIndices = &(swapChainImageIndex);
    present_info.pResults = NULL;

    VkResult vk_res =
        vkQueuePresentKHR(pSwapchain->pPresentQueue ? pSwapchain->pPresentQueue : pQueue->pVkQueue, &present_info);
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // TODO : Fix bug where we get this error if window is closed before able to present queue.
    }
    else
        REI_ASSERT(VK_SUCCESS == vk_res);
}

void REI_waitForFences(REI_Renderer* pRenderer, uint32_t fenceCount, REI_Fence** ppFences)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(fenceCount);
    REI_ASSERT(ppFences);

    VkFence* pFences = (VkFence*)alloca(fenceCount * sizeof(VkFence));
    uint32_t numValidFences = 0;
    for (uint32_t i = 0; i < fenceCount; ++i)
    {
        if (ppFences[i]->submitted)
            pFences[numValidFences++] = ppFences[i]->pVkFence;
    }

    if (numValidFences)
    {
        vkWaitForFences(pRenderer->pVkDevice, numValidFences, pFences, VK_TRUE, UINT64_MAX);
        vkResetFences(pRenderer->pVkDevice, numValidFences, pFences);
    }

    for (uint32_t i = 0; i < fenceCount; ++i)
        ppFences[i]->submitted = false;
}

void REI_waitQueueIdle(REI_Queue* pQueue) { vkQueueWaitIdle(pQueue->pVkQueue); }

void REI_getFenceStatus(REI_Renderer* pRenderer, REI_Fence* pFence, REI_FenceStatus* pFenceStatus)
{
    *pFenceStatus = REI_FENCE_STATUS_COMPLETE;

    if (pFence->submitted)
    {
        VkResult vkRes = vkGetFenceStatus(pRenderer->pVkDevice, pFence->pVkFence);
        if (vkRes == VK_SUCCESS)
        {
            vkResetFences(pRenderer->pVkDevice, 1, &pFence->pVkFence);
            pFence->submitted = false;
        }

        *pFenceStatus = vkRes == VK_SUCCESS ? REI_FENCE_STATUS_COMPLETE : REI_FENCE_STATUS_INCOMPLETE;
    }
    else
    {
        *pFenceStatus = REI_FENCE_STATUS_NOTSUBMITTED;
    }
}

/************************************************************************/
// Indirect draw functions
/************************************************************************/
void REI_addIndirectCommandSignature(
    REI_Renderer* pRenderer, const REI_CommandSignatureDesc* pDesc, REI_CommandSignature** ppCommandSignature)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);

    REI_CommandSignature* pCommandSignature = (REI_CommandSignature*)pRenderer->allocator.pMalloc(
        pRenderer->allocator.pUserData, sizeof(REI_CommandSignature), 0);
    pCommandSignature->indirectArgDescCounts = 0;

    for (uint32_t i = 0; i < pDesc->indirectArgCount; ++i)    // counting for all types;
    {
        switch (pDesc->pArgDescs[i].type)
        {
            case REI_INDIRECT_DRAW:
                pCommandSignature->drawType = REI_INDIRECT_DRAW;
                pCommandSignature->drawCommandStride += sizeof(REI_IndirectDrawArguments);
                break;
            case REI_INDIRECT_DRAW_INDEX:
                pCommandSignature->drawType = REI_INDIRECT_DRAW_INDEX;
                pCommandSignature->drawCommandStride += sizeof(REI_IndirectDrawIndexArguments);
                break;
            case REI_INDIRECT_DISPATCH:
                pCommandSignature->drawType = REI_INDIRECT_DISPATCH;
                pCommandSignature->drawCommandStride += sizeof(REI_IndirectDispatchArguments);
                break;
            default:
                pRenderer->pLog(
                    REI_LOG_TYPE_ERROR,
                    "Vulkan runtime only supports IndirectDraw, IndirectDrawIndex and IndirectDispatch at this point");
                break;
        }
    }

    pCommandSignature->drawCommandStride = REI_align_up(pCommandSignature->drawCommandStride, 16u);

    *ppCommandSignature = pCommandSignature;
}

void REI_removeIndirectCommandSignature(REI_Renderer* pRenderer, REI_CommandSignature* pCommandSignature)
{
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pCommandSignature);
}

void REI_cmdExecuteIndirect(
    REI_Cmd* pCmd, REI_CommandSignature* pCommandSignature, uint32_t maxCommandCount, REI_Buffer* pIndirectBuffer,
    uint64_t bufferOffset, REI_Buffer* pCounterBuffer, uint64_t counterBufferOffset)
{
    if (pCommandSignature->drawType == REI_INDIRECT_DRAW)
    {
        if (pCounterBuffer && pCmd->pRenderer->pfn_VkCmdDrawIndirectCountKHR)
            pCmd->pRenderer->pfn_VkCmdDrawIndirectCountKHR(
                pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, pCounterBuffer->pVkBuffer,
                counterBufferOffset, maxCommandCount, pCommandSignature->drawCommandStride);
        else
            vkCmdDrawIndirect(
                pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, maxCommandCount,
                pCommandSignature->drawCommandStride);
    }
    else if (pCommandSignature->drawType == REI_INDIRECT_DRAW_INDEX)
    {
        if (pCounterBuffer && pCmd->pRenderer->pfn_VkCmdDrawIndexedIndirectCountKHR)
            pCmd->pRenderer->pfn_VkCmdDrawIndexedIndirectCountKHR(
                pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, pCounterBuffer->pVkBuffer,
                counterBufferOffset, maxCommandCount, pCommandSignature->drawCommandStride);
        else
            vkCmdDrawIndexedIndirect(
                pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset, maxCommandCount,
                pCommandSignature->drawCommandStride);
    }
    else if (pCommandSignature->drawType == REI_INDIRECT_DISPATCH)
    {
        vkCmdDispatchIndirect(pCmd->pVkCmdBuf, pIndirectBuffer->pVkBuffer, bufferOffset);
    }
}
/************************************************************************/
// Query Heap Implementation
/************************************************************************/
VkQueryType util_to_vk_query_type(REI_QueryType type)
{
    switch (type)
    {
        case REI_QUERY_TYPE_TIMESTAMP: return VK_QUERY_TYPE_TIMESTAMP;
        case REI_QUERY_TYPE_PIPELINE_STATISTICS: return VK_QUERY_TYPE_PIPELINE_STATISTICS;
        case REI_QUERY_TYPE_OCCLUSION: return VK_QUERY_TYPE_OCCLUSION;
        default: REI_ASSERT(false && "Invalid query heap type"); return VK_QUERY_TYPE_MAX_ENUM;
    }
}

void REI_getQueueProperties(REI_Queue* pQueue, REI_QueueProperties* outProperties)
{
    REI_ASSERT(pQueue);
    REI_ASSERT(outProperties);

    // The framework is using ticks per sec as frequency. Vulkan is nano sec per tick.
    // Convert to ticks/sec
    outProperties->gpuTimestampFreq =
        1.0 / ((double)pQueue->pRenderer->vkDeviceProperties.properties.limits.timestampPeriod * 1e-9);

    VkQueueFamilyProperties& queueProps = pQueue->pRenderer->vkQueueFamilyProperties[pQueue->vkQueueFamilyIndex];
    outProperties->uploadGranularity = { queueProps.minImageTransferGranularity.width,
                                         queueProps.minImageTransferGranularity.height,
                                         queueProps.minImageTransferGranularity.depth };
}

void REI_addQueryPool(REI_Renderer* pRenderer, const REI_QueryPoolDesc* pDesc, REI_QueryPool** ppQueryPool)
{
    REI_QueryPool* pQueryPool = (REI_QueryPool*)REI_calloc(pRenderer->allocator, sizeof(*pQueryPool));
    pQueryPool->desc = *pDesc;

    VkQueryPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    createInfo.pNext = NULL;
    createInfo.queryCount = pDesc->queryCount;
    createInfo.queryType = util_to_vk_query_type(pDesc->type);
    createInfo.flags = 0;
    createInfo.pipelineStatistics = 0;
    vkCreateQueryPool(pRenderer->pVkDevice, &createInfo, NULL, &pQueryPool->pVkQueryPool);

    *ppQueryPool = pQueryPool;
}

void REI_removeQueryPool(REI_Renderer* pRenderer, REI_QueryPool* pQueryPool)
{
    vkDestroyQueryPool(pRenderer->pVkDevice, pQueryPool->pVkQueryPool, NULL);
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pQueryPool);
}

void REI_cmdResetQueryPool(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t startQuery, uint32_t queryCount)
{
    vkCmdResetQueryPool(pCmd->pVkCmdBuf, pQueryPool->pVkQueryPool, startQuery, queryCount);
}

void REI_cmdBeginQuery(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t index)
{
    REI_QueryType type = pQueryPool->desc.type;
    switch (type)
    {
        case REI_QUERY_TYPE_OCCLUSION:
            vkCmdBeginQuery(pCmd->pVkCmdBuf, pQueryPool->pVkQueryPool, index, VK_QUERY_CONTROL_PRECISE_BIT);
            break;

        case REI_QUERY_TYPE_BINARY_OCCLUSION:
            vkCmdBeginQuery(pCmd->pVkCmdBuf, pQueryPool->pVkQueryPool, index, 0);
            break;

        case REI_QUERY_TYPE_TIMESTAMP:
            REI_ASSERT(false && "Illegal call to BeginQuery for QUERY_TYPE_TIMESTAMP");
            break;

        default: REI_ASSERT(false, "Specified QUERY_TYPE is not implemented"); break;
    }
}

void REI_cmdEndQuery(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t index)
{
    REI_QueryType type = pQueryPool->desc.type;
    switch (type)
    {
        case REI_QUERY_TYPE_OCCLUSION:
        case REI_QUERY_TYPE_BINARY_OCCLUSION: vkCmdEndQuery(pCmd->pVkCmdBuf, pQueryPool->pVkQueryPool, index); break;

        case REI_QUERY_TYPE_TIMESTAMP:
            vkCmdWriteTimestamp(pCmd->pVkCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pQueryPool->pVkQueryPool, index);
            break;

        default: REI_ASSERT(false, "Specified QUERY_TYPE is not implemented"); break;
    }
}

void REI_cmdResolveQuery(
    REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t bufferOffset, REI_QueryPool* pQueryPool, uint32_t startQuery,
    uint32_t queryCount)
{
    vkCmdCopyQueryPoolResults(
        pCmd->pVkCmdBuf, pQueryPool->pVkQueryPool, startQuery, queryCount, pBuffer->pVkBuffer, bufferOffset,
        sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
}

/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void REI_cmdBeginDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName)
{
    if (pCmd->pRenderer->hasDebugMarkerExtension)
    {
#if USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsLabelEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pLabelName = pName;
        pCmd->pRenderer->pfn_vkCmdBeginDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
#else
        VkDebugMarkerMarkerInfoEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pMarkerName = pName;
        pCmd->pRenderer->pfn_vkCmdDebugMarkerBeginEXT(pCmd->pVkCmdBuf, &markerInfo);
#endif
    }
}

void REI_cmdEndDebugMarker(REI_Cmd* pCmd)
{
    if (pCmd->pRenderer->hasDebugMarkerExtension)
    {
#if USE_DEBUG_UTILS_EXTENSION
        pCmd->pRenderer->pfn_vkCmdEndDebugUtilsLabelEXT(pCmd->pVkCmdBuf);
#else
        pCmd->pRenderer->pfn_vkCmdDebugMarkerEndEXT(pCmd->pVkCmdBuf);
#endif
    }
}

void REI_cmdAddDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName)
{
    if (pCmd->pRenderer->hasDebugMarkerExtension)
    {
#if USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsLabelEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pLabelName = pName;
        pCmd->pRenderer->pfn_vkCmdInsertDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
#else
        VkDebugMarkerMarkerInfoEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pMarkerName = pName;
        pCmd->pRenderer->pfn_vkCmdDebugMarkerInsertEXT(pCmd->pVkCmdBuf, &markerInfo);
#endif
    }
}
/************************************************************************/
// Resource Debug Naming Interface
/************************************************************************/
void REI_setBufferName(REI_Renderer* pRenderer, REI_Buffer* pBuffer, const char* pName)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pBuffer);
    REI_ASSERT(pName);

    if (pRenderer->hasDebugMarkerExtension)
    {
#if USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
        nameInfo.objectHandle = (uint64_t)pBuffer->pVkBuffer;
        nameInfo.pObjectName = pName;
        pRenderer->pfn_vkSetDebugUtilsObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
#else
        VkDebugMarkerObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_BUFFER_EXT;
        nameInfo.object = (uint64_t)pBuffer->pVkBuffer;
        nameInfo.pObjectName = pName;
        pRenderer->pfn_vkDebugMarkerSetObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
#endif
    }
}

void REI_setTextureName(REI_Renderer* pRenderer, REI_Texture* pTexture, const char* pName)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pTexture);
    REI_ASSERT(pName);

    if (pRenderer->hasDebugMarkerExtension)
    {
#if USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
        nameInfo.objectHandle = (uint64_t)pTexture->pVkImage;
        nameInfo.pObjectName = pName;
        pRenderer->pfn_vkSetDebugUtilsObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
#else
        VkDebugMarkerObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT;
        nameInfo.object = (uint64_t)pTexture->pVkImage;
        nameInfo.pObjectName = pName;
        pRenderer->pfn_vkDebugMarkerSetObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
#endif
    }
}
