#pragma once

#include "Renderer.h"
//RHI
#include "RHI.h"
#if !defined(__EMSCRIPTEN__)
#include "SDLGPURHI.h"
#endif
#if defined(_WIN32)
#include "DirectX12RHI.h"
#endif
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
#include "MeshWinding.h"
#include "MotionHistory.h"
#include "Shader.h"
#include "Texture.h"
#include "MaterialInterface.h"
//Light
#include "LightManager.h"
#include "RaytracingSceneManager.h"

#include "SamplerManager.h"
#include "SceneManager.h"
#include "WorldComponents.h"
#include "PickingManager.h"

#include "TimeManager.h"
#include "PlatformPath.h"
#include <algorithm>

IMPLEMENT_SINGLETON(Renderer)

#pragma region Constructor&Destructor
EResult Renderer::Initialize(void* arg)
{	
	if (!arg) return EResult::InvalidArgument;

	CAST_DESC
	m_RHIType = desc->rhiType;
	switch (m_RHIType)
	{
#if !defined(__EMSCRIPTEN__)
	case ERHIType::SDLGPU:
		m_RHI = SDLGPURHI::Create(desc->rhiDesc);
		break;
#endif
#if defined(_WIN32)
	case ERHIType::DirectX12:
		m_RHI = DirectX12RHI::Create(desc->rhiDesc);
		break;
#endif
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
	m_DefaultShadowDepthPS = {};
	m_MaskedShadowDepthVS = {};
	m_MaskedShadowDepthSkinningVS = {};
	m_MaskedShadowDepthPS = {};

	m_RenderPassManager = nullptr;

	if (m_RHI)
	{
		Safe_Release(m_RHI);
	}
}
#pragma endregion

namespace
{
	// 반투명 패스: 카메라에서 먼 물체부터 그린다. 거리가 같으면 제출 순서를 유지해 깜빡임을 막는다.
	void SortStaticBackToFront(vector<StaticDrawCommand>& commands, const vec3& cameraPos)
	{
		const auto distanceSq = [&cameraPos](const StaticDrawCommand& command)
			{
				const vec3 center = command.worldBounds ? command.worldBounds->Center() : vec3(command.worldMatrix[3]);
				const vec3 offset = center - cameraPos;
				return glm::dot(offset, offset);
			};

		std::stable_sort(commands.begin(), commands.end(), [&distanceSq](const StaticDrawCommand& a, const StaticDrawCommand& b) { return distanceSq(a) > distanceSq(b); });
	}
}

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
	m_PreRenderDelegate.Broadcast(dt);

	const EResult lightResult = LightManager::Get().UpdateLightBuffer();
	if (IsFailure(lightResult))
		return lightResult;

	// 레이트레이싱 그림자용 BLAS/TLAS를 패스 실행 전에 빌드한다.
	// 실패한 씬은 GetTopLevelAS가 nullptr를 반환해 기존 섀도맵 경로로 남으므로 프레임은 계속 진행한다.
	if (RaytracingSceneManager::IsCreated())
	{
		auto& raytracing = RaytracingSceneManager::Get();
		const bool measure = raytracing.HasRequests();
		if (measure) m_RHI->BeginPassMetrics(L"RaytracingAccelerationStructures");
		raytracing.BuildRequestedScenes();
		if (measure) m_RHI->EndPassMetrics();
	}

	auto& jobsPerPass = m_ViewportJobsPerPass;
	for (auto& [passID, jobs] : jobsPerPass)
		jobs.clear();

	for (const auto& viewportInfo : m_ViewportCameras)
	{
		if (!viewportInfo.renderPass) continue;
		jobsPerPass[viewportInfo.renderPass->GetID()].push_back({ viewportInfo.cameraBuffer, viewportInfo.hasCamera,
			viewportInfo.lodCamera, viewportInfo.hasLODCamera });
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
			(m_DynamicGeometryDrawCommands.find(pid) != m_DynamicGeometryDrawCommands.end() && !m_DynamicGeometryDrawCommands.at(pid).empty()) ||
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
			if (passType == ERenderPassType::Shadow)
			{
				std::sort(commands.begin(), commands.end(), [](const StaticDrawCommand& a, const StaticDrawCommand& b)
					{
						if (a.material && b.material)
						{
							const bool aMasked = (a.material->GetBlendMode() == EBlendMode::Masked);
							const bool bMasked = (b.material->GetBlendMode() == EBlendMode::Masked);
							if (aMasked != bMasked)
								return aMasked < bMasked;
							if (a.material->GetCullMode() != b.material->GetCullMode())
								return a.material->GetCullMode() < b.material->GetCullMode();
							if (a.material->GetFrontFace() != b.material->GetFrontFace())
								return a.material->GetFrontFace() < b.material->GetFrontFace();
							if (aMasked && a.material != b.material)
								return std::less<MaterialInterface*>{}(a.material, b.material);
						}
						if (a.mesh != b.mesh)
							return std::less<Mesh*>{}(a.mesh, b.mesh);
						if (a.mirrored != b.mirrored)
							return a.mirrored < b.mirrored;
						return std::less<MaterialInterface*>{}(a.material, b.material);
					});
			}
			else
			{
				std::sort(commands.begin(), commands.end(), [passType](const StaticDrawCommand& a, const StaticDrawCommand& b)
					{
						if (a.material != b.material)
						{
							if (a.material && b.material)
							{
								const Shader* aPS = a.material->GetPixelShader(passType);
								const Shader* bPS = b.material->GetPixelShader(passType);
								if (aPS != bPS) return std::less<const Shader*>{}(aPS, bPS);

								const Shader* aVS = a.material->GetVertexShader(passType);
								const Shader* bVS = b.material->GetVertexShader(passType);
								if (aVS != bVS) return std::less<const Shader*>{}(aVS, bVS);

								if (a.material->GetBlendMode() != b.material->GetBlendMode())
									return a.material->GetBlendMode() < b.material->GetBlendMode();
								if (a.material->GetCullMode() != b.material->GetCullMode())
									return a.material->GetCullMode() < b.material->GetCullMode();
								if (a.material->GetFillMode() != b.material->GetFillMode())
									return a.material->GetFillMode() < b.material->GetFillMode();
								if (a.material->GetDepthMode() != b.material->GetDepthMode())
									return a.material->GetDepthMode() < b.material->GetDepthMode();
								if (a.material->GetFrontFace() != b.material->GetFrontFace())
									return a.material->GetFrontFace() < b.material->GetFrontFace();
							}
							return std::less<MaterialInterface*>{}(a.material, b.material);
						}
						if (a.mesh != b.mesh)
							return std::less<Mesh*>{}(a.mesh, b.mesh);
						return a.mirrored < b.mirrored;
					});
			}
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
			if (pass->HasPrePassCopy() && m_RHI)
			{
				auto& rtm = RenderTargetManager::Get();
				auto* srcRT = rtm.GetRenderTarget(pass->GetPrePassCopySource());
				auto* dstRT = rtm.GetRenderTarget(pass->GetPrePassCopyDest());
				if (srcRT && dstRT && srcRT->GetTexture() && dstRT->GetTexture())
				{
					m_RHI->CopyTexture(srcRT->GetTexture(), dstRT->GetTexture());
				}
			}

			const EResult beginResult = m_RHI->BeginRenderPass(pass);
			if (IsFailure(beginResult))
			{
				ENGINE_LOG_ERROR("BeginRenderPass failed. Pass ID: {}", pass->GetID());
				return beginResult;
			}

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

				const auto meshIt = m_StaticDrawCommands.find(pid);
				const auto skinnedIt = m_SkinnedDrawCommands.find(pid);
				const bool hasStaticCasters = meshIt != m_StaticDrawCommands.end() && !meshIt->second.empty();
				const bool hasSkinnedCasters = skinnedIt != m_SkinnedDrawCommands.end() && !skinnedIt->second.empty();
				// 레이트레이싱하는 광원의 섀도맵에는 이번 프레임 TLAS에 들어가지 못한 캐스터만 그린다.
				// BLAS 대기·실패나 TLAS 빌드 실패 시에는 해당 캐스터가 섀도맵에 남아 그림자가 사라지지 않는다.
				// 같은 아틀라스라도 레이트레이싱하지 않는 광원은 전체 캐스터를 그대로 사용한다.
				// 정적·스킨드 캐스터 모두 같은 기준으로 거른다(스킨드 메시는 Compute 스키닝 BLAS로 TLAS에 들어간다).
				vector<StaticDrawCommand> rasterOnlyCasters;
				vector<SkinnedDrawCommand> rasterOnlySkinnedCasters;
				const bool hasCasters = hasStaticCasters || hasSkinnedCasters;
				const bool filterCascades = hasCasters && !isCubeShadow && pass->SkipsRaytracedShadowCasters();
				const bool filterSpot = hasCasters && !isCubeShadow && pass->SkipsRaytracedSpotCasters();
				const bool filterPoint = hasCasters && isCubeShadow && pass->SkipsRaytracedShadowCasters();
				if (filterCascades || filterSpot || filterPoint)
				{
					const Scene* casterScene = nullptr;
					RenderPassID casterView = INVALID_PASS_ID;
					const RaytracedCasterSet* tracedCasters = nullptr;
					const auto isRasterOnly = [&](const Scene* scene, RenderPassID viewID, uint32 entityIndex)
						{
							if (!scene) return true;
							if (scene != casterScene || viewID != casterView)
							{
								casterScene = scene;
								casterView = viewID;
								tracedCasters = RaytracingSceneManager::IsCreated() ?
									RaytracingSceneManager::Get().GetTopLevelCasters(casterScene, casterView) : nullptr;
							}
							return !tracedCasters || !tracedCasters->Contains(entityIndex);
						};
					if (hasStaticCasters)
					{
						std::copy_if(meshIt->second.begin(), meshIt->second.end(), std::back_inserter(rasterOnlyCasters),
							[&](const StaticDrawCommand& command)
							{ return isRasterOnly(command.raytracedScene, command.raytracedViewID, command.raytracedEntityIndex); });
					}
					if (hasSkinnedCasters)
					{
						std::copy_if(skinnedIt->second.begin(), skinnedIt->second.end(), std::back_inserter(rasterOnlySkinnedCasters),
							[&](const SkinnedDrawCommand& command)
							{ return isRasterOnly(command.raytracedScene, command.raytracedViewID, command.raytracedEntityIndex); });
					}
				}
				bool rasterOnly = false;
				const auto renderShadowCasters = [&](const CameraBuffer& lightCamera) -> EResult
				{
					const vector<StaticDrawCommand>* staticCasters = hasStaticCasters ? &meshIt->second : nullptr;
					vector<SkinnedDrawCommand>* skinnedCasters = hasSkinnedCasters ? &skinnedIt->second : nullptr;
					if (rasterOnly)
					{
						staticCasters = &rasterOnlyCasters;
						skinnedCasters = &rasterOnlySkinnedCasters;
					}
					if (staticCasters && !staticCasters->empty())
					{
						const Frustum frustum = FrustumCuller::ExtractFrustum(lightCamera.viewProjMatrix);
						const EResult result = RenderMeshes(*staticCasters, pass, &frustum);
						if (IsFailure(result)) return result;
					}
					if (skinnedCasters && !skinnedCasters->empty())
						return RenderSkinnedMeshes(dt, *skinnedCasters, pass->GetSortType(), pass);
					return EResult::Success;
				};

				if (hasStaticCasters || hasSkinnedCasters)
				{
					if (isCubeShadow)
					{
						// ── Point Light: 6면 큐브맵 개별 슬라이스 순회 렌더링 ──
						if (LightManager::Get().HasPointShadow())
						{
							rasterOnly = filterPoint;
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

								const EResult result = renderShadowCasters(faceCam);
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

						// 1) Directional Light CSM (사분면 0, 1, 2). 캐스케이드는 이 패스를 등록한 뷰의 카메라를 덮는다.
						DirectionalShadowCascades cascades;
						const bool hasCascades = LightManager::Get().BuildDirectionalCascades(job.hasViewCamera ? &job.viewCamera : nullptr, cascades);
						for (uint32 c = 0; hasCascades && c < MAX_SHADOW_CASCADES; ++c)
						{
							CameraBuffer cascadeCam = cascades.Cameras[c];
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

							rasterOnly = filterCascades;
							const EResult result = renderShadowCasters(cascadeCam);
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

							rasterOnly = filterSpot;
							const EResult result = renderShadowCasters(spotCam);
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

				if (!pass->GetAllPassTextures().empty())
				{
					RHISampler* defaultSampler = SamplerManager::Get().GetDefaultSampler();
					for (const auto& [slot, rtName] : pass->GetAllPassTextures())
					{
						if (rtName.empty()) continue;
						RenderTarget* sourceRT = RenderTargetManager::Get().GetRenderTarget(rtName);
						if (sourceRT && sourceRT->GetTexture())
							m_RHI->BindTextureSampler(sourceRT->GetTexture(), defaultSampler, slot);
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
					if (pass->GetSortType() == ERenderSortType::BackToFront && job.hasCamera)
						SortStaticBackToFront(meshIt->second, cameraBuffer.cameraPosition);

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

				if (const auto dynIt = m_DynamicGeometryDrawCommands.find(pass->GetID()); dynIt != m_DynamicGeometryDrawCommands.end())
				{
					const EResult result = RenderDynamicGeometry(dynIt->second, pass);
					if (IsFailure(result))
					{
						ENGINE_LOG_ERROR("Dynamic geometry rendering failed. Pass ID: {}", pass->GetID());
						return result;
					}
				}

				if (const auto images = m_UIImageDrawCommands.find(pass->GetID()); images != m_UIImageDrawCommands.end())
				{
					const EResult result = RenderUIImages(images->second, pass, rtWidth, rtHeight);
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
	RHIPipeline* lastPipeline = nullptr;

	bool lastMirrored = false;

	MaterialInterface* batchMaterial = nullptr;
	Mesh* batchMesh = nullptr;
	bool batchMirrored = false;
	vector<InstanceData> instanceBatch;
	instanceBatch.reserve(128);

	auto flushBatch = [&]() -> EResult
		{
			if (instanceBatch.empty() || !batchMesh || !batchMaterial)
				return EResult::Success;

			const bool sameMeshPipelineSignature = (batchMaterial == lastMaterial && lastMesh != nullptr && lastPipeline != nullptr &&
				batchMirrored == lastMirrored &&
				batchMesh->IsSkinned() == lastMesh->IsSkinned() &&
				batchMesh->GetTopology() == lastMesh->GetTopology() &&
				batchMesh->GetInputLayoutDescs() == lastMesh->GetInputLayoutDescs());

			if (!sameMeshPipelineSignature)
			{
				const auto bindResult = BindMeshPipeline(batchMesh, batchMaterial, passType, pipelineDesc, &lastPipeline, batchMirrored);
				if (bindResult != EResult::Success)
				{
					// [결함 격리]: 특정 머티리얼 오류로 전체 프레임 렌더링이 멈추지 않도록 해당 배치만 스킵
					instanceBatch.clear();
					return EResult::Success;
				}
				lastMirrored = batchMirrored;
			}
			if (batchMaterial != lastMaterial)
			{
				const bool needsMaterialBind = (passType != ERenderPassType::Shadow) ||
					UsesMaskedShadow(batchMesh, batchMaterial) ||
					batchMaterial->HasPassShader(ERenderPassType::Shadow);
				if (needsMaterialBind && batchMaterial->Bind(2) != EResult::Success)
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

			if (material != lastMaterial || mesh != lastMesh || command.mirrored != lastMirrored)
			{
				if (IsFailure(BindMeshPipeline(mesh, material, passType, pipelineDesc, &lastPipeline, command.mirrored)))
					return EResult::Fail;
				lastMirrored = command.mirrored;
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

		// 일반 정적 메시 배칭 플러시 판정:
		// 1) 메시가 달라진 경우 무조건 플러시
		// 2) 일반 패스(Geometry 등): 머티리얼이 달라지면 플러시
		// 3) 섀도우 패스: 깊이 전용 렌더링이므로 CullMode와 FrontFace가 일치하면 서로 다른 머티리얼이라도 단일 인스턴스 배치로 병합 허용
		//    단, 텍스처로 구멍을 내는 Masked 재질은 머티리얼이 다르면 병합하지 않음
		// 4) 동적 링 버퍼 오버플로 방지를 위한 최대 배치 인스턴스 수(1024) 도달 시 분할 플러시
		const bool isShadowPass = (passType == ERenderPassType::Shadow);
		bool needsFlush = false;

		if (batchMesh)
		{
			// 감긴 방향이 다르면 앞면 방향이 다른 파이프라인이 필요하다.
			if (mesh != batchMesh || command.mirrored != batchMirrored)
			{
				needsFlush = true;
			}
			else if (!isShadowPass)
			{
				if (material != batchMaterial)
					needsFlush = true;
			}
			else
			{
				if (batchMaterial && material)
				{
					if (material->GetCullMode() != batchMaterial->GetCullMode() ||
						material->GetFrontFace() != batchMaterial->GetFrontFace())
					{
						needsFlush = true;
					}
					else if (material != batchMaterial &&
						(UsesMaskedShadow(mesh, material) || UsesMaskedShadow(batchMesh, batchMaterial)))
					{
						needsFlush = true;
					}
				}
			}

			if (!needsFlush && instanceBatch.size() >= 1024)
			{
				needsFlush = true;
			}
		}

		if (needsFlush)
		{
			if (IsFailure(flushBatch()))
				return EResult::Fail;

			batchMaterial = material;
			batchMesh = mesh;
			batchMirrored = command.mirrored;
		}
		else if (!batchMesh)
		{
			batchMaterial = material;
			batchMesh = mesh;
			batchMirrored = command.mirrored;
		}

		InstanceData inst{};
		inst.worldMatrix = command.worldMatrix;
		inst.pickingData.x = static_cast<float>(command.pickingId);
		inst.pickingData.y = command.receiveDecals ? 0.0f : 1.0f;
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
	bool lastMirrored = false;
	static const vector<mat4> s_IdentityBones(256, glm::identity<mat4>());

	for (auto& command : commands)
	{
		if (!command.mesh || !command.material) continue;
		const bool mirrored = IsMirroredTransform(command.worldMatrix);
		if (command.material != lastMaterial || command.mesh != lastMesh || mirrored != lastMirrored)
		{
			if (IsFailure(BindMeshPipeline(command.mesh, command.material, passType, pipelineDesc, nullptr, mirrored)))
				return EResult::Fail;
			lastMirrored = mirrored;
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
		objectData.pickingData.y = command.receiveDecals ? 0.0f : 1.0f;
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

	MaterialInterface* batchMaterial = nullptr;
	Mesh* batchMesh = nullptr;
	Texture* batchTexture = nullptr;
	vector<SpriteInstanceData> instanceBatch;
	instanceBatch.reserve(128);

	auto flushBatch = [&]() -> EResult
		{
			if (instanceBatch.empty() || !batchMesh || !batchMaterial || !batchTexture)
				return EResult::Success;

			if (batchMaterial != lastMaterial || batchMesh != lastMesh)
			{
				const auto bindResult = BindMeshPipeline(batchMesh, batchMaterial, passType, pipelineDesc);
				if (bindResult != EResult::Success)
				{
					instanceBatch.clear();
					return EResult::Success;
				}
				lastMaterial = batchMaterial;
			}

			if (batchTexture != lastTexture)
			{
				RHITexture* rhiTexture = batchTexture->GetRHITexture();
				if (!rhiTexture || IsFailure(m_RHI->BindTextureSampler(rhiTexture, sampler, 0)))
				{
					instanceBatch.clear();
					return EResult::Fail;
				}
				lastTexture = batchTexture;
			}

			if (batchMesh != lastMesh)
			{
				if (IsFailure(batchMesh->Bind(0)))
				{
					instanceBatch.clear();
					return EResult::Fail;
				}
				lastMesh = batchMesh;
			}

			const uint32 instanceCount = static_cast<uint32>(instanceBatch.size());
			if (IsFailure(m_RHI->BindInstanceBuffer(instanceBatch.data(), instanceCount, sizeof(SpriteInstanceData))))
			{
				instanceBatch.clear();
				return EResult::Fail;
			}

			const EResult result = batchMesh->GetIndexBuffer()
				? m_RHI->DrawIndexedInstanced(batchMesh->GetIndexCount(), instanceCount)
				: m_RHI->DrawInstanced(batchMesh->GetVertexCount(), instanceCount);

			instanceBatch.clear();
			return result;
		};

	for (const auto& command : commands)
	{
		if (!command.mesh || !command.material || !command.texture)
			return EResult::Fail;

		RHITexture* texture = command.texture->GetRHITexture();
		if (!texture || !command.mesh->GetIndexBuffer())
			return EResult::Fail;

		// 머티리얼·메시·텍스처 상태가 변경되거나 최대 인스턴스 배치 수(1024) 도달 시 이전 배치 일괄 제출
		if (batchMesh && (command.material != batchMaterial || command.mesh != batchMesh ||
			command.texture != batchTexture || instanceBatch.size() >= 1024))
		{
			if (IsFailure(flushBatch()))
				return EResult::Fail;
		}

		batchMaterial = command.material;
		batchMesh = command.mesh;
		batchTexture = command.texture;

		SpriteInstanceData inst{};
		inst.worldMatrix = command.worldMatrix;
		inst.color = command.color;
		inst.uvTransform = command.uvTransform;
		inst.pickingData = vec4(static_cast<float>(command.pickingId), 0.0f, 0.0f, 0.0f);
		instanceBatch.push_back(inst);
	}

	return flushBatch();
}

EResult Renderer::RenderDynamicGeometry(const vector<DynamicGeometryDrawCommand>& commands, RenderPass* renderPass)
{
	if (!m_RHI || !renderPass) return EResult::Fail;
	if (commands.empty()) return EResult::Success;

	RHIPipelineDesc pipelineDesc;
	if (const EResult result = PreparePassPipelineDesc(renderPass, pipelineDesc); IsFailure(result)) return result;
	RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();
	if (!sampler) return EResult::Fail;

	// 메시와 같은 2스트림 레이아웃(슬롯 0: 위치, 슬롯 1: 재질 속성)을 써서 기존 스프라이트 VS를 그대로 사용한다.
	static const vector<InputLayoutDesc> s_DynamicLayouts = { VertexPosition::Layout, VertexMaterial::Layout };
	const ERenderPassType passType = renderPass->GetPassType();

	for (const auto& command : commands)
	{
		const uint32 vertexCount = static_cast<uint32>(command.positions.size());
		if (!command.material || !command.texture || vertexCount < 3 || command.attributes.size() != vertexCount) continue;
		RHITexture* rhiTexture = command.texture->GetRHITexture();
		if (!rhiTexture) continue;

		if (IsFailure(PrepareMeshPipelineDesc(nullptr, command.material, passType, pipelineDesc))) continue;
		pipelineDesc.inputLayouts = s_DynamicLayouts;
		pipelineDesc.topology = command.topology;
		auto* pipeline = PipelineManager::Get().GetOrCreatePipeline(pipelineDesc);
		if (!pipeline || IsFailure(m_RHI->BindPipeline(pipeline))) continue;

		// 동적 정점 버퍼를 지원하지 않는 백엔드는 조용히 건너뛴다. (호출 측 대체 경로는 3단계에서 처리)
		const EResult vbResult = m_RHI->BindDynamicVertexBuffer(0, command.positions.data(), vertexCount, sizeof(VertexPosition));
		if (vbResult == EResult::NotImplemented) return EResult::Success;
		if (IsFailure(vbResult) ||
			IsFailure(m_RHI->BindDynamicVertexBuffer(1, command.attributes.data(), vertexCount, sizeof(VertexMaterial))))
			return EResult::Fail;

		SpriteInstanceData instance{};
		instance.worldMatrix = glm::identity<mat4>();
		instance.color = command.color;
		instance.uvTransform = vec4(0.0f, 0.0f, 1.0f, 1.0f);
		instance.pickingData = vec4(static_cast<f32>(command.pickingId), 0.0f, 0.0f, 0.0f);

		if (IsFailure(m_RHI->BindTextureSampler(rhiTexture, sampler, 0)) ||
			IsFailure(m_RHI->BindInstanceBuffer(&instance, 1, sizeof(SpriteInstanceData))) ||
			IsFailure(m_RHI->DrawInstanced(vertexCount, 1)))
			return EResult::Fail;
	}
	return EResult::Success;
}

EResult Renderer::RenderUIImages(const vector<UIImageDrawCommand>& commands, RenderPass* renderPass, uint32 width, uint32 height)
{
	if (!m_RHI || !renderPass) return EResult::Fail;
	if (commands.empty()) return EResult::Success;
	struct AlphaMaskUBO
	{
		vec4 xPlane{ 0.0f };
		vec4 yPlane{ 0.0f };
		vec4 uvTransform{ 0.0f };
		vec4 uvClamp{ 0.0f };
	};
	struct ImageUBO
	{
		mat4 worldMatrix;
		vec4 uvRect;
		vec2 tiling;
		vec2 offset;
		vec4 color;
		UIEllipseMaskPlanes ellipseMaskPlanes;
		std::array<AlphaMaskUBO, MaxUIAlphaMasks> alphaMasks;
	};
	static_assert(sizeof(AlphaMaskUBO) == 64);
	static_assert(sizeof(ImageUBO) == 368);
	static_assert(offsetof(ImageUBO, color) == 96);
	static_assert(offsetof(ImageUBO, ellipseMaskPlanes) == 112);
	static_assert(offsetof(ImageUBO, alphaMasks) == 240);
	auto* sampler = SamplerManager::Get().GetDefaultSampler();
	if (!sampler) return EResult::Fail;
	RHIPipelineDesc pipelineDesc;
	const EResult prepareResult = PreparePassPipelineDesc(renderPass, pipelineDesc);
	if (IsFailure(prepareResult)) return prepareResult;

	const ERenderPassType passType = renderPass->GetPassType();
	auto defaultMaterialHandle = ResourceManager::Get().GetResourceHandle<MaterialInterface>(L"Engine://Material/UIMaterial.bammat.json");
	if (!defaultMaterialHandle.IsValid())
		defaultMaterialHandle = ResourceManager::Get().GetResourceHandle<MaterialInterface>(L"Resources/Material/UIMaterial.bammat.json");
	auto* defaultMaterial = defaultMaterialHandle.Get();
	auto* defaultPixelShader = defaultMaterial ? defaultMaterial->GetPixelShader(passType) : nullptr;
	MaterialInterface* lastMaterial = nullptr;
	bool bindAlphaTextures = false;
	vec2 lastCanvasSize(0.0f);
	const glm::uvec4 fullScissor(0, 0, width, height);
	glm::uvec4 lastScissor = fullScissor;
	for (const auto& command : commands)
	{
		Texture* texture = command.textureHandle.Get();
		MaterialInterface* material = command.alphaMaskCount > 0 ? defaultMaterial : command.materialHandle.Get();
		if (command.alphaMaskCount > MaxUIAlphaMasks || (command.alphaMaskCount > 0 && !defaultPixelShader)) continue;
		if (!texture || !material || !texture->GetRHITexture() ||
			command.canvasSize.x <= 0.0f || command.canvasSize.y <= 0.0f) return EResult::InvalidArgument;
		if (material != lastMaterial)
		{
			if (IsFailure(BindMeshPipeline(nullptr, material, passType, pipelineDesc)))
			{
				if (defaultMaterial && defaultMaterial != material &&
					IsSuccess(BindMeshPipeline(nullptr, defaultMaterial, passType, pipelineDesc)))
				{
					lastMaterial = defaultMaterial;
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
			bindAlphaTextures = defaultPixelShader && lastMaterial->GetPixelShader(passType) == defaultPixelShader;
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
		glm::uvec4 scissor = fullScissor;
		if (const auto& clip = command.clipRect)
		{
			if (!std::isfinite(clip->x) || !std::isfinite(clip->y) || !std::isfinite(clip->z) || !std::isfinite(clip->w))
				return EResult::InvalidArgument;
			const vec2 scale = vec2(width, height) / command.canvasSize;
			const auto toPixel = [](float edge, float pixelScale, uint32 limit)
			{
				// Use half-open bounds at pixel centers, matching the logical hit test.
				return static_cast<uint32>(glm::clamp(std::ceil(edge * pixelScale - 0.5f), 0.0f, static_cast<float>(limit)));
			};
			scissor = { toPixel(clip->x, scale.x, width), toPixel(clip->y, scale.y, height),
				toPixel(clip->z, scale.x, width), toPixel(clip->w, scale.y, height) };
			if (scissor.x >= scissor.z || scissor.y >= scissor.w) continue;
		}
		if (scissor != lastScissor)
		{
			if (IsFailure(m_RHI->SetScissorRect(scissor.x, scissor.y, scissor.z, scissor.w))) return EResult::Fail;
			lastScissor = scissor;
		}
		ImageUBO data{};
		data.worldMatrix = command.worldMatrix;
		data.uvRect = vec4(vec2(command.uvTransform),
			vec2(command.uvTransform) + vec2(command.uvTransform.z, command.uvTransform.w));
		data.tiling = vec2(1.0f);
		data.color = command.color;
		data.ellipseMaskPlanes.fill(vec4(0.0f));
		const auto maskPlanes = std::min(command.ellipseMaskCount, MaxUIEllipseMasks) * 2;
		std::copy_n(command.ellipseMaskPlanes.begin(), maskPlanes, data.ellipseMaskPlanes.begin());
		auto* baseTexture = texture->GetRHITexture();
		std::array<RHITexture*, MaxUIAlphaMasks> alphaTextures;
		alphaTextures.fill(baseTexture);
		bool validMasks = true;
		for (std::size_t i = 0; i < command.alphaMaskCount; ++i)
		{
			const auto& region = command.alphaMasks[i];
			const auto* mask = region.texture.Get();
			auto* maskTexture = mask ? mask->GetRHITexture() : nullptr;
			if (!maskTexture || maskTexture->GetWidth() == 0 || maskTexture->GetHeight() == 0)
			{
				validMasks = false;
				break;
			}
			alphaTextures[i] = maskTexture;
			const vec2 textureSize(maskTexture->GetWidth(), maskTexture->GetHeight());
			data.alphaMasks[i] = { region.xPlane, region.yPlane, region.uvTransform, UIAlphaMaskUVBounds(region.uvTransform, textureSize) };
		}
		if (!validMasks) continue;
		if (bindAlphaTextures)
		{
			// SDL declares all fragment descriptors, including both inactive mask slots.
			for (uint32 i = 0; i < alphaTextures.size(); ++i)
				if (IsFailure(m_RHI->BindTextureSampler(alphaTextures[i], sampler, 1 + i))) return EResult::Fail;
		}
		if (IsFailure(m_RHI->BindConstantBuffer(&data, sizeof(data), 1)) ||
			IsFailure(m_RHI->BindTextureSampler(baseTexture, sampler, 0)) ||
			IsFailure(m_RHI->Draw(6))) return EResult::Fail;
	}
	if (lastScissor != fullScissor && IsFailure(m_RHI->SetScissorRect(0, 0, width, height)))
		return EResult::Fail;
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

EResult Renderer::RenderMotionVectors(const MotionVectorInfo& info, MotionHistory& history, RenderPass* renderPass)
{
	if (!m_RHI || !renderPass) return EResult::InvalidArgument;

	static const vector<StaticDrawCommand> noStatic;
	static const vector<SkinnedDrawCommand> noSkinned;
	const auto staticIt = m_StaticDrawCommands.find(info.geometryPass);
	const auto skinnedIt = m_SkinnedDrawCommands.find(info.geometryPass);
	const auto& staticCommands = staticIt != m_StaticDrawCommands.end() ? staticIt->second : noStatic;
	const auto& skinnedCommands = skinnedIt != m_SkinnedDrawCommands.end() ? skinnedIt->second : noSkinned;

	// 스킨드와 정적 컴포넌트를 함께 가진 엔티티를 구분한다.
	constexpr uint64 skinnedKeyBit = 1ull << 63u;
	history.Begin();
	// 정점이 프레임마다 바뀔 수 있는 메시(동적 메시, 스플라인 변형 메시)는 형상 값을 함께 기록한다. 값이 바뀐 프레임은 이전 월드 행렬로
	// 이동량을 구할 수 없으므로 FindMoved가 돌려주지 않는다(정점이 매 프레임 바뀌는 메시는 늘 제외). 정점을 GeometryVersion을
	// 올리지 않고 바꾸는 메시는 알 수 없으므로 Mesh::SetStreamBuffer 같은 공개 API로만 바꿔야 한다.
	const auto getMotionStamp = [](const StaticDrawCommand& command)
		{
			uint64 stamp = command.motionStamp;
			if (command.mesh && (command.ownedMesh || command.mesh->IsDynamic()))
				stamp = CombineMotionStamp(stamp, MakeMotionStamp(command.mesh, command.mesh->GetGeometryVersion()));
			return stamp;
		};
	for (const auto& command : staticCommands)
	{
		if (command.objectId.IsValid())
			history.Record(command.objectId.GetKey() & ~skinnedKeyBit, command.worldMatrix, nullptr, getMotionStamp(command));
	}
	for (const auto& command : skinnedCommands)
	{
		if (command.objectId.IsValid()) history.Record(command.objectId.GetKey() | skinnedKeyBit, command.worldMatrix, command.boneMatrices);
	}
	history.End();

	const auto isReady = [](const Shader* shader) { return shader && shader->GetRHIShader(); };
	RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();
	if (!info.draw || !info.position || !sampler || !isReady(info.pixelShader))
		return EResult::Success;

	RHIPipelineDesc desc;
	if (IsFailure(PreparePassPipelineDesc(renderPass, desc))) return EResult::Fail;
	desc.pipelineType = EPipelineType::Graphics;
	desc.pixelShader = info.pixelShader->GetRHIShader();
	desc.cullMode = ECullMode::None;
	desc.depthStencilState.depthTestEnable = false;
	desc.depthStencilState.depthWriteEnable = false;

	// motionVector.hlsli의 MotionConstants (b1, space3)
	struct MotionConstants
	{
		mat4 jitteredViewProjection;
		mat4 viewProjection;
		mat4 previousViewProjection;
	};
	const MotionConstants constants{ info.jitteredViewProjection, info.viewProjection, info.previousViewProjection };

	RHIPipeline* lastPipeline = nullptr;
	const auto bindPipeline = [&](Shader* vertexShader, const Mesh& mesh, size_t layoutCount)
		{
			const auto& layouts = mesh.GetInputLayoutDescs();
			if (layouts.size() < layoutCount) return false;
			desc.vertexShader = vertexShader->GetRHIShader();
			desc.inputLayouts.assign(layouts.begin(), layouts.begin() + layoutCount);
			RHIPipeline* pipeline = PipelineManager::Get().GetOrCreatePipeline(desc);
			if (!pipeline) return false;
			if (pipeline == lastPipeline) return true;
			if (IsFailure(m_RHI->BindPipeline(pipeline)) || IsFailure(m_RHI->BindConstantBuffer(&constants, sizeof(constants), 3)) ||
				IsFailure(m_RHI->BindTextureSampler(info.position, sampler, MAX_MATERIAL_TEXTURE_SLOTS + 4)))
				return false;
			lastPipeline = pipeline;
			return true;
		};
	const auto draw = [&](Mesh& mesh, uint32 instanceCount)
		{
			return mesh.GetIndexBuffer() ? m_RHI->DrawIndexedInstanced(mesh.GetIndexCount(), instanceCount) : m_RHI->DrawInstanced(mesh.GetVertexCount(), instanceCount);
		};

	// 인스턴스 링 버퍼는 다른 패스와 함께 쓰므로 한도를 두고, 넘친 물체는 카메라 이동만 반영한다.
	// 정적 인스턴스(스플라인 배치 인스턴스 포함) 16384개, 스킨드 128개, 지형 16개까지 그린다.
	constexpr uint32 maxStaticInstances = MaxMotionStaticInstances;
	constexpr uint32 maxSkinnedDraws = MaxMotionSkinnedDraws;
	constexpr size_t maxBatch = 1024u;

	if (isReady(info.staticVertexShader))
	{
		struct MotionInstance
		{
			mat4 world;
			mat4 previousWorld;
		};
		vector<MotionInstance> batch;
		Mesh* batchMesh = nullptr;
		uint32 drawnInstances = 0;
		const auto flush = [&]()
			{
				if (batch.empty()) return true;
				const uint32 count = static_cast<uint32>(batch.size());
				const bool drawn = bindPipeline(info.staticVertexShader, *batchMesh, 1u) && IsSuccess(batchMesh->Bind(0)) &&
					IsSuccess(m_RHI->BindInstanceBuffer(batch.data(), count, sizeof(MotionInstance))) && IsSuccess(draw(*batchMesh, count));
				drawnInstances += count;
				batch.clear();
				return drawn;
			};

		const Frustum frustum = FrustumCuller::ExtractFrustum(info.jitteredViewProjection);
		for (const auto& command : staticCommands)
		{
			if (!command.mesh || command.heightfield || !command.objectId.IsValid()) continue;
			const auto* previous = history.FindMoved(command.objectId.GetKey() & ~skinnedKeyBit);
			if (!previous || command.mesh->IsSkinned() || command.mesh->GetTopology() != ETopology::TriangleList) continue;
			if (command.worldBounds && !FrustumCuller::TestBoundingBox(frustum, *command.worldBounds)) continue;
			if (drawnInstances + batch.size() >= maxStaticInstances) break;

			if (command.mesh != batchMesh || batch.size() >= maxBatch)
			{
				if (!flush()) return EResult::Fail;
				batchMesh = command.mesh;
			}
			batch.push_back({ command.worldMatrix, previous->World });
		}
		if (!flush()) return EResult::Fail;
	}

	// 지형. 높이맵으로 정점을 옮기므로 별도 정점 셰이더로 그리며, 높이 배열이 아니라 월드 행렬만 움직인 경우를 다룬다.
	// 높이맵이 바뀐 프레임은 지형이 제출되지 않거나 형상 값이 달라 FindMoved에서 빠진다.
	if (isReady(info.terrainVertexShader))
	{
		constexpr uint32 maxTerrainDraws = MaxMotionTerrainDraws;
		struct TerrainMotionObject
		{
			mat4 world;
			mat4 previousWorld;
			vec4 heightfieldData; // x 최대 높이, yz 칸 간격 (terrainVS의 HeightfieldData와 같다)
		};
		uint32 drawCount = 0;
		for (const auto& command : staticCommands)
		{
			if (!command.mesh || !command.heightfield || !command.objectId.IsValid()) continue;
			const auto* previous = history.FindMoved(command.objectId.GetKey() & ~skinnedKeyBit);
			const auto& heightfield = *command.heightfield;
			if (!previous || !heightfield.heightmap || command.mesh->GetTopology() != ETopology::TriangleList) continue;
			if (drawCount >= maxTerrainDraws) break;

			if (!bindPipeline(info.terrainVertexShader, *command.mesh, 1u)) continue;
			const TerrainMotionObject object{ command.worldMatrix, previous->World, vec4(heightfield.maxHeight, heightfield.cellSpacing.x, heightfield.cellSpacing.y, 0.0f) };
			// 높이맵은 지형 G-Buffer 셰이더와 같은 슬롯(t1, space1)에 묶는다.
			if (IsFailure(m_RHI->BindTextureSampler(heightfield.heightmap.get(), sampler, 1)) ||
				IsFailure(m_RHI->BindConstantBuffer(&object, sizeof(object), 1)) || IsFailure(command.mesh->Bind(0)) ||
				IsFailure(command.mesh->GetIndexBuffer() ? m_RHI->DrawIndexed(command.mesh->GetIndexCount()) : m_RHI->Draw(command.mesh->GetVertexCount())))
				return EResult::Fail;
			++drawCount;
		}
	}

	if (isReady(info.skinnedVertexShader))
	{
		constexpr size_t boneCount = 256u; // motionVectorSkinningVS의 BONE_COUNT
		struct MotionObject
		{
			mat4 world;
			mat4 previousWorld;
		};
		vector<mat4> bones(boneCount * 2u, mat4(1.0f));
		const auto copyBones = [&](const vector<mat4>* source, size_t offset)
			{
				const size_t count = source ? std::min(source->size(), boneCount) : 0u;
				if (count > 0u) std::copy_n(source->begin(), count, bones.begin() + offset);
				std::fill(bones.begin() + offset + count, bones.begin() + offset + boneCount, mat4(1.0f));
			};

		uint32 drawCount = 0;
		for (const auto& command : skinnedCommands)
		{
			if (!command.mesh || !command.objectId.IsValid()) continue;
			const auto* previous = history.FindMoved(command.objectId.GetKey() | skinnedKeyBit);
			if (!previous || command.mesh->IsDynamic() || !command.mesh->IsSkinned() || command.mesh->GetTopology() != ETopology::TriangleList) continue;
			if (drawCount >= maxSkinnedDraws) break;

			if (!bindPipeline(info.skinnedVertexShader, *command.mesh, 3u)) continue;
			copyBones(command.boneMatrices, 0u);
			copyBones(&previous->Bones, boneCount);
			const MotionObject object{ command.worldMatrix, previous->World };
			if (IsFailure(m_RHI->BindConstantBuffer(&object, sizeof(object), 1)) || IsFailure(command.mesh->Bind(0)) ||
				IsFailure(m_RHI->BindInstanceBuffer(bones.data(), static_cast<uint32>(bones.size()), sizeof(mat4))) || IsFailure(draw(*command.mesh, 1u)))
				return EResult::Fail;
			++drawCount;
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
	for (auto& [passID, queue] : m_DynamicGeometryDrawCommands) queue.clear();
	for (auto& [passID, queue] : m_UIImageDrawCommands) queue.clear();
	for (auto& [passID, jobs] : m_ViewportJobsPerPass) jobs.clear();

	m_ViewportCameras.clear();
	if (RaytracingSceneManager::IsCreated())
		RaytracingSceneManager::Get().ClearRequests();
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
	m_DynamicGeometryDrawCommands.erase(passID);
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
	m_DynamicGeometryDrawCommands.clear();
	m_UIImageDrawCommands.clear();
	m_ViewportJobsPerPass.clear();
	if (RaytracingSceneManager::IsCreated())
		RaytracingSceneManager::Get().ClearRequests();
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
	auto& submitted = m_StaticDrawCommands[passID].emplace_back(command);
	submitted.mirrored = IsMirroredTransform(submitted.worldMatrix);
}
void Renderer::SubmitSkinned(const SkinnedDrawCommand& command, RenderPassID passID)
{
	m_SkinnedDrawCommands[passID].push_back(command);
}
void Renderer::SubmitSprite(const SpriteDrawCommand& command, RenderPassID passID)
{
	m_SpriteDrawCommands[passID].push_back(command);
}
void Renderer::SubmitDynamicGeometry(DynamicGeometryDrawCommand command, RenderPassID passID)
{
	m_DynamicGeometryDrawCommands[passID].push_back(std::move(command));
}
void Renderer::SubmitUIImage(const UIImageDrawCommand& command, RenderPassID passID)
{
	m_UIImageDrawCommands[passID].push_back(command);
}
#pragma endregion

#pragma region Viewport Camera Management

void Renderer::RegisterViewportCamera(const CameraBuffer& cameraBuffer, RenderPass* renderPass, bool acceptsSceneSubmissions, RenderPassID viewID)
{
	if (!renderPass) return;
	ViewportCameraInfo viewport{ cameraBuffer, true, renderPass };
	viewport.acceptsSceneSubmissions = acceptsSceneSubmissions;
	if (viewID == INVALID_PASS_ID && renderPass->GetPassType() == ERenderPassType::Geometry)
		viewID = renderPass->GetID();
	viewport.viewID = viewID;
	viewport.lodCamera = cameraBuffer;
	viewport.hasLODCamera = true;
	m_ViewportCameras.push_back(viewport);
}

void Renderer::RegisterShadowPassForView(RenderPass* renderPass, const CameraBuffer& viewCamera, RenderPassID viewID)
{
	if (!renderPass) return;
	ViewportCameraInfo viewport{ {}, false, renderPass };
	viewport.viewID = viewID;
	viewport.lodCamera = viewCamera;
	viewport.hasLODCamera = true;
	m_ViewportCameras.push_back(viewport);
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
		if (!pass || !viewport.hasCamera || !viewport.acceptsSceneSubmissions || pass->GetPassType() != ERenderPassType::Geometry) continue;

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

EResult Renderer::PrepareMeshPipelineDesc(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc, bool mirrored)
{
	if (!m_RHI || !material) return EResult::InvalidArgument;
	Shader* vs = ResolveVertexShader(mesh, material, passType);
	Shader* ps = material->GetPixelShader(passType);

	// 타 패스(Shadow 등)에 머티리얼 전용 오버라이드가 없어 nullptr를 반환한 경우, 렌더러 기본 섀도우 셰이더로 안전 폴백
	if (passType == ERenderPassType::Shadow && (!vs || !ps))
	{
		EnsureDefaultShaders();
		const bool isSkinned = mesh && mesh->IsSkinned();
		if (!vs) vs = isSkinned ? m_DefaultShadowDepthSkinningVS.Get() : m_DefaultShadowDepthVS.Get();
		if (!ps) ps = m_DefaultShadowDepthPS.Get();
	}
	if (passType == ERenderPassType::Shadow && UsesMaskedShadow(mesh, material))
	{
		vs = mesh->IsSkinned() ? m_MaskedShadowDepthSkinningVS.Get() : m_MaskedShadowDepthVS.Get();
		ps = m_MaskedShadowDepthPS.Get();
	}

	if (!vs || !ps || !vs->GetRHIShader() || !ps->GetRHIShader()) return EResult::InvalidArgument;
	if (vs->GetShaderType() != EShaderType::Vertex || ps->GetShaderType() != EShaderType::Fragment)
	{
		static std::unordered_set<std::string> s_LoggedTypeMismatches;
		const std::string errKey = fmt::format("{}_{}", vs ? WStrToStr(vs->GetKey()) : "null", ps ? WStrToStr(ps->GetKey()) : "null");
		if (s_LoggedTypeMismatches.insert(errKey).second)
		{
			BAM_LOG(Error, "Renderer", "PrepareMeshPipelineDesc",
				"Shader stage type mismatch! VS '{}' (type={}) must be Vertex, PS '{}' (type={}) must be Fragment. Skipping pipeline creation.",
				vs ? WStrToStr(vs->GetKey()) : "null", static_cast<int>(vs ? vs->GetShaderType() : EShaderType::Unknown),
				ps ? WStrToStr(ps->GetKey()) : "null", static_cast<int>(ps ? ps->GetShaderType() : EShaderType::Unknown));
		}
		return EResult::InvalidArgument;
	}

	// ── VS와 PS의 시그니처 링키지(호환성) 검증 및 자동 보정 ──
	const std::string vsKeyStr = ToLowerStr(WStrToStr(vs->GetKey()));
	const std::string psKeyStr = ToLowerStr(WStrToStr(ps->GetKey()));

	const bool isGBufferPS = (psKeyStr.find("gbuffer") != std::string::npos ||
	                          psKeyStr.find("graphmaterial") != std::string::npos ||
	                          psKeyStr.find("livepixelshader") != std::string::npos ||
	                          psKeyStr.find("materialoutput") != std::string::npos);
	const bool isGBufferVS = (vsKeyStr.find("gbuffer") != std::string::npos ||
	                          vsKeyStr.find("skinning") != std::string::npos);

	// GBuffer 픽셀 셰이더인데 정점 셰이더가 GBuffer용(WorldPos, PickingId 출력)이 아닌 경우 (예: DefaultVS)
	if (isGBufferPS && !isGBufferVS)
	{
		EnsureDefaultShaders();
		const bool isSkinned = mesh && mesh->IsSkinned();
		Shader* fallbackVS = isSkinned ? m_DefaultGBufferSkinningVS.Get() : m_DefaultGBufferVS.Get();
		if (fallbackVS && fallbackVS->GetRHIShader())
		{
			static std::unordered_set<std::string> s_LoggedFixes;
			if (s_LoggedFixes.insert(vsKeyStr + "->" + psKeyStr).second)
			{
				BAM_LOG(Warn, "Renderer", "PrepareMeshPipelineDesc",
					"Incompatible shader pair: VS '{}' lacks required outputs for GBuffer PS '{}'. Automatically substituting with GBufferVS.",
					WStrToStr(vs->GetKey()), WStrToStr(ps->GetKey()));
			}
			vs = fallbackVS;
		}
		else
		{
			return EResult::InvalidArgument;
		}
	}
	// 포워드/레거시 픽셀 셰이더(DefaultPS)인데 정점 셰이더가 GBufferVS인 경우 (BINORMAL 부재 등)
	else if (!isGBufferPS && isGBufferVS && (psKeyStr.find("defaultps") != std::string::npos))
	{
		EnsureDefaultShaders();
		Shader* fallbackVS = m_DefaultVS.Get();
		if (fallbackVS && fallbackVS->GetRHIShader())
		{
			vs = fallbackVS;
		}
		else
		{
			return EResult::InvalidArgument;
		}
	}
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
	desc.frontFace = ResolveFrontFace(material->GetFrontFace(), mirrored);
	desc.cullMode = material->GetCullMode();
	desc.fillMode = material->GetFillMode();
	desc.blendState = material->GetBlendState();
	const bool hasDepthStencil = desc.depthStencilAttachmentFormat != ETextureFormat::UNKNOWN;
	desc.depthStencilState.depthTestEnable = hasDepthStencil && material->GetDepthMode() != EDepthMode::None;
	desc.depthStencilState.depthWriteEnable = hasDepthStencil && material->GetDepthMode() == EDepthMode::ReadWrite;
	desc.depthStencilState.depthCompareOp = material->GetDepthCompareOp();
	return EResult::Success;
}

EResult Renderer::BindMeshPipeline(Mesh* mesh, MaterialInterface* material, ERenderPassType passType, RHIPipelineDesc& desc, RHIPipeline** inOutLastPipeline, bool mirrored)
{
	const auto result = PrepareMeshPipelineDesc(mesh, material, passType, desc, mirrored);
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
	if (inOutLastPipeline && *inOutLastPipeline == pipeline)
		return EResult::Success;
	const EResult bindResult = m_RHI->BindPipeline(pipeline);
	if (IsSuccess(bindResult) && inOutLastPipeline)
		*inOutLastPipeline = pipeline;
	return bindResult;
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
	m_DefaultGBufferPS = getOrLoad(L"Resources/Shader/GBufferPS");
	m_DefaultVS = getOrLoad(L"Resources/Shader/DefaultVS");
	m_DefaultShadowDepthVS = getOrLoad(L"Resources/Shader/ShadowDepthVS");
	m_DefaultShadowDepthSkinningVS = getOrLoad(L"Resources/Shader/ShadowDepthSkinningVS");
	m_DefaultShadowDepthPS = getOrLoad(L"Resources/Shader/ShadowDepthPS");

	// 시작 매니페스트와 재질 에셋이 참조하지 않는 셰이더라 바이너리 경로로 직접 등록한다.
	const auto binRoot = PlatformPath::GetEngineResourceRoot() / "Shader" / "bin";
	const auto getOrRegister = [&](const wchar_t* key, const std::wstring& stem, EShaderType type) -> ResourceHandle<Shader>
		{
			auto handle = rm.GetResourceHandle<Shader>(key);
			if (handle.IsValid() && handle.Get()) return handle;
			ShaderDesc desc{};
			desc.Key = key;
			desc.dxilPath = (binRoot / "dxil" / (stem + L".cso")).wstring();
			desc.spirvPath = (binRoot / "spirv" / (stem + L".spv")).wstring();
			desc.wgslPath = (binRoot / "wgsl" / (stem + L".wgsl")).wstring();
			desc.Path = m_RHIType == ERHIType::DirectX12 ? desc.dxilPath :
				(m_RHIType == ERHIType::WebGPU ? desc.wgslPath : desc.spirvPath);
			desc.shaderType = type;
			return rm.LoadResource<Shader>(&desc);
		};
	m_MaskedShadowDepthVS = getOrRegister(L"Resources/Shader/ShadowDepthMaskedVS", L"shadowDepthMaskedVS", EShaderType::Vertex);
	m_MaskedShadowDepthSkinningVS = getOrRegister(L"Resources/Shader/ShadowDepthMaskedSkinningVS", L"shadowDepthMaskedSkinningVS", EShaderType::Vertex);
	m_MaskedShadowDepthPS = getOrRegister(L"Resources/Shader/ShadowDepthMaskedPS", L"shadowDepthMaskedPS", EShaderType::Fragment);
}
bool Renderer::UsesMaskedShadow(Mesh* mesh, MaterialInterface* material)
{
	if (!mesh || !material || material->GetBlendMode() != EBlendMode::Masked || !mesh->GetMaterialBuffer())
		return false;
	EnsureDefaultShaders();
	const auto matches = [](const Shader* shader, const Shader* defaultShader)
		{
			return shader && defaultShader && (shader == defaultShader || shader->GetKey() == defaultShader->GetKey());
		};
	// 셰이더 그래프처럼 알파 식이 다른 픽셀 셰이더나 전용 그림자 셰이더를 쓰는 재질은 기존 경로를 유지한다.
	if (!matches(material->GetPixelShader(ERenderPassType::Geometry), m_DefaultGBufferPS.Get()))
		return false;
	const Shader* shadowVS = material->GetVertexShader(ERenderPassType::Shadow);
	const Shader* shadowPS = material->GetPixelShader(ERenderPassType::Shadow);
	if (shadowVS && !matches(shadowVS, m_DefaultShadowDepthVS.Get()) && !matches(shadowVS, m_DefaultShadowDepthSkinningVS.Get()))
		return false;
	if (shadowPS && !matches(shadowPS, m_DefaultShadowDepthPS.Get()))
		return false;
	const Shader* maskedVS = mesh->IsSkinned() ? m_MaskedShadowDepthSkinningVS.Get() : m_MaskedShadowDepthVS.Get();
	const Shader* maskedPS = m_MaskedShadowDepthPS.Get();
	return maskedVS && maskedVS->GetRHIShader() && maskedPS && maskedPS->GetRHIShader();
}
Shader* Renderer::GetDefaultShadowDepthVertexShader()
{
	EnsureDefaultShaders();
	return m_DefaultShadowDepthVS.Get();
}
Shader* Renderer::GetDefaultShadowDepthPixelShader()
{
	EnsureDefaultShaders();
	return m_DefaultShadowDepthPS.Get();
}
Shader* Renderer::GetDefaultShadowDepthSkinningVertexShader()
{
	EnsureDefaultShaders();
	return m_DefaultShadowDepthSkinningVS.Get();
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
