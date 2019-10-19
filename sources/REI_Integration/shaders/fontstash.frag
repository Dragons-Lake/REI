#version 450 core
layout(location = 0) out vec4 fColor;
layout(set=0, binding=0) uniform sampler   uSampler;
layout(set=0, binding=1) uniform texture2D uTexture;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
void main()
{
    fColor = vec4(In.Color.rgb, In.Color.a * texture(sampler2D(uTexture, uSampler), In.UV).r);
}