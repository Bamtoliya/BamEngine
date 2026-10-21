#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "Mesh.h"

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
	// 월드 피킹 좌표와 브러시 파라미터를 기반으로 CPU 높이 데이터를 갱신합니다.
	static bool ApplyBrush(TerrainComponent& terrain, const glm::vec3& hitWorldPos, const glm::mat4& worldMatrix, const TerrainBrushParams& params, float dt);
	// 이미지 파일(PNG, JPG 등)로부터 높이맵 데이터를 읽어와 terrain.HeightData에 채우고 GPU 동기화를 요청합니다.
	static bool LoadHeightmapFromFile(TerrainComponent& terrain, const std::wstring& filePath);
	// 현재 terrain.HeightData를 1채널 PNG 이미지 파일로 저장(Bake / Export)합니다.
	static bool SaveHeightmapToFile(const TerrainComponent& terrain, const std::wstring& filePath);
	// 지형 표면에 텍스처 레이어(0~3) 페인팅
	static void ApplyPaint(TerrainComponent& terrain, const vec3& hitPoint, uint32 targetLayer, f32 radius, f32 intensity, f32 falloff);
};

END