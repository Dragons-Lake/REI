/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

/************************************************************************/
// Debugging Macros
/************************************************************************/
// Uncomment this to enable render doc capture support
//#define USE_RENDER_DOC

// Debug Utils Extension is still WIP and does not work with all Validation Layers
// Disable this to use the old debug report and debug marker extensions
// Debug Utils requires the Nightly Build of RenderDoc
//#define USE_DEBUG_UTILS_EXTENSION
/************************************************************************/
/************************************************************************/

#include "RendererVk.h"

#include <algorithm>

#include "../3rdParty/spirv_reflect/spirv_reflect.h"
#define VMA_IMPLEMENTATION
#include "../3rdParty/VulkanMemoryAllocator/VulkanMemoryAllocator.h"

#include "../Interface/Common.h"

static void vk_createShaderReflection(
    const uint8_t* shaderCode, uint32_t shaderSize, REI_ShaderStage shaderStage, REI_ShaderReflection* pOutReflection);

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

const char* gVkWantedInstanceExtensions[] =
{
	VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_XCB_KHR)
	VK_KHR_XCB_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#endif
	// Debug utils not supported on all devices yet
#ifdef USE_DEBUG_UTILS_EXTENSION
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#else
	VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
	VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	/************************************************************************/
	// VR Extensions
	/************************************************************************/
	VK_KHR_DISPLAY_EXTENSION_NAME,
	VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
	/************************************************************************/
	// Property querying extensions
	/************************************************************************/
	VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	/************************************************************************/
	/************************************************************************/
};

const char* gVkWantedDeviceExtensions[] =
{
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    VK_KHR_MAINTENANCE3_EXTENSION_NAME,
	VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
	VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
	VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME,
	VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
	VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
    VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_WIN32_KHR)
	VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME,
	VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
	VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,
#endif
	// Debug marker extension in case debug utils is not supported
#ifndef USE_DEBUG_UTILS_EXTENSION
	VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#endif

#if VK_KHR_draw_indirect_count
	VK_KHR_DRAW_INDIRECT_COUNT_EXTENSION_NAME,
#endif
	// Fragment shader interlock extension to be used for ROV type functionality in Vulkan
#if VK_EXT_fragment_shader_interlock
	VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
#endif
	/************************************************************************/
	// Bindless & None Uniform access Extensions
	/************************************************************************/
#if VK_EXT_descriptor_indexing
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
#endif
	/************************************************************************/
	// Descriptor Update Template Extension for efficient descriptor set updates
	/************************************************************************/
	VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
	/************************************************************************/
};
// clang-format on

// TODO: remove
#ifdef _MSC_VER
#    pragma comment(lib, "vulkan-1.lib")
#endif

#define SAFE_FREE(p_var)        \
    if (p_var)                  \
    {                           \
        REI_free((void*)p_var); \
    }

#if defined(__cplusplus)
#    define DECLARE_ZERO(type, var) type var = {};
#else
#    define DECLARE_ZERO(type, var) type var = { 0 };
#endif

// Internal utility functions (may become external one day)
static VkSampleCountFlagBits util_to_vk_sample_count(REI_SampleCount sampleCount);
static REI_Format            util_from_vk_format(VkFormat format);
static VkFormat              util_to_vk_format(REI_Format format);

static const uint32_t gDescriptorTypeRangeSize = (VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT - VK_DESCRIPTOR_TYPE_SAMPLER + 1);

/************************************************************************/
// REI_DescriptorInfo Heap Structures
/************************************************************************/
/// CPU Visible Heap to store all the resources needing CPU read / write operations - Textures/Buffers/RTV
typedef struct REI_DescriptorPool
{
    VkDevice                      pDevice;
    VkDescriptorPool              pCurrentPool;
    VkDescriptorPoolSize*         pPoolSizes;
    std::vector<VkDescriptorPool> descriptorPools;
    uint32_t                      poolSizeCount;
    uint32_t                      numDescriptorSets;
    uint32_t                      usedDescriptorSetCount;
    VkDescriptorPoolCreateFlags   flags;
    Mutex*                        pMutex;
} REI_DescriptorPool;

/************************************************************************/
// Static REI_DescriptorInfo Heap Implementation
/************************************************************************/
static void add_descriptor_pool(
    REI_Renderer* pRenderer, uint32_t numDescriptorSets, VkDescriptorPoolCreateFlags flags,
    VkDescriptorPoolSize* pPoolSizes, uint32_t numPoolSizes, REI_DescriptorPool** ppPool)
{
    REI_DescriptorPool* pPool = (REI_DescriptorPool*)REI_calloc(1, sizeof(*pPool));
    pPool->flags = flags;
    pPool->numDescriptorSets = numDescriptorSets;
    pPool->usedDescriptorSetCount = 0;
    pPool->pDevice = pRenderer->pVkDevice;
    pPool->pMutex = REI_new(Mutex);

    pPool->poolSizeCount = numPoolSizes;
    pPool->pPoolSizes = (VkDescriptorPoolSize*)REI_calloc(numPoolSizes, sizeof(VkDescriptorPoolSize));
    for (uint32_t i = 0; i < numPoolSizes; ++i)
        pPool->pPoolSizes[i] = pPoolSizes[i];

    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = NULL;
    poolCreateInfo.poolSizeCount = numPoolSizes;
    poolCreateInfo.pPoolSizes = pPoolSizes;
    poolCreateInfo.flags = flags;
    poolCreateInfo.maxSets = numDescriptorSets;

    VkResult res = vkCreateDescriptorPool(pPool->pDevice, &poolCreateInfo, NULL, &pPool->pCurrentPool);
    REI_ASSERT(VK_SUCCESS == res);

    pPool->descriptorPools.emplace_back(pPool->pCurrentPool);

    *ppPool = pPool;
}

static void remove_descriptor_pool(REI_Renderer* pRenderer, REI_DescriptorPool* pPool)
{
    for (uint32_t i = 0; i < (uint32_t)pPool->descriptorPools.size(); ++i)
        vkDestroyDescriptorPool(pRenderer->pVkDevice, pPool->descriptorPools[i], NULL);

    pPool->descriptorPools.~vector();

    REI_delete(pPool->pMutex);
    SAFE_FREE(pPool->pPoolSizes);
    SAFE_FREE(pPool);
}

static void consume_descriptor_sets(
    REI_DescriptorPool* pPool, const VkDescriptorSetLayout* pLayouts, VkDescriptorSet** pSets,
    uint32_t numDescriptorSets)
{
    // Need a lock since vkAllocateDescriptorSets needs to be externally synchronized
    // This is fine since this will only happen during Init time
    MutexLock lock(*pPool->pMutex);

    DECLARE_ZERO(VkDescriptorSetAllocateInfo, alloc_info);
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.pNext = NULL;
    alloc_info.descriptorPool = pPool->pCurrentPool;
    alloc_info.descriptorSetCount = numDescriptorSets;
    alloc_info.pSetLayouts = pLayouts;

    VkResult vk_res = vkAllocateDescriptorSets(pPool->pDevice, &alloc_info, *pSets);
    if (VK_SUCCESS != vk_res)
    {
        VkDescriptorPool pDescriptorPool = VK_NULL_HANDLE;

        VkDescriptorPoolCreateInfo poolCreateInfo = {};
        poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.pNext = NULL;
        poolCreateInfo.poolSizeCount = pPool->poolSizeCount;
        poolCreateInfo.pPoolSizes = pPool->pPoolSizes;
        poolCreateInfo.flags = pPool->flags;
        poolCreateInfo.maxSets = pPool->numDescriptorSets;

        VkResult res = vkCreateDescriptorPool(pPool->pDevice, &poolCreateInfo, NULL, &pDescriptorPool);
        REI_ASSERT(VK_SUCCESS == res);

        pPool->descriptorPools.emplace_back(pDescriptorPool);

        pPool->pCurrentPool = pDescriptorPool;
        pPool->usedDescriptorSetCount = 0;

        alloc_info.descriptorPool = pPool->pCurrentPool;
        vk_res = vkAllocateDescriptorSets(pPool->pDevice, &alloc_info, *pSets);
    }

    REI_ASSERT(VK_SUCCESS == vk_res);

    pPool->usedDescriptorSetCount += numDescriptorSets;
}

/************************************************************************/
/************************************************************************/
VkPipelineBindPoint gPipelineBindPoint[REI_PIPELINE_TYPE_COUNT] = {
    VK_PIPELINE_BIND_POINT_MAX_ENUM,
    VK_PIPELINE_BIND_POINT_COMPUTE,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
};

static const REI_DescriptorInfo* get_descriptor(const REI_RootSignature* pRootSignature, const char* pResName)
{
    DescriptorNameToIndexMap::const_iterator it = pRootSignature->pDescriptorNameToIndexMap.find(pResName);
    if (it != pRootSignature->pDescriptorNameToIndexMap.end())
    {
        return &pRootSignature->pDescriptors[it->second];
    }
    else
    {
        REI_LOG(ERROR, "Invalid descriptor param (%s)", pResName);
        return NULL;
    }
}

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
        VkFormat           fmt = util_to_vk_format((REI_Format)i);
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

static void add_render_pass(REI_Renderer* pRenderer, const REI_RenderPassDesc* pDesc, VkRenderPass* pRenderPass)
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
        attachments =
            (VkAttachmentDescription*)REI_calloc(colorAttachmentCount + depthAttachmentCount, sizeof(*attachments));
        REI_ASSERT(attachments);

        if (colorAttachmentCount > 0)
        {
            color_attachment_refs =
                (VkAttachmentReference*)REI_calloc(colorAttachmentCount, sizeof(*color_attachment_refs));
            REI_ASSERT(color_attachment_refs);
        }
        if (depthAttachmentCount > 0)
        {
            depth_stencil_attachment_ref = (VkAttachmentReference*)REI_calloc(1, sizeof(*depth_stencil_attachment_ref));
            REI_ASSERT(depth_stencil_attachment_ref);
        }

        // Color
        for (uint32_t i = 0; i < colorAttachmentCount; ++i)
        {
            const uint32_t ssidx = i;

            // descriptions
            attachments[ssidx].flags = 0;
            attachments[ssidx].format = util_to_vk_format(pDesc->pColorFormats[i]);
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
        attachments[idx].format = util_to_vk_format(pDesc->depthStencilFormat);
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

    DECLARE_ZERO(VkSubpassDescription, subpass);
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

    DECLARE_ZERO(VkRenderPassCreateInfo, create_info);
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

    SAFE_FREE(attachments);
    SAFE_FREE(color_attachment_refs);
    SAFE_FREE(depth_stencil_attachment_ref);

    *pRenderPass = renderPass;
}

static void add_framebuffer(REI_Renderer* pRenderer, const REI_FrameBufferDesc* pDesc, REI_FrameBuffer** ppFrameBuffer)
{
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    REI_FrameBuffer* pFrameBuffer = (REI_FrameBuffer*)REI_calloc(1, sizeof(*pFrameBuffer));
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

    VkImageView* pImageViews = (VkImageView*)REI_calloc(attachment_count, sizeof(*pImageViews));
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

    DECLARE_ZERO(VkFramebufferCreateInfo, add_info);
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
    SAFE_FREE(pImageViews);
    /************************************************************************/
    /************************************************************************/

    *ppFrameBuffer = pFrameBuffer;
}

static void remove_framebuffer(REI_Renderer* pRenderer, REI_FrameBuffer* pFrameBuffer)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pFrameBuffer);

    vkDestroyFramebuffer(pRenderer->pVkDevice, pFrameBuffer->pFramebuffer, NULL);
    SAFE_FREE(pFrameBuffer);
}

static RenderPassMap& get_render_pass_map(REI_Renderer* pRenderer)
{
    decltype(pRenderer->renderPassMap)::iterator it = pRenderer->renderPassMap.find(Thread::GetCurrentThreadID());
    if (it == pRenderer->renderPassMap.end())
    {
        // Only need a lock when creating a new renderpass map for this thread
        MutexLock lock(pRenderer->renderPassMutex);
        return pRenderer->renderPassMap
            .insert(std::pair<ThreadID, RenderPassMap>(Thread::GetCurrentThreadID(), RenderPassMap()))
            .first->second;
    }
    else
    {
        return it->second;
    }
}

static FrameBufferMap& get_frame_buffer_map(REI_Renderer* pRenderer)
{
    decltype(pRenderer->frameBufferMap)::iterator it = pRenderer->frameBufferMap.find(Thread::GetCurrentThreadID());
    if (it == pRenderer->frameBufferMap.end())
    {
        // Only need a lock when creating a new framebuffer map for this thread
        MutexLock lock(pRenderer->renderPassMutex);
        return pRenderer->frameBufferMap
            .insert(std::pair<ThreadID, FrameBufferMap>(Thread::GetCurrentThreadID(), FrameBufferMap()))
            .first->second;
    }
    else
    {
        return it->second;
    }
}
/************************************************************************/
// Logging, Validation layer implementation
/************************************************************************/
// Proxy log callback
static void internal_log(REI_LogType type, const char* msg, const char* component)
{
    switch (type)
    {
        case REI_LOG_TYPE_INFO: REI_LOG(INFO, "%s ( %s )", component, msg); break;
        case REI_LOG_TYPE_WARN: REI_LOG(WARNING, "%s ( %s )", component, msg); break;
        case REI_LOG_TYPE_DEBUG: REI_LOG(DEBUG, "%s ( %s )", component, msg); break;
        case REI_LOG_TYPE_ERROR: REI_LOG(ERROR, "%s ( %s )", component, msg); break;
        default: break;
    }
}

#ifdef USE_DEBUG_UTILS_EXTENSION
// Debug callback for Vulkan layers
static VkBool32 VKAPI_PTR internal_debug_report_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    const char* pLayerPrefix = pCallbackData->pMessageIdName;
    const char* pMessage = pCallbackData->pMessage;
    int32_t     messageCode = pCallbackData->messageIdNumber;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    {
        LOGF(INFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        // Code 64 is vkCmdClearAttachments issued before any draws
        // We ignore this since we dont use load store actions
        // Instead we clear the attachments in the DirectX way
        if (messageCode == 64)
            return VK_FALSE;

        LOGF(WARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        LOGF(ERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }

    return VK_FALSE;
}
#else
static VKAPI_ATTR VkBool32 VKAPI_CALL internal_debug_report_callback(
    VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location,
    int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
    {
        REI_LOG(INFO, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
    {
        // Vulkan SDK 1.0.68 fixes the Dedicated memory binding validation error bugs
#    if VK_HEADER_VERSION < 68
        // Disable warnings for bind memory for dedicated allocations extension
        if (gDedicatedAllocationExtension && messageCode != 11 && messageCode != 12)
#    endif
            REI_LOG(WARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
    {
        REI_LOG(WARNING, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }
    else if (flags & (VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_ERROR_VALIDATION_FAILED_EXT))
    {
        REI_LOG(ERROR, "[%s] : %s (%i)", pLayerPrefix, pMessage, messageCode);
    }

    return VK_FALSE;
}
#endif
/************************************************************************/
// Create default resources to be used a null descriptors in case user does not specify some descriptors
/************************************************************************/
static void create_default_resources(REI_Renderer* pRenderer)
{
    // 1D texture
    REI_TextureDesc textureDesc = {};
    textureDesc.arraySize = 1;
    textureDesc.depth = 1;
    textureDesc.format = REI_FMT_R8G8B8A8_UNORM;
    textureDesc.height = 1;
    textureDesc.mipLevels = 1;
    textureDesc.sampleCount = REI_SAMPLE_COUNT_1;
    textureDesc.startState = REI_RESOURCE_STATE_COMMON;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.width = 1;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_1D]);
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_RW_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureUAV[REI_TEXTURE_DIM_1D]);

    // 1D texture array
    textureDesc.arraySize = 2;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_1D_ARRAY]);
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_RW_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureUAV[REI_TEXTURE_DIM_1D_ARRAY]);

    // 2D texture
    textureDesc.width = 2;
    textureDesc.height = 2;
    textureDesc.arraySize = 1;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_2D]);
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_RW_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureUAV[REI_TEXTURE_DIM_2D]);

    // 2D MS texture
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.sampleCount = REI_SAMPLE_COUNT_2;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_2DMS]);
    textureDesc.sampleCount = REI_SAMPLE_COUNT_1;

    // 2D texture array
    textureDesc.arraySize = 2;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_2D_ARRAY]);
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_RW_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureUAV[REI_TEXTURE_DIM_2D_ARRAY]);

    // 2D MS texture array
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    textureDesc.sampleCount = REI_SAMPLE_COUNT_2;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_2DMS_ARRAY]);
    textureDesc.sampleCount = REI_SAMPLE_COUNT_1;

    // 3D texture
    textureDesc.depth = 2;
    textureDesc.arraySize = 1;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_3D]);
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_RW_TEXTURE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureUAV[REI_TEXTURE_DIM_3D]);

    // Cube texture
    textureDesc.width = 2;
    textureDesc.height = 2;
    textureDesc.depth = 1;
    textureDesc.arraySize = 6;
    textureDesc.descriptors = REI_DESCRIPTOR_TYPE_TEXTURE_CUBE;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_CUBE]);
    textureDesc.arraySize = 6 * 2;
    REI_addTexture(pRenderer, &textureDesc, &pRenderer->pDefaultTextureSRV[REI_TEXTURE_DIM_CUBE_ARRAY]);

    REI_BufferDesc bufferDesc = {};
    bufferDesc.descriptors = REI_DESCRIPTOR_TYPE_BUFFER | REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bufferDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_ONLY;
    bufferDesc.startState = REI_RESOURCE_STATE_COMMON;
    bufferDesc.size = sizeof(uint32_t);
    bufferDesc.firstElement = 0;
    bufferDesc.elementCount = 1;
    bufferDesc.structStride = sizeof(uint32_t);
    bufferDesc.format = REI_FMT_R32_UINT;
    REI_addBuffer(pRenderer, &bufferDesc, &pRenderer->pDefaultBufferSRV);
    bufferDesc.descriptors = REI_DESCRIPTOR_TYPE_RW_BUFFER;
    REI_addBuffer(pRenderer, &bufferDesc, &pRenderer->pDefaultBufferUAV);

    REI_SamplerDesc samplerDesc = {};
    samplerDesc.addressU = REI_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerDesc.addressV = REI_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerDesc.addressW = REI_ADDRESS_MODE_CLAMP_TO_BORDER;
    REI_addSampler(pRenderer, &samplerDesc, &pRenderer->pDefaultSampler);

    // Create command buffer to transition resources to the correct state
    REI_Queue*   graphicsQueue = NULL;
    REI_CmdPool* cmdPool = NULL;
    REI_Cmd*     cmd = NULL;

    REI_QueueDesc queueDesc = {};
    queueDesc.type = REI_CMD_POOL_DIRECT;
    REI_addQueue(pRenderer, &queueDesc, &graphicsQueue);

    REI_addCmdPool(pRenderer, graphicsQueue, false, &cmdPool);
    REI_addCmd(pRenderer, cmdPool, false, &cmd);

    // Transition resources
    REI_beginCmd(cmd);

    std::vector<REI_BufferBarrier>  bufferBarriers;
    std::vector<REI_TextureBarrier> textureBarriers;

    for (uint32_t dim = 0; dim < REI_TEXTURE_DIM_COUNT; ++dim)
    {
        if (pRenderer->pDefaultTextureSRV[dim])
            textureBarriers.push_back(REI_TextureBarrier{
                pRenderer->pDefaultTextureSRV[dim], REI_RESOURCE_STATE_UNDEFINED, REI_RESOURCE_STATE_SHADER_RESOURCE });

        if (pRenderer->pDefaultTextureUAV[dim])
            textureBarriers.push_back(REI_TextureBarrier{ pRenderer->pDefaultTextureUAV[dim],
                                                          REI_RESOURCE_STATE_UNDEFINED,
                                                          REI_RESOURCE_STATE_UNORDERED_ACCESS });
    }

    bufferBarriers.push_back(REI_BufferBarrier{ pRenderer->pDefaultBufferSRV, REI_RESOURCE_STATE_UNDEFINED,
                                                REI_RESOURCE_STATE_SHADER_RESOURCE });
    bufferBarriers.push_back(REI_BufferBarrier{ pRenderer->pDefaultBufferUAV, REI_RESOURCE_STATE_UNDEFINED,
                                                REI_RESOURCE_STATE_UNORDERED_ACCESS });

    uint32_t bufferBarrierCount = (uint32_t)bufferBarriers.size();
    uint32_t textureBarrierCount = (uint32_t)textureBarriers.size();
    REI_cmdResourceBarrier(cmd, bufferBarrierCount, bufferBarriers.data(), textureBarrierCount, textureBarriers.data());
    REI_endCmd(cmd);

    REI_queueSubmit(graphicsQueue, 1, &cmd, NULL, 0, NULL, 0, NULL);
    REI_waitQueueIdle(graphicsQueue);

    // Delete command buffer
    REI_removeCmd(pRenderer, cmdPool, cmd);
    REI_removeCmdPool(pRenderer, cmdPool);
    REI_removeQueue(graphicsQueue);
}

static void destroy_default_resources(REI_Renderer* pRenderer)
{
    for (uint32_t dim = 0; dim < REI_TEXTURE_DIM_COUNT; ++dim)
    {
        if (pRenderer->pDefaultTextureSRV[dim])
            REI_removeTexture(pRenderer, pRenderer->pDefaultTextureSRV[dim]);

        if (pRenderer->pDefaultTextureUAV[dim])
            REI_removeTexture(pRenderer, pRenderer->pDefaultTextureUAV[dim]);
    }

    REI_removeBuffer(pRenderer, pRenderer->pDefaultBufferSRV);
    REI_removeBuffer(pRenderer, pRenderer->pDefaultBufferUAV);

    REI_removeSampler(pRenderer, pRenderer->pDefaultSampler);
}

/************************************************************************/
// Internal utility functions
/************************************************************************/
VkFilter util_to_vk_filter(REI_FilterType filter)
{
    switch (filter)
    {
        case REI_FILTER_NEAREST: return VK_FILTER_NEAREST;
        case REI_FILTER_LINEAR: return VK_FILTER_LINEAR;
        default: return VK_FILTER_LINEAR;
    }
}

VkSamplerMipmapMode util_to_vk_mip_map_mode(REI_MipmapMode mipMapMode)
{
    switch (mipMapMode)
    {
        case REI_MIPMAP_MODE_NEAREST: return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case REI_MIPMAP_MODE_LINEAR: return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        default: REI_ASSERT(false && "Invalid Mip Map Mode"); return VK_SAMPLER_MIPMAP_MODE_MAX_ENUM;
    }
}

VkSamplerAddressMode util_to_vk_address_mode(REI_AddressMode addressMode)
{
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

VkSampleCountFlagBits util_to_vk_sample_count(REI_SampleCount sampleCount)
{
    VkSampleCountFlagBits result = VK_SAMPLE_COUNT_1_BIT;
    switch (sampleCount)
    {
        case REI_SAMPLE_COUNT_1: result = VK_SAMPLE_COUNT_1_BIT; break;
        case REI_SAMPLE_COUNT_2: result = VK_SAMPLE_COUNT_2_BIT; break;
        case REI_SAMPLE_COUNT_4: result = VK_SAMPLE_COUNT_4_BIT; break;
        case REI_SAMPLE_COUNT_8: result = VK_SAMPLE_COUNT_8_BIT; break;
        case REI_SAMPLE_COUNT_16: result = VK_SAMPLE_COUNT_16_BIT; break;
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

inline VkFormat util_to_vk_format(REI_Format fmt)
{
    switch (fmt)
    {
        case REI_FMT_UNDEFINED: return VK_FORMAT_UNDEFINED;
        case REI_FMT_G4R4_UNORM: return VK_FORMAT_R4G4_UNORM_PACK8;
        case REI_FMT_A4B4G4R4_UNORM: return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
        case REI_FMT_A4R4G4B4_UNORM: return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
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

inline REI_Format util_from_vk_format(VkFormat fmt)
{
    switch (fmt)
    {
        case VK_FORMAT_UNDEFINED: return REI_FMT_UNDEFINED;
        case VK_FORMAT_R4G4_UNORM_PACK8: return REI_FMT_G4R4_UNORM;
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16: return REI_FMT_A4B4G4R4_UNORM;
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return REI_FMT_A4R4G4B4_UNORM;
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

        case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
        case VK_FORMAT_R8_USCALED:
        case VK_FORMAT_R8_SSCALED:
        case VK_FORMAT_R8G8_USCALED:
        case VK_FORMAT_R8G8_SSCALED:
        case VK_FORMAT_R8G8B8_USCALED:
        case VK_FORMAT_R8G8B8_SSCALED:
        case VK_FORMAT_B8G8R8_USCALED:
        case VK_FORMAT_B8G8R8_SSCALED:
        case VK_FORMAT_R8G8B8A8_USCALED:
        case VK_FORMAT_R8G8B8A8_SSCALED:
        case VK_FORMAT_B8G8R8A8_USCALED:
        case VK_FORMAT_B8G8R8A8_SSCALED:
        case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
        case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
        case VK_FORMAT_A8B8G8R8_UINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SINT_PACK32:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
        case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
        case VK_FORMAT_A2R10G10B10_SINT_PACK32:
        case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
        case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
        case VK_FORMAT_A2B10G10R10_SINT_PACK32:
        case VK_FORMAT_R16_USCALED:
        case VK_FORMAT_R16_SSCALED:
        case VK_FORMAT_R16G16_USCALED:
        case VK_FORMAT_R16G16_SSCALED:
        case VK_FORMAT_R16G16B16_USCALED:
        case VK_FORMAT_R16G16B16_SSCALED:
        case VK_FORMAT_R16G16B16A16_USCALED:
        case VK_FORMAT_R16G16B16A16_SSCALED:
        case VK_FORMAT_G8B8G8R8_422_UNORM:
        case VK_FORMAT_B8G8R8G8_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM:
        case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM:
        case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM:
        case VK_FORMAT_R10X6_UNORM_PACK16:
        case VK_FORMAT_R10X6G10X6_UNORM_2PACK16:
        case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16:
        case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16:
        case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_R12X4_UNORM_PACK16:
        case VK_FORMAT_R12X4G12X4_UNORM_2PACK16:
        case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16:
        case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16:
        case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16:
        case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16:
        case VK_FORMAT_G16B16G16R16_422_UNORM:
        case VK_FORMAT_B16G16R16G16_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM:
        case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM:
        case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM:
        case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG:
        case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG: return REI_FMT_UNDEFINED;
    }
    return REI_FMT_UNDEFINED;
}

static inline bool util_has_depth_aspect(REI_Format const fmt)
{
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

static inline bool util_has_stencil_aspect(REI_Format const fmt)
{
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

static VkQueueFlags util_to_vk_queue_flags(REI_CmdPoolType cmdPoolType)
{
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
        case REI_DESCRIPTOR_TYPE_RW_TEXTURE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case REI_DESCRIPTOR_TYPE_BUFFER:
        case REI_DESCRIPTOR_TYPE_RW_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
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

/************************************************************************/
// Internal init functions
/************************************************************************/
static void create_instance(const REI_RendererDescVk* pDescVk, REI_Renderer* pRenderer)
{
    uint32_t layerCount = 0;
    uint32_t extCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, NULL);
    std::vector<VkLayerProperties> layers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, layers.data());
    for (uint32_t i = 0; i < layerCount; ++i)
    {
        internal_log(REI_LOG_TYPE_INFO, layers[i].layerName, "vkinstance-layer");
    }
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, NULL);
    std::vector<VkExtensionProperties> exts(extCount);
    vkEnumerateInstanceExtensionProperties(NULL, &extCount, exts.data());
    for (uint32_t i = 0; i < extCount; ++i)
    {
        internal_log(REI_LOG_TYPE_INFO, exts[i].extensionName, "vkinstance-ext");
    }

    DECLARE_ZERO(VkApplicationInfo, app_info);
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = NULL;
    app_info.pApplicationName = pDescVk->desc.app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "TheForge";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_1;

    VkResult vk_res = VK_RESULT_MAX_ENUM;

    std::vector<const char*> instanceLayers;

#if defined(_DEBUG)
    // this turns on all validation layers
    // NOTE: disable VK_LAYER_KHRONOS_validation for now as it crashes on win x86 with Vulkan SDK 1.2.135.0
    instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
    instanceLayers.push_back("VK_LAYER_LUNARG_standard_validation");
#endif

    // this turns on render doc layer for gpu capture
#ifdef USE_RENDER_DOC
    pRenderer->mInstanceLayers.push_back("VK_LAYER_RENDERDOC_Capture");
#endif

    // Add user specified instance layers for instance creation
    instanceLayers.assign(pDescVk->ppInstanceLayers, pDescVk->ppInstanceLayers + pDescVk->instanceLayerCount);

    // Instance
    {
        // check to see if the layers are present
        for (auto it = instanceLayers.begin(); it != instanceLayers.end();)
        {
            bool layerFound = false;
            for (uint32_t j = 0; j < layerCount; ++j)
            {
                if (strcmp(*it, layers[j].layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (layerFound == false)
            {
                internal_log(REI_LOG_TYPE_WARN, *it, "vkinstance-layer-missing");
                // delete layer
                it = instanceLayers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        std::vector<const char*> instanceExtensions;

        const uint32_t initialCount = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
        const uint32_t userRequestedCount = (uint32_t)pDescVk->instanceExtensionCount;
        std::vector<const char*> wantedInstanceExtensions(initialCount + userRequestedCount);
        for (uint32_t i = 0; i < initialCount; ++i)
        {
            wantedInstanceExtensions[i] = gVkWantedInstanceExtensions[i];
        }
        for (uint32_t i = 0; i < userRequestedCount; ++i)
        {
            wantedInstanceExtensions[initialCount + i] = pDescVk->ppInstanceExtensions[i];
        }
        const uint32_t wanted_extension_count = (uint32_t)wantedInstanceExtensions.size();
        // Layer extensions
        for (uint32_t i = 0; i < instanceLayers.size(); ++i)
        {
            const char* layer_name = instanceLayers[i];
            uint32_t    count = 0;
            vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
            VkExtensionProperties* properties = (VkExtensionProperties*)REI_calloc(count, sizeof(*properties));
            REI_ASSERT(properties != NULL);
            vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
            for (uint32_t j = 0; j < count; ++j)
            {
                for (uint32_t k = 0; k < wanted_extension_count; ++k)
                {
                    if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)
                    {
#ifdef USE_DEBUG_UTILS_EXTENSION
                        if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                            gDebugUtilsExtension = true;
#endif
                        instanceExtensions.push_back(wantedInstanceExtensions[k]);
                        // clear wanted extenstion so we dont load it more then once
                        wantedInstanceExtensions[k] = "";
                        break;
                    }
                }
            }
            SAFE_FREE((void*)properties);
        }
        // Standalone extensions
        {
            const char* layer_name = NULL;
            uint32_t    count = 0;
            vkEnumerateInstanceExtensionProperties(layer_name, &count, NULL);
            if (count > 0)
            {
                VkExtensionProperties* properties = (VkExtensionProperties*)REI_calloc(count, sizeof(*properties));
                REI_ASSERT(properties != NULL);
                vkEnumerateInstanceExtensionProperties(layer_name, &count, properties);
                for (uint32_t j = 0; j < count; ++j)
                {
                    for (uint32_t k = 0; k < wanted_extension_count; ++k)
                    {
                        if (strcmp(wantedInstanceExtensions[k], properties[j].extensionName) == 0)
                        {
                            instanceExtensions.push_back(wantedInstanceExtensions[k]);
                            // clear wanted extenstion so we dont load it more then once
                            //gVkWantedInstanceExtensions[k] = "";
#ifdef USE_DEBUG_UTILS_EXTENSION
                            if (strcmp(wantedInstanceExtensions[k], VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
                                gDebugUtilsExtension = true;
#endif
                            break;
                        }
                    }
                }
                SAFE_FREE((void*)properties);
            }
        }

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

        // Add more extensions here
        DECLARE_ZERO(VkInstanceCreateInfo, create_info);
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#if VK_HEADER_VERSION >= 108
        create_info.pNext = &validationFeaturesExt;
#endif
        create_info.flags = 0;
        create_info.pApplicationInfo = &app_info;
        create_info.enabledLayerCount = (uint32_t)instanceLayers.size();
        create_info.ppEnabledLayerNames = instanceLayers.data();
        create_info.enabledExtensionCount = (uint32_t)instanceExtensions.size();
        create_info.ppEnabledExtensionNames = instanceExtensions.data();
        vk_res = vkCreateInstance(&create_info, NULL, &(pRenderer->pVkInstance));
        REI_ASSERT(VK_SUCCESS == vk_res);
    }

    pRenderer->pfn_vkGetPhysicalDeviceFeatures2KHR = (PFN_vkGetPhysicalDeviceFeatures2KHR)vkGetInstanceProcAddr(
        pRenderer->pVkInstance, "vkGetPhysicalDeviceFeatures2KHR");

    // Debug
    {
#ifdef USE_DEBUG_UTILS_EXTENSION
        if (gDebugUtilsExtension)
        {
            VkDebugUtilsMessengerCreateInfoEXT create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            create_info.pfnUserCallback = internal_debug_report_callback;
            create_info.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            create_info.flags = 0;
            create_info.pUserData = NULL;
            VkResult res = vkCreateDebugUtilsMessengerEXT(
                pRenderer->pVkInstance, &create_info, NULL, &(pRenderer->pVkDebugUtilsMessenger));
            if (VK_SUCCESS != res)
            {
                internal_log(
                    LOG_TYPE_ERROR, "vkCreateDebugUtilsMessengerEXT failed - disabling Vulkan debug callbacks",
                    "internal_vk_init_instance");
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
        VkResult res = pfn_vkCreateDebugReportCallbackEXT(
            pRenderer->pVkInstance, &create_info, NULL, &(pRenderer->pVkDebugReport));
        if (VK_SUCCESS != res)
        {
            internal_log(
                REI_LOG_TYPE_ERROR, "vkCreateDebugReportCallbackEXT failed - disabling Vulkan debug callbacks",
                "internal_vk_init_instance");
        }
#endif
    }
}

static void remove_instance(REI_Renderer* pRenderer)
{
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);

#ifdef USE_DEBUG_UTILS_EXTENSION
    if (pRenderer->pVkDebugUtilsMessenger)
    {
        vkDestroyDebugUtilsMessengerEXT(pRenderer->pVkInstance, pRenderer->pVkDebugUtilsMessenger, NULL);
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

    pRenderer->numOfDevices = REI_min<uint32_t>(REI_MAX_GPUS, pRenderer->numOfDevices);

    VkPhysicalDevice                 pVkDevices[REI_MAX_GPUS];
    VkPhysicalDeviceProperties2      vkDeviceProperties[REI_MAX_GPUS];
    VkPhysicalDeviceMemoryProperties vkDeviceMemoryProperties[REI_MAX_GPUS];
    uint32_t                         vkQueueFamilyCount[REI_MAX_GPUS];
    VkQueueFamilyProperties          vkQueueFamilyProperties[REI_MAX_GPUS][REI_VK_MAX_QUEUE_FAMILY_COUNT];

    memset(pVkDevices, 0, sizeof(pVkDevices));
    memset(vkDeviceProperties, 0, sizeof(vkDeviceProperties));
    memset(vkDeviceMemoryProperties, 0, sizeof(vkDeviceMemoryProperties));
    memset(vkQueueFamilyCount, 0, sizeof(vkQueueFamilyCount));
    memset(vkQueueFamilyProperties, 0, sizeof(vkQueueFamilyProperties));

    vk_res = vkEnumeratePhysicalDevices(pRenderer->pVkInstance, &(pRenderer->numOfDevices), pVkDevices);
    REI_ASSERT(VK_SUCCESS == vk_res);

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
        vkGetPhysicalDeviceQueueFamilyProperties(pVkDevices[i], &vkQueueFamilyCount[i], vkQueueFamilyProperties[i]);

        REI_LOG(
            INFO, "GPU found: %s (VendorID:0x%04X, DeviceID: 0x%04X)", vkDeviceProperties[i].properties.deviceName,
            vkDeviceProperties[i].properties.vendorID, vkDeviceProperties[i].properties.deviceID);
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
            VkQueueFamilyProperties* properties = vkQueueFamilyProperties[i];

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
    REI_ASSERT(sizeof(pRenderer->vkQueueFamilyProperties) == sizeof(vkQueueFamilyProperties[deviceIndex]));
    memcpy(
        pRenderer->vkQueueFamilyProperties, vkQueueFamilyProperties[deviceIndex],
        sizeof(vkQueueFamilyProperties[deviceIndex]));
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkPhysicalDevice);

    REI_LOG(
        INFO, "GPU selected: %s (VendorID:0x%04X, DeviceID: 0x%04X)",
        pRenderer->vkDeviceProperties.properties.deviceName, pRenderer->vkDeviceProperties.properties.vendorID,
        pRenderer->vkDeviceProperties.properties.deviceID);

    uint32_t                           count = 0;
    std::vector<VkLayerProperties>     layers;
    std::vector<VkExtensionProperties> exts;
    vkEnumerateDeviceLayerProperties(pRenderer->pVkPhysicalDevice, &count, NULL);
    layers.resize(count);
    vkEnumerateDeviceLayerProperties(pRenderer->pVkPhysicalDevice, &count, layers.data());
    for (uint32_t i = 0; i < count; ++i)
    {
        internal_log(REI_LOG_TYPE_INFO, layers[i].layerName, "vkdevice-layer");
        if (strcmp(layers[i].layerName, "VK_LAYER_RENDERDOC_Capture") == 0)
            pRenderer->hasRenderDocLayerEnabled = true;
    }
    vkEnumerateDeviceExtensionProperties(pRenderer->pVkPhysicalDevice, NULL, &count, NULL);
    exts.resize(count);
    vkEnumerateDeviceExtensionProperties(pRenderer->pVkPhysicalDevice, NULL, &count, exts.data());
    for (uint32_t i = 0; i < count; ++i)
    {
        internal_log(REI_LOG_TYPE_INFO, exts[i].extensionName, "vkdevice-ext");
    }

    std::vector<const char*> vkDeviceExtensions;

    bool dedicatedAllocationExtension = false;
    bool memoryReq2Extension = false;
    bool externalMemoryExtension = false;
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    bool externalMemoryWin32Extension = false;
#endif
    // Standalone extensions
    {
        const char*    layer_name = NULL;
        uint32_t       initialCount = sizeof(gVkWantedDeviceExtensions) / sizeof(gVkWantedDeviceExtensions[0]);
        const uint32_t userRequestedCount = (uint32_t)pDescVk->deviceExtensionCount;
        std::vector<const char*> wantedDeviceExtensions(initialCount + userRequestedCount);
        for (uint32_t i = 0; i < initialCount; ++i)
        {
            wantedDeviceExtensions[i] = gVkWantedDeviceExtensions[i];
        }
        for (uint32_t i = 0; i < userRequestedCount; ++i)
        {
            wantedDeviceExtensions[initialCount + i] = pDescVk->ppDeviceExtensions[i];
        }
        const uint32_t wanted_extension_count = (uint32_t)wantedDeviceExtensions.size();
        uint32_t       count = 0;
        vkEnumerateDeviceExtensionProperties(pRenderer->pVkPhysicalDevice, layer_name, &count, NULL);
        if (count > 0)
        {
            VkExtensionProperties* properties = (VkExtensionProperties*)REI_calloc(count, sizeof(*properties));
            REI_ASSERT(properties != NULL);
            vkEnumerateDeviceExtensionProperties(pRenderer->pVkPhysicalDevice, layer_name, &count, properties);
            for (uint32_t j = 0; j < count; ++j)
            {
                for (uint32_t k = 0; k < wanted_extension_count; ++k)
                {
                    if (strcmp(wantedDeviceExtensions[k], properties[j].extensionName) == 0)
                    {
                        vkDeviceExtensions.push_back(wantedDeviceExtensions[k]);

#ifndef USE_DEBUG_UTILS_EXTENSION
                        if (strcmp(wantedDeviceExtensions[k], VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
                            pRenderer->hasDebugMarkerExtension = true;
#endif
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) == 0)
                            dedicatedAllocationExtension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) == 0)
                            memoryReq2Extension = true;
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME) == 0)
                            externalMemoryExtension = true;
#ifdef VK_USE_PLATFORM_WIN32_KHR
                        if (strcmp(wantedDeviceExtensions[k], VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME) == 0)
                            externalMemoryWin32Extension = true;
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
            SAFE_FREE((void*)properties);
        }
    }

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
    pExtensionList = &descriptorIndexingFeatures;
#endif    // VK_EXT_fragment_shader_interlock

    VkPhysicalDeviceFeatures2KHR gpuFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR, pExtensionList };
    pRenderer->pfn_vkGetPhysicalDeviceFeatures2KHR(pRenderer->pVkPhysicalDevice, &gpuFeatures2);

    // need a queue_priorite for each queue in the queue family we create
    uint32_t                 queueFamiliesCount = pRenderer->vkQueueFamilyCount;
    VkQueueFamilyProperties* queueFamiliesProperties = pRenderer->vkQueueFamilyProperties;
    std::vector<float>       queue_priorities[REI_VK_MAX_QUEUE_FAMILY_COUNT];
    uint32_t                 queue_create_infos_count = 0;
    DECLARE_ZERO(VkDeviceQueueCreateInfo, queue_create_infos[REI_VK_MAX_QUEUE_FAMILY_COUNT]);

    //create all queue families with maximum amount of queues
    for (uint32_t i = 0; i < queueFamiliesCount; i++)
    {
        uint32_t queueCount = queueFamiliesProperties[i].queueCount;
        if (queueCount > 0)
        {
            queue_create_infos[queue_create_infos_count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_infos[queue_create_infos_count].pNext = NULL;
            queue_create_infos[queue_create_infos_count].flags = 0;
            queue_create_infos[queue_create_infos_count].queueFamilyIndex = i;
            queue_create_infos[queue_create_infos_count].queueCount = queueCount;
            queue_priorities[i].resize(queueCount);
            memset(queue_priorities[i].data(), 1, queue_priorities[i].size() * sizeof(float));
            queue_create_infos[queue_create_infos_count].pQueuePriorities = queue_priorities[i].data();
            queue_create_infos_count++;
        }
    }

    DECLARE_ZERO(VkDeviceCreateInfo, create_info);
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = &gpuFeatures2;
    create_info.flags = 0;
    create_info.queueCreateInfoCount = queue_create_infos_count;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = NULL;
    create_info.enabledExtensionCount = (uint32_t)vkDeviceExtensions.size();
    create_info.ppEnabledExtensionNames = vkDeviceExtensions.data();
    create_info.pEnabledFeatures = NULL;
    vk_res = vkCreateDevice(pRenderer->pVkPhysicalDevice, &create_info, NULL, &(pRenderer->pVkDevice));
    REI_ASSERT(pRenderer->pVkDevice && VK_SUCCESS == vk_res);

    pRenderer->hasDedicatedAllocationExtension = dedicatedAllocationExtension && memoryReq2Extension;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    pRenderer->hasExternalMemoryExtension = externalMemoryExtension && externalMemoryWin32Extension;
#endif

    if (pRenderer->hasDedicatedAllocationExtension)
    {
        REI_LOG(INFO, "Successfully loaded Dedicated Allocation extension");
    }

    if (pRenderer->hasExternalMemoryExtension)
    {
        REI_LOG(INFO, "Successfully loaded External Memory extension");
    }

#ifndef USE_DEBUG_UTILS_EXTENSION
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
        REI_LOG(INFO, "Successfully loaded Draw Indirect extension");
    }

    if (pRenderer->hasDescriptorIndexingExtension)
    {
        REI_LOG(INFO, "Successfully loaded Descriptor Indexing extension");
    }

#ifdef USE_DEBUG_UTILS_EXTENSION
    pRenderer->hasDebugMarkerExtension = vkCmdBeginDebugUtilsLabelEXT && vkCmdEndDebugUtilsLabelEXT &&
                                         vkCmdInsertDebugUtilsLabelEXT && vkSetDebugUtilsObjectNameEXT;
#endif

    pRenderer->pfn_vkCreateDescriptorUpdateTemplateKHR = (PFN_vkCreateDescriptorUpdateTemplateKHR)vkGetDeviceProcAddr(
        pRenderer->pVkDevice, "vkCreateDescriptorUpdateTemplateKHR");
    pRenderer->pfn_vkDestroyDescriptorUpdateTemplateKHR = (PFN_vkDestroyDescriptorUpdateTemplateKHR)vkGetDeviceProcAddr(
        pRenderer->pVkDevice, "vkDestroyDescriptorUpdateTemplateKHR");
    pRenderer->pfn_vkUpdateDescriptorSetWithTemplateKHR = (PFN_vkUpdateDescriptorSetWithTemplateKHR)vkGetDeviceProcAddr(
        pRenderer->pVkDevice, "vkUpdateDescriptorSetWithTemplateKHR");
}

static void REI_removeDevice(REI_Renderer* pRenderer) { vkDestroyDevice(pRenderer->pVkDevice, NULL); }

static VkDeviceMemory get_vk_device_memory(REI_Renderer* pRenderer, REI_Buffer* pBuffer)
{
    VmaAllocationInfo allocInfo = {};
    vmaGetAllocationInfo(pRenderer->pVmaAllocator, pBuffer->pVkAllocation, &allocInfo);
    return allocInfo.deviceMemory;
}

static uint64_t get_vk_device_memory_offset(REI_Renderer* pRenderer, REI_Buffer* pBuffer)
{
    VmaAllocationInfo allocInfo = {};
    vmaGetAllocationInfo(pRenderer->pVmaAllocator, pBuffer->pVkAllocation, &allocInfo);
    return (uint64_t)allocInfo.offset;
}

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
    REI_Renderer* pRenderer = REI_new(REI_Renderer);
    REI_ASSERT(pRenderer);

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

    create_default_resources(pRenderer);

    VkDescriptorPoolSize descriptorPoolSizes[gDescriptorTypeRangeSize] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1024 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8192 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8192 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 },
    };
    add_descriptor_pool(
        pRenderer, 8192, (VkDescriptorPoolCreateFlags)0, descriptorPoolSizes, gDescriptorTypeRangeSize,
        &pRenderer->pDescriptorPool);

    // REI_Renderer is good! Assign it to result!
    *(ppRenderer) = pRenderer;
}

void REI_removeRenderer(REI_Renderer* pRenderer)
{
    REI_ASSERT(pRenderer);

    remove_descriptor_pool(pRenderer, pRenderer->pDescriptorPool);
    destroy_default_resources(pRenderer);

    // Remove the renderpasses
    for (decltype(pRenderer->renderPassMap)::value_type& t: pRenderer->renderPassMap)
        for (RenderPassMapNode& it: t.second)
            vkDestroyRenderPass(pRenderer->pVkDevice, it.second, NULL);

    for (decltype(pRenderer->frameBufferMap)::value_type& t: pRenderer->frameBufferMap)
        for (FrameBufferMapNode& it: t.second)
            remove_framebuffer(pRenderer, it.second);

    pRenderer->renderPassMap.~unordered_map();
    pRenderer->frameBufferMap.~unordered_map();

    pRenderer->renderPassMutex.~Mutex();

    // Destroy the Vulkan bits
    vmaDestroyAllocator(pRenderer->pVmaAllocator);

    REI_removeDevice(pRenderer);
    remove_instance(pRenderer);

    // Free all the renderer components!
    SAFE_FREE(pRenderer);
}
/************************************************************************/
// Resource Creation Functions
/************************************************************************/
void REI_addFence(REI_Renderer* pRenderer, REI_Fence** ppFence)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    REI_Fence* pFence = (REI_Fence*)REI_calloc(1, sizeof(*pFence));
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

    SAFE_FREE(pFence);
}

void REI_addSemaphore(REI_Renderer* pRenderer, REI_Semaphore** ppSemaphore)
{
    REI_ASSERT(pRenderer);

    REI_Semaphore* pSemaphore = (REI_Semaphore*)REI_calloc(1, sizeof(*pSemaphore));
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

    SAFE_FREE(pSemaphore);
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

        REI_LOG(WARNING, "Could not find queue of type %u. Using default queue", (uint32_t)pDesc->type);
    }

    if (found)
    {
        VkQueueFamilyProperties& queueProps = pRenderer->vkQueueFamilyProperties[queueFamilyIndex];
        REI_Queue*               pQueueToCreate = (REI_Queue*)REI_calloc(1, sizeof(*pQueueToCreate));
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
        REI_LOG(ERROR, "Cannot create queue of type (%u)", pDesc->type);
    }
}

void REI_removeQueue(REI_Queue* pQueue)
{
    REI_ASSERT(pQueue != NULL);
    --pQueue->pRenderer->vkUsedQueueCount[pQueue->vkQueueFamilyIndex];
    SAFE_FREE(pQueue);
}

void REI_addCmdPool(REI_Renderer* pRenderer, REI_Queue* pQueue, bool transient, REI_CmdPool** ppCmdPool)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    REI_CmdPool* pCmdPool = (REI_CmdPool*)REI_calloc(1, sizeof(*pCmdPool));
    REI_ASSERT(pCmdPool);

    pCmdPool->cmdPoolType = pQueue->queueDesc.type;

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

void REI_removeCmdPool(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pCmdPool);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

    vkDestroyCommandPool(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, NULL);

    SAFE_FREE(pCmdPool);
}

void REI_addCmd(REI_Renderer* pRenderer, REI_CmdPool* pCmdPool, bool secondary, REI_Cmd** ppCmd)
{
    REI_ASSERT(pCmdPool);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pCmdPool->pVkCmdPool);

    REI_Cmd* pCmd = (REI_Cmd*)REI_calloc(1, sizeof(*pCmd));
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

    vkFreeCommandBuffers(pRenderer->pVkDevice, pCmdPool->pVkCmdPool, 1, &(pCmd->pVkCmdBuf));

    SAFE_FREE(pCmd);
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

    // TODO: properly fail swapchain creation
    REI_Swapchain* pSwapchain = (REI_Swapchain*)REI_calloc(1, sizeof(*pSwapchain));
    pSwapchain->desc = *pDesc;

    /************************************************************************/
    // Create surface
    /************************************************************************/
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkInstance);
    VkResult vk_res;
    // Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    DECLARE_ZERO(VkWin32SurfaceCreateInfoKHR, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.hinstance = ::GetModuleHandle(NULL);
    add_info.hwnd = (HWND)pDesc->windowHandle.window;
    vk_res = vkCreateWin32SurfaceKHR(pRenderer->pVkInstance, &add_info, NULL, &pSwapchain->pVkSurface);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    DECLARE_ZERO(VkXlibSurfaceCreateInfoKHR, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.dpy = pDesc->mWindowHandle.display;      //TODO
    add_info.window = pDesc->mWindowHandle.window;    //TODO

    vk_res = vkCreateXlibSurfaceKHR(pRenderer->pVkInstance, &add_info, NULL, &pSwapchain->pVkSurface);
#elif defined(VK_USE_PLATFORM_XCB_KHR)
    DECLARE_ZERO(VkXcbSurfaceCreateInfoKHR, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.connection = pDesc->mWindowHandle.connection;    //TODO
    add_info.window = pDesc->mWindowHandle.window;            //TODO

    vk_res = vkCreateXcbSurfaceKHR(pRenderer->pVkInstance, &add_info, NULL, &pSwapchain->pVkSurface);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    // Add IOS support here
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    // Add MacOS support here
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    DECLARE_ZERO(VkAndroidSurfaceCreateInfoKHR, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.window = (ANativeWindow*)pDesc->windowHandle.window;
    vk_res = vkCreateAndroidSurfaceKHR(pRenderer->pVkInstance, &add_info, NULL, &pSwapchain->pVkSurface);
#else
#    error PLATFORM NOT SUPPORTED
#endif
    REI_ASSERT(VK_SUCCESS == vk_res);
    /************************************************************************/
    // Create swap chain
    /************************************************************************/
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkPhysicalDevice);

    // Most GPUs will not go beyond VK_SAMPLE_COUNT_8_BIT
    REI_ASSERT(
        0 !=
        (pRenderer->vkDeviceProperties.properties.limits.framebufferColorSampleCounts & pSwapchain->desc.sampleCount));

    // Image count
    if (0 == pSwapchain->desc.imageCount)
    {
        pSwapchain->desc.imageCount = 2;
    }

    DECLARE_ZERO(VkSurfaceCapabilitiesKHR, caps);
    vk_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->pVkPhysicalDevice, pSwapchain->pVkSurface, &caps);
    REI_ASSERT(VK_SUCCESS == vk_res);

    if ((caps.maxImageCount > 0) && (pSwapchain->desc.imageCount > caps.maxImageCount))
    {
        pSwapchain->desc.imageCount = caps.maxImageCount;
    }

    // Surface format
    DECLARE_ZERO(VkSurfaceFormatKHR, surface_format);
    surface_format.format = VK_FORMAT_UNDEFINED;
    uint32_t            surfaceFormatCount = 0;
    VkSurfaceFormatKHR* formats = NULL;

    // Get surface formats count
    vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(
        pRenderer->pVkPhysicalDevice, pSwapchain->pVkSurface, &surfaceFormatCount, NULL);
    REI_ASSERT(VK_SUCCESS == vk_res);

    // Allocate and get surface formats
    formats = (VkSurfaceFormatKHR*)REI_calloc(surfaceFormatCount, sizeof(*formats));
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
        VkFormat requested_format = util_to_vk_format(pSwapchain->desc.colorFormat);
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

    // Free formats
    SAFE_FREE(formats);

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")
    VkPresentModeKHR  present_mode = VK_PRESENT_MODE_FIFO_KHR;
    uint32_t          swapChainImageCount = 0;
    VkPresentModeKHR* modes = NULL;
    // Get present mode count
    vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(
        pRenderer->pVkPhysicalDevice, pSwapchain->pVkSurface, &swapChainImageCount, NULL);
    REI_ASSERT(VK_SUCCESS == vk_res);

    // Allocate and get present modes
    modes = (VkPresentModeKHR*)REI_calloc(swapChainImageCount, sizeof(*modes));
    vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(
        pRenderer->pVkPhysicalDevice, pSwapchain->pVkSurface, &swapChainImageCount, modes);
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
        for (; i < swapChainImageCount; ++i)
        {
            if (modes[i] == mode)
            {
                break;
            }
        }
        if (i < swapChainImageCount)
        {
            present_mode = mode;
            break;
        }
    }

    // Free modes
    SAFE_FREE(modes);

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

    pSwapchain->desc.imageCount = REI_clamp(pSwapchain->desc.imageCount, caps.minImageCount, caps.maxImageCount);

    DECLARE_ZERO(VkSwapchainCreateInfoKHR, swapChainCreateInfo);
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.pNext = NULL;
    swapChainCreateInfo.flags = 0;
    swapChainCreateInfo.surface = pSwapchain->pVkSurface;
    swapChainCreateInfo.minImageCount = pSwapchain->desc.imageCount;
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
    REI_ASSERT(VK_SUCCESS == vk_res);

    pSwapchain->desc.colorFormat = util_from_vk_format(surface_format.format);

    // Create rendertargets from swapchain
    uint32_t image_count = 0;
    vk_res = vkGetSwapchainImagesKHR(pRenderer->pVkDevice, pSwapchain->pSwapchain, &image_count, NULL);
    REI_ASSERT(VK_SUCCESS == vk_res);

    REI_ASSERT(image_count == pSwapchain->desc.imageCount);

    pSwapchain->ppVkSwapchainImages = (VkImage*)REI_calloc(image_count, sizeof(*pSwapchain->ppVkSwapchainImages));
    REI_ASSERT(pSwapchain->ppVkSwapchainImages);

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
    descColor.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET;

    pSwapchain->ppSwapchainTextures =
        (REI_Texture**)REI_calloc(pSwapchain->desc.imageCount, sizeof(*pSwapchain->ppSwapchainTextures));

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

    for (uint32_t i = 0; i < pSwapchain->desc.imageCount; ++i)
    {
        REI_removeTexture(pRenderer, pSwapchain->ppSwapchainTextures[i]);
    }

    vkDestroySwapchainKHR(pRenderer->pVkDevice, pSwapchain->pSwapchain, NULL);
    vkDestroySurfaceKHR(pRenderer->pVkInstance, pSwapchain->pVkSurface, NULL);

    SAFE_FREE(pSwapchain->ppSwapchainTextures);
    SAFE_FREE(pSwapchain->ppVkSwapchainImages);
    SAFE_FREE(pSwapchain);
}

void REI_addBuffer(REI_Renderer* pRenderer, const REI_BufferDesc* pDesc, REI_Buffer** pp_buffer)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(pDesc->size > 0);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    REI_Buffer* pBuffer = (REI_Buffer*)REI_calloc(1, sizeof(*pBuffer));
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
        VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
        viewInfo.buffer = pBuffer->pVkBuffer;
        viewInfo.flags = 0;
        viewInfo.format = util_to_vk_format(pDesc->format);
        viewInfo.offset = pDesc->firstElement * pDesc->structStride;
        viewInfo.range = pDesc->elementCount * pDesc->structStride;
        VkFormatProperties formatProps = {};
        vkGetPhysicalDeviceFormatProperties(pRenderer->pVkPhysicalDevice, viewInfo.format, &formatProps);
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_UNIFORM_TEXEL_BUFFER_BIT))
        {
            REI_LOG(WARNING, "Failed to create uniform texel buffer view for format %u", (uint32_t)pDesc->format);
        }
        else
        {
            vkCreateBufferView(pRenderer->pVkDevice, &viewInfo, NULL, &pBuffer->pVkUniformTexelView);
        }
    }
    if (add_info.usage & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT)
    {
        VkBufferViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO, NULL };
        viewInfo.buffer = pBuffer->pVkBuffer;
        viewInfo.flags = 0;
        viewInfo.format = util_to_vk_format(pDesc->format);
        viewInfo.offset = pDesc->firstElement * pDesc->structStride;
        viewInfo.range = pDesc->elementCount * pDesc->structStride;
        VkFormatProperties formatProps = {};
        vkGetPhysicalDeviceFormatProperties(pRenderer->pVkPhysicalDevice, viewInfo.format, &formatProps);
        if (!(formatProps.bufferFeatures & VK_FORMAT_FEATURE_STORAGE_TEXEL_BUFFER_BIT))
        {
            REI_LOG(WARNING, "Failed to create storage texel buffer view for format %u", (uint32_t)pDesc->format);
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

    SAFE_FREE(pBuffer);
}

void REI_addTexture(REI_Renderer* pRenderer, const REI_TextureDesc* pDesc, REI_Texture** ppTexture)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc && pDesc->width && pDesc->height);
    if (pDesc->sampleCount > REI_SAMPLE_COUNT_1 && pDesc->mipLevels > 1)
    {
        REI_LOG(ERROR, "Multi-Sampled textures cannot have mip maps");
        REI_ASSERT(false);
        return;
    }

    REI_Texture* pTexture = (REI_Texture*)REI_calloc(1, sizeof(*pTexture));
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

    uint32_t descriptors = desc.descriptors;
    bool     isRT = descriptors & (REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_RENDER_TARGET_ARRAY_SLICES |
                               REI_DESCRIPTOR_TYPE_RENDER_TARGET_DEPTH_SLICES);
    bool const isDepth = util_has_depth_aspect(desc.format);
    REI_ASSERT(
        !((isDepth) && (desc.descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)) && "Cannot use depth stencil as UAV");

    VkImageUsageFlags additionalFlags =
        isRT ? (isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) : 0;

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
        add_info.format = util_to_vk_format(desc.format);
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

        if (desc.sampleCount != REI_SAMPLE_COUNT_1)
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
                REI_LOG(ERROR, "Cannot support 3D REI_Texture Array in Vulkan");
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
    srvDesc.format = util_to_vk_format(desc.format);
    srvDesc.components.r = VK_COMPONENT_SWIZZLE_R;
    srvDesc.components.g = VK_COMPONENT_SWIZZLE_G;
    srvDesc.components.b = VK_COMPONENT_SWIZZLE_B;
    srvDesc.components.a = VK_COMPONENT_SWIZZLE_A;
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
        pTexture->pVkSRVStencilDescriptor = (VkImageView*)REI_malloc(sizeof(VkImageView));
        srvDesc.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
        VkResult vk_res = vkCreateImageView(pRenderer->pVkDevice, &srvDesc, NULL, pTexture->pVkSRVStencilDescriptor);
        REI_ASSERT(VK_SUCCESS == vk_res);
    }

    // UAV
    if (descriptors & REI_DESCRIPTOR_TYPE_RW_TEXTURE)
    {
        pTexture->pVkUAVDescriptors = (VkImageView*)REI_calloc(desc.mipLevels, sizeof(VkImageView));
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
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_MAX_ENUM;
        if (desc.depth > 1)
            viewType = VK_IMAGE_VIEW_TYPE_3D;
        else if (desc.height > 1)
            viewType = desc.arraySize > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        else
            viewType = desc.arraySize > 1 ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_1D;

        VkImageViewCreateInfo rtvDesc = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL };
        rtvDesc.flags = 0;
        rtvDesc.image = pTexture->pVkImage;
        rtvDesc.viewType = viewType;
        rtvDesc.format = util_to_vk_format(pTexture->desc.format);
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

        pTexture->pVkRTDescriptors = (VkImageView*)REI_calloc(numRTVs, sizeof(VkImageView));

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
        desc.pDebugName = (wchar_t*)REI_calloc(wcslen(pDesc->pDebugName) + 1, sizeof(wchar_t));
        wcscpy((wchar_t*)desc.pDebugName, pDesc->pDebugName);
    }

    *ppTexture = pTexture;
}

void REI_removeTexture(REI_Renderer* pRenderer, REI_Texture* pTexture)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pTexture);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(VK_NULL_HANDLE != pTexture->pVkImage);

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

    SAFE_FREE((wchar_t*)pTexture->desc.pDebugName);
    SAFE_FREE(pTexture->pVkSRVStencilDescriptor);
    SAFE_FREE(pTexture->pVkUAVDescriptors);
    SAFE_FREE(pTexture->pVkRTDescriptors);
    SAFE_FREE(pTexture);
}

void REI_addSampler(REI_Renderer* pRenderer, const REI_SamplerDesc* pDesc, REI_Sampler** pp_sampler)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    REI_ASSERT(pDesc->compareFunc < REI_MAX_COMPARE_MODES);

    REI_Sampler* pSampler = (REI_Sampler*)REI_calloc(1, sizeof(*pSampler));
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

    SAFE_FREE(pSampler);
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
void REI_addDescriptorSet(
    REI_Renderer* pRenderer, const REI_DescriptorSetDesc* pDesc, REI_DescriptorSet** ppDescriptorSet)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(ppDescriptorSet);

    REI_DescriptorSet* pDescriptorSet = (REI_DescriptorSet*)REI_calloc(1, sizeof(*pDescriptorSet));
    REI_ASSERT(pDescriptorSet);

    const REI_RootSignature*     pRootSignature = pDesc->pRootSignature;
    const REI_DescriptorSetIndex setIndex = pDesc->setIndex;

    pDescriptorSet->pRootSignature = pRootSignature;
    pDescriptorSet->setIndex = setIndex;
    pDescriptorSet->dynamicOffsetCount = pRootSignature->vkDynamicDescriptorCounts[setIndex];
    pDescriptorSet->maxSets = pDesc->maxSets;

    if (pDescriptorSet->dynamicOffsetCount)
    {
        pDescriptorSet->pDynamicOffsets = (uint32_t**)REI_calloc(pDescriptorSet->maxSets, sizeof(uint32_t*));
        pDescriptorSet->pDynamicSizes = (uint32_t**)REI_calloc(pDescriptorSet->maxSets, sizeof(uint32_t*));
        for (uint32_t i = 0; i < pDescriptorSet->maxSets; ++i)
        {
            pDescriptorSet->pDynamicOffsets[i] =
                (uint32_t*)REI_calloc(pDescriptorSet->dynamicOffsetCount, sizeof(uint32_t));
            pDescriptorSet->pDynamicSizes[i] =
                (uint32_t*)REI_calloc(pDescriptorSet->dynamicOffsetCount, sizeof(uint32_t));
        }
    }

    if (VK_NULL_HANDLE != pRootSignature->vkDescriptorSetLayouts[setIndex])
    {
        pDescriptorSet->pHandles = (VkDescriptorSet*)REI_calloc(pDesc->maxSets, sizeof(VkDescriptorSet));
        pDescriptorSet->ppUpdateData =
            (DescriptorUpdateData**)REI_calloc(pDesc->maxSets, sizeof(DescriptorUpdateData*));

        VkDescriptorSetLayout* pLayouts =
            (VkDescriptorSetLayout*)alloca(pDesc->maxSets * sizeof(VkDescriptorSetLayout));
        VkDescriptorSet** pHandles = (VkDescriptorSet**)alloca(pDesc->maxSets * sizeof(VkDescriptorSet*));

        for (uint32_t i = 0; i < pDesc->maxSets; ++i)
        {
            pLayouts[i] = pRootSignature->vkDescriptorSetLayouts[setIndex];
            pHandles[i] = &pDescriptorSet->pHandles[i];

            pDescriptorSet->ppUpdateData[i] = (DescriptorUpdateData*)REI_malloc(
                pRootSignature->vkCumulativeDescriptorCounts[setIndex] * sizeof(DescriptorUpdateData));
            memcpy(
                pDescriptorSet->ppUpdateData[i], pRootSignature->pUpdateTemplateData[setIndex],
                pRootSignature->vkCumulativeDescriptorCounts[setIndex] * sizeof(DescriptorUpdateData));
        }

        consume_descriptor_sets(pRenderer->pDescriptorPool, pLayouts, pHandles, pDesc->maxSets);
    }
    else
    {
        REI_LOG(
            ERROR, "NULL Descriptor Set Layout for update frequency %u. Cannot allocate descriptor set",
            (uint32_t)setIndex);
        REI_ASSERT(false && "NULL Descriptor Set Layout for update frequency. Cannot allocate descriptor set");
    }

    *ppDescriptorSet = pDescriptorSet;
}

void REI_removeDescriptorSet(REI_Renderer* pRenderer, REI_DescriptorSet* pDescriptorSet)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDescriptorSet);

    if (pDescriptorSet->dynamicOffsetCount)
    {
        for (uint32_t i = 0; i < pDescriptorSet->maxSets; ++i)
        {
            SAFE_FREE(pDescriptorSet->pDynamicOffsets[i]);
            SAFE_FREE(pDescriptorSet->pDynamicSizes[i]);
        }
    }

    for (uint32_t i = 0; i < pDescriptorSet->maxSets; ++i)
    {
        SAFE_FREE(pDescriptorSet->ppUpdateData[i]);
    }

    SAFE_FREE(pDescriptorSet->ppUpdateData);
    SAFE_FREE(pDescriptorSet->pHandles);
    SAFE_FREE(pDescriptorSet->pDynamicOffsets);
    SAFE_FREE(pDescriptorSet->pDynamicSizes);
    SAFE_FREE(pDescriptorSet);
}

void REI_resetDescriptorSet(REI_Renderer* pRenderer, REI_DescriptorSet* pDescriptorSet, uint32_t index)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDescriptorSet);
    REI_ASSERT(pDescriptorSet->pHandles);
    REI_ASSERT(index < pDescriptorSet->maxSets);

    const REI_RootSignature* pRootSignature = pDescriptorSet->pRootSignature;
    REI_DescriptorSetIndex   setIndex = (REI_DescriptorSetIndex)pDescriptorSet->setIndex;
    DescriptorUpdateData*    pUpdateData = pDescriptorSet->ppUpdateData[index];

    memcpy(
        pUpdateData, pRootSignature->pUpdateTemplateData[setIndex],
        pRootSignature->vkCumulativeDescriptorCounts[setIndex] * sizeof(DescriptorUpdateData));

    pRenderer->pfn_vkUpdateDescriptorSetWithTemplateKHR(
        pRenderer->pVkDevice, pDescriptorSet->pHandles[index], pRootSignature->updateTemplates[setIndex], pUpdateData);
}

void REI_updateDescriptorSet(
    REI_Renderer* pRenderer, REI_DescriptorSet* pDescriptorSet, uint32_t index, uint32_t count,
    const REI_DescriptorData* pParams)
{
#ifdef _DEBUG
#    define VALIDATE_DESCRIPTOR(descriptor, msg, ...)                 \
        if (!(descriptor))                                            \
        {                                                             \
            REI_LOG(ERROR, "%s : " msg, __FUNCTION__, ##__VA_ARGS__); \
            REI_ASSERT(false, "Descriptor validation failed");        \
            continue;                                                 \
        }
#else
#    define VALIDATE_DESCRIPTOR(descriptor, ...)
#endif

    REI_ASSERT(pRenderer);
    REI_ASSERT(pDescriptorSet);
    REI_ASSERT(pDescriptorSet->pHandles);
    REI_ASSERT(index < pDescriptorSet->maxSets);

    const REI_RootSignature* pRootSignature = pDescriptorSet->pRootSignature;
    REI_DescriptorSetIndex   setIndex = (REI_DescriptorSetIndex)pDescriptorSet->setIndex;
    DescriptorUpdateData*    pUpdateData = pDescriptorSet->ppUpdateData[index];

    bool update = false;

    for (uint32_t i = 0; i < count; ++i)
    {
        const REI_DescriptorData* pParam = pParams + i;
        uint32_t                  paramIndex = pParam->index;

        VALIDATE_DESCRIPTOR(pParam->pName || (paramIndex != -1), "REI_DescriptorData has NULL name and invalid index");

        const REI_DescriptorInfo* pDesc = (paramIndex != -1) ? (pRootSignature->pDescriptors + paramIndex)
                                                             : get_descriptor(pRootSignature, pParam->pName);
        if (paramIndex != -1)
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param index (%u)", paramIndex);
        }
        else
        {
            VALIDATE_DESCRIPTOR(pDesc, "Invalid descriptor with param name (%s)", pParam->pName);
        }

        const REI_DescriptorType type = pDesc->desc.type;
        const uint32_t           arrayCount = REI_max(1U, pParam->count);

        VALIDATE_DESCRIPTOR(pDesc->desc.set == setIndex, "Descriptor (%s) - Mismatching set index", pDesc->desc.name);

        switch (type)
        {
            case REI_DESCRIPTOR_TYPE_SAMPLER:
            {
                // Index is invalid when descriptor is a static sampler
                VALIDATE_DESCRIPTOR(
                    pDesc->indexInParent != -1,
                    "Trying to update a static sampler (%s). All static samplers must be set in addRootSignature and "
                    "cannot be updated later",
                    pDesc->desc.name);

                VALIDATE_DESCRIPTOR(pParam->ppSamplers, "NULL REI_Sampler (%s)", pDesc->desc.name);

                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppSamplers[arr], "NULL REI_Sampler (%s [%u] )", pDesc->desc.name, arr);

                    pUpdateData[pDesc->handleIndex + arr].mImageInfo = pParam->ppSamplers[arr]->vkSamplerView;
                    update = true;
                }
                break;
            }
            case REI_DESCRIPTOR_TYPE_TEXTURE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL REI_Texture (%s)", pDesc->desc.name);

                if (!pParam->bindStencilResource)
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(
                            pParam->ppTextures[arr], "NULL REI_Texture (%s [%u] )", pDesc->desc.name, arr);

                        pUpdateData[pDesc->handleIndex + arr].mImageInfo = {
                            VK_NULL_HANDLE,                               // REI_Sampler
                            pParam->ppTextures[arr]->pVkSRVDescriptor,    // Image View
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL      // Image Layout
                        };

                        update = true;
                    }
                }
                else
                {
                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(
                            pParam->ppTextures[arr], "NULL REI_Texture (%s [%u] )", pDesc->desc.name, arr);

                        pUpdateData[pDesc->handleIndex + arr].mImageInfo = {
                            VK_NULL_HANDLE,                                       // REI_Sampler
                            *pParam->ppTextures[arr]->pVkSRVStencilDescriptor,    // Image View
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL              // Image Layout
                        };

                        update = true;
                    }
                }
                break;
            }
            case REI_DESCRIPTOR_TYPE_RW_TEXTURE:
            {
                VALIDATE_DESCRIPTOR(pParam->ppTextures, "NULL RW REI_Texture (%s)", pDesc->desc.name);
                const uint32_t mipSlice = pParam->UAVMipSlice;

                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(
                        pParam->ppTextures[arr], "NULL RW REI_Texture (%s [%u] )", pDesc->desc.name, arr);
                    VALIDATE_DESCRIPTOR(
                        mipSlice < pParam->ppTextures[arr]->desc.mipLevels,
                        "Descriptor : (%s [%u] ) Mip Slice (%u) exceeds mip levels (%u)", pDesc->desc.name, arr,
                        mipSlice, pParam->ppTextures[arr]->desc.mipLevels);

                    pUpdateData[pDesc->handleIndex + arr].mImageInfo = {
                        VK_NULL_HANDLE,                                          // REI_Sampler
                        pParam->ppTextures[arr]->pVkUAVDescriptors[mipSlice],    // Image View
                        VK_IMAGE_LAYOUT_GENERAL                                  // Image Layout
                    };

                    update = true;
                }
                break;
            }
            case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            {
                if (pDesc->vkType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Uniform REI_Buffer (%s)", pDesc->desc.name);
                    VALIDATE_DESCRIPTOR(
                        pParam->ppBuffers[0], "NULL Uniform REI_Buffer (%s [%u] )", pDesc->desc.name, 0);
                    VALIDATE_DESCRIPTOR(
                        arrayCount == 1,
                        "Descriptor (%s) : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC does not support arrays",
                        pDesc->desc.name);
                    VALIDATE_DESCRIPTOR(
                        pParam->pSizes,
                        "Descriptor (%s) : Must provide pSizes for VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
                        pDesc->desc.name);
                    VALIDATE_DESCRIPTOR(
                        pParam->pSizes[0] > 0, "Descriptor (%s) - pSizes[%u] is zero", pDesc->desc.name, 0);
                    VALIDATE_DESCRIPTOR(
                        pParam->pSizes[0] <= pRenderer->vkDeviceProperties.properties.limits.maxUniformBufferRange,
                        "Descriptor (%s) - pSizes[%u] is %ull which exceeds max size %u", pDesc->desc.name, 0,
                        pParam->pSizes[0], pRenderer->vkDeviceProperties.properties.limits.maxUniformBufferRange);

                    pDescriptorSet->pDynamicOffsets[index][pDesc->dynamicUniformIndex] =
                        pParam->pOffsets ? (uint32_t)pParam->pOffsets[0] : 0;
                    pUpdateData[pDesc->handleIndex + 0].mBufferInfo = pParam->ppBuffers[0]->vkBufferInfo;
                    pUpdateData[pDesc->handleIndex + 0].mBufferInfo.range = pParam->pSizes[0];

                    // If this is a different size we have to update the VkDescriptorBufferInfo::range so a call to vkUpdateDescriptorSet is necessary
                    if (pParam->pSizes[0] != (uint32_t)pDescriptorSet->pDynamicSizes[index][pDesc->dynamicUniformIndex])
                    {
                        pDescriptorSet->pDynamicSizes[index][pDesc->dynamicUniformIndex] = (uint32_t)pParam->pSizes[0];
                        update = true;
                    }

                    break;
                }
                case REI_DESCRIPTOR_TYPE_BUFFER:
                case REI_DESCRIPTOR_TYPE_BUFFER_RAW:
                case REI_DESCRIPTOR_TYPE_RW_BUFFER:
                case REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                {
                    VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL REI_Buffer (%s)", pDesc->desc.name);

                    for (uint32_t arr = 0; arr < arrayCount; ++arr)
                    {
                        VALIDATE_DESCRIPTOR(
                            pParam->ppBuffers[arr], "NULL REI_Buffer (%s [%u] )", pDesc->desc.name, arr);

                        pUpdateData[pDesc->handleIndex + arr].mBufferInfo = pParam->ppBuffers[arr]->vkBufferInfo;
                        if (pParam->pOffsets)
                        {
                            VALIDATE_DESCRIPTOR(
                                pParam->pSizes, "Descriptor (%s) - pSizes must be provided with pOffsets",
                                pDesc->desc.name);
                            VALIDATE_DESCRIPTOR(
                                pParam->pSizes[arr] > 0, "Descriptor (%s) - pSizes[%u] is zero", pDesc->desc.name, arr);
                            VALIDATE_DESCRIPTOR(
                                pParam->pSizes[arr] <=
                                    pRenderer->vkDeviceProperties.properties.limits.maxUniformBufferRange,
                                "Descriptor (%s) - pSizes[%u] is %ull which exceeds max size %u", pDesc->desc.name, arr,
                                pParam->pSizes[arr],
                                pRenderer->vkDeviceProperties.properties.limits.maxUniformBufferRange);

                            pUpdateData[pDesc->handleIndex + arr].mBufferInfo.offset = pParam->pOffsets[arr];
                            pUpdateData[pDesc->handleIndex + arr].mBufferInfo.range = pParam->pSizes[arr];
                        }

                        update = true;
                    }
                }
                break;
            }
            case REI_DESCRIPTOR_TYPE_TEXEL_BUFFER:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL Texel REI_Buffer (%s)", pDesc->desc.name);

                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(
                        pParam->ppBuffers[arr], "NULL Texel REI_Buffer (%s [%u] )", pDesc->desc.name, arr);
                    pUpdateData[pDesc->handleIndex + arr].mBuferView = pParam->ppBuffers[arr]->pVkUniformTexelView;
                    update = true;
                }

                break;
            }
            case REI_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
            {
                VALIDATE_DESCRIPTOR(pParam->ppBuffers, "NULL RW Texel REI_Buffer (%s)", pDesc->desc.name);

                for (uint32_t arr = 0; arr < arrayCount; ++arr)
                {
                    VALIDATE_DESCRIPTOR(
                        pParam->ppBuffers[arr], "NULL RW Texel REI_Buffer (%s [%u] )", pDesc->desc.name, arr);
                    pUpdateData[pDesc->handleIndex + arr].mBuferView = pParam->ppBuffers[arr]->pVkStorageTexelView;
                    update = true;
                }

                break;
            }

            default: break;
        }
    }

    // If this was called to just update a dynamic offset skip the update
    if (update)
    {
        pRenderer->pfn_vkUpdateDescriptorSetWithTemplateKHR(
            pRenderer->pVkDevice, pDescriptorSet->pHandles[index], pRootSignature->updateTemplates[setIndex],
            pUpdateData);
    }
}

void REI_cmdBindDescriptorSet(REI_Cmd* pCmd, uint32_t index, REI_DescriptorSet* pDescriptorSet)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pDescriptorSet);
    REI_ASSERT(pDescriptorSet->pHandles);
    REI_ASSERT(index < pDescriptorSet->maxSets);

    const REI_RootSignature* pRootSignature = pDescriptorSet->pRootSignature;

    if (pCmd->pBoundRootSignature != pRootSignature)
    {
        pCmd->pBoundRootSignature = pRootSignature;

        // Vulkan requires to bind all descriptor sets upto the highest set number even if they are empty
        // Example: If shader uses only set 2, we still have to bind empty sets for set=0 and set=1
        for (uint32_t setIndex = 0; setIndex < REI_DESCRIPTOR_SET_COUNT; ++setIndex)
        {
            if (pRootSignature->vkEmptyDescriptorSets[setIndex] != VK_NULL_HANDLE)
            {
                vkCmdBindDescriptorSets(
                    pCmd->pVkCmdBuf, gPipelineBindPoint[pRootSignature->pipelineType], pRootSignature->pPipelineLayout,
                    setIndex, 1, &pRootSignature->vkEmptyDescriptorSets[setIndex], 0, NULL);
            }
        }
    }

    vkCmdBindDescriptorSets(
        pCmd->pVkCmdBuf, gPipelineBindPoint[pRootSignature->pipelineType], pRootSignature->pPipelineLayout,
        pDescriptorSet->setIndex, 1, &pDescriptorSet->pHandles[index], pDescriptorSet->dynamicOffsetCount,
        pDescriptorSet->dynamicOffsetCount ? pDescriptorSet->pDynamicOffsets[index] : NULL);
}

void REI_cmdBindPushConstants(
    REI_Cmd* pCmd, REI_RootSignature* pRootSignature, const char* pName, const void* pConstants)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pConstants);
    REI_ASSERT(pRootSignature);
    REI_ASSERT(pName);

    const REI_DescriptorInfo* pDesc = get_descriptor(pRootSignature, pName);
    REI_ASSERT(pDesc);
    REI_ASSERT(REI_DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->desc.type);

    vkCmdPushConstants(
        pCmd->pVkCmdBuf, pRootSignature->pPipelineLayout, pDesc->vkStages, pDesc->desc.reg, pDesc->desc.size,
        pConstants);
}

void REI_cmdBindPushConstantsByIndex(
    REI_Cmd* pCmd, REI_RootSignature* pRootSignature, uint32_t paramIndex, const void* pConstants)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pConstants);
    REI_ASSERT(pRootSignature);
    REI_ASSERT(paramIndex >= 0 && paramIndex < pRootSignature->descriptorCount);

    const REI_DescriptorInfo* pDesc = pRootSignature->pDescriptors + paramIndex;
    REI_ASSERT(pDesc);
    REI_ASSERT(REI_DESCRIPTOR_TYPE_ROOT_CONSTANT == pDesc->desc.type);

    vkCmdPushConstants(
        pCmd->pVkCmdBuf, pRootSignature->pPipelineLayout, pDesc->vkStages, 0, pDesc->desc.size, pConstants);
}

void REI_addShader(REI_Renderer* pRenderer, const REI_ShaderDesc* pDesc, REI_Shader** ppShaderProgram)
{
    REI_Shader* pShaderProgram = REI_new(REI_Shader);
    REI_ASSERT(pShaderProgram);

    pShaderProgram->stages = pDesc->stages;

    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    uint32_t                    counter = 0;
    REI_ShaderReflection        stageReflections[REI_SHADER_STAGE_COUNT] = {};
    std::vector<VkShaderModule> modules(REI_SHADER_STAGE_COUNT);

    for (uint32_t i = 0; i < REI_SHADER_STAGE_COUNT; ++i)
    {
        REI_ShaderStage stage_mask = (REI_ShaderStage)(1 << i);
        if (stage_mask == (pShaderProgram->stages & stage_mask))
        {
            DECLARE_ZERO(VkShaderModuleCreateInfo, create_info);
            create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            create_info.pNext = NULL;
            create_info.flags = 0;

            const REI_ShaderSource* pStageDesc = nullptr;
            switch (stage_mask)
            {
                case REI_SHADER_STAGE_VERT:
                {
                    vk_createShaderReflection(
                        (const uint8_t*)pDesc->vert.pByteCode, (uint32_t)pDesc->vert.byteCodeSize, stage_mask,
                        &stageReflections[counter]);

                    create_info.codeSize = pDesc->vert.byteCodeSize;
                    create_info.pCode = (const uint32_t*)pDesc->vert.pByteCode;
                    pStageDesc = &pDesc->vert;
                    VkResult vk_res =
                        vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
                    REI_ASSERT(VK_SUCCESS == vk_res);
                }
                break;
                case REI_SHADER_STAGE_TESC:
                {
                    vk_createShaderReflection(
                        (const uint8_t*)pDesc->hull.pByteCode, (uint32_t)pDesc->hull.byteCodeSize, stage_mask,
                        &stageReflections[counter]);

                    create_info.codeSize = pDesc->hull.byteCodeSize;
                    create_info.pCode = (const uint32_t*)pDesc->hull.pByteCode;
                    pStageDesc = &pDesc->hull;
                    VkResult vk_res =
                        vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
                    REI_ASSERT(VK_SUCCESS == vk_res);
                }
                break;
                case REI_SHADER_STAGE_TESE:
                {
                    vk_createShaderReflection(
                        (const uint8_t*)pDesc->domain.pByteCode, (uint32_t)pDesc->domain.byteCodeSize, stage_mask,
                        &stageReflections[counter]);

                    create_info.codeSize = pDesc->domain.byteCodeSize;
                    create_info.pCode = (const uint32_t*)pDesc->domain.pByteCode;
                    pStageDesc = &pDesc->domain;
                    VkResult vk_res =
                        vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
                    REI_ASSERT(VK_SUCCESS == vk_res);
                }
                break;
                case REI_SHADER_STAGE_GEOM:
                {
                    vk_createShaderReflection(
                        (const uint8_t*)pDesc->geom.pByteCode, (uint32_t)pDesc->geom.byteCodeSize, stage_mask,
                        &stageReflections[counter]);

                    create_info.codeSize = pDesc->geom.byteCodeSize;
                    create_info.pCode = (const uint32_t*)pDesc->geom.pByteCode;
                    pStageDesc = &pDesc->geom;
                    VkResult vk_res =
                        vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
                    REI_ASSERT(VK_SUCCESS == vk_res);
                }
                break;
                case REI_SHADER_STAGE_FRAG:
                {
                    vk_createShaderReflection(
                        (const uint8_t*)pDesc->frag.pByteCode, (uint32_t)pDesc->frag.byteCodeSize, stage_mask,
                        &stageReflections[counter]);

                    create_info.codeSize = pDesc->frag.byteCodeSize;
                    create_info.pCode = (const uint32_t*)pDesc->frag.pByteCode;
                    pStageDesc = &pDesc->frag;
                    VkResult vk_res =
                        vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
                    REI_ASSERT(VK_SUCCESS == vk_res);
                }
                break;
                case REI_SHADER_STAGE_COMP:
                {
                    vk_createShaderReflection(
                        (const uint8_t*)pDesc->comp.pByteCode, (uint32_t)pDesc->comp.byteCodeSize, stage_mask,
                        &stageReflections[counter]);

                    create_info.codeSize = pDesc->comp.byteCodeSize;
                    create_info.pCode = (const uint32_t*)pDesc->comp.pByteCode;
                    pStageDesc = &pDesc->comp;
                    VkResult vk_res =
                        vkCreateShaderModule(pRenderer->pVkDevice, &create_info, NULL, &(modules[counter]));
                    REI_ASSERT(VK_SUCCESS == vk_res);
                }
                break;
                default: REI_ASSERT(false && "REI_Shader Stage not supported!"); break;
            }

            ++counter;
        }
    }

    pShaderProgram->pShaderModules = (VkShaderModule*)REI_calloc(counter, sizeof(VkShaderModule));
    memcpy(pShaderProgram->pShaderModules, modules.data(), counter * sizeof(VkShaderModule));

    REI_createPipelineReflection(stageReflections, counter, &pShaderProgram->reflection);

    *ppShaderProgram = pShaderProgram;
}

void REI_removeShader(REI_Renderer* pRenderer, REI_Shader* pShaderProgram)
{
    REI_ASSERT(pRenderer);

    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);

    if (pShaderProgram->stages & REI_SHADER_STAGE_VERT)
    {
        vkDestroyShaderModule(
            pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->reflection.mVertexStageIndex], NULL);
    }

    if (pShaderProgram->stages & REI_SHADER_STAGE_TESC)
    {
        vkDestroyShaderModule(
            pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->reflection.mHullStageIndex], NULL);
    }

    if (pShaderProgram->stages & REI_SHADER_STAGE_TESE)
    {
        vkDestroyShaderModule(
            pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->reflection.mDomainStageIndex], NULL);
    }

    if (pShaderProgram->stages & REI_SHADER_STAGE_GEOM)
    {
        vkDestroyShaderModule(
            pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->reflection.mGeometryStageIndex], NULL);
    }

    if (pShaderProgram->stages & REI_SHADER_STAGE_FRAG)
    {
        vkDestroyShaderModule(
            pRenderer->pVkDevice, pShaderProgram->pShaderModules[pShaderProgram->reflection.mPixelStageIndex], NULL);
    }

    if (pShaderProgram->stages & REI_SHADER_STAGE_COMP)
    {
        vkDestroyShaderModule(pRenderer->pVkDevice, pShaderProgram->pShaderModules[0], NULL);
    }

    REI_destroyPipelineReflection(&pShaderProgram->reflection);
    SAFE_FREE(pShaderProgram->pShaderModules);
    REI_delete(pShaderProgram);
}
/************************************************************************/
// Root Signature Functions
/************************************************************************/
typedef struct REI_UpdateFrequencyLayoutInfo
{
    /// Array of all bindings in the descriptor set
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    /// Array of all descriptors in this descriptor set
    std::vector<REI_DescriptorInfo*> descriptors;
    /// Array of all descriptors marked as dynamic in this descriptor set (applicable to REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
    std::vector<REI_DescriptorInfo*> dynamicDescriptors;
    /// Hash map to get index of the descriptor in the root signature
    std::unordered_map<REI_DescriptorInfo*, uint32_t> descriptorIndexMap;
} REI_UpdateFrequencyLayoutInfo;

void REI_addRootSignature(
    REI_Renderer* pRenderer, const REI_RootSignatureDesc* pRootSignatureDesc, REI_RootSignature** ppRootSignature)
{
    REI_RootSignature* pRootSignature = REI_new(REI_RootSignature);

    std::vector<REI_UpdateFrequencyLayoutInfo>    layouts(REI_DESCRIPTOR_SET_COUNT);
    std::vector<REI_DescriptorInfo*>              pushConstantDescriptors;
    std::vector<REI_ShaderResource>               shaderResources;
    std::unordered_map<std::string, REI_Sampler*> staticSamplerMap;

    for (uint32_t i = 0; i < pRootSignatureDesc->staticSamplerCount; ++i)
    {
        REI_ASSERT(pRootSignatureDesc->ppStaticSamplers[i]);
        staticSamplerMap.insert(
            { { pRootSignatureDesc->ppStaticSamplerNames[i], pRootSignatureDesc->ppStaticSamplers[i] } });
    }

    // Collect all unique shader resources in the given shaders
    // Resources are parsed by name (two resources named "XYZ" in two shaders will be considered the same resource)
    for (uint32_t sh = 0; sh < pRootSignatureDesc->shaderCount; ++sh)
    {
        REI_PipelineReflection const* pReflection = &pRootSignatureDesc->ppShaders[sh]->reflection;

        if (pReflection->mShaderStages & REI_SHADER_STAGE_COMP)
            pRootSignature->pipelineType = REI_PIPELINE_TYPE_COMPUTE;
        else
            pRootSignature->pipelineType = REI_PIPELINE_TYPE_GRAPHICS;

        for (uint32_t i = 0; i < pReflection->mShaderResourceCount; ++i)
        {
            REI_ShaderResource const* pRes = &pReflection->pShaderResources[i];
            uint32_t                  setIndex = pRes->set;

            if (pRes->type == REI_DESCRIPTOR_TYPE_ROOT_CONSTANT)
                setIndex = 0;

            std::unordered_map<std::string, uint32_t>::iterator it =
                pRootSignature->pDescriptorNameToIndexMap.find(pRes->name);
            if (it == pRootSignature->pDescriptorNameToIndexMap.end())
            {
                pRootSignature->pDescriptorNameToIndexMap.insert({ pRes->name, (uint32_t)shaderResources.size() });
                shaderResources.emplace_back(*pRes);
            }
            else
            {
                if (shaderResources[it->second].reg != pRes->reg)
                {
                    REI_LOG(
                        ERROR,
                        "\nFailed to create root signature\n"
                        "Shared shader resource %s has mismatching binding. All shader resources "
                        "shared by multiple shaders specified in addRootSignature "
                        "must have the same binding and set",
                        pRes->name);
                    return;
                }
                if (shaderResources[it->second].set != pRes->set)
                {
                    REI_LOG(
                        ERROR,
                        "\nFailed to create root signature\n"
                        "Shared shader resource %s has mismatching set. All shader resources "
                        "shared by multiple shaders specified in addRootSignature "
                        "must have the same binding and set",
                        pRes->name);
                    return;
                }

                for (REI_ShaderResource& res: shaderResources)
                {
                    if (strcmp(res.name, it->first.c_str()) == 0)
                    {
                        res.used_stages |= pRes->used_stages;
                        break;
                    }
                }
            }
        }
    }

    if ((uint32_t)shaderResources.size())
    {
        pRootSignature->descriptorCount = (uint32_t)shaderResources.size();
        pRootSignature->pDescriptors =
            (REI_DescriptorInfo*)REI_calloc(pRootSignature->descriptorCount, sizeof(REI_DescriptorInfo));
    }

    // Fill the descriptor array to be stored in the root signature
    for (uint32_t i = 0; i < (uint32_t)shaderResources.size(); ++i)
    {
        REI_DescriptorInfo*       pDesc = &pRootSignature->pDescriptors[i];
        REI_ShaderResource const* pRes = &shaderResources[i];
        REI_DescriptorSetIndex    setIndex = (REI_DescriptorSetIndex)pRes->set;

        // Copy the binding information generated from the shader reflection into the descriptor
        pDesc->desc.reg = pRes->reg;
        pDesc->desc.set = pRes->set;
        pDesc->desc.size = pRes->size;
        pDesc->desc.type = pRes->type;
        pDesc->desc.used_stages = pRes->used_stages;
        pDesc->desc.name_size = pRes->name_size;
        pDesc->desc.name = (const char*)REI_calloc(pDesc->desc.name_size + 1, sizeof(char));
        pDesc->desc.dim = pRes->dim;
        memcpy((char*)pDesc->desc.name, pRes->name, pRes->name_size);

        // If descriptor is not a root constant create a new layout binding for this descriptor and add it to the binding array
        if (pDesc->desc.type != REI_DESCRIPTOR_TYPE_ROOT_CONSTANT)
        {
            VkDescriptorSetLayoutBinding binding = {};
            binding.binding = pDesc->desc.reg;
            binding.descriptorCount = pDesc->desc.size;
            binding.descriptorType = util_to_vk_descriptor_type(pDesc->desc.type);

            // If a user specified a uniform buffer to be used as a dynamic uniform buffer change its type to VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
            // Also log a message for debugging purpose
            if (std::string(pDesc->desc.name).find("rootcbv") != std::string::npos)
            {
                if (pDesc->desc.size == 1)
                {
                    REI_LOG(
                        INFO, "Descriptor (%s) : User specified VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
                        pDesc->desc.name);
                    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                }
                else
                {
                    REI_LOG(
                        WARNING, "Descriptor (%s) : Cannot use VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC for arrays",
                        pDesc->desc.name);
                }
            }

            binding.stageFlags = util_to_vk_shader_stage_flags(pDesc->desc.used_stages);

            // Store the vulkan related info in the descriptor to avoid constantly calling the util_to_vk mapping functions
            pDesc->vkType = binding.descriptorType;
            pDesc->vkStages = binding.stageFlags;
            pDesc->setIndex = setIndex;

            if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                layouts[setIndex].dynamicDescriptors.emplace_back(pDesc);
            }

            // Find if the given descriptor is a static sampler
            decltype(staticSamplerMap)::iterator it = staticSamplerMap.find(pDesc->desc.name);
            if (it != staticSamplerMap.end())
            {
                REI_LOG(INFO, "Descriptor (%s) : User specified Static REI_Sampler", pDesc->desc.name);

                // Set the index to an invalid value so we can use this later for error checking if user tries to update a static sampler
                pDesc->indexInParent = -1;
                binding.pImmutableSamplers = &it->second->pVkSampler;
            }
            else
            {
                layouts[setIndex].descriptors.emplace_back(pDesc);
            }

            layouts[setIndex].bindings.push_back(binding);
        }
        // If descriptor is a root constant, add it to the root constant array
        else
        {
            pDesc->desc.set = 0;
            pDesc->vkStages = util_to_vk_shader_stage_flags(pDesc->desc.used_stages);
            setIndex = REI_DESCRIPTOR_SET_INDEX_0;
            pushConstantDescriptors.emplace_back(pDesc);
        }

        layouts[setIndex].descriptorIndexMap[pDesc] = i;
    }

    pRootSignature->vkPushConstantCount = (uint32_t)pushConstantDescriptors.size();
    if (pRootSignature->vkPushConstantCount)
        pRootSignature->pVkPushConstantRanges = (VkPushConstantRange*)REI_calloc(
            pRootSignature->vkPushConstantCount, sizeof(*pRootSignature->pVkPushConstantRanges));

    // Create push constant ranges
    for (uint32_t i = 0; i < pRootSignature->vkPushConstantCount; ++i)
    {
        VkPushConstantRange* pConst = &pRootSignature->pVkPushConstantRanges[i];
        REI_DescriptorInfo*  pDesc = pushConstantDescriptors[i];
        pDesc->indexInParent = i;
        pConst->offset = pDesc->desc.reg;
        pConst->size = pDesc->desc.size;
        pConst->stageFlags = util_to_vk_shader_stage_flags(pDesc->desc.used_stages);
    }

    // Create descriptor layouts
    // Put most frequently changed params first
    for (uint32_t i = (uint32_t)layouts.size(); i-- > 0U;)
    {
        REI_UpdateFrequencyLayoutInfo& layout = layouts[i];

        if (layouts[i].bindings.size())
        {
            // sort table by type (CBV/SRV/UAV) by register
            std::stable_sort(
                layout.bindings.begin(), layout.bindings.end(),
                [](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs)
                { return lhs.binding > rhs.binding; });
            std::stable_sort(
                layout.bindings.begin(), layout.bindings.end(),
                [](const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs)
                { return lhs.descriptorType > rhs.descriptorType; });
        }

        bool createLayout = layout.bindings.size() > 0;
        // Check if we need to create an empty layout in case there is an empty set between two used sets
        // Example: set = 0 is used, set = 2 is used. In this case, set = 1 needs to exist even if it is empty
        if (!createLayout && i < layouts.size() - 1)
        {
            createLayout = pRootSignature->vkDescriptorSetLayouts[i + 1] != VK_NULL_HANDLE;
        }

        if (createLayout)
        {
            VkDescriptorSetLayoutCreateInfo layoutInfo = {};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.pNext = NULL;
            layoutInfo.bindingCount = (uint32_t)layout.bindings.size();
            layoutInfo.pBindings = layout.bindings.data();
            layoutInfo.flags = 0;

            vkCreateDescriptorSetLayout(
                pRenderer->pVkDevice, &layoutInfo, NULL, &pRootSignature->vkDescriptorSetLayouts[i]);
        }

        if (!layouts[i].bindings.size())
            continue;

        pRootSignature->vkDescriptorCounts[i] = (uint32_t)layout.descriptors.size();

        // Loop through descriptors belonging to this update frequency and increment the cumulative descriptor count
        for (uint32_t descIndex = 0; descIndex < (uint32_t)layout.descriptors.size(); ++descIndex)
        {
            REI_DescriptorInfo* pDesc = layout.descriptors[descIndex];
            pDesc->indexInParent = descIndex;
            pDesc->handleIndex = pRootSignature->vkCumulativeDescriptorCounts[i];
            pRootSignature->vkCumulativeDescriptorCounts[i] += pDesc->desc.size;
        }

        std::sort(
            layout.dynamicDescriptors.begin(), layout.dynamicDescriptors.end(),
            [](REI_DescriptorInfo* const lhs, REI_DescriptorInfo* const rhs) { return lhs->desc.reg > rhs->desc.reg; });

        pRootSignature->vkDynamicDescriptorCounts[i] = (uint32_t)layout.dynamicDescriptors.size();
        for (uint32_t descIndex = 0; descIndex < pRootSignature->vkDynamicDescriptorCounts[i]; ++descIndex)
        {
            REI_DescriptorInfo* pDesc = layout.dynamicDescriptors[descIndex];
            pDesc->dynamicUniformIndex = descIndex;
        }
    }
    /************************************************************************/
    // REI_Pipeline layout
    /************************************************************************/
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange>   pushConstants(pRootSignature->vkPushConstantCount);
    for (uint32_t i = 0; i < REI_DESCRIPTOR_SET_COUNT; ++i)
        if (pRootSignature->vkDescriptorSetLayouts[i])
            descriptorSetLayouts.emplace_back(pRootSignature->vkDescriptorSetLayouts[i]);
    for (uint32_t i = 0; i < pRootSignature->vkPushConstantCount; ++i)
        pushConstants[i] = pRootSignature->pVkPushConstantRanges[i];

    DECLARE_ZERO(VkPipelineLayoutCreateInfo, add_info);
    add_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    add_info.pSetLayouts = descriptorSetLayouts.data();
    add_info.pushConstantRangeCount = pRootSignature->vkPushConstantCount;
    add_info.pPushConstantRanges = pushConstants.data();
    VkResult vk_res = vkCreatePipelineLayout(pRenderer->pVkDevice, &add_info, NULL, &(pRootSignature->pPipelineLayout));
    REI_ASSERT(VK_SUCCESS == vk_res);
    /************************************************************************/
    // Update templates
    /************************************************************************/
    for (uint32_t setIndex = 0; setIndex < REI_DESCRIPTOR_SET_COUNT; ++setIndex)
    {
        if (pRootSignature->vkDescriptorCounts[setIndex])
        {
            const REI_UpdateFrequencyLayoutInfo& layout = layouts[setIndex];
            VkDescriptorUpdateTemplateEntry*     pEntries = (VkDescriptorUpdateTemplateEntry*)alloca(
                pRootSignature->vkDescriptorCounts[setIndex] * sizeof(VkDescriptorUpdateTemplateEntry));
            uint32_t entryCount = 0;

            pRootSignature->pUpdateTemplateData[setIndex] =
                REI_calloc(pRootSignature->vkCumulativeDescriptorCounts[setIndex], sizeof(DescriptorUpdateData));

            // Fill the write descriptors with default values during initialize so the only thing we change in cmdBindDescriptors is the the VkBuffer / VkImageView objects
            for (uint32_t i = 0; i < (uint32_t)layout.descriptors.size(); ++i)
            {
                const REI_DescriptorInfo* pDesc = layout.descriptors[i];
                const size_t              offset = pDesc->handleIndex * sizeof(DescriptorUpdateData);

                ++entryCount;

                pEntries[i].descriptorCount = pDesc->desc.size;
                pEntries[i].descriptorType = pDesc->vkType;
                pEntries[i].dstArrayElement = 0;
                pEntries[i].dstBinding = pDesc->desc.reg;
                pEntries[i].offset = offset;
                pEntries[i].stride = sizeof(DescriptorUpdateData);

                DescriptorUpdateData* pUpdateData =
                    (DescriptorUpdateData*)pRootSignature->pUpdateTemplateData[setIndex];

                const REI_DescriptorType type = pDesc->desc.type;
                const uint32_t           arrayCount = pDesc->desc.size;

                switch (type)
                {
                    case REI_DESCRIPTOR_TYPE_SAMPLER:
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                            pUpdateData[pDesc->handleIndex + arr].mImageInfo =
                                pRenderer->pDefaultSampler->vkSamplerView;
                        break;
                    }
                    case REI_DESCRIPTOR_TYPE_TEXTURE:
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                            pUpdateData[pDesc->handleIndex + arr].mImageInfo = {
                                VK_NULL_HANDLE, pRenderer->pDefaultTextureSRV[pDesc->desc.dim]->pVkSRVDescriptor,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                            };
                        break;
                    }
                    case REI_DESCRIPTOR_TYPE_RW_TEXTURE:
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                            pUpdateData[pDesc->handleIndex + arr].mImageInfo = {
                                VK_NULL_HANDLE, pRenderer->pDefaultTextureUAV[pDesc->desc.dim]->pVkUAVDescriptors[0],
                                VK_IMAGE_LAYOUT_GENERAL
                            };
                        break;
                    }
                    case REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    case REI_DESCRIPTOR_TYPE_BUFFER:
                    case REI_DESCRIPTOR_TYPE_BUFFER_RAW:
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                            pUpdateData[pDesc->handleIndex + arr].mBufferInfo =
                                pRenderer->pDefaultBufferSRV->vkBufferInfo;
                        break;
                    }
                    case REI_DESCRIPTOR_TYPE_RW_BUFFER:
                    case REI_DESCRIPTOR_TYPE_RW_BUFFER_RAW:
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                            pUpdateData[pDesc->handleIndex + arr].mBufferInfo =
                                pRenderer->pDefaultBufferUAV->vkBufferInfo;
                        break;
                    }
                    case REI_DESCRIPTOR_TYPE_TEXEL_BUFFER:
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                            pUpdateData[pDesc->handleIndex + arr].mBuferView =
                                pRenderer->pDefaultBufferSRV->pVkUniformTexelView;
                        break;
                    }
                    case REI_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER:
                    {
                        for (uint32_t arr = 0; arr < arrayCount; ++arr)
                            pUpdateData[pDesc->handleIndex + arr].mBuferView =
                                pRenderer->pDefaultBufferUAV->pVkStorageTexelView;
                        break;
                    }
                    default: break;
                }
            }

            VkDescriptorUpdateTemplateCreateInfoKHR createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
            createInfo.pNext = NULL;
            createInfo.descriptorSetLayout = pRootSignature->vkDescriptorSetLayouts[setIndex];
            createInfo.descriptorUpdateEntryCount = entryCount;
            createInfo.pDescriptorUpdateEntries = pEntries;
            createInfo.pipelineBindPoint = gPipelineBindPoint[pRootSignature->pipelineType];
            createInfo.pipelineLayout = pRootSignature->pPipelineLayout;
            createInfo.set = setIndex;
            createInfo.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
            VkResult vkRes = pRenderer->pfn_vkCreateDescriptorUpdateTemplateKHR(
                pRenderer->pVkDevice, &createInfo, NULL, &pRootSignature->updateTemplates[setIndex]);
            REI_ASSERT(VK_SUCCESS == vkRes);
        }
        else if (VK_NULL_HANDLE != pRootSignature->vkDescriptorSetLayouts[setIndex])
        {
            // Consume empty descriptor sets from empty descriptor set pool
            VkDescriptorSet* pSets[] = { &pRootSignature->vkEmptyDescriptorSets[setIndex] };
            consume_descriptor_sets(
                pRenderer->pDescriptorPool, &pRootSignature->vkDescriptorSetLayouts[setIndex], pSets, 1);
        }
    }

    *ppRootSignature = pRootSignature;
}

void REI_removeRootSignature(REI_Renderer* pRenderer, REI_RootSignature* pRootSignature)
{
    for (uint32_t i = 0; i < REI_DESCRIPTOR_SET_COUNT; ++i)
    {
        vkDestroyDescriptorSetLayout(pRenderer->pVkDevice, pRootSignature->vkDescriptorSetLayouts[i], NULL);
        if (VK_NULL_HANDLE != pRootSignature->updateTemplates[i])
        {
            pRenderer->pfn_vkDestroyDescriptorUpdateTemplateKHR(
                pRenderer->pVkDevice, pRootSignature->updateTemplates[i], NULL);
        }

        SAFE_FREE(pRootSignature->pUpdateTemplateData[i]);
    }

    for (uint32_t i = 0; i < pRootSignature->descriptorCount; ++i)
    {
        SAFE_FREE(pRootSignature->pDescriptors[i].desc.name);
    }

    SAFE_FREE(pRootSignature->pDescriptors);
    SAFE_FREE(pRootSignature->pVkPushConstantRanges);

    vkDestroyPipelineLayout(pRenderer->pVkDevice, pRootSignature->pPipelineLayout, NULL);

    REI_delete(pRootSignature);
}

/************************************************************************/
// Pipeline State Functions
/************************************************************************/
static void
    addGraphicsPipelineImpl(REI_Renderer* pRenderer, const REI_GraphicsPipelineDesc* pDesc, REI_Pipeline** ppPipeline)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);
    REI_ASSERT(pDesc->pShaderProgram);
    REI_ASSERT(pDesc->pRootSignature);

    REI_Pipeline* pPipeline = REI_new(REI_Pipeline);
    REI_ASSERT(pPipeline);

    const REI_Shader*       pShaderProgram = pDesc->pShaderProgram;
    const REI_VertexLayout* pVertexLayout = pDesc->pVertexLayout;

    pPipeline->type = REI_PIPELINE_TYPE_GRAPHICS;

    // Create tempporary renderpass for pipeline creation
    REI_RenderPassDesc renderPassDesc = { 0 };
    VkRenderPass       renderPass = NULL;
    renderPassDesc.renderTargetCount = pDesc->renderTargetCount;
    renderPassDesc.pColorFormats = pDesc->pColorFormats;
    renderPassDesc.sampleCount = pDesc->sampleCount;
    renderPassDesc.depthStencilFormat = pDesc->depthStencilFormat;
    add_render_pass(pRenderer, &renderPassDesc, &renderPass);

    REI_ASSERT(VK_NULL_HANDLE != pRenderer->pVkDevice);
    for (uint32_t i = 0; i < pShaderProgram->reflection.mStageReflectionCount; ++i)
        REI_ASSERT(VK_NULL_HANDLE != pShaderProgram->pShaderModules[i]);

    // REI_Pipeline
    {
        uint32_t stage_count = 0;
        DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stages[5]);
        for (uint32_t i = 0; i < 5; ++i)
        {
            REI_ShaderStage stage_mask = (REI_ShaderStage)(1 << i);
            if (stage_mask == (pShaderProgram->stages & stage_mask))
            {
                stages[stage_count].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                stages[stage_count].pNext = NULL;
                stages[stage_count].flags = 0;
                stages[stage_count].pSpecializationInfo = NULL;
                switch (stage_mask)
                {
                    case REI_SHADER_STAGE_VERT:
                    {
                        stages[stage_count].pName =
                            pShaderProgram->reflection.mStageReflections[pShaderProgram->reflection.mVertexStageIndex]
                                .pEntryPoint;
                        stages[stage_count].stage = VK_SHADER_STAGE_VERTEX_BIT;
                        stages[stage_count].module =
                            pShaderProgram->pShaderModules[pShaderProgram->reflection.mVertexStageIndex];
                    }
                    break;
                    case REI_SHADER_STAGE_TESC:
                    {
                        stages[stage_count].pName =
                            pShaderProgram->reflection.mStageReflections[pShaderProgram->reflection.mHullStageIndex]
                                .pEntryPoint;
                        stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
                        stages[stage_count].module =
                            pShaderProgram->pShaderModules[pShaderProgram->reflection.mHullStageIndex];
                    }
                    break;
                    case REI_SHADER_STAGE_TESE:
                    {
                        stages[stage_count].pName =
                            pShaderProgram->reflection.mStageReflections[pShaderProgram->reflection.mDomainStageIndex]
                                .pEntryPoint;
                        stages[stage_count].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
                        stages[stage_count].module =
                            pShaderProgram->pShaderModules[pShaderProgram->reflection.mDomainStageIndex];
                    }
                    break;
                    case REI_SHADER_STAGE_GEOM:
                    {
                        stages[stage_count].pName =
                            pShaderProgram->reflection.mStageReflections[pShaderProgram->reflection.mGeometryStageIndex]
                                .pEntryPoint;
                        stages[stage_count].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
                        stages[stage_count].module =
                            pShaderProgram->pShaderModules[pShaderProgram->reflection.mGeometryStageIndex];
                    }
                    break;
                    case REI_SHADER_STAGE_FRAG:
                    {
                        stages[stage_count].pName =
                            pShaderProgram->reflection.mStageReflections[pShaderProgram->reflection.mPixelStageIndex]
                                .pEntryPoint;
                        stages[stage_count].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                        stages[stage_count].module =
                            pShaderProgram->pShaderModules[pShaderProgram->reflection.mPixelStageIndex];
                    }
                    break;
                    default: REI_ASSERT(false && "REI_Shader Stage not supported!"); break;
                }
                ++stage_count;
            }
        }

        // Make sure there's a shader
        REI_ASSERT(0 != stage_count);

        uint32_t                          input_binding_count = 0;
        VkVertexInputBindingDescription   input_bindings[REI_MAX_VERTEX_BINDINGS] = { { 0 } };
        uint32_t                          input_attribute_count = 0;
        VkVertexInputAttributeDescription input_attributes[REI_MAX_VERTEX_ATTRIBS] = { { 0 } };

        // Make sure there's attributes
        if (pVertexLayout != NULL)
        {
            // Ignore everything that's beyond max_vertex_attribs
            uint32_t attrib_count = pVertexLayout->attribCount > REI_MAX_VERTEX_ATTRIBS ? REI_MAX_VERTEX_ATTRIBS
                                                                                        : pVertexLayout->attribCount;
            uint32_t binding_value = UINT32_MAX;

            // Initial values
            for (uint32_t i = 0; i < attrib_count; ++i)
            {
                const REI_VertexAttrib* attrib = &(pVertexLayout->attribs[i]);

                if (binding_value != attrib->binding)
                {
                    binding_value = attrib->binding;
                    ++input_binding_count;
                }

                input_bindings[input_binding_count - 1].binding = binding_value;
                if (attrib->rate == REI_VERTEX_ATTRIB_RATE_INSTANCE)
                {
                    input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
                }
                else
                {
                    input_bindings[input_binding_count - 1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                }
                input_bindings[input_binding_count - 1].stride += util_bit_size_of_block(attrib->format) / 8;

                input_attributes[input_attribute_count].location = attrib->location;
                input_attributes[input_attribute_count].binding = attrib->binding;
                input_attributes[input_attribute_count].format = util_to_vk_format(attrib->format);
                input_attributes[input_attribute_count].offset = attrib->offset;
                ++input_attribute_count;
            }
        }

        DECLARE_ZERO(VkPipelineVertexInputStateCreateInfo, vi);
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
        DECLARE_ZERO(VkPipelineInputAssemblyStateCreateInfo, ia);
        ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.pNext = NULL;
        ia.flags = 0;
        ia.topology = topology;
        ia.primitiveRestartEnable = VK_FALSE;

        DECLARE_ZERO(VkPipelineTessellationStateCreateInfo, ts);
        if ((pShaderProgram->stages & REI_SHADER_STAGE_TESC) && (pShaderProgram->stages & REI_SHADER_STAGE_TESE))
        {
            ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
            ts.pNext = NULL;
            ts.flags = 0;
            ts.patchControlPoints =
                pShaderProgram->reflection.mStageReflections[pShaderProgram->reflection.mHullStageIndex]
                    .numControlPoint;
        }

        DECLARE_ZERO(VkPipelineViewportStateCreateInfo, vs);
        vs.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vs.pNext = NULL;
        vs.flags = 0;
        // we are using dynimic viewports but we must set the count to 1
        vs.viewportCount = 1;
        vs.pViewports = NULL;
        vs.scissorCount = 1;
        vs.pScissors = NULL;

        DECLARE_ZERO(VkPipelineRasterizationStateCreateInfo, rs);
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

        DECLARE_ZERO(VkPipelineMultisampleStateCreateInfo, ms);
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
        DECLARE_ZERO(VkPipelineDepthStencilStateCreateInfo, ds);
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

        VkPipelineColorBlendAttachmentState RTBlendStates[REI_MAX_RENDER_TARGET_ATTACHMENTS]{};
        DECLARE_ZERO(VkPipelineColorBlendStateCreateInfo, cb);
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

        DECLARE_ZERO(VkDynamicState, dyn_states[6]);
        dyn_states[0] = VK_DYNAMIC_STATE_VIEWPORT;
        dyn_states[1] = VK_DYNAMIC_STATE_SCISSOR;
        dyn_states[2] = VK_DYNAMIC_STATE_DEPTH_BIAS;
        dyn_states[3] = VK_DYNAMIC_STATE_BLEND_CONSTANTS;
        dyn_states[4] = VK_DYNAMIC_STATE_DEPTH_BOUNDS;
        dyn_states[5] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
        DECLARE_ZERO(VkPipelineDynamicStateCreateInfo, dy);
        dy.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dy.pNext = NULL;
        dy.flags = 0;
        dy.dynamicStateCount = 6;
        dy.pDynamicStates = dyn_states;

        DECLARE_ZERO(VkGraphicsPipelineCreateInfo, add_info);
        add_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        add_info.pNext = NULL;
        add_info.flags = 0;
        add_info.stageCount = stage_count;
        add_info.pStages = stages;
        add_info.pVertexInputState = &vi;
        add_info.pInputAssemblyState = &ia;

        if ((pShaderProgram->stages & REI_SHADER_STAGE_TESC) && (pShaderProgram->stages & REI_SHADER_STAGE_TESE))
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
        VkResult vk_res = vkCreateGraphicsPipelines(
            pRenderer->pVkDevice, VK_NULL_HANDLE, 1, &add_info, NULL, &(pPipeline->pVkPipeline));
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
    REI_ASSERT(pDesc->pShaderProgram->pShaderModules[0] != VK_NULL_HANDLE);

    REI_Pipeline* pPipeline = REI_new(REI_Pipeline);
    REI_ASSERT(pPipeline);

    pPipeline->type = REI_PIPELINE_TYPE_COMPUTE;

    // REI_Pipeline
    {
        DECLARE_ZERO(VkPipelineShaderStageCreateInfo, stage);
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.pNext = NULL;
        stage.flags = 0;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = pDesc->pShaderProgram->pShaderModules[0];
        stage.pName = pDesc->pShaderProgram->reflection.mStageReflections[0].pEntryPoint;
        stage.pSpecializationInfo = NULL;

        DECLARE_ZERO(VkComputePipelineCreateInfo, create_info);
        create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        create_info.pNext = NULL;
        create_info.flags = 0;
        create_info.stage = stage;
        create_info.layout = pDesc->pRootSignature->pPipelineLayout;
        create_info.basePipelineHandle = 0;
        create_info.basePipelineIndex = 0;
        VkResult vk_res = vkCreateComputePipelines(
            pRenderer->pVkDevice, VK_NULL_HANDLE, 1, &create_info, NULL, &(pPipeline->pVkPipeline));
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
    REI_delete(pPipeline);
}

/************************************************************************/
// Command buffer functions
/************************************************************************/
void REI_beginCmd(REI_Cmd* pCmd)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    // reset buffer to REI_free memory
    vkResetCommandBuffer(pCmd->pVkCmdBuf, 0);

    DECLARE_ZERO(VkCommandBufferBeginInfo, begin_info);
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    begin_info.pInheritanceInfo = NULL;

    VkResult vk_res = vkBeginCommandBuffer(pCmd->pVkCmdBuf, &begin_info);
    REI_ASSERT(VK_SUCCESS == vk_res);

    // Reset CPU side data
    pCmd->pBoundRootSignature = NULL;
}

void REI_endCmd(REI_Cmd* pCmd)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    if (pCmd->pVkActiveRenderPass)
    {
        vkCmdEndRenderPass(pCmd->pVkCmdBuf);
    }

    pCmd->pVkActiveRenderPass = VK_NULL_HANDLE;

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

    if (pCmd->pVkActiveRenderPass)
    {
        vkCmdEndRenderPass(pCmd->pVkCmdBuf);
        pCmd->pVkActiveRenderPass = VK_NULL_HANDLE;
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

    REI_SampleCount sampleCount =
        renderTargetCount ? ppRenderTargets[0]->desc.sampleCount : pDepthStencil->desc.sampleCount;

    RenderPassMap&  renderPassMap = get_render_pass_map(pCmd->pRenderer);
    FrameBufferMap& frameBufferMap = get_frame_buffer_map(pCmd->pRenderer);

    const RenderPassMap::iterator  pNode = renderPassMap.find(renderPassHash);
    const FrameBufferMap::iterator pFrameBufferNode = frameBufferMap.find(frameBufferHash);

    VkRenderPass     renderPass = VK_NULL_HANDLE;
    REI_FrameBuffer* pFrameBuffer = NULL;

    // If a render pass of this combination already exists just use it or create a new one
    if (pNode != renderPassMap.end())
    {
        renderPass = pNode->second;
    }
    else
    {
        REI_Format colorFormats[REI_MAX_RENDER_TARGET_ATTACHMENTS] = {};
        REI_Format depthStencilFormat = REI_FMT_UNDEFINED;
        for (uint32_t i = 0; i < renderTargetCount; ++i)
        {
            colorFormats[i] = ppRenderTargets[i]->desc.format;
        }
        if (pDepthStencil)
        {
            depthStencilFormat = pDepthStencil->desc.format;
        }

        REI_RenderPassDesc renderPassDesc = {};
        renderPassDesc.renderTargetCount = renderTargetCount;
        renderPassDesc.sampleCount = sampleCount;
        renderPassDesc.pColorFormats = colorFormats;
        renderPassDesc.depthStencilFormat = depthStencilFormat;
        renderPassDesc.pLoadActionsColor = pLoadActions ? pLoadActions->loadActionsColor : NULL;
        renderPassDesc.loadActionDepth = pLoadActions ? pLoadActions->loadActionDepth : REI_LOAD_ACTION_DONTCARE;
        renderPassDesc.loadActionStencil = pLoadActions ? pLoadActions->loadActionStencil : REI_LOAD_ACTION_DONTCARE;
        add_render_pass(pCmd->pRenderer, &renderPassDesc, &renderPass);

        // No need of a lock here since this map is per thread
        renderPassMap.insert({ { renderPassHash, renderPass } });
    }

    // If a frame buffer of this combination already exists just use it or create a new one
    if (pFrameBufferNode != frameBufferMap.end())
    {
        pFrameBuffer = pFrameBufferNode->second;
    }
    else
    {
        FrameBufferDesc desc = { 0 };
        desc.renderTargetCount = renderTargetCount;
        desc.pDepthStencil = pDepthStencil;
        desc.ppRenderTargets = ppRenderTargets;
        desc.renderPass = renderPass;
        desc.pColorArraySlices = pColorArraySlices;
        desc.pColorMipSlices = pColorMipSlices;
        desc.depthArraySlice = depthArraySlice;
        desc.depthMipSlice = depthMipSlice;
        add_framebuffer(pCmd->pRenderer, &desc, &pFrameBuffer);

        // No need of a lock here since this map is per thread
        frameBufferMap.insert({ { frameBufferHash, pFrameBuffer } });
    }

    DECLARE_ZERO(VkRect2D, render_area);
    render_area.offset.x = 0;
    render_area.offset.y = 0;
    render_area.extent.width = pFrameBuffer->width;
    render_area.extent.height = pFrameBuffer->height;

    uint32_t     clearValueCount = renderTargetCount;
    VkClearValue clearValues[REI_MAX_RENDER_TARGET_ATTACHMENTS + 1] = {};
    if (pLoadActions)
    {
        for (uint32_t i = 0; i < renderTargetCount; ++i)
        {
            REI_ClearValue clearValue = pLoadActions->clearColorValues[i];
            clearValues[i].color = { { clearValue.rt.r, clearValue.rt.g, clearValue.rt.b, clearValue.rt.a } };
        }
        if (pDepthStencil)
        {
            clearValues[renderTargetCount].depthStencil = { pLoadActions->clearDepth.ds.depth,
                                                            pLoadActions->clearDepth.ds.stencil };
            ++clearValueCount;
        }
    }

    DECLARE_ZERO(VkRenderPassBeginInfo, begin_info);
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.renderPass = renderPass;
    begin_info.framebuffer = pFrameBuffer->pFramebuffer;
    begin_info.renderArea = render_area;
    begin_info.clearValueCount = clearValueCount;
    begin_info.pClearValues = clearValues;

    vkCmdBeginRenderPass(pCmd->pVkCmdBuf, &begin_info, VK_SUBPASS_CONTENTS_INLINE);
    pCmd->pVkActiveRenderPass = renderPass;
}

void REI_cmdSetViewport(REI_Cmd* pCmd, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    DECLARE_ZERO(VkViewport, viewport);
    viewport.x = x;
    viewport.y = y + height;
    viewport.width = width;
    viewport.height = -height;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
    vkCmdSetViewport(pCmd->pVkCmdBuf, 0, 1, &viewport);
}

void REI_cmdSetScissor(REI_Cmd* pCmd, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    DECLARE_ZERO(VkRect2D, rect);
    rect.offset.x = x;
    rect.offset.y = y;
    rect.extent.width = width;
    rect.extent.height = height;
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

    VkPipelineBindPoint pipeline_bind_point = gPipelineBindPoint[pPipeline->type];
    vkCmdBindPipeline(pCmd->pVkCmdBuf, pipeline_bind_point, pPipeline->pVkPipeline);
}

void REI_cmdBindIndexBuffer(REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t offset)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pBuffer);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    VkIndexType vk_index_type =
        (REI_INDEX_TYPE_UINT16 == pBuffer->desc.indexType) ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
    vkCmdBindIndexBuffer(pCmd->pVkCmdBuf, pBuffer->pVkBuffer, pBuffer->positionInHeap + offset, vk_index_type);
}

void REI_cmdBindVertexBuffer(REI_Cmd* pCmd, uint32_t bufferCount, REI_Buffer** ppBuffers, uint64_t* pOffsets)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(0 != bufferCount);
    REI_ASSERT(ppBuffers);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    const uint32_t max_buffers = pCmd->pRenderer->vkDeviceProperties.properties.limits.maxVertexInputBindings;
    uint32_t       capped_buffer_count = bufferCount > max_buffers ? max_buffers : bufferCount;

    // No upper bound for this, so use 64 for now
    REI_ASSERT(capped_buffer_count < 64);

    DECLARE_ZERO(VkBuffer, buffers[64]);
    DECLARE_ZERO(VkDeviceSize, offsets[64]);

    for (uint32_t i = 0; i < capped_buffer_count; ++i)
    {
        buffers[i] = ppBuffers[i]->pVkBuffer;
        offsets[i] = (ppBuffers[i]->positionInHeap + (pOffsets ? pOffsets[i] : 0));
    }

    vkCmdBindVertexBuffers(pCmd->pVkCmdBuf, 0, capped_buffer_count, buffers, offsets);
}

void REI_cmdDraw(REI_Cmd* pCmd, uint32_t vertex_count, uint32_t first_vertex)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);
    REI_ASSERT(vertex_count);

    vkCmdDraw(pCmd->pVkCmdBuf, vertex_count, 1, first_vertex, 0);
}

void REI_cmdDrawInstanced(
    REI_Cmd* pCmd, uint32_t vertexCount, uint32_t firstVertex, uint32_t instanceCount, uint32_t firstInstance)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    vkCmdDraw(pCmd->pVkCmdBuf, vertexCount, instanceCount, firstVertex, firstInstance);
}

void REI_cmdDrawIndexed(REI_Cmd* pCmd, uint32_t index_count, uint32_t first_index, uint32_t first_vertex)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

    vkCmdDrawIndexed(pCmd->pVkCmdBuf, index_count, 1, first_index, first_vertex, 0);
}

void REI_cmdDrawIndexedInstanced(
    REI_Cmd* pCmd, uint32_t indexCount, uint32_t firstIndex, uint32_t instanceCount, uint32_t firstInstance,
    uint32_t firstVertex)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(VK_NULL_HANDLE != pCmd->pVkCmdBuf);

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
    VkImageMemoryBarrier* imageBarriers =
        numTextureBarriers ? (VkImageMemoryBarrier*)alloca(numTextureBarriers * sizeof(VkImageMemoryBarrier)) : NULL;
    uint32_t imageBarrierCount = 0;

    VkBufferMemoryBarrier* bufferBarriers =
        numBufferBarriers ? (VkBufferMemoryBarrier*)alloca(numBufferBarriers * sizeof(VkBufferMemoryBarrier)) : NULL;
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

    DECLARE_ZERO(VkBufferCopy, region);
    region.srcOffset = pSrcBuffer->positionInHeap + srcOffset;
    region.dstOffset = pBuffer->positionInHeap + dstOffset;
    region.size = (VkDeviceSize)size;
    vkCmdCopyBuffer(pCmd->pVkCmdBuf, pSrcBuffer->pVkBuffer, pBuffer->pVkBuffer, 1, &region);
}

void REI_cmdCopyBufferToTexture(
    REI_Cmd* pCmd, REI_Texture* pTexture, REI_Buffer* pSrcBuffer, REI_SubresourceDesc* pSubresourceDesc)
{
    VkBufferImageCopy copyData;
    copyData.bufferOffset = pSrcBuffer->positionInHeap + pSubresourceDesc->bufferOffset;
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
    copyData.bufferOffset = pDstBuffer->positionInHeap + pSubresourceDesc->bufferOffset;
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
// Utility functions
/************************************************************************/
REI_Format REI_getRecommendedSwapchainFormat(bool hintHDR)
{
    //TODO: figure out this properly. BGRA not supported on android
#ifndef VK_USE_PLATFORM_ANDROID_KHR
    return REI_FMT_B8G8R8A8_UNORM;
#else
    return REI_FMT_R8G8B8A8_UNORM;
#endif
}

/************************************************************************/
// Indirect draw functions
/************************************************************************/
void REI_addIndirectCommandSignature(
    REI_Renderer* pRenderer, const REI_CommandSignatureDesc* pDesc, REI_CommandSignature** ppCommandSignature)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pDesc);

    REI_CommandSignature* pCommandSignature = (REI_CommandSignature*)REI_calloc(1, sizeof(REI_CommandSignature));

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
                REI_LOG(
                    ERROR,
                    "Vulkan runtime only supports IndirectDraw, IndirectDrawIndex and IndirectDispatch at this point");
                break;
        }
    }

    pCommandSignature->drawCommandStride = REI_align_up(pCommandSignature->drawCommandStride, 16u);

    *ppCommandSignature = pCommandSignature;
}

void REI_removeIndirectCommandSignature(REI_Renderer* pRenderer, REI_CommandSignature* pCommandSignature)
{
    SAFE_FREE(pCommandSignature);
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
    REI_QueryPool* pQueryPool = (REI_QueryPool*)REI_calloc(1, sizeof(*pQueryPool));
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
    SAFE_FREE(pQueryPool);
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
        case REI_QUERY_TYPE_TIMESTAMP: REI_ASSERT(false); break;
        case REI_QUERY_TYPE_PIPELINE_STATISTICS: break;
        case REI_QUERY_TYPE_OCCLUSION: break;
        default: break;
    }
}

void REI_cmdEndQuery(REI_Cmd* pCmd, REI_QueryPool* pQueryPool, uint32_t index)
{
    REI_QueryType type = pQueryPool->desc.type;
    switch (type)
    {
        case REI_QUERY_TYPE_TIMESTAMP:
            vkCmdWriteTimestamp(pCmd->pVkCmdBuf, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pQueryPool->pVkQueryPool, index);
            break;
        case REI_QUERY_TYPE_PIPELINE_STATISTICS: break;
        case REI_QUERY_TYPE_OCCLUSION: break;
        default: break;
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
// Memory Stats Implementation
/************************************************************************/
void REI_calculateMemoryStats(REI_Renderer* pRenderer, char** stats)
{
    vmaBuildStatsString(pRenderer->pVmaAllocator, stats, 0);
}

void REI_freeMemoryStats(REI_Renderer* pRenderer, char* stats) { vmaFreeStatsString(pRenderer->pVmaAllocator, stats); }
/************************************************************************/
// Debug Marker Implementation
/************************************************************************/
void REI_cmdBeginDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName)
{
    if (pCmd->pRenderer->hasDebugMarkerExtension)
    {
#ifdef USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsLabelEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pLabelName = pName;
        vkCmdBeginDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
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
#ifdef USE_DEBUG_UTILS_EXTENSION
        vkCmdEndDebugUtilsLabelEXT(pCmd->pVkCmdBuf);
#else
        pCmd->pRenderer->pfn_vkCmdDebugMarkerEndEXT(pCmd->pVkCmdBuf);
#endif
    }
}

void REI_cmdAddDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName)
{
    if (pCmd->pRenderer->hasDebugMarkerExtension)
    {
#ifdef USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsLabelEXT markerInfo = {};
        markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        markerInfo.color[0] = r;
        markerInfo.color[1] = g;
        markerInfo.color[2] = b;
        markerInfo.color[3] = 1.0f;
        markerInfo.pLabelName = pName;
        vkCmdInsertDebugUtilsLabelEXT(pCmd->pVkCmdBuf, &markerInfo);
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
#ifdef USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_BUFFER;
        nameInfo.objectHandle = (uint64_t)pBuffer->pVkBuffer;
        nameInfo.pObjectName = pName;
        vkSetDebugUtilsObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
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
#ifdef USE_DEBUG_UTILS_EXTENSION
        VkDebugUtilsObjectNameInfoEXT nameInfo = {};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = VK_OBJECT_TYPE_IMAGE;
        nameInfo.objectHandle = (uint64_t)pTexture->pVkImage;
        nameInfo.pObjectName = pName;
        vkSetDebugUtilsObjectNameEXT(pRenderer->pVkDevice, &nameInfo);
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

static REI_DescriptorType to_descriptor_type(SpvReflectDescriptorType type)
{
    switch (type)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return REI_DESCRIPTOR_TYPE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return REI_DESCRIPTOR_TYPE_TEXTURE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return REI_DESCRIPTOR_TYPE_RW_TEXTURE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return REI_DESCRIPTOR_TYPE_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return REI_DESCRIPTOR_TYPE_RW_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return REI_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return REI_DESCRIPTOR_TYPE_RW_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return REI_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        //case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        default: return REI_DESCRIPTOR_TYPE_UNDEFINED;
    }
}

static uint32_t calc_array_size(SpvReflectBindingArrayTraits& array)
{
    uint32_t size = 1;
    for (uint32_t i = 0; i < array.dims_count; ++i)
        size *= array.dims[i];
    return size;
}

static REI_TextureDimension to_resource_dim(SpvReflectImageTraits& image)
{
    switch (image.dim)
    {
        case SpvDim1D: return image.arrayed ? REI_TEXTURE_DIM_1D_ARRAY : REI_TEXTURE_DIM_1D;
        case SpvDim2D:
            if (image.ms)
                return image.arrayed ? REI_TEXTURE_DIM_2DMS_ARRAY : REI_TEXTURE_DIM_2DMS;
            else
                return image.arrayed ? REI_TEXTURE_DIM_2D_ARRAY : REI_TEXTURE_DIM_2D;
        case SpvDim3D: return REI_TEXTURE_DIM_3D;
        case SpvDimCube: return image.arrayed ? REI_TEXTURE_DIM_CUBE_ARRAY : REI_TEXTURE_DIM_CUBE;
        default: return REI_TEXTURE_DIM_UNDEFINED;
    }
}

static uint32_t calc_var_size(SpvReflectInterfaceVariable& var)
{
    switch (var.type_description->op)
    {
        case SpvOpTypeBool:
        {
            return 4;
        }
        break;

        case SpvOpTypeInt:
        case SpvOpTypeFloat:
        {
            return var.numeric.scalar.width / 8;
        }
        break;

        case SpvOpTypeVector:
        {
            return var.numeric.vector.component_count * (var.numeric.scalar.width / 8);
        }
        break;

        case SpvOpTypeMatrix:
        {
            if (var.decoration_flags & SPV_REFLECT_DECORATION_COLUMN_MAJOR)
            {
                return var.numeric.matrix.column_count * var.numeric.matrix.stride;
            }
            else if (var.decoration_flags & SPV_REFLECT_DECORATION_ROW_MAJOR)
            {
                return var.numeric.matrix.row_count * var.numeric.matrix.stride;
            }
        }
        break;

        case SpvOpTypeArray:
        {
            // If array of structs, parse members first...
            bool is_struct =
                (var.type_description->type_flags & SPV_REFLECT_TYPE_FLAG_STRUCT) == SPV_REFLECT_TYPE_FLAG_STRUCT;
            if (is_struct)
            {
                assert(false);
                break;
            }
            // ...then array
            uint32_t element_count = (var.array.dims_count > 0 ? 1 : 0);
            for (uint32_t i = 0; i < var.array.dims_count; ++i)
            {
                element_count *= var.array.dims[i];
            }
            return element_count * var.array.stride;
        }
        break;
        default: assert(false);
    }
    return 0;
}

static void vk_createShaderReflection(
    const uint8_t* shaderCode, uint32_t shaderSize, REI_ShaderStage shaderStage, REI_ShaderReflection* pOutReflection)
{
    if (pOutReflection == NULL)
    {
        REI_LOG(ERROR, "Create Shader Refection failed. Invalid reflection output!");
        return;    // TODO: error msg
    }

    SpvReflectShaderModule module;

    if (spvReflectCreateShaderModule(shaderSize, shaderCode, &module) != SPV_REFLECT_RESULT_SUCCESS)
    {
        REI_LOG(ERROR, "Create Shader Refection failed. Invalid input!");
        return;    // TODO: error msg
    }

    SpvReflectEntryPoint& entryPoint = module.entry_points[0];

    if (entryPoint.shader_stage == SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
    {
        pOutReflection->numThreadsPerGroup[0] = entryPoint.local_size.x;
        pOutReflection->numThreadsPerGroup[1] = entryPoint.local_size.y;
        pOutReflection->numThreadsPerGroup[2] = entryPoint.local_size.z;
    }
    else if (entryPoint.shader_stage == SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
    {
        pOutReflection->numControlPoint = entryPoint.output_vertices;
    }

    // lets find out the size of the name pool we need
    // also get number of resources while we are at it
    uint32_t resouceCount = 0;
    size_t   entryPointSize = strlen(entryPoint.name);
    size_t   namePoolSize = 0;
    namePoolSize += entryPointSize + 1;

    // vertex inputs
    if (entryPoint.shader_stage == SPV_REFLECT_SHADER_STAGE_VERTEX_BIT)
    {
        for (uint32_t i = 0; i < entryPoint.input_variable_count; ++i)
        {
            SpvReflectInterfaceVariable& v = entryPoint.input_variables[i];

            namePoolSize += strlen(v.name) + 1;
        }
    }

    // descriptors
    for (uint32_t j = 0; j < entryPoint.descriptor_set_count; ++j)
    {
        SpvReflectDescriptorSet& ds = entryPoint.descriptor_sets[j];
        resouceCount += ds.binding_count;
        for (uint32_t i = 0; i < ds.binding_count; ++i)
        {
            SpvReflectDescriptorBinding* b = ds.bindings[i];

            // filter out what we don't use
            namePoolSize += strlen(b->name) + 1;
        }
    }

    // push constants
    for (uint32_t i = 0; i < entryPoint.used_push_constant_count; ++i)
    {
        uint32_t id = entryPoint.used_push_constants[i];
        for (uint32_t j = 0; j < module.push_constant_block_count; ++j)
        {
            SpvReflectBlockVariable& b = module.push_constant_blocks[j];
            if (b.spirv_id == id)
            {
                namePoolSize += strlen(b.name) + 1;
                ++resouceCount;
            }
        }
    }

    // we now have the size of the memory pool and number of resources
    char* namePool = NULL;
    if (namePoolSize)
        namePool = (char*)REI_calloc(namePoolSize, 1);
    char* pCurrentName = namePool;

    pOutReflection->pEntryPoint = pCurrentName;
    memcpy(pCurrentName, entryPoint.name, entryPointSize);
    pCurrentName += entryPointSize + 1;

    REI_VertexInput* pVertexInputs = NULL;
    // start with the vertex input
    if (shaderStage == REI_SHADER_STAGE_VERT && entryPoint.input_variable_count > 0)
    {
        pVertexInputs = (REI_VertexInput*)REI_malloc(sizeof(REI_VertexInput) * entryPoint.input_variable_count);

        for (uint32_t i = 0; i < entryPoint.input_variable_count; ++i)
        {
            SpvReflectInterfaceVariable& v = entryPoint.input_variables[i];

            pVertexInputs[i].size = calc_var_size(v);
            pVertexInputs[i].name = pCurrentName;
            size_t name_len = strlen(v.name);
            pVertexInputs[i].name_size = (uint32_t)name_len;
            // we dont own the names memory we need to copy it to the name pool
            memcpy(pCurrentName, v.name, name_len);
            pCurrentName += name_len + 1;
        }
    }

    REI_ShaderResource* pResources = NULL;
    // continue with resources
    if (resouceCount)
    {
        pResources = (REI_ShaderResource*)REI_malloc(sizeof(REI_ShaderResource) * resouceCount);

        uint32_t r = 0;
        for (uint32_t i = 0; i < entryPoint.descriptor_set_count; ++i)
        {
            SpvReflectDescriptorSet& ds = entryPoint.descriptor_sets[i];
            for (uint32_t j = 0; j < ds.binding_count; ++j)
            {
                SpvReflectDescriptorBinding* b = ds.bindings[j];
                pResources[r].type = to_descriptor_type(b->descriptor_type);
                pResources[r].set = ds.set;
                pResources[r].reg = b->binding;
                pResources[r].size = calc_array_size(b->array);    //resource->size;
                pResources[r].used_stages = shaderStage;

                pResources[r].name = pCurrentName;
                size_t name_len = strlen(b->name);
                pResources[r].name_size = (uint32_t)name_len;
                pResources[r].dim = to_resource_dim(b->image);
                // we dont own the names memory we need to copy it to the name pool
                memcpy(pCurrentName, b->name, name_len);
                pCurrentName += name_len + 1;
                ++r;
            }
        }

        for (uint32_t i = 0; i < entryPoint.used_push_constant_count; ++i)
        {
            uint32_t id = entryPoint.used_push_constants[i];
            for (uint32_t j = 0; j < module.push_constant_block_count; ++j)
            {
                SpvReflectBlockVariable& b = module.push_constant_blocks[j];
                if (b.spirv_id == id)
                {
                    pResources[r].type = REI_DESCRIPTOR_TYPE_ROOT_CONSTANT;
                    pResources[r].set = UINT32_MAX;
                    pResources[r].reg = b.offset;
                    pResources[r].size = b.size;
                    pResources[r].used_stages = shaderStage;

                    pResources[r].name = pCurrentName;
                    size_t name_len = strlen(b.name);
                    pResources[r].name_size = (uint32_t)name_len;
                    pResources[r].dim = REI_TEXTURE_DIM_UNDEFINED;
                    // we dont own the names memory we need to copy it to the name pool
                    memcpy(pCurrentName, b.name, name_len);
                    pCurrentName += name_len + 1;
                    ++r;
                }
            }
        }

        assert(r == resouceCount);
    }

    // all refection structs should be built now
    pOutReflection->shaderStage = shaderStage;

    pOutReflection->pNamePool = namePool;
    pOutReflection->namePoolSize = (uint32_t)namePoolSize;

    pOutReflection->pVertexInputs = pVertexInputs;
    pOutReflection->vertexInputsCount = entryPoint.input_variable_count;

    pOutReflection->pShaderResources = pResources;
    pOutReflection->shaderResourceCount = resouceCount;

    spvReflectDestroyShaderModule(&module);
}
