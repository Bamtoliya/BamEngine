#pragma once

#include "Renderer.h"
#include "RHI.h"

#include "SDLGPURHI.h"
#include "SDLRendererRHI.h"

#include "ResourceManager.h"

#include "PipelineManager.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "RenderComponent.h"

#include "RHIPipeline.h"

#include "Shader.h"
#include "Camera.h"

#include "Transform.h"

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

	tagRenderTargetDesc rtDesc;
	rtDesc.Width = desc->RHIDesc.Width;
	rtDesc.Height = desc->RHIDesc.Height;
	rtDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource | ERenderTargetBindFlag::RTBF_RenderTarget;
	m_SceneBuffer = RenderTargetManager::Get().CreateRenderTarget(&rtDesc);
	tagRenderTargetDesc depthStencilDesc;
	depthStencilDesc.Width = desc->RHIDesc.Width;
	depthStencilDesc.Height = desc->RHIDesc.Height;
	depthStencilDesc.Type = ERenderTargetType::DepthStencil;
	depthStencilDesc.Usage = ERenderTargetUsage::RTU_DepthStencil;
	depthStencilDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource | ERenderTargetBindFlag::RTBF_DepthStencil | ERenderTargetBindFlag::RTBF_RenderTarget;
	depthStencilDesc.Format = ERenderTargetFormat::RTF_DEPTH24STENCIL8;
	m_DepthBuffer = RenderTargetManager::Get().CreateRenderTarget(&depthStencilDesc);
	RenderPassManager::Get().RegisterRenderPass(L"MainPass", { L"RenderTarget_1" }, L"RenderTarget_2", ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store, vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, ERenderSortType::FrontToBack);

	if (!m_SceneBuffer) return EResult::Fail;

	return EResult::Success;
}

void Renderer::Free()
{	
	m_RenderPassManager = nullptr;
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
	if (!m_RenderPassManager)
		m_RenderPassManager = &RenderPassManager::Get();
	if (m_RHI)
	{
		return m_RHI->BeginFrame();
	}
	return EResult::Fail;
}



EResult Renderer::Render(f32 dt)
{
	unordered_map<RenderPassID, vector<Camera*>> jobsPerPass;

	for (const auto& viewportInfo : m_ViewportCameras)
	{
		jobsPerPass[viewportInfo.PassID].push_back(viewportInfo.Camera);
	}

	const vector<RenderPass*>& renderPasses = m_RenderPassManager->GetAllRenderPasses();
	for (const auto& pass : renderPasses)
	{
		auto jobsIt = jobsPerPass.find(pass->GetID());
		if (jobsIt != jobsPerPass.end())
		{
			const vector<Camera*>& cameras = jobsIt->second;
			for (Camera* cam : cameras)
			{
				tagCameraBuffer cameraBuffer = cam->GetCameraBuffer();
				cameraBuffer.time = dt;
				m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(tagCameraBuffer), 0);
				if (IsFailure(m_RHI->BeginRenderPass(pass)))
					return EResult::Fail;
				m_RHI->SetViewport(0, 0, 1920, 1080);
				auto it = m_RenderQueues.find(pass->GetID());
				if (it != m_RenderQueues.end())
				{
					RenderComponents(dt, it->second, pass->GetSortType(), pass);
				}
				if (IsFailure(m_RHI->EndRenderPass()))
					return EResult::Fail;
			}
		}
		if (IsFailure(m_RHI->EndRenderPass()))
			return EResult::Fail;
		GetRenderPassDelegate(pass->GetID()).Broadcast(dt);
	}
	return EResult::Success;
}

EResult Renderer::RenderComponents(f32 dt, vector<class RenderComponent*> queue, ERenderSortType sortType, RenderPass* renderPass)
{
	for (auto* component : queue)
	{
		if (component)
		{
			component->Render(dt, renderPass);
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
	m_RenderQueues.clear();
	m_ViewportCameras.clear();
	return EResult::Success;
}
#pragma endregion


#pragma region Queue Management
void Renderer::Submit(class RenderComponent* component, RenderPassID passID)
{
	Safe_AddRef(component);
	m_RenderQueues[passID].push_back(component);
}
void Renderer::SubmitAllPass(RenderComponent* component)
{
	for (auto& pair : m_RenderPassManager->GetAllRenderPasses())
	{
		m_RenderQueues[pair->GetID()].push_back(component);
		Safe_AddRef(component);
	}
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

#pragma region Viewport Camera Management

void Renderer::RegisterViewportCamera(Camera* camera, RenderPassID passID)
{
	m_ViewportCameras.push_back({ camera, passID });
}

void Renderer::UnregisterViewportCamera(RenderPassID passID)
{
	m_ViewportCameras.erase(std::remove_if(m_ViewportCameras.begin(), m_ViewportCameras.end(),
		[passID](const tagViewportCameraInfo& info) { return info.PassID == passID; }), m_ViewportCameras.end());
}

Camera* Renderer::GetViewportCamera(RenderPassID passID) const
{
	for (const auto& info : m_ViewportCameras)
	{
		if (info.PassID == passID)
		{
			return info.Camera;
		}
	}
	return nullptr;
}
#pragma endregion
