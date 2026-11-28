#include "RaytracingSceneManager.h"
#include "RHI.h"
#include "RHIBuffer.h"
#include "RHITexture.h"
#include "Texture.h"
#include "MaterialInstance.h"
#include "Vertex.h"
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
#include "SkinnedMeshRendererComponent.h"
#include "AnimatorComponent.h"
#include "AnimationSceneQuery.h"
#include "PipelineManager.h"
#include "RHIPipeline.h"
#include "ResourceManager.h"
#include "PlatformPath.h"

#include <limits>

IMPLEMENT_SINGLETON(RaytracingSceneManager)

namespace
{
	// 이 프레임 수 동안 어느 씬에서도 쓰이지 않은 BLAS는 해제한다(메시 해제·LOD 전환 후 남은 항목 정리).
	constexpr Engine::uint64 BottomLevelRetentionFrames = 600;
	// 이 프레임 수 동안 요청되지 않은 씬의 TLAS는 해제한다. 프레임 제한된 뷰포트가 건너뛴 프레임에는 버퍼를 유지한다.
	constexpr Engine::uint64 TopLevelRetentionFrames = 120;

	// raytracedSkinningRTCS.hlsl의 RaytracedSkinningParams (b1, space3)
	struct RaytracedSkinningParams
	{
		Engine::uint32 SourcePositions = 0;
		Engine::uint32 SourceMaterials = 0;
		Engine::uint32 SourceSkin = 0;
		Engine::uint32 Bones = 0;
		Engine::uint32 VertexCount = 0;
		Engine::uint32 PositionStride = 0;
		Engine::uint32 MaterialStride = 0;
		Engine::uint32 SkinStride = 0;
		Engine::uint32 BoneOffset = 0;
		Engine::uint32 Padding[3] = {};
	};
	static_assert(sizeof(RaytracedSkinningParams) == 48);

	constexpr const wchar_t* SkinningShaderKey = L"Resources/Shader/RaytracedSkinningRTCS";
	constexpr const wchar_t* SkinningShaderFile = L"raytracedSkinningRTCS.cso";
	constexpr Engine::uint32 SkinnedPositionStride = sizeof(Engine::vec3);
	constexpr Engine::uint32 SkinningGroupSize = 64;   // raytracedSkinningRTCS의 numthreads
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
		ReleaseTopLevel(entry);
	m_TopLevels.clear();

	for (auto& [key, entry] : m_BottomLevels)
		ReleaseBottomLevel(key, entry);
	m_BottomLevels.clear();

	for (auto& [key, entry] : m_Skinned)
		ReleaseSkinned(entry);
	m_Skinned.clear();
	for (auto*& table : m_BoneTables)
		Safe_Release(table);

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

void RaytracingSceneManager::RequestScene(Scene* scene, uint32 viewID, bool reflections, const vec3* cameraPosition)
{
	if (!scene || !IsSupported())
		return;
	for (auto& request : m_RequestedScenes)
	{
		if (request.Source != scene || request.ViewID != viewID) continue;
		request.Reflections |= reflections;
		if (cameraPosition)
		{
			request.HasCameraPosition = true;
			request.CameraPosition = *cameraPosition;
		}
		return;
	}
	TopLevelRequest request{ scene, viewID, reflections };
	if (cameraPosition)
	{
		request.HasCameraPosition = true;
		request.CameraPosition = *cameraPosition;
	}
	m_RequestedScenes.push_back(request);
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
		// 스키닝과 스킨드 BLAS 갱신을 모든 TLAS 빌드보다 먼저 기록한다. TLAS 빌드 앞의 전역 UAV 배리어가 둘을 잇는다.
		PrepareSkinnedMeshes(stats);
		for (const TopLevelRequest& request : m_RequestedScenes)
		{
			TopLevelEntry& entry = m_TopLevels[TopLevelKey{ request.Source, request.ViewID }];
			entry.LastRequestedFrame = m_FrameCounter;
			const EResult result = BuildScene(*request.Source, request.ViewID, request.Reflections, entry, stats);
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
		ReleaseTopLevel(it->second);
		it = m_TopLevels.erase(it);
	}

	PruneBottomLevels();
	for (const auto& [key, entry] : m_BottomLevels)
	{
		if (!entry.AccelerationStructure) continue;
		++stats.BottomLevelCount;
		stats.BottomLevelBytes += entry.AccelerationStructure->GetResultSizeInBytes();
	}
	PruneSkinned();
	for (const auto& [key, entry] : m_Skinned)
	{
		if (entry.Positions) stats.SkinnedBufferBytes += entry.Positions->GetSize();
		if (entry.Materials) stats.SkinnedBufferBytes += entry.Materials->GetSize();
		if (!entry.BottomLevel) continue;
		++stats.BottomLevelCount;
		stats.BottomLevelBytes += entry.BottomLevel->GetResultSizeInBytes();
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
		stats.UnsupportedCasterCount != m_LastStats.UnsupportedCasterCount || stats.FailedCasterCount != m_LastStats.FailedCasterCount ||
		stats.SkinnedCount != m_LastStats.SkinnedCount || stats.SkinnedOverLimitCount != m_LastStats.SkinnedOverLimitCount;
	if (pendingChanged || (!pending && resultChanged))
	{
		ENGINE_LOG_INFO("Raytracing AS: {} TLAS, {} instance(s), {} BLAS ({} new, {:.2f} MB), TLAS {:.2f} MB, {} caster(s) left to shadow maps, {} pending, {} failed, "
			"{} skinned ({} over limit, {:.2f} MB skinned vertices).",
			stats.TopLevelCount, stats.InstanceCount, stats.BottomLevelCount, stats.BottomLevelBuiltThisFrame,
			static_cast<double>(stats.BottomLevelBytes) / (1024.0 * 1024.0), static_cast<double>(stats.TopLevelBytes) / (1024.0 * 1024.0),
			stats.UnsupportedCasterCount, stats.PendingCasterCount, stats.FailedCasterCount,
			stats.SkinnedCount, stats.SkinnedOverLimitCount, static_cast<double>(stats.SkinnedBufferBytes) / (1024.0 * 1024.0));
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

uint32 RaytracingSceneManager::GetInstanceTableIndex(const Scene* scene, uint32 viewID) const
{
	const auto it = m_TopLevels.find(TopLevelKey{ scene, viewID });
	if (it == m_TopLevels.end() || !it->second.BuiltThisFrame || !it->second.AccelerationStructure)
		return RHI::INVALID_BINDLESS_INDEX;
	return it->second.InstanceTableIndex;
}

void RaytracingSceneManager::ReleaseTopLevel(TopLevelEntry& entry)
{
	Safe_Release(entry.AccelerationStructure);
	for (auto*& table : entry.InstanceTables)
		Safe_Release(table);
}

bool RaytracingSceneManager::BuildInstanceData(const Mesh& mesh, const MaterialInterface& material, const MaterialConstants* overrideConstants,
	RaytracingInstanceData& data) const
{
	data.PositionBuffer = m_RHI->GetBindlessIndex(mesh.GetPositionBuffer());
	data.MaterialBuffer = m_RHI->GetBindlessIndex(mesh.GetMaterialBuffer());
	data.PositionStride = mesh.GetStreamStride(EMeshStream::Position);
	data.MaterialStride = mesh.GetStreamStride(EMeshStream::Material);
	if (data.PositionBuffer == RHI::INVALID_BINDLESS_INDEX || data.MaterialBuffer == RHI::INVALID_BINDLESS_INDEX)
		return false;
	if (mesh.GetIndexBuffer())
	{
		data.IndexBuffer = m_RHI->GetBindlessIndex(mesh.GetIndexBuffer());
		data.IndexStride = mesh.GetIndexStride();
		if (data.IndexBuffer == RHI::INVALID_BINDLESS_INDEX)
			return false;
	}

	MaterialConstants constants;
	if (overrideConstants)
		constants = *overrideConstants;
	else
		material.BuildMaterialConstants(constants);
	data.MaterialFlags = constants.materialFlags;
	data.Roughness = constants.roughness;
	data.Metallic = constants.metallic;
	data.BaseColor = constants.baseColor;
	data.EmissiveColor = constants.emissiveColor;
	data.AmbientOcclusion = constants.ao;

	const auto* instance = dynamic_cast<const MaterialInstance*>(&material);
	const auto* base = instance ? instance->GetBaseMaterial() : nullptr;
	const auto findTexture = [&](uint32 slot) -> uint32
		{
			const Texture* texture = material.GetTextureBySlot(slot);
			if (!texture && base && !material.HasTextureBindingBySlot(slot))
				texture = base->GetTextureBySlot(slot);
			// 스트리밍 중이면 지금 올라가 있는 밉을 쓴다.
			return texture ? m_RHI->GetBindlessIndex(texture->GetAvailableRHITexture()) : RHI::INVALID_BINDLESS_INDEX;
		};
	data.BaseColorTexture = findTexture(0);
	data.PbrTexture = findTexture(2);
	data.EmissiveTexture = findTexture(3);
	// G-Buffer와 같이 재질 플래그가 법선 맵을 가리킬 때만 쓴다.
	if (HasFlag(static_cast<EMaterialFeatureFlags>(constants.materialFlags), EMaterialFeatureFlags::HasNormal))
		data.NormalTexture = findTexture(1);
	return true;
}

bool RaytracingSceneManager::UploadInstanceTable(TopLevelEntry& entry)
{
	const uint32 frameIndex = m_RHI->GetFrameResourceIndex();
	if (frameIndex >= MAX_SWAPCHAIN_BUFFERS)
		return false;

	const auto count = static_cast<uint32>(m_InstanceDataScratch.size());
	auto*& table = entry.InstanceTables[frameIndex];
	auto& capacity = entry.InstanceTableCapacity[frameIndex];
	if (!table || capacity < count)
	{
		const uint32 newCapacity = std::max({ count, capacity * 2, 64u });
		std::vector<RaytracingInstanceData> zeros(newCapacity);
		auto* buffer = m_RHI->CreateBuffer(zeros.data(), newCapacity * static_cast<uint32>(sizeof(RaytracingInstanceData)),
			sizeof(RaytracingInstanceData), ERHIBufferType::Structured);
		if (!buffer)
			return false;
		Safe_Release(table);
		table = buffer;
		capacity = newCapacity;
	}
	if (count > 0)
		table->SetData(m_InstanceDataScratch.data(), count * static_cast<uint32>(sizeof(RaytracingInstanceData)));
	entry.InstanceTableIndex = m_RHI->GetBindlessIndex(table);
	return entry.InstanceTableIndex != RHI::INVALID_BINDLESS_INDEX;
}

EResult RaytracingSceneManager::BuildScene(Scene& scene, uint32 viewID, bool reflections, TopLevelEntry& entry, Stats& stats)
{
	auto& world = scene.GetRegistry();
	m_InstanceScratch.clear();
	m_InstanceDataScratch.clear();
	entry.InstanceTableIndex = RHI::INVALID_BINDLESS_INDEX;
	const bool allowBindless = m_RHI->GetCapabilities().SupportsBindlessResources;
	reflections = reflections && allowBindless;
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
		const bool castsShadow = meshRenderer.castShadow;
		if (!castsShadow && !reflections) continue;

		Mesh* mesh = meshRenderer.meshHandle.Get();
		MaterialInterface* material = meshRenderer.materialHandle.Get();
		if (!mesh || !material) continue;

		// 스플라인 배치·변형 메시는 프레임마다 형상이 달라질 수 있어 섀도맵에 맡긴다.
		if (TryGetEnabledComponent<SplineMeshComponent>(world, entity))
		{
			if (castsShadow) ++entry.UnsupportedCasterCount;
			continue;
		}

		// 이 뷰의 GBuffer가 고른 LOD와 같은 형상으로 차폐해 자기 그림자 오차를 막는다.
		const auto* lodGroup = TryGetEnabledComponent<LODGroupComponent>(world, entity);
		mesh = ResidentMeshLODs(mesh, lodGroup).Find(lodGroup ? lodGroup->viewLODs.Get(viewID) : 0);
		if (!mesh)
		{
			if (castsShadow) ++entry.PendingCasterCount;
			continue;
		}

		// 정적 렌더러에 붙은 스킨드 메시는 자세를 알 수 없어 섀도맵에 맡긴다(스키닝은 SkinnedMeshRendererComponent만).
		const bool staticGeometry = !mesh->IsSkinned();
		const bool isMasked = material->GetBlendMode() == EBlendMode::Masked;
		bool shadowSupported = castsShadow && staticGeometry && IsShadowCasterSupported(mesh, material, allowBindless);

		RaytracingInstanceData instanceData;
		const bool reflectionSupported = reflections && staticGeometry && IsReflectionSupported(mesh, material) &&
			BuildInstanceData(*mesh, *material, nullptr, instanceData);
		if (!reflectionSupported && shadowSupported && isMasked && !BuildInstanceData(*mesh, *material, nullptr, instanceData))
			shadowSupported = false;
		if (castsShadow && !shadowSupported)
			++entry.UnsupportedCasterCount;
		if (!shadowSupported && !reflectionSupported)
			continue;

		// TLAS에 넣지 못한 캐스터는 Casters에 없으므로 섀도맵에 남는다.
		RHIAccelerationStructure* bottomLevel = nullptr;
		const EBottomLevelState state = GetOrBuildBottomLevel(*mesh, stats, bottomLevel);
		if (state != EBottomLevelState::Ready)
		{
			if (shadowSupported)
				++(state == EBottomLevelState::Pending ? entry.PendingCasterCount : entry.FailedCasterCount);
			continue;
		}

		RHIRaytracingInstanceDesc instance;
		instance.worldMatrix = worldTransform.worldMatrix;
		instance.bottomLevel = bottomLevel;
		instance.instanceMask = 0;
		if (shadowSupported)
			instance.instanceMask |= RAYTRACING_MASK_SHADOW;
		if (reflectionSupported)
		{
			instance.instanceMask |= RAYTRACING_MASK_REFLECTION;
			if (castsShadow) instance.instanceMask |= RAYTRACING_MASK_REFLECTION_SHADOW;
		}
		if (reflectionSupported || (shadowSupported && isMasked))
		{
			instance.forceNonOpaque = isMasked;
			instance.instanceID = static_cast<uint32>(m_InstanceDataScratch.size());
			m_InstanceDataScratch.push_back(instanceData);
		}
		m_InstanceScratch.push_back(instance);
		if (shadowSupported)
			entry.Casters.Add(static_cast<uint32>(entt::to_entity(entity)));
	}

	// 스킨드 메시는 PrepareSkinnedMeshes가 이번 프레임에 스키닝하고 BLAS를 맞춘 것만 넣는다.
	// 한도 초과·대기·실패한 캐스터는 Casters에 없으므로 섀도맵에 남는다.
	auto skinnedView = world.view<SkinnedMeshRendererComponent, FlagComponent>();
	for (auto [entity, skinnedRenderer, flag] : skinnedView.each())
	{
		if (!IsComponentEnabled<SkinnedMeshRendererComponent>(world, entity)) continue;
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
			continue;
		const bool castsShadow = skinnedRenderer.castShadow;
		if (!castsShadow && !reflections) continue;

		Mesh* mesh = nullptr;
		MaterialInterface* material = nullptr;
		MaterialConstants constants{};
		skinnedRenderer.ResolveRenderState(mesh, material, constants);
		if (!mesh || !material) continue;

		const bool skinnable = IsSkinningSupported(mesh);
		const bool isMasked = material->GetBlendMode() == EBlendMode::Masked;
		bool shadowSupported = castsShadow && skinnable && IsShadowCasterSupported(mesh, material, allowBindless);
		const bool reflectionWanted = reflections && skinnable && IsReflectionSupported(mesh, material);
		if (castsShadow && !shadowSupported)
			++entry.UnsupportedCasterCount;
		if (!shadowSupported && !reflectionWanted)
			continue;

		const uint32 entityIndex = static_cast<uint32>(entt::to_entity(entity));
		ESkinnedState state = ESkinnedState::Pending;
		const SkinnedEntry* skinned = FindReadySkinned(&scene, entityIndex, state);
		if (!skinned)
		{
			if (shadowSupported)
			{
				if (state == ESkinnedState::Failed) ++entry.FailedCasterCount;
				else if (state == ESkinnedState::Pending) ++entry.PendingCasterCount;
				else ++entry.UnsupportedCasterCount;   // 한도 초과, 스키닝 불가
			}
			continue;
		}

		// 정점·재질은 스키닝 결과를, 인덱스는 원본 메시를 읽는다.
		RaytracingInstanceData instanceData;
		const bool needInstanceData = reflectionWanted || (shadowSupported && isMasked);
		bool hasInstanceData = needInstanceData && BuildInstanceData(*mesh, *material, &constants, instanceData);
		if (hasInstanceData)
		{
			instanceData.PositionBuffer = m_RHI->GetBindlessIndex(skinned->Positions);
			instanceData.PositionStride = SkinnedPositionStride;
			instanceData.MaterialBuffer = m_RHI->GetBindlessIndex(skinned->Materials);
			instanceData.MaterialStride = skinned->MaterialStride;
			hasInstanceData = instanceData.PositionBuffer != RHI::INVALID_BINDLESS_INDEX &&
				instanceData.MaterialBuffer != RHI::INVALID_BINDLESS_INDEX;
		}
		const bool reflectionSupported = reflectionWanted && hasInstanceData;
		if (shadowSupported && isMasked && !hasInstanceData)
		{
			shadowSupported = false;
			++entry.UnsupportedCasterCount;
		}
		if (!shadowSupported && !reflectionSupported)
			continue;

		RHIRaytracingInstanceDesc instance;
		instance.worldMatrix = skinned->WorldMatrix;
		instance.bottomLevel = skinned->BottomLevel;
		instance.instanceMask = 0;
		if (shadowSupported)
			instance.instanceMask |= RAYTRACING_MASK_SHADOW;
		if (reflectionSupported)
		{
			instance.instanceMask |= RAYTRACING_MASK_REFLECTION;
			if (castsShadow) instance.instanceMask |= RAYTRACING_MASK_REFLECTION_SHADOW;
		}
		if (hasInstanceData)
		{
			instance.forceNonOpaque = isMasked;
			instance.instanceID = static_cast<uint32>(m_InstanceDataScratch.size());
			m_InstanceDataScratch.push_back(instanceData);
		}
		m_InstanceScratch.push_back(instance);
		if (shadowSupported)
			entry.Casters.Add(entityIndex);
	}

	const EResult result = m_RHI->BuildTopLevelAS(entry.AccelerationStructure, m_InstanceScratch.data(),
		static_cast<uint32>(m_InstanceScratch.size()));
	if (IsSuccess(result))
	{
		entry.InstanceCount = static_cast<uint32>(m_InstanceScratch.size());
		// 표를 만들지 못하면 이번 프레임의 반사와 알파 테스트에 쓰지 못한다.
		if (!m_InstanceDataScratch.empty() && !UploadInstanceTable(entry))
			entry.InstanceTableIndex = RHI::INVALID_BINDLESS_INDEX;
	}
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
			Engine::Unicode::WideToUtf8(mesh.GetKey()), key.VertexCount, key.IndexCount);
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

#pragma region Skinned
void RaytracingSceneManager::PrepareSkinnedMeshes(Stats& stats)
{
	m_SkinnedJobs.clear();
	m_BoneScratch.clear();
	m_SkinnedLimit.Reset();
	if (m_RequestedScenes.empty() || !m_RHI->GetCapabilities().SupportsBindlessResources)
		return;
	if (!CollectSkinnedJobs(stats))
		return;

	const auto fail = [](SkinnedJob& job) { job.Entry->State = ESkinnedState::Failed; job.Entry = nullptr; };

	// 모든 메시의 뼈 행렬을 한 표로 올린다.
	RHIPipeline* pipeline = GetSkinningPipeline();
	const uint32 frameIndex = m_RHI->GetFrameResourceIndex();
	uint32 boneTable = RHI::INVALID_BINDLESS_INDEX;
	if (pipeline && frameIndex < MAX_SWAPCHAIN_BUFFERS)
	{
		const auto boneCount = static_cast<uint32>(m_BoneScratch.size());
		auto*& table = m_BoneTables[frameIndex];
		auto& capacity = m_BoneTableCapacity[frameIndex];
		if (!table || capacity < boneCount)
		{
			const uint32 newCapacity = std::max({ boneCount, capacity * 2, SkinnedBoneCount });
			std::vector<mat4> identity(newCapacity, mat4(1.0f));
			auto* buffer = m_RHI->CreateBuffer(identity.data(), newCapacity * static_cast<uint32>(sizeof(mat4)), sizeof(mat4), ERHIBufferType::Structured);
			if (buffer)
			{
				Safe_Release(table);
				table = buffer;
				capacity = newCapacity;
			}
		}
		if (table && capacity >= boneCount)
		{
			table->SetData(m_BoneScratch.data(), boneCount * static_cast<uint32>(sizeof(mat4)));
			boneTable = m_RHI->GetBindlessIndex(table);
		}
	}
	if (!pipeline || boneTable == RHI::INVALID_BINDLESS_INDEX || IsFailure(m_RHI->BindPipeline(pipeline)))
	{
		for (SkinnedJob& job : m_SkinnedJobs)
			fail(job);
		return;
	}

	// 1) 스키닝: 메시마다 정점 하나에 스레드 하나
	for (SkinnedJob& job : m_SkinnedJobs)
	{
		SkinnedEntry& entry = *job.Entry;
		const Mesh& mesh = *job.SourceMesh;
		if (!EnsureSkinnedOutputs(entry, mesh))
		{
			fail(job);
			continue;
		}
		RaytracedSkinningParams params;
		params.SourcePositions = m_RHI->GetBindlessIndex(mesh.GetPositionBuffer());
		params.SourceMaterials = m_RHI->GetBindlessIndex(mesh.GetMaterialBuffer());
		params.SourceSkin = m_RHI->GetBindlessIndex(mesh.GetSkinDataBuffer());
		params.Bones = boneTable;
		params.VertexCount = mesh.GetVertexCount();
		params.PositionStride = mesh.GetStreamStride(EMeshStream::Position);
		params.MaterialStride = mesh.GetStreamStride(EMeshStream::Material);
		params.SkinStride = mesh.GetStreamStride(EMeshStream::SkinData);
		params.BoneOffset = job.BoneOffset;
		if (params.SourcePositions == RHI::INVALID_BINDLESS_INDEX || params.SourceMaterials == RHI::INVALID_BINDLESS_INDEX ||
			params.SourceSkin == RHI::INVALID_BINDLESS_INDEX ||
			IsFailure(m_RHI->BindConstantBuffer(&params, sizeof(params), 3)) ||
			IsFailure(m_RHI->BindComputeStorageBuffer(entry.Positions, 0)) ||
			IsFailure(m_RHI->BindComputeStorageBuffer(entry.Materials, 1)) ||
			IsFailure(m_RHI->Dispatch((params.VertexCount + SkinningGroupSize - 1) / SkinningGroupSize, 1, 1)))
		{
			fail(job);
		}
	}

	// 2) 결과를 BLAS 빌드 입력과 반사 셰이더가 읽을 상태로 바꾼다. 전환이 Compute 쓰기의 완료도 보장한다.
	for (SkinnedJob& job : m_SkinnedJobs)
	{
		if (!job.Entry) continue;
		if (IsFailure(m_RHI->TransitionStorageBufferToShaderRead(job.Entry->Positions)) ||
			IsFailure(m_RHI->TransitionStorageBufferToShaderRead(job.Entry->Materials)))
			fail(job);
	}

	// 3) BLAS 빌드 또는 refit
	for (SkinnedJob& job : m_SkinnedJobs)
	{
		if (job.Entry)
			UpdateSkinnedBottomLevel(*job.Entry, *job.SourceMesh, stats);
	}
}

bool RaytracingSceneManager::CollectSkinnedJobs(Stats& stats)
{
	// 같은 씬을 여러 뷰가 요청해도 한 번만 스키닝한다. 반사 요청과 카메라 위치는 합친다.
	struct SceneRequestSummary
	{
		Scene* Source = nullptr;
		bool Reflections = false;
		std::vector<vec3> CameraPositions;
	};
	std::vector<SceneRequestSummary> scenes;
	for (const TopLevelRequest& request : m_RequestedScenes)
	{
		const auto it = std::find_if(scenes.begin(), scenes.end(), [&](const SceneRequestSummary& item) { return item.Source == request.Source; });
		if (it == scenes.end())
		{
			SceneRequestSummary summary{ request.Source, request.Reflections, {} };
			if (request.HasCameraPosition)
				summary.CameraPositions.push_back(request.CameraPosition);
			scenes.push_back(std::move(summary));
		}
		else
		{
			it->Reflections |= request.Reflections;
			if (request.HasCameraPosition)
				it->CameraPositions.push_back(request.CameraPosition);
		}
	}

	struct SkinnedCandidate
	{
		Scene* Source = nullptr;
		uint32 EntityIndex = 0;
		Mesh* SourceMesh = nullptr;
		mat4 WorldMatrix{ 1.0f };
		const std::vector<mat4>* Bones = nullptr;
		float DistanceSquared = 0.0f;
	};
	std::vector<SkinnedCandidate> candidates;
	std::vector<RaytracingSkinnedPriority> priorities;
	bool sortByDistance = false;
	const bool allowMaskedShadow = m_RHI->GetCapabilities().SupportsBindlessResources;

	for (const SceneRequestSummary& summary : scenes)
	{
		Scene* scene = summary.Source;
		auto& world = scene->GetRegistry();
		auto view = world.view<SkinnedMeshRendererComponent, WorldTransformComponent, FlagComponent>();
		for (auto [entity, skinnedRenderer, worldTransform, flag] : view.each())
		{
			// MeshRenderSystem의 스킨드 메시 제출과 같은 가시성 조건
			if (!IsComponentEnabled<SkinnedMeshRendererComponent>(world, entity)) continue;
			if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
				continue;

			Mesh* mesh = nullptr;
			MaterialInterface* material = nullptr;
			MaterialConstants constants{};
			skinnedRenderer.ResolveRenderState(mesh, material, constants);
			if (!mesh || !material || !IsSkinningSupported(mesh))
				continue;
			const bool needed = (skinnedRenderer.castShadow && IsShadowCasterSupported(mesh, material, allowMaskedShadow)) ||
				(summary.Reflections && IsReflectionSupported(mesh, material));
			if (!needed)
				continue;

			// 그리기 경로와 같이 애니메이터(루트)의 월드 행렬과 뼈 행렬을 쓴다. 애니메이터가 없으면 단위 뼈다.
			mat4 worldMatrix = worldTransform.worldMatrix;
			const std::vector<mat4>* bones = nullptr;
			const entt::entity animatorEntity = FindAnimatorEntityInAncestors(world, entity);
			if (animatorEntity != entt::null)
			{
				if (const auto* animWorld = world.try_get<WorldTransformComponent>(animatorEntity))
					worldMatrix = animWorld->worldMatrix;
				if (const auto* animator = TryGetEnabledComponent<AnimatorComponent>(world, animatorEntity); animator && !animator->finalBoneMatrices.empty())
					bones = &animator->finalBoneMatrices;
			}

			float minDistanceSq = std::numeric_limits<float>::max();
			if (!summary.CameraPositions.empty())
			{
				sortByDistance = true;
				const vec3 worldPos(worldMatrix[3]);
				for (const vec3& camPos : summary.CameraPositions)
				{
					const vec3 delta = worldPos - camPos;
					const float distSq = glm::dot(delta, delta);
					if (distSq < minDistanceSq)
						minDistanceSq = distSq;
				}
			}

			const auto order = static_cast<uint32>(candidates.size());
			candidates.push_back({ scene, static_cast<uint32>(entt::to_entity(entity)), mesh, worldMatrix, bones, minDistanceSq });
			priorities.push_back({ minDistanceSq, order });
		}
	}

	if (sortByDistance)
		SortSkinnedByDistance(priorities);

	for (const RaytracingSkinnedPriority& priority : priorities)
	{
		const SkinnedCandidate& candidate = candidates[priority.Order];
		const SkinnedKey key{ candidate.Source, candidate.EntityIndex };
		if (!m_SkinnedLimit.TryAcquire())
		{
			if (const auto it = m_Skinned.find(key); it != m_Skinned.end())
			{
				it->second.LastUsedFrame = m_FrameCounter;
				it->second.StateFrame = m_FrameCounter;
				it->second.State = ESkinnedState::OverLimit;
			}
			++stats.SkinnedOverLimitCount;
			continue;
		}

		SkinnedEntry& entry = m_Skinned[key];
		entry.LastUsedFrame = m_FrameCounter;
		entry.StateFrame = m_FrameCounter;
		entry.State = ESkinnedState::Pending;
		entry.WorldMatrix = candidate.WorldMatrix;

		const auto boneOffset = static_cast<uint32>(m_BoneScratch.size());
		const size_t copyCount = candidate.Bones ? std::min<size_t>(candidate.Bones->size(), SkinnedBoneCount) : 0;
		if (copyCount > 0)
			m_BoneScratch.insert(m_BoneScratch.end(), candidate.Bones->begin(), candidate.Bones->begin() + static_cast<std::ptrdiff_t>(copyCount));
		m_BoneScratch.resize(static_cast<size_t>(boneOffset) + SkinnedBoneCount, mat4(1.0f));
		m_SkinnedJobs.push_back({ &entry, candidate.SourceMesh, boneOffset });
	}
	return !m_SkinnedJobs.empty();
}

bool RaytracingSceneManager::EnsureSkinnedOutputs(SkinnedEntry& entry, const Mesh& mesh)
{
	const uint32 vertexCount = mesh.GetVertexCount();
	const uint32 materialStride = mesh.GetStreamStride(EMeshStream::Material);
	const uint64 positionBytes = static_cast<uint64>(vertexCount) * SkinnedPositionStride;
	const uint64 materialBytes = static_cast<uint64>(vertexCount) * materialStride;
	if (materialBytes > UINT32_MAX)
		return false;
	if (entry.Positions && entry.Materials && entry.Positions->GetSize() == positionBytes && entry.Materials->GetSize() == materialBytes)
		return true;

	// 크기가 바뀌면 정점 수가 바뀐 것이므로 BLAS도 다시 만든다.
	Safe_Release(entry.Positions);
	Safe_Release(entry.Materials);
	Safe_Release(entry.BottomLevel);
	entry.RefitCount = 0;
	entry.Positions = m_RHI->CreateBuffer(nullptr, static_cast<uint32>(positionBytes), SkinnedPositionStride, ERHIBufferType::Storage);
	entry.Materials = m_RHI->CreateBuffer(nullptr, static_cast<uint32>(materialBytes), materialStride, ERHIBufferType::Storage);
	entry.MaterialStride = materialStride;
	if (entry.Positions && entry.Materials)
		return true;
	ENGINE_LOG_WARN("Failed to create skinned vertex buffers for raytracing ({} vertices). The mesh stays in shadow maps.", vertexCount);
	Safe_Release(entry.Positions);
	Safe_Release(entry.Materials);
	return false;
}

void RaytracingSceneManager::UpdateSkinnedBottomLevel(SkinnedEntry& entry, const Mesh& mesh, Stats& stats)
{
	RHIRaytracingGeometryDesc geometry;
	geometry.positionBuffer = entry.Positions;
	geometry.vertexCount = mesh.GetVertexCount();
	geometry.vertexStride = SkinnedPositionStride;
	geometry.indexBuffer = mesh.GetIndexBuffer();
	geometry.indexCount = geometry.indexBuffer ? mesh.GetIndexCount() : 0;
	geometry.indexStride = mesh.GetIndexStride();
	geometry.allowUpdate = true;

	// 장비 교체 등으로 원본 메시가 바뀌면 BLAS를 새로 만들고 지난 실패 기록도 지운다.
	RHIBuffer* source = mesh.GetPositionBuffer();
	const bool sameGeometry = entry.SourcePositions == source && entry.GeometryVersion == mesh.GetGeometryVersion() &&
		entry.VertexCount == geometry.vertexCount && entry.IndexCount == geometry.indexCount;
	if (!sameGeometry)
	{
		Safe_AddRef(source);
		Safe_Release(entry.SourcePositions);
		entry.SourcePositions = source;
		entry.GeometryVersion = mesh.GetGeometryVersion();
		entry.VertexCount = geometry.vertexCount;
		entry.IndexCount = geometry.indexCount;
		entry.BuildFailed = false;
	}
	if (entry.BuildFailed)
	{
		entry.State = ESkinnedState::Failed;
		return;
	}

	const auto markReady = [&]()
		{
			entry.State = ESkinnedState::Ready;
			++stats.SkinnedCount;
		};
	const auto refit = [&]() -> bool
		{
			if (!entry.BottomLevel || IsFailure(m_RHI->UpdateBottomLevelAS(entry.BottomLevel, geometry)))
				return false;
			++entry.RefitCount;
			return true;
		};

	const uint64 triangles = (geometry.indexBuffer ? geometry.indexCount : geometry.vertexCount) / 3;
	const ESkinnedBottomLevelAction action = ChooseSkinnedBottomLevelAction(entry.BottomLevel != nullptr, sameGeometry,
		entry.RefitCount, SkinnedRebuildInterval);
	bool budgetTaken = false;
	if (action == ESkinnedBottomLevelAction::Rebuild)
		budgetTaken = m_BuildBudget.TryConsume(triangles);
	// Refit이거나, 재빌드할 예산이 없으면 이번 프레임은 refit으로 대신한다.
	if (action != ESkinnedBottomLevelAction::Build && !budgetTaken)
	{
		if (refit())
		{
			markReady();
			return;
		}
		Safe_Release(entry.BottomLevel);
	}
	if (!budgetTaken && !m_BuildBudget.TryConsume(triangles))
	{
		Safe_Release(entry.BottomLevel);   // 기하가 바뀐 BLAS를 남겨 두지 않는다
		entry.State = ESkinnedState::Pending;
		return;
	}

	const bool firstBuild = action == ESkinnedBottomLevelAction::Build;
	if (firstBuild)
		Safe_Release(entry.BottomLevel);
	RHIAccelerationStructure* built = m_RHI->CreateBottomLevelAS(geometry);
	if (!built)
	{
		// 주기적 재빌드에 실패하면 기존 BLAS를 계속 갱신한다.
		if (refit())
		{
			markReady();
			return;
		}
		Safe_Release(entry.BottomLevel);
		entry.BuildFailed = true;
		entry.State = ESkinnedState::Failed;
		ENGINE_LOG_WARN("Failed to build a skinned BLAS for mesh '{}' ({} vertices, {} indices). It stays in shadow maps.",
			Engine::Unicode::WideToUtf8(mesh.GetKey()), geometry.vertexCount, geometry.indexCount);
		return;
	}
	// 처음 빌드만 센다. 주기적 재빌드가 통계 로그를 반복하지 않게 한다.
	if (firstBuild)
		++stats.BottomLevelBuiltThisFrame;
	Safe_Release(entry.BottomLevel);
	entry.BottomLevel = built;
	entry.RefitCount = 0;
	markReady();
}

const RaytracingSceneManager::SkinnedEntry* RaytracingSceneManager::FindReadySkinned(const Scene* scene, uint32 entity, ESkinnedState& state) const
{
	const auto it = m_Skinned.find(SkinnedKey{ scene, entity });
	if (it == m_Skinned.end() || it->second.StateFrame != m_FrameCounter)
	{
		state = ESkinnedState::Unavailable;
		return nullptr;
	}
	state = it->second.State;
	return state == ESkinnedState::Ready && it->second.BottomLevel ? &it->second : nullptr;
}

RHIPipeline* RaytracingSceneManager::GetSkinningPipeline()
{
	if (m_SkinningShaderMissing)
		return nullptr;

	// DeferredSceneRenderer의 RT 셰이더처럼 처음 사용할 때 DXIL을 직접 등록한다.
	auto& resources = ResourceManager::Get();
	auto handle = resources.GetResourceHandle<Shader>(SkinningShaderKey);
	if (!handle.Get())
	{
		ShaderDesc desc{};
		desc.Key = SkinningShaderKey;
		desc.dxilPath = (PlatformPath::GetEngineResourceRoot() / "Shader" / "bin" / "dxil" / SkinningShaderFile).wstring();
		desc.Path = desc.dxilPath;
		desc.shaderType = EShaderType::Compute;
		handle = resources.LoadResource<Shader>(&desc);
	}
	Shader* shader = handle.Get();
	if (!shader || !shader->GetRHIShader())
	{
		ENGINE_LOG_ERROR("Shader is not available: {}. Skinned meshes stay in shadow maps and out of raytraced reflections.",
			Engine::Unicode::WideToUtf8(SkinningShaderFile));
		m_SkinningShaderMissing = true;
		return nullptr;
	}

	// 셰이더 핫 리로드로 RHI 셰이더가 바뀌어도 맞는 파이프라인을 쓰도록 매번 캐시에서 조회한다.
	RHIPipelineDesc desc{};
	desc.pipelineType = EPipelineType::Compute;
	desc.computeShader = shader->GetRHIShader();
	return PipelineManager::Get().GetOrCreatePipeline(desc);
}

void RaytracingSceneManager::ReleaseSkinned(SkinnedEntry& entry)
{
	Safe_Release(entry.BottomLevel);
	Safe_Release(entry.Positions);
	Safe_Release(entry.Materials);
	Safe_Release(entry.SourcePositions);
}

void RaytracingSceneManager::PruneSkinned()
{
	// 스키닝 버퍼는 크므로 TLAS와 같은 기간만 남긴다(엔티티 삭제·비활성, 씬 해제).
	for (auto it = m_Skinned.begin(); it != m_Skinned.end();)
	{
		if (m_FrameCounter - it->second.LastUsedFrame <= TopLevelRetentionFrames)
		{
			++it;
			continue;
		}
		ReleaseSkinned(it->second);
		it = m_Skinned.erase(it);
	}
}
#pragma endregion

#pragma region Caster Policy
bool RaytracingSceneManager::IsSkinningSupported(const Mesh* mesh)
{
	// ModelImporter는 스킨드 메시에 EMeshFlag::Dynamic을 부여하므로 스키닝 경로에서는 Dynamic 플래그로 제외하지 않는다.
	if (!mesh || !mesh->IsSkinned() || mesh->GetTopology() != ETopology::TriangleList)
		return false;
	const uint32 vertexCount = mesh->GetVertexCount();
	if (vertexCount == 0 || !mesh->GetPositionBuffer() || !mesh->GetMaterialBuffer())
		return false;
	// raytracedSkinningRTCS가 읽는 정점 형식
	if (mesh->GetStreamStride(EMeshStream::Position) < sizeof(vec3) ||
		mesh->GetStreamStride(EMeshStream::Material) != sizeof(VertexMaterial) ||
		mesh->GetStreamStride(EMeshStream::SkinData) != sizeof(VertexSkinData))
		return false;
	return mesh->GetStreamCount(EMeshStream::Material) == vertexCount && mesh->GetStreamCount(EMeshStream::SkinData) == vertexCount;
}

bool RaytracingSceneManager::IsReflectionSupported(const Mesh* mesh, const MaterialInterface* material)
{
	// 비스킨드 동적 메시는 매 프레임 정점이 바뀔 수 있어 제외하고, 스킨드 메시는 ModelImporter의 Dynamic 플래그를 허용한다.
	if (!mesh || !material || (!mesh->IsSkinned() && mesh->IsDynamic()) || mesh->GetTopology() != ETopology::TriangleList)
		return false;
	if (!mesh->GetPositionBuffer() || !mesh->GetMaterialBuffer() || mesh->GetVertexCount() == 0 ||
		mesh->GetStreamStride(EMeshStream::Material) != sizeof(VertexMaterial))
		return false;
	const EBlendMode blendMode = material->GetBlendMode();
	return blendMode == EBlendMode::Opaque || blendMode == EBlendMode::Masked;
}

bool RaytracingSceneManager::IsShadowCasterSupported(const Mesh* mesh, const MaterialInterface* material, bool allowMasked)
{
	if (!mesh || !material)
		return false;
	// 스킨드 메시는 Compute 스키닝 결과로 BLAS를 만들 때만 호출부가 지원으로 본다(IsSkinningSupported).
	// ModelImporter가 스킨드 메시에 EMeshFlag::Dynamic을 부여하므로 비스킨드 동적 메시만 여기서 제외한다.
	if ((!mesh->IsSkinned() && mesh->IsDynamic()) || mesh->GetTopology() != ETopology::TriangleList)
		return false;
	if (!mesh->GetPositionBuffer() || mesh->GetVertexCount() == 0)
		return false;
	const EBlendMode blendMode = material->GetBlendMode();
	if (blendMode != EBlendMode::Opaque && (!allowMasked || blendMode != EBlendMode::Masked))
		return false;
	// Masked 알파 테스트는 정점 UV를 읽기 위해 VertexMaterial 스트림이 필요하다.
	if (blendMode == EBlendMode::Masked &&
		(!mesh->GetMaterialBuffer() || mesh->GetStreamStride(EMeshStream::Material) != sizeof(VertexMaterial)))
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
	// 메시 종류와 맞지 않는 기본 VS는 Renderer::ResolveVertexShader가 다른 기본 VS로 바꾸므로 둘 다 기본으로 본다.
	const Shader* shadowVS = material->GetVertexShader(ERenderPassType::Shadow);
	if (!isDefault(shadowVS, renderer.GetDefaultShadowDepthVertexShader()) &&
		!isDefault(shadowVS, renderer.GetDefaultShadowDepthSkinningVertexShader()))
		return false;
	if (!isDefault(material->GetPixelShader(ERenderPassType::Shadow), renderer.GetDefaultShadowDepthPixelShader()))
		return false;
	return true;
}
#pragma endregion
