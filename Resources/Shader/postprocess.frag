#version 450

layout(location = 0) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D inputTexture;

layout(std140, set = 3, binding = 0) uniform PostProcessParams
{
    float exposure;
    float gamma;
    float _pad0;
    float _pad1;
} gParams;

// ACES Filmic Tone Mapping
vec3 ACESFilmic(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(inputTexture, inTexCoord).rgb;

    // Exposure
    hdrColor *= gParams.exposure;

    // Tone Mapping (ACES Filmic)
    vec3 mapped = ACESFilmic(hdrColor);

    // Gamma Correction
    mapped = pow(mapped, vec3(1.0 / gParams.gamma));

    outColor = vec4(mapped, 1.0);
}