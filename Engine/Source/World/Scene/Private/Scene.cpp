#pragma once

#include "Scene.h"
#include "SceneEntityReference.h"
#include "WorldComponents.h"
#include "NavigationSystem.h"
#include "Adapters/GlmArchiveAdapter.h"

#pragma region Constructor&Destructor
EResult Scene::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_Name = desc->name;
	}

	return EResult::Success;
}

Scene* Scene::Create(void* arg)
{
	Scene* instance = new Scene();
	
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void Scene::Free()
{
	NavigationSystem::RemoveSceneNavGrid(this);
	NavigationSystem::RemoveSceneNavMesh(this);
	ClearEntities();
}
#pragma endregion

#pragma region Loop
void Scene::FixedUpdate(f32 dt)
{
	if (!IsActive()) return;
}

void Scene::Update(f32 dt)
{
	if (!IsActive()) return;
	UpdateHierarchyFlags();
}

void Scene::LateUpdate(f32 dt)
{
	if (!IsActive()) return;
}
#pragma endregion

#pragma region Entity Management
Entity& Scene::CreateEntity()
{
	if (FindEntityById(m_NextEntityId) != nullptr)
	{
		std::unordered_set<uint64> occupiedIds;
		occupiedIds.reserve(m_EntityMap.size());

		for (auto [handle, idComponent] : m_LocalRegistry.view<const IDComponent>().each())
		{
			if (m_EntityMap.contains(handle))
				occupiedIds.insert(idComponent.id);
		}

		do
		{
			m_NextEntityId = m_NextEntityId == (std::numeric_limits<uint64>::max)() ?
				1 : m_NextEntityId + 1;
		} while (occupiedIds.contains(m_NextEntityId));
	}

	return CreateEntityWithId(m_NextEntityId);
}
Entity& Scene::CreateEntityWithId(uint64 id)
{
	m_NextEntityId = id == (std::numeric_limits<uint64>::max)() ? 1 : id + 1;

	const entt::entity entityHandle = m_LocalRegistry.create();
	m_LocalRegistry.emplace<IDComponent>(entityHandle, id);
	Entity* entity = Entity::Create(entityHandle, this);
	m_EntityMap[entityHandle] = entity;
	return *entity;
}

std::vector<entt::entity> Scene::CreateEntities(std::size_t count)
{
	std::vector<entt::entity> created;
	if (count == 0) return created;
	created.reserve(count);

	std::unordered_set<uint64> occupiedIds;
	occupiedIds.reserve(m_EntityMap.size());

	for (auto [handle, idComponent] : m_LocalRegistry.view<const IDComponent>().each())
	{
		if (m_EntityMap.contains(handle))
			occupiedIds.insert(idComponent.id);
	}

	for (std::size_t i = 0; i < count; ++i)
	{
		while (occupiedIds.contains(m_NextEntityId))
		{
			m_NextEntityId = m_NextEntityId == (std::numeric_limits<uint64>::max)() ?
				1 : m_NextEntityId + 1;
		}

		const uint64 id = m_NextEntityId;
		occupiedIds.insert(id);
		created.push_back(CreateEntityWithId(id).GetEntityHandle());
	}

	return created;
}
Entity* Scene::FindEntity(entt::entity handle)
{
	if (!m_LocalRegistry.valid(handle))
		return nullptr;

	auto it = m_EntityMap.find(handle);

	return it != m_EntityMap.end() ? it->second : nullptr;
}
namespace
{
	using SceneExternalEntityReference = std::pair<const reflection::PropertyInfo*, reflection::ObjectView>;

	// root와 모든 하위 엔티티를 모은다. 계층 링크가 어긋났거나 하위 트리 밖의 엔티티가 그 안을 부모로 가리키면 실패한다.
	bool CollectSceneSubtreeForRemoval(Scene& scene, entt::registry& world, entt::entity root,
		std::vector<entt::entity>& removal, std::unordered_set<entt::entity>& removalSet)
	{
		std::vector<entt::entity> pending{ root };
		while (!pending.empty())
		{
			const auto current = pending.back();
			pending.pop_back();

			if (!scene.FindEntity(current) || !removalSet.insert(current).second) return false;
			removal.push_back(current);

			if (const auto* link = world.try_get<HierarchyComponent>(current))
			{
				for (const auto child : link->children)
				{
					const auto* childLink = world.valid(child) ? world.try_get<HierarchyComponent>(child) : nullptr;
					if (!childLink || childLink->parent != current) return false;
					pending.push_back(child);
				}
			}
		}

		for (auto [other, link] : world.view<HierarchyComponent>().each())
		{
			if (removalSet.contains(other)) continue;
			if (removalSet.contains(link.parent)) return false;
		}
		return true;
	}

	// 하위 트리 밖 컴포넌트의 리플렉션 엔티티 참조 중 하위 트리를 가리키는 것을 모은다.
	// 쓸 수 있는 참조는 writable에 담고, 비울 수 없는 읽기 전용 참조의 수를 반환한다.
	std::size_t CollectExternalSceneEntityReferences(entt::registry& world, const std::unordered_set<entt::entity>& removalSet,
		std::vector<SceneExternalEntityReference>& writable)
	{
		std::size_t readOnly = 0;
		if (!ReflectionRegistry::IsCreated()) return readOnly;

		std::unordered_set<uint64> removalIds;
		for (const auto handle : removalSet)
		{
			if (const auto* id = world.try_get<IDComponent>(handle); id && id->id != 0)
				removalIds.insert(id->id);
		}

		for (const auto& entry : ReflectionRegistry::Get().GetComponentEntries())
		{
			if (!entry.ReflectionType || !entry.MatchesStorage(world)) continue;
			for (auto [handle] : world.storage<entt::entity>().each())
			{
				if (!entry.Has(world, handle) || removalSet.contains(handle)) continue;
				const auto object = entry.Edit(world, handle);
				for (const auto& property : entry.ReflectionType->Properties)
				{
					const auto type = property.GetValueCppType();
					if (type == std::type_index(typeid(entt::entity)))
					{
						const auto value = property.ReadValue(object);
						const auto* reference = value.Get<entt::entity>();
						if (!reference || !removalSet.contains(*reference)) continue;
					}
					else if (type == std::type_index(typeid(SceneEntityReference)))
					{
						const auto value = property.ReadValue(object);
						const auto* reference = value.Get<SceneEntityReference>();
						if (!reference || !removalIds.contains(reference->PersistentId)) continue;
					}
					else continue;
					if (!property.CanWrite() || object.IsReadOnly())
						++readOnly;
					else
						writable.emplace_back(&property, object);
				}
			}
		}
		return readOnly;
	}
}

std::optional<std::size_t> Scene::CountExternalEntityReferences(Entity& root)
{
	if (root.GetScene() != this || FindEntity(root.GetEntityHandle()) != &root) return std::nullopt;

	std::vector<entt::entity> removal;
	std::unordered_set<entt::entity> removalSet;
	if (!CollectSceneSubtreeForRemoval(*this, m_LocalRegistry, root.GetEntityHandle(), removal, removalSet)) return std::nullopt;

	std::vector<SceneExternalEntityReference> writable;
	const std::size_t readOnly = CollectExternalSceneEntityReferences(m_LocalRegistry, removalSet, writable);
	return writable.size() + readOnly;
}

bool Scene::DestroyEntity(Entity& entity)
{
	if (entity.GetScene() != this || FindEntity(entity.GetEntityHandle()) != &entity) return false;

	std::vector<entt::entity> removal;
	std::unordered_set<entt::entity> removalSet;
	if (!CollectSceneSubtreeForRemoval(*this, m_LocalRegistry, entity.GetEntityHandle(), removal, removalSet)) return false;

	// 읽기 전용 참조는 비울 수 없으므로 삭제를 거부한다.
	std::vector<SceneExternalEntityReference> references;
	if (CollectExternalSceneEntityReferences(m_LocalRegistry, removalSet, references) != 0) return false;

	const entt::entity cleared = entt::null;
	const SceneEntityReference clearedPersistent;
	for (const auto& [property, object] : references)
	{
		const auto value = property->GetValueCppType() == std::type_index(typeid(SceneEntityReference))
			? reflection::ValueView::From(clearedPersistent) : reflection::ValueView::From(cleared);
		if (!property->WriteValue(object, value)) return false;
	}

	for (auto [other, link] : m_LocalRegistry.view<HierarchyComponent>().each())
		std::erase_if(link.children, [&](auto child) { return removalSet.contains(child); });

	for (const auto handle : removal)
	{
		Entity* wrapper = m_EntityMap.at(handle);
		m_EntityMap.erase(handle);
		m_LocalRegistry.destroy(handle);
		delete wrapper;
	}
	return true;
}
Entity* Scene::FindEntityById(uint64 id)
{
	return const_cast<Entity*>(std::as_const(*this).FindEntityById(id));
}
const Entity* Scene::FindEntityById(uint64 id) const
{
	if (id == 0) return nullptr;
	for (auto [handle, idComponent] : m_LocalRegistry.view<const IDComponent>().each())
	{
		if (idComponent.id != id) continue;
		const auto found = m_EntityMap.find(handle);
		if (found != m_EntityMap.end()) return found->second;
	}
	return nullptr;
}
bool Engine::Scene::MoveSiblingOrder(entt::entity entity, int direction)
{
	if (!m_LocalRegistry.valid(entity) || (direction != -1 && direction != 1)) return false;
	const auto* link = m_LocalRegistry.try_get<HierarchyComponent>(entity);
	if (!link || link->parent == entt::null) return false;

	auto* parentLink = m_LocalRegistry.try_get<HierarchyComponent>(link->parent);
	if (!parentLink) return false;

	auto& siblings = parentLink->children;
	auto it = std::find(siblings.begin(), siblings.end(), entity);
	if (it == siblings.end()) return false;

	const auto index = std::distance(siblings.begin(), it);
	const auto targetIndex = index + direction;
	if (targetIndex < 0 || targetIndex >= static_cast<std::ptrdiff_t>(siblings.size())) return false;

	std::swap(siblings[index], siblings[targetIndex]);
	return true;
}
void Scene::ClearEntities()
{
	m_LocalRegistry.clear();
	for (auto& pair : m_EntityMap)
	{
		Safe_Delete(pair.second);
	}
	m_EntityMap.clear();
	m_NextEntityId = 1;
}
#pragma endregion

#pragma region Flag Management
void Scene::UpdateHierarchyFlags()
{
	auto& world = m_LocalRegistry;
	const auto* entityStorage = std::as_const(world).storage<entt::entity>();
	if (!entityStorage || entityStorage->empty()) return;

	thread_local std::vector<std::uint8_t> visited;
	visited.assign(entityStorage->size(), 0);

	const auto propagate = [&](auto&& self, entt::entity entity, EEntityFlag parentEffective, bool hasParent) -> void
		{
			if (!entityStorage->contains(entity)) return;
			const auto idx = entityStorage->index(entity);
			if (visited[idx]) return;
			visited[idx] = 1;

			auto* flagComp = world.try_get<FlagComponent>(entity);
			EEntityFlag currentEffective = flagComp ? flagComp->flags : EEntityFlag::Default;

			if (hasParent)
			{
				const bool parentActive = HasFlag(parentEffective, EEntityFlag::Active);
				const bool parentVisible = HasFlag(parentEffective, EEntityFlag::Visible) && parentActive;
				const bool parentDead = HasFlag(parentEffective, EEntityFlag::Dead);

				if (!parentActive) RemoveFlag(currentEffective, EEntityFlag::Active);
				if (!parentVisible) RemoveFlag(currentEffective, EEntityFlag::Visible);
				if (parentDead) AddFlag(currentEffective, EEntityFlag::Dead);
			}

			if (flagComp) flagComp->effectiveFlags = currentEffective;

			if (const auto* link = world.try_get<HierarchyComponent>(entity))
				for (const auto child : link->children) self(self, child, currentEffective, true);
		};

	// FlagComponent 유무와 관계없이 모든 루트에서 전파합니다.
	for (auto [entity] : entityStorage->each())
	{
		const auto* link = world.try_get<HierarchyComponent>(entity);
		if (!link || link->parent == entt::null)
			propagate(propagate, entity, EEntityFlag::None, false);
	}
}
#pragma endregion
