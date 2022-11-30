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
    float4 CSPos: SV_Position;

    REI_SPIRV([[vk::location(0)]]) struct
    {
        float4 Color;
        float2 UV;
    } In: INPUT_DATA;
};

struct PS_OUTPUT
{
    REI_SPIRV([[vk::location(0)]]) float4 outColor: SV_Target0;
};

REI_SPIRV([[vk::binding(0, 0)]]) SamplerState uSampler REI_REGISTER(s0, space0);

REI_SPIRV([[vk::binding(0, 1)]]) Texture2D uTexture REI_REGISTER(t0, space1);

PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    output.outColor = float4(input.In.Color.rgb, input.In.Color.a * uTexture.Sample(uSampler, input.In.UV).r);
    return output;
}