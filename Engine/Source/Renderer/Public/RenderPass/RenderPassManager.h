#pragma once

#include "Base.h"
#include "RenderTypes.h"

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
	RenderPassID RegisterRenderPass(const wstring& name, uint32 priority, ERenderSortType sortType);
	RenderPassID GetRenderPassIDByName(const wstring& name);
	const vector<RenderPassInfo>& GetAllRenderPasses() const { return m_RenderPasses; }
private:
	void SortRenderPasses();
#pragma endregion

#pragma region Variable
private:
	vector<RenderPassInfo> m_RenderPasses;
	uint32 m_NextRenderPassID = { 0 };
#pragma endregion
};
END