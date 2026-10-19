#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "Types.h"
#include "Resource.h"
#include "Mesh.h"
#include "Material.h"
#include "RHITexture.h"
#include <vector>

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

		// 런타임 GPU 리소스
		ResourceHandle<Mesh> GridMesh;
		ResourceHandle<Material> TerrainMaterial;
		std::shared_ptr<RHITexture> HeightmapTexture;

		// CPU 높이 데이터 (가로 Resolution x 세로 Resolution)
		std::vector<f32> HeightData;
		bool IsDirty = false;
	};
}