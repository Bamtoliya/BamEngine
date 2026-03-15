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

	tagRenderTargetDesc rtDesc = {};
	rtDesc.Width = desc->RHIDesc.Width;
	rtDesc.Height = desc->RHIDesc.Height;
	rtDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource | ERenderTargetBindFlag::RTBF_RenderTarget;
	m_SceneBuffer = RenderTargetManager::Get().CreateRenderTarget(&rtDesc);
	tagRenderTargetDesc depthStencilDesc = {};
	depthStencilDesc.Width = desc->RHIDesc.Width;
	depthStencilDesc.Height = desc->RHIDesc.Height;
	depthStencilDesc.Type = ERenderTargetType::DepthStencil;
	depthStencilDesc.Usage = ERenderTargetUsage::RTU_DepthStencil;
	depthStencilDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource | ERenderTargetBindFlag::RTBF_DepthStencil | ERenderTargetBindFlag::RTBF_RenderTarget;
	depthStencilDesc.Format = ETextureFormat::D24_UNORM_S8_UINT;
	m_DepthBuffer = RenderTargetManager::Get().CreateRenderTarget(&depthStencilDesc);
	RenderPassManager::Get().RegisterRenderPass(L"MainPass", { L"RenderTarget_1" }, L"RenderTarget_2", ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store, vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, ERenderSortType::FrontToBack);

	if (!m_SceneBuffer) return EResult::Fail;


#ifdef _DEBUG
	tagShaderDesc debugLineVsDesc;
	debugLineVsDesc.ShaderType = EShaderType::Vertex;
	debugLineVsDesc.FilePath = L"Resources/Shader/debugLine.vert.spv";
	debugLineVsDesc.EntryPoint = "main";
	ResourceManager::Get().LoadShader(L"DebugLineVS", &debugLineVsDesc);

	tagShaderDesc debugLinePsDesc;
	debugLinePsDesc.ShaderType = EShaderType::Pixel;
	debugLinePsDesc.FilePath = L"Resources/Shader/debugLine.frag.spv";
	debugLinePsDesc.EntryPoint = "main";
	ResourceManager::Get().LoadShader(L"DebugLinePS", &debugLinePsDesc);
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
	Safe_Release(m_SceneBuffer);
	Safe_Release(m_DepthBuffer);
#ifdef _DEBUG
	Safe_Release(m_DebugVertexBuffer);
	Safe_Release(m_DebugPipeline);
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
				m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(tagCameraBuffer), 3);
				if (IsFailure(m_RHI->BeginRenderPass(pass)))
					return EResult::Fail;
				m_RHI->SetViewport(0, 0, 1920, 1080);
#ifdef _DEBUG
				if(pass->GetName().find(L"Debug") == 0)
				{
					if (IsFailure(RenderDebugLines(dt)))
						return EResult::Fail;
					if (IsFailure(m_RHI->EndRenderPass()))
						return EResult::Fail;
					continue;
				}
#endif
				auto it = m_RenderQueues.find(pass->GetID());
				if (it != m_RenderQueues.end())
				{
					if(IsFailure(RenderComponents(dt, it->second, pass->GetSortType(), pass)))
						return EResult::Fail;
				}
				auto customIt = m_CustomRenderQueues.find(pass->GetID());
				if (customIt != m_CustomRenderQueues.end())
				{
					for (const auto& command : customIt->second)
					{
						if (IsFailure(command(dt, pass)))
						{
							return EResult::Fail;
						}
					}
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
			if (IsFailure(component->Render(dt, renderPass)))
			{
				return EResult::Fail;
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
		debugPipelineDesc.VertexShader = ResourceManager::Get().GetShader(L"DebugLineVS")->GetRHIShader();
		debugPipelineDesc.PixelShader = ResourceManager::Get().GetShader(L"DebugLinePS")->GetRHIShader();
		debugPipelineDesc.InputLayout = DebugVertex::Layout;
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
	m_RHI->BindVertexBuffer(m_DebugVertexBuffer);
	m_RHI->Draw((uint32)m_DebugVertices.size());
	return EResult::Success;
}
#endif
#pragma endregion