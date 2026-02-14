#pragma once

#include "RenderTargetManager.h"
#include "RenderTarget.h"

IMPLEMENT_SINGLETON(RenderTargetManager)

#pragma region Constructor&Destructor
EResult RenderTargetManager::Initialize(void* arg)
{
	return EResult::Success;
}
void RenderTargetManager::Free()
{
	for (auto& [name, renderTarget] : m_RenderTargets)
	{
		Safe_Release(renderTarget);
	}
	m_RenderTargets.clear();
}
#pragma endregion


#pragma region Render Target Management
RenderTarget* RenderTargetManager::CreateRenderTarget(void* arg)
{
	RenderTarget* renderTarget = RenderTarget::Create(arg);
	if (!renderTarget)
		return nullptr;
	m_RenderTargets[renderTarget->GetName()] = renderTarget;
	return renderTarget;
}

void RenderTargetManager::RemoveRenderTarget(const wstring& name)
{
	auto it = m_RenderTargets.find(name);
	if (it != m_RenderTargets.end())
	{
		Safe_Release(it->second);
		m_RenderTargets.erase(it);
	}
}

RenderTarget* RenderTargetManager::GetRenderTarget(const wstring& name) const
{
	auto it = m_RenderTargets.find(name);
	if (it != m_RenderTargets.end())
	{
		return it->second;
	}
	return nullptr;
}

void RenderTargetManager::ResizeRenderTarget(const wstring& name, uint32 width, uint32 height)
{
	RenderTarget* renderTarget = GetRenderTarget(name);
	if (renderTarget)
	{
		renderTarget->Resize(width, height);
	}
}

void RenderTargetManager::ResizeAllRenderTargets(uint32 width, uint32 height)
{
	for (auto& [name, renderTarget] : m_RenderTargets)
	{
		if (renderTarget)
		{
			renderTarget->Resize(width, height);
		}
	}
}
#pragma endregion
