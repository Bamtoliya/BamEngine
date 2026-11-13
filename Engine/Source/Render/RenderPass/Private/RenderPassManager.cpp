#pragma once
#include "RenderPassManager.h"
#include "Renderer.h"

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
	ERenderPassLoadOperation stencilLoadOperation,
	ERenderPassStoreOperation stencilStoreOperation,
	vec4 overrideClearColor,
	uint32 priority, ERenderSortType sortType, ERenderPassType passType, EBlendMode acceptedModes)
{
	RenderPassDesc newPass = {};
	newPass.name = name;
	newPass.renderTargetNames = renderTargetNames;
	newPass.depthStencilName = depthstencilName;
	newPass.priority = priority;
	newPass.sortType = sortType;
	newPass.loadOperation = loadOperation;
	newPass.storeOperation = storeOperation;
	newPass.stencilLoadOperation = stencilLoadOperation;
	newPass.stencilStoreOperation = stencilStoreOperation;
	newPass.overrideClearColor = overrideClearColor;
	newPass.passType = passType;
	newPass.acceptedBlendModes = acceptedModes;

	return RegisterRenderPass(std::move(newPass));
}

RenderPassID RenderPassManager::RegisterRenderPass(RenderPassDesc desc)
{
	if (m_NextRenderPassID == INVALID_PASS_ID)
		return INVALID_PASS_ID;

	desc.ID = m_NextRenderPassID;
	RenderPass* pass = RenderPass::Create(&desc);
	if (!pass)
		return INVALID_PASS_ID;

	++m_NextRenderPassID;
	m_RenderPasses.push_back(pass);
	SortRenderPasses();
	return desc.ID;
}

void RenderPassManager::RemoveRenderPass(RenderPassID id)
{
	const auto pass = std::find_if(m_RenderPasses.begin(), m_RenderPasses.end(),
		[id](const RenderPass* candidate) { return candidate->GetID() == id; });
	if (pass == m_RenderPasses.end())
		return;

	if (Renderer::IsCreated())
		Renderer::Get().ForgetRenderPass(id);
	Safe_Release(*pass);
	m_RenderPasses.erase(pass);
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

RenderPass* RenderPassManager::GetRenderPassByID(RenderPassID id)
{
	for (const auto& pass : m_RenderPasses)
	{
		if (pass->GetID() == id)
		{
			return pass;
		}
	}
	return nullptr;
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
