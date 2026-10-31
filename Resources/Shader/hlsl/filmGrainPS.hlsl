struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// 머티리얼 파라미터 상수 버퍼 (b0, space3)
struct FilmGrainParams
{
    float Intensity;     // 노이즈 강도 (기본 0.05, 범위 0.0 ~ 0.5)
    float LuminanceBias; // 암부 가중치 (기본 0.5, 범위 0.0 ~ 1.0, 1.0일수록 어두운 곳에 집중)
    float Scale;         // 입자 크기 스케일 (기본 1.0, 범위 0.1 ~ 5.0)
    float _pad0;         // 16바이트 정렬
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<FilmGrainParams> gParams : register(b0, space3);

// 고주파 의사 난수 생성 함수
float GenerateNoise(float2 uv)
{
    return frac(sin(dot(uv, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = inputTexture.Sample(inputSampler, input.TexCoord);

    // 노이즈 계산 (-0.5 ~ 0.5)
    float noise = GenerateNoise(input.TexCoord * (1000.0f * gParams.Scale)) - 0.5f;

    // Rec.709 휘도(Luminance) 계산
    float lum = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));

    // LuminanceBias에 따라 어두운 곳에서 노이즈가 더 강하게 나타나도록 보간
    float grainAmount = lerp(1.0f, 1.0f - lum, gParams.LuminanceBias) * gParams.Intensity;

    // 원본에 가산 합성
    color.rgb += noise * grainAmount;

    return saturate(color);
}