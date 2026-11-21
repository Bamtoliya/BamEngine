#ifndef BAM_UI_COMMON_HLSL
#define BAM_UI_COMMON_HLSL

struct UIAlphaMaskData
{
    float4 XPlane;
    float4 YPlane;
    float4 UvTransform;
    float4 UvClamp;
};

// Adjacent stages must preserve the same declaration order, including unused SV_POSITION in the PS.
struct UIVertexOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color : TEXCOORD1;
    float4 Ellipse01 : TEXCOORD2;
    float4 Ellipse23 : TEXCOORD3;
    float4 AlphaMaskLocal : TEXCOORD4;
    nointerpolation float4 AlphaMask0Uv : TEXCOORD5;
    nointerpolation float4 AlphaMask0Clamp : TEXCOORD6;
    nointerpolation float4 AlphaMask1Uv : TEXCOORD7;
    nointerpolation float4 AlphaMask1Clamp : TEXCOORD8;
};

#endif
