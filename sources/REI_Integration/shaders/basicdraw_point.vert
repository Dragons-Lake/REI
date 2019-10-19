#version 450 core

layout(std140, column_major) uniform;

struct Point
{
    vec3 p;
    uint c;
};

struct Uniforms
{
    mat4 uMVP;
    vec3 uPxScalers;
    uint uColor;
};

layout(push_constant) uniform uVertPC { layout(offset=0) uint idx; uint type; } vpc;

layout(set=0, binding = 0) buffer UniformData
{
    Uniforms a[];
} uUniforms;

layout(set=1, binding = 0) buffer Stream0
{
    float a[];
} uStream0;

layout(set=1, binding = 1) buffer Stream1
{
    uint a[];
} uStream1;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; } Out;

vec4 Color32toVec4(uint c)
{
    vec4 r = vec4(
        float(c & 0xFF),
        float((c >> 8) &0xFF),
        float((c >> 16) &0xFF),
        float((c >> 24) &0xFF)
    );

    return r /255.0f;
}

struct PointData
{
    vec3 p;
    vec4 c;
};

PointData loadPoint(uint index)
{
    vec3 p = vec3(0);
    uint c = 0;

    if (vpc.type == 0)
    {
        p = vec3(uStream0.a[4 * index], uStream0.a[4 * index + 1], uStream0.a[4 * index + 2]);
        c =  floatBitsToUint(uStream0.a[4 * index + 3]);
    }
    else if(vpc.type == 1)
    {
        p = vec3(uStream0.a[3 * index], uStream0.a[3 * index + 1], uStream0.a[3 * index + 2]);
        c = uStream1.a[index];
    }
    else if(vpc.type == 2)
    {
        p = vec3(uStream0.a[3 * index], uStream0.a[3 * index + 1], uStream0.a[3 * index + 2]);
        c = uUniforms.a[vpc.idx].uColor;
    }

    return PointData(p, Color32toVec4(c));
}

void main()
{
    uint pointIndex = gl_VertexIndex / 6;

    PointData pt = loadPoint(pointIndex);

    vec4 p0 = uUniforms.a[vpc.idx].uMVP * vec4(pt.p, 1.0);

    vec2 offset = uUniforms.a[vpc.idx].uPxScalers.xy * -p0.w; //undo perspective

    uint vertexIndex = abs(2 - int(gl_VertexIndex - pointIndex * 6));
    p0.x += bool(vertexIndex & 1) ? -offset.x : offset.x;
    p0.y += bool(vertexIndex & 2) ? -offset.y : offset.y;

    gl_Position = p0;
    Out.Color = pt.c;
}