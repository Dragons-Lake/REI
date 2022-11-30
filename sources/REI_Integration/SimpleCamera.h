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

#pragma once

#include "REI_Integration/rm_math.h"
#include "REI/Common.h"

typedef enum SimpleCameraProjType {
    SimpleCameraProjInfiniteVulkan = 0,
    SimpleCameraProjInfiniteInversedVulkan,
} SimpleCameraProjType;

typedef struct SimpleCameraProjDesc {
    SimpleCameraProjType proj_type;
    float y_fov;
    float aspect;
    float z_near;
} SimpleCameraProjDesc;

typedef struct SimpleCamera {
    rm_mat4 proj;
    rm_quat q;
    rm_vec3 p;
    float yaw;
    float pitch;
} SimpleCamera;

static void SimpleCamera_init(SimpleCamera* cam, SimpleCameraProjDesc projDesc) {
    switch (projDesc.proj_type) {
    case SimpleCameraProjInfiniteVulkan:
        cam->proj = rm_mat4_perspective_inf_vk(projDesc.y_fov, projDesc.aspect, projDesc.z_near);
        break;
    default:
        REI_ASSERT(projDesc.proj_type == SimpleCameraProjInfiniteInversedVulkan);
        cam->proj = rm_mat4_perspective_inf_invz_vk(projDesc.y_fov, projDesc.aspect, projDesc.z_near);
        break;
    }

    cam->q = rm_quat_identity();
    cam->p = RM_VALUE(rm_vec3, 0,0,0);
    cam->yaw = 0;
    cam->pitch = 0;
}

static void SimpleCamera_rotate(SimpleCamera* cam, float rot_y, float rot_x) {
    rm_quat ry;
    rm_quat rx;

    cam->yaw = fmodf(cam->yaw + rot_y, 2.0f * RM_PI);
    cam->pitch = rm_max(-RM_PI_2, rm_min(cam->pitch + rot_x, RM_PI_2));

    ry = rm_quat_axis_rotation(RM_VALUE(rm_vec3, 0,1,0), -cam->yaw);
    rx = rm_quat_axis_rotation(RM_VALUE(rm_vec3, 1,0,0), -cam->pitch);
    cam->q = rm_quat_mul(rx, ry);
}

static void SimpleCamera_move(SimpleCamera* cam, float dx, float dy, float dz) {
    rm_quat qc = rm_quat_conj(cam->q);
    rm_vec3 dp = RM_VALUE(rm_vec3, -dx,-dy,-dz);

    dp = rm_quat_mul_vec3(qc, dp);
    cam->p = rm_vec3_add(cam->p, dp);
}

static rm_mat4 SimpleCamera_buildView(SimpleCamera const* cam) {
    rm_mat4 mv = rm_mat4_from_quat(cam->q);
    rm_mat4 mt = rm_mat4_translation(cam->p);
    return rm_mat4_mul(mv, mt);
}

static rm_mat4 SimpleCamera_buildViewProj(SimpleCamera const* cam) {
    rm_mat4 vp = SimpleCamera_buildView(cam);
    return rm_mat4_mul(cam->proj, vp);
}
