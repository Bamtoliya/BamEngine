#pragma once

#include "Base.h"
#include "Structs.h"
#include "RHIs.h"
#include "RenderTypes.h"
#include "RenderTarget.h"
#include "FrustumCuller.h"

BEGIN(Engine)

enum class ERHIType
{
	Unknown,
	SDLGPU,
	DirectX12,		// LH, Z:[0,1], Y-UP Row-Major (adjust needed)
	Vulkan,			// LH, Z:[0,1], Y-DOWN Column-Major
	Metal			// LH, Z:[0,1], Y-UP Column-Major
};

struct tagRendererDesc
{
	ERHIType RHIType = ERHIType::Unknown;
	tagRHIDesc* RHIDesc = {};
};

struct tagViewportCameraInfo
{
	class Camera* Camera = { nullptr };
	class RenderPass* RenderPass = { nullptr };
};


class ENGINE_API Renderer final : public Base
{
	DECLARE_SINGLETON(Renderer)
	using DESC = tagRendererDesc;
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
#pragma endregion

#pragma region Queue Management
public:
	void Submit(class RenderComponent* component, RenderPassID passID);
	void SubmitAllPass(class RenderComponent* component);
	void SubmitUI(class UIRenderComponent* uiRenderer, RenderPassID passID);
	void SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID);
	void ClearRenderQueue(RenderPassID passID);
	void ClearAllRenderQueues();
#pragma endregion

#pragma region Viewport Camera Management
public:
	void RegisterViewportCamera(Camera* camera, RenderPass* renderPass);
	void UnregisterViewportCamera(RenderPassID passID);
	Camera* GetViewportCamera(RenderPassID passID) const;
	vector<tagViewportCameraInfo>& GetActiveViewportCameras() { return m_ViewportCameras; }
#pragma endregion

#pragma region Frustum Culling
public:
	bool TryGetPassFrustum(RenderPassID passID, tagFrustum& outFrustum, bool& outIsShadow) const;
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
private:
	class RenderPassManager* m_RenderPassManager = { nullptr };
private:
	vector<tagViewportCameraInfo> m_ViewportCameras;
private:
	unordered_map<RenderPassID, tagFrustum> m_PassFrustums;
	unordered_map<RenderPassID, bool>    m_PassFrustumIsShadow;
#pragma endregion
};
END