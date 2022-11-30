/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
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

#include "REI/Thread.h"
#include "REI/RendererD3D12.h"

#define USE_DRED

#if defined(__cplusplus)
#    define DECLARE_ZERO(type, var) type var = {};
#else
#    define DECLARE_ZERO(type, var) type var = { 0 };
#endif

#define SAFE_RELEASE(ptr)     \
    do                        \
    {                         \
        if (ptr)              \
        {                     \
            (ptr)->Release(); \
            (ptr) = NULL;     \
        }                     \
    } while (false)

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

typedef struct REI_Renderer_Windows: public REI_Renderer
{
    ID3D12InfoQueue* pDxDebugValidation;
} REI_Renderer_Windows;

typedef struct REI_Swapchain_Windows: public REI_Swapchain
{
    /// Use IDXGISwapChain3 for now since IDXGISwapChain4
    /// isn't supported by older devices.
    IDXGISwapChain3* pDxSwapChain;
} REI_Swapchain_Windows;

const D3D12_COMMAND_QUEUE_PRIORITY gDx12WindowsQueuePriorityTranslator[REI_MAX_QUEUE_PRIORITY]{
    D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
    D3D12_COMMAND_QUEUE_PRIORITY_HIGH,
    D3D12_COMMAND_QUEUE_PRIORITY_GLOBAL_REALTIME,
};

//Forward declarations of functions defined in RendererD3D12.cpp and used here
DXGI_FORMAT REI_Format_ToDXGI_FORMAT(uint32_t fmt);
void        util_to_GpuSettings(const REI_GpuDesc* pGpuDesc, REI_DeviceProperties* pOutDeviceProperties);
void        util_fill_gpu_desc(ID3D12Device* pDxDevice, D3D_FEATURE_LEVEL featureLevel, REI_GpuDesc* pInOutDesc);

uint32_t d3d12_platform_get_texture_row_alignment() { return D3D12_TEXTURE_DATA_PITCH_ALIGNMENT; }

HMODULE d3d12_platform_get_d3d12_module_handle() { return GetModuleHandle(TEXT("d3d12.dll")); }

DXGI_FORMAT d3d12_platform_other_rei_formats_to_dxgi(uint32_t fmt) { return DXGI_FORMAT_UNKNOWN; }

DXGI_FORMAT d3d12_platform_other_dxgi_formats_to_dxgi_typeless(DXGI_FORMAT fmt) { return DXGI_FORMAT_UNKNOWN; }

void d3d12_platform_create_copy_command_list(
    REI_Renderer* pRenderer, REI_CmdPool* pCmdPool, ID3D12CommandList** ppDxCmdList)
{
    ID3D12GraphicsCommandList* pTempCmd;
    CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_COPY, pCmdPool->pDxCmdAlloc, nullptr, IID_PPV_ARGS(&pTempCmd)));
    *ppDxCmdList = pTempCmd;
}
void d3d12_platform_create_copy_command_allocator(REI_Renderer* pRenderer, ID3D12CommandAllocator** ppDxCmdAlloc)
{
    CHECK_HRESULT(
        pRenderer->pDxDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(ppDxCmdAlloc)));
}

void d3d12_platform_create_copy_command_queue(
    REI_Renderer* pRenderer, REI_QueueDesc* pQDesc, ID3D12CommandQueue** ppDxCmdQueue)
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    if (pQDesc->flag & REI_QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
        queueDesc.Flags |= D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    queueDesc.Priority = gDx12WindowsQueuePriorityTranslator[pQDesc->priority];

    CHECK_HRESULT(pRenderer->pDxDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(ppDxCmdQueue)));
}

void d3d12_platform_submit_resource_barriers(REI_Cmd* pCmd, uint32_t barriersCount, D3D12_RESOURCE_BARRIER* barriers)
{
    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)->ResourceBarrier(barriersCount, barriers);
}

DXGI_FORMAT util_to_dx12_swapchain_format(uint32_t const format)
{
    REI_ASSERT(format < REI_FMT_COUNT, "Invalid format");
    DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;

    // FLIP_DISCARD and FLIP_SEQEUNTIAL swapchain buffers only support these formats
    switch (format)
    {
        case REI_FMT_R16G16B16A16_SFLOAT: result = DXGI_FORMAT_R16G16B16A16_FLOAT; break;
        case REI_FMT_B8G8R8A8_UNORM:
        case REI_FMT_B8G8R8A8_SRGB: result = DXGI_FORMAT_B8G8R8A8_UNORM; break;
        case REI_FMT_R8G8B8A8_UNORM:
        case REI_FMT_R8G8B8A8_SRGB: result = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case REI_FMT_R10G10B10A2_UNORM: result = DXGI_FORMAT_R10G10B10A2_UNORM; break;
        default: break;
    }

    return result;
}

void util_create_dxgi_factory(IDXGIFactory6** ppFactory)
{
    UINT flags = 0;
#ifdef _DEBUG
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    CHECK_HRESULT(CreateDXGIFactory2(flags, IID_PPV_ARGS(ppFactory)));
}

constexpr uint32_t util_enumerate_gpus_stack_size_in_bytes() 
{ return sizeof(DXGI_ADAPTER_DESC3); }

void util_enumerate_gpus(
    REI_StackAllocator<true>& stackAlloc, uint32_t* pGpuCount, REI_GpuDesc* gpuDesc, bool* pFoundSoftwareAdapter)
{
    IDXGIFactory6* pDXGIFactory;
    util_create_dxgi_factory(&pDXGIFactory);

    D3D_FEATURE_LEVEL feature_levels[4] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    uint32_t       gpuCount = 0;
    IDXGIAdapter4* adapter = NULL;
    bool           foundSoftwareAdapter = false;

    DXGI_ADAPTER_DESC3& desc = *stackAlloc.allocZeroed<DXGI_ADAPTER_DESC3>();

    // Find number of usable GPUs
    // Use DXGI6 interface which lets us specify gpu preference so we dont need to use NVOptimus or AMDPowerExpress exports
    for (UINT i = 0; DXGI_ERROR_NOT_FOUND != pDXGIFactory->EnumAdapterByGpuPreference(
                                                 i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
         ++i)
    {
        if (gpuCount >= REI_MAX_GPUS)
        {
            break;
        }

        adapter->GetDesc3(&desc);

        // Ignore Microsoft Driver
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
        {
            for (uint32_t level = 0; level < sizeof(feature_levels) / sizeof(feature_levels[0]); ++level)
            {
                // Make sure the adapter can support a D3D12 device
                if (SUCCEEDED(D3D12CreateDevice(adapter, feature_levels[level], __uuidof(ID3D12Device), NULL)))
                {
                    IDXGIAdapter* pGpu;
                    HRESULT       hres = adapter->QueryInterface(IID_PPV_ARGS(&pGpu));
                    if (SUCCEEDED(hres))
                    {
                        if (gpuDesc)
                        {
                            ID3D12Device* pDevice;
                            REI_GpuDesc*  pGpuDesc = &gpuDesc[gpuCount];
                            pGpuDesc->pGpu = pGpu;
                            D3D12CreateDevice(adapter, feature_levels[level], IID_PPV_ARGS(&pDevice));
                            util_fill_gpu_desc(pDevice, feature_levels[level], pGpuDesc);
                            SAFE_RELEASE(pDevice);
                        }
                        else
                        {
                            SAFE_RELEASE(pGpu);
                        }
                        ++gpuCount;
                        break;
                    }
                }
            }
        }
        else
        {
            foundSoftwareAdapter = true;
        }

        adapter->Release();
    }
    pDXGIFactory->Release();

    if (pGpuCount)
        *pGpuCount = gpuCount;

    if (pFoundSoftwareAdapter)
        *pFoundSoftwareAdapter = foundSoftwareAdapter;
}

constexpr uint32_t util_REI_selectBestGpu_stack_size_in_bytes() 
{
    return util_enumerate_gpus_stack_size_in_bytes() +
           (sizeof(REI_DeviceProperties) + sizeof(REI_GpuDesc)) * REI_MAX_GPUS;
}

static bool REI_selectBestGpu(REI_StackAllocator<true>& stackAlloc, REI_Renderer* pRenderer, D3D_FEATURE_LEVEL* pFeatureLevel)
{
    const REI_AllocatorCallbacks& allocator = pRenderer->allocator;
    REI_LogPtr pLog = pRenderer->pLog;

    uint32_t gpuCount = 0;
    bool     foundSoftwareAdapter = false;
    REI_GpuDesc* gpuDesc = stackAlloc.allocZeroed<REI_GpuDesc>(REI_MAX_GPUS);

    // Find number of usable GPUs
    util_enumerate_gpus(stackAlloc, &gpuCount, gpuDesc, &foundSoftwareAdapter);

    // If the only adapter we found is a software adapter, log error message for QA
    if (!gpuCount && foundSoftwareAdapter)
    {
        pLog(REI_LOG_TYPE_ERROR, "The only available GPU has DXGI_ADAPTER_FLAG_SOFTWARE. Early exiting");
        REI_ASSERT(false);
        return false;
    }

    typedef bool (*DeviceBetterFn)(REI_GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex);
    DeviceBetterFn isDeviceBetter = [](REI_GpuDesc* gpuDesc, uint32_t testIndex, uint32_t refIndex) -> bool
    {
        const REI_GpuDesc& gpu1 = gpuDesc[testIndex];
        const REI_GpuDesc& gpu2 = gpuDesc[refIndex];

        return gpu1.mDedicatedVideoMemory > gpu2.mDedicatedVideoMemory;
    };

    uint32_t              gpuIndex = UINT32_MAX;
    REI_DeviceProperties* gpuSettings = (REI_DeviceProperties*)stackAlloc.allocZeroed<REI_DeviceProperties>(gpuCount);

    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        util_to_GpuSettings(&gpuDesc[i], &gpuSettings[i]);
        pLog(
            REI_LOG_TYPE_INFO, "GPU[%u] detected. Vendor ID: %s, Model ID: %s, Revision ID: %s, GPU Name: %s", i,
            gpuSettings[i].vendorId, gpuSettings[i].modelId, gpuSettings[i].revisionId, gpuSettings[i].deviceName);

        // Check that gpu supports at least graphics
        if (gpuIndex == UINT32_MAX || isDeviceBetter(gpuDesc, i, gpuIndex))
        {
            gpuIndex = i;
        }
    }
    // Get the latest and greatest feature level gpu
    if (!SUCCEEDED(gpuDesc[gpuIndex].pGpu->QueryInterface(IID_PPV_ARGS(&pRenderer->pDxActiveGPU))))
    {
        REI_ASSERT(false);
    }
    REI_ASSERT(pRenderer->pDxActiveGPU != NULL);
    pRenderer->pActiveGpuSettings = (REI_DeviceProperties*)pRenderer->allocator.pMalloc(
        pRenderer->allocator.pUserData, sizeof(REI_DeviceProperties), 0);
    *pRenderer->pActiveGpuSettings = gpuSettings[gpuIndex];

    for (uint32_t i = 0; i < gpuCount; ++i)
    {
        SAFE_RELEASE(gpuDesc[i].pGpu);
    }

    // Print selected GPU information
    pLog(REI_LOG_TYPE_INFO, "GPU[%d] is selected as default GPU", gpuIndex);
    pLog(REI_LOG_TYPE_INFO, "Name of selected gpu: %s", pRenderer->pActiveGpuSettings->deviceName);
    pLog(REI_LOG_TYPE_INFO, "Vendor id of selected gpu: %s", pRenderer->pActiveGpuSettings->vendorId);
    pLog(REI_LOG_TYPE_INFO, "Model id of selected gpu: %s", pRenderer->pActiveGpuSettings->modelId);
    pLog(REI_LOG_TYPE_INFO, "Revision id of selected gpu: %s", pRenderer->pActiveGpuSettings->revisionId);

    if (pFeatureLevel)
        *pFeatureLevel = gpuDesc[gpuIndex].mMaxSupportedFeatureLevel;

    return true;
}

inline void d3d12_utils_caps_builder(REI_Renderer* pRenderer)
{
    REI_DeviceCapabilities& capabilities = pRenderer->pActiveGpuSettings->capabilities;
    for (uint32_t i = 0; i < REI_Format::REI_FMT_COUNT; ++i)
    {
        DXGI_FORMAT fmt = REI_Format_ToDXGI_FORMAT(i);
        if (fmt == DXGI_FORMAT_UNKNOWN)
            continue;

        D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { fmt };

        pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
        capabilities.canShaderReadFrom[i] = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
        capabilities.canShaderWriteTo[i] = (formatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE) != 0;
        capabilities.canRenderTargetWriteTo[i] = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET) != 0;
    }
}

void d3d12_platform_create_renderer(const REI_AllocatorCallbacks& allocator, REI_Renderer** ppRenderer)
{
    *ppRenderer = REI_new<REI_Renderer_Windows>(allocator);
    REI_ASSERT(*ppRenderer);

    (*ppRenderer)->allocator = allocator;
}

bool d3d12_platform_add_device(const REI_RendererDescD3D12* pDesc, REI_Renderer* pRenderer)
{
    REI_StackAllocator<true> stackAlloc = { 0 };
    stackAlloc.reserve<uint8_t>(util_REI_selectBestGpu_stack_size_in_bytes());
#if defined(_DEBUG)
    stackAlloc.reserve<uint8_t>(util_enumerate_gpus_stack_size_in_bytes());
#endif

    if (!stackAlloc.done(pRenderer->allocator))
    {
        pRenderer->pLog(REI_LOG_TYPE_ERROR, "REI_selectBestGpu wasn't able to allocate enough memory for stackAlloc");
        REI_ASSERT(false);
        return false;
    }

    D3D_FEATURE_LEVEL     supportedFeatureLevel = (D3D_FEATURE_LEVEL)0;
    REI_Renderer_Windows* pWindowsRenderer = (REI_Renderer_Windows*)pRenderer;
    if (!REI_selectBestGpu(stackAlloc, pWindowsRenderer, &supportedFeatureLevel))
        return false;

    CHECK_HRESULT(D3D12CreateDevice(
        pWindowsRenderer->pDxActiveGPU, supportedFeatureLevel, IID_PPV_ARGS(&pWindowsRenderer->pDxDevice)));

#if defined(_DEBUG)
    HRESULT hr = pWindowsRenderer->pDxDevice->QueryInterface(IID_PPV_ARGS(&pWindowsRenderer->pDxDebugValidation));
    if (SUCCEEDED(hr))
    {
        pWindowsRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
        pWindowsRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        // D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND breaks even when it is disabled
        pWindowsRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
        pWindowsRenderer->pDxDebugValidation->SetBreakOnID(D3D12_MESSAGE_ID_LOADPIPELINE_NAMENOTFOUND, false);

        const uint8_t    DENY_LIST_MAX_SIZE = 10;
        D3D12_MESSAGE_ID denyList[DENY_LIST_MAX_SIZE];
        uint8_t          denyListCounter = 0;
        denyList[denyListCounter] = D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE;
        ++denyListCounter;
        denyList[denyListCounter] = D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE;
        ++denyListCounter;
        // On Windows 11 there's a bug in the DXGI debug layer that triggers a false-positive on hybrid GPU
        // laptops during Present. The problem appears to be a race condition, so it may or may not happen.
        // Suppressing D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE avoids this problem.
        uint32_t gpuCount = 0;
        util_enumerate_gpus(stackAlloc, &gpuCount, NULL, NULL);
        // If we have >2 GPU's (eg. Laptop with integrated and dedicated GPU).
        if (gpuCount >= 2)
        {
            denyList[denyListCounter] = D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE;
            ++denyListCounter;
        }

        if (denyListCounter)
        {
            REI_ASSERT(denyListCounter < DENY_LIST_MAX_SIZE);
            D3D12_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = static_cast<UINT>(denyListCounter);
            filter.DenyList.pIDList = denyList;
            pWindowsRenderer->pDxDebugValidation->AddStorageFilterEntries(&filter);
        }
    }
#endif

    d3d12_utils_caps_builder(pWindowsRenderer);

    if (pWindowsRenderer->mShaderTarget >= REI_SHADER_TARGET_6_0)
    {
        // Query the level of support of Shader Model.
        D3D12_FEATURE_DATA_SHADER_MODEL   shaderModelSupport = { D3D_SHADER_MODEL_6_0 };
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 waveIntrinsicsSupport = {};
        if (!SUCCEEDED(pWindowsRenderer->pDxDevice->CheckFeatureSupport(
                (D3D12_FEATURE)D3D12_FEATURE_SHADER_MODEL, &shaderModelSupport, sizeof(shaderModelSupport))))
        {
            return false;
        }
        // Query the level of support of Wave Intrinsics.
        if (!SUCCEEDED(pWindowsRenderer->pDxDevice->CheckFeatureSupport(
                (D3D12_FEATURE)D3D12_FEATURE_D3D12_OPTIONS1, &waveIntrinsicsSupport, sizeof(waveIntrinsicsSupport))))
        {
            return false;
        }
    }

    return true;
}

void d3d12_platform_remove_device(REI_Renderer* pRenderer)
{
    REI_Renderer_Windows* pWindowsRenderer = (REI_Renderer_Windows*)pRenderer;
    SAFE_RELEASE(pWindowsRenderer->pDxActiveGPU);
#if defined(_DEBUG)
    if (pWindowsRenderer->pDxDebugValidation)
    {
        pWindowsRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, false);
        pWindowsRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, false);
        pWindowsRenderer->pDxDebugValidation->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, false);
        SAFE_RELEASE(pWindowsRenderer->pDxDebugValidation);
    }
    ID3D12DebugDevice* pDebugDevice = NULL;
    pWindowsRenderer->pDxDevice->QueryInterface(IID_PPV_ARGS(&pDebugDevice));

    SAFE_RELEASE(pWindowsRenderer->pDxDevice);

    if (pDebugDevice)
    {
        // Debug device is released first so report live objects don't show its ref as a warning.
        pDebugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        SAFE_RELEASE(pDebugDevice);
    }
#else
    SAFE_RELEASE(pWindowsRenderer->pDxDevice);
#endif
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

    if (!(pRenderer->mShaderCacheFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY))
    {
        ppPipelineCache = nullptr;
        return;
    }

    // Persistent allocator
    REI_StackAllocator<false> persistentAlloc = { 0 };
    persistentAlloc.reserve<REI_PipelineCache>().reserve<uint8_t>(pDesc->mSize);

    if (!persistentAlloc.done(pRenderer->allocator))
    {
        pRenderer->pLog(
            REI_LOG_TYPE_ERROR, "REI_addPipelineCache wasn't able to allocate enough memory for persistentAlloc");
        REI_ASSERT(false);
        *ppPipelineCache = nullptr;
        return;
    }

    REI_PipelineCache* pPipelineCache = persistentAlloc.alloc<REI_PipelineCache>();
    REI_ASSERT(pPipelineCache);

    if (pDesc->mSize)
    {
        // D3D12 does not copy pipeline cache data. We have to keep it around until the cache is alive
        pPipelineCache->pData = persistentAlloc.allocZeroed<uint8_t>(pDesc->mSize);
        memcpy(pPipelineCache->pData, pDesc->pData, pDesc->mSize);
    }

    ID3D12Device1* device1 = NULL;
    HRESULT        result = pRenderer->pDxDevice->QueryInterface(IID_PPV_ARGS(&device1));
    if (SUCCEEDED(result))
    {
        result = device1->CreatePipelineLibrary(
            pPipelineCache->pData, pDesc->mSize, IID_PPV_ARGS(&pPipelineCache->pLibrary));
    }
    SAFE_RELEASE(device1);

    if (!SUCCEEDED(result))
    {
        pRenderer->pLog(REI_LogType::REI_LOG_TYPE_ERROR, "Failed to create pipeline cache");
        REI_removePipelineCache(pRenderer, pPipelineCache);
        pPipelineCache = nullptr;
    }

    *ppPipelineCache = pPipelineCache;
}

void REI_removePipelineCache(REI_Renderer* pRenderer, REI_PipelineCache* pPipelineCache)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pPipelineCache);

    SAFE_RELEASE(pPipelineCache->pLibrary);
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, pPipelineCache);
}

void REI_getPipelineCacheData(REI_Renderer* pRenderer, REI_PipelineCache* pPipelineCache, size_t* pSize, void* pData)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pPipelineCache);
    REI_ASSERT(pSize);

    if (pPipelineCache->pLibrary)
    {
        size_t cacheSize = pPipelineCache->pLibrary->GetSerializedSize();
        if (pData)
        {
            CHECK_HRESULT(pPipelineCache->pLibrary->Serialize(pData, *pSize));
        }
        *pSize = cacheSize;
    }
}

void REI_addSwapchain(REI_Renderer* pRenderer, const REI_SwapchainDesc* p_desc, REI_Swapchain** pp_swap_chain)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(p_desc);
    REI_ASSERT(pp_swap_chain);

    REI_LogPtr pLog = pRenderer->pLog;

    IDXGIFactory6* pDXGIFactory;
    util_create_dxgi_factory(&pDXGIFactory);

    REI_Swapchain_Windows* pSwapChain = (REI_Swapchain_Windows*)REI_calloc(
        pRenderer->allocator, sizeof(REI_Swapchain_Windows) + p_desc->imageCount * sizeof(REI_Texture*));

    REI_ASSERT(pSwapChain);
    pSwapChain->pAllocator = &pRenderer->allocator;
    pSwapChain->ppRenderTargets = (REI_Texture**)(pSwapChain + 1);
    REI_ASSERT(pSwapChain->ppRenderTargets);

    pSwapChain->mDxSyncInterval = p_desc->enableVsync ? 1 : 0;

    DXGI_FORMAT dxFormat = util_to_dx12_swapchain_format(p_desc->colorFormat);
    if (dxFormat == DXGI_FORMAT_UNKNOWN)
    {
        pLog(REI_LOG_TYPE_ERROR, "Image Format (%u) not supported for creating swapchain buffer", (uint32_t)dxFormat);
    }

    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = p_desc->width;
    desc.Height = p_desc->height;
    desc.Format = dxFormat;
    desc.Stereo = false;
    desc.SampleDesc.Count = 1;    // If multisampling is needed, we'll resolve it later
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    desc.BufferCount = p_desc->imageCount;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    BOOL allowTearing = FALSE;
    pDXGIFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    desc.Flags |= allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    pSwapChain->mFlags |= (!p_desc->enableVsync && allowTearing) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    IDXGISwapChain1* swapchain;

    HWND hwnd = (HWND)p_desc->windowHandle.window;

    CHECK_HRESULT(pDXGIFactory->CreateSwapChainForHwnd(
        p_desc->ppPresentQueues[0]->pDxQueue, hwnd, &desc, NULL, NULL, &swapchain));

    CHECK_HRESULT(pDXGIFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

    CHECK_HRESULT(swapchain->QueryInterface(IID_PPV_ARGS(&pSwapChain->pDxSwapChain)));
    swapchain->Release();
    pDXGIFactory->Release();

    ID3D12Resource** buffers = (ID3D12Resource**)alloca(p_desc->imageCount * sizeof(ID3D12Resource*));

    // Create rendertargets from swapchain
    for (uint32_t i = 0; i < p_desc->imageCount; ++i)
    {
        CHECK_HRESULT(pSwapChain->pDxSwapChain->GetBuffer(i, IID_PPV_ARGS(&buffers[i])));
    }

    REI_TextureDesc descColor = {};
    descColor.width = p_desc->width;
    descColor.height = p_desc->height;
    descColor.depth = 1;
    descColor.arraySize = 1;
    descColor.format = p_desc->colorFormat;
    descColor.clearValue = p_desc->colorClearValue;
    descColor.sampleCount = REI_SAMPLE_COUNT_1;
    descColor.pNativeHandle = NULL;
    descColor.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET | REI_DESCRIPTOR_TYPE_TEXTURE;
    (uint32_t&)descColor.flags = REI_TEXTURE_CREATION_FLAG_ALLOW_DISPLAY_TARGET;
    descColor.componentMapping[0] = REI_COMPONENT_MAPPING_R;
    descColor.componentMapping[1] = REI_COMPONENT_MAPPING_G;
    descColor.componentMapping[2] = REI_COMPONENT_MAPPING_B;
    descColor.componentMapping[3] = REI_COMPONENT_MAPPING_A;

    for (uint32_t i = 0; i < p_desc->imageCount; ++i)
    {
        descColor.pNativeHandle = (uint64_t)buffers[i];
        REI_addTexture(pRenderer, &descColor, &pSwapChain->ppRenderTargets[i]);
    }

    pSwapChain->mImageCount = p_desc->imageCount;
    pSwapChain->mEnableVsync = p_desc->enableVsync;

    *pp_swap_chain = pSwapChain;
}

void REI_queuePresent(
    REI_Queue* p_queue, REI_Swapchain* p_swap_chain, uint32_t swap_chain_image_index, uint32_t wait_semaphore_count,
    REI_Semaphore** pp_wait_semaphores)
{
    REI_LogPtr pLog = p_queue->pRenderer->pLog;

    if (p_swap_chain)
    {
        REI_Swapchain_Windows* pWindowsSwapChain = (REI_Swapchain_Windows*)p_swap_chain;
        HRESULT                hr;
        hr = pWindowsSwapChain->pDxSwapChain->Present(pWindowsSwapChain->mDxSyncInterval, pWindowsSwapChain->mFlags);

        if (FAILED(hr))
        {
            ID3D12Device* device = NULL;
            pWindowsSwapChain->pDxSwapChain->GetDevice(IID_PPV_ARGS(&device));
            HRESULT removeHr = device->GetDeviceRemovedReason();

            if (FAILED(removeHr))
            {
                Sleep(5000);    // Wait for a few seconds to allow the driver to come back online before doing a reset.
            }

#if defined(USE_DRED)
            ID3D12DeviceRemovedExtendedData* pDread;
            if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&pDread))))
            {
                D3D12_DRED_AUTO_BREADCRUMBS_OUTPUT breadcrumbs;
                if (SUCCEEDED(pDread->GetAutoBreadcrumbsOutput(&breadcrumbs)))
                {
                    pLog(REI_LOG_TYPE_INFO, "Gathered auto-breadcrumbs output.");
                }

                D3D12_DRED_PAGE_FAULT_OUTPUT pageFault;
                if (SUCCEEDED(pDread->GetPageFaultAllocationOutput(&pageFault)))
                {
                    pLog(REI_LOG_TYPE_INFO, "Gathered page fault allocation output.");
                }
            }
            pDread->Release();
#endif
            device->Release();

            pLog(REI_LOG_TYPE_ERROR, "Failed to present swapchain render target");
            REI_ASSERT(false);
        }
    }
}

void REI_removeSwapchain(REI_Renderer* pRenderer, REI_Swapchain* p_swap_chain)
{
    REI_Swapchain_Windows* pWindowsSwapChain = (REI_Swapchain_Windows*)p_swap_chain;
    for (uint32_t i = 0; i < pWindowsSwapChain->mImageCount; ++i)
    {
        ID3D12Resource* resource = pWindowsSwapChain->ppRenderTargets[i]->pDxResource;
        REI_removeTexture(pRenderer, pWindowsSwapChain->ppRenderTargets[i]);
        SAFE_RELEASE(resource);
    }

    CHECK_HRESULT(pWindowsSwapChain->pDxSwapChain->SetFullscreenState(false, nullptr));
    SAFE_RELEASE(pWindowsSwapChain->pDxSwapChain);
    pRenderer->allocator.pFree(pRenderer->allocator.pUserData, p_swap_chain);
}

void REI_acquireNextImage(
    REI_Renderer* pRenderer, REI_Swapchain* pSwapchain, REI_Semaphore* pSignalSemaphore, REI_Fence* pFence,
    uint32_t* pImageIndex)
{
    REI_ASSERT(pRenderer);
    REI_ASSERT(pImageIndex);
    REI_Swapchain_Windows* pWindowsSwapChain = (REI_Swapchain_Windows*)pSwapchain;
    *pImageIndex = pWindowsSwapChain->pDxSwapChain->GetCurrentBackBufferIndex();
}

void REI_cmdCopyBuffer(
    REI_Cmd* pCmd, REI_Buffer* pBuffer, uint64_t dstOffset, REI_Buffer* pSrcBuffer, uint64_t srcOffset, uint64_t size)
{
    REI_ASSERT(pCmd);
    REI_ASSERT(pSrcBuffer);
    REI_ASSERT(pSrcBuffer->pDxResource);
    REI_ASSERT(pBuffer);
    REI_ASSERT(pBuffer->pDxResource);

    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
        ->CopyBufferRegion(pBuffer->pDxResource, dstOffset, pSrcBuffer->pDxResource, srcOffset, size);
}

void REI_cmdCopyBufferToTexture(
    REI_Cmd* pCmd, REI_Texture* pTexture, REI_Buffer* pSrcBuffer, REI_SubresourceDesc* pSubresourceDesc)
{
    uint32_t subresource = CALC_SUBRESOURCE_INDEX(
        pSubresourceDesc->mipLevel, pSubresourceDesc->arrayLayer, 0, pTexture->mMipLevels,
        pTexture->mArraySizeMinusOne + 1);
    D3D12_RESOURCE_DESC resourceDesc = pTexture->pDxResource->GetDesc();

    D3D12_TEXTURE_COPY_LOCATION src = {};
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src.pResource = pSrcBuffer->pDxResource;
    pCmd->pRenderer->pDxDevice->GetCopyableFootprints(
        &resourceDesc, subresource, 1, pSubresourceDesc->bufferOffset, &src.PlacedFootprint, NULL, NULL, NULL);
    src.PlacedFootprint.Offset = pSubresourceDesc->bufferOffset;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = pTexture->pDxResource;
    dst.SubresourceIndex = subresource;

    D3D12_BOX copyBox = { 0, 0, 0, pSubresourceDesc->region.w, pSubresourceDesc->region.h, pSubresourceDesc->region.d };

    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
        ->CopyTextureRegion(
            &dst, pSubresourceDesc->region.x, pSubresourceDesc->region.y, pSubresourceDesc->region.z, &src, &copyBox);
}

void REI_cmdCopyTextureToBuffer(
    REI_Cmd* pCmd, REI_Buffer* pDstBuffer, REI_Texture* pTexture, REI_SubresourceDesc* pSubresourceDesc)
{
    uint32_t subresource = CALC_SUBRESOURCE_INDEX(
        pSubresourceDesc->mipLevel, pSubresourceDesc->arrayLayer, 0, pTexture->mMipLevels,
        pTexture->mArraySizeMinusOne + 1);
    D3D12_RESOURCE_DESC resourceDesc = pTexture->pDxResource->GetDesc();

    D3D12_TEXTURE_COPY_LOCATION src = {};
    D3D12_TEXTURE_COPY_LOCATION dst = {};
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src.pResource = pTexture->pDxResource;
    src.SubresourceIndex = subresource;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.pResource = pDstBuffer->pDxResource;
    pCmd->pRenderer->pDxDevice->GetCopyableFootprints(
        &resourceDesc, subresource, 1, pSubresourceDesc->bufferOffset, &dst.PlacedFootprint, NULL, NULL, NULL);
    dst.PlacedFootprint.Offset = pSubresourceDesc->bufferOffset;

    D3D12_BOX copyBox = { 0,
                          0,
                          0,
                          pSubresourceDesc->region.w,
                          pSubresourceDesc->region.h,
                          pSubresourceDesc->region.d ? pSubresourceDesc->region.d : 1 };

    ((ID3D12GraphicsCommandList*)pCmd->pDxCmdList)
        ->CopyTextureRegion(
            &dst, pSubresourceDesc->region.x, pSubresourceDesc->region.y, pSubresourceDesc->region.z, &src, &copyBox);
}

void REI_cmdBeginDebugMarker(REI_Cmd* pCmd, float r, float g, float b, const char* pName)
{
#if REI_ENABLE_PIX
    //color is in B8G8R8X8 format where X is padding
    PIXBeginEvent(
        (ID3D12GraphicsCommandList*)pCmd->pDxCmdList, PIX_COLOR((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255)),
        pName);
#endif
}

void REI_cmdEndDebugMarker(REI_Cmd* pCmd)
{
#if REI_ENABLE_PIX
    PIXEndEvent((ID3D12GraphicsCommandList*)pCmd->pDxCmdList);
#endif
}