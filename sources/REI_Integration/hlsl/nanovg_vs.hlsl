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

struct VS_INPUT
{
    REI_SPIRV([[vk::location(0)]]) float2 aPos: POSITION;
    REI_SPIRV([[vk::location(1)]]) float2 aUV: TEXCOORD;
};

struct PS_INPUT
{
    REI_SPIRV([[vk::location(0)]]) float2 Pos: POSITION0;
    REI_SPIRV([[vk::location(1)]]) float2 UV: TEXCOORD0;
    float4                     CSPos: SV_Position;
};

struct uPushConstant
{
    float2 uScale;
    float2 uTranslate;
};

REI_DECLARE_PUSH_CONSTANT(v_pushconstant, uPushConstant, 0, 0);

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;
    output.Pos = input.aPos;
    output.UV = input.aUV;
    output.CSPos = float4(input.aPos * v_pushconstant.uScale + v_pushconstant.uTranslate, 0, 1);

    return output;
}
