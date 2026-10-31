#include "Scene.h"
#include "Model.h"
#include "MaterialInterface.h"
#include "CoreComponents.h"
#include "StaticMeshRendererComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "AnimatorComponent.h"
#include "TransformSystem.h"
#include "Logger.h"

EResult Engine::Scene::InstantiateModel(const Model& model,
	const ResourceHandle<MaterialInterface>& material, Entity*& outRoot)
{
	outRoot = nullptr;
	if (!material.IsValid())
		return EResult::InvalidArgument;
	// 기존 NotImplemented 가드 제거: Skeleton 유무 판별
	const bool hasSkeleton = model.GetSkeleton().IsValid();
	const auto& nodes = model.GetNodes();
	const auto meshes = model.GetMeshes();
	if (nodes.empty())
		return EResult::InvalidArgument;

	// 씬을 변경하기 전에 계층, 메시 참조, 로컬 변환을 검사합니다.
	std::vector<TransformComponent> localTransforms(nodes.size());
	size_t entityCount = nodes.size() + 1;

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const auto& node = nodes[i];

		if (node.ParentIndex < -1 ||
			(node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) >= i))
			return EResult::InvalidArgument;

		if (!TransformSystem::TryAssignLocalMatrix(localTransforms[i], glm::dmat4(node.LocalTransform)))
		{
			ENGINE_LOG_ERROR("Model node cannot be represented as TRS: node={}", i);
			return EResult::NotImplemented;
		}

		for (const uint32 meshIndex : node.MeshIndices)
		{
			if (meshIndex >= meshes.size() || !meshes[meshIndex].IsValid())
				return EResult::InvalidArgument;
		}

		const size_t extraMeshes = node.MeshIndices.empty() ? 0 : node.MeshIndices.size() - 1;
		if (extraMeshes > (std::numeric_limits<size_t>::max)() - entityCount)
			return EResult::InvalidArgument;

		entityCount += extraMeshes;
	}

	const uint64 savedNextId = m_NextEntityId;
	const auto created = CreateEntities(entityCount);
	auto& world = m_LocalRegistry;

	const auto initialize = [&](entt::entity handle, const wstring& name, const TransformComponent& local)
		{
			auto& component = world.emplace<NameComponent>(handle);
			if (!name.empty())
				component.name = name;

			world.emplace<FlagComponent>(handle);
			world.emplace<HierarchyComponent>(handle);
			world.emplace<TransformComponent>(handle, local);
			world.emplace<WorldTransformComponent>(handle);
		};

	const auto attach = [&](entt::entity child, entt::entity parent)
		{
			world.get<HierarchyComponent>(child).parent = parent;
			world.get<HierarchyComponent>(parent).children.push_back(child);
		};

	const auto addRenderer = [&](entt::entity handle, uint32 meshIndex)
		{
			const auto assignedMaterial = model.GetMeshMaterial(meshIndex);
			const auto matHandle = assignedMaterial.IsValid() ?
				ResourceHandle<MaterialInterface>(assignedMaterial) : material;
			if (hasSkeleton)
			{
				auto& skinnedRenderer = world.emplace<SkinnedMeshRendererComponent>(handle);
				skinnedRenderer.meshHandle = meshes[meshIndex];
				skinnedRenderer.materialHandle = matHandle;
				skinnedRenderer.skeletonHandle = model.GetSkeleton();
			}
			else
			{
				auto& staticRenderer = world.emplace<StaticMeshRendererComponent>(handle);
				staticRenderer.meshHandle = meshes[meshIndex];
				staticRenderer.materialHandle = matHandle;
			}
		};
	const entt::entity rootHandle = created.front();
	const auto modelPath = model.GetPath().empty() ? model.GetKey() : model.GetPath();
	initialize(rootHandle, std::filesystem::path(modelPath).stem().wstring(), TransformComponent{});
	// 스켈레톤이나 애니메이션이 있는 모델의 경우 루트 엔티티에 AnimatorComponent 자동 부착
	if (hasSkeleton || !model.GetAnimations().empty())
	{
		auto& animator = world.emplace<AnimatorComponent>(rootHandle);
		const auto& animations = model.GetAnimations();
		if (!animations.empty())
			animator.currentClip = animations.front(); // 첫 번째 기본 클립 지정
	}

	size_t nextExtraEntity = nodes.size() + 1;

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		const auto& node = nodes[i];
		const entt::entity handle = created[i + 1];

		initialize(handle, node.Name, localTransforms[i]);

		const entt::entity parent = node.ParentIndex < 0 ?
			rootHandle : created[static_cast<size_t>(node.ParentIndex) + 1];

		attach(handle, parent);

		for (size_t j = 0; j < node.MeshIndices.size(); ++j)
		{
			const uint32 meshIndex = node.MeshIndices[j];

			if (j == 0)
			{
				addRenderer(handle, meshIndex);
			}
			else
			{
				const entt::entity meshEntity = created[nextExtraEntity++];
				const auto meshName = std::filesystem::path(meshes[meshIndex]->GetPath()).stem().wstring();

				initialize(meshEntity, meshName, TransformComponent{});
				attach(meshEntity, handle);
				addRenderer(meshEntity, meshIndex);
			}
		}
	}

	Entity* root = FindEntity(rootHandle);

	if (!TransformSystem::UpdateHierarchy(world))
	{
		if (!DestroyEntity(*root))
		{
			ENGINE_LOG_ERROR("Model placement rollback failed.");
			return EResult::Fail;
		}

		m_NextEntityId = savedNextId;
		return EResult::InvalidArgument;
	}

	UpdateHierarchyFlags();
	outRoot = root;
	return EResult::Success;
}