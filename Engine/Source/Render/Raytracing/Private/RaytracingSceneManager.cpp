#include "RaytracingSceneManager.h"
#include "RHI.h"
#include "RHIBuffer.h"
#include "Scene.h"
#include "ComponentEnable.h"
#include "WorldComponents.h"
#include "RenderComponents.h"
#include "SplineMeshComponent.h"
#include "Mesh.h"
#include "ResidentMeshLODs.h"
#include "MaterialInterface.h"
#include "Renderer.h"
#include "Shader.h"
#include "Logger.h"

IMPLEMENT_SINGLETON(RaytracingSceneManager)

namespace
{
	// 이 프레임 수 동안 어느 씬에서도 쓰이지 않은 BLAS는 해제한다(메시 해제·LOD 전환 후 남은 항목 정리).
	constexpr Engine::uint64 BottomLevelRetentionFrames = 600;
	// 이 프레임 수 동안 요청되지 않은 씬의 TLAS는 해제한다. 프레임 제한된 뷰포트가 건너뛴 프레임에는 버퍼를 유지한다.
	constexpr Engine::uint64 TopLevelRetentionFrames = 120;
}

#pragma region Constructor&Destructor
EResult RaytracingSceneManager::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	if (!desc->RHI) return EResult::InvalidArgument;
	m_RHI = desc->RHI;
	Safe_AddRef(m_RHI);
	return EResult::Success;
}

void RaytracingSceneManager::Free()
{
	for (auto& [scene, entry] : m_TopLevels)
		Safe_Release(entry.AccelerationStructure);
	m_TopLevels.clear();

	for (auto& [key, entry] : m_BottomLevels)
		ReleaseBottomLevel(key, entry);
	m_BottomLevels.clear();

	m_RequestedScenes.clear();
	Safe_Release(m_RHI);
	__super::Free();
}
#pragma endregion

#pragma region Frame
bool RaytracingSceneManager::IsSupported() const
{
	return m_RHI && m_RHI->GetCapabilities().SupportsInlineRaytracing;
}

void RaytracingSceneManager::RequestScene(Scene* scene, uint32 viewID)
{
	if (!scene || !IsSupported())
		return;
	if (!IsSceneRequested(scene, viewID))
		m_RequestedScenes.push_back({ scene, viewID });
}

bool RaytracingSceneManager::IsSceneRequested(const Scene* scene, uint32 viewID) const
{
	return std::any_of(m_RequestedScenes.begin(), m_RequestedScenes.end(),
		[scene, viewID](const TopLevelRequest& request) { return request.Source == scene && request.ViewID == viewID; });
}

EResult RaytracingSceneManager::BuildRequestedScenes()
{
	++m_FrameCounter;
	m_BuildBudget.Reset();
	Stats stats{};

	for (auto& [key, entry] : m_TopLevels)
		entry.BuiltThisFrame = false;

	EResult frameResult = EResult::Success;
	if (IsSupported())
	{
		for (const TopLevelRequest& request : m_RequestedScenes)
		{
			TopLevelEntry& entry = m_TopLevels[TopLevelKey{ request.Source, request.ViewID }];
			entry.LastRequestedFrame = m_FrameCounter;
			const EResult result = BuildScene(*request.Source, request.ViewID, entry, stats);
			entry.BuiltThisFrame = IsSuccess(result);
			if (IsFailure(result))
			{
				frameResult = result;
				ENGINE_LOG_ERROR("Failed to build the raytracing top-level acceleration structure for a scene.");
			}
		}
	}
	const bool builtAnyScene = !m_RequestedScenes.empty();
	m_RequestedScenes.clear();

	// 뷰포트는 프레임 제한 등으로 매 프레임 제출하지 않을 수 있다. 버퍼를 매번 다시 만들지 않도록
	// 일정 기간 요청이 없을 때만(닫힌 프리뷰, 해제된 씬) TLAS를 지연 해제한다.
	for (auto it = m_TopLevels.begin(); it != m_TopLevels.end();)
	{
		if (m_FrameCounter - it->second.LastRequestedFrame <= TopLevelRetentionFrames)
		{
			// 프레임 제한으로 이번 프레임을 건너뛴 뷰의 값도 합쳐 통계와 로그가 프레임마다 오가지 않게 한다.
			const TopLevelEntry& entry = it->second;
			if (entry.AccelerationStructure)
			{
				stats.TopLevelBytes += entry.AccelerationStructure->GetResultSizeInBytes();
				++stats.TopLevelCount;
			}
			stats.InstanceCount += entry.InstanceCount;
			stats.UnsupportedCasterCount += entry.UnsupportedCasterCount;
			stats.PendingCasterCount += entry.PendingCasterCount;
			stats.FailedCasterCount += entry.FailedCasterCount;
			++it;
			continue;
		}
		Safe_Release(it->second.AccelerationStructure);
		it = m_TopLevels.erase(it);
	}

	PruneBottomLevels();
	for (const auto& [key, entry] : m_BottomLevels)
	{
		if (!entry.AccelerationStructure) continue;
		++stats.BottomLevelCount;
		stats.BottomLevelBytes += entry.AccelerationStructure->GetResultSizeInBytes();
	}

	// 빌드한 프레임의 통계만 보관한다. 새 BLAS가 생기거나 수집 결과가 바뀐 경우에만 기록해 로그를 반복하지 않는다.
	// 오래 빌드하지 않았으면(레이트레이싱 꺼짐) 지난 통계를 현재 값처럼 보이지 않게 비운다.
	if (!builtAnyScene)
	{
		if (m_FrameCounter - m_LastBuildFrame > TopLevelRetentionFrames)
			m_LastStats = {};
		return frameResult;
	}
	m_LastBuildFrame = m_FrameCounter;

	// 예산을 나눠 쓰는 동안에는 시작과 끝만 기록한다.
	const bool pending = stats.PendingCasterCount > 0;
	const bool pendingChanged = pending != (m_LastStats.PendingCasterCount > 0);
	const bool resultChanged = stats.BottomLevelBuiltThisFrame > 0 || stats.TopLevelCount != m_LastStats.TopLevelCount ||
		stats.InstanceCount != m_LastStats.InstanceCount ||
		stats.UnsupportedCasterCount != m_LastStats.UnsupportedCasterCount || stats.FailedCasterCount != m_LastStats.FailedCasterCount;
	if (pendingChanged || (!pending && resultChanged))
	{
		ENGINE_LOG_INFO("Raytracing AS: {} TLAS, {} instance(s), {} BLAS ({} new, {:.2f} MB), TLAS {:.2f} MB, {} caster(s) left to shadow maps, {} pending, {} failed.",
			stats.TopLevelCount, stats.InstanceCount, stats.BottomLevelCount, stats.BottomLevelBuiltThisFrame,
			static_cast<double>(stats.BottomLevelBytes) / (1024.0 * 1024.0), static_cast<double>(stats.TopLevelBytes) / (1024.0 * 1024.0),
			stats.UnsupportedCasterCount, stats.PendingCasterCount, stats.FailedCasterCount);
	}

	m_LastStats = stats;
	return frameResult;
}

RHIAccelerationStructure* RaytracingSceneManager::GetTopLevelAS(const Scene* scene, uint32 viewID) const
{
	const auto it = m_TopLevels.find(TopLevelKey{ scene, viewID });
	if (it == m_TopLevels.end() || !it->second.BuiltThisFrame)
		return nullptr;
	return it->second.AccelerationStructure;
}

const RaytracedCasterSet* RaytracingSceneManager::GetTopLevelCasters(const Scene* scene, uint32 viewID) const
{
	const auto it = m_TopLevels.find(TopLevelKey{ scene, viewID });
	if (it == m_TopLevels.end() || !it->second.BuiltThisFrame || !it->second.AccelerationStructure)
		return nullptr;
	return &it->second.Casters;
}

EResult RaytracingSceneManager::BuildScene(Scene& scene, uint32 viewID, TopLevelEntry& entry, Stats& stats)
{
	auto& world = scene.GetRegistry();
	m_InstanceScratch.clear();
	entry.Casters.Begin();
	entry.InstanceCount = 0;
	entry.UnsupportedCasterCount = 0;
	entry.PendingCasterCount = 0;
	entry.FailedCasterCount = 0;

	auto view = world.view<StaticMeshRendererComponent, WorldTransformComponent, FlagComponent>();
	for (auto [entity, meshRenderer, worldTransform, flag] : view.each())
	{
		// 래스터 섀도 패스(MeshRenderSystem)와 같은 가시성·캐스트 조건을 사용한다.
		if (!IsComponentEnabled<StaticMeshRendererComponent>(world, entity)) continue;
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
			continue;
		if (!meshRenderer.castShadow) continue;

		Mesh* mesh = meshRenderer.meshHandle.Get();
		MaterialInterface* material = meshRenderer.materialHandle.Get();
		if (!mesh || !material) continue;

		// 스플라인 배치·변형 메시는 프레임마다 형상이 달라질 수 있어 섀도맵에 맡긴다.
		if (TryGetEnabledComponent<SplineMeshComponent>(world, entity))
		{
			++entry.UnsupportedCasterCount;
			continue;
		}

		// 이 뷰의 GBuffer가 고른 LOD와 같은 형상으로 차폐해 자기 그림자 오차를 막는다.
		const auto* lodGroup = TryGetEnabledComponent<LODGroupComponent>(world, entity);
		mesh = ResidentMeshLODs(mesh, lodGroup).Find(lodGroup ? lodGroup->viewLODs.Get(viewID) : 0);
		if (!mesh)
		{
			++entry.PendingCasterCount;
			continue;
		}

		if (!IsShadowCasterSupported(mesh, material))
		{
			++entry.UnsupportedCasterCount;
			continue;
		}

		// TLAS에 넣지 못한 캐스터는 Casters에 없으므로 섀도맵에 남는다.
		RHIAccelerationStructure* bottomLevel = nullptr;
		const EBottomLevelState state = GetOrBuildBottomLevel(*mesh, stats, bottomLevel);
		if (state != EBottomLevelState::Ready)
		{
			++(state == EBottomLevelState::Pending ? entry.PendingCasterCount : entry.FailedCasterCount);
			continue;
		}

		RHIRaytracingInstanceDesc instance;
		instance.worldMatrix = worldTransform.worldMatrix;
		instance.bottomLevel = bottomLevel;
		instance.instanceID = static_cast<uint32>(entt::to_integral(entity));
		m_InstanceScratch.push_back(instance);
		entry.Casters.Add(static_cast<uint32>(entt::to_entity(entity)));
	}

	// 스킨드 캐스터는 1차 범위에서 섀도맵이 담당한다(통계만 집계).
	auto skinnedView = world.view<SkinnedMeshRendererComponent, FlagComponent>();
	for (auto [entity, skinnedRenderer, flag] : skinnedView.each())
	{
		if (!IsComponentEnabled<SkinnedMeshRendererComponent>(world, entity)) continue;
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
			continue;
		if (skinnedRenderer.castShadow)
			++entry.UnsupportedCasterCount;
	}

	const EResult result = m_RHI->BuildTopLevelAS(entry.AccelerationStructure, m_InstanceScratch.data(),
		static_cast<uint32>(m_InstanceScratch.size()));
	if (IsSuccess(result))
		entry.InstanceCount = static_cast<uint32>(m_InstanceScratch.size());
	return result;
}
#pragma endregion

#pragma region Bottom Level
size_t RaytracingSceneManager::BottomLevelKeyHash::operator()(const BottomLevelKey& key) const
{
	size_t seed = std::hash<const void*>()(key.PositionBuffer);
	const auto combine = [&seed](size_t value) { seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2); };
	combine(std::hash<const void*>()(key.IndexBuffer));
	combine(std::hash<uint32>()(key.VertexCount));
	combine(std::hash<uint32>()(key.IndexCount));
	combine(std::hash<uint64>()(key.GeometryVersion));
	return seed;
}

RaytracingSceneManager::EBottomLevelState RaytracingSceneManager::GetOrBuildBottomLevel(Mesh& mesh, Stats& stats,
	RHIAccelerationStructure*& bottomLevel)
{
	bottomLevel = nullptr;
	BottomLevelKey key;
	key.PositionBuffer = mesh.GetPositionBuffer();
	key.IndexBuffer = mesh.GetIndexBuffer();
	key.VertexCount = mesh.GetVertexCount();
	key.IndexCount = mesh.GetIndexBuffer() ? mesh.GetIndexCount() : 0;
	key.GeometryVersion = mesh.GetGeometryVersion();

	if (const auto it = m_BottomLevels.find(key); it != m_BottomLevels.end())
	{
		it->second.LastUsedFrame = m_FrameCounter;
		bottomLevel = it->second.AccelerationStructure;
		return bottomLevel ? EBottomLevelState::Ready : EBottomLevelState::Failed;
	}

	const uint64 triangleCount = (key.IndexBuffer ? key.IndexCount : key.VertexCount) / 3;
	if (!m_BuildBudget.TryConsume(triangleCount))
		return EBottomLevelState::Pending;

	RHIRaytracingGeometryDesc geometry;
	geometry.positionBuffer = key.PositionBuffer;
	geometry.vertexCount = key.VertexCount;
	geometry.vertexStride = mesh.GetStreamStride(EMeshStream::Position);
	geometry.indexBuffer = key.IndexBuffer;
	geometry.indexCount = key.IndexCount;
	geometry.indexStride = mesh.GetIndexStride();

	// 실패한 기하도 항목으로 남겨 같은 메시를 매 프레임 재시도하거나 로그를 반복하지 않는다(보존 기간이 지나면 정리).
	bottomLevel = m_RHI->CreateBottomLevelAS(geometry);
	if (!bottomLevel)
	{
		ENGINE_LOG_WARN("Failed to build a BLAS for mesh '{}' ({} vertices, {} indices). It stays in shadow maps.",
			WStrToStr(mesh.GetKey()), key.VertexCount, key.IndexCount);
	}
	else
	{
		++stats.BottomLevelBuiltThisFrame;
	}

	Safe_AddRef(key.PositionBuffer);
	Safe_AddRef(key.IndexBuffer);
	m_BottomLevels.emplace(key, BottomLevelEntry{ bottomLevel, m_FrameCounter });
	return bottomLevel ? EBottomLevelState::Ready : EBottomLevelState::Failed;
}

void RaytracingSceneManager::ReleaseBottomLevel(const BottomLevelKey& key, BottomLevelEntry& entry)
{
	Safe_Release(entry.AccelerationStructure);
	RHIBuffer* positionBuffer = key.PositionBuffer;
	RHIBuffer* indexBuffer = key.IndexBuffer;
	Safe_Release(positionBuffer);
	Safe_Release(indexBuffer);
}

void RaytracingSceneManager::PruneBottomLevels()
{
	for (auto it = m_BottomLevels.begin(); it != m_BottomLevels.end();)
	{
		if (m_FrameCounter - it->second.LastUsedFrame <= BottomLevelRetentionFrames)
		{
			++it;
			continue;
		}
		ReleaseBottomLevel(it->first, it->second);
		it = m_BottomLevels.erase(it);
	}
}
#pragma endregion

#pragma region Caster Policy
bool RaytracingSceneManager::IsShadowCasterSupported(const Mesh* mesh, const MaterialInterface* material)
{
	if (!mesh || !material)
		return false;
	if (mesh->IsDynamic() || mesh->IsSkinned() || mesh->GetTopology() != ETopology::TriangleList)
		return false;
	if (!mesh->GetPositionBuffer() || mesh->GetVertexCount() == 0)
		return false;
	// 불투명 재질만 지원한다. Masked는 알파 테스트가 필요해 섀도맵이 담당한다.
	if (material->GetBlendMode() != EBlendMode::Opaque)
		return false;
	// 그림자 패스에 엔진 기본(ShadowDepthVS/PS)이 아닌 셰이더를 쓰는 재질은 형상을 바꾸거나(WPO 등)
	// 픽셀을 버릴 수 있으므로 섀도맵이 담당한다. 기본 재질은 기본 그림자 셰이더를 명시적으로 바인딩한다.
	if (!Renderer::IsCreated())
		return false;
	Renderer& renderer = Renderer::Get();
	const auto isDefault = [](const Shader* shader, const Shader* defaultShader)
		{
			return !shader || shader == defaultShader || (defaultShader && shader->GetKey() == defaultShader->GetKey());
		};
	if (!isDefault(material->GetVertexShader(ERenderPassType::Shadow), renderer.GetDefaultShadowDepthVertexShader()))
		return false;
	if (!isDefault(material->GetPixelShader(ERenderPassType::Shadow), renderer.GetDefaultShadowDepthPixelShader()))
		return false;
	return true;
}
#pragma endregion
