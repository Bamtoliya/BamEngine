#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_PIXEL_PERFECT_CAMERA"), CATEGORY("CATEGORY_RENDER"))
	struct PixelPerfectCameraComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ASSETS_PPU"))
		f32 assetsPPU = 16.0f; // 픽셀 아트 PPU (1 Unit당 픽셀 수, 기본 16)

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_REF_RESOLUTION_X"))
		int32 refResolutionX = 320; // 기준 가상 해상도 가로 (예: 320)

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_REF_RESOLUTION_Y"))
		int32 refResolutionY = 180; // 기준 가상 해상도 세로 (예: 180)

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SNAP_SUBPIXEL"))
		bool snapSubpixel = true; // 서브픽셀 위치 스냅으로 카메라 이동 시 떨림(Jittering) 방지

		PROPERTY(READONLY)
		int32 currentZoom = 1; // 계산된 정수 줌 배율 (1x, 2x, 3x ...)
	};
}
