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

PS_INPUT main(uint vertexId : SV_VertexID)
{
    PS_INPUT output;

    float2 vertexPos[3] = { float2(-1, -1) + 0.5, float2(-0.5, -1) + 0.5, float2(-0.75, 1) + 0.5 };

    output.CSPos = float4(vertexPos[vertexId], 0, 1);
    return output;
}
