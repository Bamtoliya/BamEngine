// 디스토션 V2: 반투명 메시·파티클·트레일이 UV 오프셋 버퍼(R16G16_FLOAT)에 Additive(One/One)로 왜곡 오프셋을 누적 기록한다.
#include "common.hlsli"

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPos : WORLD_POSITION;
	float3 Normal : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float3 Tangent : TANGENT;
	float4 Color : COLOR0;
	nointerpolation float PickingId : TEXCOORD5;
};

Texture2D gDistortionTex : register(t0, space1);
SamplerState gDistortionSampler : register(s0, space1);

float2 main(PS_INPUT input) : SV_TARGET0
{
	const float2 uv = input.TexCoord;
	const float4 texSample = gDistortionTex.Sample(gDistortionSampler, uv);

	// 폭(v)·진행(u) 가장자리를 부드럽게 감쇠해 트레일/파티클 경계의 하드 에지를 방지한다.
	const float widthFade = sin(saturate(uv.y) * 3.14159265f);
	const float lengthFade = smoothstep(0.0f, 0.08f, uv.x) * smoothstep(0.0f, 0.15f, 1.0f - uv.x);
	const float edgeFade = widthFade * lengthFade;

	// 노멀맵 텍스처(RG 편차 + B>0.5)는 그대로 방향 벡터로 쓰고, 흑백 노이즈/마스크 텍스처는 시간 기반 파동 방향을 생성한다.
	const bool isNormalMap = (texSample.b > 0.5f) &&
		((abs(texSample.r - texSample.b) + abs(texSample.g - texSample.b)) > 0.08f);

	float2 dir;
	float mask;
	if (isNormalMap)
	{
		dir = texSample.rg * 2.0f - 1.0f;
		mask = texSample.a;
	}
	else
	{
		const float noisePhase = texSample.r * 6.2831853f;
		const float waveA = sin(uv.x * 16.0f - commonData.Time * 7.0f + uv.y * 6.0f + noisePhase);
		const float waveB = cos(uv.y * 12.0f - commonData.Time * 5.0f + uv.x * 10.0f + noisePhase);
		dir = float2(waveA, waveB);
		mask = texSample.a * texSample.r;
	}

	const float baseStrength = 0.05f;
	const float weight = saturate(input.Color.a) * edgeFade * mask;
	return dir * (baseStrength * weight);
}
