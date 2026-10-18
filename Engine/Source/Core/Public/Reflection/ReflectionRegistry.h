#pragma once

#include "Types.h"
#include "Base.h"
#include <reflection/Registry.h>
#include <reflection_archive/PropertyArchiveCodecs.h>
#include <entt/entity/registry.hpp>

#include <functional>
#include <span>
#include <string>
#include <typeindex>
#include <vector>

BEGIN(Engine)

enum class ComponentVisitResult { Completed, Stopped, InvalidEntity, NotInitialized };
using ComponentVisitor = std::function<bool(const reflection::TypeInfo&, const reflection::ObjectView&)>;

struct ComponentReflectionEntry
{
    using AddFunc = bool (*)(entt::registry&, entt::entity);
    using ReadFunc = archive::ArchiveResult (*)(const archive::IArchive&, std::string_view,
        entt::registry&, entt::entity, const ComponentReflectionEntry&, const reflection::Registry&,
        const archive_reflection::PropertyCodecs*);

    std::type_index Type{ typeid(void) };
    entt::id_type StorageId = 0;
    std::string_view StorageTypeName;
    std::string Name;
    std::string Category;
    const reflection::TypeInfo* ReflectionType = nullptr;
    bool UserAddable = true;
    bool (*Has)(const entt::registry&, entt::entity) = nullptr;
    AddFunc Add = nullptr;
    reflection::ObjectView (*Edit)(entt::registry&, entt::entity) = nullptr;
    reflection::ObjectView (*View)(const entt::registry&, entt::entity) = nullptr;
    reflection::ObjectView(*Default)() = nullptr;
    ReadFunc Read = nullptr;
    bool (*Copy)(const entt::registry&, entt::entity, entt::registry&, entt::entity) = nullptr;
    void (*Remove)(entt::registry&, entt::entity) = nullptr;

    [[nodiscard]] bool TryAdd(entt::registry& world, entt::entity entity) const
    {
        if (!world.valid(entity) || !MatchesStorage(world) || Has == nullptr || Add == nullptr || Has(world, entity))
            return false;
        return Add(world, entity) && world.valid(entity) && Has(world, entity);
    }

    [[nodiscard]] bool MatchesStorage(const entt::registry& world) const
    {
        const auto* storage = world.storage(StorageId);
        return storage == nullptr ||
            (storage->info().hash() == StorageId && storage->info().name() == StorageTypeName);
    }
};

class ENGINE_API ReflectionRegistry final : public Base
{
    DECLARE_SINGLETON(ReflectionRegistry)

private:
    ReflectionRegistry() = default;
    ~ReflectionRegistry() override = default;
    EResult Initialize(void* arg = nullptr);

public:
    void Free() override;

    [[nodiscard]] const reflection::Registry& GetRegistry() const noexcept { return m_Registry; }
    [[nodiscard]] const archive_reflection::PropertyCodecs& GetPropertyCodecs() const noexcept { return m_Codecs; }
    [[nodiscard]] const reflection::RegistrationResult& GetRegistrationResult() const noexcept
    {
        return m_RegistrationResult;
    }
    [[nodiscard]] std::span<const ComponentReflectionEntry> GetComponentEntries() const { return m_Components; }
    [[nodiscard]] const ComponentReflectionEntry* FindComponent(std::type_index type) const;
    [[nodiscard]] const ComponentReflectionEntry* FindComponent(entt::id_type storageId) const;
    [[nodiscard]] const ComponentReflectionEntry* FindComponent(std::string_view qualifiedName) const;

    [[nodiscard]] reflection::ObjectView GetComponentObject(entt::registry& world, entt::id_type storageId, entt::entity entity) const;
    [[nodiscard]] reflection::ObjectView GetComponentObject(const entt::registry& world, entt::id_type storageId, entt::entity entity) const;
    [[nodiscard]] ComponentVisitResult ForEachComponent(entt::registry& world, entt::entity entity, const ComponentVisitor& visitor) const;
    [[nodiscard]] ComponentVisitResult ForEachComponent(const entt::registry& world, entt::entity entity, const ComponentVisitor& visitor) const;

    [[nodiscard]] reflection::ObjectView GetDefaultComponentObject(entt::id_type storageId) const;
    [[nodiscard]] reflection::ObjectView GetDefaultComponentObject(const reflection::TypeInfo& type) const;

    [[nodiscard]] archive::ArchiveResult WriteComponent(archive::IArchive& ar, std::string_view key,
        const entt::registry& world, entt::entity entity, std::type_index type,
        const archive_reflection::PropertyCodecs* codecs = nullptr) const;
    [[nodiscard]] archive::ArchiveResult ReadComponent(const archive::IArchive& ar, std::string_view key,
        entt::registry& world, entt::entity entity, std::type_index type,
        const archive_reflection::PropertyCodecs* codecs = nullptr) const;
    [[nodiscard]] archive::ArchiveResult WriteComponents(archive::IArchive& ar, std::string_view key,
        const entt::registry& world, entt::entity entity,
        const archive_reflection::PropertyCodecs* codecs = nullptr) const;

    template<typename T>
    [[nodiscard]] archive::ArchiveResult WriteComponent(archive::IArchive& ar, std::string_view key,
        const entt::registry& world, entt::entity entity,
        const archive_reflection::PropertyCodecs* codecs = nullptr) const
    {
        return WriteComponent(ar, key, world, entity, std::type_index(typeid(T)), codecs);
    }

    template<typename T>
    [[nodiscard]] archive::ArchiveResult ReadComponent(const archive::IArchive& ar, std::string_view key,
        entt::registry& world, entt::entity entity,
        const archive_reflection::PropertyCodecs* codecs = nullptr) const
    {
        return ReadComponent(ar, key, world, entity, std::type_index(typeid(T)), codecs);
    }

private:
    static bool RegisterGeneratedTypes(reflection::Registry& registry, reflection::RegistrationResult& failure);
    bool RegisterComponentOperations();

    reflection::Registry m_Registry;
    reflection::RegistrationResult m_RegistrationResult;
    std::vector<ComponentReflectionEntry> m_Components;
    archive_reflection::PropertyCodecs m_Codecs;
};

END
