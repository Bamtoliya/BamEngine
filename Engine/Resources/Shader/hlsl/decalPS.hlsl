// 디퍼드 데칼. G-Buffer 위치를 상자 공간으로 옮겨 상자 안의 표면에 텍스처를 투영한다.
// 쓰기 마스크가 RGB라 각 타깃의 a(하늘 판정·셰이딩 플래그·AO)는 그대로 남고, 출력 알파는 섞는 비율로만 쓰인다.
#include "shading_flags.hlsli"
#include "decal.hlsli"

Texture2D gPosition : register(t0, space3);   // G-Buffer 위치
Texture2D gNormalCopy : register(t1, space3); // 데칼을 그리기 전 G-Buffer 법선(a에 셰이딩 플래그)

Texture2D gBaseColorMap : register(t0, space1);
Texture2D gNormalMap : register(t1, space1);  // 접선 공간 법선
Texture2D gPBRMap : register(t2, space1);     // R 거칠기, G 금속성, B 캐비티
SamplerState gDecalSampler : register(s0, space1);

// 깊이가 끊기는 픽셀은 이웃과 텍스처 좌표가 크게 달라 가장 작은 밉을 고른다. 한 픽셀이 텍스처의 이 비율을 넘게
// 건너뛰지 않도록 기울기를 자른다. 측정 전 시작값이다.
static const float MAX_UV_GRADIENT = 0.05f;

struct PS_INPUT
{
	float4 Position : SV_POSITION;
};

struct PS_OUTPUT
{
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 PBR : SV_TARGET2;
};

float2 ClampGradient(float2 gradient)
{
	const float len = length(gradient);
	return len > MAX_UV_GRADIENT ? gradient * (MAX_UV_GRADIENT / len) : gradient;
}

PS_OUTPUT main(PS_INPUT input)
{
	const int3 pixel = int3(input.Position.xy, 0);
	const float3 worldPosition = gPosition.Load(pixel).xyz;
	const float4 normalSample = gNormalCopy.Load(pixel);
	const float3 local = mul(gDecal.WorldToBox, float4(worldPosition, 1.0f)).xyz;

	// 기울기는 버리기 전에 모든 픽셀에서 구한다.
	const float2 uv = float2(local.x + 0.5f, 0.5f - local.y);
	const float2 uvDx = ClampGradient(ddx(uv));
	const float2 uvDy = ClampGradient(ddy(uv));

	// 비어 있는 픽셀(법선 0)과 데칼을 받지 않는 표면은 플래그가 없다.
	const uint shadingData = UnpackShadingData(normalSample.a);
	if ((shadingData & FLAG_RECEIVE_DECALS) == 0u || any(abs(local) > 0.5f))
		discard;

	const float3 surfaceNormal = normalize(normalSample.rgb * 2.0f - 1.0f);
	const float cosAngle = abs(dot(surfaceNormal, gDecal.AxisZ));
	const float fadeWidth = gDecal.AngleFadeRange.y - gDecal.AngleFadeRange.x;
	float angleFade = cosAngle >= gDecal.AngleFadeRange.x ? 1.0f : 0.0f;
	if (fadeWidth > 1e-5f)
	{
		const float t = saturate((cosAngle - gDecal.AngleFadeRange.x) / fadeWidth);
		angleFade = t * t * (3.0f - 2.0f * t);
	}

	const float depthWidth = saturate(gDecal.DepthFade) * 0.5f;
	const float distanceToEnd = 0.5f - abs(local.z);
	const float depthFade = depthWidth > 1e-5f ? saturate(distanceToEnd / depthWidth) : 1.0f;

	const float fade = angleFade * depthFade * gDecal.Opacity;
	if (fade <= 0.0f)
		discard;

	// 기본 색 텍스처의 알파가 데칼의 모양이며 모든 채널에 함께 곱한다.
	float4 baseColor = gDecal.BaseColor;
	float shape = 1.0f;
	if (gDecal.TextureFlags & DECAL_HAS_BASE_COLOR_TEXTURE)
	{
		const float4 texel = gBaseColorMap.SampleGrad(gDecalSampler, uv, uvDx, uvDy);
		baseColor.rgb *= texel.rgb;
		shape = texel.a;
	}

	PS_OUTPUT output;
	output.Diffuse = float4(baseColor.rgb, saturate(baseColor.a * shape * fade));

	// 표면 법선 위에 데칼 축을 펼친 접선 공간으로 법선 맵을 옮긴다.
	float3 decalNormal = surfaceNormal;
	float normalAlpha = 0.0f;
	if (gDecal.TextureFlags & DECAL_HAS_NORMAL_TEXTURE)
	{
		float3 tangent = gDecal.AxisX - dot(gDecal.AxisX, surfaceNormal) * surfaceNormal;
		tangent = dot(tangent, tangent) > 1e-6f ? normalize(tangent) : normalize(cross(surfaceNormal, gDecal.AxisZ));
		float3 bitangent = cross(surfaceNormal, tangent);
		if (dot(bitangent, gDecal.AxisY) < 0.0f)
			bitangent = -bitangent;
		const float3 tangentNormal = gNormalMap.SampleGrad(gDecalSampler, uv, uvDx, uvDy).xyz * 2.0f - 1.0f;
		const float3 mapped = tangentNormal.x * tangent + tangentNormal.y * bitangent + tangentNormal.z * surfaceNormal;
		if (dot(mapped, mapped) > 1e-6f)
		{
			decalNormal = normalize(mapped);
			normalAlpha = saturate(gDecal.NormalOpacity * shape * fade);
		}
	}
	output.Normal = float4(decalNormal * 0.5f + 0.5f, normalAlpha);

	float3 surface = float3(gDecal.Roughness, gDecal.Metallic, 1.0f);
	if (gDecal.TextureFlags & DECAL_HAS_PBR_TEXTURE)
		surface = gPBRMap.SampleGrad(gDecalSampler, uv, uvDx, uvDy).rgb;
	output.PBR = float4(clamp(surface, float3(0.04f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f)), saturate(gDecal.PBROpacity * shape * fade));
	return output;
}
