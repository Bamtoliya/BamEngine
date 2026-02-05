#pragma once

#include "RenderTarget.h"


BEGIN(Engine)
class ENGINE_API RenderTargetManager final : public Base
{
	DECLARE_SINGLETON(RenderTargetManager)
#pragma region Constructor&Destructor
private:
	RenderTargetManager() {}
	virtual ~RenderTargetManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Render Target Management
public:
	RenderTarget* CreateRenderTarget(void* arg = nullptr);
	void RemoveRenderTarget(const wstring& name);
	RenderTarget* GetRenderTarget(const wstring& name) const;
	void ResizeRenderTarget(const wstring& name, uint32 width, uint32 height);
	void ResizeAllRenderTargets(uint32 width, uint32 height);
#pragma endregion


#pragma region Members
private:
	unordered_map<wstring, RenderTarget*> m_RenderTargets;
#pragma endregion
};

END