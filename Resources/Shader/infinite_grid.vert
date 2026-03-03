#version 450
#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;
layout(location = 5) in vec4 inColor;

layout(location = 0) out vec2 outNDC; 

void main() {
    outNDC = inPosition.xy; // 넘겨줄 데이터 할당
    gl_Position = vec4(inPosition.xy, 1.0, 1.0);
}