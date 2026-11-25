#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "Mesh.h"
#include "FrustumCuller.h"
#include <cstddef>

BEGIN(Engine)

struct TerrainComponent;
enum class ETerrainBrushMode : uint8
{
	Raise = 0,
	Lower,
	Smooth,
	Flatten
};
struct TerrainBrushParams
{
	ETerrainBrushMode Mode = ETerrainBrushMode::Raise;
	f32 Radius = 10.0f;
	f32 Strength = 5.0f;
	f32 Falloff = 0.5f;
	f32 TargetHeight = 0.0f;
};

class ENGINE_API TerrainUtility
{
public:
	// 지정한 가로/세로 크기와 분할 해상도로 XZ 평면 그리드 Mesh를 생성하여 반환합니다.
	static Mesh* CreateGridMesh(float width, float length, uint32 resolution);
	static Mesh* CreateGridMesh(float width, float length, uint32 segmentsX, uint32 segmentsZ, bool reverseWinding);
	// 전체 그리드 [0..resolution] 중 [minVertexX..maxVertexX] x [minVertexZ..maxVertexZ] 서브영역을 lodStep 간격으로 샘플링한 청크 Mesh를 생성한다.
	static Mesh* CreateChunkGridMesh(float width, float length, uint32 resolution, uint32 minVertexX, uint32 maxVertexX, uint32 minVertexZ, uint32 maxVertexZ, uint32 lodStep);
	// 청크 좌표 (chunkX, chunkZ)의 정점 인덱스 범위와 전체 지형 기준 UV 서브영역을 계산한다.
	static bool ComputeChunkRange(uint32 resolution, uint32 chunkCountPerAxis, uint32 chunkX, uint32 chunkZ, uint32& outMinVertexX, uint32& outMaxVertexX, uint32& outMinVertexZ, uint32& outMaxVertexZ, vec2& outUVOffset, vec2& outUVScale);
	// 청크 범위의 높이 데이터를 기반으로 로컬 공간 AABB를 계산한다.
	static bool ComputeChunkLocalBounds(const TerrainComponent& terrain, uint32 minVertexX, uint32 maxVertexX, uint32 minVertexZ, uint32 maxVertexZ, BoundingBox& outLocalBounds);
	// 로컬 AABB를 월드 행렬로 변환해 월드 AABB를 계산한다.
	static bool ComputeChunkWorldBounds(const BoundingBox& localBounds, const mat4& worldMatrix, BoundingBox& outWorldBounds);
	// 점(카메라)에서 AABB까지의 최단 거리를 계산한다(내부면 0).
	static f32 ComputeDistanceToBoundingBox(const vec3& point, const BoundingBox& bounds);
	// 거리와 이전 LOD, 히스테리시스를 고려해 0..2 LOD 레벨을 선택한다.
	static uint32 SelectChunkLOD(f32 distance, uint32 previousLOD, f32 lod0Distance, f32 lod1Distance, f32 lod2Distance, f32 hysteresis);
	// 스트리밍 로드 거리 임계치(<= 0이면 항상 로드)와 히스테리시스로 청크 활성/로드 여부를 결정한다.
	static bool ShouldChunkBeLoaded(f32 distance, bool currentlyLoaded, f32 loadDistance, f32 hysteresis);
	// 프러스텀과 청크 월드 AABB의 교차 여부를 판정한다.
	static bool IsChunkVisibleInFrustum(const Frustum& frustum, const BoundingBox& worldBounds);
	// 월드 피킹 좌표와 브러시 파라미터를 기반으로 CPU 높이 데이터를 갱신합니다.
	static bool ApplyBrush(TerrainComponent& terrain, const glm::vec3& hitWorldPos, const glm::mat4& worldMatrix, const TerrainBrushParams& params, float dt);
	// 높이맵을 읽어 terrain.HeightData를 채운다(크기가 다르면 바이리니어로 맞춘다). PNG 등은 16비트까지, .raw·.r16은 리틀 엔디언 uint16 정사각형이다.
	// 넓은 문자 경로로 읽으므로 한글 경로를 쓸 수 있다. 실패하면 기존 높이를 바꾸지 않는다.
	static bool LoadHeightmapFromFile(TerrainComponent& terrain, const std::wstring& filePath);
	// 현재 높이를 16비트로 내보낸다. 확장자가 .raw·.r16이면 RAW, 그 외에는 16비트 그레이스케일 PNG다.
	static bool SaveHeightmapToFile(const TerrainComponent& terrain, const std::wstring& filePath);
	// 스플랫 레이어(0~7)를 칠한다. 월드 피킹 좌표를 worldMatrix의 역행렬로 지형 로컬 좌표로 바꾸므로 이동·회전한 지형에서도 위치가 맞는다.
	// radius는 로컬 단위다(균일 배율 기준). 칠한 정점의 8개 가중치 합은 255로 정규화한다. 바뀐 정점이 있으면 true.
	static bool ApplyPaint(TerrainComponent& terrain, const vec3& hitWorldPos, const glm::mat4& worldMatrix, uint32 targetLayer, f32 radius, f32 intensity, f32 falloff);
	// 속성 맵(ETerrainAttribute)을 반경 안 정점에 그대로 칠한다. 바뀐 정점이 있으면 true.
	static bool ApplyAttributePaint(TerrainComponent& terrain, const vec3& hitWorldPos, const glm::mat4& worldMatrix, uint8 attribute, f32 radius);
	// 레이어 0~3·4~7 평면과 속성 맵이 현재 해상도 크기인지 맞춘다. 비었으면 기본값(레이어 0 = 255, 나머지 0)으로 채운다.
	// 해상도가 바뀌어 크기가 다르면 최근접 리샘플링한다. 데이터가 바뀌었으면 true.
	static bool EnsureLayerData(TerrainComponent& terrain);
	static bool ResampleHeightData(const std::vector<f32>& source, uint32 sourceResolution, uint32 targetResolution, std::vector<f32>& result);
	static bool TryGetGridLayout(uint32 resolution, uint32& axisCount, std::size_t& sampleCount);
};

END