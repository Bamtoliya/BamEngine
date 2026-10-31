#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "Mesh.h"
#include "Material.h"
#include "RHITexture.h"
#include "Structs/PixelRect.h"

namespace Engine
{
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

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RESOLUTION"))
		uint32 Resolution = 128; // 그리드 가로/세로 분할 수

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<Material> TerrainMaterial;

		// ── 런타임 GPU/CPU 리소스 (직렬화 제외) ──
		std::shared_ptr<Mesh> GridMesh;
		std::vector<f32> HeightData = {};
		std::shared_ptr<RHITexture> HeightmapTexture = nullptr;
		bool IsDirty = false;
		// ── 스플랫 맵 (4채널 레이어 가중치: R=잔디, G=암석, B=흙, A=자갈) ──
		std::vector<uint8> SplatData = {}; // (Resolution + 1)^2 * 4 바이트 (RGBA)
		std::shared_ptr<RHITexture> SplatmapTexture = nullptr;
		bool IsSplatDirty = false;
		std::optional<PixelRect> HeightDirtyRegion;

		void MarkHeightDirty()
		{
			IsDirty = true;
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
		}
	};
}