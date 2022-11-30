/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 */

#include "defines.hlsli"
#pragma pack_matrix(column_major)

struct PointStruct
{
    float3 p;
    uint   c;
};
static const uint SIZEOF_POINT_0 = 16;
static const uint SIZEOF_POINT_1_2 = 12;

struct Uniforms
{
    float4x4 uMVP;
    float3   uPxScalers;
    uint     uColor;
};

struct uVertPC
{
    REI_SPIRV([[vk::offset(0)]]) uint idx;
    uint                   type;
    uint                   baseVertexLocation;
};


REI_DECLARE_PUSH_CONSTANT(v_pushconstant, uVertPC, 0, 0);

REI_SPIRV([[vk::binding(0, 0)]]) StructuredBuffer<Uniforms> uUniforms REI_REGISTER(t0, space0);

REI_SPIRV([[vk::binding(0, 1)]]) ByteAddressBuffer uStream0 REI_REGISTER(t0, space1);

REI_SPIRV([[vk::binding(1, 1)]]) StructuredBuffer<uint> uStream1 REI_REGISTER(t1, space1);

struct VS_INPUT
{
    uint vertexID: SV_VertexID;
};

struct PS_INPUT
{
    float4 CSPos: SV_Position;

    REI_SPIRV([[vk::location(0)]]) struct
    {
        float4 Color;

    } Out: COLOR0;
};

float4 Color32toVec4(uint c)
{
    float4 r = float4(float(c & 0xFF), float((c >> 8) & 0xFF), float((c >> 16) & 0xFF), float((c >> 24) & 0xFF));

    return r / 255.0f;
}

struct PointData
{
    float3 p;
    float4 c;
};

PointData loadPoint(uint index)
{
    float3 p = (float3)0;
    uint   c = 0;

    if (v_pushconstant.type == 0)
    {
        p = asfloat(uStream0.Load3(index * SIZEOF_POINT_0));
        c = asuint(uStream0.Load(index * SIZEOF_POINT_0 + 3 * 4));

    }
    else if (v_pushconstant.type == 1)
    {
        p = asfloat(uStream0.Load3(index * SIZEOF_POINT_1_2));
        c = uStream1[index];
    }
    else if (v_pushconstant.type == 2)
    {
        p = asfloat(uStream0.Load3(index * SIZEOF_POINT_1_2));
        c = uUniforms[v_pushconstant.idx].uColor;
    }
    PointData pData;
    pData.p = p;
    pData.c = Color32toVec4(c);
    return pData;
}

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    uint curVertexID = input.vertexID;
    #if !defined(__spirv__)
    curVertexID += v_pushconstant.baseVertexLocation;
    #endif
    uint pointIndex = curVertexID / 6;

    PointData pt = loadPoint(pointIndex);

    float4 p0 = mul(uUniforms[v_pushconstant.idx].uMVP, float4(pt.p, 1.0));

    float2 offset = uUniforms[v_pushconstant.idx].uPxScalers.xy * -p0.w; //undo perspective

    uint vertexIndex = abs(2 - int(curVertexID - pointIndex * 6));
    p0.x += bool(vertexIndex & 1) ? -offset.x : offset.x;
    p0.y += bool(vertexIndex & 2) ? -offset.y : offset.y;

    output.CSPos = p0;
    output.Out.Color = pt.c;

    return output;
}