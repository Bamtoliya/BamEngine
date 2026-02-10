#pragma once

#include "Renderer.h"
#include "RHI.h"

#include "SDLGPURHI.h"
#include "SDLRendererRHI.h"

#include "ResourceManager.h"

#include "PipelineManager.h"
#include "RenderPassManager.h"
#include "RenderComponent.h"

#include "RHIPipeline.h"

#include "Shader.h"

IMPLEMENT_SINGLETON(Renderer)

#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{	
	if (!arg) return EResult::InvalidArgument;

	CAST_DESC
	tagRHIDesc RHIDesc = desc->RHIDesc;
	m_RHIType = desc->RHIType;
	switch (m_RHIType)
	{
	case ERHIType::SDLRenderer:
		m_RHI = SDLRendererRHI::Create(&RHIDesc);
		break;
	case ERHIType::SDLGPU:
		m_RHI = SDLGPURHI::Create(&RHIDesc);
		break;
	default:
		return EResult::Fail;
	}
	
	if (!m_RHI) return EResult::Fail;

	// 쉐이더 & 파이프라인 생성 (SDLGPU일 때)
	if (m_RHIType == ERHIType::SDLGPU)
	{
		
	}

	tagRenderTargetDesc rtDesc;
	rtDesc.Width = desc->RHIDesc.Width;
	rtDesc.Height = desc->RHIDesc.Height;
	m_SceneBuffer = RenderTargetManager::Get().CreateRenderTarget(&rtDesc);
	if (!m_SceneBuffer) return EResult::Fail;

	return EResult::Success;
}

void Renderer::Free()
{	
	Safe_Release(m_SceneBuffer);

	if (m_RHI)
	{
		//순환참조 때문에 명시적으로 해제
		m_RHI->Free();
		Safe_Release(m_RHI);
	}
}
#pragma endregion


#pragma region Render Management	
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
	if (m_SceneBuffer && m_RHI)
	{
		if(IsFailure(m_RHI->BindRenderTarget(m_SceneBuffer->GetTexture(0), m_SceneBuffer->GetDepthStencilTexture())))
		{
			return EResult::Fail;
		}
		if(IsFailure(m_RHI->SetViewport(0, 0, m_SceneBuffer->GetWidth(), m_SceneBuffer->GetHeight())))
		{
			return EResult::Fail;
		}
		vec4 clearColor = { 0.1f, 0.1f, 0.1f, 1.0f }; // 게임 배경색 (검정/회색)
		m_RHI->ClearRenderTarget(m_SceneBuffer->GetTexture(0), clearColor);
	}
	for (const auto& pass : renderPasses)
	{
		auto it = m_RenderQueues.find(pass.ID);
		if (it != m_RenderQueues.end())
		{
			RenderComponents(dt, it->second, pass.SortType);
		}
	}

	if (m_RHI)
	{
		m_RHI->BindRenderTarget(nullptr, nullptr);
		m_RHI->SetViewport(0, 0, m_RHI->GetSwapChainWidth(), m_RHI->GetSwapChainHeight());
		m_RHI->DrawTexture(m_SceneBuffer->GetTexture(0));
		if (m_RHIType == ERHIType::SDLGPU)
		{
			static_cast<SDLGPURHI*>(m_RHI)->ClearRenderPass();
		}
	}

	for (const auto& pass : renderPasses)
	{
		auto it = m_RenderQueues.find(pass.ID);
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
		m_RHI->BindRenderTarget(nullptr, nullptr);
		m_RHI->SetViewport(0, 0, m_RHI->GetSwapChainWidth(), m_RHI->GetSwapChainHeight());
		m_RHI->EndFrame();
	}

	for (auto& pair : m_RenderQueues)
	{
		RELEASE_VECTOR(pair.second);
	}
	return EResult::Success;
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

