/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
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

#include "REI/RendererVk.h"

static const char* gVkWantedInstanceLayers[] =
#if defined(_DEBUG) || USE_RENDER_DOC
    {
#    if defined(_DEBUG)
        // this turns on all validation layers
        // NOTE: disable VK_LAYER_KHRONOS_validation for now as it crashes on win x86 with Vulkan SDK 1.2.135.0
        "VK_LAYER_KHRONOS_validation",
        "VK_LAYER_LUNARG_standard_validation",
#    endif
// this turns on render doc layer for gpu capture
#    if USE_RENDER_DOC
        "VK_LAYER_RENDERDOC_Capture",
#    endif
    };
#else
    { nullptr };
#endif

const char* gVkWantedInstanceExtensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
// Debug utils not supported on all devices yet
#if USE_DEBUG_UTILS_EXTENSION
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#else
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
    VK_NV_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    /************************************************************************/
    // VR Extensions
    /************************************************************************/
    VK_KHR_DISPLAY_EXTENSION_NAME, VK_EXT_DIRECT_MODE_DISPLAY_EXTENSION_NAME,
    /************************************************************************/
    // Property querying extensions
    /************************************************************************/
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    /************************************************************************/
    /************************************************************************/
};

const char* gVkWantedDeviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME, VK_KHR_MAINTENANCE3_EXTENSION_NAME,
    VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME, VK_EXT_SHADER_SUBGROUP_BALLOT_EXTENSION_NAME,
    VK_EXT_SHADER_SUBGROUP_VOTE_EXTENSION_NAME, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
    VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME, VK_KHR_EXTERNAL_FENCE_CAPABILITIES_EXTENSION_NAME,
    VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,

    VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME,
    VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME,

// Debug marker extension in case debug utils is not supported
#if !USE_DEBUG_UTILS_EXTENSION
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

void vk_platfom_get_wanted_instance_layers(uint32_t* outLayerCount, const char*** outLayerNames)
{
    if (gVkWantedInstanceLayers[0])
    {
        *outLayerCount = sizeof(gVkWantedInstanceLayers) / sizeof(gVkWantedInstanceLayers[0]);
        *outLayerNames = gVkWantedInstanceLayers;
    }
    else
    {
        *outLayerCount = 0;
        *outLayerNames = nullptr;
    }
}

void vk_platfom_get_wanted_instance_extensions(uint32_t* outExtensionCount, const char*** outExtensionNames)
{
    *outExtensionCount = sizeof(gVkWantedInstanceExtensions) / sizeof(gVkWantedInstanceExtensions[0]);
    *outExtensionNames = gVkWantedInstanceExtensions;
}

void vk_platfom_get_wanted_device_extensions(uint32_t* outExtensionCount, const char*** outExtensionNames)
{
    *outExtensionCount = sizeof(gVkWantedDeviceExtensions) / sizeof(gVkWantedDeviceExtensions[0]);
    *outExtensionNames = gVkWantedDeviceExtensions;
}

VkResult vk_platform_create_surface(
    VkInstance instance, const REI_WindowHandle* pNativeWindow, const VkAllocationCallbacks* pAllocator,
    VkSurfaceKHR* pSurface)
{
    VkWin32SurfaceCreateInfoKHR add_info = {};
    add_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    add_info.pNext = NULL;
    add_info.flags = 0;
    add_info.hinstance = ::GetModuleHandle(NULL);
    add_info.hwnd = (HWND)pNativeWindow->window;
    return vkCreateWin32SurfaceKHR(instance, &add_info, pAllocator, pSurface);
}

REI_Format REI_getRecommendedSwapchainFormat(bool hintHDR) { return REI_FMT_B8G8R8A8_UNORM; }