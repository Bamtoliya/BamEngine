#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inColor;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec4 outColor;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

void main() {
    vec4 worldPos = ubo.worldMatrix * vec4(inPosition, 1.0);
    outWorldPos   = worldPos.xyz;
    outNormal     = normalize(mat3(ubo.worldMatrix) * inNormal);
    outTexCoord   = inTexCoord;
    outTangent    = vec3(1.0, 0.0, 0.0);
    outColor      = inColor;
    gl_Position   = commonVertdata.viewProjection * worldPos;
}
