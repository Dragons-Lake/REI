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
};

struct PS_OUTPUT
{
    REI_SPIRV([[vk::location(0)]]) float4 outColor: SV_Target0;
};

REI_SPIRV([[vk::binding(0, 0)]]) Texture2D<float4> uTexture REI_REGISTER(t0, space0);

PS_OUTPUT main(PS_INPUT input)
{
    uint2 texel = uint2(input.CSPos.xy);
    
    PS_OUTPUT output;
    output.outColor = uTexture.Load(uint3(texel, 0));
    return output;
}