#pragma once

#include "Renderer.h"
#include "RHI.h"

#include "SDLGPURHI.h"

#include "ResourceManager.h"

#include "PipelineManager.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "RenderComponent.h"
#include "UIRenderComponent.h"

#include "RHIPipeline.h"

#include "GameObject.h"

#include "Shader.h"
#include "Camera.h"

#include "Transform.h"
#include "LightManager.h"

IMPLEMENT_SINGLETON(Renderer)

#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{	
	if (!arg) return EResult::InvalidArgument;

	CAST_DESC
	m_RHIType = desc->RHIType;
	switch (m_RHIType)
	{
	case ERHIType::SDLGPU:
		m_RHI = SDLGPURHI::Create(desc->RHIDesc);
		break;
	default:
		return EResult::Fail;
	}
	
	if (!m_RHI) return EResult::Fail;

	return EResult::Success;
}

void Renderer::Free()
{	
	m_RenderPassManager = nullptr;
	//Safe_Release(m_SceneBuffer);
	//Safe_Release(m_DepthBuffer);

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
	uint32 rtWidth = m_RHI->GetSwapChainWidth();
	uint32 rtHeight = m_RHI->GetSwapChainHeight();
	for (const auto& viewportInfo : m_ViewportCameras)
	{
    	if (!viewportInfo.RenderPass) continue;
    	jobsPerPass[viewportInfo.RenderPass->GetID()].push_back(viewportInfo.Camera);
	}

	const vector<RenderPass*>& renderPasses = m_RenderPassManager->GetAllRenderPasses();

	for (const auto& pass : renderPasses)
	{
		const ERenderPassType passType = pass->GetPassType();
		if (passType == ERenderPassType::Shadow && LightManager::Get().GetShadowCastingLights().empty())
			continue;
		auto jobsIt = jobsPerPass.find(pass->GetID());

		vector<Camera*> cameras;
		if (jobsIt != jobsPerPass.end())
			cameras = jobsIt->second;
		else
			cameras.push_back(nullptr);

		for (Camera* cam : cameras)
		{
			tagCameraBuffer cameraBuffer = {};
			
			if (passType == ERenderPassType::Shadow)
			{
				cameraBuffer = LightManager::Get().GetShadowCameraBuffer(uint32(0));
			}
			else if (passType == ERenderPassType::UI)
			{
				cameraBuffer.viewMatrix = glm::identity<mat4>();
				cameraBuffer.projMatrix = glm::ortho(0.0f, static_cast<f32>(rtWidth), static_cast<f32>(rtHeight), 0.0f, -1.0f, 1.0f);
				cameraBuffer.viewProjMatrix = cameraBuffer.projMatrix * cameraBuffer.viewMatrix;
				cameraBuffer.cameraPosition = vec3(0.f);
			}
			else if (cam)
			{
				cameraBuffer = cam->GetCameraBuffer();
			}
			else
			{
				cameraBuffer.viewMatrix = glm::identity<mat4>();
				cameraBuffer.projMatrix = glm::identity<mat4>();
				cameraBuffer.viewProjMatrix = glm::identity<mat4>();
				cameraBuffer.cameraPosition = vec3(0.f);
			}
			cameraBuffer.time = dt;
			m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(tagCameraBuffer), 0, EShaderType::Vertex);
			m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(tagCameraBuffer), 0, EShaderType::Pixel);

			if (IsFailure(m_RHI->BeginRenderPass(pass)))
				continue;

			if (pass->GetRenderTargetCount() > 0)
			{
				RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(pass->GetRenderTargetName(0));
				if (rt)
				{
					rtWidth = rt->GetWidth();
					rtHeight = rt->GetHeight();
				}
			}
			else if (!pass->GetDepthStencilName().empty())
			{
				RenderTarget* rt = RenderTargetManager::Get().GetRenderTarget(pass->GetDepthStencilName());
				if (rt)
				{
					rtWidth = rt->GetWidth();
					rtHeight = rt->GetHeight();
				}
			}
			m_RHI->SetViewport(0, 0, rtWidth, rtHeight);


			if (passType == ERenderPassType::UI)
			{
				auto uiIt = m_UIRenderQueues.find(pass->GetID());
				if (uiIt != m_UIRenderQueues.end())
					RenderUIComponents(dt, uiIt->second, pass->GetSortType(), pass);
			}
			else
			{
				auto it = m_RenderQueues.find(pass->GetID());
				if (it != m_RenderQueues.end())
					RenderComponents(dt, it->second, pass->GetSortType(), pass);
			}
			
			auto customIt = m_CustomRenderQueues.find(pass->GetID());
			if (customIt != m_CustomRenderQueues.end())
			{
				for (const auto& command : customIt->second)
					command(dt, pass);
			}

			m_RHI->EndRenderPass();
		}

		GetRenderPassDelegate(pass->GetID()).Broadcast(dt);
	}

	return EResult::Success;
}

EResult Renderer::RenderComponents(f32 dt, vector<class RenderComponent*> queue, ERenderSortType sortType, RenderPass* renderPass)
{
	if (sortType != ERenderSortType::None && !queue.empty())
	{
		const Camera* cam = GetViewportCamera(renderPass->GetID());
		const vec3 camPos = cam ? cam->GetCameraBuffer().cameraPosition : vec3(0.f);

		const bool bBackToFront = (sortType == ERenderSortType::BackToFront);

		std::sort(queue.begin(), queue.end(),
			[&camPos, bBackToFront](const RenderComponent* a, const RenderComponent* b)
			{
				Transform* ta = a->GetOwner()->GetComponent<Transform>();
				Transform* tb = b->GetOwner()->GetComponent<Transform>();
				if (!ta || !tb) return false;
				const float da = glm::distance2(vec3(ta->GetWorldMatrix()[3]), camPos);
				const float db = glm::distance2(vec3(tb->GetWorldMatrix()[3]), camPos);
				return bBackToFront ? (da > db) : (da < db);
			});
	}

	for (auto* component : queue)
	{
		if (component)
		{
			if (IsFailure(component->Render(dt, renderPass)))
			{
				ENGINE_LOG_ERROR("Component Render Failed! Component Name: {0}", WStrToStr(component->GetOwner()->GetName()));
				continue;
			}
		}
	}
	return EResult::Success;
}

EResult Renderer::RenderUIComponents(f32 dt, vector<class UIRenderComponent*> queue, ERenderSortType sortType, RenderPass* renderPass)
{

	for (auto* component : queue)
	{
		if (component)
		{
			if (IsFailure(component->Render(dt, renderPass)))
			{
				ENGINE_LOG_ERROR("UI Component Render Failed! Component Name: {0}", WStrToStr(component->GetOwner()->GetName()));
				continue;
			}
		}
	}
	return EResult();
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
	m_UIRenderQueues.clear();
	m_CustomRenderQueues.clear();
	m_ViewportCameras.clear(); 
	m_PassFrustums.clear();
	m_PassFrustumIsShadow.clear();
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
void Renderer::SubmitUI(UIRenderComponent* uiRenderer, RenderPassID passID)
{
	Safe_AddRef(uiRenderer);
	m_UIRenderQueues[passID].push_back(uiRenderer);
}
void Renderer::SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID)
{
	m_CustomRenderQueues[passID].push_back(command);
}

void Renderer::ClearRenderQueue(RenderPassID passID)
{
	auto it = m_RenderQueues.find(passID);
	if (it != m_RenderQueues.end())
	{
		RELEASE_VECTOR(it->second);
		m_RenderQueues.erase(it);
	}
	auto customIt = m_CustomRenderQueues.find(passID);
	if (customIt != m_CustomRenderQueues.end())
	{
		customIt->second.clear();
		m_CustomRenderQueues.erase(customIt);
	}
}

void Renderer::ClearAllRenderQueues()
{
	for (auto& pair : m_RenderQueues)
	{
		RELEASE_VECTOR(pair.second);
	}
	m_RenderQueues.clear();
	for (auto& pair : m_CustomRenderQueues)
	{
		pair.second.clear();
	}
	m_CustomRenderQueues.clear();
}
#pragma endregion

#pragma region Viewport Camera Management

void Renderer::RegisterViewportCamera(Camera* camera, RenderPass* renderPass)
{
	m_ViewportCameras.push_back({ camera, renderPass });
	if (!renderPass) return;

	const RenderPassID passID = renderPass->GetID();
	const bool isShadow = (renderPass->GetPassType() == ERenderPassType::Shadow);

	if (isShadow)
	{
		// Shadow Pass: 라이트의 View-Projection 기준으로 프러스텀 생성
		if (!LightManager::Get().GetShadowCastingLights().empty())
		{
			m_PassFrustums[passID] = FrustumCuller::ExtractFrustum(
				LightManager::Get().GetShadowCameraBuffer(uint32(0)).viewProjMatrix);
			m_PassFrustumIsShadow[passID] = true;
		}
	}
	else if (camera)
	{
		// 일반 Pass: 카메라의 View-Projection 기준으로 프러스텀 생성
		m_PassFrustums[passID] = FrustumCuller::ExtractFrustum(
			camera->GetCameraBuffer().viewProjMatrix);
		m_PassFrustumIsShadow[passID] = false;
	}
}

void Renderer::UnregisterViewportCamera(RenderPassID passID)
{
	m_ViewportCameras.erase(std::remove_if(m_ViewportCameras.begin(), m_ViewportCameras.end(),
		[passID](const tagViewportCameraInfo& info) { return info.RenderPass->GetID() == passID; }), m_ViewportCameras.end());
}

Camera* Renderer::GetViewportCamera(RenderPassID passID) const
{
	for (const auto& info : m_ViewportCameras)
	{
		if (info.RenderPass->GetID() == passID)
		{
			return info.Camera;
		}
	}
	return nullptr;
}
#pragma endregion

#pragma region Frustum Culling
bool Renderer::TryGetPassFrustum(RenderPassID passID, tagFrustum& outFrustum, bool& outIsShadow) const
{
	const auto it = m_PassFrustums.find(passID);
	if (it == m_PassFrustums.end())
		return false;

	outFrustum = it->second;
	outIsShadow = m_PassFrustumIsShadow.at(passID);
	return true;
}
#pragma endregion