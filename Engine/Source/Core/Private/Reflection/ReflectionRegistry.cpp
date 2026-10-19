#include "ReflectionRegistry.h"
#include "BamEngine.gen.h"
#include "CoreComponents.h"
#include "Resources.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include "TransformSystem.h"
#include "UIComponents.h"
#include "Adapters/GlmArchiveAdapter.h"
#include "Adapters/ResourceHandleArchiveAdapter.h"

#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <entt/core/type_info.hpp>
#include <entt/entity/component.hpp>

#include <algorithm>
#include <iostream>
#include <utility>
#include <optional>

namespace
{
    template<typename T>
    bool AddComponentValue(entt::registry& world, entt::entity entity)
    {
        world.emplace<T>(entity);
        return true;
    }

    template<typename T>
    bool AddSpatialComponent(entt::registry& world, entt::entity entity)
    {
        static_cast<void>(world.get_or_emplace<Engine::TransformComponent>(entity));
        static_cast<void>(world.get_or_emplace<Engine::WorldTransformComponent>(entity));
        static_cast<void>(world.get_or_emplace<Engine::FlagComponent>(entity));
        static_cast<void>(world.get_or_emplace<T>(entity));
        Engine::TransformSystem::UpdateHierarchy(world);
        return true;
    }

    template<typename T, typename World>
    reflection::ObjectView ComponentObject(World& world, entt::entity entity)
    {
        if constexpr (entt::component_traits<T>::page_size != 0)
        {
            const auto* storage = world.storage(entt::type_hash<T>::value());
            const auto& expected = entt::type_id<T>();
            if (world.valid(entity) && storage != nullptr && storage->contains(entity) &&
                storage->info().hash() == expected.hash() && storage->info().name() == expected.name())
            {
                if (auto* value = world.template try_get<T>(entity))
                    return reflection::ObjectView::From(*value);
            }
        }
        return {};
    }

    template<typename T>
    bool ValidateComponent(T& value)
    {
        if constexpr (std::is_same_v<T, Engine::TransformComponent>)
        {
            return value.HasValidAbsoluteFlags() && value.SetLocalPosition(value.position) && value.SetLocalScale(value.scale) &&
                value.RestoreLocalRotation(value.rotation, value.eulerRotation);
        }
        else if constexpr (std::is_same_v<T, Engine::ColliderComponent> || std::is_same_v<T, Engine::Collider2DComponent>)
        {
            return value.SetOffset(value.GetOffset()) && value.SetSettings(value.GetSettings()) &&
                value.SetShape(value.GetShape());
        }
        else if constexpr (requires { value.InvalidateCache(); })
        {
            value.InvalidateCache();
        }
        return true;
    }

    template<typename Variant>
    archive::ArchiveResult WriteShapeVariant(archive::IArchive& ar, std::string_view key, const Variant& value,
        const reflection::Registry& registry, const archive_reflection::PropertyCodecs& codecs)
    {
        return ar.WriteObject(key, value, [&](archive::IArchive& object, const Variant& current) -> archive::ArchiveResult
        {
            return std::visit([&](const auto& alt) -> archive::ArchiveResult
            {
                using Alt = std::decay_t<decltype(alt)>;
                if (!alt.HasValidParameters())
                    return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };
                const auto* type = registry.FindType<Alt>();
                if (type == nullptr)
                    return { archive::ArchiveErrorCode::UnsupportedType, std::string(key) };
                auto res = object.Write("ShapeType", static_cast<uint8>(Alt::Type));
                if (res) res = archive_reflection::WriteObject(object, "ShapeData", *type,
                    reflection::ObjectView::From(alt), &registry, &codecs);
                return res;
            }, current);
        });
    }

    template<typename Alt, typename Variant>
    bool TryReadShapeAlternative(archive::IArchive& object, uint8 shapeType, Variant& candidate,
        const reflection::Registry& registry, const archive_reflection::PropertyCodecs& codecs, archive::ArchiveResult& result)
    {
        if (shapeType != static_cast<uint8>(Alt::Type))
            return false;
        const auto* type = registry.FindType<Alt>();
        if (type == nullptr)
        {
            result = { archive::ArchiveErrorCode::UnsupportedType, "ShapeData" };
            return true;
        }
        Alt alt{};
        result = archive_reflection::ReadObject(object, "ShapeData", *type, alt, &registry, &codecs);
        if (result && !alt.HasValidParameters())
            result = { archive::ArchiveErrorCode::InvalidValue, "ShapeData" };
        if (result)
            candidate = std::move(alt);
        return true;
    }

    template<typename Variant, typename... Alts>
    archive::ArchiveResult ReadShapeVariant(const archive::IArchive& ar, std::string_view key, Variant& value,
        const reflection::Registry& registry, const archive_reflection::PropertyCodecs& codecs)
    {
        Variant candidate = value;
        const auto result = ar.ReadObject(key, candidate, [&](archive::IArchive& object, Variant& target) -> archive::ArchiveResult
        {
            uint8 shapeType = 0;
            auto res = object.Read("ShapeType", shapeType);
            if (!res) return res;
            if ((TryReadShapeAlternative<Alts>(object, shapeType, target, registry, codecs, res) || ...))
                return res;
            return { archive::ArchiveErrorCode::InvalidValue, "ShapeType" };
        });
        if (result) value = std::move(candidate);
        return result;
    }

    template<typename T>
    archive::ArchiveResult ReadComponentValue(const archive::IArchive& ar, std::string_view key,
        entt::registry& world, entt::entity entity, const Engine::ComponentReflectionEntry& entry,
        const reflection::Registry& registry, const archive_reflection::PropertyCodecs* codecs)
    {
        if constexpr (entt::component_traits<T>::page_size != 0 &&
            std::copy_constructible<T> && std::assignable_from<T&, T>)
        {
            const auto* previous = world.try_get<T>(entity);
            const bool hadWorld = world.all_of<Engine::WorldTransformComponent>(entity);
            const bool hadFlags = world.all_of<Engine::FlagComponent>(entity);
            T candidate = previous != nullptr ? *previous : T{};
            auto result = archive_reflection::ReadObject(ar, key, *entry.ReflectionType, candidate, &registry, codecs);
            if (!result)
                return result;
            if (!ValidateComponent(candidate))
                return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };

            if (previous == nullptr && !entry.TryAdd(world, entity))
                return { archive::ArchiveErrorCode::InvalidOperation, std::string(key) };

            const std::optional<T> backup = previous ? std::optional<T>(*previous) : std::nullopt;
            auto& target = world.get<T>(entity);
            if constexpr (std::is_copy_assignable_v<T>)
                target = std::as_const(candidate);
            else
                target = std::move(candidate);

            if constexpr (std::is_same_v<T, Engine::TransformComponent>)
            {
                if (!Engine::TransformSystem::UpdateHierarchy(world))
                {
                    if (backup) target = *backup;
                    else
                    {
                        world.remove<T>(entity);
                        if (!hadWorld) world.remove<Engine::WorldTransformComponent>(entity);
                        if (!hadFlags) world.remove<Engine::FlagComponent>(entity);
                    }
                    return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };
                }
            }
            return {};
        }
        return { archive::ArchiveErrorCode::UnsupportedType, std::string(key) };
    }

    template<typename T>
    Engine::ComponentReflectionEntry MakeComponentEntry(const reflection::Registry& registry,
        std::string name, std::string category, Engine::ComponentReflectionEntry::AddFunc add = nullptr,
        bool userAddable = true)
    {
        Engine::ComponentReflectionEntry entry;
        entry.Type = std::type_index(typeid(T));
        entry.StorageId = entt::type_hash<T>::value();
        entry.StorageTypeName = entt::type_id<T>().name();
        entry.Name = std::move(name);
        entry.Category = std::move(category);
        entry.ReflectionType = registry.FindType<T>();
        entry.UserAddable = userAddable;
        entry.Has = [](const entt::registry& world, entt::entity entity)
        {
            const auto* storage = world.storage(entt::type_hash<T>::value());
            const auto& type = entt::type_id<T>();
            return world.valid(entity) && storage != nullptr && storage->info().hash() == type.hash() &&
                storage->info().name() == type.name() && storage->contains(entity);
        };
        entry.Add = add != nullptr ? add : &AddComponentValue<T>;
        entry.Edit = &ComponentObject<T, entt::registry>;
        entry.View = &ComponentObject<T, const entt::registry>;
        entry.Read = &ReadComponentValue<T>;
        if constexpr (std::is_default_constructible_v<T>)
        {
            entry.Default = []() -> reflection::ObjectView
                {
                    static const T s_Default{};
                    return reflection::ObjectView::From(s_Default);
                };
        }
        entry.Copy = [](const entt::registry& source, entt::entity from, entt::registry& target, entt::entity to)
        {
            if constexpr (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>)
            {
                if (!source.valid(from) || !target.valid(to) || !source.all_of<T>(from)) return false;
                if constexpr (entt::component_traits<T>::page_size == 0) target.emplace_or_replace<T>(to);
                else target.emplace_or_replace<T>(to, source.get<T>(from));
                return true;
            }
            return false;
        };
        entry.Remove = [](entt::registry& world, entt::entity entity) { world.remove<T>(entity); };
        return entry;
    }

    std::string MetadataText(const reflection::TypeInfo& type, std::string_view key, std::string fallback)
    {
        const auto* value = type.Metadata.Find(key);
        const auto* text = value != nullptr ? std::get_if<std::string>(value) : nullptr;
        return text != nullptr && !text->empty() ? *text : std::move(fallback);
    }

    template<typename World>
    Engine::ComponentVisitResult VisitComponents(const Engine::ReflectionRegistry& registry, World& world,
        entt::entity entity, const Engine::ComponentVisitor& visitor)
    {
        if (!world.valid(entity))
            return Engine::ComponentVisitResult::InvalidEntity;
        if (registry.GetComponentEntries().empty())
            return Engine::ComponentVisitResult::NotInitialized;
        if (!visitor)
            return Engine::ComponentVisitResult::Stopped;
        for (const auto& entry : registry.GetComponentEntries())
        {
            if (entry.ReflectionType == nullptr || !entry.Has(world, entity))
                continue;
            const auto object = registry.GetComponentObject(world, entry.StorageId, entity);
            if (object.IsValid() && !visitor(*entry.ReflectionType, object))
                return Engine::ComponentVisitResult::Stopped;
        }
        return Engine::ComponentVisitResult::Completed;
    }
}

BEGIN(Engine)

IMPLEMENT_SINGLETON(ReflectionRegistry)

EResult ReflectionRegistry::Initialize(void*)
{
    const bool registeredHandles = reflection_generated::ForEach_BamEngine_PropertyType(
        [this]<typename Owner, typename Value>(std::string_view)
        {
            return archive_resource::DiscoveredHandleRegistration<Owner>::Register(m_Codecs) &&
                archive_resource::DiscoveredHandleRegistration<Value>::Register(m_Codecs);
        });

    if (!RegisterGeneratedTypes(m_Registry, m_RegistrationResult) || !RegisterComponentOperations() ||
        !registeredHandles ||
        !m_Codecs.Register<glm::vec2>(archive_glm::WriteVec2, archive_glm::ReadVec2) ||
        !m_Codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3) ||
        !m_Codecs.Register<glm::vec4>(archive_glm::WriteVec4, archive_glm::ReadVec4) ||
        !m_Codecs.Register<glm::quat>(archive_glm::WriteQuat, archive_glm::ReadQuat) ||
        !m_Codecs.Register<glm::mat4>(archive_glm::WriteMat4, archive_glm::ReadMat4) ||
        !m_Codecs.Register<ColliderShape>(
            [this](archive::IArchive& ar, std::string_view key, const ColliderShape& value)
            {
                return WriteShapeVariant(ar, key, value, m_Registry, m_Codecs);
            },
            [this](const archive::IArchive& ar, std::string_view key, ColliderShape& value)
            {
                return ReadShapeVariant<ColliderShape, BoxShape, SphereShape, CapsuleShape, MeshShape>(
                    ar, key, value, m_Registry, m_Codecs);
            }) ||
        !m_Codecs.Register<Collider2DShape>(
            [this](archive::IArchive& ar, std::string_view key, const Collider2DShape& value)
            {
                return WriteShapeVariant(ar, key, value, m_Registry, m_Codecs);
            },
            [this](const archive::IArchive& ar, std::string_view key, Collider2DShape& value)
            {
                return ReadShapeVariant<Collider2DShape, Box2DShape, Circle2DShape, Capsule2DShape>(
                    ar, key, value, m_Registry, m_Codecs);
            }))
    {
        std::cerr << "Engine reflection initialization failed: "
            << reflection::ToString(m_RegistrationResult.Error)
            << ", owner: " << m_RegistrationResult.Owner << ", member: " << m_RegistrationResult.Member << '\n';
        Free();
        return EResult::Fail;
    }
    return EResult::Success;
}

bool ReflectionRegistry::RegisterComponentOperations()
{
    std::vector<ComponentReflectionEntry> entries;
    const auto append = [&]<typename T>(std::string name, std::string category,
        ComponentReflectionEntry::AddFunc add = nullptr)
    {
        entries.push_back(MakeComponentEntry<T>(m_Registry, std::move(name), std::move(category), add));
    };

    append.template operator()<NameComponent>("Name", "Core");
    append.template operator()<TagComponent>("Tag", "Core");
    append.template operator()<FlagComponent>("Flags", "Core");
    append.template operator()<TransformComponent>("Transform", "Core", &AddSpatialComponent<TransformComponent>);
    append.template operator()<CameraComponent>("Camera", "Rendering", &AddSpatialComponent<CameraComponent>);
    append.template operator()<LightComponent>("Light", "Rendering", &AddSpatialComponent<LightComponent>);
    append.template operator()<StaticMeshRendererComponent>(
        "Static Mesh Renderer", "Rendering", &AddSpatialComponent<StaticMeshRendererComponent>);
    append.template operator()<SpriteRendererComponent>(
        "Sprite Renderer", "Rendering", &AddSpatialComponent<SpriteRendererComponent>);
    append.template operator()<SkinnedMeshRendererComponent>(
        "Skinned Mesh Renderer", "Rendering", &AddSpatialComponent<SkinnedMeshRendererComponent>);
    append.template operator()<RigidBodyComponent>(
        "Rigid Body", "Physics 3D", &AddSpatialComponent<RigidBodyComponent>);
    append.template operator()<ColliderComponent>("Collider", "Physics 3D", &AddSpatialComponent<ColliderComponent>);
    append.template operator()<JointComponent>("Joint", "Physics 3D", &AddSpatialComponent<JointComponent>);
    append.template operator()<RigidBody2DComponent>(
        "Rigid Body 2D", "Physics 2D", &AddSpatialComponent<RigidBody2DComponent>);
    append.template operator()<Collider2DComponent>(
        "Collider 2D", "Physics 2D", &AddSpatialComponent<Collider2DComponent>);
    append.template operator()<Joint2DComponent>("Joint 2D", "Physics 2D", &AddSpatialComponent<Joint2DComponent>);
    append.template operator()<SkyAtmosphereComponent>("Sky Atmosphere", "Rendering", &AddSpatialComponent<SkyAtmosphereComponent>);
    append.template operator()<TerrainComponent>("Terrain", "Rendering", &AddSpatialComponent<TerrainComponent>);

    const bool generated = reflection_generated::ForEach_BamEngine_Type([&]<typename T>()
    {
        const auto type = std::type_index(typeid(T));
        if (std::ranges::any_of(entries, [&](const auto& entry) { return entry.Type == type; }))
            return true;
        if constexpr (std::is_default_constructible_v<T>)
        {
            const auto* info = m_Registry.FindType<T>();
            if (info == nullptr)
                return false;
            constexpr bool addable = !std::is_same_v<T, IDComponent> && !std::is_same_v<T, WorldTransformComponent> &&
                !std::is_same_v<T, UIInteractionStateComponent>;
            entries.push_back(MakeComponentEntry<T>(m_Registry, MetadataText(*info, "DisplayName", info->QualifiedName),
                MetadataText(*info, "Category", "Core"), nullptr, addable));
            return true;
        }
        return false;
    });

    if (!generated)
        return false;
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        for (std::size_t j = 0; j < i; ++j)
        {
            if (entries[i].Type == entries[j].Type || entries[i].StorageId == entries[j].StorageId)
                return false;
        }
    }
    // Built-in creation policies and generated entries share the same display metadata.
    for (auto& entry : entries)
    {
        if (entry.ReflectionType != nullptr)
        {
            entry.Name = MetadataText(*entry.ReflectionType, "DisplayName", entry.Name);
            entry.Category = MetadataText(*entry.ReflectionType, "Category", entry.Category);
        }
    }
    m_Components = std::move(entries);
    return true;
}

void ReflectionRegistry::Free()
{
    m_Components.clear();
    m_Codecs = archive_reflection::PropertyCodecs{};
    m_Registry = reflection::Registry{};
}

const ComponentReflectionEntry* ReflectionRegistry::FindComponent(std::type_index type) const
{
    for (const auto& entry : m_Components)
        if (entry.Type == type)
            return &entry;
    return nullptr;
}

const ComponentReflectionEntry* ReflectionRegistry::FindComponent(entt::id_type storageId) const
{
    for (const auto& entry : m_Components)
        if (entry.StorageId == storageId)
            return &entry;
    return nullptr;
}

const ComponentReflectionEntry* ReflectionRegistry::FindComponent(std::string_view qualifiedName) const
{
    for (const auto& entry : m_Components)
        if (entry.ReflectionType != nullptr && entry.ReflectionType->QualifiedName == qualifiedName)
            return &entry;
    return nullptr;
}

reflection::ObjectView ReflectionRegistry::GetComponentObject(entt::registry& world,
    entt::id_type storageId, entt::entity entity) const
{
    const auto* entry = FindComponent(storageId);
    return entry != nullptr ? entry->Edit(world, entity) : reflection::ObjectView{};
}

reflection::ObjectView ReflectionRegistry::GetComponentObject(const entt::registry& world,
    entt::id_type storageId, entt::entity entity) const
{
    const auto* entry = FindComponent(storageId);
    return entry != nullptr ? entry->View(world, entity) : reflection::ObjectView{};
}

reflection::ObjectView ReflectionRegistry::GetDefaultComponentObject(entt::id_type storageId) const
{
    const auto* entry = FindComponent(storageId);
    return (entry != nullptr && entry->Default != nullptr) ? entry->Default() : reflection::ObjectView{};
}
reflection::ObjectView ReflectionRegistry::GetDefaultComponentObject(const reflection::TypeInfo& type) const
{
    const auto* entry = FindComponent(type.GetCppType());
    return (entry != nullptr && entry->Default != nullptr) ? entry->Default() : reflection::ObjectView{};
}
ComponentVisitResult ReflectionRegistry::ForEachComponent(entt::registry& world,
    entt::entity entity, const ComponentVisitor& visitor) const
{
    return VisitComponents(*this, world, entity, visitor);
}

ComponentVisitResult ReflectionRegistry::ForEachComponent(const entt::registry& world,
    entt::entity entity, const ComponentVisitor& visitor) const
{
    return VisitComponents(*this, world, entity, visitor);
}

archive::ArchiveResult ReflectionRegistry::WriteComponent(archive::IArchive& ar, std::string_view key,
    const entt::registry& world, entt::entity entity, std::type_index type,
    const archive_reflection::PropertyCodecs* codecs) const
{
    if (!world.valid(entity))
        return { archive::ArchiveErrorCode::InvalidOperation, std::string(key) };
    const auto* entry = FindComponent(type);
    if (entry == nullptr || entry->ReflectionType == nullptr)
        return { archive::ArchiveErrorCode::UnsupportedType, std::string(key) };
    if (!entry->MatchesStorage(world))
        return { archive::ArchiveErrorCode::InvalidOperation, std::string(key) };
    if (!entry->Has(world, entity))
        return { archive::ArchiveErrorCode::MissingField, std::string(key) };
    bool enabled = true;
    auto result = archive_reflection::IsSerializationEnabled(entry->ReflectionType->Metadata, key, enabled);
    if (!result || !enabled)
        return result;
    codecs = codecs != nullptr ? codecs : &m_Codecs;
    if (type == std::type_index(typeid(TransformComponent)))
    {
        auto candidate = world.get<TransformComponent>(entity);
        if (!candidate.HasValidAbsoluteFlags() ||
            !candidate.SetLocalPosition(candidate.position) || !candidate.SetLocalScale(candidate.scale) ||
            !candidate.SynchronizeRotation())
            return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };
        glm::mat4 checkedWorld;
        if (!TransformSystem::TryGetWorldMatrix(world, entity, checkedWorld))
            return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };
        return archive_reflection::WriteObject(ar, key, *entry->ReflectionType,
            reflection::ObjectView::From(std::as_const(candidate)), &m_Registry, codecs);
    }
    const auto object = entry->View(world, entity);
    if (!object.IsValid())
        return { archive::ArchiveErrorCode::UnsupportedType, std::string(key) };
    return archive_reflection::WriteObject(ar, key, *entry->ReflectionType, object, &m_Registry, codecs);
}

archive::ArchiveResult ReflectionRegistry::ReadComponent(const archive::IArchive& ar, std::string_view key,
    entt::registry& world, entt::entity entity, std::type_index type,
    const archive_reflection::PropertyCodecs* codecs) const
{
    if (!world.valid(entity))
        return { archive::ArchiveErrorCode::InvalidOperation, std::string(key) };
    const auto* entry = FindComponent(type);
    if (entry == nullptr || entry->ReflectionType == nullptr)
        return { archive::ArchiveErrorCode::UnsupportedType, std::string(key) };
    if (!entry->MatchesStorage(world))
        return { archive::ArchiveErrorCode::InvalidOperation, std::string(key) };
    bool enabled = true;
    const auto result = archive_reflection::IsSerializationEnabled(entry->ReflectionType->Metadata, key, enabled);
    if (!result || !enabled)
        return result;
    return entry->Read(ar, key, world, entity, *entry, m_Registry, codecs != nullptr ? codecs : &m_Codecs);
}

archive::ArchiveResult ReflectionRegistry::WriteComponents(archive::IArchive& ar, std::string_view key,
    const entt::registry& world, entt::entity entity, const archive_reflection::PropertyCodecs* codecs) const
{
    if (!world.valid(entity))
        return { archive::ArchiveErrorCode::InvalidOperation, std::string(key) };
    return ar.WriteObject(key, entity, [&](archive::IArchive& object, const auto&) -> archive::ArchiveResult
    {
        for (auto&& [storageId, storage] : world.storage())
        {
            if (!storage.contains(entity))
                continue;
            const auto* entry = FindComponent(storageId);
            if (entry == nullptr || entry->ReflectionType == nullptr)
                return { archive::ArchiveErrorCode::UnsupportedType, std::to_string(storageId) };
            auto result = WriteComponent(
                object, entry->ReflectionType->QualifiedName, world, entity, entry->Type, codecs);
            if (!result)
                return result;
        }
        return {};
    });
}

END
