struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// 머티리얼 파라미터 상수 버퍼 (b0, space3)
struct LensDistortionParams
{
    float Distortion;      // 1차 왜곡 강도 (-1.0: 핀쿠션 ~ 1.0: 배럴)
    float CubicDistortion; // 2차 고차 왜곡 강도 (-1.0 ~ 1.0)
    float Scale;           // 줌/스케일 보정 (0.5 ~ 2.0, 기본 1.0)
    float _pad0;           // 16바이트 정렬
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<LensDistortionParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    // 화면 중심 (0.5, 0.5) 기준 상대 좌표
    float2 coord = input.TexCoord - 0.5f;

    // 반지름 제곱(r^2) 기반 다항식 왜곡 계수 계산
    float r2 = dot(coord, coord);
    float distortionFactor = 1.0f + r2 * (gParams.Distortion + r2 * gParams.CubicDistortion);

    // 스케일 및 왜곡 계수 적용 후 UV 좌표 복원
    float2 distortedUV = coord * distortionFactor * gParams.Scale + 0.5f;

    // 화면 영역을 벗어난 경계 외곽은 검은색 처리
    if (distortedUV.x < 0.0f || distortedUV.x > 1.0f || distortedUV.y < 0.0f || distortedUV.y > 1.0f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    return inputTexture.Sample(inputSampler, distortedUV);
}