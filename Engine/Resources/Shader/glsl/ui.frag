#version 450

#include "common.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 fragColor;
layout(location = 2) in vec4 fragEllipse01;
layout(location = 3) in vec4 fragEllipse23;
layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 radiusSquared = vec4(dot(fragEllipse01.xy, fragEllipse01.xy), dot(fragEllipse01.zw, fragEllipse01.zw),
        dot(fragEllipse23.xy, fragEllipse23.xy), dot(fragEllipse23.zw, fragEllipse23.zw));
    vec4 color = texture(texSampler, fragTexCoord) * fragColor;
    if (any(greaterThan(radiusSquared, vec4(1.0)))) discard;
    outColor = color;
}