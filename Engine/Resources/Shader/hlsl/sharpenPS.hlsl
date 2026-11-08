struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// 머티리얼 파라미터 상수 버퍼 (b0, space3)
struct SharpenParams
{
    float Sharpness; // 선명화 강도 (기본 0.5, 범위 0.0 ~ 2.0)
    float _pad0;     // 16바이트 정렬 패딩
    float _pad1;
    float _pad2;
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<SharpenParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    // 텍스처 해상도를 쿼리하여 정확한 1픽셀(Texel) 크기 계산
    uint width, height;
    inputTexture.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);

    float4 center = inputTexture.Sample(inputSampler, input.TexCoord);
    float3 up     = inputTexture.Sample(inputSampler, input.TexCoord + float2(0.0f, -texelSize.y)).rgb;
    float3 down   = inputTexture.Sample(inputSampler, input.TexCoord + float2(0.0f,  texelSize.y)).rgb;
    float3 left   = inputTexture.Sample(inputSampler, input.TexCoord + float2(-texelSize.x, 0.0f)).rgb;
    float3 right  = inputTexture.Sample(inputSampler, input.TexCoord + float2( texelSize.x, 0.0f)).rgb;

    // 주변 4개 픽셀의 합산
    float3 neighborSum = up + down + left + right;

    // 표준 라플라시안 엣지 강조 공식:
    // Sharpness가 0이면 정확히 center.rgb 반환
    // Sharpness가 올라갈수록 중심과 주변의 차이가 뚜렷하게 증폭됨
    float3 sharpened = center.rgb * (1.0f + 4.0f * gParams.Sharpness) - neighborSum * gParams.Sharpness;

    return float4(saturate(sharpened), center.a);
}