#version 450

#include "common.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec4 outDiffuse;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outPBR;
layout(location = 3) out vec4 outEmission;
layout(location = 4) out vec4 outPosition;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 texColor = texture(texSampler, inTexCoord);
    if (texColor.a < 0.01) discard;  // 스프라이트 알파 컷아웃

    outDiffuse  = texColor;
    outNormal   = vec4(inNormal * 0.5 + 0.5, 0.0);
    outPBR      = vec4(1.0, 0.0, 0.0, 1.0);  // 풀 러프니스, 비금속
    outEmission = vec4(0.0);
    outPosition = vec4(inWorldPos, 0.0);
}
