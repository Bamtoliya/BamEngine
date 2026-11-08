#include "common.hlsli"

struct VS_INPUT
{
    float3 Position     : POSITION;
    float3 Normal       : NORMAL;
    float2 TexCoord     : TEXCOORD0;
    float3 Tangent      : TANGENT;
    float3 Binormal     : BINORMAL;
    float4 Color        : COLOR;
};

struct VS_OUTPUT
{
    float4 Position     : SV_POSITION;
    float3 Direction    : TEXCOORD0;
};

VS_OUTPUT main(VS_INPUT input)
{
    VS_OUTPUT output;

    // View 행렬의 3x3 회전만 적용하여 카메라 위치 이동에 영향을 받지 않는 무한대 스카이 형성
    float3 viewPos = mul((float3x3)commonData.View, input.Position);
    float4 clipPos = mul(commonData.Projection, float4(viewPos, 1.0f));

    // 깊이 버퍼(1.0)와의 부동소수점 오차로 인한 Depth Reject를 방지하기 위해 0.99999f 적용
    output.Position = float4(clipPos.x, clipPos.y, clipPos.w * 0.99999f, clipPos.w);
    output.Direction = input.Position;

    return output;
}