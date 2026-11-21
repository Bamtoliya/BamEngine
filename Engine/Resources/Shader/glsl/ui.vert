#version 450

#include "common.glsl"

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 fragColor;
layout(location = 2) out vec4 fragEllipse01;
layout(location = 3) out vec4 fragEllipse23;
layout(location = 4) out vec4 fragAlphaMaskLocal;
layout(location = 5) flat out vec4 fragAlphaMask0Uv;
layout(location = 6) flat out vec4 fragAlphaMask0Clamp;
layout(location = 7) flat out vec4 fragAlphaMask1Uv;
layout(location = 8) flat out vec4 fragAlphaMask1Clamp;

struct UIAlphaMaskData {
    vec4 xPlane;
    vec4 yPlane;
    vec4 uvTransform;
    vec4 uvClamp;
};

layout(set = 1, binding = 1) uniform UBO {
    mat4 worldMatrix;
    vec4 uvRect;
    vec2 uvTiling;
    vec2 uvOffset;
    vec4 color;
    vec4 ellipseMaskPlanes[8];
    UIAlphaMaskData alphaMasks[2];
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
    vec4 world = ubo.worldMatrix * vec4(myPos, 0.0, 1.0);
    gl_Position = commonVertdata.viewProjection * world;
    vec3 canvas = vec3(world.xy, 1.0);
    fragEllipse01 = vec4(dot(canvas, ubo.ellipseMaskPlanes[0].xyz), dot(canvas, ubo.ellipseMaskPlanes[1].xyz),
        dot(canvas, ubo.ellipseMaskPlanes[2].xyz), dot(canvas, ubo.ellipseMaskPlanes[3].xyz));
    fragEllipse23 = vec4(dot(canvas, ubo.ellipseMaskPlanes[4].xyz), dot(canvas, ubo.ellipseMaskPlanes[5].xyz),
        dot(canvas, ubo.ellipseMaskPlanes[6].xyz), dot(canvas, ubo.ellipseMaskPlanes[7].xyz));
    fragAlphaMaskLocal = vec4(dot(canvas, ubo.alphaMasks[0].xPlane.xyz), dot(canvas, ubo.alphaMasks[0].yPlane.xyz),
        dot(canvas, ubo.alphaMasks[1].xPlane.xyz), dot(canvas, ubo.alphaMasks[1].yPlane.xyz));
    fragAlphaMask0Uv = ubo.alphaMasks[0].uvTransform;
    fragAlphaMask0Clamp = ubo.alphaMasks[0].uvClamp;
    fragAlphaMask1Uv = ubo.alphaMasks[1].uvTransform;
    fragAlphaMask1Clamp = ubo.alphaMasks[1].uvClamp;
    vec2 regionUV = mix(ubo.uvRect.xy, ubo.uvRect.zw, myUV);
    fragTexCoord = regionUV * ubo.uvTiling + ubo.uvOffset;
    fragColor = ubo.color;
}
