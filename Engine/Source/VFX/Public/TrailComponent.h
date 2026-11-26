#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "CompositeVFXComponent.h"

namespace Engine
{
	class Texture;
	class MaterialInterface;

	// 독립 엔티티의 이동 궤적(슬라임 점액, 투사체 꼬리 등)을 리본 스트립으로 그리는 컴포넌트.
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_TRAIL"), CATEGORY("CATEGORY_RENDER"))
	struct TrailComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"), META("ComponentEnable", true))
		bool enabled = true;
		static constexpr auto ComponentEnableMember = &TrailComponent::enabled;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_EMITTING"))
		bool emitting = true;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_TRAIL_WIDTH"), RANGE(0.01, 10.0, 0.02))
		f32 width = 0.5f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_LIFETIME"), STEP(0.05))
		f32 lifetime = 0.35f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_MIN_VERTEX_DISTANCE"), RANGE(0.005, 2.0, 0.01))
		f32 minVertexDistance = 0.05f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_TAPER_RATIO"), RANGE(0.0, 1.0, 0.02))
		f32 taperRatio = 0.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_TRAIL_ALIGNMENT"))
		EVFXTrailAlignment alignment = EVFXTrailAlignment::ViewFacing;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_VFX_TRANSFORM"), DISPLAY_NAME("PROP_OFFSET_A"), STEP(0.02))
		vec3 localOffsetA = vec3(0.0f, 0.3f, 0.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_VFX_TRANSFORM"), DISPLAY_NAME("PROP_OFFSET_B"), STEP(0.02))
		vec3 localOffsetB = vec3(0.0f, -0.3f, 0.0f);

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_COLOR_START"), COLOR_PROPERTY)
		vec4 colorStart = vec4(1.0f, 1.0f, 1.0f, 1.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_COLOR_END"), COLOR_PROPERTY)
		vec4 colorEnd = vec4(1.0f, 1.0f, 1.0f, 0.0f);

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_TEXTURE_HANDLE"))
		ResourceHandle<Texture> textureHandle;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_VFX_RENDER"), DISPLAY_NAME("PROP_MATERIAL_HANDLE"))
		ResourceHandle<MaterialInterface> materialHandle;

		// 런타임 포인트 버퍼(직렬화하지 않는다).
		VFXTrailTrackRuntime runtime;

		void Clear() { runtime.Reset(); }
	};
}
