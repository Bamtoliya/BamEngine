#pragma once

#include "Renderer.h"
#include "RHI.h"
#include "SDLRendererRHI.h"
#include "RenderPassManager.h"
#include "RenderComponent.h"

IMPLEMENT_SINGLETON(Renderer)

#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{
	RENDERERDESC* pDesc = reinterpret_cast<RENDERERDESC*>(arg);
	RHICREATEINFO RHIDesc = {};

	RHIDesc.WindowHandle = pDesc->WindowHandle;
	RHIDesc.Width = pDesc->Width;
	RHIDesc.Height = pDesc->Height;
	RHIDesc.IsVSync = pDesc->IsVSync;

	if (!pDesc) return EResult::Fail;
	switch (pDesc->RHIType)
	{
	case ERHIType::SDLRenderer:
		m_RHI = SDLRendererRHI::Create(&RHIDesc);
		break;
	default:
		return EResult::Fail;
	}
	if (!m_RHI) return EResult::Fail;

	return EResult::Success;
}

void Renderer::Free()
{	
	Safe_Release(m_RHI);
}
#pragma endregion


#pragma region Render Manangement
EResult Renderer::BeginFrame()
{
	if (m_RHI)
	{
		return m_RHI->BeginFrame();
	}
	return EResult::Fail;
}

EResult Renderer::Render(f32 dt)
{
	RenderPassManager& renderPassManager = RenderPassManager::Get();
	const vector<RenderPassInfo>& renderPasses = renderPassManager.GetAllRenderPasses();
	for (const auto& pass : renderPasses)
	{
		auto it = m_RenderQueues.find(pass.ID);
		if (it != m_RenderQueues.end())
		{
			RenderComponents(dt, it->second, pass.SortType);
		}
		GetRenderPassDelegate(pass.ID).Broadcast(dt);
	}
	return EResult::Success;
}

EResult Renderer::RenderComponents(f32 dt, vector<class RenderComponent*> queue, ERenderSortType sortType)
{
	for (auto* component : queue)
	{
		if (component)
		{
			component->Render(dt);
		}
	}
	return EResult::Success;
}

EResult Renderer::EndFrame()
{
	if (m_RHI)
	{
		return m_RHI->EndFrame();
	}

	for (auto& pair : m_RenderQueues)
	{
		RELEASE_VECTOR(pair.second);
	}
	return EResult::Fail;
}
#pragma endregion


#pragma region Queue Management
void Renderer::Submit(class RenderComponent* component, RenderPassID passID)
{
	Safe_AddRef(component);
	m_RenderQueues[passID].push_back(component);
}

void Renderer::ClearRenderQueue(RenderPassID passID)
{
	auto it = m_RenderQueues.find(passID);
	if (it != m_RenderQueues.end())
	{
		RELEASE_VECTOR(it->second);
		m_RenderQueues.erase(it);
	}
}

void Renderer::ClearAllRenderQueues()
{
	for (auto& pair : m_RenderQueues)
	{
		RELEASE_VECTOR(pair.second);
	}
	m_RenderQueues.clear();
}
#pragma endregion

