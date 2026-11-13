#pragma once

#include "CameraBuffer.h"
#include "RenderPassTypes.h"
#include "RenderTargetTypes.h"
#include <array>

BEGIN(Engine)

class RHIBuffer;
class RHIPipeline;
class Scene;
struct SkyAtmosphereData;

struct DeferredScenePassBindings
{
	RenderPassID DirectionalShadow;
	RenderPassID PointShadow;
	RenderPassID Geometry;
	RenderPassID Lighting;
	RenderPassID ForwardOpaque;
	RenderPassID ForwardTransparent;
	const std::array<std::wstring, 6>& LightingInputs;
	const std::wstring& PointShadowInput;
	bool AcceptSceneMeshes = true;
};

class ENGINE_API DeferredSceneRenderer
{
public:
	DeferredSceneRenderer() = default;
	~DeferredSceneRenderer();
	DeferredSceneRenderer(const DeferredSceneRenderer&) = delete;
	DeferredSceneRenderer& operator=(const DeferredSceneRenderer&) = delete;

	bool Initialize();
	void Free();
	void Submit(const CameraBuffer& camera, Scene* scene, const DeferredScenePassBindings& passes);

private:
	SkyAtmosphereData BuildSkyAtmosphereData(Scene* scene);
	void SubmitLightingPass(const CameraBuffer& camera, Scene* scene, const SkyAtmosphereData& sky,
		const DeferredScenePassBindings& passes);

	RHIPipeline* m_LightingPipeline = nullptr;
	RHIBuffer* m_LocalLightBuffer[MAX_SWAPCHAIN_BUFFERS] = {};
};

END
