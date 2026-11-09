#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"

namespace Engine
{
	class Mesh;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_LOD_GROUP"), CATEGORY("CATEGORY_RENDER"))
	struct LODGroupComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOD1_MESH"))
		ResourceHandle<Mesh> lod1Mesh;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOD1_SCREEN_COVERAGE"), RANGE(0.01f, 1.0f, 0.01f))
		float lod1ScreenCoverage = 0.25f; // 화면 점유율 25% 이하일 때 LOD 1로 전환

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOD2_MESH"))
		ResourceHandle<Mesh> lod2Mesh;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LOD2_SCREEN_COVERAGE"), RANGE(0.01f, 1.0f, 0.01f))
		float lod2ScreenCoverage = 0.08f; // 화면 점유율 8% 이하일 때 LOD 2로 전환

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_HYSTERESIS_MARGIN"), RANGE(0.005f, 0.1f, 0.005f))
		float hysteresisMargin = 0.02f; // 경계 진동 방지 마진 (2%)

		// 런타임 상태 (0: 기본 LOD 0, 1: LOD 1, 2: LOD 2)
		int32 currentLODIndex = 0;
	};
}