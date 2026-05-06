#version 450

#include "common.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec4 inColor;

layout(location = 0) out vec4 outDiffuse;    // RT0: Diffuse RGBA
layout(location = 1) out vec4 outNormal;     // RT1: Normal RGB + Spare
layout(location = 2) out vec4 outPBR;        // RT2: Roughness/Metallic/Specular/AO
layout(location = 3) out vec4 outEmission;   // RT3: Emission RGB + Flags
layout(location = 4) out vec4 outPosition;   // RT4: Position RGB + ObjectID

layout(set = 2, binding = 0) uniform sampler2D diffuseMap;

void main()
{
    vec4 diffuse = texture(diffuseMap, inTexCoord) * inColor;

    outDiffuse  = diffuse;
    outNormal   = vec4(inNormal * 0.5 + 0.5, 0.0);
    outPBR      = vec4(0.5, 0.0, 0.5, 1.0);  // Roughness, Metallic, Specular, AO
    outEmission = vec4(0.0, 0.0, 0.0, 0.0);
    outPosition = vec4(inWorldPos, 0.0);       // ObjectID는 나중에
}
