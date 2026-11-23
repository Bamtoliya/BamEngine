// 레이트레이싱용 Compute 스키닝 (cs_6_5, DirectX12 전용)
// 스킨드 메시 하나의 정점을 이번 프레임 뼈 행렬로 변환해 BLAS 갱신과 반사 음영이 읽을 버퍼에 쓴다.
// 변환 식은 gbufferSkinningVS.hlsl과 같다(뼈 번호 255로 제한, 가중치 합이 0이면 단위 행렬). 한쪽을 바꾸면 다른 쪽도 맞춘다.
// 결과 위치는 애니메이터(루트) 공간이며, TLAS 인스턴스 행렬이 월드로 옮긴다.
#include "raytracing.hlsli"

struct RaytracedSkinningParams
{
	uint SourcePositions;   // 원본 위치 버퍼 번호 (float3)
	uint SourceMaterials;   // 원본 VertexMaterial 버퍼 번호
	uint SourceSkin;        // VertexSkinData 버퍼 번호 (uint4 뼈 번호, float4 가중치)
	uint Bones;             // 뼈 행렬 표 번호 (열 우선 float4x4)
	uint VertexCount;
	uint PositionStride;
	uint MaterialStride;
	uint SkinStride;
	uint BoneOffset;        // 표에서 이 메시의 첫 뼈 위치
	uint3 Padding;
};

ConstantBuffer<RaytracedSkinningParams> gParams : register(b1, space3);
// 위치는 12바이트 간격, 재질은 원본과 같은 간격으로 쓴다.
RWByteAddressBuffer gSkinnedPositions : register(u0, space3);
RWByteAddressBuffer gSkinnedMaterials : register(u1, space3);

static const uint MAX_BONE_INDEX = 255u;
// VertexMaterial: 법선 0, UV 12, 접선 20, 종법선 32, 정점 색 44
static const uint VERTEX_BINORMAL_OFFSET = 32u;

// 열 우선으로 저장한 행렬의 네 열
struct BoneColumns
{
	float4 c0;
	float4 c1;
	float4 c2;
	float4 c3;
};

BoneColumns LoadBone(ByteAddressBuffer bones, uint boneIndex)
{
	const uint address = (gParams.BoneOffset + min(boneIndex, MAX_BONE_INDEX)) * 64u;
	BoneColumns bone;
	bone.c0 = asfloat(bones.Load4(address));
	bone.c1 = asfloat(bones.Load4(address + 16u));
	bone.c2 = asfloat(bones.Load4(address + 32u));
	bone.c3 = asfloat(bones.Load4(address + 48u));
	return bone;
}

void Accumulate(inout BoneColumns sum, BoneColumns bone, float weight)
{
	sum.c0 += bone.c0 * weight;
	sum.c1 += bone.c1 * weight;
	sum.c2 += bone.c2 * weight;
	sum.c3 += bone.c3 * weight;
}

float3 TransformDirection(BoneColumns m, float3 v)
{
	return m.c0.xyz * v.x + m.c1.xyz * v.y + m.c2.xyz * v.z;
}

float3 NormalizeOr(float3 v, float3 fallback)
{
	const float len = length(v);
	return len > 1e-6f ? v / len : fallback;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchId : SV_DispatchThreadID)
{
	const uint vertex = dispatchId.x;
	if (vertex >= gParams.VertexCount)
		return;

	ByteAddressBuffer positions = gBindlessBuffers[gParams.SourcePositions];
	ByteAddressBuffer materials = gBindlessBuffers[gParams.SourceMaterials];
	ByteAddressBuffer skin = gBindlessBuffers[gParams.SourceSkin];
	ByteAddressBuffer bones = gBindlessBuffers[gParams.Bones];

	const uint skinAddress = vertex * gParams.SkinStride;
	const uint4 boneIds = skin.Load4(skinAddress);
	const float4 weights = asfloat(skin.Load4(skinAddress + 16u));

	BoneColumns m;
	if (weights.x + weights.y + weights.z + weights.w > 0.0001f)
	{
		m.c0 = float4(0.0f, 0.0f, 0.0f, 0.0f);
		m.c1 = m.c0;
		m.c2 = m.c0;
		m.c3 = m.c0;
		Accumulate(m, LoadBone(bones, boneIds.x), weights.x);
		Accumulate(m, LoadBone(bones, boneIds.y), weights.y);
		Accumulate(m, LoadBone(bones, boneIds.z), weights.z);
		Accumulate(m, LoadBone(bones, boneIds.w), weights.w);
	}
	else
	{
		m.c0 = float4(1.0f, 0.0f, 0.0f, 0.0f);
		m.c1 = float4(0.0f, 1.0f, 0.0f, 0.0f);
		m.c2 = float4(0.0f, 0.0f, 1.0f, 0.0f);
		m.c3 = float4(0.0f, 0.0f, 0.0f, 1.0f);
	}

	const float3 position = asfloat(positions.Load3(vertex * gParams.PositionStride));
	const float3 skinnedPosition = TransformDirection(m, position) + m.c3.xyz;
	gSkinnedPositions.Store3(vertex * 12u, asuint(skinnedPosition));

	// 재질 정점은 그대로 복사하고 법선·접선·종법선만 바꾼다. gbufferSkinningVS처럼 섞은 행렬의 3x3을 쓴다.
	const uint source = vertex * gParams.MaterialStride;
	for (uint offset = 0u; offset < gParams.MaterialStride; offset += 4u)
		gSkinnedMaterials.Store(source + offset, materials.Load(source + offset));

	const float3 normal = asfloat(materials.Load3(source + VERTEX_NORMAL_OFFSET));
	const float3 tangent = asfloat(materials.Load3(source + VERTEX_TANGENT_OFFSET));
	const float3 binormal = asfloat(materials.Load3(source + VERTEX_BINORMAL_OFFSET));
	gSkinnedMaterials.Store3(source + VERTEX_NORMAL_OFFSET, asuint(NormalizeOr(TransformDirection(m, normal), normal)));
	gSkinnedMaterials.Store3(source + VERTEX_TANGENT_OFFSET, asuint(NormalizeOr(TransformDirection(m, tangent), tangent)));
	gSkinnedMaterials.Store3(source + VERTEX_BINORMAL_OFFSET, asuint(NormalizeOr(TransformDirection(m, binormal), binormal)));
}
