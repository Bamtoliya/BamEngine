#pragma once

#include "Renderer.h"
//RHI
#include "RHI.h"
#include "SDLGPURHI.h"
//Rendering
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "InstanceData.h"
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
#include "WorldComponents.h"
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
	// 기본 셰이더 핸들 초기화
	m_DefaultGBufferVS = {};
	m_DefaultGBufferSkinningVS = {};
	m_DefaultShadowDepthVS = {};
	m_DefaultShadowDepthSkinningVS = {};

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
	const EResult lightResult = LightManager::Get().UpdateLightBuffer();
	if (IsFailure(lightResult))
		return lightResult;

	auto& jobsPerPass = m_ViewportJobsPerPass;
	for (auto& [passID, jobs] : jobsPerPass)
		jobs.clear();

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
		// 1. 패스가 비활성화된 경우: 드로우콜은 모두 건너뛰되, 이전 프레임 잔상을 방지하기 위해 Clear만 수행
		if (!pass->IsEnabled())
		{
			if (pass->GetLoadOperation() == ERenderPassLoadOperation::RPLO_Clear ||
				pass->GetStencilLoadOperation() == ERenderPassLoadOperation::RPLO_Clear ||
				pass->GetPassType() == ERenderPassType::Shadow)
			{
				m_RHI->BeginRenderPass(pass);
				// Point Light 큐브맵 섀도우인 경우 6개 면을 모두 1.0(최대 깊이)으로 Clear
				if (pass->GetPassType() == ERenderPassType::Shadow && !pass->GetDepthStencilName().empty())
				{
					if (auto* depthRT = RenderTargetManager::Get().GetRenderTarget(pass->GetDepthStencilName()))
					{
						if (depthRT->GetTexture() && depthRT->GetTexture()->GetDimension() == ETextureDimension::TextureCube)
						{
							for (uint32 face = 0; face < 6; ++face)
							{
								m_RHI->BindDepthSlice(depthRT->GetTexture(), face);
								m_RHI->ClearDepthSlice(depthRT->GetTexture(), face, 1.0f, 0);
							}
						}
					}
				}
				m_RHI->EndRenderPass();
			}
			continue;
		}

		const ERenderPassType passType = pass->GetPassType();
		if (passType == ERenderPassType::Shadow && !LightManager::Get().HasShadowCastingLights())
			continue;
		// ── [핵심 최적화] 실제 수행할 작업이 전혀 없는 빈 패스(Empty Pass) 완전 스킵 ──
		const RenderPassID pid = pass->GetID();
		auto delIt = m_RenderPassDelegates.find(pid);
		const bool hasDelegate = (delIt != m_RenderPassDelegates.end() && delIt->second.IsBound());
		const bool hasCommands =
			(m_StaticDrawCommands.find(pid) != m_StaticDrawCommands.end() && !m_StaticDrawCommands.at(pid).empty()) ||
			(m_SkinnedDrawCommands.find(pid) != m_SkinnedDrawCommands.end() && !m_SkinnedDrawCommands.at(pid).empty()) ||
			(m_SpriteDrawCommands.find(pid) != m_SpriteDrawCommands.end() && !m_SpriteDrawCommands.at(pid).empty()) ||
			(m_UIImageDrawCommands.find(pid) != m_UIImageDrawCommands.end() && !m_UIImageDrawCommands.at(pid).empty()) ||
			(m_CustomRenderQueues.find(pid) != m_CustomRenderQueues.end() && !m_CustomRenderQueues.at(pid).empty()) ||
			hasDelegate;

		auto jobsIt = jobsPerPass.find(pid);
		const bool hasJobs = (jobsIt != jobsPerPass.end() && !jobsIt->second.empty());
		const bool needsClear = (pass->GetLoadOperation() == ERenderPassLoadOperation::RPLO_Clear ||
			pass->GetStencilLoadOperation() == ERenderPassLoadOperation::RPLO_Clear);

		// [수정] 드로우 커맨드가 없는 패스 스킵 판정:
		// 1) 뷰포트 카메라(Job)도 없고 커맨드도 없음 -> 쓰로틀링(15 FPS) 등으로 쉰 뷰포트이므로 이전 RT 보존을 위해 완전 스킵!
		// 2) 카메라는 등록되었으나(활성 뷰포트), 커맨드가 없고 클리어도 불필요한 패스(RPLO_Load 패스 등) -> 스킵!
		// 3) 카메라는 등록되었으나 커맨드가 없고 RPLO_Clear인 패스(예: 빈 씬의 GeometryPass) -> 스킵하지 않고 진행하여 RT를 클리어! (이전 프레임 잔상 방지)
		if (!hasCommands && (!hasJobs || !needsClear))
			continue;

		m_RHI->BeginPassMetrics(pass->GetName());

		auto staticIt = m_StaticDrawCommands.find(pass->GetID());
		if (staticIt != m_StaticDrawCommands.end() && staticIt->second.size() > 1 &&
			pass->GetSortType() != ERenderSortType::BackToFront)
		{
			auto& commands = staticIt->second;
			std::sort(commands.begin(), commands.end(), [](const StaticDrawCommand& a, const StaticDrawCommand& b)
				{
					if (a.material != b.material)
						return std::less<MaterialInterface*>{}(a.material, b.material);
					return std::less<Mesh*>{}(a.mesh, b.mesh);
				});
		}
		const auto spriteIt = m_SpriteDrawCommands.find(pid);
		if (spriteIt != m_SpriteDrawCommands.end() && passType == ERenderPassType::Geometry &&
			pass->GetSortType() == ERenderSortType::FrontToBack)
		{
			auto& commands = spriteIt->second;
			const auto higherPriorityFirst = [](const SpriteDrawCommand& a, const SpriteDrawCommand& b)
				{
					if (a.sortingLayerID != b.sortingLayerID)
						return a.sortingLayerID > b.sortingLayerID;
					return a.orderInLayer > b.orderInLayer;
				};

			if (!std::is_sorted(commands.begin(), commands.end(), higherPriorityFirst))
				std::stable_sort(commands.begin(), commands.end(), higherPriorityFirst);
		}
		static const vector<ViewportJob> s_DefaultJobs = { ViewportJob{} };
		const vector<ViewportJob>& jobs = hasJobs ? jobsIt->second : s_DefaultJobs;

		for (const auto& job : jobs)
		{
			const EResult beginResult = m_RHI->BeginRenderPass(pass);
			if (IsFailure(beginResult))
			{
				ENGINE_LOG_ERROR("BeginRenderPass failed. Pass ID: {}", pass->GetID());
				return beginResult;
			}

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
					if (buffer.empty()) continue;

					const EResult result = m_RHI->BindConstantBuffer(
						buffer.data(), static_cast<uint32>(buffer.size()), slot);
					if (IsFailure(result))
					{
						ENGINE_LOG_ERROR("Failed to bind pass constant buffer. Slot: {}, Pass ID: {}", slot, pass->GetID());
						return result;
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
								if (IsFailure(m_RHI->BindDepthSlice(depthRT->GetTexture(), face)) ||
									IsFailure(m_RHI->ClearDepthSlice(depthRT->GetTexture(), face, 1.0f, 0)) ||
									IsFailure(m_RHI->SetViewport(0, 0, rtWidth, rtHeight)))
								{
									ENGINE_LOG_ERROR("Failed to prepare point shadow face. Face: {}", face);
									return EResult::Fail;
								}

								CameraBuffer faceCam = LightManager::Get().GetPointShadowCameraBuffer(face);
								faceCam.time = static_cast<f32>(TimeManager::Get().GetTotalTime());
								if (IsFailure(m_RHI->BindConstantBuffer(&faceCam, sizeof(CameraBuffer), 0)))
								{
									ENGINE_LOG_ERROR("Failed to bind point shadow face camera buffer. Face: {}", face);
									return EResult::Fail;
								}

								const Frustum faceFrustum = FrustumCuller::ExtractFrustum(faceCam.viewProjMatrix);
								const EResult result = RenderMeshes(meshIt->second, pass, &faceFrustum);
								if (IsFailure(result))
								{
									ENGINE_LOG_ERROR("Point shadow mesh rendering failed for face: {}", face);
									return result;
								}
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
							const EResult viewportResult = m_RHI->SetViewport(vpX, vpY, halfW, halfH);
							if (IsFailure(viewportResult))
							{
								ENGINE_LOG_ERROR("Failed to set shadow cascade viewport. Cascade: {}", c);
								return viewportResult;
							}

							const Frustum cascadeFrustum = FrustumCuller::ExtractFrustum(cascadeCam.viewProjMatrix);
							const EResult result = RenderMeshes(meshIt->second, pass, &cascadeFrustum);
							if (IsFailure(result))
							{
								ENGINE_LOG_ERROR("Shadow mesh rendering failed for cascade: {}", c);
								return result;
							}
						}

						// 2) Spot Light Shadow (우하단 사분면: halfW, halfH)
						if (LightManager::Get().HasSpotShadow())
						{
							CameraBuffer spotCam = LightManager::Get().GetSpotShadowCameraBuffer();
							spotCam.time = static_cast<f32>(TimeManager::Get().GetTotalTime());

							const EResult cameraResult = m_RHI->BindConstantBuffer(&spotCam, sizeof(CameraBuffer), 0);
							if (IsFailure(cameraResult))
							{
								ENGINE_LOG_ERROR("Failed to bind spot shadow camera buffer.");
								return cameraResult;
							}

							const EResult viewportResult = m_RHI->SetViewport(
								static_cast<int32>(halfW), static_cast<int32>(halfH), halfW, halfH);
							if (IsFailure(viewportResult))
							{
								ENGINE_LOG_ERROR("Failed to set spot shadow viewport.");
								return viewportResult;
							}

							const Frustum spotFrustum = FrustumCuller::ExtractFrustum(spotCam.viewProjMatrix);
							const EResult result = RenderMeshes(meshIt->second, pass, &spotFrustum);
							if (IsFailure(result))
							{
								ENGINE_LOG_ERROR("Spot shadow mesh rendering failed.");
								return result;
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
					if (buffer.empty()) continue;

					const EResult result = m_RHI->BindConstantBuffer(buffer.data(), static_cast<uint32>(buffer.size()), slot);
					if (IsFailure(result))
					{
						ENGINE_LOG_ERROR("Failed to bind pass constant buffer. Slot: {}, Pass ID: {}", slot, pass->GetID());
						return result;
					}
				}
				const EResult viewportResult = m_RHI->SetViewport(0, 0, rtWidth, rtHeight);
				if (IsFailure(viewportResult))
				{
					ENGINE_LOG_ERROR("Failed to set viewport. Pass ID: {}", pass->GetID());
					return viewportResult;
				}

				auto meshIt = m_StaticDrawCommands.find(pass->GetID());
				if (meshIt != m_StaticDrawCommands.end())
				{
					Frustum frustum{};
					const Frustum* activeFrustum = nullptr;

					if (passType == ERenderPassType::Geometry && job.hasCamera)
					{
						frustum = FrustumCuller::ExtractFrustum(cameraBuffer.viewProjMatrix);
						activeFrustum = &frustum;
					}

					const EResult result = RenderMeshes(meshIt->second, pass, activeFrustum);
					if (IsFailure(result))
					{
						ENGINE_LOG_ERROR("ECS mesh rendering failed. Pass ID: {}", pass->GetID());
						return result;
					}
				}

				auto skinnedIt = m_SkinnedDrawCommands.find(pass->GetID());
				if (skinnedIt != m_SkinnedDrawCommands.end())
				{
					const EResult result = RenderSkinnedMeshes(dt, skinnedIt->second, pass->GetSortType(), pass);
					if (IsFailure(result))
					{
						ENGINE_LOG_ERROR("ECS skinned mesh rendering failed. Pass ID: {}", pass->GetID());
						return result;
					}
				}

				auto spriteIt = m_SpriteDrawCommands.find(pass->GetID());
				if (spriteIt != m_SpriteDrawCommands.end())
				{
					const EResult result = RenderSprite(dt, spriteIt->second, pass->GetSortType(), pass);
					if (IsFailure(result))
					{
						ENGINE_LOG_ERROR("ECS sprite rendering failed. Pass ID: {}", pass->GetID());
						return result;
					}
				}

				if (const auto images = m_UIImageDrawCommands.find(pass->GetID()); images != m_UIImageDrawCommands.end())
				{
					const EResult result = RenderUIImages(images->second, pass);
					if (IsFailure(result))
					{
						ENGINE_LOG_ERROR("ECS UI image rendering failed. Pass ID: {}", pass->GetID());
						return result;
					}
				}
			}
			
			auto customIt = m_CustomRenderQueues.find(pass->GetID());
			if (customIt != m_CustomRenderQueues.end())
			{
				for (const auto& command : customIt->second)
				{
					const EResult result = command(dt, pass);
					if (IsFailure(result))
						return result;
				}
			}

			if (IsFailure(m_RHI->EndRenderPass()))
				return EResult::Fail;
			if (passType == ERenderPassType::Geometry) 
				PickingManager::Get().MarkPickingPass(pass->GetID());
		}
		GetRenderPassDelegate(pass->GetID()).Broadcast(dt);
		m_RHI->EndPassMetrics();
	}

	return EResult::Success;
}

TODO("Renderer::RenderMeshes, RenderSkinnedMeshes, RenderSprite 함수에서 Material과 Mesh를 Bind하고 DrawIndexed 호출하는 부분을 RHI에 맞게 구현 필요")
EResult Renderer::RenderMeshes(const vector<StaticDrawCommand>& commands, RenderPass* renderPass, const Frustum* frustum)
{
	if (!m_RHI || !renderPass)
		return EResult::Fail;

	if (commands.empty())
		return EResult::Success;

	RHIPipelineDesc pipelineDesc;
	const EResult prepareResult = PreparePassPipelineDesc(renderPass, pipelineDesc);
	if (IsFailure(prepareResult))
		return prepareResult;

	const ERenderPassType passType = renderPass->GetPassType();
	MaterialInterface* lastMaterial = nullptr;
	Mesh* lastMesh = nullptr;

	MaterialInterface* batchMaterial = nullptr;
	Mesh* batchMesh = nullptr;
	vector<InstanceData> instanceBatch;
	instanceBatch.reserve(128);

	auto flushBatch = [&]() -> EResult
		{
			if (instanceBatch.empty() || !batchMesh || !batchMaterial)
				return EResult::Success;

			if (batchMaterial != lastMaterial || batchMesh != lastMesh)
			{
				const auto bindResult = BindMeshPipeline(batchMesh, batchMaterial, passType, pipelineDesc);
				if (bindResult != EResult::Success)
				{
					// [결함 격리]: 특정 머티리얼 오류로 전체 프레임 렌더링이 멈추지 않도록 해당 배치만 스킵
					instanceBatch.clear();
					return EResult::Success;
				}
			}
			if (batchMaterial != lastMaterial)
			{
				if (batchMaterial->Bind(2) != EResult::Success)
				{
					instanceBatch.clear();
					return EResult::Success;
				}
				lastMaterial = batchMaterial;
			}

			if (batchMesh != lastMesh)
			{
				if (IsFailure(batchMesh->Bind(0)))
					return EResult::Fail;
				lastMesh = batchMesh;
			}

			const uint32 instanceCount = static_cast<uint32>(instanceBatch.size());
			if (IsFailure(m_RHI->BindInstanceBuffer(instanceBatch.data(), instanceCount)))
				return EResult::Fail;

			const EResult result = batchMesh->GetIndexBuffer()
				? m_RHI->DrawIndexedInstanced(batchMesh->GetIndexCount(), instanceCount)
				: m_RHI->DrawInstanced(batchMesh->GetVertexCount(), instanceCount);

			instanceBatch.clear();
			return result;
		};

	for (const auto& command : commands)
	{
		Mesh* mesh = command.mesh;
		MaterialInterface* material = command.material;

		if (!mesh || !material)
			return EResult::Fail;

		// 프러스텀 컬링
		if (frustum && command.worldBounds && !FrustumCuller::TestBoundingBox(*frustum, *command.worldBounds))
			continue;

		// 지형(Heightfield) 등 단일 드로우 특수 객체 처리
		if (command.heightfield)
		{
			if (IsFailure(flushBatch()))
				return EResult::Fail;

			batchMaterial = nullptr;
			batchMesh = nullptr;

			if (material != lastMaterial || mesh != lastMesh)
			{
				if (IsFailure(BindMeshPipeline(mesh, material, passType, pipelineDesc)))
					return EResult::Fail;
			}

			if (material != lastMaterial)
			{
				if (IsFailure(material->Bind(2)))
					return EResult::Fail;
				lastMaterial = material;
			}

			const auto& heightfield = *command.heightfield;
			RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();
			if (!heightfield.heightmap || !heightfield.splatmap || !sampler)
				return EResult::Fail;

			if (IsFailure(m_RHI->BindTextureSampler(heightfield.heightmap.get(), sampler, 1)) ||
				IsFailure(m_RHI->BindTextureSampler(heightfield.splatmap.get(), sampler, 4)))
				return EResult::Fail;

			SceneUBO objectData{};
			objectData.worldMatrix = command.worldMatrix;
			objectData.pickingData.x = static_cast<float>(command.pickingId);
			objectData.heightfieldData = vec4(
				heightfield.maxHeight, heightfield.cellSpacing.x, heightfield.cellSpacing.y, 0.f);

			lastMaterial = nullptr;

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

			continue;
		}

		// 일반 정적 메시: 머티리얼 또는 메시가 달라지면 이전 배치 플러시
		if (material != batchMaterial || mesh != batchMesh)
		{
			if (IsFailure(flushBatch()))
				return EResult::Fail;

			batchMaterial = material;
			batchMesh = mesh;
		}

		InstanceData inst{};
		inst.worldMatrix = command.worldMatrix;
		inst.pickingData.x = static_cast<float>(command.pickingId);
		instanceBatch.push_back(inst);
	}

	// 잔여 배치 최종 플러시
	return flushBatch();
}

EResult Renderer::RenderSkinnedMeshes(f32 dt, vector<SkinnedDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass)
{
	if (!m_RHI || !renderPass) return EResult::Fail;
	if (commands.empty()) return EResult::Success;
	RHIPipelineDesc pipelineDesc;
	const EResult prepareResult = PreparePassPipelineDesc(renderPass, pipelineDesc);
	if (IsFailure(prepareResult)) return prepareResult;
	const ERenderPassType passType = renderPass->GetPassType();
	MaterialInterface* lastMaterial = nullptr;
	Mesh* lastMesh = nullptr;
	static const vector<mat4> s_IdentityBones(256, glm::identity<mat4>());

	for (auto& command : commands)
	{
		if (!command.mesh || !command.material) continue;
		if (command.material != lastMaterial || command.mesh != lastMesh)
		{
			if (IsFailure(BindMeshPipeline(command.mesh, command.material, passType, pipelineDesc)))
				return EResult::Fail;
		}
		if (command.material != lastMaterial)
		{
			if (IsFailure(command.material->Bind(2)))
				return EResult::Fail;
			lastMaterial = command.material;
		}
		// 드로우별 머티리얼 상수 버퍼 바인딩 (slot 2: b0, space3)
		// 머티리얼을 공유하더라도 개별 드로우의 상수(피격 발광, 파라미터 오버라이드 등)가 독립 적용되도록 보장
		MaterialConstants drawConstants = command.materialConstants;
		if (drawConstants.materialFlags == 0 && command.material)
		{
			command.material->BuildMaterialConstants(drawConstants);
		}
		if (IsFailure(m_RHI->BindConstantBuffer(&drawConstants, sizeof(MaterialConstants), 2)))
			return EResult::Fail;
		if (command.mesh != lastMesh)
		{
			if (IsFailure(command.mesh->Bind(0)))
				return EResult::Fail;
			lastMesh = command.mesh;
		}
		// 1. 오브젝트 상수 버퍼 (월드 행렬 등) 바인딩: b0, space2 (Root Parameter [3])
		SceneUBO objectData{};
		objectData.worldMatrix = command.worldMatrix;
		objectData.pickingData.x = static_cast<float>(command.pickingId);
		if (IsFailure(m_RHI->BindConstantBuffer(&objectData, sizeof(objectData), 1)))
			return EResult::Fail;
		// 2. 뼈 행렬 StructuredBuffer 바인딩 (최소 256개 본 크기 보장으로 GPU OOB 원천 차단)
		static vector<mat4> s_BoneUploadBuffer(256, glm::identity<mat4>());
		if (command.boneMatrices && !command.boneMatrices->empty())
		{
			const size_t copyCount = std::min(command.boneMatrices->size(), s_BoneUploadBuffer.size());
			std::memcpy(s_BoneUploadBuffer.data(), command.boneMatrices->data(), copyCount * sizeof(mat4));
			if (copyCount < s_BoneUploadBuffer.size())
			{
				std::fill(s_BoneUploadBuffer.begin() + copyCount, s_BoneUploadBuffer.end(), glm::identity<mat4>());
			}
			if (IsFailure(m_RHI->BindInstanceBuffer(s_BoneUploadBuffer.data(), static_cast<uint32>(s_BoneUploadBuffer.size()), sizeof(mat4))))
				return EResult::Fail;
		}
		else
		{
			if (IsFailure(m_RHI->BindInstanceBuffer(s_IdentityBones.data(), static_cast<uint32>(s_IdentityBones.size()), sizeof(mat4))))
				return EResult::Fail;
		}
		// 3. 인덱스 드로우
		const EResult drawResult = command.mesh->GetIndexBuffer()
			? m_RHI->DrawIndexed(command.mesh->GetIndexCount())
			: m_RHI->Draw(command.mesh->GetVertexCount());
		if (IsFailure(drawResult))
			return drawResult;
	}
	return EResult::Success;
}

EResult Renderer::RenderSprite(f32 dt, vector<SpriteDrawCommand>& commands, ERenderSortType sortType, RenderPass* renderPass)
{
	if (!m_RHI || !renderPass)
		return EResult::Fail;

	if (commands.empty())
		return EResult::Success;

	RHIPipelineDesc pipelineDesc;
	const EResult prepareResult = PreparePassPipelineDesc(renderPass, pipelineDesc);
	if (IsFailure(prepareResult))
		return prepareResult;

	const ERenderPassType passType = renderPass->GetPassType();

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

	auto& samplerManager = SamplerManager::Get();
	RHISampler* sampler = samplerManager.GetDefaultSampler();

	if (renderPass->UsesPointSpriteSampling())
	{
		SamplerDesc desc{};
		desc.MinFilter = ESamplerFilter::Point;
		desc.MagFilter = ESamplerFilter::Point;
		desc.MipFilter = ESamplerFilter::Point;
		sampler = samplerManager.GetOrCreateSampler(desc);
	}

	if (!sampler)
		return EResult::Fail;

	MaterialInterface* lastMaterial = nullptr;
	Mesh* lastMesh = nullptr;
	Texture* lastTexture = nullptr;

	for (const auto& command : commands)
	{
		if (!command.mesh || !command.material || !command.texture)
			return EResult::Fail;
		RHITexture* texture = command.texture->GetRHITexture();
		if (!texture || !command.mesh->GetIndexBuffer())
			return EResult::Fail;
		if (command.material != lastMaterial || command.mesh != lastMesh)
		{
			if (IsFailure(BindMeshPipeline(command.mesh, command.material, passType, pipelineDesc)))
				return EResult::Fail;
			lastMaterial = command.material;
		}
		// 텍스처가 이전 커맨드와 다를 때만 무거운 Sampler 바인딩 갱신
		if (command.texture != lastTexture)
		{
			if (IsFailure(m_RHI->BindTextureSampler(texture, sampler, 0)))
				return EResult::Fail;
			lastTexture = command.texture;
		}
		SpriteUBO objectData{};
		objectData.worldMatrix = command.worldMatrix;
		objectData.color = command.color;
		objectData.uvTransform = command.uvTransform;
		objectData.pickingData = vec4(static_cast<float>(command.pickingId), 0.0f, 0.0f, 0.0f);
		if (IsFailure(m_RHI->BindConstantBuffer(&objectData, sizeof(objectData), 1)))
			return EResult::Fail;
		if (command.mesh != lastMesh)
		{
			if (IsFailure(command.mesh->Bind(0)))
				return EResult::Fail;
			lastMesh = command.mesh;
		}
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
	RHIPipelineDesc pipelineDesc;
	if (!commands.empty())
	{
		const EResult prepareResult = PreparePassPipelineDesc(renderPass, pipelineDesc);
		if (IsFailure(prepareResult))
			return prepareResult;
	}

	const ERenderPassType passType = renderPass->GetPassType();
	MaterialInterface* lastMaterial = nullptr;
	vec2 lastCanvasSize(0.0f);
	for (const auto& command : commands)
	{
		Texture* texture = command.textureHandle.Get();
		MaterialInterface* material = command.materialHandle.Get();
		if (!texture || !material || !texture->GetRHITexture() ||
			command.canvasSize.x <= 0.0f || command.canvasSize.y <= 0.0f) return EResult::InvalidArgument;
		if (material != lastMaterial)
		{
			if (IsFailure(BindMeshPipeline(nullptr, material, passType, pipelineDesc)))
			{
				auto fallbackHandle = ResourceManager::Get().GetResourceHandle<MaterialInterface>(L"Resources/Material/UIMaterial.bammat.json");
				MaterialInterface* fallbackMat = fallbackHandle.Get();
				if (fallbackMat && fallbackMat != material && IsSuccess(BindMeshPipeline(nullptr, fallbackMat, passType, pipelineDesc)))
				{
					lastMaterial = fallbackMat;
				}
				else
				{
					continue;
				}
			}
			else
			{
				lastMaterial = material;
			}
		}
		if (command.canvasSize != lastCanvasSize)
		{
			CameraBuffer camera{};
			camera.viewMatrix = camera.invViewMatrix = mat4(1.0f);
			camera.projMatrix = glm::ortho(0.0f, command.canvasSize.x, command.canvasSize.y, 0.0f, -1.0f, 1.0f);
			camera.invProjMatrix = glm::inverse(camera.projMatrix);
			camera.viewProjMatrix = camera.projMatrix;
			camera.invViewProjMatrix = camera.invProjMatrix;

			if (IsFailure(m_RHI->BindConstantBuffer(&camera, sizeof(camera), 0)))
				return EResult::Fail;

			lastCanvasSize = command.canvasSize;
		}
		ImageUBO data{};
		data.worldMatrix = command.worldMatrix;
		data.uvRect = vec4(vec2(command.uvTransform),
			vec2(command.uvTransform) + vec2(command.uvTransform.z, command.uvTransform.w));
		data.tiling = vec2(1.0f);
		data.color = command.color;
		if (IsFailure(m_RHI->BindConstantBuffer(&data, sizeof(data), 1)) ||
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

void Renderer::ClearFrameQueues()
{
	for (auto& [passID, queue] : m_CustomRenderQueues) queue.clear();
	for (auto& [passID, queue] : m_StaticDrawCommands) queue.clear();
	for (auto& [passID, queue] : m_SkinnedDrawCommands) queue.clear();
	for (auto& [passID, queue] : m_SpriteDrawCommands) queue.clear();
	for (auto& [passID, queue] : m_UIImageDrawCommands) queue.clear();
	for (auto& [passID, jobs] : m_ViewportJobsPerPass) jobs.clear();

	m_ViewportCameras.clear();
}

EResult Renderer::EndFrame()
{
	EResult frameResult = EResult::Fail;
	if (m_RHI)
	{
		frameResult = m_RHI->EndRenderPass();
		if (IsSuccess(frameResult))
			frameResult = m_RHI->EndFrame();
	}

	ClearFrameQueues();
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
	m_ViewportJobsPerPass.erase(passID);
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
	m_ViewportJobsPerPass.clear();
}

void Renderer::ForgetRenderPass(RenderPassID passID)
{
	ClearRenderQueue(passID);
	UnregisterViewportCamera(passID);
	m_RenderPassDelegates.erase(passID);
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
	if (!renderPass) return;
	m_ViewportCameras.push_back({ cameraBuffer, true, renderPass });
}

void Renderer::RegisterViewportCamera(RenderPass* renderPass)
{
	if (!renderPass) return;
	m_ViewportCameras.push_back({ {}, false, renderPass });
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
vector<ViewportCameraInfo> Renderer::CollectGeometryViewportCameras() const
{
	vector<ViewportCameraInfo> targets;
	for (const auto& viewport : m_ViewportCameras)
	{
		auto* pass = viewport.renderPass;
		if (!pass || !viewport.hasCamera || pass->GetPassType() != ERenderPassType::Geometry) continue;

		if (std::ranges::find(targets, pass, &ViewportCameraInfo::renderPass) == targets.end())
			targets.push_back(viewport);
	}

	return targets;
}
#pragma endregion

#pragma region Binding
EResult Renderer::PreparePassPipelineDesc(RenderPass* renderPass, RHIPipelineDesc& desc)
{
	if (!renderPass)
		return EResult::InvalidArgument;

	desc = RHIPipelineDesc{};
	desc.colorAttachmentCount = renderPass->GetRenderTargetCount();

	if (desc.colorAttachmentCount > MAX_RENDER_TARGET_COUNT)
		return EResult::InvalidArgument;

	auto& rtManager = RenderTargetManager::Get();

	for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
	{
		RenderTarget* rt = rtManager.GetRenderTarget(renderPass->GetRenderTargetName(i));
		if (!rt)
			return EResult::InvalidArgument;

		desc.colorAttachmentFormats[i] = rt->GetFormat();
	}

	const wstring& depthStencilName = renderPass->GetDepthStencilName();

	// [수정] 렌더 타깃이 없고 Depth도 없는 백버퍼(스왑체인) 전용 패스인 경우 백버퍼 RTV 1개 설정
	if (desc.colorAttachmentCount == 0 && depthStencilName.empty())
	{
		desc.colorAttachmentCount = 1;
		desc.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
	}

	desc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;

	if (!depthStencilName.empty())
	{
		RenderTarget* ds = rtManager.GetRenderTarget(depthStencilName);
		if (!ds)
			return EResult::InvalidArgument;

		desc.depthStencilAttachmentFormat = ds->GetFormat();
	}

	return EResult::Success;
}

EResult Renderer::PrepareMeshPipelineDesc(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc)
{
	if (!m_RHI || !material) return EResult::InvalidArgument;
	Shader* vs = ResolveVertexShader(mesh, material, passType);
	Shader* ps = material->GetPixelShader(passType);
	if (!vs || !ps || !vs->GetRHIShader() || !ps->GetRHIShader()) return EResult::InvalidArgument;
	desc.pipelineType = EPipelineType::Graphics;
	desc.topology = mesh ? mesh->GetTopology() : ETopology::TriangleList;
	if (mesh)
	{
		const auto& inputLayouts = mesh->GetInputLayoutDescs();
		const std::wstring& vsKey = vs->GetKey();
		const bool isSkinningVS = (vsKey.find(L"Skinning") != std::wstring::npos || vsKey.find(L"skinning") != std::wstring::npos);
		const bool hasSkinLayout = (inputLayouts.size() > 2);
		if (isSkinningVS && !hasSkinLayout) return EResult::InvalidArgument;
		// 정적 VS인데 스킨드 레이아웃이 온 경우, 셰이더가 요구하지 않는 슬롯 2(SkinData)를 제외하고 안전하게 정적 레이아웃만 전달
		if (!isSkinningVS && hasSkinLayout)
		{
			desc.inputLayouts.assign(inputLayouts.begin(), inputLayouts.begin() + 2);
		}
		else
		{
			if (desc.inputLayouts != inputLayouts) desc.inputLayouts = inputLayouts;
		}
	}
	else
	{
		desc.inputLayouts.clear();
	}
	desc.vertexShader = vs->GetRHIShader();
	desc.pixelShader = ps->GetRHIShader();
	desc.frontFace = material->GetFrontFace();
	desc.cullMode = material->GetCullMode();
	desc.fillMode = material->GetFillMode();
	desc.blendState = material->GetBlendState();
	const bool hasDepthStencil = desc.depthStencilAttachmentFormat != ETextureFormat::UNKNOWN;
	desc.depthStencilState.depthTestEnable = hasDepthStencil && material->GetDepthMode() != EDepthMode::None;
	desc.depthStencilState.depthWriteEnable = hasDepthStencil && material->GetDepthMode() == EDepthMode::ReadWrite;
	desc.depthStencilState.depthCompareOp = material->GetDepthCompareOp();
	return EResult::Success;
}

EResult Renderer::BindMeshPipeline(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc)
{
	const auto result = PrepareMeshPipelineDesc(mesh, material, passType, desc);
	if (result != EResult::Success)
	{
		static std::unordered_set<std::string> s_LoggedInvalidMaterials;
		const std::string matKey = material ? WStrToStr(material->GetKey()) : "null";
		if (s_LoggedInvalidMaterials.insert(matKey).second)
		{
			BAM_LOG(Warn, "Renderer", "BindMeshPipeline", "Failed to prepare pipeline for material: '{}' (Missing VS/PS). Skipping draw call.", matKey);
		}
		return result;
	}
	auto* pipeline = PipelineManager::Get().GetOrCreatePipeline(desc);
	if (!pipeline) return EResult::Fail;
	return m_RHI->BindPipeline(pipeline);
}

EResult Renderer::ValidateMeshMaterial(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass)
{
	if (!m_RHI || !mesh || !material || !renderPass || !PipelineManager::IsCreated())
		return EResult::InvalidArgument;

	RHIPipelineDesc desc;
	auto result = PreparePassPipelineDesc(renderPass, desc);
	if (result != EResult::Success) return result;

	result = PrepareMeshPipelineDesc(mesh, material, renderPass->GetPassType(), desc);
	if (result != EResult::Success) return result;

	return PipelineManager::Get().GetOrCreatePipeline(desc) ? EResult::Success : EResult::Fail;
}

EResult Renderer::BindMeshPipeline(Mesh* mesh, MaterialInterface* material, RenderPass* renderPass)
{
	if (!m_RHI || !material || !renderPass)
		return EResult::InvalidArgument;

	RHIPipelineDesc desc;
	const auto result = PreparePassPipelineDesc(renderPass, desc);
	if (IsFailure(result)) return result;

	return BindMeshPipeline(mesh, material, renderPass->GetPassType(), desc);
}
#pragma endregion

#pragma region Default Shaders
void Renderer::EnsureDefaultShaders()
{
	if (m_DefaultGBufferVS.IsValid() && m_DefaultGBufferVS.Get()) return;
	auto& rm = ResourceManager::Get();
	auto getOrLoad = [&](const std::wstring& path) -> ResourceHandle<Shader>
		{
			auto handle = rm.GetResourceHandle<Shader>(path);
			if (!handle.IsValid() || !handle.Get())
			{
				auto loaded = rm.LoadFile(path);
				if (loaded.IsValid()) handle = ResourceHandle<Shader>(loaded);
			}
			return handle;
		};
	m_DefaultGBufferVS = getOrLoad(L"Resources/Shader/GBufferVS");
	m_DefaultGBufferSkinningVS = getOrLoad(L"Resources/Shader/GBufferSkinningVS");
	m_DefaultShadowDepthVS = getOrLoad(L"Resources/Shader/ShadowDepthVS");
	m_DefaultShadowDepthSkinningVS = getOrLoad(L"Resources/Shader/ShadowDepthSkinningVS");
}
Shader* Renderer::ResolveVertexShader(Mesh* mesh, MaterialInterface* material, ERenderPassType passType)
{
	Shader* matVS = material ? material->GetVertexShader(passType) : nullptr;
	if (!mesh || !matVS) return matVS;
	const bool isSkinned = mesh->IsSkinned();
	const std::wstring vsKey = matVS->GetKey();
	const bool isSkinningVS = (vsKey.find(L"Skinning") != std::wstring::npos || vsKey.find(L"skinning") != std::wstring::npos);
	// 1. [안전 보장] 메시와 머티리얼의 스키닝 속성이 일치하면 기존 셰이더를 100% 그대로 유지합니다.
	// (기존의 모든 정상적인 정적 메시 드로우콜은 이 분기를 타고 그대로 렌더링되므로 안전합니다.)
	if (isSkinned == isSkinningVS)
	{
		return matVS;
	}
	// 2. [불일치 해결] 오직 타입이 어긋났을 때만 대체 셰이더를 찾습니다.
	EnsureDefaultShaders();
	if (isSkinned)
	{
		// 스킨드 메시인데 정적 VS가 온 경우 -> 스키닝 VS로 자동 승격
		if (passType == ERenderPassType::Shadow)
		{
			Shader* shadowSkinVS = m_DefaultShadowDepthSkinningVS.Get();
			return shadowSkinVS ? shadowSkinVS : matVS;
		}
		Shader* gbufferSkinVS = m_DefaultGBufferSkinningVS.Get();
		return gbufferSkinVS ? gbufferSkinVS : matVS;
	}
	else
	{
		// 정적 메시인데 스키닝 VS가 온 경우 -> 정적 VS로 안전 강등 (마테리얼 에디터 프리뷰 구체 등)
		if (passType == ERenderPassType::Shadow)
		{
			Shader* shadowVS = m_DefaultShadowDepthVS.Get();
			return shadowVS ? shadowVS : matVS;
		}
		Shader* gbufferVS = m_DefaultGBufferVS.Get();
		return gbufferVS ? gbufferVS : matVS;
	}
}
#pragma endregion
