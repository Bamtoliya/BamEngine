#pragma once

#include "Component.h"
#include "Light.h"
#include "Camera.h"

BEGIN(Engine)

ENUM()
enum class ELightType
{
	Point,
	Directional,
	Spot
};

ENUM()
enum class EAttenuationMode : uint8
{
	Coefficients,
	InverseSquare,
	Disabled
};

ENUM()
enum class ELightFlags
{
	None = 0,
	CastShadows = 1 << 0,
	UseInForwardRendering = 1 << 1,
	UseInDeferredRendering = 1 << 2,
	Volumetric = 1 << 3,
	AffectDiffuse = 1 << 4,
	AffectSpecular = 1 << 5
};

ENABLE_BITMASK_OPERATORS(ELightFlags)

struct tagLightSourceDesc : public tagComponentDesc
{
	ELightType Type = ELightType::Point;
	vec3 Color = vec3(1.0f);
	f32 Intensity = 1.0f;

	f32 Range = 10.0f;
	EAttenuationMode AttenuationMode = EAttenuationMode::Coefficients;
	vec3 AttenuationCoefficients = vec3(1.0f, 0.09f, 0.032f);

	f32 SpotInnerAngle = 15.0f;
	f32 SpotOuterAngle = 30.0f;
	f32 SpotFalloffExponent = 1.0f;

	uint32 LightingLayerMask = 0xFFFFFFFFu;
	ELightFlags Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular;
};


CLASS()
class ENGINE_API LightSource : public Component
{
	REFLECT_CLASS()
	using DESC = tagLightSourceDesc;
#pragma region Constructor&Destructor
private:
	LightSource() {}
	virtual ~LightSource() = default;
	EResult Initialize(void* arg = nullptr) override;
	EResult LateInitialize(void* arg = nullptr) override;
public:
	static LightSource* Create(void* arg = nullptr);
	Component* Clone(GameObject* owner, void* arg = nullptr) override;
	void Free() override;
#pragma endregion

#pragma region Loop
public:
	void LateUpdate(f32 dt) override;
#pragma endregion

#pragma region GPU
public:
	tagGPULight BuildGPULightDesc() const;
#pragma endregion

#pragma region Shadow
public:
	tagCameraBuffer BuildShadowCameraBuffer() const;
	tagLightShadowData BuildShadowData() const;
#pragma endregion


#pragma region Getter
public:
	ELightType GetType() const { return m_Type; }
	const vec3& GetColor() const { return m_Color; }
	f32 GetIntensity() const { return m_Intensity; }

	f32 GetRange() const { return m_Range; }
	EAttenuationMode GetAttenuationMode() const { return m_AttenuationMode; }
	const vec3& GetAttenuationCoefficients() const { return m_AttenuationCoefficients; }

	f32 GetSpotInnerAngle() const { return m_SpotInnerAngle; }
	f32 GetSpotOuterAngle() const { return m_SpotOuterAngle; }
	f32 GetSpotFalloffExponent() const { return m_SpotFalloffExponent; }

	uint32 GetLightingLayerMask() const { return m_LightingLayerMask; }
	ELightFlags GetFlags() const { return m_Flags; }
#pragma endregion

#pragma region Setter
public:
	EResult SetType(ELightType type) { m_Type = type; return EResult::Success; }
	EResult SetColor(const vec3& color) { m_Color = color; return EResult::Success; }
	EResult SetIntensity(f32 intensity) { m_Intensity = glm::max(0.0f, intensity); return EResult::Success; }

	EResult SetRange(f32 range) { m_Range = glm::max(0.0f, range); return EResult::Success; }
	EResult SetAttenuationMode(EAttenuationMode mode) { m_AttenuationMode = mode; return EResult::Success; }
	EResult SetAttenuationCoefficients(const vec3& coeff) { m_AttenuationCoefficients = glm::max(coeff, vec3(0.0f)); return EResult::Success; }

	EResult SetSpotInnerAngle(f32 angleDeg) { m_SpotInnerAngle = glm::clamp(angleDeg, 0.0f, 89.0f); return EResult::Success; }
	EResult SetSpotOuterAngle(f32 angleDeg) { m_SpotOuterAngle = glm::clamp(angleDeg, m_SpotInnerAngle, 89.0f); return EResult::Success; }
	EResult SetSpotFalloffExponent(f32 value) { m_SpotFalloffExponent = glm::max(0.01f, value); return EResult::Success; }

	EResult SetLightingLayerMask(uint32 mask) { m_LightingLayerMask = mask; return EResult::Success; }
	EResult SetFlags(ELightFlags flags) { m_Flags = flags; return EResult::Success; }
#pragma endregion

private:
	PROPERTY(EDITABLE)
	ELightType m_Type = ELightType::Point;

	PROPERTY(EDITABLE)
	vec3 m_Color = vec3(1.0f);

	PROPERTY(EDITABLE)
	f32 m_Intensity = 1.0f;

	PROPERTY(EDITABLE)
	f32 m_Range = 10.0f;

	PROPERTY(EDITABLE)
	EAttenuationMode m_AttenuationMode = EAttenuationMode::Coefficients;

	PROPERTY(EDITABLE)
	vec3 m_AttenuationCoefficients = vec3(1.0f, 0.09f, 0.032f);

	PROPERTY(EDITABLE)
	f32 m_SpotInnerAngle = 15.0f;

	PROPERTY(EDITABLE)
	f32 m_SpotOuterAngle = 30.0f;

	PROPERTY(EDITABLE)
	f32 m_SpotFalloffExponent = 1.0f;

	PROPERTY(EDITABLE)
	uint32 m_LightingLayerMask = 0xFFFFFFFFu;


	PROPERTY(EDITABLE, EDITCONDITION("m_Flags", ELightFlags::CastShadows, false), CATEGORY("Shadow"), RANGE(-FLT_MAX, FLT_MAX, 0.01f))
	f32 m_ShadowRange = 50.0f;
	PROPERTY(EDITABLE, EDITCONDITION("m_Flags", ELightFlags::CastShadows, false), CATEGORY("Shadow"), RANGE(-FLT_MAX, FLT_MAX, 0.01f))
	f32 m_ShadowBias = 0.002f;
	PROPERTY(EDITABLE, EDITCONDITION("m_Flags", ELightFlags::CastShadows, false), CATEGORY("Shadow"), RANGE(-FLT_MAX, FLT_MAX, 0.01f))
	f32 m_ShadowSlopeBias = 0.5f;
	PROPERTY(EDITABLE, EDITCONDITION("m_Flags", ELightFlags::CastShadows, false), CATEGORY("Shadow"), RANGE(-FLT_MAX, FLT_MAX, 0.01f))
	f32 m_ShadowNormalBias = 0.01f;

	PROPERTY(EDITABLE)
	ELightFlags m_Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular;
};
END