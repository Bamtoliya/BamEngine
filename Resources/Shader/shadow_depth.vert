#version 450

#include "common.glsl"

layout(location = 0) in vec3 inPosition;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
} ubo;

void main()
{
    gl_Position = commonVertdata.viewProjection * ubo.worldMatrix * vec4(inPosition, 1.0);
}