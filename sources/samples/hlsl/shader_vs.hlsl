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

struct PS_INPUT
{
    float4                     position: SV_Position;
    REI_SPIRV([[vk::location(0)]]) float4 color: COLOR0;
};

static const float2 vertices[3] = { float2(0.0f, -0.5f), float2(0.5f, 0.5f), float2(-0.5f, 0.5f) };

static const float3 colors[3] = { float3(1.0f, 0.0f, 0.0f), float3(0.0f, 1.0f, 0.0f), float3(0.0f, 0.0f, 1.0f) };

PS_INPUT main(uint vertexID : SV_VertexID)
{
    PS_INPUT output;
    output.position = float4(vertices[vertexID], 0.f, 1.f);
    output.color = float4(colors[vertexID], 1.f);
    return output;
}