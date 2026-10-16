#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inColor;

layout(location = 0) out vec3 outLocalDir;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

void main() {
    outLocalDir = inPosition;
    mat4 rotOnlyView = mat4(mat3(commonVertdata.view));
    vec4 clipPos = commonVertdata.projection * rotOnlyView * ubo.worldMatrix * vec4(inPosition, 1.0);
    gl_Position = clipPos.xyww;
}