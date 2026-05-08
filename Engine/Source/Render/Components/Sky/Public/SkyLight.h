#pragma once

#include "LightSource.h"

BEGIN(Engine)

struct tagSkyLightDesc : public tagLightSourceDesc
{
	vec3 GroundColor = vec3(0.05f, 0.04f, 0.03f);
	f32  IndirectIntensity = 0.5f;
};

CLASS()
class ENGINE_API SkyLight : public LightSource
{
	REFLECT_CLASS()
	using DESC = tagSkyLightDesc;
#pragma region Constructor&Destructor
private:
	SkyLight() = default;
	virtual ~SkyLight() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static SkyLight* Create(void* arg = nullptr);
	Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	void LateUpdate(f32 dt) override;
#pragma endregion


#pragma region GPU
public:
	virtual tagGPULight BuildGPULightDesc() const override;
#pragma endregion

#pragma region Getter
public:
	const vec3& GetGroundColor()     const { return m_GroundColor; }
	f32         GetIndirectIntensity() const { return m_IndirectIntensity; }
#pragma endregion

#pragma region Member Variable
private:
	// GroundColor: GPULight.AttenuationCoeff 슬롯 재활용 (Attenuation 불필요)
	PROPERTY(EDITABLE, COLOR)
	vec3 m_GroundColor = vec3(0.05f, 0.04f, 0.03f);

	PROPERTY(EDITABLE)
	f32  m_IndirectIntensity = 0.5f;

	PROPERTY()
	vec3 m_Direction = vec3(0.0f, -1.0f, 0.0f);
#pragma endregion
};
END