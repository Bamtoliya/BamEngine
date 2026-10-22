#pragma once

#include "Scene.h"
#include "CoreComponents.h"
#include "Adapters/GlmArchiveAdapter.h"
#include <limits>

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
	while (FindEntityById(m_NextEntityId) != nullptr)
		m_NextEntityId = m_NextEntityId == (std::numeric_limits<uint64>::max)() ? 1 : m_NextEntityId + 1;
	const uint64 id = m_NextEntityId;
	m_NextEntityId = id == (std::numeric_limits<uint64>::max)() ? 1 : id + 1;
	entt::entity entityHandle = m_LocalRegistry.create();
	m_LocalRegistry.emplace<IDComponent>(entityHandle, id);
	Entity* entity = Entity::Create(entityHandle, this);
	m_EntityMap[entityHandle] = entity;
	return *entity;
}
Entity* Scene::FindEntity(entt::entity handle)
{
	if (!m_LocalRegistry.valid(handle))
		return nullptr;

	auto it = m_EntityMap.find(handle);

	return it != m_EntityMap.end() ? it->second : nullptr;
}
bool Scene::DestroyEntity(Entity& entity)
{
	if (entity.GetScene() != this || FindEntity(entity.GetEntityHandle()) != &entity) return false;
	std::vector<entt::entity> pending{ entity.GetEntityHandle() }, removal;
	while (!pending.empty())
	{
		const auto current = pending.back();
		pending.pop_back();
		if (!FindEntity(current) || std::ranges::find(removal, current) != removal.end()) return false;
		removal.push_back(current);
		if (const auto* link = m_LocalRegistry.try_get<HierarchyComponent>(current))
		{
			for (const auto child : link->children)
			{
				const auto* childLink = m_LocalRegistry.valid(child) ?
					m_LocalRegistry.try_get<HierarchyComponent>(child) : nullptr;
				if (!childLink || childLink->parent != current) return false;
				pending.push_back(child);
			}
		}
	}
	for (auto [other, link] : m_LocalRegistry.view<HierarchyComponent>().each())
	{
		if (std::ranges::find(removal, other) != removal.end()) continue;
		if (std::ranges::find(removal, link.parent) != removal.end()) return false;
	}
	std::vector<std::pair<const reflection::PropertyInfo*, reflection::ObjectView>> references;
	if (ReflectionRegistry::IsCreated())
		for (const auto& entry : ReflectionRegistry::Get().GetComponentEntries())
		{
			if (!entry.ReflectionType || !entry.MatchesStorage(m_LocalRegistry)) continue;
			for (auto [handle] : m_LocalRegistry.storage<entt::entity>().each())
			{
				if (!entry.Has(m_LocalRegistry, handle) || std::ranges::find(removal, handle) != removal.end()) continue;
				const auto object = entry.Edit(m_LocalRegistry, handle);
				for (const auto& property : entry.ReflectionType->Properties)
				{
					if (property.GetValueCppType() != std::type_index(typeid(entt::entity))) continue;
					const auto* reference = property.ReadValue(object).Get<entt::entity>();
					if (!reference || std::ranges::find(removal, *reference) == removal.end()) continue;
					if (!property.CanWrite() || object.IsReadOnly()) return false;
					references.emplace_back(&property, object);
				}
			}
		}
	const entt::entity cleared = entt::null;
	for (const auto& [property, object] : references)
		if (!property->WriteValue(object, reflection::ValueView::From(cleared))) return false;
	for (auto [other, link] : m_LocalRegistry.view<HierarchyComponent>().each())
		std::erase_if(link.children, [&](auto child) { return std::ranges::find(removal, child) != removal.end(); });
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
	for (auto& pair : m_EntityMap)
	{
		Safe_Delete(pair.second);
	}
	m_EntityMap.clear();
	m_LocalRegistry.clear();
	m_NextEntityId = 1;
}
#pragma endregion

#pragma region Flag Management
void Scene::SetActive(bool active)
{
	if (active) AddFlag(m_Flags, ESceneFlags::Active);
	else RemoveFlag(m_Flags, ESceneFlags::Active);
}
void Scene::SetPaused(bool paused)
{
	if (paused) AddFlag(m_Flags, ESceneFlags::Paused);
	else RemoveFlag(m_Flags, ESceneFlags::Paused);
}
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

	// 1. 루트 엔티티(부모가 없는 엔티티)부터 하향식으로 전파
	for (const auto entity : world.view<FlagComponent>())
	{
		const auto* link = world.try_get<HierarchyComponent>(entity);
		if (!link || link->parent == entt::null) propagate(propagate, entity, EEntityFlag::None, false);
	}
}
#pragma endregion
