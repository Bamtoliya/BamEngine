#version 450

#include "common.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec4 fragEllipse01;
layout(location = 3) in vec4 fragEllipse23;
layout(location = 4) in vec4 fragAlphaMaskLocal;
layout(location = 5) flat in vec4 fragAlphaMask0Uv;
layout(location = 6) flat in vec4 fragAlphaMask0Clamp;
layout(location = 7) flat in vec4 fragAlphaMask1Uv;
layout(location = 8) flat in vec4 fragAlphaMask1Clamp;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D texSampler;
layout(set = 2, binding = 1) uniform sampler2D alphaMask0;
layout(set = 2, binding = 2) uniform sampler2D alphaMask1;

float sampleAlphaMask(sampler2D maskTexture, vec2 local, vec4 uvTransform, vec4 uvClamp) {
    if (uvTransform.z <= 0.0) return 1.0;
    if (any(lessThan(local, vec2(0.0))) || any(greaterThan(local, vec2(1.0)))) return 0.0;
    vec2 uv = clamp(uvTransform.xy + local * uvTransform.zw, uvClamp.xy, uvClamp.zw);
    return textureLod(maskTexture, uv, 0.0).a;
}

void main() {
    vec4 radiusSquared = vec4(dot(fragEllipse01.xy, fragEllipse01.xy), dot(fragEllipse01.zw, fragEllipse01.zw),
        dot(fragEllipse23.xy, fragEllipse23.xy), dot(fragEllipse23.zw, fragEllipse23.zw));
    vec4 color = texture(texSampler, fragTexCoord) * fragColor;
    if (any(greaterThan(radiusSquared, vec4(1.0)))) discard;
    color.a *= sampleAlphaMask(alphaMask0, fragAlphaMaskLocal.xy, fragAlphaMask0Uv, fragAlphaMask0Clamp);
    color.a *= sampleAlphaMask(alphaMask1, fragAlphaMaskLocal.zw, fragAlphaMask1Uv, fragAlphaMask1Clamp);
    outColor = color;
}