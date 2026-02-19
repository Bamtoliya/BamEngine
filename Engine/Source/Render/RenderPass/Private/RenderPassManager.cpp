#pragma once
#include "RenderPassManager.h"

IMPLEMENT_SINGLETON(RenderPassManager)

#pragma region Constructor&Destructor
EResult RenderPassManager::Initialize(void* arg)
{
	return EResult::Success;
}

void RenderPassManager::Free()
{
	for (auto& pass : m_RenderPasses)
	{
		Safe_Release(pass);
	}
	m_RenderPasses.clear();
}
#pragma endregion


#pragma region RenderPass Management
RenderPassID RenderPassManager::RegisterRenderPass(const wstring& name, vector<wstring> renderTargetNames, const wstring& depthstencilName,
	ERenderPassLoadOperation loadOperation,
	ERenderPassStoreOperation storeOperation,
	vec4 overrideClearColor,
	uint32 priority, ERenderSortType sortType)
{
	tagRenderPassDesc newPass = {};
	newPass.ID = m_NextRenderPassID++;
	newPass.Name = name;
	newPass.RenderTargetNames = renderTargetNames;
	newPass.DepthStencilName = depthstencilName;
	newPass.Priority = priority;
	newPass.SortType = sortType;
	newPass.LoadOperation = loadOperation;
	newPass.StoreOperation = storeOperation;
	newPass.OverrideClearColor = overrideClearColor;
	RenderPass* pass = RenderPass::Create(&newPass);
	m_RenderPasses.push_back(pass);
	SortRenderPasses();
	return newPass.ID;
}

RenderPassID RenderPassManager::GetRenderPassIDByName(const wstring& name)
{
	for (const auto& pass : m_RenderPasses)
	{
		if (pass->GetName() == name)
		{
			return pass->GetID();
		}
	}
	return INVALID_PASS_ID;
}

void RenderPassManager::SortRenderPasses()
{
	std::sort(m_RenderPasses.begin(), m_RenderPasses.end(),
		[](const RenderPass* a, const RenderPass* b)
		{
			if (a->GetPriority() != b->GetPriority())
			{
				return a->GetPriority() < b->GetPriority();
			}
			return a->GetID() < b->GetID();
		});
}

#pragma endregion
