// 프로브 GI의 격자·아틀라스 규칙과 갱신 상수 (DirectX12 전용).
// 계산 규칙은 ProbeGrid.h와 같다. 한쪽을 바꾸면 다른 쪽과 ProbeGridCheck도 맞춘다.
#ifndef BAM_PROBE_GI_HLSLI
#define BAM_PROBE_GI_HLSLI

static const uint PROBE_IRRADIANCE_TEXELS = 8u;
static const uint PROBE_DISTANCE_TEXELS = 16u;

// DeferredSceneRenderer.cpp의 ProbeGIUpdateParams (b1, space3)
struct ProbeGIUpdateParams
{
	float3 GridOrigin;      // 첫 프로브의 위치
	uint RaysPerProbe;
	float3 GridSpacing;
	uint ProbeCount;
	uint3 GridCounts;
	uint UpdateCount;       // 이번 프레임에 갱신하는 프로브 수(광선 결과의 행 수)
	uint FirstProbe;        // 행 r은 (FirstProbe + r) % ProbeCount 프로브다
	uint FreshCount;        // 앞에서부터 이 수만큼의 행은 이전 값과 섞지 않는다
	uint InstanceTable;     // RaytracedInstance 표의 번호
	uint ClearAll;          // 1이면 모든 프로브의 아틀라스를 0으로 비운다(행 = 프로브 번호)
	float4 RayRotation0;    // 광선 묶음 회전의 행(xyz)
	float4 RayRotation1;
	float4 RayRotation2;
	float3 SkyZenith;
	float IBLIntensity;
	uint HasIBLMaps;
	uint MaxHitLights;      // 닿은 면에서 확인할 광원 수
	float NormalBias;       // 그림자 광선 시작점 오프셋(m)
	float RayMaxDistance;
	float Hysteresis;       // 이전 값을 남기는 비율
	float MaxProbeDistance; // 거리 아틀라스에 기록하는 최대 거리(m)
	uint RayDataTexture;    // 광선 결과 텍스처의 번호(섞는 패스에서 읽음)
	float DistanceExponent; // 거리를 섞을 때 방향 가중치의 지수
};

uint3 ProbeCoord(uint probeIndex, uint3 counts)
{
	const uint slice = counts.x * counts.y;
	return uint3(probeIndex % counts.x, (probeIndex % slice) / counts.x, probeIndex / slice);
}

float3 ProbeWorldPosition(uint probeIndex, ProbeGIUpdateParams params)
{
	return params.GridOrigin + params.GridSpacing * (float3)ProbeCoord(probeIndex, params.GridCounts);
}

// 프로브 칸의 왼쪽 위 텍셀(테두리 포함). 칸 크기는 texels + 2다.
uint2 ProbeTileOrigin(uint probeIndex, uint3 counts, uint texels)
{
	const uint3 coord = ProbeCoord(probeIndex, counts);
	return uint2(coord.x + coord.y * counts.x, coord.z) * (texels + 2u);
}

float2 EncodeOctahedral(float3 direction)
{
	float2 result = direction.xz / max(abs(direction.x) + abs(direction.y) + abs(direction.z), 1e-8f);
	if (direction.y < 0.0f)
	{
		const float2 signs = float2(result.x >= 0.0f ? 1.0f : -1.0f, result.y >= 0.0f ? 1.0f : -1.0f);
		result = (1.0f - abs(result.yx)) * signs;
	}
	return result;
}

float3 DecodeOctahedral(float2 octahedral)
{
	float3 direction = float3(octahedral.x, 1.0f - abs(octahedral.x) - abs(octahedral.y), octahedral.y);
	if (direction.y < 0.0f)
	{
		const float2 signs = float2(direction.x >= 0.0f ? 1.0f : -1.0f, direction.z >= 0.0f ? 1.0f : -1.0f);
		direction.xz = (1.0f - abs(direction.zx)) * signs;
	}
	return normalize(direction);
}

// 칸 안쪽 텍셀(1~N)의 중심이 가리키는 방향
float3 ProbeTexelDirection(uint2 local, uint texels)
{
	const float2 uv = ((float2)local - 0.5f) / (float)texels;
	return DecodeOctahedral(uv * 2.0f - 1.0f);
}

// 테두리 텍셀(칸 기준 0~N+1)이 복사해 올 안쪽 텍셀. 옥타헤드럴 경계는 가운데를 기준으로 접힌다.
uint2 ProbeBorderSource(uint2 local, uint texels)
{
	const uint last = texels + 1u;
	const bool left = local.x == 0u, right = local.x == last, top = local.y == 0u, bottom = local.y == last;
	if ((left || right) && (top || bottom))
		return uint2(left ? texels : 1u, top ? texels : 1u);
	if (top || bottom)
		return uint2(last - local.x, top ? 1u : texels);
	if (left || right)
		return uint2(left ? 1u : texels, last - local.y);
	return local;
}

// 테두리 텍셀을 0 ~ 4N+3 번호로 센다: 위 줄, 아래 줄(각 N+2개), 왼쪽 열, 오른쪽 열(각 N개).
uint2 ProbeBorderTexel(uint index, uint texels)
{
	const uint row = texels + 2u;
	if (index < row)
		return uint2(index, 0u);
	if (index < 2u * row)
		return uint2(index - row, texels + 1u);
	if (index < 2u * row + texels)
		return uint2(0u, 1u + index - 2u * row);
	return uint2(texels + 1u, 1u + index - 2u * row - texels);
}

// 구 위에 고르게 퍼진 i번째 방향(구면 피보나치)을 이번 프레임의 회전으로 돌린다.
float3 ProbeRayDirection(uint rayIndex, ProbeGIUpdateParams params)
{
	const float goldenAngle = 2.39996322972865332f;
	const float y = 1.0f - (2.0f * (float)rayIndex + 1.0f) / (float)params.RaysPerProbe;
	const float radius = sqrt(max(0.0f, 1.0f - y * y));
	const float phi = goldenAngle * (float)rayIndex;
	const float3 direction = float3(cos(phi) * radius, y, sin(phi) * radius);
	return normalize(float3(dot(params.RayRotation0.xyz, direction), dot(params.RayRotation1.xyz, direction), dot(params.RayRotation2.xyz, direction)));
}

#endif
