#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include "LightTypes.h"
#include "Light.h"
#include "DirectionalShadowCascades.h"
#include "CameraBuffer.h"

#include <optional>

namespace Engine
{
	class LightSystem;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_LIGHT"), CATEGORY("CATEGORY_RENDER"))
	struct LightComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"), META("ComponentEnable", true))
		bool IsEnabled = true;
		static constexpr auto ComponentEnableMember = &LightComponent::IsEnabled;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIGHT_TYPE"))
		ELightType LightType = ELightType::Directional;

		PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR"))
		vec3 Color{ 1.f };

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INTENSITY"))
		f32 Intensity = 1.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RANGE"), EDITCONDITION(LightType == Point || LightType == Spot))
		f32 Range = 10.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ATTENUATION_MODE"), EDITCONDITION(LightType == Point || LightType == Spot))
		EAttenuationMode AttenuationMode = EAttenuationMode::Coefficients;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ATTENUATION_COEFFICIENTS"), EDITCONDITION((LightType == Point || LightType == Spot) && AttenuationMode == Coefficients))
		vec3 AttenuationCoefficients{ 1.f, 0.09f, 0.032f };

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPOT_INNER_ANGLE"), EDITCONDITION(LightType == Spot))
		f32 SpotInnerAngle = 15.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPOT_OUTER_ANGLE"), EDITCONDITION(LightType == Spot))
		f32 SpotOuterAngle = 30.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPOT_FALLOFF_EXPONENT"), EDITCONDITION(LightType == Spot))
		f32 SpotFalloffExponent = 1.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIGHTING_LAYER_MASK"), VISIBLE(false))
		uint32 LightingLayerMask = 0xFFFFFFFFu;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FLAGS"), META("HiddenEntries", "UseInForwardRendering|Volumetric"))
		ELightFlags Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_STRENGTH"), RANGE(0.0f, 1.0f, 0.05f), EDITCONDITION(LightType != Sky && (Flags & CastShadows)))
		f32 ShadowStrength = 1.0f;
		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_RANGE"), RANGE(1.0f, 500.0f, 1.0f), EDITCONDITION(LightType == Directional && (Flags & CastShadows)))
		f32 ShadowRange = 50.f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_BIAS"), RANGE(0.0f, 0.05f, 0.0005f), EDITCONDITION((LightType == Directional || LightType == Spot) && (Flags & CastShadows)))
		f32 ShadowBias = 0.001f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_SLOPE_BIAS"), RANGE(0.0f, 0.1f, 0.0005f), EDITCONDITION((LightType == Directional || LightType == Spot) && (Flags & CastShadows)))
		f32 ShadowSlopeBias = 0.005f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_NORMAL_BIAS"), RANGE(0.0f, 0.5f, 0.005f), EDITCONDITION((LightType == Directional || LightType == Spot) && (Flags & CastShadows)))
		f32 ShadowNormalBias = 0.01f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_MAX_DISTANCE"), RANGE(10.0f, 500.0f, 1.0f), EDITCONDITION(LightType == Directional && (Flags & CastShadows)))
		f32 ShadowMaxDistance = 100.f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_CASCADE_SPLIT_LAMBDA"), RANGE(0.1f, 1.0f, 0.05f), EDITCONDITION(LightType == Directional && (Flags & CastShadows)))
		f32 CascadeSplitLambda = 0.75f;

		// 지원 장치에서 이 광원의 그림자를 레이트레이싱한다(뷰마다 최대 4개). 프로젝트·그래픽·뷰포트 설정이 덮어쓸 수 있다.
		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_RAYTRACED_SHADOW"), EDITCONDITION(LightType != Sky && (Flags & CastShadows)))
		bool RaytracedShadow = false;

		PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_SKY"), DISPLAY_NAME("PROP_LOWER_AMBIENT_COLOR"), EDITCONDITION(LightType == Sky))
		vec3 GroundColor{ 0.05f, 0.04f, 0.03f };

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SKY"), DISPLAY_NAME("PROP_INDIRECT_INTENSITY"), EDITCONDITION(LightType == Sky))
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
			if (!IsEnabled || !m_IsCacheValid)
				return std::nullopt;

			return m_GPULight;
		}

		[[nodiscard]]
		std::optional<CameraBuffer> GetSpotShadowCameraBuffer() const
		{
			if (!IsEnabled || !m_IsCacheValid || !m_IsShadowCacheValid || LightType != ELightType::Spot)
				return std::nullopt;
			return m_SpotShadowCameraBuffer;
		}

		[[nodiscard]]
		std::optional<DirectionalShadowSettings> GetDirectionalShadowSettings() const
		{
			if (!IsEnabled || !m_IsCacheValid || !m_IsShadowCacheValid || LightType != ELightType::Directional)
				return std::nullopt;
			return m_DirectionalShadow;
		}

		[[nodiscard]]
		vec4 GetShadowParams() const { return m_ShadowParams; }

		[[nodiscard]]
		std::optional<CameraBuffer> GetPointShadowCameraBuffer(uint32 faceIndex) const
		{
			if (!IsEnabled || !m_IsCacheValid || !m_IsShadowCacheValid || faceIndex >= 6)
				return std::nullopt;
			return m_PointShadowCameraBuffers[faceIndex];
		}

	private:
		friend class LightSystem;

		// 계산 결과는 리플렉션 기반 저장·편집 대상에서 제외합니다.
		GPULight m_GPULight{};

		CameraBuffer m_PointShadowCameraBuffers[6]{};
		mat4 m_PointShadowViewProjMatrices[6]{};

		DirectionalShadowSettings m_DirectionalShadow{};
		CameraBuffer m_SpotShadowCameraBuffer{};
		vec4 m_ShadowParams{ 0.f };

		bool m_IsCacheValid = false;
		bool m_IsShadowCacheValid = false;
	};
}
