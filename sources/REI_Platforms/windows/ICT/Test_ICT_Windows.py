# Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
# Copyright (c) 2019-2022 Mykhailo Parfeniuk, Vladyslav Serhiienko.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
#
# This file contains modified code from the REI project source code
# (see https://github.com/Vi3LM/REI).

from Test_ICT_Include import *

def build_tests():
    projectPath = mkpath("../../sources/REI_Platforms/windows/VSProjects/Test_ICT.vcxproj")
    build_project(projectPath, "DebugD3D12", "Win32")
    build_project(projectPath, "DebugD3D12", "x64")
    build_project(projectPath, "DebugVulkan", "Win32")
    build_project(projectPath, "DebugVulkan", "x64")

def run_tests():
    mkdir(mkpath('../../build/Test_ICT/D3D12_x86'))
    mkdir(mkpath('../../build/Test_ICT/D3D12_x64'))
    mkdir(mkpath('../../build/Test_ICT/Vulkan_x86'))
    mkdir(mkpath('../../build/Test_ICT/Vulkan_x64'))
    shell(mkpath('../../build/Test_ICT_DebugD3D12_Win32/Test_ICT.exe'),mkpath('../../build/Test_ICT/D3D12_x86'))
    shell(mkpath('../../build/Test_ICT_DebugD3D12_x64/Test_ICT.exe'), mkpath('../../build/Test_ICT/D3D12_x64'))
    shell(mkpath('../../build/Test_ICT_DebugVulkan_Win32/Test_ICT.exe'), mkpath('../../build/Test_ICT/Vulkan_x86'))
    shell(mkpath('../../build/Test_ICT_DebugVulkan_x64/Test_ICT.exe'), mkpath('../../build/Test_ICT/Vulkan_x64'))
    print(filter_lines(mkpath('../../build/Test_ICT/D3D12_x86/Test_ICT.exe.log'), "( ERR\|)"))
    print(filter_lines(mkpath('../../build/Test_ICT/D3D12_x64/Test_ICT.exe.log'), "( ERR\|)"))
    print(filter_lines(mkpath('../../build/Test_ICT/Vulkan_x86/Test_ICT.exe.log'), "( ERR\|)"))
    print(filter_lines(mkpath('../../build/Test_ICT/Vulkan_x64/Test_ICT.exe.log'), "( ERR\|)"))
