#include "common.hlsli"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float3 Binormal : BINORMAL;
    float4 Color : COLOR;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : WORLD_POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
    float3 Tangent : TANGENT;
    float4 Color : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

struct InstanceData
{
    float4x4 WorldMatrix;
    float4 PickingData;
};

StructuredBuffer<InstanceData> g_Instances : register(t0, space2);

// 거스트너 파도 단일 파동 평가 함수 (Trochoidal Gerstner Wave)
void EvaluateGerstnerWave(
    float2 dir, float wavelength, float amplitude, float steepness, float speed,
    float3 originPos, float time,
    inout float3 offset, inout float3 normal, inout float3 tangent)
{
    float k = 2.0f * 3.14159265f / wavelength; // 파수 (Wave number)
    float c = sqrt(9.81f / k) * speed;         // 위상 속도 (Phase speed)
    float2 d = normalize(dir);
    float f = k * (dot(d, originPos.xz) - c * time);
    float a = amplitude;
    float q = steepness / (k * a * 4.0f + 0.001f); // 루프/자기교차 방지 스케일링

    float cosF = cos(f);
    float sinF = sin(f);

    // 정점 변위 (Trochoid: 수평 집속 및 수직 솟구침)
    offset.x += q * a * d.x * cosF;
    offset.y += a * sinF;
    offset.z += q * a * d.y * cosF;

    // 분석적 법선 도함수 (Analytical Normal Derivatives)
    float wa = k * a;
    normal.x -= d.x * wa * cosF;
    normal.z -= d.y * wa * cosF;
    normal.y -= q * wa * sinF;

    // 분석적 탄젠트 도함수
    tangent.x -= q * d.x * d.x * wa * sinF;
    tangent.y += d.x * wa * cosF;
    tangent.z -= q * d.x * d.y * wa * sinF;
}

VS_OUTPUT main(VS_INPUT input, uint instanceId : SV_InstanceID)
{
    VS_OUTPUT output;
    float4x4 world = g_Instances[instanceId].WorldMatrix;
    float4 worldPosition = mul(world, float4(input.Position, 1.0f));

    // 4개의 서로 다른 방향과 주기를 가진 거스트너 파도 중첩
    float time = commonData.Time;
    float3 waveOffset = float3(0.0f, 0.0f, 0.0f);
    float3 waveNormal = float3(0.0f, 1.0f, 0.0f);
    float3 waveTangent = float3(1.0f, 0.0f, 0.0f);

    // 1. 주 거대 파도 (Main Swell)
    EvaluateGerstnerWave(float2(0.707f, 0.707f), 4.2f, 0.09f, 0.65f, 0.85f, worldPosition.xyz, time, waveOffset, waveNormal, waveTangent);

    // 2. 교차 파도 (Cross Swell)
    EvaluateGerstnerWave(float2(-0.55f, 0.83f), 2.6f, 0.045f, 0.55f, 1.15f, worldPosition.xyz, time, waveOffset, waveNormal, waveTangent);

    // 3. 중형 파도 (Chop Wave)
    EvaluateGerstnerWave(float2(0.35f, -0.93f), 1.3f, 0.025f, 0.45f, 1.6f, worldPosition.xyz, time, waveOffset, waveNormal, waveTangent);

    // 4. 미세 파도 (Capillary Wave)
    EvaluateGerstnerWave(float2(-0.88f, -0.47f), 0.65f, 0.012f, 0.35f, 2.2f, worldPosition.xyz, time, waveOffset, waveNormal, waveTangent);

    // 실제 파도가 변위된 월드 위치 계산
    float3 displacedWorldPos = worldPosition.xyz + waveOffset;

    output.Position = mul(commonData.ViewProjection, float4(displacedWorldPos, 1.0f));
    output.WorldPos = displacedWorldPos;

    // 회전/스케일 행렬에 따른 법선 변환
    float3x3 world3x3 = (float3x3)world;
    // waveNormal과 waveTangent는 이미 worldPosition.xyz 기준으로 유도된 월드 공간 벡터이므로
    // 스케일 행렬을 다시 곱하지 않고 정규화만 수행하여 스케일 왜곡 방지
    output.Normal = normalize(waveNormal);
    output.Tangent = normalize(waveTangent);

    output.TexCoord = input.TexCoord;
    output.Color = input.Color;
    output.PickingId = g_Instances[instanceId].PickingData.x;
    return output;
}
