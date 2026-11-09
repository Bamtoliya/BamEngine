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

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_UPSCALE_RT"))
		bool upscaleRT = true; // 가상 저해상도 렌더타깃에 렌더 후 정수배 업스케일

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CROP_FRAME_X"))
		bool cropFrameX = false; // X축 필러박스 정수비 클리핑

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CROP_FRAME_Y"))
		bool cropFrameY = false; // Y축 레터박스 정수비 클리핑

		PROPERTY(READONLY, DISPLAY_NAME("PROP_CURRENT_ZOOM"))
		int32 currentZoom = 1; // 계산된 정수 줌 배율 (1x, 2x, 3x ...)
	};
}
