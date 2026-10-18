#pragma once

#include "RHI.h"
#include "Light.h"
#include "CameraBuffer.h"

#define MAX_LIGHTS 32

struct LightManagerDesc
{
	Engine::RHI* RHI = { nullptr };
};

BEGIN(Engine)
class ENGINE_API LightManager : public Base
{
	DECLARE_SINGLETON(LightManager)
	using DESC = LightManagerDesc;
#pragma region Constructor&Destructor
private:
	LightManager() {}
	virtual ~LightManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion


#pragma region Loop
public:
	void Update(f32 dt);
#pragma endregion


#pragma region Light Management
public:
	EResult Bind(uint32 slot);
	EResult UpdateLightBuffer();
private:
	EResult EnsureBuffer();
#pragma endregion

#pragma region Shadow
public:
	bool HasShadowCastingLights() const { return !m_ShadowCameraBuffers.empty(); }
	CameraBuffer GetShadowCameraBuffer(uint32 shadowLightIndex = 0) const;
	LightShadowData GetShadowData(uint32 shadowLightIndex = 0) const;
#pragma endregion

#pragma region Member Variables
private:
	class RHIBuffer* m_LightBuffer = { nullptr };
	uint32 m_LightCount = { 0 };
	bool m_BufferDirty = { false };
	vector<CameraBuffer> m_ShadowCameraBuffers;
	vector<LightShadowData> m_ShadowData;
private:
	RHI* m_RHI = { nullptr };
#pragma endregion
};
END
