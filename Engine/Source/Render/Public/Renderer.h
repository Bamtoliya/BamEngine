#pragma once

#include "Base.h"
#include "Structs.h"
#include "RHIs.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include "FrustumCuller.h"
#include "RenderCommands.h"
#include "CameraBuffer.h"
#include "Components/Render/SkyAtmosphereComponent.h"

struct RHIPipelineDesc;

BEGIN(Engine)
enum class ERHIType
{
	Unknown,
	SDLGPU,
	DirectX12,		// LH, Z:[0,1], Y-UP Row-Major (adjust needed)
	Vulkan,			// LH, Z:[0,1], Y-DOWN Column-Major
	Metal			// LH, Z:[0,1], Y-UP Column-Major
};

struct RendererDesc
{
	ERHIType rhiType = ERHIType::Unknown;
	RHIDesc* rhiDesc = {};
};

struct ViewportCameraInfo
{
	CameraBuffer cameraBuffer;
	bool hasCamera = false;
	class RenderPass* renderPass = { nullptr };
	class Scene* uiScene = nullptr;
	entt::entity uiCanvas = entt::null;
	bool filterUICanvas = false;
};

class ENGINE_API Renderer final : public Base
{
	DECLARE_SINGLETON(Renderer)
	using DESC = RendererDesc;
	friend class Runtime;
	using RenderDelegate = MulticastDelegate<f32>;
	using CustomRenderCommand = function<EResult(f32, RenderPass*)>;
#pragma region Constructor&Destructor
private:
	Renderer() {}
	virtual ~Renderer() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Render
private:
	EResult BeginFrame();
	EResult EndFrame();
	EResult Render(f32 dt);
private: //Entity Render
	EResult RenderMeshes(const vector<StaticDrawCommand>& commands, RenderPass* renderPass, const Frustum* frustum = nullptr);
	EResult RenderSkinnedMeshes(f32 dt, vector<SkinnedDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass);
	EResult RenderSprite(f32 dt, vector<SpriteDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass);
	EResult RenderUIImages(const vector<UIImageDrawCommand>& commands, RenderPass* renderPass);
	void ClearFrameQueues();
#pragma endregion

#pragma region Queue Management
public:
	void SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID);
	void ClearRenderQueue(RenderPassID passID);
	void ClearAllRenderQueues();
public: //Entity Submit
	void SubmitStatic(const StaticDrawCommand& command, RenderPassID passID);
	void SubmitSkinned(const SkinnedDrawCommand& command, RenderPassID passID);
	void SubmitSprite(const SpriteDrawCommand& command, RenderPassID passID);
	void SubmitUIImage(const UIImageDrawCommand& command, RenderPassID passID);

#pragma endregion

#pragma region Viewport Camera Management
public:
	void RegisterViewportCamera(const CameraBuffer& cameraBuffer, RenderPass* renderPass);
	void RegisterViewportCamera(RenderPass* renderPass);
	void RegisterUICanvas(class Scene* scene, entt::entity canvas, RenderPass* renderPass);
	void UnregisterViewportCamera(RenderPassID passID);
	vector<ViewportCameraInfo>& GetActiveViewportCameras() { return m_ViewportCameras; }
#pragma endregion

#pragma region Binding
private:
	EResult PreparePassPipelineDesc(RenderPass* renderPass, RHIPipelineDesc& desc);
	EResult BindMeshPipeline(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc);
	EResult PrepareMeshPipelineDesc(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc);
public:
	EResult BindMeshPipeline(class Mesh*, class MaterialInterface*, class RenderPass*);
	EResult ValidateMeshMaterial(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass);
#pragma endregion

#pragma region Getter
public:
	class RHI* GetRHI() const { return m_RHI; }
	RenderDelegate& GetRenderPassDelegate(RenderPassID passID)
	{
		return m_RenderPassDelegates[passID];
	}
	ERHIType GetRHIType() const { return m_RHIType; }
#pragma endregion

#pragma region Variable
private:
	class RHI* m_RHI = { nullptr };
	ERHIType m_RHIType = ERHIType::Unknown;
	map<RenderPassID, RenderDelegate> m_RenderPassDelegates;
	map<RenderPassID, vector<CustomRenderCommand>> m_CustomRenderQueues;
private: //ECS Based Render Queue
	map<RenderPassID, vector<StaticDrawCommand>> m_StaticDrawCommands;
	map<RenderPassID, vector<SkinnedDrawCommand>> m_SkinnedDrawCommands;
	map<RenderPassID, vector<SpriteDrawCommand>> m_SpriteDrawCommands;
	map<RenderPassID, vector<UIImageDrawCommand>> m_UIImageDrawCommands;
private:
	class RenderPassManager* m_RenderPassManager = { nullptr };
private:
	struct ViewportJob
	{
		CameraBuffer cameraBuffer = {};
		bool hasCamera = false;
	};

	vector<ViewportCameraInfo> m_ViewportCameras;
	unordered_map<RenderPassID, vector<ViewportJob>> m_ViewportJobsPerPass;
#pragma endregion
};
END
