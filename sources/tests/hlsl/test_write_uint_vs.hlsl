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
    REI_SPIRV([[vk::location(0)]]) float4 aPos : POSITION;
    REI_SPIRV([[vk::location(1)]]) uint aColor : COLOR;
};

struct PS_INPUT
{
    float4 CSPos: SV_Position;

    REI_SPIRV([[vk::location(0)]]) struct
    {
        uint Color;
    } Out : INPUT_DATA;
};

PS_INPUT main(VS_INPUT input)
{
    PS_INPUT output;

    output.CSPos = input.aPos;
    output.Out.Color = input.aColor;
    return output;
}
