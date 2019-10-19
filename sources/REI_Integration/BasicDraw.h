#pragma once

#include "REI/Renderer/Renderer.h"

struct REI_BasicDraw_Desc
{
    REI_Format      colorFormat;
    REI_Format      depthStencilFormat;
    REI_SampleCount sampleCount;
    uint32_t        fbWidth;
    uint32_t        fbHeight;
    uint32_t        resourceSetCount;
    uint32_t        maxDrawCount;
    uint32_t        maxBufferSets;
    uint64_t        maxDataSize;
};

struct REI_BasicDraw_V_P3C
{
    float    p[3];
    uint32_t c;
};

struct REI_BasicDraw_V_P3
{
    float p[3];
};

struct REI_BasicDraw_Line
{
    float    p0[3];
    float    w;
    float    p1[3];
    uint32_t c;
};

struct REI_BasicDraw;

REI_BasicDraw* REI_BasicDraw_Init(REI_Renderer* Renderer, REI_BasicDraw_Desc* info);
void           REI_BasicDraw_SetupRender(REI_BasicDraw* state, uint32_t set_index);
void           REI_BasicDraw_Shutdown(REI_BasicDraw* state);

void REI_RegisterPointBufferSet(
    REI_BasicDraw* state, uint32_t idx, REI_Buffer* interleaved, REI_Buffer* positions, REI_Buffer* colors);

void REI_BasicDraw_RenderPoints(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], float ptsize, uint32_t count, REI_BasicDraw_V_P3C** point_data);
void REI_BasicDraw_RenderPoints(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], float ptsize, uint32_t count, REI_BasicDraw_V_P3** positions,
    uint32_t color);

void REI_BasicDraw_RenderLines(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], uint32_t count, REI_BasicDraw_Line** line_data);
void REI_BasicDraw_RenderMesh(
    REI_BasicDraw* state, REI_Cmd* pCmd, const float mvp[16], uint32_t count, REI_BasicDraw_V_P3C** verts);
