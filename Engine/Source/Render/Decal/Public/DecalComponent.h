#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include "Types.h"

namespace Engine
{
	// 상자 부피 안의 불투명 표면(G-Buffer)에 텍스처를 투영한다. 상자는 엔티티 변환에 Size를 곱한 단위 상자이며
	// 로컬 z가 투영 방향, 로컬 xy가 텍스처 좌표다. 지면에 붙이려면 엔티티를 x축으로 90도 돌린다.
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_DECAL"), CATEGORY("CATEGORY_RENDER"))
	struct DecalComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"), META("ComponentEnable", true))
		bool IsEnabled = true;
		static constexpr auto ComponentEnableMember = &DecalComponent::IsEnabled;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_DECAL_SIZE"), STEP(0.05))
		vec3 Size{ 1.0f, 1.0f, 1.0f };

		// 작은 값부터 그리고 큰 값이 위에 덮인다.
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_SORTING_ORDER"))
		int32 SortOrder = 0;

		// 투영 방향과 표면 법선의 각도가 이보다 크면 그리지 않는다. 옆면에 길게 늘어나는 것을 막는다.
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_DECAL_MAX_ANGLE"), RANGE(0.0f, 90.0f, 1.0f))
		f32 MaxAngle = 70.0f;

		// 투영 방향의 양 끝에서 서서히 사라지는 폭(반 깊이에 대한 비율)
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_DECAL_DEPTH_FADE"), RANGE(0.0f, 1.0f, 0.01f))
		f32 DepthFade = 0.1f;

		// 모든 채널에 곱하는 불투명도
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_DECAL_OPACITY"), RANGE(0.0f, 1.0f, 0.01f))
		f32 Opacity = 1.0f;

		// 텍스처가 없으면 색상만 칠한다. 텍스처의 알파가 모양을 정한다.
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_BASE_COLOR"), DISPLAY_NAME("PROP_DECAL_BASE_COLOR_TEXTURE"))
		ResourceHandle<Texture> BaseColorTexture;

		PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_DECAL_BASE_COLOR"), DISPLAY_NAME("PROP_COLOR_TINT"))
		vec4 BaseColor{ 1.0f, 1.0f, 1.0f, 1.0f };

		// 접선 공간 법선 맵. 없으면 법선을 바꾸지 않는다.
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_NORMAL"), DISPLAY_NAME("PROP_DECAL_NORMAL_TEXTURE"))
		ResourceHandle<Texture> NormalTexture;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_NORMAL"), DISPLAY_NAME("PROP_DECAL_NORMAL_OPACITY"), RANGE(0.0f, 1.0f, 0.01f))
		f32 NormalOpacity = 1.0f;

		// R 거칠기, G 금속성, B 캐비티. 없으면 아래 거칠기·금속성 값을 쓴다. 표면 불투명도가 0이면 바꾸지 않는다.
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_SURFACE"), DISPLAY_NAME("PROP_DECAL_PBR_TEXTURE"))
		ResourceHandle<Texture> PBRTexture;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_SURFACE"), DISPLAY_NAME("PROP_DECAL_ROUGHNESS"), RANGE(0.04f, 1.0f, 0.01f))
		f32 Roughness = 0.5f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_SURFACE"), DISPLAY_NAME("PROP_DECAL_METALLIC"), RANGE(0.0f, 1.0f, 0.01f))
		f32 Metallic = 0.0f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_DECAL_SURFACE"), DISPLAY_NAME("PROP_DECAL_PBR_OPACITY"), RANGE(0.0f, 1.0f, 0.01f))
		f32 PBROpacity = 0.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_UV_OFFSET"), STEP(0.01))
		vec2 uvOffset{ 0.0f, 0.0f };

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_UV_SCALE"), STEP(0.05))
		vec2 uvScale{ 1.0f, 1.0f };

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_UV_SCROLL_SPEED"), STEP(0.05))
		vec2 uvScrollSpeed{ 0.0f, 0.0f };

		// 0 이하이면 영구 유지. 양수이면 경과 시간이 lifetime에 도달할 때 사라진다.
		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_LIFETIME"), STEP(0.05))
		f32 lifetime = 0.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_DECAL_PROJECTION"), DISPLAY_NAME("PROP_FADE_OUT_DURATION"), STEP(0.05))
		f32 fadeOutDuration = 0.0f;

		// 수명 추적용 런타임 경과 시간(직렬화하지 않는다).
		f32 elapsedTime = 0.0f;
	};
}
