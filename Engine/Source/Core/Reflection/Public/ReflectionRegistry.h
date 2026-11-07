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
struct DeferredComponentTransformUpdate {};

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

    ENGINE_API void EnsureDependencies(entt::registry& world, entt::entity entity) const;

    [[nodiscard]] bool TryAdd(entt::registry& world, entt::entity entity) const
    {
        if (!world.valid(entity) || !MatchesStorage(world) || Has == nullptr || Add == nullptr || Has(world, entity))
            return false;

        // 필수 의존 컴포넌트(RequireComponents)가 정의되어 있다면 먼저 안전하게 추가!
        EnsureDependencies(world, entity);

        return Add(world, entity) && world.valid(entity) && Has(world, entity);
    }

    [[nodiscard]] bool MatchesStorage(const entt::registry& world) const
    {
        const auto* storage = world.storage(StorageId);
        return storage == nullptr ||
            (storage->info().hash() == StorageId && storage->info().name() == StorageTypeName);
    }
};

namespace detail
{
    template<typename T>
    inline bool DefaultAddComponent(entt::registry& world, entt::entity entity)
    {
        if (!world.valid(entity) || world.all_of<T>(entity)) return false;
        if constexpr (std::is_default_constructible_v<T>) {
            world.emplace<T>(entity);
            return true;
        }
        return false;
    }
    template<typename T>
    inline Engine::ComponentReflectionEntry BuildComponentEntry(
        const reflection::Registry& registry,
        std::string name, std::string category,
        Engine::ComponentReflectionEntry::AddFunc add = nullptr,
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
        entry.Has = [](const entt::registry& world, entt::entity entity) {
            const auto* storage = world.storage(entt::type_hash<T>::value());
            const auto& type = entt::type_id<T>();
            return world.valid(entity) && storage != nullptr && storage->info().hash() == type.hash() &&
                storage->info().name() == type.name() && storage->contains(entity);
            };
        entry.Add = add != nullptr ? add : &DefaultAddComponent<T>;
        entry.Edit = [](entt::registry& world, entt::entity entity) -> reflection::ObjectView {
            if constexpr (entt::component_traits<T>::page_size != 0) {
                auto* comp = world.try_get<T>(entity);
                return comp ? reflection::ObjectView::From(*comp) : reflection::ObjectView{};
            }
            return {};
            };
        entry.View = [](const entt::registry& world, entt::entity entity) -> reflection::ObjectView {
            if constexpr (entt::component_traits<T>::page_size != 0) {
                const auto* comp = world.try_get<T>(entity);
                return comp ? reflection::ObjectView::From(*comp) : reflection::ObjectView{};
            }
            return {};
            };
        if constexpr (std::is_default_constructible_v<T>) {
            entry.Default = []() -> reflection::ObjectView {
                static const T s_Default{};
                return reflection::ObjectView::From(s_Default);
                };
        }
        entry.Copy = [](const entt::registry& source, entt::entity from, entt::registry& target, entt::entity to) {
            if constexpr (std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>) {
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
}

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
    [[nodiscard]] reflection::Registry& GetMutableRegistry() noexcept { return m_Registry; }
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

    [[nodiscard]] archive::ArchiveResult WriteComponent(archive::IArchive& ar, std::string_view key, const entt::registry& world, entt::entity entity, std::type_index type, const archive_reflection::PropertyCodecs* codecs = nullptr) const;
    [[nodiscard]] archive::ArchiveResult ReadComponent(const archive::IArchive& ar, std::string_view key, entt::registry& world, entt::entity entity, std::type_index type, const archive_reflection::PropertyCodecs* codecs = nullptr) const;
    [[nodiscard]] archive::ArchiveResult WriteComponents(archive::IArchive& ar, std::string_view key, const entt::registry& world, entt::entity entity, const archive_reflection::PropertyCodecs* codecs = nullptr) const;

    template<typename T>
    [[nodiscard]] archive::ArchiveResult WriteComponent(archive::IArchive& ar, std::string_view key, const entt::registry& world, entt::entity entity, const archive_reflection::PropertyCodecs* codecs = nullptr) const
    {
        return WriteComponent(ar, key, world, entity, std::type_index(typeid(T)), codecs);
    }

    template<typename T>
    [[nodiscard]] archive::ArchiveResult ReadComponent(const archive::IArchive& ar, std::string_view key, entt::registry& world, entt::entity entity, const archive_reflection::PropertyCodecs* codecs = nullptr) const
    {
        return ReadComponent(ar, key, world, entity, std::type_index(typeid(T)), codecs);
    }

public:
    // ── 동적 컴포넌트 단일 등록 ──
    template<typename T>
    bool RegisterComponent(std::string displayName = "", std::string category = "Custom",
        ComponentReflectionEntry::AddFunc customAdd = nullptr, bool userAddable = true)
    {
        const auto type = std::type_index(typeid(T));
        if (FindComponent(type) != nullptr) return false;
        const auto* info = m_Registry.FindType<T>();
        if (displayName.empty()) {
            if (info != nullptr) {
                const auto* val = info->Metadata.Find("DisplayName");
                const auto* str = val ? std::get_if<std::string>(val) : nullptr;
                displayName = (str && !str->empty()) ? *str : info->QualifiedName;
            }
            else {
                displayName = typeid(T).name();
            }
        }
        if (category == "Custom" && info != nullptr) {
            const auto* val = info->Metadata.Find("Category");
            const auto* str = val ? std::get_if<std::string>(val) : nullptr;
            if (str && !str->empty()) category = *str;
        }
        auto entry = detail::BuildComponentEntry<T>(m_Registry, std::move(displayName), std::move(category), customAdd, userAddable);
        m_Components.push_back(std::move(entry));
        return true;
    }
    // ── 동적 컴포넌트 단일 등록 해제 ──
    template<typename T>
    void UnregisterComponent()
    {
        const auto type = std::type_index(typeid(T));
        std::erase_if(m_Components, [&](const ComponentReflectionEntry& entry) {
            return entry.Type == type;
            });
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


#define REGISTER_MODULE_COMPONENTS(ModuleName) \
    reflection_generated::ForEach_##ModuleName##_Type([&]<typename T>() { \
        return Engine::ReflectionRegistry::Get().RegisterComponent<T>(); \
    })

#define UNREGISTER_MODULE_COMPONENTS(ModuleName) \
    reflection_generated::ForEach_##ModuleName##_Type([&]<typename T>() { \
        Engine::ReflectionRegistry::Get().UnregisterComponent<T>(); \
        return true; \
    })