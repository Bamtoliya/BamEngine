#pragma once

#include "Renderer.h"
//RHI
#include "RHI.h"
#include "SDLGPURHI.h"
//Rendering
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
//Pipeline
#include "RHIPipeline.h"
#include "PipelineManager.h"
//Resource
#include "ResourceManager.h"
#include "Mesh.h"
#include "Shader.h"
#include "Texture.h"
#include "MaterialInterface.h"

//Light
#include "LightManager.h"

#include "SamplerManager.h"
#include "SceneManager.h"
#include "CoreComponents.h"
#include "PickingManager.h"

#include "TimeManager.h"

IMPLEMENT_SINGLETON(Renderer)

#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{	
	if (!arg) return EResult::InvalidArgument;

	CAST_DESC
	m_RHIType = desc->rhiType;
	switch (m_RHIType)
	{
	case ERHIType::SDLGPU:
		m_RHI = SDLGPURHI::Create(desc->rhiDesc);
		break;
	case ERHIType::DirectX12:
		m_RHI = DirectX12RHI::Create(desc->rhiDesc);
		break;
	default:
		return EResult::Fail;
	}
	
	if (!m_RHI)
		return EResult::Fail;

	return EResult::Success;
}

void Renderer::Free()
{	
	m_RenderPassManager = nullptr;

	if (m_RHI)
	{
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
	LightManager::Get().UpdateLightBuffer();

	struct ViewportJob
	{
		CameraBuffer cameraBuffer = {};
		bool hasCamera = false;
	};

	unordered_map<RenderPassID, vector<ViewportJob>> jobsPerPass;
	uint32 rtWidth = m_RHI->GetSwapChainWidth();
	uint32 rtHeight = m_RHI->GetSwapChainHeight();
	for (const auto& viewportInfo : m_ViewportCameras)
	{
		if (!viewportInfo.renderPass) continue;
		jobsPerPass[viewportInfo.renderPass->GetID()].push_back({ viewportInfo.cameraBuffer, viewportInfo.hasCamera });
	}

	const vector<RenderPass*>& renderPasses = m_RenderPassManager->GetAllRenderPasses();

	for (const auto& pass : renderPasses)
	{
		const ERenderPassType passType = pass->GetPassType();
		if (passType == ERenderPassType::Shadow && !LightManager::Get().HasShadowCastingLights())
			continue;
		static const vector<ViewportJob> s_DefaultJobs = { ViewportJob{} };
		auto jobsIt = jobsPerPass.find(pass->GetID());
		const vector<ViewportJob>& jobs = (jobsIt != jobsPerPass.end()) ? jobsIt->second : s_DefaultJobs;

		for (const auto& job : jobs)
		{
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

			// ── Shadow Pass: Directional/Spot Atlas 또는 Point CubeMap ──
			if (passType == ERenderPassType::Shadow)
			{
				for (const auto& [slot, buffer] : pass->GetAllPassConstants())
				{
					if (!buffer.empty())
					{
						if (IsFailure(m_RHI->BindConstantBuffer(buffer.data(), static_cast<uint32>(buffer.size()), slot)))
							ENGINE_LOG_ERROR("Failed to bind pass constant buffer. Slot: {}, Pass ID: {}", slot, pass->GetID());
					}
				}

				RenderTarget* depthRT = pass->GetDepthStencilName().empty() ? nullptr :
					RenderTargetManager::Get().GetRenderTarget(pass->GetDepthStencilName());
				const bool isCubeShadow = (depthRT && depthRT->GetTexture() &&
					depthRT->GetTexture()->GetDimension() == ETextureDimension::TextureCube);

				auto meshIt = m_StaticDrawCommands.find(pass->GetID());
				if (meshIt != m_StaticDrawCommands.end())
				{
					if (isCubeShadow)
					{
						// ── Point Light: 6면 큐브맵 개별 슬라이스 순회 렌더링 ──
						if (LightManager::Get().HasPointShadow())
						{
							for (uint32 face = 0; face < 6; ++face)
							{
								m_RHI->BindDepthSlice(depthRT->GetTexture(), face);
								m_RHI->ClearDepthSlice(depthRT->GetTexture(), face, 1.0f, 0);
								m_RHI->SetViewport(0, 0, rtWidth, rtHeight);

								CameraBuffer faceCam = LightManager::Get().GetPointShadowCameraBuffer(face);
								faceCam.time = static_cast<f32>(TimeManager::Get().GetTotalTime());
								if (IsFailure(m_RHI->BindConstantBuffer(&faceCam, sizeof(CameraBuffer), 0)))
								{
									ENGINE_LOG_ERROR("Failed to bind point shadow face camera buffer. Face: {}", face);
									return EResult::Fail;
								}

								const EResult result = RenderMeshes(dt, meshIt->second, pass->GetSortType(), pass);
								if (IsFailure(result))
									ENGINE_LOG_ERROR("Point shadow mesh rendering failed for face: {}", face);
							}
						}
					}
					else
					{
						const uint32 halfW = rtWidth / 2;
						const uint32 halfH = rtHeight / 2;

						// 1) Directional Light CSM (사분면 0, 1, 2)
						for (uint32 c = 0; c < MAX_SHADOW_CASCADES; ++c)
						{
							CameraBuffer cascadeCam = LightManager::Get().GetShadowCameraBuffer(c);
							cascadeCam.time = static_cast<f32>(TimeManager::Get().GetTotalTime());
							if (IsFailure(m_RHI->BindConstantBuffer(&cascadeCam, sizeof(CameraBuffer), 0)))
							{
								ENGINE_LOG_ERROR("Failed to bind shadow cascade camera buffer. Cascade: {}", c);
								return EResult::Fail;
							}

							const int32 vpX = static_cast<int32>((c % 2) * halfW);
							const int32 vpY = static_cast<int32>((c / 2) * halfH);
							m_RHI->SetViewport(vpX, vpY, halfW, halfH);

							const EResult result = RenderMeshes(dt, meshIt->second, pass->GetSortType(), pass);
							if (IsFailure(result))
								ENGINE_LOG_ERROR("Shadow mesh rendering failed for cascade: {}", c);
						}

						// 2) Spot Light Shadow (우하단 사분면: halfW, halfH)
						if (LightManager::Get().HasSpotShadow())
						{
							CameraBuffer spotCam = LightManager::Get().GetSpotShadowCameraBuffer();
							spotCam.time = static_cast<f32>(TimeManager::Get().GetTotalTime());
							if (IsSuccess(m_RHI->BindConstantBuffer(&spotCam, sizeof(CameraBuffer), 0)))
							{
								m_RHI->SetViewport(static_cast<int32>(halfW), static_cast<int32>(halfH), halfW, halfH);
								const EResult result = RenderMeshes(dt, meshIt->second, pass->GetSortType(), pass);
								if (IsFailure(result))
									ENGINE_LOG_ERROR("Spot shadow mesh rendering failed.");
							}
						}
					}
				}
			}
			else
			{
				CameraBuffer cameraBuffer = {};

				if (passType == ERenderPassType::UI)
				{
					cameraBuffer.viewMatrix = glm::identity<mat4>();
					cameraBuffer.projMatrix = glm::ortho(0.0f, static_cast<f32>(rtWidth), static_cast<f32>(rtHeight), 0.0f, -1.0f, 1.0f);
					cameraBuffer.viewProjMatrix = cameraBuffer.projMatrix * cameraBuffer.viewMatrix;
					cameraBuffer.cameraPosition = vec3(0.f);
				}
				else if (job.hasCamera)
				{
					cameraBuffer = job.cameraBuffer;
				}
				else
				{
					cameraBuffer.viewMatrix = glm::identity<mat4>();
					cameraBuffer.projMatrix = glm::identity<mat4>();
					cameraBuffer.viewProjMatrix = glm::identity<mat4>();
					cameraBuffer.cameraPosition = vec3(0.f);
				}

				cameraBuffer.time = static_cast<f32>(TimeManager::Get().GetTotalTime());
				if (IsFailure(m_RHI->BindConstantBuffer(&cameraBuffer, sizeof(CameraBuffer), 0)))
				{
					ENGINE_LOG_ERROR("Failed to bind camera constant buffer.");
					return EResult::Fail;
				}

				for (const auto& [slot, buffer] : pass->GetAllPassConstants())
				{
					if (!buffer.empty())
					{
						if (IsFailure(m_RHI->BindConstantBuffer(buffer.data(), static_cast<uint32>(buffer.size()), slot)))
							ENGINE_LOG_ERROR("Failed to bind pass constant buffer. Slot: {}, Pass ID: {}", slot, pass->GetID());
					}
				}

				m_RHI->SetViewport(0, 0, rtWidth, rtHeight);

				auto meshIt = m_StaticDrawCommands.find(pass->GetID());
				if (meshIt != m_StaticDrawCommands.end())
				{
					const EResult result = RenderMeshes(dt, meshIt->second, pass->GetSortType(), pass);
					if (IsFailure(result))
						ENGINE_LOG_ERROR("ECS mesh rendering failed. Pass ID: {}", pass->GetID());
				}

				auto spriteIt = m_SpriteDrawCommands.find(pass->GetID());
				if (spriteIt != m_SpriteDrawCommands.end())
				{
					const EResult result = RenderSprite(dt, spriteIt->second, pass->GetSortType(), pass);
					if (IsFailure(result))
						ENGINE_LOG_ERROR("ECS sprite rendering failed. Pass ID: {}", pass->GetID());
				}

				if (const auto images = m_UIImageDrawCommands.find(pass->GetID()); images != m_UIImageDrawCommands.end())
				{
					if (IsFailure(RenderUIImages(images->second, pass)))
						ENGINE_LOG_ERROR("ECS UI image rendering failed. Pass ID: {}", pass->GetID());
				}
			}
			
			auto customIt = m_CustomRenderQueues.find(pass->GetID());
			if (customIt != m_CustomRenderQueues.end())
			{
				for (const auto& command : customIt->second)
					command(dt, pass);
			}

			if (IsFailure(m_RHI->EndRenderPass()))
				return EResult::Fail;
			if (passType == ERenderPassType::Geometry) 
				PickingManager::Get().MarkPickingPass(pass->GetID());
		}
		GetRenderPassDelegate(pass->GetID()).Broadcast(dt);
	}

	return EResult::Success;
}

TODO("Renderer::RenderMeshes, RenderSkinnedMeshes, RenderSprite 함수에서 Material과 Mesh를 Bind하고 DrawIndexed 호출하는 부분을 RHI에 맞게 구현 필요")
EResult Renderer::RenderMeshes(f32 dt, vector<StaticDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass)
{
	(void)dt;

	if (!m_RHI || !renderPass)
		return EResult::Fail;

	if (commands.size() > 1 && sortType != ERenderSortType::BackToFront)
	{
		std::sort(commands.begin(), commands.end(), [](const StaticDrawCommand& a, const StaticDrawCommand& b)
			{
				if (a.material != b.material) return a.material < b.material;
				return a.mesh < b.mesh;
			});
	}

	MaterialInterface* lastMaterial = nullptr;
	Mesh* lastMesh = nullptr;

	for (const auto& command : commands)
	{
		Mesh* mesh = command.mesh;
		MaterialInterface* material = command.material;

		if (!mesh || !material)
			return EResult::Fail;

		if (material != lastMaterial || mesh != lastMesh)
		{
			if (IsFailure(BindMeshPipeline(mesh, material, renderPass)))
				return EResult::Fail;
		}

		if (material != lastMaterial)
		{
			if (IsFailure(material->Bind(2)))
				return EResult::Fail;
			lastMaterial = material;
		}

		SceneUBO objectData = {};
		objectData.worldMatrix = command.worldMatrix;
		objectData.pickingData.x = static_cast<float>(command.pickingId);

		if (IsFailure(m_RHI->BindConstantBuffer(&objectData, sizeof(objectData), 1)))
			return EResult::Fail;

		if (mesh != lastMesh)
		{
			if (IsFailure(mesh->Bind(0)))
				return EResult::Fail;
			lastMesh = mesh;
		}

		const EResult result = mesh->GetIndexBuffer()
			? m_RHI->DrawIndexed(mesh->GetIndexCount())
			: m_RHI->Draw(mesh->GetVertexCount());

		if (IsFailure(result))
			return result;
	}

	return EResult::Success;
}

EResult Renderer::RenderSkinnedMeshes(f32 dt, vector<SkinnedDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass)
{
	for (auto& command : commands)
	{
		if (command.mesh && command.material)
		{
			if (IsFailure(command.material->Bind(0)))
				return EResult::Fail;
			if (IsFailure(command.mesh->Bind(0)))
				return EResult::Fail;
			//m_RHI->BindConstantBuffer(command.pBoneMatrices, sizeof(mat4) * command.pMesh->GetBoneCount(), 1, EShaderType::Vertex);
			m_RHI->DrawIndexed(command.mesh->GetIndexCount());
		}
	}
	return EResult();
}

EResult Renderer::RenderSprite(
	f32 dt,
	vector<SpriteDrawCommand>& commands,
	ERenderSortType sortType,
	RenderPass* renderPass)
{
	if (!m_RHI || !renderPass)
		return EResult::Fail;

	struct SpriteUBO
	{
		mat4 worldMatrix;
		vec4 color;
		vec4 uvTransform;
		vec4 pickingData;
	};

	static_assert(offsetof(SpriteUBO, color) == 64);
	static_assert(offsetof(SpriteUBO, uvTransform) == 80);
	static_assert(offsetof(SpriteUBO, pickingData) == 96);
	static_assert(sizeof(SpriteUBO) == 112);

	RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();

	if (!sampler)
		return EResult::Fail;

	for (const auto& command : commands)
	{
		if (!command.mesh || !command.material || !command.texture)
			return EResult::Fail;

		RHITexture* texture = command.texture->GetRHITexture();

		if (!texture || !command.mesh->GetIndexBuffer())
			return EResult::Fail;

		if (IsFailure(BindMeshPipeline(
			command.mesh, command.material, renderPass)))
		{
			return EResult::Fail;
		}

		// 이번 Sprite 셰이더의 t0는 Sprite 리소스가 제공합니다.
		if (IsFailure(m_RHI->BindTextureSampler(texture, sampler, 0)))
			return EResult::Fail;

		SpriteUBO objectData{};
		objectData.worldMatrix = command.worldMatrix;
		objectData.color = command.color;
		objectData.uvTransform = command.uvTransform;
		objectData.pickingData = vec4(static_cast<float>(command.pickingId), 0.0f, 0.0f, 0.0f);

		if (IsFailure(m_RHI->BindConstantBuffer(
			&objectData, sizeof(objectData), 1)))
		{
			return EResult::Fail;
		}

		if (IsFailure(command.mesh->Bind(0)))
			return EResult::Fail;

		if (IsFailure(m_RHI->DrawIndexed(command.mesh->GetIndexCount())))
			return EResult::Fail;
	}

	return EResult::Success;
}

EResult Renderer::RenderUIImages(const vector<UIImageDrawCommand>& commands, RenderPass* renderPass)
{
	if (!m_RHI || !renderPass) return EResult::Fail;
	struct ImageUBO
	{
		mat4 worldMatrix;
		vec4 uvRect;
		vec2 tiling;
		vec2 offset;
		vec4 color;
	};
	static_assert(sizeof(ImageUBO) == 112);
	static_assert(offsetof(ImageUBO, color) == 96);
	auto* sampler = SamplerManager::Get().GetDefaultSampler();
	if (!sampler) return EResult::Fail;
	for (const auto& command : commands)
	{
		Texture* texture = command.textureHandle.Get();
		MaterialInterface* material = command.materialHandle.Get();
		if (!texture || !material || !texture->GetRHITexture() ||
			command.canvasSize.x <= 0.0f || command.canvasSize.y <= 0.0f) return EResult::InvalidArgument;
		if (IsFailure(BindMeshPipeline(nullptr, material, renderPass))) return EResult::Fail;
		CameraBuffer camera{};
		camera.viewMatrix = camera.invViewMatrix = mat4(1.0f);
		camera.projMatrix = glm::ortho(0.0f, command.canvasSize.x, command.canvasSize.y, 0.0f, -1.0f, 1.0f);
		camera.invProjMatrix = glm::inverse(camera.projMatrix);
		camera.viewProjMatrix = camera.projMatrix;
		camera.invViewProjMatrix = camera.invProjMatrix;
		ImageUBO data{};
		data.worldMatrix = command.worldMatrix;
		data.uvRect = vec4(vec2(command.uvTransform),
			vec2(command.uvTransform) + vec2(command.uvTransform.z, command.uvTransform.w));
		data.tiling = vec2(1.0f);
		data.color = command.color;
		if (IsFailure(m_RHI->BindConstantBuffer(&camera, sizeof(camera), 0)) ||
			IsFailure(m_RHI->BindConstantBuffer(&data, sizeof(data), 1)) ||
			IsFailure(m_RHI->BindTextureSampler(texture->GetRHITexture(), sampler, 0)) ||
			IsFailure(m_RHI->Draw(6))) return EResult::Fail;
	}
	// Restore the pass projection for legacy UI components drawn afterwards.
	if (renderPass->GetRenderTargetCount() > 0)
	{
		if (auto* target = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(0)))
		{
			CameraBuffer camera{};
			camera.viewMatrix = camera.invViewMatrix = mat4(1.0f);
			camera.projMatrix = glm::ortho(0.0f, float(target->GetWidth()),
				float(target->GetHeight()), 0.0f, -1.0f, 1.0f);
			camera.invProjMatrix = glm::inverse(camera.projMatrix);
			camera.viewProjMatrix = camera.projMatrix;
			camera.invViewProjMatrix = camera.invProjMatrix;
			return m_RHI->BindConstantBuffer(&camera, sizeof(camera), 0);
		}
	}
	return EResult::Success;
}

EResult Renderer::EndFrame()
{
	EResult frameResult = EResult::Fail;
	if (m_RHI)
	{
		m_RHI->BindRenderTarget(nullptr, nullptr);
		m_RHI->SetViewport(0, 0, m_RHI->GetSwapChainWidth(), m_RHI->GetSwapChainHeight());
		frameResult = m_RHI->EndFrame();
	}

	for (auto& [passID, queue] : m_CustomRenderQueues) queue.clear();
	for (auto& [passID, queue] : m_StaticDrawCommands) queue.clear();
	for (auto& [passID, queue] : m_SkinnedDrawCommands) queue.clear();
	for (auto& [passID, queue] : m_SpriteDrawCommands) queue.clear();
	for (auto& [passID, queue] : m_UIImageDrawCommands) queue.clear();

	m_ViewportCameras.clear();
	m_PassFrustums.clear();
	m_PassFrustumIsShadow.clear();
	return frameResult;
}
#pragma endregion

#pragma region Queue Management
void Renderer::SubmitCustomCommand(const CustomRenderCommand& command, RenderPassID passID)
{
	m_CustomRenderQueues[passID].push_back(command);
}

void Renderer::ClearRenderQueue(RenderPassID passID)
{
	auto customIt = m_CustomRenderQueues.find(passID);
	if (customIt != m_CustomRenderQueues.end())
	{
		customIt->second.clear();
		m_CustomRenderQueues.erase(customIt);
	}
	m_StaticDrawCommands.erase(passID);
	m_SkinnedDrawCommands.erase(passID);
	m_SpriteDrawCommands.erase(passID);
	m_UIImageDrawCommands.erase(passID);
}

void Renderer::ClearAllRenderQueues()
{
	for (auto& pair : m_CustomRenderQueues)
	{
		pair.second.clear();
	}
	m_CustomRenderQueues.clear();
	m_StaticDrawCommands.clear();
	m_SkinnedDrawCommands.clear();
	m_SpriteDrawCommands.clear();
	m_UIImageDrawCommands.clear();
}

//Entity Submit
void Renderer::SubmitStatic(const StaticDrawCommand& command, RenderPassID passID)
{
	m_StaticDrawCommands[passID].push_back(command);
}
void Renderer::SubmitSkinned(const SkinnedDrawCommand& command, RenderPassID passID)
{
	m_SkinnedDrawCommands[passID].push_back(command);
}
void Renderer::SubmitSprite(const SpriteDrawCommand& command, RenderPassID passID)
{
	m_SpriteDrawCommands[passID].push_back(command);
}

void Renderer::SubmitUIImage(const UIImageDrawCommand& command, RenderPassID passID)
{
	m_UIImageDrawCommands[passID].push_back(command);
}
#pragma endregion

#pragma region Viewport Camera Management

void Renderer::RegisterViewportCamera(const CameraBuffer& cameraBuffer, RenderPass* renderPass)
{
	m_ViewportCameras.push_back({ cameraBuffer, true, renderPass });
	if (!renderPass) return;

	const RenderPassID passID = renderPass->GetID();
	if (renderPass->GetPassType() == ERenderPassType::Shadow)
	{
		if (LightManager::Get().HasShadowCastingLights())
		{
			m_PassFrustums[passID] = FrustumCuller::ExtractFrustum(LightManager::Get().GetShadowCameraBuffer(uint32(0)).viewProjMatrix);
			m_PassFrustumIsShadow[passID] = true;
		}
	}
	else
	{
		m_PassFrustums[passID] = FrustumCuller::ExtractFrustum(cameraBuffer.viewProjMatrix);
		m_PassFrustumIsShadow[passID] = false;
	}
}

void Renderer::RegisterViewportCamera(RenderPass* renderPass)
{
	m_ViewportCameras.push_back({ {}, false, renderPass });
	if (!renderPass) return;

	const RenderPassID passID = renderPass->GetID();
	if (renderPass->GetPassType() == ERenderPassType::Shadow)
	{
		if (LightManager::Get().HasShadowCastingLights())
		{
			m_PassFrustums[passID] = FrustumCuller::ExtractFrustum(LightManager::Get().GetShadowCameraBuffer(uint32(0)).viewProjMatrix);
			m_PassFrustumIsShadow[passID] = true;
		}
	}
}

void Renderer::RegisterUICanvas(Scene* scene, entt::entity canvas, RenderPass* renderPass)
{
	if (renderPass) m_ViewportCameras.push_back({ {}, false, renderPass, scene, canvas, true });
}

void Renderer::UnregisterViewportCamera(RenderPassID passID)
{
	m_ViewportCameras.erase(std::remove_if(m_ViewportCameras.begin(), m_ViewportCameras.end(),
		[passID](const ViewportCameraInfo& info) { return info.renderPass->GetID() == passID; }), m_ViewportCameras.end());
}
#pragma endregion

#pragma region Frustum Culling
bool Renderer::TryGetPassFrustum(RenderPassID passID, Frustum& outFrustum, bool& outIsShadow) const
{
	const auto it = m_PassFrustums.find(passID);
	if (it == m_PassFrustums.end())
		return false;

	outFrustum = it->second;
	outIsShadow = m_PassFrustumIsShadow.at(passID);
	return true;
}

#pragma endregion

#pragma region Binding
EResult Renderer::BindMeshPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass) 
{
	if (!m_RHI || !material || !renderPass)
		return EResult::InvalidArgument;

	const ERenderPassType passType = renderPass->GetPassType();
	Shader* vs = material->GetVertexShader(passType);
	Shader* ps = material->GetPixelShader(passType);

	if (!vs || !ps)
		return EResult::InvalidArgument;

	if (!vs->GetRHIShader() || !ps->GetRHIShader())
		return EResult::InvalidArgument;

	RHIPipelineDesc desc;
	desc.pipelineType = EPipelineType::Graphics;
	desc.topology = mesh ? mesh->GetTopology() : ETopology::TriangleList;
	if (mesh) desc.inputLayouts = mesh->GetInputLayoutDescs();

	desc.vertexShader = vs->GetRHIShader();
	desc.pixelShader = ps->GetRHIShader();

	desc.frontFace = material->GetFrontFace();
	desc.cullMode = material->GetCullMode();
	desc.fillMode = material->GetFillMode();

	desc.blendState = material->GetBlendState();

	RenderTargetManager& rtManager = RenderTargetManager::Get();

	desc.colorAttachmentCount = renderPass->GetRenderTargetCount();

	if(desc.colorAttachmentCount > MAX_RENDER_TARGET_COUNT)
		return EResult::InvalidArgument;

	for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
	{
		const wstring& rtName = renderPass->GetRenderTargetName(i);
		RenderTarget* rt = rtManager.GetRenderTarget(rtName);
		if (!rt)
			return EResult::InvalidArgument;
		desc.colorAttachmentFormats[i] = rt->GetFormat();
	}

	desc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;

	const wstring depthStencilName = renderPass->GetDepthStencilName();
	const bool hasDepthStencil = !depthStencilName.empty();

	if (hasDepthStencil)
	{
		RenderTarget* ds = rtManager.GetRenderTarget(depthStencilName);
		if (!ds)
			return EResult::InvalidArgument;
		desc.depthStencilAttachmentFormat = ds->GetFormat();
	}

	desc.depthStencilState.depthTestEnable = hasDepthStencil && material->GetDepthMode() != EDepthMode::None;
	desc.depthStencilState.depthWriteEnable = hasDepthStencil && material->GetDepthMode() == EDepthMode::ReadWrite;
	desc.depthStencilState.depthCompareOp = material->GetDepthCompareOp();

	RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(desc);

	if (!pipeline)
		return EResult::Fail;
	
	return m_RHI->BindPipeline(pipeline);
}
#pragma endregion