#pragma once

#include "Base.h"
#include "RHI.h"
#include "RenderTypes.h"

BEGIN(Engine)

enum class ERHIType
{
	Unknown,
	SDLRenderer,
	D3D11,
	D3D12,
	Vulkan,
	OpenGL,
	Metal
};


class ENGINE_API Renderer final : public Base
{
	DECLARE_SINGLETON(Renderer)
	friend class Runtime;
	using RenderDelegate = MulticastDelegate<f32>;
#pragma region Struct
public:
	typedef struct tagRendererCreateInfo
	{
		ERHIType RHIType = ERHIType::Unknown;
		void* WindowHandle = nullptr;
		uint32 Width = 0;
		uint32 Height = 0;
		bool IsVSync = true;
	} RENDERERDESC;
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
#pragma endregion

#pragma region Variable
private:
	class RHI* m_RHI = { nullptr };
	map<RenderPassID, RenderDelegate> m_RenderPassDelegates;
	map<RenderPassID, vector<class RenderComponent*>> m_RenderQueues;
#pragma endregion
};
END