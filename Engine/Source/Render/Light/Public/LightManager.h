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
	bool HasShadowCastingLights() const { return !m_ShadowCameraBuffers.empty() || m_HasSpotShadow || m_HasPointShadow; }
	bool HasSpotShadow() const { return m_HasSpotShadow; }
	bool HasPointShadow() const { return m_HasPointShadow; }
	CameraBuffer GetShadowCameraBuffer(uint32 cascadeIndex = 0, uint32 lightIndex = 0) const;
	CameraBuffer GetSpotShadowCameraBuffer() const { return m_SpotShadowCameraBuffer; }
	CameraBuffer GetPointShadowCameraBuffer(uint32 faceIndex = 0) const;
	LightShadowData GetShadowData(uint32 lightIndex = 0) const;
#pragma endregion

#pragma region Member Variables
private:
	class RHIBuffer* m_LightBuffers[MAX_SWAPCHAIN_BUFFERS] = {};
	bool m_BufferDirty[MAX_SWAPCHAIN_BUFFERS] = {};
	vector<std::array<CameraBuffer, MAX_SHADOW_CASCADES>> m_ShadowCameraBuffers;
	vector<LightShadowData> m_ShadowData;

	CameraBuffer m_SpotShadowCameraBuffer{};
	mat4 m_SpotShadowViewProj{ 1.0f };
	vec4 m_SpotShadowParams{ 0.0f };
	bool m_HasSpotShadow = false;

	CameraBuffer m_PointShadowCameraBuffers[6]{};
	vec4 m_PointShadowParams{ 0.0f };
	vec4 m_PointShadowPosRange{ 0.0f };
	bool m_HasPointShadow = false;
private:
	RHI* m_RHI = { nullptr };
#pragma endregion
};
END
