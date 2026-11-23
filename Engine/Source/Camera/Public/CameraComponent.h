#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include "CameraBuffer.h"
#include "DepthOfField.h"
#include <optional>
#include "ComponentAnnotations.h"

namespace Engine
{
	class CameraSystem;

	ENUM(DISPLAY_NAME("ENUM_CAMERA_TYPE"))
	enum class ECameraType
	{
		Perspective,
		Orthographic
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_CAMERA"), CATEGORY("CATEGORY_RENDER"))
	struct CameraComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CAMERA_TYPE"))
		ECameraType CameraType = ECameraType::Perspective;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FOV"), EDITCONDITION(CameraType == Perspective))
		f32 FOV = 60.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ASPECT_RATIO"))
		f32 AspectRatio = 16.f / 9.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NEAR_PLANE"))
		f32 NearPlane = 0.1f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FAR_PLANE"))
		f32 FarPlane = 1000.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ORTHOGRAPHIC_SIZE"), EDITCONDITION(CameraType == Orthographic))
		f32 OrthographicSize = 10.f;
		PROPERTY(READONLY)
		bool IsMainCamera = false;

		// 켜면 이 카메라로 그리는 Game View·Client가 후처리의 피사계 심도 초점 값 대신 아래 값을 쓴다.
		// 효과를 켜고 끄는 것은 후처리 설정이 정한다. 씬 뷰는 항상 후처리 값을 쓴다.
		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_OVERRIDE_DEPTH_OF_FIELD"))
		bool OverrideDepthOfField = false;
		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_DEPTH_OF_FIELD_MODE"), EDITCONDITION(OverrideDepthOfField))
		EDepthOfFieldMode DepthOfFieldMode = EDepthOfFieldMode::FocusRange;
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.1, 200.0), DISPLAY_NAME("PROP_FOCUS_DISTANCE"), EDITCONDITION(OverrideDepthOfField))
		f32 FocusDistance = 10.f;
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 50.0), DISPLAY_NAME("PROP_FOCUS_RANGE"), EDITCONDITION(OverrideDepthOfField && DepthOfFieldMode == FocusRange))
		f32 FocusRange = 4.f;
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.1, 100.0), DISPLAY_NAME("PROP_FOCUS_TRANSITION_RANGE"), EDITCONDITION(OverrideDepthOfField && DepthOfFieldMode == FocusRange))
		f32 FocusTransitionRange = 10.f;
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.7, 22.0), DISPLAY_NAME("PROP_F_STOP"), EDITCONDITION(OverrideDepthOfField && DepthOfFieldMode == PhysicalLens))
		f32 FStop = 2.8f;
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(4.0, 60.0), DISPLAY_NAME("PROP_SENSOR_HEIGHT"), EDITCONDITION(OverrideDepthOfField && DepthOfFieldMode == PhysicalLens))
		f32 SensorHeight = 24.f;
		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.0, 32.0), DISPLAY_NAME("PROP_MAX_BLUR_RADIUS"), EDITCONDITION(OverrideDepthOfField))
		f32 MaxBlurRadius = 12.f;
	public:
		// 덮어쓰기를 켰을 때만 값이 있다.
		[[nodiscard]]
		std::optional<DepthOfFieldSettings> GetDepthOfFieldOverride() const
		{
			if (!OverrideDepthOfField)
				return std::nullopt;
			DepthOfFieldSettings settings;
			settings.Mode = DepthOfFieldMode;
			settings.FocusDistance = FocusDistance;
			settings.FocusRange = FocusRange;
			settings.TransitionRange = FocusTransitionRange;
			settings.FStop = FStop;
			settings.SensorHeight = SensorHeight;
			settings.MaxBlurRadius = MaxBlurRadius;
			return settings;
		}

		void InvalidateCache()
		{
			m_IsCacheValid = false;
		}

		[[nodiscard]]
		std::optional<CameraBuffer> GetCameraBuffer() const
		{
			if (!m_IsCacheValid)
				return std::nullopt;

			CameraBuffer buffer{};
			buffer.viewMatrix = m_ViewMatrix;
			buffer.invViewMatrix = m_ViewMatrixInv;
			buffer.projMatrix = m_ProjMatrix;
			buffer.invProjMatrix = m_ProjMatrixInv;
			buffer.viewProjMatrix = m_ViewProjMatrix;
			buffer.invViewProjMatrix = m_ViewProjMatrixInv;
			buffer.cameraPosition = m_CameraPosition;
			buffer.time = 0.f;
			return buffer;
		}
	private:
		friend class CameraSystem;
		mat4 m_ViewMatrix{ 1.f };
		mat4 m_ViewMatrixInv{ 1.f };
		mat4 m_ProjMatrix{ 1.f };
		mat4 m_ProjMatrixInv{ 1.f };
		mat4 m_ViewProjMatrix{ 1.f };
		mat4 m_ViewProjMatrixInv{ 1.f };
		vec3 m_CameraPosition{ 0.f };
		bool m_IsCacheValid = false;
	};
}