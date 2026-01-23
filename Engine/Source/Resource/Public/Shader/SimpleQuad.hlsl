// SimpleQuad.hlsl

cbuffer TransformData : register(b0)
{
    matrix WorldMatrix;
    matrix ViewMatrix;
    matrix ProjectionMatrix;  
};


cbuffer MaterialData : register(b1)
{
    float4 BaseColor;
    float2 Tiling;
    float2 Offset;
    float Metallic;
    float Roughness;
    float AmbientOcclusion;
    float Padding1; // Padding to align to 16 bytes  
};

Texture2D MainTexture : register(t0);
SamplerState MainSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float4 Color : COLOR0;
    float3 Normal : NORMAL0;
    float3 Tangent : TANGENT0;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float4 Color : COLOR0;
    float2 TexCoord : TEXCOORD0;
};


PS_INPUT VS_Main(VS_INPUT input)
{
    PS_INPUT output;
    // Apply transformations
    float4 worldPosition = mul(float4(input.Position, 1.0f), WorldMatrix);
    float4 viewPosition = mul(worldPosition, ViewMatrix);
    output.Position = mul(viewPosition, ProjectionMatrix);
    // Pass through color and texture coordinates
    output.Color = input.Color;
    output.TexCoord = input.TexCoord * Tiling + Offset;
    return output;
}

float4 PS_Main(PS_INPUT input) : SV_TARGET
{
    // Sample the texture
    float4 textureColor = MainTexture.Sample(MainSampler, input.TexCoord);
    // Combine with base color
    float4 finalColor = textureColor * BaseColor * input.Color;
    return finalColor;
}