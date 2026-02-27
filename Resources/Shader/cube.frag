#version 450

#include "common.glsl"

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragTangent;

layout(location = 0) out vec4 outColor;

layout(set = 2, binding = 0) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, fragTexCoord);
    //vec4 texColor = texture(texSampler, fragTexCoord);
    // 아주 간단한 조명 효과 (방향광)
    //vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    //float diff = max(dot(normalize(fragNormal), lightDir), 0.2); // 0.2는 최소 밝기(Ambient)
    
    //outColor = vec4(texColor.rgb * diff, texColor.a);
}