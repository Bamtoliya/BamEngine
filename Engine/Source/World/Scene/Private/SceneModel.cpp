#include "Scene.h"
#include "Model.h"
#include "MaterialInterface.h"
#include "WorldComponents.h"
#include "LODGroupComponent.h"
#include "StaticMeshRendererComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "AnimatorComponent.h"
#include "TransformSystem.h"
#include "Logger.h"
#include "AnimationSceneQuery.h"

#include <filesystem>
#include <limits>
#include <map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Engine
{
	namespace
	{
		void InitializeModelEntity(entt::registry& world, entt::entity entity, const wstring& name,
			const TransformComponent& local)
		{
			auto& component = world.emplace<NameComponent>(entity);
			if (!name.empty())
				component.name = name;

			world.emplace<FlagComponent>(entity);
			world.emplace<HierarchyComponent>(entity);
			world.emplace<TransformComponent>(entity, local);
			world.emplace<WorldTransformComponent>(entity);
		}

		void AttachModelEntity(entt::registry& world, entt::entity child, entt::entity parent)
		{
			world.get<HierarchyComponent>(child).parent = parent;
			world.get<HierarchyComponent>(parent).children.push_back(child);
		}

		ResourceHandle<MaterialInterface> ResolveModelMeshMaterial(const Model& model, uint32 meshIndex,
			const ResourceHandle<MaterialInterface>& fallback)
		{
			auto material = model.GetMeshMaterial(meshIndex);
			return material.IsValid() ? material : fallback;
		}

		void AddModelMeshRenderer(entt::registry& world, entt::entity entity, const ResourceHandle<Mesh>& mesh,
			const ResourceHandle<MaterialInterface>& material, const ResourceHandle<Skeleton>& skeleton,
			const ResourceHandle<Mesh>& lod1 = {}, const ResourceHandle<Mesh>& lod2 = {})
		{
			if (skeleton.IsValid())
			{
				auto& renderer = world.emplace<SkinnedMeshRendererComponent>(entity);
				renderer.meshHandle = mesh;
				renderer.materialHandle = material;
				renderer.skeletonHandle = skeleton;
			}
			else
			{
				auto& renderer = world.emplace<StaticMeshRendererComponent>(entity);
				renderer.meshHandle = mesh;
				renderer.materialHandle = material;
			}

			if (lod1.IsValid() || lod2.IsValid())
			{
				auto& lodComp = world.emplace<LODGroupComponent>(entity);
				lodComp.lod1Mesh = lod1;
				lodComp.lod2Mesh = lod2;
				lodComp.lod1ScreenCoverage = 0.25f;
				lodComp.lod2ScreenCoverage = 0.08f;
				lodComp.hysteresisMargin = 0.02f;
			}
		}

		void InitializeLinkedModelMesh(entt::registry& world, entt::entity entity, entt::entity parent,
			const wstring& nodeKey, const wstring& attachmentKey, const ResourceHandle<Mesh>& mesh,
			const ResourceHandle<MaterialInterface>& material, const ResourceHandle<Skeleton>& skeleton,
			const ResourceHandle<Mesh>& lod1 = {}, const ResourceHandle<Mesh>& lod2 = {})
		{
			const auto name = std::filesystem::path(mesh->GetPath()).stem().wstring();
			InitializeModelEntity(world, entity, name, TransformComponent{});
			AttachModelEntity(world, entity, parent);
			AddModelMeshRenderer(world, entity, mesh, material, skeleton, lod1, lod2);

			auto& source = world.emplace<ModelSourceComponent>(entity);
			source.sourceNodeKey = nodeKey;
			source.meshAttachmentKey = attachmentKey;
			source.isMeshAttachment = true;
			source.useModelMaterial = true;
			source.useModelMesh = skeleton.IsValid();
		}
	}

	EResult Scene::InstantiateModel(const Model& model, const ResourceHandle<MaterialInterface>& material, Entity*& outRoot)
	{
		return InstantiateModelInternal(model, {}, material, outRoot);
	}

	EResult Scene::InstantiateLinkedModel(const ResourceHandle<Model>& sourceModel,
		const ResourceHandle<MaterialInterface>& material, Entity*& outRoot)
	{
		outRoot = nullptr;
		const auto* model = sourceModel.Get();
		if (!model)
			return EResult::InvalidArgument;

		return InstantiateModelInternal(*model, sourceModel, material, outRoot);
	}

	EResult Scene::RefreshLinkedModelParts(const Entity& root, bool& changed)
	{
		return RefreshLinkedModelPartsInternal(root, entt::null, EModelPartRefresh::All, changed);
	}

	EResult Scene::RestoreLinkedModelPartMaterial(const Entity& part, bool& changed)
	{
		return RestoreLinkedModelPart(part, EModelPartRefresh::RestoreMaterial, changed);
	}

	EResult Scene::RestoreLinkedModelPartMesh(const Entity& part, bool& changed)
	{
		return RestoreLinkedModelPart(part, EModelPartRefresh::RestoreMesh, changed);
	}

	EResult Scene::ReplaceLinkedSkinnedPartMesh(const Entity& part, const ResourceHandle<Mesh>& replacement)
	{
		auto& world = m_LocalRegistry;
		const auto handle = part.GetEntityHandle();
		if (part.GetScene() != this || !world.valid(handle))
			return EResult::InvalidArgument;

		auto* source = world.try_get<ModelSourceComponent>(handle);
		auto* renderer = world.try_get<SkinnedMeshRendererComponent>(handle);
		const auto* mesh = replacement.Get();
		if (!source || !source->isMeshAttachment || !renderer || !mesh)
			return EResult::InvalidArgument;

		const auto animatorEntity = FindAnimatorEntityInAncestors(world, handle);
		if (animatorEntity == entt::null)
			return EResult::InvalidArgument;

		const auto* instance = world.try_get<ModelInstanceComponent>(animatorEntity);
		const auto* model = instance ? instance->modelHandle.Get() : nullptr;
		if (!model)
			return EResult::InvalidArgument;

		const auto modelSkeleton = model->GetSkeleton();
		const auto* skeleton = FindSkeletonInHierarchy(world, animatorEntity);
		if (!skeleton || skeleton != modelSkeleton.Get() || skeleton != renderer->skeletonHandle.Get() ||
			!mesh->IsSkinCompatibleWith(*skeleton))
			return EResult::InvalidArgument;

		renderer->meshHandle = replacement;
		source->useModelMesh = false;
		return EResult::Success;
	}

	EResult Scene::InstantiateModelInternal(const Model& model, const ResourceHandle<Model>& sourceModel,
		const ResourceHandle<MaterialInterface>& material, Entity*& outRoot)
	{
		outRoot = nullptr;
		if (!material.IsValid())
			return EResult::InvalidArgument;

		const auto skeleton = model.GetSkeleton();
		const bool hasSkeleton = skeleton.IsValid();
		const bool linked = sourceModel.IsValid();
		const auto& nodes = model.GetNodes();
		const auto meshes = model.GetMeshes();
		if (nodes.empty())
			return EResult::InvalidArgument;

		std::vector<TransformComponent> localTransforms(nodes.size());
		size_t entityCount = nodes.size() + 1;

		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const auto& node = nodes[i];
			if (node.ParentIndex < -1 || (node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) >= i))
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

			size_t extraMeshes = node.MeshIndices.size();
			if (!linked && extraMeshes > 0)
				--extraMeshes;
			if (extraMeshes > (std::numeric_limits<size_t>::max)() - entityCount)
				return EResult::InvalidArgument;

			entityCount += extraMeshes;
		}

		const uint64 savedNextId = m_NextEntityId;
		const auto created = CreateEntities(entityCount);
		auto& world = m_LocalRegistry;

		const auto rootHandle = created.front();
		const auto modelPath = model.GetPath().empty() ? model.GetKey() : model.GetPath();
		InitializeModelEntity(world, rootHandle, std::filesystem::path(modelPath).stem().wstring(), TransformComponent{});

		if (linked)
		{
			auto& instance = world.emplace<ModelInstanceComponent>(rootHandle);
			instance.modelHandle = sourceModel;
			instance.fallbackMaterial = material;
		}

		const auto animations = model.GetAnimations();
		if (hasSkeleton || !animations.empty())
		{
			auto& animator = world.emplace<AnimatorComponent>(rootHandle);
			if (!animations.empty())
				animator.currentClip = animations.front();
		}

		size_t nextExtraEntity = nodes.size() + 1;
		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const auto& node = nodes[i];
			const auto entity = created[i + 1];
			const auto parent = node.ParentIndex < 0 ? rootHandle : created[static_cast<size_t>(node.ParentIndex) + 1];

			InitializeModelEntity(world, entity, node.Name, localTransforms[i]);
			AttachModelEntity(world, entity, parent);
			if (linked)
				world.emplace<ModelSourceComponent>(entity).sourceNodeKey = node.SourceNodeKey;

			for (size_t j = 0; j < node.MeshIndices.size(); ++j)
			{
				const uint32 meshIndex = node.MeshIndices[j];
				const auto& mesh = meshes[meshIndex];
				const auto assignedMaterial = ResolveModelMeshMaterial(model, meshIndex, material);
				const auto lod1 = model.GetMeshLOD(meshIndex, 1);
				const auto lod2 = model.GetMeshLOD(meshIndex, 2);

				if (!linked && j == 0)
				{
					AddModelMeshRenderer(world, entity, mesh, assignedMaterial, skeleton, lod1, lod2);
					continue;
				}

				const auto meshEntity = created[nextExtraEntity++];
				if (linked)
				{
					const auto attachmentKey = j < node.MeshAttachmentKeys.size() ? node.MeshAttachmentKeys[j] : wstring{};
					InitializeLinkedModelMesh(world, meshEntity, entity, node.SourceNodeKey, attachmentKey,
						mesh, assignedMaterial, skeleton, lod1, lod2);
				}
				else
				{
					const auto name = std::filesystem::path(mesh->GetPath()).stem().wstring();
					InitializeModelEntity(world, meshEntity, name, TransformComponent{});
					AttachModelEntity(world, meshEntity, entity);
					AddModelMeshRenderer(world, meshEntity, mesh, assignedMaterial, skeleton, lod1, lod2);
				}
			}
		}

		auto* root = FindEntity(rootHandle);
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

	EResult Scene::RestoreLinkedModelPart(const Entity& part, EModelPartRefresh mode, bool& changed)
	{
		changed = false;
		auto& world = m_LocalRegistry;
		const auto partHandle = part.GetEntityHandle();
		if (part.GetScene() != this || !world.valid(partHandle))
			return EResult::InvalidArgument;

		const auto* source = world.try_get<ModelSourceComponent>(partHandle);
		if (!source || !source->isMeshAttachment)
			return EResult::InvalidArgument;

		std::unordered_set<entt::entity> visited{ partHandle };
		auto current = partHandle;
		while (const auto* link = world.try_get<HierarchyComponent>(current))
		{
			current = link->parent;
			if (!world.valid(current) || !visited.insert(current).second)
				break;

			if (world.all_of<ModelInstanceComponent>(current))
			{
				const auto* root = FindEntity(current);
				if (!root)
					return EResult::InvalidArgument;

				return RefreshLinkedModelPartsInternal(*root, partHandle, mode, changed);
			}

			const auto* parent = world.try_get<ModelSourceComponent>(current);
			if (!parent || parent->isMeshAttachment)
				break;
		}

		return EResult::InvalidArgument;
	}

	EResult Scene::RefreshLinkedModelPartsInternal(const Entity& root, entt::entity restorePart,
		EModelPartRefresh mode, bool& changed)
	{
		changed = false;
		auto& world = m_LocalRegistry;
		const auto rootHandle = root.GetEntityHandle();
		if (root.GetScene() != this || !world.valid(rootHandle))
			return EResult::InvalidArgument;

		const auto* instance = world.try_get<ModelInstanceComponent>(rootHandle);
		const auto* hierarchy = world.try_get<HierarchyComponent>(rootHandle);
		const auto* model = instance ? instance->modelHandle.Get() : nullptr;
		if (!model || !hierarchy)
			return EResult::InvalidArgument;

		using PartKey = std::pair<wstring, wstring>;
		using MeshHandle = ResourceHandle<Mesh>;
		using MaterialHandle = ResourceHandle<MaterialInterface>;

		const bool refreshAll = mode == EModelPartRefresh::All;
		const auto& nodes = model->GetNodes();
		const auto skeleton = model->GetSkeleton();
		const auto meshes = model->GetMeshes();
		const auto* activeSkeleton = skeleton.IsValid() && mode != EModelPartRefresh::RestoreMaterial
			? FindSkeletonInHierarchy(world, rootHandle) : nullptr;

		const auto canBindSkin = [&](entt::entity entity, const MeshHandle& handle)
			{
				const auto* mesh = handle.Get();
				return mesh && activeSkeleton && activeSkeleton == skeleton.Get() &&
					FindAnimatorEntityInAncestors(world, entity) == rootHandle &&
					mesh->IsSkinCompatibleWith(*activeSkeleton);
			};

		std::map<wstring, size_t> sourceNodes;
		std::map<PartKey, uint32> unmatched;

		for (size_t i = 0; i < nodes.size(); ++i)
		{
			const auto& node = nodes[i];
			if (node.ParentIndex < -1 || (node.ParentIndex >= 0 && static_cast<size_t>(node.ParentIndex) >= i))
				return EResult::InvalidArgument;

			if (!node.SourceNodeKey.empty() && !sourceNodes.emplace(node.SourceNodeKey, i).second)
				return EResult::NotImplemented;

			if (node.MeshIndices.empty())
				continue;
			if (node.SourceNodeKey.empty() || node.MeshAttachmentKeys.size() != node.MeshIndices.size())
				return EResult::NotImplemented;

			for (size_t j = 0; j < node.MeshIndices.size(); ++j)
			{
				const auto& key = node.MeshAttachmentKeys[j];
				if (key.empty() || !unmatched.emplace(PartKey{ node.SourceNodeKey, key }, node.MeshIndices[j]).second)
					return EResult::NotImplemented;
			}
		}

		std::vector<std::pair<entt::entity, MeshHandle>> meshUpdates;
		std::vector<std::pair<entt::entity, MaterialHandle>> materialUpdates;
		std::vector<std::pair<entt::entity, MeshHandle>> effectiveSkinMeshes;
		std::map<wstring, entt::entity> sceneNodes;
		std::vector<entt::entity> pending(hierarchy->children.begin(), hierarchy->children.end());
		std::unordered_set<entt::entity> visited{ rootHandle };
		bool foundRestorePart = false;

		while (!pending.empty())
		{
			const auto entity = pending.back();
			pending.pop_back();

			if (!world.valid(entity) || !visited.insert(entity).second)
				return EResult::InvalidArgument;
			if (world.all_of<ModelInstanceComponent>(entity))
				continue;

			const auto* source = world.try_get<ModelSourceComponent>(entity);
			if (!source)
				continue;

			if (!source->isMeshAttachment)
			{
				if (!source->sourceNodeKey.empty() && !sceneNodes.emplace(source->sourceNodeKey, entity).second)
					return EResult::NotImplemented;

				if (const auto* link = world.try_get<HierarchyComponent>(entity))
					pending.insert(pending.end(), link->children.begin(), link->children.end());
				continue;
			}

			const auto match = unmatched.find({ source->sourceNodeKey, source->meshAttachmentKey });
			if (match == unmatched.end())
				return EResult::NotImplemented;

			const auto meshIndex = match->second;
			unmatched.erase(match);

			const auto* skinned = world.try_get<SkinnedMeshRendererComponent>(entity);
			const auto* rigid = world.try_get<StaticMeshRendererComponent>(entity);
			if (skinned)
			{
				if (rigid || !skeleton.IsValid() || skinned->skeletonHandle != skeleton)
					return EResult::NotImplemented;
			}
			else if (!rigid || skeleton.IsValid())
				return EResult::NotImplemented;

			const bool restoring = entity == restorePart;
			if (!refreshAll && !restoring)
				continue;
			foundRestorePart |= restoring;

			const bool refreshMesh = mode == EModelPartRefresh::RestoreMesh || (refreshAll && source->useModelMesh);
			if (refreshMesh)
			{
				if (!skinned || meshIndex >= meshes.size() || !meshes[meshIndex].IsValid())
					return EResult::NotImplemented;

				const auto& originalMesh = meshes[meshIndex];
				if ((skinned->meshHandle != originalMesh || restoring) && !canBindSkin(entity, originalMesh))
					return EResult::NotImplemented;
				if (skinned->meshHandle != originalMesh)
					meshUpdates.emplace_back(entity, originalMesh);
			}

			if (refreshAll && skinned)
				effectiveSkinMeshes.emplace_back(entity, refreshMesh ? meshes[meshIndex] : skinned->meshHandle);

			const bool refreshMaterial = mode == EModelPartRefresh::RestoreMaterial ||
				(refreshAll && source->useModelMaterial);

			if (refreshMaterial)
			{
				auto material = ResolveModelMeshMaterial(*model, meshIndex, instance->fallbackMaterial);
				if (!material.IsValid())
					return EResult::InvalidArgument;

				const auto& current = skinned ? skinned->materialHandle : rigid->materialHandle;
				if (current != material)
					materialUpdates.emplace_back(entity, std::move(material));
			}
		}

		if (!refreshAll && (!unmatched.empty() || !foundRestorePart))
			return EResult::NotImplemented;

		struct NodeTarget
		{
			entt::entity entity = entt::null;
			entt::entity existingAncestor = entt::null;
		};

		struct PendingNodeAddition
		{
			size_t nodeIndex;
			TransformComponent local;
		};

		struct PendingMeshAddition
		{
			size_t nodeIndex;
			wstring attachmentKey;
			uint32 meshIndex;
			MaterialHandle material;
		};

		std::vector<NodeTarget> nodeTargets(nodes.size());
		std::vector<PendingNodeAddition> nodeAdditions;
		std::vector<PendingMeshAddition> meshAdditions;

		if (refreshAll)
		{
			for (const auto& [key, entity] : sceneNodes)
			{
				if (!sourceNodes.contains(key))
					return EResult::NotImplemented;
			}

			for (size_t i = 0; i < nodes.size(); ++i)
			{
				const auto& node = nodes[i];
				if (node.SourceNodeKey.empty())
					continue;

				const auto existing = sceneNodes.find(node.SourceNodeKey);
				if (existing != sceneNodes.end())
				{
					nodeTargets[i] = { existing->second, existing->second };
					continue;
				}

				const auto ancestor = node.ParentIndex < 0
					? rootHandle : nodeTargets[static_cast<size_t>(node.ParentIndex)].existingAncestor;
				if (ancestor == entt::null ||
					!world.all_of<HierarchyComponent, TransformComponent, WorldTransformComponent>(ancestor))
					return EResult::NotImplemented;

				TransformComponent local;
				if (!TransformSystem::TryAssignLocalMatrix(local, glm::dmat4(node.LocalTransform)))
					return EResult::NotImplemented;

				nodeTargets[i].existingAncestor = ancestor;
				nodeAdditions.push_back({ i, std::move(local) });
			}

			meshAdditions.reserve(unmatched.size());
			for (const auto& [key, meshIndex] : unmatched)
			{
				const auto parent = sourceNodes.find(key.first);
				if (parent == sourceNodes.end() || meshIndex >= meshes.size() || !meshes[meshIndex].IsValid())
					return EResult::NotImplemented;

				const auto ancestor = nodeTargets[parent->second].existingAncestor;
				if (ancestor == entt::null ||
					!world.all_of<HierarchyComponent, TransformComponent, WorldTransformComponent>(ancestor))
					return EResult::NotImplemented;

				if (skeleton.IsValid() && !canBindSkin(ancestor, meshes[meshIndex]))
					return EResult::NotImplemented;

				auto material = ResolveModelMeshMaterial(*model, meshIndex, instance->fallbackMaterial);
				if (!material.IsValid())
					return EResult::InvalidArgument;

				meshAdditions.push_back({ parent->second, key.second, meshIndex, std::move(material) });
			}
		}

		const bool hasAdditions = !nodeAdditions.empty() || !meshAdditions.empty();
		if (hasAdditions)
		{
			// 새 계층을 붙이기 전에 유지할 스킨드 메시도 현재 리그와 대조합니다.
			for (const auto& [entity, mesh] : effectiveSkinMeshes)
			{
				if (!canBindSkin(entity, mesh))
					return EResult::NotImplemented;
			}

			if (meshAdditions.size() > (std::numeric_limits<size_t>::max)() - nodeAdditions.size())
				return EResult::InvalidArgument;

			const uint64 savedNextId = m_NextEntityId;
			const auto created = CreateEntities(nodeAdditions.size() + meshAdditions.size());

			for (size_t i = 0; i < nodeAdditions.size(); ++i)
			{
				const auto& add = nodeAdditions[i];
				const auto& node = nodes[add.nodeIndex];
				const auto entity = created[i];
				const auto parent = node.ParentIndex < 0
					? rootHandle : nodeTargets[static_cast<size_t>(node.ParentIndex)].entity;

				InitializeModelEntity(world, entity, node.Name, add.local);
				AttachModelEntity(world, entity, parent);
				world.emplace<ModelSourceComponent>(entity).sourceNodeKey = node.SourceNodeKey;
				nodeTargets[add.nodeIndex].entity = entity;
			}

			for (size_t i = 0; i < meshAdditions.size(); ++i)
			{
				const auto& add = meshAdditions[i];
				const auto entity = created[nodeAdditions.size() + i];
				const auto parent = nodeTargets[add.nodeIndex].entity;

				InitializeLinkedModelMesh(world, entity, parent, nodes[add.nodeIndex].SourceNodeKey,
					add.attachmentKey, meshes[add.meshIndex], add.material, skeleton);
			}

			if (!TransformSystem::UpdateHierarchy(world))
			{
				bool rolledBack = true;
				for (auto it = created.rbegin(); it != created.rend(); ++it)
				{
					auto* entity = FindEntity(*it);
					if (!entity || !DestroyEntity(*entity))
						rolledBack = false;
				}

				if (!rolledBack)
				{
					ENGINE_LOG_ERROR("Linked model hierarchy addition rollback failed.");
					return EResult::Fail;
				}

				m_NextEntityId = savedNextId;
				return EResult::InvalidArgument;
			}

			UpdateHierarchyFlags();
		}

		for (auto& [entity, mesh] : meshUpdates)
			world.get<SkinnedMeshRendererComponent>(entity).meshHandle = std::move(mesh);

		for (auto& [entity, material] : materialUpdates)
		{
			if (auto* skinned = world.try_get<SkinnedMeshRendererComponent>(entity))
				skinned->materialHandle = std::move(material);
			else
				world.get<StaticMeshRendererComponent>(entity).materialHandle = std::move(material);
		}

		changed = hasAdditions || !meshUpdates.empty() || !materialUpdates.empty();
		if (foundRestorePart)
		{
			auto& source = world.get<ModelSourceComponent>(restorePart);
			bool& followSource = mode == EModelPartRefresh::RestoreMesh
				? source.useModelMesh : source.useModelMaterial;

			if (!followSource)
			{
				followSource = true;
				changed = true;
			}
		}

		return EResult::Success;
	}
}