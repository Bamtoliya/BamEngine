#pragma once

#include "Engine_API.h"
#include "Resource.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include <filesystem>
#include <string>

BEGIN(Engine)

// 지형 레이어 하나의 설정. 에셋이 저장하고, 편집기는 이 구조체 단위로 사본·편집 이력·미저장 판정을 한다.
// 텍스처·반복 길이는 레이어 렌더링 단계에서, Walkable·NavCost·SurfaceType은 내비게이션(T4)·게임플레이가 읽는다.
STRUCT()
struct TerrainLayerSettings
{
	REFLECT_BODY()

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LAYER_ALBEDO"))
	ResourceHandle<Texture> Albedo;

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LAYER_NORMAL"))
	ResourceHandle<Texture> NormalTexture;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.04, 1.0, 0.01), DISPLAY_NAME("PROP_TERRAIN_LAYER_ROUGHNESS"))
	f32 Roughness = 0.85f;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 1.0, 0.01), DISPLAY_NAME("PROP_TERRAIN_LAYER_METALLIC"))
	f32 Metallic = 0.0f;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.1, 256.0, 0.1), DISPLAY_NAME("PROP_TERRAIN_LAYER_TILING"))
	f32 Tiling = 8.0f; // 텍스처가 반복되는 월드 길이(m)

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LAYER_WALKABLE"))
	bool Walkable = true;

	PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 100.0, 0.1), DISPLAY_NAME("PROP_TERRAIN_LAYER_NAV_COST"))
	f32 NavCost = 1.0f;

	PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LAYER_SURFACE"))
	std::string SurfaceType; // 발소리·이펙트 구분용 이름(예: Grass, Rock)

	bool operator==(const TerrainLayerSettings& other) const = default;
};

struct TerrainLayerCreateDesc : public ResourceCreateDesc
{
};

// 지형 레이어 에셋(.bamterrainlayer.json). 여러 지형이 같은 레이어를 참조해 공유한다.
CLASS()
class ENGINE_API TerrainLayerAsset : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(TerrainLayerAsset)

private:
	using DESC = TerrainLayerCreateDesc;
	TerrainLayerAsset() : Resource(EResourceType::TerrainLayer) {}
	virtual ~TerrainLayerAsset() = default;

public:
	static constexpr const wchar_t* FileSuffix = L".bamterrainlayer.json";

	virtual EResult Initialize(void* arg = nullptr) override;
	virtual void Free() override {}
	static TerrainLayerAsset* Create(void* arg = nullptr);

	// 가상 URI·프로젝트 상대·절대 경로를 받는다.
	bool SaveToFile(const std::filesystem::path& path) const;
	bool LoadFromFile(const std::filesystem::path& path);

	const TerrainLayerSettings& GetSettings() const { return m_Settings; }
	void SetSettings(const TerrainLayerSettings& settings) { m_Settings = settings; }

	// 팔레트 등에 보일 이름: 파일 이름에서 .bamterrainlayer(.json)을 뗀 값
	std::string GetDisplayName() const;
	static bool IsTerrainLayerFile(const std::filesystem::path& path);

private:
	PROPERTY(EDITABLE)
	TerrainLayerSettings m_Settings;
};

END
