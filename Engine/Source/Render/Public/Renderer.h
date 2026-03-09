#pragma once

#include "Base.h"
#include "Structs.h"
#include "RHI.h"
#include "RenderTypes.h"
#include "RenderTarget.h"

BEGIN(Engine)

enum class ERHIType
{
	Unknown,
	SDLRenderer,
	SDLGPU,
	D3D11,
	D3D12,
	Vulkan,
	OpenGL,
	Metal
};

struct tagRendererDesc
{
	ERHIType RHIType = ERHIType::Unknown;
	tagRHIDesc RHIDesc = {};
};

struct tagViewportCameraInfo
{
	class Camera* Camera = { nullptr };
	RenderPassID PassID = { 0 };
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
#pragma endregion

#pragma region Queue Management
public:
	void Submit(class RenderComponent* component, RenderPassID passID);
	void SubmitAllPass(class RenderComponent* component);
	void SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID);
	void ClearRenderQueue(RenderPassID passID);
	void ClearAllRenderQueues();
#pragma endregion

#pragma region Viewport Camera Management
public:
	void RegisterViewportCamera(Camera* camera, RenderPassID passID);
	void UnregisterViewportCamera(RenderPassID passID);
	Camera* GetViewportCamera(RenderPassID passID) const;
	vector<tagViewportCameraInfo>& GetActiveViewportCameras() { return m_ViewportCameras; }
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




#ifdef _DEBUG
#pragma region Functions
public:
	void DrawDebugLine(const vec3& start, const vec3& end, const vec4& color);
	void DrawDebugRect(const Rect& rect, const vec4& color);
	void DrawDebugBox(const vec3& center, const vec3& extent, const vec4& color, const mat4& transform = glm::identity<mat4>());
	void DrawDebugSphere(const vec3& center, float radius, const vec4& color);
private:
	EResult RenderDebugLines(f32 dt);
#pragma endregion

#pragma region Variable
private:
	vector<DebugVertex> m_DebugVertices;
	class RHIBuffer* m_DebugVertexBuffer = { nullptr };
	class RHIPipeline* m_DebugPipeline = { nullptr };
#pragma endregion
#endif // _DEBUG


#pragma region Variable
private:
	class RHI* m_RHI = { nullptr };
	ERHIType m_RHIType = ERHIType::Unknown;
	map<RenderPassID, RenderDelegate> m_RenderPassDelegates;
	map<RenderPassID, vector<class RenderComponent*>> m_RenderQueues;
	map<RenderPassID, vector<CustomRenderCommand>> m_CustomRenderQueues;
	RenderTarget* m_SceneBuffer = { nullptr };
	RenderTarget* m_DepthBuffer = { nullptr };
private:
	class RenderPassManager* m_RenderPassManager = { nullptr };
private:
	vector<tagViewportCameraInfo> m_ViewportCameras;
#pragma endregion
};
END