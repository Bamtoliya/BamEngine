#pragma once

#include <reflection/Annotations.h>
#include "Types.h"
#include "CameraBuffer.h"
#include <optional>

namespace Engine
{
	class CameraSystem;

	ENUM()
	enum class ECameraType
	{
		Perspective,
		Orthographic
	};

	STRUCT()
	struct CameraComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CAMERA_TYPE"))
		ECameraType CameraType = ECameraType::Perspective;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FOV"))
		f32 FOV = 60.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ASPECT_RATIO"))
		f32 AspectRatio = 16.f / 9.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NEAR_PLANE"))
		f32 NearPlane = 0.1f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FAR_PLANE"))
		f32 FarPlane = 1000.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ORTHOGRAPHIC_SIZE"))
		f32 OrthographicSize = 10.f;
		PROPERTY(READONLY)
		bool IsMainCamera = false;
	public:
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