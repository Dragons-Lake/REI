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

struct Paint
{
    float4 scissorMat[2];
    float4 paintMat[2];
    float4 innerCol;
    float4 outerCol;
    float2 scissorExt;
    float2 scissorScale;
    float2 extent;
    float  radius;
    float  feather;
    float  strokeMult;
    float  strokeThr;
    int    texType;
    int    type;
};

struct PS_INPUT
{
    REI_SPIRV([[vk::location(0)]]) float2 Pos: POSITION0;
    REI_SPIRV([[vk::location(1)]]) float2 UV: TEXCOORD0;
    float4                     CSPos: SV_Position;
};

struct PS_OUTPUT
{
    REI_SPIRV([[vk::location(0)]]) float4 outColor: SV_Target0;
};

REI_SPIRV([[vk::binding(0, 0)]]) SamplerState uSampler REI_REGISTER(s0);

REI_SPIRV([[vk::binding(1, 1)]]) StructuredBuffer<Paint> uPaints REI_REGISTER(t1, space1);

REI_SPIRV([[vk::binding(0, 2)]]) Texture2D uTexture REI_REGISTER(t0, space2);

struct uFragPC
{
    REI_SPIRV([[vk::offset(16)]]) uint idx;
};

REI_DECLARE_PUSH_CONSTANT(f_pushconstant, uFragPC, 0, 0);

float sdroundrect(float2 pt, float2 ext, float rad)
{
    float2 ext2 = ext - float2(rad, rad);
    float2 d = abs(pt) - ext2;
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - rad;
}
float2 mulMax23Vec2(float4 m[2], float2 v)
{
    return float2(m[0].x * v.x + m[0].y * v.y + m[0].z, m[1].x * v.x + m[1].y * v.y + m[1].z);
}

// Scissoring
float scissorMask(float2 p)
{
    float2 sc = (abs(mulMax23Vec2(uPaints[f_pushconstant.idx].scissorMat, p)) - uPaints[f_pushconstant.idx].scissorExt);
    sc = float2(0.5, 0.5) - sc * uPaints[f_pushconstant.idx].scissorScale;
    return clamp(sc.x, 0.0, 1.0) * clamp(sc.y, 0.0, 1.0);
}

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    float4    result = float4(1, 1, 1, 1);
    float     scissor = scissorMask(input.Pos);
    float     strokeAlpha = 1.0;
    if (uPaints[f_pushconstant.idx].type == 0)
    {    // Gradient
         // Calculate gradient color using box gradient
        float2 pt = mulMax23Vec2(uPaints[f_pushconstant.idx].paintMat, input.Pos);
        float  d = clamp(
            (sdroundrect(pt, uPaints[f_pushconstant.idx].extent, uPaints[f_pushconstant.idx].radius) + uPaints[f_pushconstant.idx].feather * 0.5f) /
                uPaints[f_pushconstant.idx].feather,
            0.0, 1.0);
        float4 color = lerp(uPaints[f_pushconstant.idx].innerCol, uPaints[f_pushconstant.idx].outerCol, d);
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    }
    else if (uPaints[f_pushconstant.idx].type == 1)
    {    // Image
         // Calculate color fron texture
        float2 pt = mulMax23Vec2(uPaints[f_pushconstant.idx].paintMat, input.Pos) / uPaints[f_pushconstant.idx].extent;

        float4 color = uTexture.Sample(uSampler, pt);
        if (uPaints[f_pushconstant.idx].texType == 1)
            color = float4(color.xyz * color.w, color.w);
        if (uPaints[f_pushconstant.idx].texType == 2)
            color = (float4)color.x;
        // Apply color tint and alpha.
        color *= uPaints[f_pushconstant.idx].innerCol;
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    } /*else if (uPaints.a[fpc.idx].type == 2) {        // Stencil fill
        result = vec4(1,1,1,1);
    }*/
    else if (uPaints[f_pushconstant.idx].type == 3)
    {    // Textured tris
        float4 color = uTexture.Sample(uSampler, input.UV);
        if (uPaints[f_pushconstant.idx].texType == 1)
            color = float4(color.xyz * color.w, color.w);
        if (uPaints[f_pushconstant.idx].texType == 2)
            color = (float4)color.x;
        color *= scissor;
        result = color * uPaints[f_pushconstant.idx].innerCol;
    }
    output.outColor = result;
    return output;
}
