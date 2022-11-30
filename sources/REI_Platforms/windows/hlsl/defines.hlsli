/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 */

#ifndef _DEFINES_HLSLI_
#define _DEFINES_HLSLI_

#ifdef __spirv__
#define REI_SPIRV(x) x
#else
#define REI_SPIRV(x) 
#endif

#define REI_REGISTER(...) : register(__VA_ARGS__)

#define REI_DECLARE_PUSH_CONSTANT(name, structType, registerIndex, spaceIndex) \
REI_SPIRV([[vk::push_constant]])                                               \
ConstantBuffer<structType> name REI_REGISTER(b##registerIndex, space##spaceIndex)

#define POSITION POSITION0
#define TEXCOORD TEXCOORD0
#define COLOR COLOR0

#endif