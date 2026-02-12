#pragma once

#include "Base.h"
#include "RenderPass.h"

BEGIN(Engine)
class ENGINE_API RenderPassManager final : public Base
{
	DECLARE_SINGLETON(RenderPassManager)
#pragma region Constructor&Destructor
	private:
	RenderPassManager() {}
	virtual ~RenderPassManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region RenderPass Management
public:
	RenderPassID RegisterRenderPass(const wstring& name, vector<wstring> renderTargetNames, const wstring& depthstencilName,
		ERenderPassLoadOperation loadOperation = ERenderPassLoadOperation::RPLO_Load,
		ERenderPassStoreOperation storeOperation = ERenderPassStoreOperation::RPSO_Store,
		vec4 overrideClearColor = vec4(0.0f, 0.0f, 0.0f, -1.0f),
		uint32 priority = 0, ERenderSortType sortType = ERenderSortType::None);
	RenderPassID GetRenderPassIDByName(const wstring& name);
	const vector<RenderPass*>& GetAllRenderPasses() const { return m_RenderPasses; }
private:
	void SortRenderPasses();
#pragma endregion

#pragma region Variable
private:
	vector<RenderPass*> m_RenderPasses;
	uint32 m_NextRenderPassID = { 0 };
#pragma endregion
};
END