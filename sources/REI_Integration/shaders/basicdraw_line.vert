#version 450 core

layout(std140, column_major) uniform;

struct Line
{
    vec3  p0;
    float w;
    vec3  p1;
    uint  c;
};

struct Uniforms
{
    mat4  uMVP;
    vec3  uPxScalers;
};

layout(push_constant) uniform uVertPC { layout(offset=0) uint idx; } vpc;

layout(set=0, binding = 0) buffer UniformData
{
    Uniforms a[];
} uUniforms;

layout(set=0, binding = 1) buffer LineData
{
    Line a[];
} uLines;

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

void main()
{
    uint lineIndex = gl_VertexIndex / 6;
    // 0-------------2
    // |             |
    // 1-------------3
    // Line is triangles 210 and 123
    uint vertexIndex = abs(2 - int(gl_VertexIndex - lineIndex * 6));
    vec4 p0 = vec4(uLines.a[lineIndex].p0, 1.0);
    vec4 p1 = vec4(uLines.a[lineIndex].p1, 1.0);

    p0 = uUniforms.a[vpc.idx].uMVP * p0;
    p1 = uUniforms.a[vpc.idx].uMVP * p1;

    //Fix near plane intersection - place point on near plane if it is behind it;
    //Works only for non-reverse projection
    vec4 pNear = mix(p0, p1, -p0.z/(p1.z - p0.z));
    if (p0.z < 0.0)
    {
        p0 = pNear;
    }
    if (p1.z < 0.0)
    {
        p1 = pNear;
    }

    vec2 n = p1.xy/p1.w - p0.xy/p0.w;

    n = normalize(vec2(-n.y, n.x * uUniforms.a[vpc.idx].uPxScalers.z));
    //TODO: add caps support - currently they do not cover ends 
    //n = normalize(vec2(n.x, n.y * uUniforms.a[vpc.idx].uPxScalers.z));
    //n = vec2(n.x - n.y, n.y + n.x);

    bool useP0 = bool(vertexIndex & 2);
    p0 = useP0 ? p0 : p1;
    float s = uLines.a[lineIndex].w * (bool(vertexIndex & 1) ? p0.w : -p0.w); // undo perspective and choose direction, scale by line width
    p0.xy += s * uUniforms.a[vpc.idx].uPxScalers.xy * n;

    gl_Position = p0;
    Out.Color = Color32toVec4(uLines.a[lineIndex].c);
}
