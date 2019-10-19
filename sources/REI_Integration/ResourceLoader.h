/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#pragma once

#include "REI/Renderer/Renderer.h"

typedef struct REI_RL_BufferUpdateDesc
{
    REI_Buffer* pBuffer;
    const void* pData;
    uint64_t    srcOffset;
    uint64_t    dstOffset;
    uint64_t    size;
} REI_RL_BufferUpdateDesc;

typedef struct REI_RL_TextureUpdateDesc
{
    REI_Texture*      pTexture;
    uint8_t*          pRawData;
    REI_Format        format;
    uint32_t          x, y, z;
    uint32_t          width, height, depth;
    uint32_t          arrayLayer;
    uint32_t          mipLevel;
    REI_ResourceState endState;
} REI_RL_TextureUpdateDesc;

typedef uintptr_t REI_RL_RequestId;

typedef struct REI_RL_ResourceLoaderDesc
{
    uint64_t bufferSize;
    uint32_t bufferCount;
    uint32_t timesliceMs;
} REI_RL_ResourceLoaderDesc;

struct REI_RL_State;

void REI_RL_addResourceLoader(REI_Renderer* pRenderer, REI_RL_ResourceLoaderDesc* pDesc, REI_RL_State** ppRMState);
void REI_RL_removeResourceLoader(REI_RL_State* pLoader);

void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_BufferUpdateDesc* pBuffer, bool batch = false);
void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_TextureUpdateDesc* pTexture, bool batch = false);
void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_BufferUpdateDesc* pBuffer, REI_RL_RequestId* token);
void REI_RL_updateResource(REI_RL_State* pRMState, REI_RL_TextureUpdateDesc* pTexture, REI_RL_RequestId* token);

bool REI_RL_isBatchCompleted(REI_RL_State* pRMState);
void REI_RL_waitBatchCompleted(REI_RL_State* pRMState);
bool REI_RL_isTokenCompleted(REI_RL_State* pRMState, REI_RL_RequestId token);
void REI_RL_waitTokenCompleted(REI_RL_State* pRMState, REI_RL_RequestId token);
