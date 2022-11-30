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

struct Uniforms
{
    float4x4 uMVP;
    float3   uPxScalers;
    uint     uColor;
};

struct VS_INPUT
{
    REI_SPIRV([[vk::location(0)]]) float3 aPos : POSITION;
    REI_SPIRV([[vk::location(1)]]) float4 aColor : COLOR;
};

struct PS_INPUT
{
    float4 CSPos: SV_Position;

    REI_SPIRV([[vk::location(0)]]) struct
    {
        float4 Color;

    } Out: COLOR0;
};

struct uVertPC
{
    REI_SPIRV([[vk::offset(0)]]) uint idx;
};

REI_DECLARE_PUSH_CONSTANT(v_pushconstant, uVertPC, 0, 0);

REI_SPIRV([[vk::binding(0, 0)]]) StructuredBuffer<Uniforms> uUniforms REI_REGISTER(t0, space0);

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Out.Color = input.aColor;
    output.CSPos = mul(uUniforms[v_pushconstant.idx].uMVP, float4(input.aPos, 1.0));
    return output;
}
