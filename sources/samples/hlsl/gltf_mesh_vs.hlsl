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

struct SceneUniforms
{
    float4x4 uVP;
};
struct ModelUniforms
{
    float4x4 uModel;
};
struct VS_INPUT
{
    REI_SPIRV([[vk::location(0)]]) float3 aPos : POSITION;
    REI_SPIRV([[vk::location(1)]]) float3 aColor: COLOR;
    REI_SPIRV([[vk::location(2)]]) float3 aNorm: NORMAL;
    REI_SPIRV([[vk::location(3)]]) float2 aUV: TEXCOORD;
};

struct PS_INPUT
{
    float4 CSPos: SV_Position;

    REI_SPIRV([[vk::location(0)]]) struct
    {
        float3 Pos;
        float3 Color;
        float3 Norm;
        float2 UV;
    } Out: COLOR0;
};

struct uVertPC
{
    REI_SPIRV([[vk::offset(0)]]) uint idx;
};
REI_DECLARE_PUSH_CONSTANT(v_pushconstant, uVertPC, 0, 0);

REI_SPIRV([[vk::binding(0, 0)]]) StructuredBuffer<SceneUniforms> uSceneUniforms REI_REGISTER(t0, space0);
REI_SPIRV([[vk::binding(0, 1)]]) StructuredBuffer<ModelUniforms> uModelUniforms REI_REGISTER(t0, space1);

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Out.Color = input.aColor;
    output.Out.Pos = input.aPos;
    output.Out.Norm = input.aNorm;
    output.Out.UV = input.aUV;

    output.CSPos = mul(uSceneUniforms[0].uVP, mul(uModelUniforms[v_pushconstant.idx].uModel, float4(input.aPos, 1.0)));
    
    return output;
}
