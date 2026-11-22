#pragma once

#include "RHI.h"
#include "Light.h"
#include "CameraBuffer.h"
#include "DirectionalShadowCascades.h"
#include <optional>

#define MAX_LIGHTS 1024

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
	EResult UpdateLightBuffer(class Scene* targetScene = nullptr);
private:
	EResult EnsureBuffer();
#pragma endregion

#pragma region Shadow
public:
	bool HasShadowCastingLights() const { return m_DirectionalShadow.has_value() || m_HasSpotShadow || m_HasPointShadow; }
	bool HasSpotShadow() const { return m_HasSpotShadow; }
	bool HasPointShadow() const { return m_HasPointShadow; }
	bool HasDirectionalShadow() const { return m_DirectionalShadow.has_value(); }
	// 첫 그림자 방향광의 캐스케이드. 그림자 패스와 조명 패스는 같은 뷰의 카메라를 넘겨야 한다.
	bool BuildDirectionalCascades(const CameraBuffer* view, DirectionalShadowCascades& outCascades) const;
	CameraBuffer GetSpotShadowCameraBuffer() const { return m_SpotShadowCameraBuffer; }
	CameraBuffer GetPointShadowCameraBuffer(uint32 faceIndex = 0) const;
	LightShadowData GetShadowData(const CameraBuffer& view) const;
#pragma endregion

#pragma region Tiled Lighting
public:
	EResult EnsureTileBuffers(uint32 width, uint32 height);
	EResult DispatchLightCulling(const CameraBuffer& cameraBuffer, class RHITexture* positionTexture);
	class RHIBuffer* GetTileLightGridBuffer() const { return m_TileLightGridBuffer; }
	class RHIBuffer* GetTileLightIndexBuffer() const { return m_TileLightIndexBuffer; }
	uint32 GetTileCountX() const { return m_TileCountX; }
	uint32 GetTileCountY() const { return m_TileCountY; }
private:
	EResult EnsureTiledCullingPipeline();
#pragma endregion

#pragma region Member Variables
private:
	class RHIBuffer* m_LightBuffers[MAX_SWAPCHAIN_BUFFERS] = {};
	bool m_BufferDirty[MAX_SWAPCHAIN_BUFFERS] = {};
	std::optional<DirectionalShadowSettings> m_DirectionalShadow;

	class RHIBuffer* m_TileLightGridBuffer = nullptr;
	class RHIBuffer* m_TileLightIndexBuffer = nullptr;
	class RHIPipeline* m_TiledCullingPipeline = nullptr;
	uint32 m_TileCountX = 0;
	uint32 m_TileCountY = 0;

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
