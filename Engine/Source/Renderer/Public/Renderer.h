#pragma once

#include "Base.h"
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


class ENGINE_API Renderer final : public Base
{
	DECLARE_SINGLETON(Renderer)
	using DESC = tagRendererDesc;
	friend class Runtime;
	using RenderDelegate = MulticastDelegate<f32>;
#pragma region Struct
public:
	
#pragma endregion

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
	EResult RenderComponents(f32 dt, vector<class RenderComponent*> queue, ERenderSortType sortType);
#pragma endregion

#pragma region Queue Management
public:
	void Submit(class RenderComponent* component, RenderPassID passID);
	void ClearRenderQueue(RenderPassID passID);
	void ClearAllRenderQueues();
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
	RenderTarget* m_SceneBuffer = { nullptr };
private:
	class RenderPassManager* m_RenderPassManager = { nullptr };
#pragma endregion
};
END