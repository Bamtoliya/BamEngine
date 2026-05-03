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

#ifdef _DEBUG
	ResourceManager& rm = ResourceManager::Get();
	tagShaderDesc debugLineVsDesc;
	debugLineVsDesc.ShaderType = EShaderType::Vertex;
	
	debugLineVsDesc.Key = L"DebugLineVS";
	//debugLineVsDesc.Path = L"Resources/Shader/debugLine.vert.spv";
	debugLineVsDesc.Path = L"I:\\BamtoliyaGithub\\BamEngine\\Resources\\Shader\\debugLine.vert.spv";
	debugLineVsDesc.SpirvPath = L"I:\\BamtoliyaGithub\\BamEngine\\Resources\\Shader\\debugLine.vert.spv";
	debugLineVsDesc.EntryPoint = "main";
	rm.LoadResource<Shader>(&debugLineVsDesc);
	//ResourceManager::Get().LoadShader(L"DebugLineVS", &debugLineVsDesc);

	tagShaderDesc debugLinePsDesc;
	debugLinePsDesc.ShaderType = EShaderType::Pixel;
	debugLinePsDesc.Key = L"DebugLinePS";
	debugLinePsDesc.Path = L"Resources/Shader/debugLine.frag.spv";
	debugLinePsDesc.SpirvPath = L"Resources/Shader/debugLine.frag.spv";
	debugLinePsDesc.EntryPoint = "main";
	rm.LoadResource<Shader>(&debugLinePsDesc);
	//ResourceManager::Get().LoadShader(L"DebugLinePS", &debugLinePsDesc);
	
	//디버그 버텍스 버퍼는 최대 1000개의 라인을 그릴 수 있도록 설정
	uint32 maxDebugLines = 1000;
	tagRHIBufferDesc bufferDesc;
	m_DebugVertices.reserve(2 * maxDebugLines);
	bufferDesc.InitialData = nullptr;
	bufferDesc.BufferType = ERHIBufferType::Vertex;
	bufferDesc.Size = 2 * maxDebugLines* sizeof(DebugVertex); // 1000 lines * 2 vertices per line
	bufferDesc.Stride = sizeof(DebugVertex);
	m_DebugVertexBuffer = m_RHI->CreateBuffer(nullptr, bufferDesc.Size, bufferDesc.Stride, bufferDesc.BufferType);
	if (!m_DebugVertexBuffer) return EResult::Fail;
#endif // _DEBUG

	return EResult::Success;
}

void Renderer::Free()
{	
	m_RenderPassManager = nullptr;
	//Safe_Release(m_SceneBuffer);
	//Safe_Release(m_DepthBuffer);
#ifdef _DEBUG
	Safe_Release(m_DebugVertexBuffer);
	m_DebugPipeline = nullptr;
	//Safe_Release(m_DebugPipeline);
#endif // _DEBUG
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
		jobsPerPass[viewportInfo.RenderPass->GetID()].push_back(viewportInfo.Camera);

	const vector<RenderPass*>& renderPasses = m_RenderPassManager->GetAllRenderPasses();

	for (const auto& pass : renderPasses)
	{
		if (pass->GetPassType() == ERenderPassType::Shadow && LightManager::Get().GetShadowCastingLights().empty())
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
			if (pass->GetPassType() == ERenderPassType::Shadow)
			{
				cameraBuffer = LightManager::Get().GetShadowCameraBuffer(uint32(0));
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
			m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(tagCameraBuffer), 0);
			m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(tagCameraBuffer), 3);

			if (IsFailure(m_RHI->BeginRenderPass(pass)))
				continue;

			uint32 rtWidth = m_RHI->GetSwapChainWidth();
			uint32 rtHeight = m_RHI->GetSwapChainHeight();
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

#ifdef _DEBUG
			if (pass->GetName().find(L"Debug") == 0)
			{
				RenderDebugLines(dt);
				m_RHI->EndRenderPass();
				continue;
			}
#endif
			auto it = m_RenderQueues.find(pass->GetID());
			if (it != m_RenderQueues.end())
				RenderComponents(dt, it->second, pass->GetSortType(), pass);

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
#ifdef _DEBUG
	m_DebugVertices.clear();
#endif // _DEBUG
	m_RenderQueues.clear();
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



#pragma region Debug
#ifdef _DEBUG
void Renderer::DrawDebugLine(const vec3& start, const vec3& end, const vec4& color)
{
	m_DebugVertices.push_back({ start, color });
	m_DebugVertices.push_back({ end, color });
}

void Renderer::DrawDebugRect(const Rect& rect, const vec4& color)
{
	DrawDebugLine(vec3(rect.Left, rect.Top, 0.f), vec3(rect.Right(), rect.Top, 0.f), color);
	DrawDebugLine(vec3(rect.Right(), rect.Top, 0.f), vec3(rect.Right(), rect.Bottom(), 0.f), color);
	DrawDebugLine(vec3(rect.Right(), rect.Bottom(), 0.f), vec3(rect.Left, rect.Bottom(), 0.f), color);
	DrawDebugLine(vec3(rect.Left, rect.Bottom(), 0.f), vec3(rect.Left, rect.Top, 0.f), color);
}

void Renderer::DrawDebugBox(const vec3& center, const vec3& extent, const vec4& color, const mat4& transform)
{
	vec3 localCorners[8] = {
		vec3(center.x - extent.x, center.y - extent.y, center.z - extent.z),
		vec3(center.x + extent.x, center.y - extent.y, center.z - extent.z),
		vec3(center.x + extent.x, center.y + extent.y, center.z - extent.z),
		vec3(center.x - extent.x, center.y + extent.y, center.z - extent.z),
		vec3(center.x - extent.x, center.y - extent.y, center.z + extent.z),
		vec3(center.x + extent.x, center.y - extent.y, center.z + extent.z),
		vec3(center.x + extent.x, center.y + extent.y, center.z + extent.z),
		vec3(center.x - extent.x, center.y + extent.y, center.z + extent.z)
	};

	vec3 worldCorners[8];
	for (int i = 0; i < 8; ++i) {
		worldCorners[i] = vec3(transform * vec4(localCorners[i], 1.0f));
	}

	DrawDebugLine(worldCorners[0], worldCorners[1], color); DrawDebugLine(worldCorners[1], worldCorners[2], color);
	DrawDebugLine(worldCorners[2], worldCorners[3], color); DrawDebugLine(worldCorners[3], worldCorners[0], color);
	DrawDebugLine(worldCorners[4], worldCorners[5], color); DrawDebugLine(worldCorners[5], worldCorners[6], color);
	DrawDebugLine(worldCorners[6], worldCorners[7], color); DrawDebugLine(worldCorners[7], worldCorners[4], color);
	DrawDebugLine(worldCorners[0], worldCorners[4], color); DrawDebugLine(worldCorners[1], worldCorners[5], color);
	DrawDebugLine(worldCorners[2], worldCorners[6], color); DrawDebugLine(worldCorners[3], worldCorners[7], color);
}

void Renderer::DrawDebugSphere(const vec3& center, float radius, const vec4& color)
{
	// Sphere drawing logic (e.g., approximate with line segments and call DrawDebugLine)
}
EResult Renderer::RenderDebugLines(f32 dt)
{
	if (!m_DebugPipeline)
	{
		tagRHIPipelineDesc debugPipelineDesc;
		debugPipelineDesc.PipelineType = EPipelineType::Graphics;
		debugPipelineDesc.BlendMode = EBlendMode::AlphaBlend;
		debugPipelineDesc.FillMode = EFillMode::Solid;
		debugPipelineDesc.CullMode = ECullMode::None;
		debugPipelineDesc.FrontFace = EFrontFace::CounterClockwise;
		debugPipelineDesc.Topology = ETopology::LineList;

		ResourceManager& rm = ResourceManager::Get();
		debugPipelineDesc.VertexShader = rm.GetResourceHandle<Shader>(L"DebugLineVS").Get()->GetRHIShader();
		debugPipelineDesc.PixelShader = rm.GetResourceHandle<Shader>(L"DebugLinePS").Get()->GetRHIShader();
		debugPipelineDesc.InputLayouts.push_back(DebugVertex::Layout);
		debugPipelineDesc.DepthStencilState.DepthTestEnable = false;
		debugPipelineDesc.DepthStencilState.StencilTestEnable = false;
		debugPipelineDesc.DepthStencilState.DepthWriteEnable = false;
		debugPipelineDesc.DepthStencilState.DepthCompareOp = ECompareOp::Always;
		debugPipelineDesc.ColorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
		m_DebugPipeline = PipelineManager::Get().GetOrCreatePipeline(debugPipelineDesc);
	}
	if (m_DebugVertices.empty()) return EResult::Success;
	m_DebugVertexBuffer->SetData(m_DebugVertices.data(), (uint32)m_DebugVertices.size() * sizeof(DebugVertex));
	if(IsFailure(m_RHI->BindPipeline(m_DebugPipeline))) return EResult::Fail;
	RHIBuffer* buffersToBind[1] = { m_DebugVertexBuffer };
	m_RHI->BindVertexBuffers(0, buffersToBind, 1);
	m_RHI->Draw((uint32)m_DebugVertices.size());
	return EResult::Success;
}
#endif
#pragma endregion