/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 * Copyright (c) 2019-2022 Mykhailo Parfeniuk, Vladyslav Serhiienko.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 *
 * This file contains modified code from the REI project source code
 * (see https://github.com/Vi3LM/REI).
 */

#include "REI_Sample/sample.h"
#include <SDL2/SDL_syswm.h>

extern const char* dir_paths[DIRECTORY_COUNT];

bool dir_exists(const char* dir)
{
    struct _stat fi;
    return dir && (_stat(dir, &fi) == 0) && ((fi.st_mode & _S_IFDIR) != 0);
}

void init_directories()
{
    if (dir_exists("data"))
    {
        dir_paths[DIRECTORY_DATA] = "data";
    }
    else if (dir_exists("../data"))
    {
        dir_paths[DIRECTORY_DATA] = "../data";
    }
    else if (dir_exists("../../data"))
    {
        dir_paths[DIRECTORY_DATA] = "../../data";
    }
    else if (dir_exists("../../../data"))
    {
        dir_paths[DIRECTORY_DATA] = "../../../data";
    }
    else if (dir_exists("../../../../data"))
    {
        dir_paths[DIRECTORY_DATA] = "../../../../data";
    }
    dir_paths[DIRECTORY_LOG] = "\0";
}

REI_WindowHandle getPlatformWindowHandle(const SDL_SysWMinfo* inSysWMinfo)
{
    return { inSysWMinfo->info.win.window };
}

void getPlatformWindowProperties(int32_t* outWidth, int32_t* outHeight, uint32_t* outFlags)
{
    *outWidth = 1280;
    *outHeight = 720;
    *outFlags = SDL_WINDOW_RESIZABLE;
}

const char* getPlatformImguiIniFilename()
{
    static char imguiIniFilename[64] = {};
    if (!imguiIniFilename[0])
        snprintf(
            imguiIniFilename, sizeof(imguiIniFilename) / sizeof(imguiIniFilename[0]), "%s%s", dir_paths[DIRECTORY_LOG],
            "imgui.ini");

    return imguiIniFilename;
}
const char* getPlatformImguiLogFilename()
{
    static char imguiLogFilename[64] = {};
    if (!imguiLogFilename[0])
        snprintf(
            imguiLogFilename, sizeof(imguiLogFilename) / sizeof(imguiLogFilename[0]), "%s%s", dir_paths[DIRECTORY_LOG],
            "imgui_log.txt");

    return imguiLogFilename;
}