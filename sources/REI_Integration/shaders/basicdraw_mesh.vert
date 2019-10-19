#version 450 core

struct Uniforms
{
    mat4 uMVP;
    vec3 uPxScalers;
    uint uColor;
};

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;

layout(push_constant) uniform uVertPC { layout(offset=0) uint idx; } vpc;

layout(set=0, binding = 0) buffer UniformData
{
    Uniforms a[];
} uUniforms;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out struct { vec4 Color; } Out;

void main()
{
    Out.Color = aColor;
    gl_Position = uUniforms.a[vpc.idx].uMVP * vec4(aPos, 1.0);
}
