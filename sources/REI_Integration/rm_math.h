#pragma once

#ifndef RM_NO_INCLUDES
#include <math.h>
#endif

#ifdef __cplusplus
#define RM_VALUE(T, ...) T{ {__VA_ARGS__} }
#else
#define RM_VALUE(T, ...) (T){__VA_ARGS__}
#endif

#ifndef RM_CALL
#define RM_CALL static inline
#endif

#ifndef rm_scalar
#define rm_scalar float
#endif

#ifndef RM_NO_MATH_DEFINES
#define RM_E        ((rm_scalar) 2.71828182845904523536  )
#define RM_LOG2E    ((rm_scalar) 1.44269504088896340736  )
#define RM_LOG10E   ((rm_scalar) 0.434294481903251827651 )
#define RM_LN2      ((rm_scalar) 0.693147180559945309417 )
#define RM_LN10     ((rm_scalar) 2.30258509299404568402  )
#define RM_PI       ((rm_scalar) 3.14159265358979323846  )
#define RM_PI_2     ((rm_scalar) 1.57079632679489661923  )
#define RM_PI_4     ((rm_scalar) 0.785398163397448309616 )
#define RM_1_PI     ((rm_scalar) 0.318309886183790671538 )
#define RM_2_PI     ((rm_scalar) 0.636619772367581343076 )
#define RM_2_SQRTPI ((rm_scalar) 1.12837916709551257390  )
#define RM_SQRT2    ((rm_scalar) 1.41421356237309504880  )
#define RM_SQRT1_2  ((rm_scalar) 0.707106781186547524401 )
#endif

#ifndef RM_EPS
#define RM_EPS      ((rm_scalar) 1e-6 )
#endif

#ifndef rm_sinf
#define rm_sinf sinf
#endif

#ifndef rm_cosf
#define rm_cosf cosf
#endif

#ifndef rm_tanf
#define rm_tanf tanf
#endif

#ifndef rm_sqrtf
#define rm_sqrtf sqrtf
#endif

#ifndef rm_min
#define rm_min(A, B) (((A) < (B)) ? (A) : (B))
#endif

#ifndef rm_max
#define rm_max(A, B) (((A) > (B)) ? (A) : (B))
#endif

typedef union rm_vec2 {
    rm_scalar a[2];
    struct {
        rm_scalar x;
        rm_scalar y;
    };
} rm_vec2;

typedef union rm_vec3 {
    rm_scalar a[3];
    rm_vec2 xy;
    struct {
        rm_scalar x;
        rm_scalar y;
        rm_scalar z;
    };
    struct {
        rm_scalar unused;
        rm_vec2 yz;
    };
} rm_vec3;

typedef union rm_vec4 {
    rm_scalar a[4];
    rm_vec3 xyz;
    struct {
        rm_scalar x;
        rm_scalar y;
        rm_scalar z;
        rm_scalar w;
    };
    struct {
        rm_scalar unused0;
        rm_vec2 yz;
    };
    struct {
        rm_vec2 xy;
        rm_vec2 zw;
    };
    struct {
        rm_scalar unused1;
        rm_vec3 yzw;
    };
} rm_vec4;

typedef union rm_quat {
    rm_scalar a[4];
    rm_vec4 v;
    rm_vec3 n;
    struct {
        rm_scalar x;
        rm_scalar y;
        rm_scalar z;
        rm_scalar w;
    };
} rm_quat;

typedef union rm_mat4 {
    rm_scalar a[16];
    rm_scalar m[4][4];
    rm_vec4 c[4];
    struct {
        rm_vec4 c0;
        rm_vec4 c1;
        rm_vec4 c2;
        rm_vec4 c3;
    };
} rm_mat4;

#ifdef __cplusplus
static_assert(sizeof(rm_vec2) == (sizeof(rm_scalar)*2), "Caught size mismatch.");
static_assert(sizeof(rm_vec3) == (sizeof(rm_scalar)*3), "Caught size mismatch.");
static_assert(sizeof(rm_vec4) == (sizeof(rm_scalar)*4), "Caught size mismatch.");
static_assert(sizeof(rm_quat) == (sizeof(rm_scalar)*4), "Caught size mismatch.");
static_assert(sizeof(rm_mat4) == (sizeof(rm_scalar)*16), "Caught size mismatch.");
#endif

//
// rm_vec2
//

RM_CALL rm_vec2 rm_vec2_splat(rm_scalar s) { return RM_VALUE(rm_vec2, s,s); }
RM_CALL rm_vec2 rm_vec2_add(rm_vec2 a, rm_vec2 b) { return RM_VALUE(rm_vec2, a.x + b.x, a.y + b.y); }
RM_CALL rm_vec2 rm_vec2_sub(rm_vec2 a, rm_vec2 b) { return RM_VALUE(rm_vec2, a.x - b.x, a.y - b.y); }
RM_CALL rm_vec2 rm_vec2_mul(rm_vec2 a, rm_vec2 b) { return RM_VALUE(rm_vec2, a.x * b.x, a.y * b.y); }
RM_CALL rm_vec2 rm_vec2_div(rm_vec2 a, rm_vec2 b) { return RM_VALUE(rm_vec2, a.x / b.x, a.y / b.y); }
RM_CALL rm_vec2 rm_vec2_mad(rm_vec2 m, rm_vec2 a, rm_vec2 b) { return RM_VALUE(rm_vec2, m.x*a.x + b.x, m.y*a.y + b.y); }
RM_CALL rm_vec2 rm_vec2_scale(rm_vec2 a, rm_scalar s) { return RM_VALUE(rm_vec2, a.x * s, a.y * s); }
RM_CALL rm_vec2 rm_vec2_min(rm_vec2 a, rm_vec2 b) { return RM_VALUE(rm_vec2, rm_min(a.x, b.x), rm_min(a.y, b.y)); }
RM_CALL rm_vec2 rm_vec2_max(rm_vec2 a, rm_vec2 b) { return RM_VALUE(rm_vec2, rm_max(a.x, b.x), rm_max(a.y, b.y)); }
RM_CALL rm_scalar rm_vec2_dot(rm_vec2 a, rm_vec2 b) { return a.x * b.x + a.y * b.y; }
RM_CALL rm_scalar rm_vec2_len(rm_vec2 a) { return rm_sqrtf(rm_vec2_dot(a, a)); }
RM_CALL rm_vec2 rm_vec2_norm(rm_vec2 a) { return rm_vec2_scale(a, 1.0f / rm_vec2_len(a)); }
RM_CALL rm_vec2 rm_vec2_rcp(rm_vec2 a) { return RM_VALUE(rm_vec2, 1.0f/a.x, 1.0f/a.y); }
RM_CALL rm_vec2 rm_vec2_neg(rm_vec2 a) { return RM_VALUE(rm_vec2, -a.x, -a.y); }

//
// rm_vec3
//

RM_CALL rm_vec3 rm_vec3_splat(rm_scalar s) { return RM_VALUE(rm_vec3, s,s,s); }
RM_CALL rm_vec3 rm_vec3_add(rm_vec3 a, rm_vec3 b) { return RM_VALUE(rm_vec3, a.x + b.x, a.y + b.y, a.z + b.z); }
RM_CALL rm_vec3 rm_vec3_sub(rm_vec3 a, rm_vec3 b) { return RM_VALUE(rm_vec3, a.x - b.x, a.y - b.y, a.z - b.z); }
RM_CALL rm_vec3 rm_vec3_mul(rm_vec3 a, rm_vec3 b) { return RM_VALUE(rm_vec3, a.x * b.x, a.y * b.y, a.z * b.z); }
RM_CALL rm_vec3 rm_vec3_div(rm_vec3 a, rm_vec3 b) { return RM_VALUE(rm_vec3, a.x / b.x, a.y / b.y, a.z / b.z); }
RM_CALL rm_vec3 rm_vec3_mad(rm_vec3 m, rm_vec3 a, rm_vec3 b) { return RM_VALUE(rm_vec3, m.x*a.x + b.x, m.y*a.y + b.y, m.z*a.z + b.z); }
RM_CALL rm_vec3 rm_vec3_scale(rm_vec3 a, rm_scalar s) { return RM_VALUE(rm_vec3, a.x * s, a.y * s, a.z * s); }
RM_CALL rm_vec3 rm_vec3_min(rm_vec3 a, rm_vec3 b) { return RM_VALUE(rm_vec3, rm_min(a.x, b.x), rm_min(a.y, b.y), rm_min(a.z, b.z)); }
RM_CALL rm_vec3 rm_vec3_max(rm_vec3 a, rm_vec3 b) { return RM_VALUE(rm_vec3, rm_max(a.x, b.x), rm_max(a.y, b.y), rm_max(a.z, b.z)); }
RM_CALL rm_scalar rm_vec3_dot(rm_vec3 a, rm_vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
RM_CALL rm_scalar rm_vec3_len(rm_vec3 a) { return rm_sqrtf(rm_vec3_dot(a, a)); }
RM_CALL rm_vec3 rm_vec3_norm(rm_vec3 a) { return rm_vec3_scale(a, 1.0f / rm_vec3_len(a)); }
RM_CALL rm_vec3 rm_vec3_rcp(rm_vec3 a) { return RM_VALUE(rm_vec3, 1.0f / a.x, 1.0f / a.y, 1.0f / a.z); }
RM_CALL rm_vec3 rm_vec3_neg(rm_vec3 a) { return RM_VALUE(rm_vec3, -a.x, -a.y, -a.z); }

RM_CALL rm_vec3 rm_vec3_cross(rm_vec3 a, rm_vec3 b) {
    return RM_VALUE(rm_vec3,
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x);
}

RM_CALL rm_vec3 rm_vec3_reflect(rm_vec3 v, rm_vec3 n) {
    const rm_scalar p = 2.f * rm_vec3_dot(v, n);
    // clang-format off
    return RM_VALUE(rm_vec3,
        v.x - p * n.x,
        v.y - p * n.y,
        v.z - p * n.z);
    // clang-format on
}

//
// rm_vec4
//

RM_CALL rm_vec4 rm_vec4_splat(rm_scalar s) { return RM_VALUE(rm_vec4, s,s,s,s); }
RM_CALL rm_vec4 rm_vec4_add(rm_vec4 a, rm_vec4 b) { return RM_VALUE(rm_vec4, a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w); }
RM_CALL rm_vec4 rm_vec4_sub(rm_vec4 a, rm_vec4 b) { return RM_VALUE(rm_vec4, a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w); }
RM_CALL rm_vec4 rm_vec4_mul(rm_vec4 a, rm_vec4 b) { return RM_VALUE(rm_vec4, a.x * b.x, a.y * b.y, a.z * b.z, a.w * b.w); }
RM_CALL rm_vec4 rm_vec4_div(rm_vec4 a, rm_vec4 b) { return RM_VALUE(rm_vec4, a.x / b.x, a.y / b.y, a.z / b.z, a.w / b.w); }
RM_CALL rm_vec4 rm_vec4_mad(rm_vec4 m, rm_vec4 a, rm_vec4 b) { return RM_VALUE(rm_vec4, m.x*a.x + b.x, m.y*a.y + b.y, m.z*a.z + b.z, m.w*a.w + b.w); }
RM_CALL rm_vec4 rm_vec4_scale(rm_vec4 a, rm_scalar s) { return RM_VALUE(rm_vec4, a.x * s, a.y * s, a.z * s, a.w * s); }
RM_CALL rm_vec4 rm_vec4_min(rm_vec4 a, rm_vec4 b) { return RM_VALUE(rm_vec4, rm_min(a.x, b.x), rm_min(a.y, b.y), rm_min(a.z, b.z), rm_min(a.w, b.w)); }
RM_CALL rm_vec4 rm_vec4_max(rm_vec4 a, rm_vec4 b) { return RM_VALUE(rm_vec4, rm_max(a.x, b.x), rm_max(a.y, b.y), rm_max(a.z, b.z), rm_max(a.w, b.w)); }
RM_CALL rm_scalar rm_vec4_dot(rm_vec4 a, rm_vec4 b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
RM_CALL rm_scalar rm_vec4_len(rm_vec4 a) { return rm_sqrtf(rm_vec4_dot(a, a)); }
RM_CALL rm_vec4 rm_vec4_norm(rm_vec4 a) { return rm_vec4_scale(a, 1.0f / rm_vec4_len(a)); }
RM_CALL rm_vec4 rm_vec4_rcp(rm_vec4 a) { return RM_VALUE(rm_vec4, 1.0f/a.x, 1.0f/a.y, 1.0f/a.z, 1.0f/a.w); }
RM_CALL rm_vec4 rm_vec4_neg(rm_vec4 a) { return RM_VALUE(rm_vec4, -a.x, -a.y, -a.z, -a.w); }

RM_CALL rm_vec4 rm_vec4_reflect(rm_vec4 v, rm_vec4 n) {
    const rm_scalar p = 2.f * rm_vec4_dot(v, n);
    // clang-format off
    return RM_VALUE(rm_vec4,
        v.x - p * n.x,
        v.y - p * n.y,
        v.z - p * n.z,
        v.w - p * n.w);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_from_columns(rm_vec4 c0, rm_vec4 c1, rm_vec4 c2, rm_vec4 c3) {
    // clang-format off
    return RM_VALUE(rm_mat4,
        c0.x, c0.y, c0.z, c0.w,
        c1.x, c1.y, c1.z, c1.w,
        c2.x, c2.y, c2.z, c2.w,
        c3.x, c3.y, c3.z, c3.w);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_identity() {
    // clang-format off
    return RM_VALUE(rm_mat4,
        1,0,0,0,
        0,1,0,0,
        0,0,1,0,
        0,0,0,1);
    // clang-format on
}

RM_CALL rm_vec4 rm_mat4_row(rm_mat4 M, int i) {
    return RM_VALUE(rm_vec4,
        M.c0.a[i],
        M.c1.a[i],
        M.c2.a[i],
        M.c3.a[i]);
}

RM_CALL rm_vec4 rm_mat4_col(rm_mat4 M, int i) {
    return M.c[i];
}

RM_CALL rm_mat4 rm_mat4_transpose(rm_mat4 M) {
    return rm_mat4_from_columns(
        rm_mat4_row(M, 0),
        rm_mat4_row(M, 1),
        rm_mat4_row(M, 2),
        rm_mat4_row(M, 3));
}

RM_CALL rm_mat4 rm_mat4_add(rm_mat4 A, rm_mat4 B) {
    return rm_mat4_from_columns(
        rm_vec4_add(A.c0, B.c0),
        rm_vec4_add(A.c1, B.c1),
        rm_vec4_add(A.c2, B.c2),
        rm_vec4_add(A.c3, B.c3));
}

RM_CALL rm_mat4 rm_mat4_sub(rm_mat4 A, rm_mat4 B) {
    return rm_mat4_from_columns(
        rm_vec4_sub(A.c0, B.c0),
        rm_vec4_sub(A.c1, B.c1),
        rm_vec4_sub(A.c2, B.c2),
        rm_vec4_sub(A.c3, B.c3));
}

RM_CALL rm_mat4 rm_mat4_scaling_iso(rm_scalar s) {
    // clang-format off
    return RM_VALUE(rm_mat4,
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0,
        0, 0, 0, 1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_scaling(rm_vec3 s) {
    // clang-format off
    return RM_VALUE(rm_mat4,
        s.x, 0,   0,   0,
        0,   s.y, 0,   0,
        0,   0,   s.z, 0,
        0,   0,   0,   1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_mul(rm_mat4 A, rm_mat4 B) {
    // https://github.com/g-truc/glm/blob/master/glm/detail/type_mat4x4.inl#L630

    const rm_vec4 a0 = A.c0;
    const rm_vec4 a1 = A.c1;
    const rm_vec4 a2 = A.c2;
    const rm_vec4 a3 = A.c3;

    const rm_vec4 b0 = B.c0;
    const rm_vec4 b1 = B.c1;
    const rm_vec4 b2 = B.c2;
    const rm_vec4 b3 = B.c3;

    // clang-format off
    return rm_mat4_from_columns(
        rm_vec4_mad(a0, rm_vec4_splat(b0.x), rm_vec4_mad(a1, rm_vec4_splat(b0.y), rm_vec4_mad(a2, rm_vec4_splat(b0.z), rm_vec4_mul(a3, rm_vec4_splat(b0.w))))),
        rm_vec4_mad(a0, rm_vec4_splat(b1.x), rm_vec4_mad(a1, rm_vec4_splat(b1.y), rm_vec4_mad(a2, rm_vec4_splat(b1.z), rm_vec4_mul(a3, rm_vec4_splat(b1.w))))),
        rm_vec4_mad(a0, rm_vec4_splat(b2.x), rm_vec4_mad(a1, rm_vec4_splat(b2.y), rm_vec4_mad(a2, rm_vec4_splat(b2.z), rm_vec4_mul(a3, rm_vec4_splat(b2.w))))),
        rm_vec4_mad(a0, rm_vec4_splat(b3.x), rm_vec4_mad(a1, rm_vec4_splat(b3.y), rm_vec4_mad(a2, rm_vec4_splat(b3.z), rm_vec4_mul(a3, rm_vec4_splat(b3.w))))));
    // clang-format on
}

RM_CALL rm_vec4 rm_mat4_mul_vec4(rm_mat4 M, rm_vec4 v) {
    // clang-format off
    return rm_vec4_add(
        rm_vec4_scale(M.c0, v.x),
        rm_vec4_add(
            rm_vec4_scale(M.c1, v.y),
            rm_vec4_add(
                rm_vec4_scale(M.c2, v.z),
                rm_vec4_scale(M.c3, v.w))));
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_translation(rm_vec3 t) {
    // clang-format off
    return RM_VALUE(rm_mat4,
          1,   0,   0, 0,
          0,   1,   0, 0,
          0,   0,   1, 0,
        t.x, t.y, t.z, 1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_from_vec3_mul_outer(rm_vec3 a, rm_vec3 b) {
    rm_mat4 R = RM_VALUE(rm_mat4, 0);

    R.c0.xyz = rm_vec3_scale(b, a.x);
    R.c1.xyz = rm_vec3_scale(b, a.y);
    R.c2.xyz = rm_vec3_scale(b, a.z);

    return R;
}

RM_CALL rm_mat4 rm_mat4_axis_rotation(rm_vec3 axis, rm_scalar angle) {
    rm_scalar s;
    rm_scalar c;
    rm_mat4 T;
    rm_mat4 S;
    rm_mat4 C;

    rm_scalar u_len = rm_vec3_len(axis);
    if (u_len < RM_EPS) { return rm_mat4_identity(); }

    s = rm_sinf(angle);
    c = rm_cosf(angle);

    axis = rm_vec3_scale(axis, 1.0f/u_len);
    T = rm_mat4_from_vec3_mul_outer(axis, axis);

    // clang-format off
    S = RM_VALUE(rm_mat4,
          0,       axis.z, -axis.y, 0,
         -axis.z,  0,       axis.x, 0,
          axis.y, -axis.x,  0,      0,
          0,       0,       0,      0);
    // clang-format on

    S = rm_mat4_mul(S, rm_mat4_scaling_iso(s));

    C = rm_mat4_identity();
    C = rm_mat4_sub(C, T);
    C = rm_mat4_mul(C, rm_mat4_scaling_iso(c));

    T = rm_mat4_add(T, C);
    T = rm_mat4_add(T, S);

    T.c3.w = 1;
    return T;
}

RM_CALL rm_mat4 rm_mat4_rotation_x(rm_scalar angle) {
    const rm_scalar s = rm_sinf(angle);
    const rm_scalar c = rm_cosf(angle);

    // clang-format off
    return RM_VALUE(rm_mat4,
        1,  0, 0, 0,
        0,  c, s, 0,
        0, -s, c, 0,
        0,  0, 0, 1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_rotation_y(rm_scalar angle) {
    const rm_scalar s = rm_sinf(angle);
    const rm_scalar c = rm_cosf(angle);

    // clang-format off
    return RM_VALUE(rm_mat4,
         c, 0, s, 0,
         0, 1, 0, 0,
        -s, 0, c, 0,
         0, 0, 0, 1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_rotation_z(rm_scalar angle) {
    const rm_scalar s = rm_sinf(angle);
    const rm_scalar c = rm_cosf(angle);

    // clang-format off
    return RM_VALUE(rm_mat4,
         c, s, 0, 0,
        -s, c, 0, 0,
         0, 0, 1, 0,
         0, 0, 0, 1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_invert_det(rm_scalar* det, rm_mat4 M) {
    rm_scalar s[6];
    rm_scalar c[6];
    rm_scalar idet;

    s[0] = M.m[0][0]*M.m[1][1] - M.m[1][0]*M.m[0][1];
    s[1] = M.m[0][0]*M.m[1][2] - M.m[1][0]*M.m[0][2];
    s[2] = M.m[0][0]*M.m[1][3] - M.m[1][0]*M.m[0][3];
    s[3] = M.m[0][1]*M.m[1][2] - M.m[1][1]*M.m[0][2];
    s[4] = M.m[0][1]*M.m[1][3] - M.m[1][1]*M.m[0][3];
    s[5] = M.m[0][2]*M.m[1][3] - M.m[1][2]*M.m[0][3];

    c[0] = M.m[2][0]*M.m[3][1] - M.m[3][0]*M.m[2][1];
    c[1] = M.m[2][0]*M.m[3][2] - M.m[3][0]*M.m[2][2];
    c[2] = M.m[2][0]*M.m[3][3] - M.m[3][0]*M.m[2][3];
    c[3] = M.m[2][1]*M.m[3][2] - M.m[3][1]*M.m[2][2];
    c[4] = M.m[2][1]*M.m[3][3] - M.m[3][1]*M.m[2][3];
    c[5] = M.m[2][2]*M.m[3][3] - M.m[3][2]*M.m[2][3];
    
    /* Assumes it is invertible */
    *det = s[0]*c[5]-s[1]*c[4]+s[2]*c[3]+s[3]*c[2]-s[4]*c[1]+s[5]*c[0];
    idet = 1.0f/ *det;

    // clang-format off
    return RM_VALUE(rm_mat4,
        ( M.m[1][1] * c[5] - M.m[1][2] * c[4] + M.m[1][3] * c[3]) * idet,
        (-M.m[0][1] * c[5] + M.m[0][2] * c[4] - M.m[0][3] * c[3]) * idet,
        ( M.m[3][1] * s[5] - M.m[3][2] * s[4] + M.m[3][3] * s[3]) * idet,
        (-M.m[2][1] * s[5] + M.m[2][2] * s[4] - M.m[2][3] * s[3]) * idet,
        (-M.m[1][0] * c[5] + M.m[1][2] * c[2] - M.m[1][3] * c[1]) * idet,
        ( M.m[0][0] * c[5] - M.m[0][2] * c[2] + M.m[0][3] * c[1]) * idet,
        (-M.m[3][0] * s[5] + M.m[3][2] * s[2] - M.m[3][3] * s[1]) * idet,
        ( M.m[2][0] * s[5] - M.m[2][2] * s[2] + M.m[2][3] * s[1]) * idet,
        ( M.m[1][0] * c[4] - M.m[1][1] * c[2] + M.m[1][3] * c[0]) * idet,
        (-M.m[0][0] * c[4] + M.m[0][1] * c[2] - M.m[0][3] * c[0]) * idet,
        ( M.m[3][0] * s[4] - M.m[3][1] * s[2] + M.m[3][3] * s[0]) * idet,
        (-M.m[2][0] * s[4] + M.m[2][1] * s[2] - M.m[2][3] * s[0]) * idet,
        (-M.m[1][0] * c[3] + M.m[1][1] * c[1] - M.m[1][2] * c[0]) * idet,
        ( M.m[0][0] * c[3] - M.m[0][1] * c[1] + M.m[0][2] * c[0]) * idet,
        (-M.m[3][0] * s[3] + M.m[3][1] * s[1] - M.m[3][2] * s[0]) * idet,
        ( M.m[2][0] * s[3] - M.m[2][1] * s[1] + M.m[2][2] * s[0]) * idet);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_invert(rm_mat4 M) {
    rm_scalar unused_det;
    return rm_mat4_invert_det(&unused_det, M);
}

RM_CALL rm_mat4 rm_mat4_orthonormalize(rm_mat4 M) {
    rm_scalar s;
    rm_vec3 h;

    M.c2.xyz = rm_vec3_norm(M.c2.xyz);

    s = rm_vec3_dot(M.c1.xyz, M.c2.xyz);
    h = rm_vec3_scale(M.c2.xyz, s);
    M.c1.xyz = rm_vec3_sub(M.c1.xyz, h);
    M.c2.xyz = rm_vec3_norm(M.c2.xyz);

    s = rm_vec3_dot(M.c1.xyz, M.c2.xyz);
    h = rm_vec3_scale(M.c2.xyz, s);
    M.c1.xyz = rm_vec3_sub(M.c1.xyz, h);
    M.c1.xyz = rm_vec3_norm(M.c1.xyz);

    s = rm_vec3_dot(M.c0.xyz, M.c1.xyz);
    h = rm_vec3_scale(M.c1.xyz, s);
    M.c0.xyz = rm_vec3_sub(M.c0.xyz, h);
    M.c0.xyz = rm_vec3_norm(M.c0.xyz);

    return M;
}

RM_CALL rm_mat4 rm_mat4_frustum_gl(rm_scalar l, rm_scalar r, rm_scalar b, rm_scalar t, rm_scalar n, rm_scalar f) {
    // clang-format off
    return RM_VALUE(rm_mat4,
        2.0f*n/(r-l), 0,             0,                0,
        0,            2.0f*n/(t-b),  0,                0,
        (r+l)/(r-l),  (t+b)/(t-b),  -(f+n)/(f-n),     -1,
        0,            0,            -2.f*(f*n)/(f-n),  0);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_ortho_gl(rm_scalar l, rm_scalar r, rm_scalar b, rm_scalar t, rm_scalar n, rm_scalar f) {
    // clang-format off
    return RM_VALUE(rm_mat4,
        2.f/(r-l),     0,            0,           0,
        0,             2.f/(t-b),    0,           0,
        0,             0,           -2.f/(f-n),   0,
        -(r+l)/(r-l), -(t+b)/(t-b), -(f+n)/(f-n), 1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_perspective_inf_vk(rm_scalar y_fov, rm_scalar aspect, rm_scalar n) {
    const rm_scalar a = 1.f / rm_tanf(y_fov / 2.f);

    // clang-format off
    return RM_VALUE(rm_mat4,
        a / aspect, 0,  0, 0,
        0,          a,  0, 0,
        0,          0,  1, 1,
        0,          0, -n, 0);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_perspective_inf_invz_vk(rm_scalar y_fov, rm_scalar aspect, rm_scalar n) {
    const rm_scalar a = 1.f / rm_tanf(y_fov / 2.f);

    // clang-format off
    return RM_VALUE(rm_mat4,
        a / aspect,  0, 0, 0,
        0,          -a, 0, 0,
        0,           0, 0, 1,
        0,           0, n, 0);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_look_at_lhs(rm_vec3 eye, rm_vec3 center, rm_vec3 up) {
    const rm_vec3 f = rm_vec3_norm(rm_vec3_sub(center, eye));
    const rm_vec3 s = rm_vec3_norm(rm_vec3_cross(f, up));
    const rm_vec3 t = rm_vec3_cross(s, f);

    // clang-format off
    return rm_mat4_mul(
        RM_VALUE(rm_mat4,
            -s.x, t.x, f.x, 0,
            -s.y, t.y, f.y, 0,
            -s.z, t.z, f.z, 0,
             0,   0,   0,   1),
        rm_mat4_translation(
            rm_vec3_neg(eye)));
    // clang-format on
}

RM_CALL rm_mat4 mat4x4_look_at_rhs(rm_vec3 eye, rm_vec3 center, rm_vec3 up) {
    const rm_vec3 f = rm_vec3_norm(rm_vec3_sub(center, eye));
    const rm_vec3 s = rm_vec3_norm(rm_vec3_cross(f, up));
    const rm_vec3 t = rm_vec3_cross(s, f);

    // clang-format off
    return rm_mat4_mul(
        RM_VALUE(rm_mat4,
            s.x, t.x, f.x, 0,
            s.y, t.y, f.y, 0,
            s.z, t.z, f.z, 0,
            0,   0,   0,   1),
        rm_mat4_translation(
            rm_vec3_neg(eye)));
    // clang-format on
}

//
// rm_quat
//

RM_CALL rm_quat rm_quat_identity() {
    return RM_VALUE(rm_quat, 0,0,0,1);
}

RM_CALL rm_quat rm_quat_add(rm_quat a, rm_quat b) {
    a.v = rm_vec4_add(a.v, b.v);
    return a;
}

RM_CALL rm_quat rm_quat_sub(rm_quat a, rm_quat b) {
    a.v = rm_vec4_sub(a.v, b.v);
    return a;
}

RM_CALL rm_quat rm_quat_mul(rm_quat p, rm_quat q) {
    rm_quat r;
    rm_vec3 w;

    r.n = rm_vec3_cross(p.n, q.n);
    w = rm_vec3_scale(p.n, q.w);
    r.n = rm_vec3_add(r.n, w);
    w = rm_vec3_scale(q.n, p.w);
    r.n = rm_vec3_add(r.n, w);
    r.w = p.w * q.w - rm_vec3_dot(p.n, q.n);

    return r;
}

RM_CALL rm_quat rm_quat_scale(rm_quat a, rm_scalar s) {
    a.v = rm_vec4_scale(a.v, s);
    return a;
}

RM_CALL rm_scalar rm_quat_dot(rm_quat a, rm_quat b) {
    return rm_vec4_dot(a.v, b.v);
}

RM_CALL rm_quat rm_quat_conj(rm_quat q) {
    q.n = rm_vec3_scale(q.n, -1);
    return q;
}

RM_CALL rm_quat rm_quat_axis_rotation(rm_vec3 axis, rm_scalar angle) {
    rm_quat r;
    r.n = rm_vec3_scale(axis, rm_sinf(angle*0.5f));
    r.w = rm_cosf(angle*0.5f);
    return r;
}

RM_CALL rm_quat rm_quat_norm(rm_quat q) {
    q.v = rm_vec4_norm(q.v);
    return q;
}

RM_CALL rm_vec3 rm_quat_mul_vec3(rm_quat q, rm_vec3 v) {
    /* Method by Fabian 'ryg' Giessen (of Farbrausch)
     * t = 2 * cross(q.xyz, v)
     * v' = v + q.w * t + cross(q.xyz, t) */

    rm_vec3 t;
    rm_vec3 u;
    rm_vec3 r;

    t = rm_vec3_cross(q.n, v);
    t = rm_vec3_scale(t, 2);

    u = rm_vec3_cross(q.n, t);
    t = rm_vec3_scale(t, q.w);

    r = rm_vec3_add(v, t);
    r = rm_vec3_add(r, u);

    return r;
}

RM_CALL rm_mat4 rm_mat4_from_quat(rm_quat q) {
    // TODO(vserhiienko): Verify
    // https://github.com/datenwolf/linmath.h/blob/master/linmath.h

    const rm_scalar a = q.w;
    const rm_scalar b = q.x;
    const rm_scalar c = q.y;
    const rm_scalar d = q.z;
    const rm_scalar a2 = a*a;
    const rm_scalar b2 = b*b;
    const rm_scalar c2 = c*c;
    const rm_scalar d2 = d*d;

    // clang-format off
    return RM_VALUE(rm_mat4,
        a2 + b2 - c2 - d2, 2.f*(b*c + a*d),   2.f*(b*d - a*c),   0,
        2.f*(b*c - a*d),   a2 - b2 + c2 - d2, 2.f*(c*d + a*b),   0,
        2.f*(b*d + a*c),   2.f*(c*d - a*b),   a2 - b2 - c2 + d2, 0,
        0,                 0,                 0,                 1);
    // clang-format on
}

RM_CALL rm_mat4 rm_mat4_ortho_mul_quat(rm_mat4 O, rm_quat q) {
    /*  XXX: The way this is written only works for othogonal matrices. */
    /* TODO: Take care of non-orthogonal case. */
    rm_mat4 R = RM_VALUE(rm_mat4, 0);

    R.c0.xyz = rm_quat_mul_vec3(q, O.c0.xyz);
    R.c1.xyz = rm_quat_mul_vec3(q, O.c1.xyz);
    R.c2.xyz = rm_quat_mul_vec3(q, O.c2.xyz);

    return R;
}

RM_CALL rm_quat rm_quat_from_mat4(rm_mat4 M) {
    // https://github.com/HandmadeMath/Handmade-Math/pull/103:
    // https://d3cw3dd2w32x2b.cloudfront.net/wp-content/uploads/2015/01/matrix-to-quat.pdf

    float t;
    float r;

    if (M.m[2][2] < 0.0f) {
        if (M.m[0][0] > M.m[1][1]) {
            t = 1 + M.m[0][0] - M.m[1][1] - M.m[2][2];
            r = 0.5f * rm_sqrtf(t);
            return RM_VALUE(rm_quat,
                r * t,
                r * (M.m[0][1] + M.m[1][0]),
                r * (M.m[2][0] + M.m[0][2]),
                r * (M.m[1][2] - M.m[2][1]));
        }

        t = 1 - M.m[0][0] + M.m[1][1] - M.m[2][2];
        r = 0.5f * rm_sqrtf(t);
        return RM_VALUE(rm_quat,
            r * (M.m[0][1] + M.m[1][0]),
            r * t,
            r * (M.m[1][2] + M.m[2][1]),
            r * (M.m[2][0] - M.m[0][2]));
    }

    if (M.m[0][0] < -M.m[1][1]) {
        t = 1 - M.m[0][0] - M.m[1][1] + M.m[2][2];
        r = 0.5f * rm_sqrtf(t);
        return RM_VALUE(rm_quat,
            r * (M.m[2][0] + M.m[0][2]),
            r * (M.m[1][2] + M.m[2][1]),
            r * t,
            r * (M.m[0][1] - M.m[1][0]));
    }

    t = 1 + M.m[0][0] + M.m[1][1] + M.m[2][2];
    r = 0.5f * rm_sqrtf(t);
    return RM_VALUE(rm_quat,
        r * (M.m[1][2] - M.m[2][1]),
        r * (M.m[2][0] - M.m[0][2]),
        r * (M.m[0][1] - M.m[1][0]),
        r * t);
}
