#pragma once

#include "ComponentAnnotations.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "TerrainLayerAsset.h"
#include "ResourceHandle.h"
#include <array>
#include <string>
#include <vector>
#include "BoundingBox.h"
#include "RHITexture.h"
#include "PixelRect.h"

namespace Engine
{
	// 청크 LOD·프러스텀 컬링·지연 스트리밍 프레임 통계(직렬화 제외).
	struct TerrainChunkStats
	{
		uint32 TotalChunks = 0;
		uint32 LoadedChunks = 0;
		uint32 VisibleChunks = 0;
		uint32 CulledChunks = 0;
		uint32 UnloadedChunks = 0;
		std::array<uint32, 3> LodChunkCounts{ 0, 0, 0 };
	};

	// 청크 하나의 런타임 메타데이터 및 LOD 0/1/2 메시 풀(직렬화 제외).
	struct TerrainChunkRuntime
	{
		uint32 ChunkX = 0;
		uint32 ChunkZ = 0;
		uint32 MinVertexX = 0;
		uint32 MaxVertexX = 0;
		uint32 MinVertexZ = 0;
		uint32 MaxVertexZ = 0;
		vec2 UVOffset{ 0.0f, 0.0f };
		vec2 UVScale{ 1.0f, 1.0f };
		BoundingBox LocalBounds{};
		bool IsLoaded = false;
		uint32 CurrentLOD = 3; // 초기 미평가 센티넬(MaxLODLevels)
		std::array<std::shared_ptr<Mesh>, 3> LodMeshes{};
	};

	// 지형 속성 맵 값. 파일에 정수로 저장되므로 순서를 바꾸지 않는다.
	enum class ETerrainAttribute : uint8
	{
		None = 0,
		Blocked = 1, // 이동 불가(보이지 않는 마스크)
		Water = 2,   // 물가·얕은 물(실제 수면 메시와 별개)
		Hole = 3     // 지형 구멍(픽셀 절단 및 그림자 제외)
	};

	// 지형 레이어 칸(스플랫 채널 하나)이 참조하는 레이어 에셋. 정의 자체는 TerrainLayerAsset이 가지며 여러 지형이 공유한다.
	// 인스펙터가 목록 요소를 구조체 단위로 편집하므로 핸들 하나를 구조체로 감싼다.
	STRUCT()
	struct TerrainLayerSlot
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LAYER_ASSET"))
		ResourceHandle<TerrainLayerAsset> Layer;
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_TERRAIN"), CATEGORY("CATEGORY_RENDER"))
	struct TerrainComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_WIDTH"))
		f32 Width = 128.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LENGTH"))
		f32 Length = 128.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MAX_HEIGHT"))
		f32 MaxHeight = 30.0f;

		// 그리드 가로/세로 분할 수. 정점은 (Resolution+1)^2이며 상한은 MaxResolution(1024 → 정점 1025)이다.
		PROPERTY(EDITABLE, RANGE(1, 1024), DISPLAY_NAME("PROP_RESOLUTION"))
		uint32 Resolution = 128;

		static constexpr uint32 MaxResolution = 1024;
		// 스플랫 레이어: 최대 8개. 레이어 0~3은 SplatData, 4~7은 SplatDataExtra(각각 정점당 RGBA8)에 담는다.
		static constexpr uint32 MaxLayers = 8;
		static constexpr uint32 LayersPerPlane = 4;
		static constexpr uint32 MaxChunkCountPerAxis = 16;
		static constexpr uint32 MaxLODLevels = 3;

		// ── 청크 분할 · 거리 기반 LOD · 스트리밍 설정 (ALLOW_MISSING으로 기존 씬 JSON 100% 하위 호환) ──
		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_ENABLE_CHUNK_LOD"))
		bool EnableChunkLOD = true;

		// 축당 청크 분할 수(기본 4 → 4x4=16청크). 1이면 기존 단일 GridMesh 경로로 폴백한다.
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(1, 16), DISPLAY_NAME("PROP_TERRAIN_CHUNK_COUNT_PER_AXIS"))
		uint32 ChunkCountPerAxis = 4;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LOD0_DISTANCE"))
		f32 LOD0Distance = 80.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LOD1_DISTANCE"))
		f32 LOD1Distance = 180.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LOD2_DISTANCE"))
		f32 LOD2Distance = 350.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LOD_HYSTERESIS"))
		f32 LODHysteresis = 8.0f;

		// 청크 스트리밍 로드 거리 임계치(<= 0이면 거리 제한 없이 전체 청크 활성).
		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_STREAMING_LOAD_DISTANCE"))
		f32 StreamingLoadDistance = 0.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> TerrainMaterial;

		// 엔진 기본 지형 머티리얼(추적되는 엔진 리소스). 머티리얼이 비어 있거나 찾지 못한 지형도 이것으로 그린다.
		static constexpr const wchar_t* DefaultMaterialUri = L"Engine://Material/TerrainMaterial.bammat.json";

		// 레이어 칸. i번째 항목의 에셋이 스플랫 레이어 i의 정의이며, MaxLayers(8)를 넘는 항목은 쓰지 않는다. 에셋이 없으면 기본 이름을 쓴다.
		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_LAYERS"))
		std::vector<TerrainLayerSlot> Layers;

		// 높이·스플랫을 담은 .bamterrain 파일(가상 URI). 비어 있으면 씬을 처음 저장할 때 씬 옆에 만든다. 이전 씬에는 없다.
		PROPERTY(READONLY, ALLOW_MISSING, DISPLAY_NAME("PROP_TERRAIN_DATA_PATH"))
		std::string DataPath;

		// 복구 스냅샷 전용: 스냅샷을 쓰는 동안만 채워지는 임시 데이터 파일(절대 경로). 복원 시 이 파일을 읽고 비운다.
		// 일반 씬 저장에서는 항상 비어 있으며, DataPath(원본 파일)는 바꾸지 않는다.
		PROPERTY(ALLOW_MISSING)
		std::string RecoveryDataPath;

		// ── 데이터 파일 동기화 (런타임 전용) ──
		// LoadedDataPath: 마지막으로 읽기를 시도했거나 쓴 경로. DataPath와 다르면 TerrainSystem이 다시 읽는다.
		// IsDataDirty: 마지막 저장 이후 높이·스플랫을 편집했다. 씬 저장 때 파일을 다시 쓴다.
		std::string LoadedDataPath;
		bool IsDataDirty = false;

		// ── 런타임 GPU/CPU 리소스 (직렬화 제외) ──
		std::shared_ptr<Mesh> GridMesh;
		std::vector<f32> HeightData = {};
		std::shared_ptr<RHITexture> HeightmapTexture = nullptr;
		bool IsDirty = false;
		// ── 스플랫 맵 (4채널 레이어 가중치: R=잔디, G=암석, B=흙, A=자갈) ──
		std::vector<uint8> SplatData = {}; // 레이어 0~3: (Resolution + 1)^2 * 4 바이트 (RGBA)
		// 레이어 4~7: SplatData와 같은 배치. SplatmapExtraTexture로 올라가 terrainPS가 읽는다.
		std::vector<uint8> SplatDataExtra = {};
		// 속성 맵: 정점당 ETerrainAttribute 1바이트. 시각 레이어와 별개로 게임 의미(차단·물)를 칠한다. NavGrid가 읽는다(T4).
		std::vector<uint8> AttributeData = {};
		std::shared_ptr<RHITexture> SplatmapTexture = nullptr;      // 레이어 0~3
		std::shared_ptr<RHITexture> SplatmapExtraTexture = nullptr; // 레이어 4~7 (IsSplatDirty로 함께 갱신)
		std::shared_ptr<RHITexture> HoleMaskTexture = nullptr;      // 구멍 마스크(R8_UNORM: 255=Hole, 0=Solid)
		bool IsSplatDirty = false;
		bool IsAttributeDirty = false;
		std::optional<PixelRect> HeightDirtyRegion;

		// ── 청크 분할 · LOD · 스트리밍 런타임 캐시 (직렬화 제외) ──
		std::vector<TerrainChunkRuntime> Chunks;
		uint32 BuiltChunkCountPerAxis = 0;
		uint32 BuiltChunkResolution = 0;
		f32 BuiltChunkWidth = 0.0f;
		f32 BuiltChunkLength = 0.0f;
		f32 BuiltChunkMaxHeight = -1.0f;
		bool ChunkBoundsDirty = true;
		TerrainChunkStats ChunkStats{};

		void MarkHeightDirty()
		{
			IsDirty = true;
			ChunkBoundsDirty = true;
			HeightDirtyRegion.reset();
		}

		void MarkHeightDirty(const PixelRect& region)
		{
			if (region.IsEmpty()) return;

			if (!IsDirty)
			{
				HeightDirtyRegion = region;
			}
			else if (HeightDirtyRegion)
			{
				auto& dirty = *HeightDirtyRegion;
				dirty.minX = (std::min)(dirty.minX, region.minX);
				dirty.minY = (std::min)(dirty.minY, region.minY);
				dirty.maxX = (std::max)(dirty.maxX, region.maxX);
				dirty.maxY = (std::max)(dirty.maxY, region.maxY);
			}

			IsDirty = true;
			ChunkBoundsDirty = true;
		}
	};
}