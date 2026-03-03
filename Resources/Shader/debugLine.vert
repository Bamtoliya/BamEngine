#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec4 inColor;

layout(location = 0) out vec4 fragColor;

void main() {
    gl_Position = commonVertdata.viewProjection * vec4(inPosition, 1.0);
    fragColor = inColor;
}