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
	m_RenderPasses.clear();
}
#pragma endregion


#pragma region RenderPass Management
RenderPassID RenderPassManager::RegisterRenderPass(const wstring& name, uint32 priority, ERenderSortType sortType)
{
	RenderPassInfo newPass = {};
	newPass.ID = m_NextRenderPassID++;
	newPass.Name = name;
	newPass.Priority = priority;
	newPass.SortType = sortType;
	m_RenderPasses.push_back(newPass);
	SortRenderPasses();
	return newPass.ID;
}

RenderPassID RenderPassManager::GetRenderPassIDByName(const wstring& name)
{
	for (const auto& pass : m_RenderPasses)
	{
		if (pass.Name == name)
		{
			return pass.ID;
		}
	}
	return INVALID_PASS_ID;
}

void RenderPassManager::SortRenderPasses()
{
	std::sort(m_RenderPasses.begin(), m_RenderPasses.end(),
		[](const RenderPassInfo& a, const RenderPassInfo& b)
		{
			if (a.Priority != b.Priority)
			{
				return a.Priority < b.Priority;
			}
			return a.ID < b.ID;
		});
}

#pragma endregion
