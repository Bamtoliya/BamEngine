#include "Scene.h"
#include "WorldComponents.h"
#include "TransformSystem.h"
#include "UIComponents.h"
#include "PrefabInstanceComponent.h"
#include "SkyAtmosphereComponent.h"
#include "VolumetricCloudComponent.h"
#include <archive/ArchiveInterface.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "ResourceHandleArchiveAdapter.h"
#include "ResourcePreloader.h"
#include "IBLManager.h"
#include "NavigationSystem.h"

namespace Engine
{
    struct EntityIdRemapContext
    {
        const std::unordered_map<entt::entity, uint64>* EntityToPersistentId = nullptr;
        const std::unordered_map<uint64, entt::entity>* PersistentIdToEntity = nullptr;
        bool IsPrefab = false;

        static void SetActiveContext(const EntityIdRemapContext* context) noexcept
        {
            s_ActiveContext = context;
        }

        static const EntityIdRemapContext* GetActiveContext() noexcept
        {
            return s_ActiveContext;
        }

    private:
        inline static thread_local const EntityIdRemapContext* s_ActiveContext = nullptr;
    };

    struct EntityIdRemapScope
    {
        explicit EntityIdRemapScope(const EntityIdRemapContext* context)
            : m_Previous(EntityIdRemapContext::GetActiveContext())
        {
            EntityIdRemapContext::SetActiveContext(context);
        }

        ~EntityIdRemapScope()
        {
            EntityIdRemapContext::SetActiveContext(m_Previous);
        }

    private:
        const EntityIdRemapContext* m_Previous = nullptr;
    };
}

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

    archive::ArchiveResult ValidateHierarchy(const std::vector<SceneEntityRecord>& entities,const entt::registry& world)
    {
        std::unordered_map<uint64, std::size_t> indices;
        indices.reserve(entities.size());
        for (std::size_t i = 0; i < entities.size(); ++i)
            if (entities[i].Id == 0 || !indices.emplace(entities[i].Id, i).second)
                return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Id") };

        std::vector<std::size_t> parentReferences(entities.size(), 0);
        for (const auto& parent : entities)
        {
            for (const uint64 child : parent.Children)
            {
                const auto found = indices.find(child);
                if (found != indices.end() && entities[found->second].Parent == parent.Id)
                    ++parentReferences[found->second];
            }
        }

        for (std::size_t i = 0; i < entities.size(); ++i)
        {
            const auto& entity = entities[i];
            if (entity.Parent != 0)
            {
                const auto found = indices.find(entity.Parent);
                if (found == indices.end() || !entities[found->second].HasHierarchy ||
                    !SupportsHierarchyLink(world, entity.Handle, entities[found->second].Handle))
                    return { archive::ArchiveErrorCode::InvalidValue, EntityField(i, "Hierarchy.Parent") };
                if (parentReferences[i] != 1)
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

    archive::ArchiveResult CollectComponents(const entt::registry& world, SceneEntityRecord& entity, bool isPrefabRoot = false)
    {
        const auto& registry = Engine::ReflectionRegistry::Get();
        for (auto&& [storageId, storage] : world.storage())
        {
            if (!storage.contains(entity.Handle) || storageId == entt::type_hash<Engine::IDComponent>::value() ||
                storageId == entt::type_hash<Engine::HierarchyComponent>::value() ||
                (isPrefabRoot && storageId == entt::type_hash<Engine::PrefabInstanceComponent>::value()))
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

    archive::ArchiveResult WriteEntityRecords(archive::IArchive& object, const entt::registry& world, const std::vector<SceneEntityRecord>& entities)
    {
        return object.WriteObjectArray("Entities", entities, [&](archive::IArchive& value, const SceneEntityRecord& entity)
            -> archive::ArchiveResult
            {
                auto field = value.Write("Id", entity.Id);
                if (!field) return field;

                if (entity.HasHierarchy)
                {
                    field = value.WriteObject("Hierarchy", entity, [](archive::IArchive& relation, const SceneEntityRecord& record)
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
                        if (data && !Engine::IsComponentEnabled(world, entity.Handle, entry->StorageId))
                            data = component.Write("Enabled", false);
                        if (data) data = Engine::ReflectionRegistry::Get().WriteComponent(
                            component, "Data", world, entity.Handle, entry->Type);
                        return data;
                    });
            });
    }

    // Sky Atmosphere에서 구름을 설정하던 이전 데이터
    struct LegacySkyClouds
    {
        entt::entity Entity = entt::null;
        bool Enabled = false;
        f32 Coverage = 0.45f;
        f32 Density = 0.8f;
        f32 Speed = 0.03f;
        f32 Altitude = 2500.0f;
    };

    archive::ArchiveResult ReadLegacySkyClouds(const archive::IArchive& component, entt::entity handle, std::vector<LegacySkyClouds>& clouds)
    {
        LegacySkyClouds legacy;
        const auto result = component.ReadObject("Data", legacy, [](const archive::IArchive& data, LegacySkyClouds& value)
            -> archive::ArchiveResult
            {
                const auto readOptional = [&](const char* key, auto& field) -> archive::ArchiveResult
                    {
                        bool exists = false;
                        const auto found = data.HasField(key, exists);
                        return found && exists ? data.Read(key, field) : found;
                    };
                auto field = readOptional("EnableClouds", value.Enabled);
                if (field) field = readOptional("CloudCoverage", value.Coverage);
                if (field) field = readOptional("CloudDensity", value.Density);
                if (field) field = readOptional("CloudSpeed", value.Speed);
                if (field) field = readOptional("CloudAltitude", value.Altitude);
                return field;
            });
        if (result && legacy.Enabled)
        {
            legacy.Entity = handle;
            clouds.push_back(legacy);
        }
        return result;
    }

    bool HasActiveVolumetricCloud(const entt::registry& world)
    {
        for (const auto entity : world.view<const Engine::VolumetricCloudComponent>())
        {
            if (Engine::IsComponentEnabled<Engine::VolumetricCloudComponent>(world, entity)) return true;
        }
        return false;
    }

    // 이전 렌더러처럼 활성 Volumetric Cloud가 없을 때 첫 활성 Sky Atmosphere의 구름만 옮긴다.
    void MigrateLegacySkyClouds(entt::registry& world, const std::vector<LegacySkyClouds>& clouds, const entt::registry* targetWorld)
    {
        if (clouds.empty() || HasActiveVolumetricCloud(world) || (targetWorld && HasActiveVolumetricCloud(*targetWorld))) return;

        for (const auto entity : world.view<Engine::SkyAtmosphereComponent>())
        {
            if (!Engine::IsComponentEnabled<Engine::SkyAtmosphereComponent>(world, entity)) continue;

            const auto found = std::ranges::find(clouds, entity, &LegacySkyClouds::Entity);
            if (found == clouds.end()) return;

            auto& cloud = world.get_or_emplace<Engine::VolumetricCloudComponent>(entity);
            cloud.IsEnabled = true;
            cloud.CloudCoverage = found->Coverage;
            cloud.CloudDensity = found->Density;
            cloud.LayerBottomAltitude = found->Altitude;
            cloud.WindSpeed = found->Speed * 100.0f;
            Engine::detail::SetComponentEnabled<Engine::VolumetricCloudComponent>(world, entity, true);
            ENGINE_LOG_INFO("Moved Sky Atmosphere cloud settings to a Volumetric Cloud component. Save the document to keep the change.");
            return;
        }
    }

    archive::ArchiveResult ReadComponents(archive::IArchive& ar, entt::registry& world, entt::entity handle, std::vector<LegacySkyClouds>& legacyClouds)
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
            bool hasEnableState = false;
            result = component.HasField("Enabled", hasEnableState);
            if (!result) return result;
            bool componentEnabled = true;
            if (hasEnableState)
            {
                result = component.Read("Enabled", componentEnabled);
                if (!result) return result;
                if (!entry->CanDisable())
                    return { archive::ArchiveErrorCode::InvalidValue, "Enabled" };
            }
            auto* recovery = Engine::MissingResourceRecoveryContext::GetActiveContext();
            if (recovery != nullptr)
            {
                recovery->CurrentComponentType = type;
                recovery->CurrentComponentDisplayName = entry->Name;
            }
            result = registry.ReadComponent(component, "Data", world, handle, entry->Type);
            if (recovery != nullptr)
            {
                recovery->CurrentComponentType.clear();
                recovery->CurrentComponentDisplayName.clear();
            }
            if (result && hasEnableState && !entry->SetEnabled(world, handle, componentEnabled))
                return { archive::ArchiveErrorCode::InvalidValue, "Enabled" };
            if (result && entry->Type == std::type_index(typeid(Engine::SkyAtmosphereComponent)))
                result = ReadLegacySkyClouds(component, handle, legacyClouds);
            return result;
        });
    }

    template<typename CreateEntity>
    archive::ArchiveResult ReadEntityRecords(const archive::IArchive& object, entt::registry& world, uint64& nextEntityId, std::vector<SceneEntityRecord>& entities, CreateEntity&& createEntity, const entt::registry* targetWorld = nullptr)
    {
        std::unordered_set<uint64> ids;
        std::unordered_map<uint64, entt::entity> idToHandle;

        auto prepass = object.ReadObjectArray("Entities", [&](archive::IArchive& value, std::size_t) -> archive::ArchiveResult
            {
                uint64 id = 0;
                auto data = value.Read("Id", id);
                if (!data) return data;
                if (id == 0 || ids.contains(id))
                    return { archive::ArchiveErrorCode::InvalidValue, "Id" };

                while (ids.contains(nextEntityId))
                {
                    nextEntityId = nextEntityId == (std::numeric_limits<uint64>::max)() ? 1 : nextEntityId + 1;
                }

                const entt::entity handle = createEntity(nextEntityId);
                world.get<Engine::IDComponent>(handle).id = id;
                ids.insert(id);
                idToHandle.emplace(id, handle);
                return {};
            });
        if (!prepass) return prepass;

        Engine::EntityIdRemapContext remapContext;
        remapContext.PersistentIdToEntity = &idToHandle;
        Engine::EntityIdRemapScope remapScope(&remapContext);

        std::vector<LegacySkyClouds> legacyClouds;
        const auto result = object.ReadObjectArray("Entities", [&](archive::IArchive& value, std::size_t) -> archive::ArchiveResult
            {
                SceneEntityRecord entity;
                auto data = value.Read("Id", entity.Id);
                if (!data) return data;

                entity.Handle = idToHandle.at(entity.Id);

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

                auto* recovery = Engine::MissingResourceRecoveryContext::GetActiveContext();
                const std::size_t missingStart = recovery != nullptr ? recovery->Entries.size() : 0;
                if (recovery != nullptr)
                {
                    recovery->CurrentEntityPersistentId = entity.Id;
                    recovery->CurrentEntityName.clear();
                }

                data = ReadComponents(value, world, entity.Handle, legacyClouds);

                if (recovery != nullptr)
                {
                    if (const auto* nameComp = world.try_get<Engine::NameComponent>(entity.Handle);
                        nameComp != nullptr && !nameComp->name.empty())
                    {
                        for (std::size_t idx = missingStart; idx < recovery->Entries.size(); ++idx)
                            recovery->Entries[idx].EntityName = nameComp->name;
                    }
                    recovery->CurrentEntityPersistentId = 0;
                    recovery->CurrentEntityName.clear();
                    recovery->CurrentComponentType.clear();
                    recovery->CurrentComponentDisplayName.clear();
                }

                if (data) entities.push_back(std::move(entity));
                return data;
            });
        if (result) MigrateLegacySkyClouds(world, legacyClouds, targetWorld);
        return result;
    }

    template<typename Writer>
    archive::ArchiveResult WriteDocumentWithResourceReferences(archive::IArchive& ar, std::string_view key, Writer&& writeDocument)
    {
        Engine::ResourceReferenceArchiveContext context;
        context.WritingPrefab = key == "Prefab";
        const auto result = ar.WriteObject(key, context,
            [&](archive::IArchive& object, const auto&) -> archive::ArchiveResult
            {
                object.SetWriteContext(&context);

                auto written = writeDocument(object);
                if (written)
                    written = archive_resource::WriteResourceReferences(object, "ResourceReferences", context.References);

                return written;
            });

        if (result)
        {
            if (auto* parent = ar.GetWriteContext<Engine::ResourceReferenceArchiveContext>())
                parent->References.insert(context.References.begin(), context.References.end());
            else if (auto* destination = ar.GetWriteContext<Engine::ResourceReferenceSet>())
                destination->insert(context.References.begin(), context.References.end());
        }

        return result;
    }

    void PreloadDocumentResources(std::string_view kind, const Engine::ResourceReferenceSet& references)
    {
        const std::vector<Engine::ResourceReference> ordered(references.begin(), references.end());
        Engine::ResourcePreloader::LoadBlocking(ordered);
        const auto unresolved = archive_resource::PreloadResourceReferences(references);

        for (const auto& reference : unresolved)
        {
            if ((reference.AssetKey == L"Engine://Texture/DefaultBRDFLut" ||
                 reference.AssetPath == L"Engine://Texture/DefaultBRDFLut") &&
                Engine::IBLManager::Get().GetDefaultBRDFLUT())
            {
                continue;
            }
            ENGINE_LOG_WARN("{} resource preload deferred: key=\"{}\", path=\"{}\"", kind,
                Engine::WStrToStr(reference.AssetKey), Engine::WStrToStr(reference.AssetPath));
        }
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

    EntityIdRemapContext remapContext;
    remapContext.EntityToPersistentId = &ids;
    remapContext.IsPrefab = false;
    EntityIdRemapScope remapScope(&remapContext);

    return WriteDocumentWithResourceReferences(ar, "Scene", [&](archive::IArchive& object) -> archive::ArchiveResult
    {
        auto written = object.Write("Version", SceneArchiveVersion);
        if (written) written = object.Write("Name", m_Name);
        if (written) written = object.Write("Active", IsActive());
        if (written && !m_NavGridPath.empty()) written = object.Write("NavGridPath", m_NavGridPath);
        if (written && !m_NavMeshPath.empty()) written = object.Write("NavMeshPath", m_NavMeshPath);
        if (!written) return written;
        return WriteEntityRecords(object, m_LocalRegistry, entities);
    });
}

archive::ArchiveResult Scene::CreateFromArchive(const archive::IArchive& ar, Scene*& scene)
{
    if (scene != nullptr || !ReflectionRegistry::IsCreated() || ar.GetObjectDepth() != 0)
        return { archive::ArchiveErrorCode::InvalidOperation, "Scene" };
    std::unique_ptr<Scene, SceneDeleter> candidate(Scene::Create());
    if (candidate == nullptr)
        return { archive::ArchiveErrorCode::InvalidOperation, "Scene" };
    candidate->m_LocalRegistry.ctx().emplace<DeferredComponentTransformUpdate>();
    std::vector<SceneEntityRecord> entities;
    bool unused = false;
    ResourceReferenceSet resourceReferences;
    auto result = ar.ReadObject("Scene", unused, [&](archive::IArchive& object, bool&) -> archive::ArchiveResult
    {
        uint32 version = 0;
        auto field = object.Read("Version", version);
        if (!field) return field;
        if (version != SceneArchiveVersion)
            return { archive::ArchiveErrorCode::InvalidValue, "Version" };
        field = archive_resource::ReadResourceReferences(object, "ResourceReferences", resourceReferences);
        if (!field) return field;
        field = object.Read("Name", candidate->m_Name);
        bool active = true;
        if (field) field = object.Read("Active", active);
        bool hasNavGridPath = false;
        if (field) field = object.HasField("NavGridPath", hasNavGridPath);
        if (field && hasNavGridPath) field = object.Read("NavGridPath", candidate->m_NavGridPath);
        bool hasNavMeshPath = false;
        if (field) field = object.HasField("NavMeshPath", hasNavMeshPath);
        if (field && hasNavMeshPath) field = object.Read("NavMeshPath", candidate->m_NavMeshPath);
        if (!field) return field;
        candidate->SetActive(active);
        PreloadDocumentResources("Scene", resourceReferences);
        return ReadEntityRecords(object, candidate->m_LocalRegistry, candidate->m_NextEntityId, entities,
            [&](uint64 id) { return candidate->CreateEntityWithId(id).GetEntityHandle(); });
    });
    if (!result) return result;
    candidate->m_LocalRegistry.ctx().erase<DeferredComponentTransformUpdate>();
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
    if (!candidate->m_NavGridPath.empty())
    {
        auto navGrid = std::make_shared<NavGrid>();
        if (navGrid->LoadFromFile(NavGrid::ResolveDataPath(candidate->m_NavGridPath)))
            NavigationSystem::SetSceneNavGrid(candidate.get(), std::move(navGrid));
    }
    if (!candidate->m_NavMeshPath.empty())
    {
        auto navMesh = std::make_shared<NavMesh>();
        if (navMesh->LoadFromFile(NavMesh::ResolveDataPath(candidate->m_NavMeshPath)))
            NavigationSystem::SetSceneNavMesh(candidate.get(), std::move(navMesh));
    }
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
            entity.HasHierarchy = (i > 0) || !hierarchy->children.empty() ||
                m_LocalRegistry.any_of<CanvasComponent, RectTransformComponent>(entity.Handle);
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
        const bool isRoot = (i == 0);
        auto result = CollectComponents(m_LocalRegistry, entity, isRoot);
        if (!result)
        {
            result.Field = EntityField(i, "Components") + "." + result.Field;
            return result;
        }
    }

    auto result = ValidateHierarchy(entities, m_LocalRegistry);
    if (!result) return result;

    EntityIdRemapContext remapContext;
    remapContext.EntityToPersistentId = &subtreeIds;
    remapContext.IsPrefab = true;
    EntityIdRemapScope remapScope(&remapContext);

    return WriteDocumentWithResourceReferences(ar, "Prefab", [&](archive::IArchive& object) -> archive::ArchiveResult
    {
        auto written = object.Write("Version", SceneArchiveVersion);
        if (written) written = object.Write("RootId", uint64(1));
        if (!written) return written;
        return WriteEntityRecords(object, m_LocalRegistry, entities);
    });
}

archive::ArchiveResult Scene::InstantiatePrefabArchive(const archive::IArchive& ar, Entity*& outRootEntity, Entity* parent)
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
    staging->m_LocalRegistry.ctx().emplace<DeferredComponentTransformUpdate>();
    std::vector<SceneEntityRecord> entities;
    uint64 rootId = 0;
    bool unused = false;
    ResourceReferenceSet resourceReferences;
    auto result = ar.ReadObject("Prefab", unused, [&](archive::IArchive& object, bool&) -> archive::ArchiveResult
    {
        uint32 version = 0;
        auto field = object.Read("Version", version);
        if (!field) return field;
        if (version != SceneArchiveVersion)
            return { archive::ArchiveErrorCode::InvalidValue, "Version" };
        field = archive_resource::ReadResourceReferences(object, "ResourceReferences", resourceReferences);
        if (!field) return field;
        field = object.Read("RootId", rootId);
        if (!field) return field;
        if (rootId == 0)
            return { archive::ArchiveErrorCode::InvalidValue, "RootId" };
        PreloadDocumentResources("Prefab", resourceReferences);
        return ReadEntityRecords(object, staging->m_LocalRegistry, staging->m_NextEntityId, entities,
            [&](uint64 id) { return staging->CreateEntityWithId(id).GetEntityHandle(); }, &m_LocalRegistry);
    });
    if (!result) return result;
    staging->m_LocalRegistry.ctx().erase<DeferredComponentTransformUpdate>();
    if (entities.empty())
        return { archive::ArchiveErrorCode::InvalidValue, "Entities" };

    result = ValidateHierarchy(entities, staging->m_LocalRegistry);
    if (!result) return result;

    if (!TransformSystem::UpdateHierarchy(staging->m_LocalRegistry))
        return { archive::ArchiveErrorCode::InvalidValue, "Entities.Transform" };

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
    std::vector<entt::entity> createdEntities = CreateEntities(entities.size());
    std::unordered_map<uint64, Entity*> prefabToLive;
    std::unordered_map<entt::entity, entt::entity> stagingToLive;

    const auto rollback = [&](archive::ArchiveResult failure) -> archive::ArchiveResult
        {
            for (auto it = createdEntities.rbegin(); it != createdEntities.rend(); ++it)
            {
                Entity* created = FindEntity(*it);
                if (created && !DestroyEntity(*created))
                    return { archive::ArchiveErrorCode::InvalidOperation, "Prefab.Rollback" };
            }

            m_NextEntityId = savedNextId;
            return failure;
        };

    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        const auto& record = entities[i];
        const entt::entity liveHandle = createdEntities[i];
        prefabToLive.emplace(record.Id, FindEntity(liveHandle));
        stagingToLive.emplace(record.Handle, liveHandle);

        for (auto&& [storageId, storage] : staging->m_LocalRegistry.storage())
        {
            if (!storage.contains(record.Handle) || storageId == entt::type_hash<IDComponent>::value() ||
                storageId == entt::type_hash<HierarchyComponent>::value())
                continue;

            const auto* entry = ReflectionRegistry::Get().FindComponent(storageId);
            if (entry == nullptr || !entry->Copy(staging->m_LocalRegistry, record.Handle, m_LocalRegistry, liveHandle))
                return rollback({ archive::ArchiveErrorCode::InvalidOperation, "Prefab.Copy" });
        }
    }

    // Remap internal reflected entt::entity references to newly created live entity handles
    for (std::size_t i = 0; i < entities.size(); ++i)
    {
        const entt::entity liveHandle = createdEntities[i];
        for (auto&& [storageId, storage] : m_LocalRegistry.storage())
        {
            if (!storage.contains(liveHandle) ||
                storageId == entt::type_hash<IDComponent>::value() ||
                storageId == entt::type_hash<HierarchyComponent>::value())
                continue;

            const auto* entry = ReflectionRegistry::Get().FindComponent(storageId);
            if (entry && entry->ReflectionType)
            {
                auto object = entry->Edit(m_LocalRegistry, liveHandle);
                if (object.IsValid())
                {
                    for (const auto& prop : entry->ReflectionType->Properties)
                    {
                        if (prop.IsValueType<entt::entity>() && prop.CanWrite())
                        {
                            auto valView = prop.TryReadValue(object);
                            if (const auto* currentEnt = valView.Value.Get<entt::entity>())
                            {
                                if (*currentEnt != entt::null)
                                {
                                    auto it = stagingToLive.find(*currentEnt);
                                    entt::entity newEnt = (it != stagingToLive.end()) ? it->second : entt::null;
                                    prop.TryWriteValue(object, reflection::ValueView::From(newEnt));
                                }
                            }
                        }
                    }
                }
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
    const auto liveRootHandle = liveRoot->GetEntityHandle();
    if (m_LocalRegistry.any_of<CanvasComponent, RectTransformComponent>(liveRootHandle))
        m_LocalRegistry.get_or_emplace<HierarchyComponent>(liveRootHandle);

    if (parent != nullptr)
    {
        const auto reparentResult = ReparentEntity(*liveRoot, parent);
        if (reparentResult != EntityReparentResult::Success && reparentResult != EntityReparentResult::Unchanged)
            return rollback({ archive::ArchiveErrorCode::InvalidValue, "Prefab.Parent" });
    }
    else if (!TransformSystem::UpdateHierarchy(m_LocalRegistry))
    {
        return rollback({ archive::ArchiveErrorCode::InvalidValue, "Entities.Transform" });
    }

    outRootEntity = liveRoot;
    return {};
}

END
