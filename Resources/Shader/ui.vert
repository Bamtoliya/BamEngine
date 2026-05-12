#version 450

#include "common.glsl"

layout(location = 0) out vec2 fragTexCoord;

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
    vec4 uvRect;
    vec2 uvTiling;
    vec2 uvOffset;
} ubo;

const vec2 positions[6] = vec2[](
    vec2(0.0, 1.0), // 0: Bottom-Left
    vec2(1.0, 1.0), // 1: Bottom-Right
    vec2(1.0, 0.0), // 2: Top-Right
    vec2(0.0, 1.0), // 0: Bottom-Left
    vec2(1.0, 0.0), // 2: Top-Right
    vec2(0.0, 0.0)  // 3: Top-Left
);

const vec2 uvs[6] = vec2[](
    vec2(0.0, 1.0), 
    vec2(1.0, 1.0), 
    vec2(1.0, 0.0), 
    vec2(0.0, 1.0), 
    vec2(1.0, 0.0), 
    vec2(0.0, 0.0)  
);

void main() {
    vec2 myPos = positions[gl_VertexIndex];
    vec2 myUV  = uvs[gl_VertexIndex];
    gl_Position = commonVertdata.viewProjection * ubo.worldMatrix * vec4(myPos, 0.0, 1.0);
    vec2 regionUV = mix(ubo.uvRect.xy, ubo.uvRect.zw, myUV);
    fragTexCoord = regionUV * ubo.uvTiling;
}