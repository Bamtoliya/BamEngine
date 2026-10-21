#pragma once

#include "Reflection/ComponentAnnotations.h"
#include "Types.h"
#include "Components/Render/LightTypes.h"
#include "Light.h"
#include "CameraBuffer.h"

#include <optional>

namespace Engine
{
	class LightSystem;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_LIGHT"), CATEGORY("CATEGORY_RENDER"))
	struct LightComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
		bool IsEnabled = true;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIGHT_TYPE"))
		ELightType LightType = ELightType::Directional;

		PROPERTY(EDITABLE, COLOR_PROPERTY, DISPLAY_NAME("PROP_COLOR"))
		vec3 Color{ 1.f };

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_INTENSITY"))
		f32 Intensity = 1.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RANGE"), EDITCONDITION(LightType != Directional))
		f32 Range = 10.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ATTENUATION_MODE"))
		EAttenuationMode AttenuationMode = EAttenuationMode::Coefficients;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ATTENUATION_COEFFICIENTS"))
		vec3 AttenuationCoefficients{ 1.f, 0.09f, 0.032f };

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPOT_INNER_ANGLE"), EDITCONDITION(LightType == Spot))
		f32 SpotInnerAngle = 15.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPOT_OUTER_ANGLE"), EDITCONDITION(LightType == Spot))
		f32 SpotOuterAngle = 30.f;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPOT_FALLOFF_EXPONENT"), EDITCONDITION(LightType == Spot))
		f32 SpotFalloffExponent = 1.f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LIGHTING_LAYER_MASK"))
		uint32 LightingLayerMask = 0xFFFFFFFFu;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FLAGS"))
		ELightFlags Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_RANGE"), RANGE(1.0f, 500.0f, 1.0f))
		f32 ShadowRange = 50.f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_BIAS"), RANGE(0.0f, 0.05f, 0.0005f))
		f32 ShadowBias = 0.001f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_SLOPE_BIAS"), RANGE(0.0f, 0.1f, 0.0005f))
		f32 ShadowSlopeBias = 0.005f;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_NORMAL_BIAS"), RANGE(0.0f, 0.5f, 0.005f))
		f32 ShadowNormalBias = 0.01f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_SHADOW_MAX_DISTANCE"), RANGE(10.0f, 500.0f, 1.0f))
		f32 ShadowMaxDistance = 100.f;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_SHADOW"), DISPLAY_NAME("PROP_CASCADE_SPLIT_LAMBDA"), RANGE(0.1f, 1.0f, 0.05f))
		f32 CascadeSplitLambda = 0.75f;

		PROPERTY(EDITABLE, COLOR_PROPERTY, CATEGORY("CATEGORY_SKY"), DISPLAY_NAME("PROP_GROUND_COLOR"))
		vec3 GroundColor{ 0.05f, 0.04f, 0.03f };

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_SKY"), DISPLAY_NAME("PROP_INDIRECT_INTENSITY"))
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
		std::optional<CameraBuffer> GetShadowCameraBuffer(uint32 cascadeIndex = 0) const
		{
			if (!m_IsCacheValid || !m_IsShadowCacheValid || cascadeIndex >= MAX_SHADOW_CASCADES)
				return std::nullopt;
			return m_ShadowCameraBuffers[cascadeIndex];
		}

		[[nodiscard]]
		std::optional<LightShadowData> GetShadowData() const
		{
			if (!m_IsCacheValid || !m_IsShadowCacheValid)
				return std::nullopt;

			LightShadowData data{};
			for (uint32 i = 0; i < MAX_SHADOW_CASCADES; ++i)
				data.LightViewProjMatrices[i] = m_ShadowViewProjMatrices[i];
			data.CascadeSplits = m_CascadeSplits;
			data.ShadowParams = m_ShadowParams;
			return data;
		}

		[[nodiscard]]
		vec4 GetShadowParams() const { return m_ShadowParams; }

		[[nodiscard]]
		std::optional<CameraBuffer> GetPointShadowCameraBuffer(uint32 faceIndex) const
		{
			if (!m_IsCacheValid || !m_IsShadowCacheValid || faceIndex >= 6)
				return std::nullopt;
			return m_PointShadowCameraBuffers[faceIndex];
		}

	private:
		friend class LightSystem;

		// 계산 결과는 리플렉션 기반 저장·편집 대상에서 제외합니다.
		GPULight m_GPULight{};

		CameraBuffer m_PointShadowCameraBuffers[6]{};
		mat4 m_PointShadowViewProjMatrices[6]{};

		mat4 m_ShadowViewProjMatrices[MAX_SHADOW_CASCADES]{ mat4(1.f), mat4(1.f), mat4(1.f) };
		CameraBuffer m_ShadowCameraBuffers[MAX_SHADOW_CASCADES]{};
		vec4 m_CascadeSplits{ 10.0f, 35.0f, 100.0f, 0.0f };
		vec4 m_ShadowParams{ 0.f };

		bool m_IsCacheValid = false;
		bool m_IsShadowCacheValid = false;
	};
}