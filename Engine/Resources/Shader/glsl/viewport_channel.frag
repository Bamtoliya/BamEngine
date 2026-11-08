#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D inputTexture;

layout(std140, set = 3, binding = 0) uniform ChannelViewData
{
    uint flags;
    vec3 padding;
} gChannelView;

const uint CHANNEL_R = 1u << 0u;
const uint CHANNEL_G = 1u << 1u;
const uint CHANNEL_B = 1u << 2u;
const uint CHANNEL_A = 1u << 3u;
const uint CHANNEL_RGBA = CHANNEL_R | CHANNEL_G | CHANNEL_B | CHANNEL_A;

bool HasChannel(uint flags, uint bit)
{
    return (flags & bit) != 0u;
}

void main()
{
    vec4 s = texture(inputTexture, inTexCoord);
    uint flags = gChannelView.flags;

    if (flags == CHANNEL_RGBA)
    {
        outColor = s;
        return;
    }

    bool showR = HasChannel(flags, CHANNEL_R);
    bool showG = HasChannel(flags, CHANNEL_G);
    bool showB = HasChannel(flags, CHANNEL_B);
    bool showA = HasChannel(flags, CHANNEL_A);

    bool anyRGB = showR || showG || showB;

    if (!anyRGB && !showA)
    {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    if (!anyRGB && showA)
    {
        outColor = vec4(s.aaa, 1.0);
        return;
    }

    vec3 rgb = vec3(
        showR ? s.r : 0.0,
        showG ? s.g : 0.0,
        showB ? s.b : 0.0
    );

    if (showA)
        rgb *= s.a;

    outColor = vec4(rgb, 1.0);
}