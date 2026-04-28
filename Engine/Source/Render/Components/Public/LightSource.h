#pragma once

#include "Component.h"

BEGIN(Engine)

ENUM()
enum class ELightType
{
	Point,
	Directional,
	Spot
};

enum class ELightAttenuationType
{
	Constant,
	Linear,
	Quadratic
};

enum class ELightSpotAttenuationType
{
	Constant,
	Linear,
	Quadratic
};

enum class ELightSpotAngleType
{
	Inner,
	Outer,
	Falloff
};

enum class ELightIntensityType
{
	Base,
	Max,
	AttenuationStartDistance
};

enum class ELightFlags
{
	None = 0,
	CastShadows = 1 << 0,
	UseInForwardRendering = 1 << 1,
	UseInDeferredRendering = 1 << 2,
	Volumetric = 1 << 3,
};


CLASS()
class ENGINE_API LightSource : public Component
{
	REFLECT_CLASS()
private:

public:
	ELightType GetType() const { return m_Type; }
	EResult SetType(ELightType type) { m_Type = type; return EResult::Success; }


private:
	ELightType m_Type = { ELightType::Point };
	vec3 m_Direction = { 0.0f, -1.0f, 0.0f }; // 방향성 광원과 스포트라이트에 사용
	vec3 m_Color = { 1.0f, 1.0f, 1.0f };
	vec3 m_Intensity = { 1.0f, 0.0f, 0.0f }; // 기본 밝기, 최대 밝기, 감쇠 시작 거리
	vec3 m_Attenuation = { 1.0f, 0.0f, 0.0f }; // 상수, 선형, 제곱 감쇠 계수
	vec3 m_SpotAngles = { 15.0f, 30.0f, 45.0f }; // 스포트라이트의 inner, outer, falloff 각도 (degree)
	vec3 m_SpotAttenuation = { 1.0f, 0.0f, 0.0f }; // 스포트라이트의 감쇠 계수 (상수, 선형, 제곱)
	ELightFlags m_Flags = { ELightFlags::None };
}
END