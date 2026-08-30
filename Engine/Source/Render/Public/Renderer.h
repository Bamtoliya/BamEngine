#pragma once

#include "Base.h"
#include "Structs.h"
#include "RHIs.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include "FrustumCuller.h"
#include "RenderCommands.h"

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
	class Camera* camera = { nullptr };
	class RenderPass* renderPass = { nullptr };
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
	EResult RenderComponents(f32 dt, vector<class RenderComponent*> queue, ERenderSortType sortType, RenderPass* renderPass);
	EResult RenderUIComponents(f32 dt, vector<class UIRenderComponent*> queue, ERenderSortType sortType, RenderPass* renderPass);
private: //Entity Render
	EResult RenderStatic(f32 dt, vector<StaticDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass);
	EResult RenderSkinned(f32 dt, vector<SkinnedDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass);
	EResult RenderSprite(f32 dt, vector<SpriteDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass);
#pragma endregion

#pragma region Queue Management
public:
	void Submit(class RenderComponent* component, RenderPassID passID);
	void SubmitAllPass(class RenderComponent* component);
	void SubmitUI(class UIRenderComponent* uiRenderer, RenderPassID passID);
	void SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID);
	void ClearRenderQueue(RenderPassID passID);
	void ClearAllRenderQueues();
public: //Entity Submit
	void SubmitStatic(const StaticDrawCommand& command, RenderPassID passID);
	void SubmitSkinned(const SkinnedDrawCommand& command, RenderPassID passID);
	void SubmitSprite(const SpriteDrawCommand& command, RenderPassID passID);

#pragma endregion

#pragma region Viewport Camera Management
public:
	void RegisterViewportCamera(Camera* camera, RenderPass* renderPass);
	void UnregisterViewportCamera(RenderPassID passID);
	Camera* GetViewportCamera(RenderPassID passID) const;
	vector<ViewportCameraInfo>& GetActiveViewportCameras() { return m_ViewportCameras; }
#pragma endregion

#pragma region Frustum Culling
public:
	bool TryGetPassFrustum(RenderPassID passID, Frustum& outFrustum, bool& outIsShadow) const;
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
	map<RenderPassID, vector<class RenderComponent*>> m_RenderQueues;
	map<RenderPassID, vector<class UIRenderComponent*>> m_UIRenderQueues;
	map<RenderPassID, vector<CustomRenderCommand>> m_CustomRenderQueues;

private: //ECS Based Render Queue
	map<RenderPassID, vector<StaticDrawCommand>> m_StaticDrawCommands;
	map<RenderPassID, vector<SkinnedDrawCommand>> m_SkinnedDrawCommands;
	map<RenderPassID, vector<SpriteDrawCommand>> m_SpriteDrawCommands;
private:
	class RenderPassManager* m_RenderPassManager = { nullptr };
private:
	vector<ViewportCameraInfo> m_ViewportCameras;
private:
	unordered_map<RenderPassID, Frustum> m_PassFrustums;
	unordered_map<RenderPassID, bool>    m_PassFrustumIsShadow;
#pragma endregion
};
END