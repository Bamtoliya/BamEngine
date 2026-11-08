struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// 머티리얼의 파라미터가 바인딩되는 상수 버퍼 (b0, space3)
struct VignetteParams
{
    float Intensity;   // 비네팅 강도 (0.0 ~ 1.0)
    float Smoothness;  // 외곽 부드러움 (0.01 ~ 1.0)
    float Roundness;   // 원형 비율 (0.5 ~ 2.0)
    float _pad0;       // 16바이트 정렬 패딩
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<VignetteParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = inputTexture.Sample(inputSampler, input.TexCoord);

    // UV 중심을 (0, 0) 기준으로 변환 (-0.5 ~ 0.5)
    float2 uv = input.TexCoord - 0.5f;

    // 타원/원형 거리 계산
    float dist = length(uv * float2(1.0f, gParams.Roundness));

    // 부드러운 가장자리 감쇠 계산
    float vignette = smoothstep(0.71f, 0.71f - gParams.Smoothness, dist);

    // 원본 색상에 비네팅 감쇠 적용
    color.rgb *= lerp(1.0f, vignette, gParams.Intensity);

    return color;
}