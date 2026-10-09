#pragma once

#include <reflection/Annotations.h>
#include "Types.h"
#include "Render/LightTypes.h"
#include "Light.h"
#include "CameraBuffer.h"

#include <optional>

namespace Engine
{
	class LightSystem;

	STRUCT()
		struct LightComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE)
		bool IsEnabled = true;

		PROPERTY(EDITABLE)
		ELightType LightType = ELightType::Directional;

		PROPERTY(EDITABLE, COLOR_PROPERTY)
		vec3 Color{ 1.f };

		PROPERTY(EDITABLE)
		f32 Intensity = 1.f;

		PROPERTY(EDITABLE)
		f32 Range = 10.f;

		PROPERTY(EDITABLE)
		EAttenuationMode AttenuationMode = EAttenuationMode::Coefficients;

		PROPERTY(EDITABLE)
		vec3 AttenuationCoefficients{ 1.f, 0.09f, 0.032f };

		PROPERTY(EDITABLE, DISPLAY_NAME("Spot Inner Angle (Degrees)"))
		f32 SpotInnerAngle = 15.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("Spot Outer Angle (Degrees)"))
		f32 SpotOuterAngle = 30.f;

		PROPERTY(EDITABLE)
		f32 SpotFalloffExponent = 1.f;

		PROPERTY(EDITABLE)
		uint32 LightingLayerMask = 0xFFFFFFFFu;

		PROPERTY(EDITABLE)
		ELightFlags Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular;

		PROPERTY(EDITABLE, CATEGORY("Shadow"))
		f32 ShadowRange = 50.f;

		PROPERTY(EDITABLE, CATEGORY("Shadow"))
		f32 ShadowBias = 0.002f;

		PROPERTY(EDITABLE, CATEGORY("Shadow"))
		f32 ShadowSlopeBias = 0.5f;

		PROPERTY(EDITABLE, CATEGORY("Shadow"))
		f32 ShadowNormalBias = 0.01f;

		PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("Sky"))
		vec3 GroundColor{ 0.05f, 0.04f, 0.03f };

		PROPERTY(EDITABLE, CATEGORY("Sky"))
		f32 IndirectIntensity = 0.5f;

	public:
		void InvalidateCache()
		{
			m_IsCacheValid = false;
			m_IsShadowCacheValid = false;
		}

		[[nodiscard]]
		std::optional<GPULight> GetGPULightDesc() const
		{
			if (!m_IsCacheValid)
				return std::nullopt;

			return m_GPULight;
		}

		[[nodiscard]]
		std::optional<CameraBuffer> GetShadowCameraBuffer() const
		{
			if (!m_IsCacheValid || !m_IsShadowCacheValid)
				return std::nullopt;

			CameraBuffer buffer{};
			buffer.viewMatrix = m_ShadowViewMatrix;
			buffer.invViewMatrix = m_ShadowViewMatrixInv;
			buffer.projMatrix = m_ShadowProjMatrix;
			buffer.invProjMatrix = m_ShadowProjMatrixInv;
			buffer.viewProjMatrix = m_ShadowViewProjMatrix;
			buffer.invViewProjMatrix = m_ShadowViewProjMatrixInv;
			buffer.cameraPosition = m_GPULight.Position;
			buffer.time = 0.f;
			return buffer;
		}

		[[nodiscard]]
		std::optional<LightShadowData> GetShadowData() const
		{
			if (!m_IsCacheValid || !m_IsShadowCacheValid)
				return std::nullopt;

			LightShadowData data{};
			data.LightViewProjMatrix = m_ShadowViewProjMatrix;
			data.ShadowParams = m_ShadowParams;
			return data;
		}

	private:
		friend class LightSystem;

		// 계산 결과는 리플렉션 기반 저장·편집 대상에서 제외합니다.
		GPULight m_GPULight{};

		mat4 m_ShadowViewMatrix{ 1.f };
		mat4 m_ShadowViewMatrixInv{ 1.f };
		mat4 m_ShadowProjMatrix{ 1.f };
		mat4 m_ShadowProjMatrixInv{ 1.f };
		mat4 m_ShadowViewProjMatrix{ 1.f };
		mat4 m_ShadowViewProjMatrixInv{ 1.f };
		vec4 m_ShadowParams{ 0.f };

		bool m_IsCacheValid = false;
		bool m_IsShadowCacheValid = false;
	};
}