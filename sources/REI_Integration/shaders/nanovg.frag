#version 450

struct Paint
{
    vec3 scissorMat[2];
    vec3 paintMat[2];
    vec4 innerCol;
    vec4 outerCol;
    vec2 scissorExt;
    vec2 scissorScale;
    vec2 extent;
    float radius;
    float feather;
    float strokeMult;
    float strokeThr;
    int texType;
    int type;
};

layout(set=0, binding=0) uniform sampler uSampler;
layout(set=1, binding=1, std140) buffer PaintData
{
    Paint a[];
} uPaints;
layout(set=2, binding=0) uniform texture2D uTexture;

layout(push_constant) uniform uFragPC { layout(offset=16) uint idx; } fpc;

layout(location = 0) in struct { vec2 Pos; vec2 UV; } In;

layout(location = 0) out vec4 outColor;

float sdroundrect(vec2 pt, vec2 ext, float rad) {
    vec2 ext2 = ext - vec2(rad,rad);
    vec2 d = abs(pt) - ext2;
    return min(max(d.x,d.y),0.0) + length(max(d,0.0)) - rad;
}
vec2 mulMax23Vec2(vec3 m[2], vec2 v)
{
    return vec2(m[0].x*v.x+m[0].y*v.y+m[0].z, m[1].x*v.x+m[1].y*v.y+m[1].z);
}

// Scissoring
float scissorMask(vec2 p) {
    vec2 sc = (abs(mulMax23Vec2(uPaints.a[fpc.idx].scissorMat, p)) - uPaints.a[fpc.idx].scissorExt);
    sc = vec2(0.5,0.5) - sc * uPaints.a[fpc.idx].scissorScale;
    return clamp(sc.x,0.0,1.0) * clamp(sc.y,0.0,1.0);
}

void main(void) {
    vec4 result = vec4(1,1,1,1);
    float scissor = scissorMask(In.Pos);
    float strokeAlpha = 1.0;
    if (uPaints.a[fpc.idx].type == 0) {            // Gradient
        // Calculate gradient color using box gradient
        vec2 pt = mulMax23Vec2(uPaints.a[fpc.idx].paintMat, In.Pos);
        float d = clamp(
            (sdroundrect(pt, uPaints.a[fpc.idx].extent, uPaints.a[fpc.idx].radius) + uPaints.a[fpc.idx].feather*0.5) / uPaints.a[fpc.idx].feather, 0.0, 1.0);
        vec4 color = mix(uPaints.a[fpc.idx].innerCol, uPaints.a[fpc.idx].outerCol, d);
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    } else if (uPaints.a[fpc.idx].type == 1) {        // Image
        // Calculate color fron texture
        vec2 pt = mulMax23Vec2(uPaints.a[fpc.idx].paintMat, In.Pos)/ uPaints.a[fpc.idx].extent;
        vec4 color = texture(sampler2D(uTexture, uSampler), pt);
        if (uPaints.a[fpc.idx].texType == 1) color = vec4(color.xyz*color.w,color.w);
        if (uPaints.a[fpc.idx].texType == 2) color = vec4(color.x);
        // Apply color tint and alpha.
        color *= uPaints.a[fpc.idx].innerCol;
        // Combine alpha
        color *= strokeAlpha * scissor;
        result = color;
    } /*else if (uPaints.a[fpc.idx].type == 2) {        // Stencil fill
        result = vec4(1,1,1,1);
    }*/ else if (uPaints.a[fpc.idx].type == 3) {        // Textured tris
        vec4 color = texture(sampler2D(uTexture, uSampler), In.UV);
        if (uPaints.a[fpc.idx].texType == 1) color = vec4(color.xyz*color.w,color.w);
        if (uPaints.a[fpc.idx].texType == 2) color = vec4(color.x);
        color *= scissor;
        result = color * uPaints.a[fpc.idx].innerCol;
    }
    outColor = result;
}
