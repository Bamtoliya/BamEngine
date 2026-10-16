#include "Scene.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "UIComponents.h"
#include <archive/ArchiveInterface.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include <algorithm>
#include <memory>
#include <unordered_set>

namespace
{
    constexpr uint32 SceneArchiveVersion = 1;
    constexpr std::size_t MaxHierarchyDepth = 256;

    bool SupportsHierarchyLink(const entt::registry& world, entt::entity child, entt::entity parent)
    {
        const bool childUI = world.any_of<Engine::CanvasComponent, Engine::RectTransformComponent>(child);
        const bool parentUI = world.any_of<Engine::CanvasComponent, Engine::RectTransformComponent>(parent);
        if (childUI || parentUI)
            return world.all_of<Engine::RectTransformComponent>(child) &&
                !world.all_of<Engine::CanvasComponent>(child) && parentUI;
        return world.all_of<Engine::TransformComponent>(child) && world.all_of<Engine::TransformComponent>(parent);
    }

    struct SceneEntityRecord
    {
        uint64 Id = 0;
        entt::entity Handle = entt::null;
        bool HasHierarchy = false;
        uint64 Parent = 0;
        std::vector<uint64> Children;
        std::vector<const Engine::ComponentReflectionEntry*> Components;
    };

    struct SceneDeleter
    {
        void operator()(Engine::Scene* scene) const { if (scene != nullptr) scene->Release(); }
    };

    std::string EntityField(std::size_t index, std::string_view field)
    {
        return "Entities[" + std::to_string(index) + "]." + std::string(field);
    }

    template<typename T>
    bool MatchesStorage(const entt::registry& world)
    {
        const auto* storage = world.storage(entt::type_hash<T>::value());
        const auto& expected = entt::type_id<T>();
        return storage == nullptr ||
            (storage->info().hash() == expected.hash() && storage->info().name() == expected.name());
    }

    archive::ArchiveResult ValidateHierarchy(const std::vector<SceneEntityRecord>& entities,
        const entt::registry& world)
    {
        std::unordered_map<uint64, std::size_t> indices;
        for (std::size_t i = 0; i < entities.size(); ++i)
            if (entities[i].Id == 0 || !indices.emplace(entities[i].Id, i).second)
                return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Id") };

        for (std::size_t i = 0; i < entities.size(); ++i)
        {
            const auto& entity = entities[i];
            if (entity.Parent != 0)
            {
                const auto found = indices.find(entity.Parent);
                if (found == indices.end() || !entities[found->second].HasHierarchy ||
                    !SupportsHierarchyLink(world, entity.Handle, entities[found->second].Handle))
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Parent") };
                const auto& siblings = entities[found->second].Children;
                if (std::count(siblings.begin(), siblings.end(), entity.Id) != 1)
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Parent") };
            }
            std::unordered_set<uint64> children;
            for (const uint64 child : entity.Children)
            {
                const auto found = indices.find(child);
                if (found == indices.end() || !children.insert(child).second ||
                    !entities[found->second].HasHierarchy || entities[found->second].Parent != entity.Id)
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Children") };
            }
        }

        // Validate the graph before the TransformSystem's recursive traversal sees it.
        std::vector<uint8> states(entities.size(), 0);
        std::vector<std::size_t> depths(entities.size(), 0);
        for (std::size_t i = 0; i < entities.size(); ++i)
        {
            if (states[i] == 2) continue;
            std::vector<std::size_t> chain;
            std::size_t current = i;
            while (current != entities.size() && states[current] == 0)
            {
                states[current] = 1;
                chain.push_back(current);
                const uint64 parent = entities[current].Parent;
                current = parent == 0 ? entities.size() : indices.at(parent);
            }
            if (current != entities.size() && states[current] == 1)
                return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Parent") };
            std::size_t depth = current == entities.size() ? 0 : depths[current];
            for (auto node = chain.rbegin(); node != chain.rend(); ++node)
            {
                if (++depth > MaxHierarchyDepth)
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(*node, "Hierarchy.Parent") };
                depths[*node] = depth;
                states[*node] = 2;
            }
        }
        return {};
    }

    archive::ArchiveResult CollectComponents(const entt::registry& world, SceneEntityRecord& entity)
    {
        const auto& registry = Engine::ReflectionRegistry::Get();
        for (auto&& [storageId, storage] : world.storage())
        {
            if (!storage.contains(entity.Handle) || storageId == entt::type_hash<Engine::IDComponent>::value() ||
                storageId == entt::type_hash<Engine::HierarchyComponent>::value())
                continue;
            const auto* entry = registry.FindComponent(storageId);
            if (entry == nullptr || entry->ReflectionType == nullptr)
                return { archive::ArchiveErrorCode::UnsupportedType, std::string(storage.info().name()) };
            if (!entry->MatchesStorage(world))
                return { archive::ArchiveErrorCode::InvalidOperation, entry->ReflectionType->QualifiedName };
            bool enabled = true;
            auto result = archive_reflection::IsSerializationEnabled(
                entry->ReflectionType->Metadata, entry->ReflectionType->QualifiedName, enabled);
            if (!result) return result;
            if (enabled) entity.Components.push_back(entry);
        }
        std::sort(entity.Components.begin(), entity.Components.end(), [](const auto* left, const auto* right)
        {
            return left->ReflectionType->QualifiedName < right->ReflectionType->QualifiedName;
        });
        return {};
    }

    archive::ArchiveResult ReadComponents(archive::IArchive& ar, entt::registry& world, entt::entity handle)
    {
        const auto& registry = Engine::ReflectionRegistry::Get();
        std::unordered_set<std::string> types;
        return ar.ReadObjectArray("Components", [&](archive::IArchive& component, std::size_t)
            -> archive::ArchiveResult
        {
            std::string type;
            auto result = component.Read("Type", type);
            if (!result) return result;
            const auto* entry = registry.FindComponent(std::string_view(type));
            if (entry == nullptr)
                return { archive::ArchiveErrorCode::UnsupportedType, "Type" };
            if (!types.insert(type).second || entry->Type == std::type_index(typeid(Engine::IDComponent)))
                return { archive::ArchiveErrorCode::InvalidValue, "Type" };
            bool enabled = true;
            result = archive_reflection::IsSerializationEnabled(entry->ReflectionType->Metadata, "Type", enabled);
            if (!result) return result;
            if (!enabled) return { archive::ArchiveErrorCode::InvalidValue, "Type" };
            return registry.ReadComponent(component, "Data", world, handle, entry->Type);
        });
    }
}

BEGIN(Engine)

archive::ArchiveResult Scene::WriteArchive(archive::IArchive& ar) const
{
    if (!ReflectionRegistry::IsCreated() || !MatchesStorage<IDComponent>(m_LocalRegistry) ||
        !MatchesStorage<HierarchyComponent>(m_LocalRegistry))
        return { archive::ArchiveErrorCode::InvalidOperation, "Scene" };

    std::vector<SceneEntityRecord> entities;
    std::unordered_map<entt::entity, uint64> ids;
    const auto* storage = m_LocalRegistry.storage<entt::entity>();
    if (storage != nullptr)
    {
        for (auto [handle] : storage->each())
        {
            const auto wrapper = m_EntityMap.find(handle);
            const auto* identity = m_LocalRegistry.try_get<IDComponent>(handle);
            if (wrapper == m_EntityMap.end() || identity == nullptr || identity->id == 0)
                return { archive::ArchiveErrorCode::InvalidValue, EntityField(entities.size(), "Id") };
            SceneEntityRecord entity;
            entity.Id = identity->id;
            entity.Handle = handle;
            ids.emplace(handle, identity->id);
            entities.push_back(std::move(entity));
        }
    }
    std::sort(entities.begin(), entities.end(), [](const auto& left, const auto& right) { return left.Id < right.Id; });
    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        auto& entity = entities[i];
        if (const auto* hierarchy = m_LocalRegistry.try_get<HierarchyComponent>(entity.Handle))
        {
            entity.HasHierarchy = true;
            if (hierarchy->parent != entt::null)
            {
                const auto parent = ids.find(hierarchy->parent);
                if (parent == ids.end())
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Parent") };
                entity.Parent = parent->second;
            }
            for (const auto childHandle : hierarchy->children)
            {
                const auto child = ids.find(childHandle);
                if (child == ids.end())
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Children") };
                entity.Children.push_back(child->second);
            }
        }
        auto result = CollectComponents(m_LocalRegistry, entity);
        if (!result)
        {
            result.Field = EntityField(i, "Components") + "." + result.Field;
            return result;
        }
    }
    auto result = ValidateHierarchy(entities, m_LocalRegistry);
    if (!result) return result;

    return ar.WriteObject("Scene", entities, [&](archive::IArchive& object, const auto&) -> archive::ArchiveResult
    {
        auto written = object.Write("Version", SceneArchiveVersion);
        if (written) written = object.Write("Name", m_Name);
        if (written) written = object.Write("Active", IsActive());
        if (!written) return written;
        return object.WriteObjectArray("Entities", entities, [&](archive::IArchive& value, const auto& entity)
            -> archive::ArchiveResult
        {
            auto field = value.Write("Id", entity.Id);
            if (!field) return field;
            if (entity.HasHierarchy)
            {
                field = value.WriteObject("Hierarchy", entity, [](archive::IArchive& relation, const auto& record)
                    -> archive::ArchiveResult
                {
                    auto link = relation.Write("Parent", record.Parent);
                    if (link) link = relation.WriteArray("Children", record.Children);
                    return link;
                });
                if (!field) return field;
            }
            return value.WriteObjectArray("Components", entity.Components,
                [&](archive::IArchive& component, const auto* entry) -> archive::ArchiveResult
            {
                auto data = component.Write("Type", entry->ReflectionType->QualifiedName);
                if (data) data = ReflectionRegistry::Get().WriteComponent(
                    component, "Data", m_LocalRegistry, entity.Handle, entry->Type);
                return data;
            });
        });
    });
}

archive::ArchiveResult Scene::CreateFromArchive(const archive::IArchive& ar, Scene*& scene)
{
    if (scene != nullptr || !ReflectionRegistry::IsCreated() || ar.GetObjectDepth() != 0)
        return { archive::ArchiveErrorCode::InvalidOperation, "Scene" };
    std::unique_ptr<Scene, SceneDeleter> candidate(Scene::Create());
    if (candidate == nullptr)
        return { archive::ArchiveErrorCode::InvalidOperation, "Scene" };
    std::vector<SceneEntityRecord> entities;
    std::unordered_set<uint64> ids;
    bool unused = false;
    auto result = ar.ReadObject("Scene", unused, [&](archive::IArchive& object, bool&) -> archive::ArchiveResult
    {
        uint32 version = 0;
        auto field = object.Read("Version", version);
        if (!field) return field;
        if (version != SceneArchiveVersion)
            return { archive::ArchiveErrorCode::InvalidValue, "Version" };
        field = object.Read("Name", candidate->m_Name);
        bool active = true;
        if (field) field = object.Read("Active", active);
        if (!field) return field;
        candidate->SetActive(active);
        return object.ReadObjectArray("Entities", [&](archive::IArchive& value, std::size_t)
            -> archive::ArchiveResult
        {
            SceneEntityRecord entity;
            auto data = value.Read("Id", entity.Id);
            if (!data) return data;
            if (entity.Id == 0 || !ids.insert(entity.Id).second)
                return { archive::ArchiveErrorCode::InvalidValue, "Id" };
            entity.Handle = candidate->CreateEntity().GetEntityHandle();
            candidate->m_LocalRegistry.get<IDComponent>(entity.Handle).id = entity.Id;
            data = value.HasField("Hierarchy", entity.HasHierarchy);
            if (!data) return data;
            if (entity.HasHierarchy)
            {
                bool relation = false;
                data = value.ReadObject("Hierarchy", relation, [&](archive::IArchive& hierarchy, bool&)
                    -> archive::ArchiveResult
                {
                    auto link = hierarchy.Read("Parent", entity.Parent);
                    if (link) link = hierarchy.ReadArray("Children", entity.Children);
                    return link;
                });
                if (!data) return data;
            }
            data = ReadComponents(value, candidate->m_LocalRegistry, entity.Handle);
            if (data) entities.push_back(std::move(entity));
            return data;
        });
    });
    if (!result) return result;
    result = ValidateHierarchy(entities, candidate->m_LocalRegistry);
    if (!result) return result;
    std::unordered_map<uint64, entt::entity> handles;
    for (const auto& entity : entities) handles.emplace(entity.Id, entity.Handle);
    for (const auto& entity : entities)
    {
        if (!entity.HasHierarchy) continue;
        auto& hierarchy = candidate->m_LocalRegistry.emplace<HierarchyComponent>(entity.Handle);
        if (entity.Parent != 0) hierarchy.parent = handles.at(entity.Parent);
        for (const uint64 child : entity.Children) hierarchy.children.push_back(handles.at(child));
    }
    if (!TransformSystem::UpdateHierarchy(candidate->m_LocalRegistry))
        return { archive::ArchiveErrorCode::InvalidValue, "Entities.Transform" };
    scene = candidate.release();
    return {};
}

archive::ArchiveResult Scene::WritePrefabArchive(archive::IArchive& ar, const Entity& rootEntity) const
{
    if (!ReflectionRegistry::IsCreated() || !MatchesStorage<IDComponent>(m_LocalRegistry) ||
        !MatchesStorage<HierarchyComponent>(m_LocalRegistry))
        return { archive::ArchiveErrorCode::InvalidOperation, "Prefab" };

    const entt::entity rootHandle = rootEntity.GetEntityHandle();
    if (rootEntity.GetScene() != this || !m_LocalRegistry.valid(rootHandle) ||
        m_EntityMap.find(rootHandle) == m_EntityMap.end())
        return { archive::ArchiveErrorCode::InvalidOperation, "Prefab.Root" };

    std::vector<SceneEntityRecord> entities;
    std::unordered_map<entt::entity, uint64> subtreeIds;
    std::vector<std::pair<entt::entity, std::size_t>> stack{ { rootHandle, 1 } };

    while (!stack.empty())
    {
        const auto [current, depth] = stack.back();
        stack.pop_back();
        if (depth > MaxHierarchyDepth || !m_LocalRegistry.valid(current) ||
            m_EntityMap.find(current) == m_EntityMap.end())
            return { archive::ArchiveErrorCode::InvalidValue, "Prefab.Hierarchy" };

        const auto* identity = m_LocalRegistry.try_get<IDComponent>(current);
        if (identity == nullptr || identity->id == 0)
            return { archive::ArchiveErrorCode::InvalidValue, EntityField(entities.size(), "Id") };

        const uint64 localId = static_cast<uint64>(entities.size() + 1);
        if (!subtreeIds.emplace(current, localId).second)
            return { archive::ArchiveErrorCode::InvalidValue, "Prefab.Hierarchy" };

        SceneEntityRecord record;
        record.Id = localId;
        record.Handle = current;
        entities.push_back(std::move(record));

        if (const auto* hierarchy = m_LocalRegistry.try_get<HierarchyComponent>(current))
        {
            for (auto it = hierarchy->children.rbegin(); it != hierarchy->children.rend(); ++it)
                stack.emplace_back(*it, depth + 1);
        }
    }

    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        auto& entity = entities[i];
        if (const auto* hierarchy = m_LocalRegistry.try_get<HierarchyComponent>(entity.Handle))
        {
            entity.HasHierarchy = (i > 0) || !hierarchy->children.empty();
            if (i > 0)
            {
                const auto parent = subtreeIds.find(hierarchy->parent);
                if (parent == subtreeIds.end())
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Parent") };
                entity.Parent = parent->second;
            }
            for (const auto childHandle : hierarchy->children)
            {
                const auto child = subtreeIds.find(childHandle);
                if (child == subtreeIds.end())
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Children") };
                entity.Children.push_back(child->second);
            }
        }
        auto result = CollectComponents(m_LocalRegistry, entity);
        if (!result)
        {
            result.Field = EntityField(i, "Components") + "." + result.Field;
            return result;
        }
    }

    auto result = ValidateHierarchy(entities, m_LocalRegistry);
    if (!result) return result;

    return ar.WriteObject("Prefab", entities, [&](archive::IArchive& object, const auto&) -> archive::ArchiveResult
    {
        auto written = object.Write("Version", SceneArchiveVersion);
        if (written) written = object.Write("RootId", uint64(1));
        if (!written) return written;
        return object.WriteObjectArray("Entities", entities, [&](archive::IArchive& value, const auto& entity)
            -> archive::ArchiveResult
        {
            auto field = value.Write("Id", entity.Id);
            if (!field) return field;
            if (entity.HasHierarchy)
            {
                field = value.WriteObject("Hierarchy", entity, [](archive::IArchive& relation, const auto& record)
                    -> archive::ArchiveResult
                {
                    auto link = relation.Write("Parent", record.Parent);
                    if (link) link = relation.WriteArray("Children", record.Children);
                    return link;
                });
                if (!field) return field;
            }
            return value.WriteObjectArray("Components", entity.Components,
                [&](archive::IArchive& component, const auto* entry) -> archive::ArchiveResult
            {
                auto data = component.Write("Type", entry->ReflectionType->QualifiedName);
                if (data) data = ReflectionRegistry::Get().WriteComponent(
                    component, "Data", m_LocalRegistry, entity.Handle, entry->Type);
                return data;
            });
        });
    });
}

archive::ArchiveResult Scene::InstantiatePrefabArchive(
    const archive::IArchive& ar, Entity*& outRootEntity, Entity* parent)
{
    if (outRootEntity != nullptr || !ReflectionRegistry::IsCreated() || ar.GetObjectDepth() != 0 ||
        !MatchesStorage<IDComponent>(m_LocalRegistry) || !MatchesStorage<HierarchyComponent>(m_LocalRegistry))
        return { archive::ArchiveErrorCode::InvalidOperation, "Prefab" };

    if (parent != nullptr && (parent->GetScene() != this || !m_LocalRegistry.valid(parent->GetEntityHandle()) ||
        m_EntityMap.find(parent->GetEntityHandle()) == m_EntityMap.end()))
        return { archive::ArchiveErrorCode::InvalidOperation, "Prefab.Parent" };

    std::unique_ptr<Scene, SceneDeleter> staging(Scene::Create());
    if (staging == nullptr)
        return { archive::ArchiveErrorCode::InvalidOperation, "Prefab" };

    std::vector<SceneEntityRecord> entities;
    std::unordered_set<uint64> ids;
    uint64 rootId = 0;
    bool unused = false;
    auto result = ar.ReadObject("Prefab", unused, [&](archive::IArchive& object, bool&) -> archive::ArchiveResult
    {
        uint32 version = 0;
        auto field = object.Read("Version", version);
        if (!field) return field;
        if (version != SceneArchiveVersion)
            return { archive::ArchiveErrorCode::InvalidValue, "Version" };
        field = object.Read("RootId", rootId);
        if (!field) return field;
        if (rootId == 0)
            return { archive::ArchiveErrorCode::InvalidValue, "RootId" };
        return object.ReadObjectArray("Entities", [&](archive::IArchive& value, std::size_t)
            -> archive::ArchiveResult
        {
            SceneEntityRecord entity;
            auto data = value.Read("Id", entity.Id);
            if (!data) return data;
            if (entity.Id == 0 || !ids.insert(entity.Id).second)
                return { archive::ArchiveErrorCode::InvalidValue, "Id" };
            entity.Handle = staging->CreateEntity().GetEntityHandle();
            staging->m_LocalRegistry.get<IDComponent>(entity.Handle).id = entity.Id;
            data = value.HasField("Hierarchy", entity.HasHierarchy);
            if (!data) return data;
            if (entity.HasHierarchy)
            {
                bool relation = false;
                data = value.ReadObject("Hierarchy", relation, [&](archive::IArchive& hierarchy, bool&)
                    -> archive::ArchiveResult
                {
                    auto link = hierarchy.Read("Parent", entity.Parent);
                    if (link) link = hierarchy.ReadArray("Children", entity.Children);
                    return link;
                });
                if (!data) return data;
            }
            data = ReadComponents(value, staging->m_LocalRegistry, entity.Handle);
            if (data) entities.push_back(std::move(entity));
            return data;
        });
    });
    if (!result) return result;
    if (entities.empty())
        return { archive::ArchiveErrorCode::InvalidValue, "Entities" };

    result = ValidateHierarchy(entities, staging->m_LocalRegistry);
    if (!result) return result;

    std::size_t rootIndex = entities.size();
    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        if (entities[i].Id == rootId)
            rootIndex = i;
        else if (entities[i].Parent == 0)
            return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Parent") };
    }
    if (rootIndex == entities.size() || entities[rootIndex].Parent != 0)
        return { archive::ArchiveErrorCode::InvalidValue, "RootId" };

    for (auto&& [storageId, storage] : staging->m_LocalRegistry.storage())
    {
        if (storageId == entt::type_hash<IDComponent>::value() ||
            storageId == entt::type_hash<HierarchyComponent>::value())
            continue;
        const auto* entry = ReflectionRegistry::Get().FindComponent(storageId);
        if (entry == nullptr || !entry->MatchesStorage(m_LocalRegistry))
            return { archive::ArchiveErrorCode::InvalidOperation, "Prefab.Storage" };
    }

    const uint64 savedNextId = m_NextEntityId;
    std::vector<Entity*> createdEntities;
    createdEntities.reserve(entities.size());
    std::unordered_map<uint64, Entity*> prefabToLive;

    const auto rollback = [&]()
    {
        for (auto it = createdEntities.rbegin(); it != createdEntities.rend(); ++it)
            static_cast<void>(DestroyEntity(**it));
        m_NextEntityId = savedNextId;
    };

    for (const auto& record : entities)
    {
        Entity& live = CreateEntity();
        createdEntities.push_back(&live);
        prefabToLive.emplace(record.Id, &live);
        const entt::entity liveHandle = live.GetEntityHandle();
        for (auto&& [storageId, storage] : staging->m_LocalRegistry.storage())
        {
            if (!storage.contains(record.Handle) || storageId == entt::type_hash<IDComponent>::value() ||
                storageId == entt::type_hash<HierarchyComponent>::value())
                continue;
            const auto* entry = ReflectionRegistry::Get().FindComponent(storageId);
            if (entry == nullptr || !entry->Copy(staging->m_LocalRegistry, record.Handle, m_LocalRegistry, liveHandle))
            {
                rollback();
                return { archive::ArchiveErrorCode::InvalidOperation, "Prefab.Copy" };
            }
        }
    }

    for (const auto& record : entities)
    {
        if (!record.HasHierarchy) continue;
        const entt::entity liveHandle = prefabToLive.at(record.Id)->GetEntityHandle();
        auto& hierarchy = m_LocalRegistry.emplace<HierarchyComponent>(liveHandle);
        if (record.Parent != 0)
            hierarchy.parent = prefabToLive.at(record.Parent)->GetEntityHandle();
        for (const uint64 childId : record.Children)
            hierarchy.children.push_back(prefabToLive.at(childId)->GetEntityHandle());
    }

    Entity* liveRoot = prefabToLive.at(rootId);
    if (parent != nullptr)
    {
        const auto reparentResult = ReparentEntity(*liveRoot, parent);
        if (reparentResult != EntityReparentResult::Success && reparentResult != EntityReparentResult::Unchanged)
        {
            rollback();
            return { archive::ArchiveErrorCode::InvalidValue, "Prefab.Parent" };
        }
    }
    else if (!TransformSystem::UpdateHierarchy(m_LocalRegistry))
    {
        rollback();
        return { archive::ArchiveErrorCode::InvalidValue, "Entities.Transform" };
    }

    outRootEntity = liveRoot;
    return {};
}

END
