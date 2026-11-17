#include "common.hlsli"

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float4 Color : COLOR;
};

// MaterialInterface.h의 MaterialConstants와 같은 배치 (Space 3, b0 / RHI Slot 2)
struct MaterialConstants
{
    uint materialFlags;
    float roughness;
    float metallic;
    float cavity;
    float4 baseColor;
    float3 emissiveColor;
    float ao;
};

ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);

// S6 1단계 합성 순서 검증용 비조명 반투명 출력
float4 main(PS_INPUT input) : SV_TARGET
{
    return gMaterial.baseColor;
}