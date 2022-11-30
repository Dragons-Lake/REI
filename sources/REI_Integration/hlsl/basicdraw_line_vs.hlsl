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

struct LineStruct
{
    float3 p0;
    float  w;
    float3 p1;
    uint   c;
};
static const uint SIZEOF_LINE = 32;

struct Uniforms
{
    float4x4 uMVP;
    float3   uPxScalers;
    uint uColor;
};

struct uVertPC
{
    REI_SPIRV([[vk::offset(0)]]) uint idx;
    uint                   baseVertexLocation;
};

REI_DECLARE_PUSH_CONSTANT(v_pushconstant, uVertPC, 0, 0);

REI_SPIRV([[vk::binding(0, 0)]]) StructuredBuffer<Uniforms> uUniforms REI_REGISTER(t0, space0);

REI_SPIRV([[vk::binding(1, 0)]]) ByteAddressBuffer uLines REI_REGISTER(t1, space0);

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

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    uint curVertexID = input.vertexID;
    #if !defined(__spirv__)
    curVertexID += v_pushconstant.baseVertexLocation;
    #endif
    uint lineIndex = curVertexID / 6;
    // 0-------------2
    // |             |
    // 1-------------3
    // Line is triangles 210 and 123
    uint vertexIndex = abs(2 - int(curVertexID - lineIndex * 6));
    uint startAddr = lineIndex * SIZEOF_LINE;
    uint offset = 0;
    LineStruct curLine;
    curLine.p0 = asfloat(uLines.Load3(startAddr + offset));
    offset += 3 * 4;
    curLine.w = asfloat(uLines.Load(startAddr + offset));
    offset += 4;
    curLine.p1 = asfloat(uLines.Load3(startAddr + offset));
    offset += 3 * 4;
    curLine.c = uLines.Load(startAddr + offset);
    
    float4 p0 = float4(curLine.p0, 1.0);
    float4 p1 = float4(curLine.p1, 1.0);

    p0 = mul(uUniforms[v_pushconstant.idx].uMVP, p0);
    p1 = mul(uUniforms[v_pushconstant.idx].uMVP, p1);

    //Fix near plane intersection - place point on near plane if it is behind it;
    //Works only for non-reverse projection
    float4 pNear = lerp(p0, p1, -p0.z / (p1.z - p0.z));
    if (p0.z < 0.0)
    {
        p0 = pNear;
    }
    if (p1.z < 0.0)
    {
        p1 = pNear;
    }

    float2 n = p1.xy / p1.w - p0.xy / p0.w;

    n = normalize(float2(-n.y, n.x * uUniforms[v_pushconstant.idx].uPxScalers.z));
    //TODO: add caps support - currently they do not cover ends
    //n = normalize(vec2(n.x, n.y * uUniforms.a[vpc.idx].uPxScalers.z));
    //n = vec2(n.x - n.y, n.y + n.x);

    bool useP0 = bool(vertexIndex & 2);
    p0 = useP0 ? p0 : p1;
    float s = curLine.w *
              (bool(vertexIndex & 1) ? p0.w : -p0.w); // undo perspective and choose direction, scale by line width
    p0.xy += s * uUniforms[v_pushconstant.idx].uPxScalers.xy * n;

    output.CSPos = p0;
    output.Out.Color = Color32toVec4(curLine.c);

    return output;
}
