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
        float3 Pos;
        float3 Color;
        float3 Norm;
        float2 UV;

    } In: COLOR0;
};

struct PS_OUTPUT
{
    REI_SPIRV([[vk::location(0)]]) float4 outColor: SV_Target0;
};
REI_SPIRV([[vk::binding(0, 3)]]) SamplerState uSampler REI_REGISTER(s0, space3);
REI_SPIRV([[vk::binding(0, 2)]]) Texture2D    uBaseColorTexture REI_REGISTER(t0, space2);
REI_SPIRV([[vk::binding(1, 2)]]) Texture2D    uMetallicRoughnessTexture REI_REGISTER(t1, space2);


PS_OUTPUT main(PS_INPUT input)
{
    PS_OUTPUT output;
    
    //Since PBR has not yet been implemented in the gltf sample, the metallic texture is not used.
    //If it is not explicitly used in a shader, then it is discarded after compilation, and we get a root signature error.
    //For this reason, it temporarily used in such a way that it is not discarded after compilation.
    float4 metallicRoughness = float4(uMetallicRoughnessTexture.Sample(uSampler, input.In.UV).rgb, 1.0);
    metallicRoughness *= 0.00001;
    float4 baseColor = float4(uBaseColorTexture.Sample(uSampler, input.In.UV).rgb, 1.0);

    output.outColor = float4(baseColor.xyz, 1) + metallicRoughness;

    return output;
}