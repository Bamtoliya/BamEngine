#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"

namespace Engine
{
	// 프로브 GI 볼륨. 엔티티의 월드 위치를 중심으로 Size 크기의 월드 축 정렬 상자에 프로브 격자를 둔다.
	// 회전과 배율은 쓰지 않는다. 계산 규칙은 ProbeGrid.h를 따른다.
	// 1단계(2026-11-23)는 데이터와 설정만 있다. 광선 추적·갱신(2단계)과 조명 적용(3단계)은 아직 없다.
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_PROBE_GI_VOLUME"), CATEGORY("CATEGORY_RENDER"))
	struct ProbeGIVolumeComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"), META("ComponentEnable", true))
		bool IsEnabled = true;
		static constexpr auto ComponentEnableMember = &ProbeGIVolumeComponent::IsEnabled;

		// 상자의 전체 크기(m)
		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_PROBE_GI_PLACEMENT"), DISPLAY_NAME("PROP_SIZE"))
		vec3 Size{ 10.0f, 4.0f, 10.0f };

		// 축마다 프로브 수. 모서리에도 프로브가 있으므로 간격은 Size / (개수 - 1)이다.
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(2, 32), CATEGORY("CATEGORY_PROBE_GI_PLACEMENT"), DISPLAY_NAME("PROP_PROBE_COUNT_X"))
		int32 ProbeCountX = 16;

		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(2, 16), CATEGORY("CATEGORY_PROBE_GI_PLACEMENT"), DISPLAY_NAME("PROP_PROBE_COUNT_Y"))
		int32 ProbeCountY = 8;

		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(2, 32), CATEGORY("CATEGORY_PROBE_GI_PLACEMENT"), DISPLAY_NAME("PROP_PROBE_COUNT_Z"))
		int32 ProbeCountZ = 16;

		// 프로브마다 프레임에 쏘는 광선 수. 측정 전 시작값이다.
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(32, 256), CATEGORY("CATEGORY_PROBE_GI_UPDATE"), DISPLAY_NAME("PROP_PROBE_RAYS"))
		int32 RaysPerProbe = 128;

		// 프레임마다 갱신할 프로브 수. 나머지는 이전 결과를 쓴다. 측정 전 시작값이다.
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(1, 16384), CATEGORY("CATEGORY_PROBE_GI_UPDATE"), DISPLAY_NAME("PROP_PROBES_PER_FRAME"))
		int32 ProbesPerFrame = 512;

		// 간접 확산광의 세기
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0f, 4.0f), CATEGORY("CATEGORY_PROBE_GI_SHADING"), DISPLAY_NAME("PROP_INTENSITY"))
		f32 Intensity = 1.0f;

		// 상자 밖으로 이 거리(m)만큼 기존 환경광과 섞는다.
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0f, 10.0f), CATEGORY("CATEGORY_PROBE_GI_SHADING"), DISPLAY_NAME("PROP_PROBE_BLEND_DISTANCE"))
		f32 BlendDistance = 1.0f;
	};
}
