#pragma once

#include "RenderTarget.h"


BEGIN(Engine)
class ENGINE_API RenderTargetManager final : public Base
{
	DECLARE_SINGLETON(RenderTargetManager)
#pragma region Constructor&Destructor
private:
	EResult Initialize(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion
public:
	RenderTarget* CreateRenderTarget(void* arg = nullptr);
#pragma region Variables
private:
	std::vector<RenderTarget*> m_RenderTargets;
#pragma endregion
};

END