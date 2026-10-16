#ifndef BAM_UI_COMMON_HLSL
#define BAM_UI_COMMON_HLSL

// Adjacent stages must preserve the same declaration order, including unused SV_POSITION in the PS.
struct UIVertexOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : TEXCOORD1;
};

#endif
