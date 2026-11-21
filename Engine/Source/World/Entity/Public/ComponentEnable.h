#pragma once

#include <entt/entity/registry.hpp>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace Engine
{
    namespace detail
    {
        struct DisabledComponents
        {
            std::unordered_map<entt::id_type, std::unordered_set<entt::entity>> ByType;
        };

        template<typename T>
        void ClearDisabledComponent(entt::registry& world, entt::entity entity)
        {
            auto* disabled = world.ctx().find<DisabledComponents>();
            if (!disabled) return;
            const auto found = disabled->ByType.find(entt::type_hash<T>::value());
            if (found != disabled->ByType.end()) found->second.erase(entity);
        }

        template<typename T>
        void SetComponentEnabled(entt::registry& world, entt::entity entity, bool enabled)
        {
            if (enabled)
            {
                ClearDisabledComponent<T>(world, entity);
                return;
            }
            auto& disabled = world.ctx().emplace<DisabledComponents>();
            const auto [found, inserted] = disabled.ByType.try_emplace(entt::type_hash<T>::value());
            if (inserted) world.on_destroy<T>().template connect<&ClearDisabledComponent<T>>();
            found->second.insert(entity);
        }
    }

    inline bool IsComponentEnabled(const entt::registry& world, entt::entity entity, entt::id_type type)
    {
        const auto* disabled = world.ctx().find<detail::DisabledComponents>();
        if (!disabled) return true;
        const auto found = disabled->ByType.find(type);
        return found == disabled->ByType.end() || !found->second.contains(entity);
    }

    template<typename T>
    bool IsComponentEnabled(const entt::registry& world, entt::entity entity, const T& component)
    {
        if constexpr (requires { T::ComponentEnableMember; })
        {
            if (!(component.*T::ComponentEnableMember)) return false;
        }
        return IsComponentEnabled(world, entity, entt::type_hash<T>::value());
    }

    template<typename T>
    bool IsComponentEnabled(const entt::registry& world, entt::entity entity)
    {
        using Component = std::remove_const_t<T>;
        if (!IsComponentEnabled(world, entity, entt::type_hash<Component>::value())) return false;
        if constexpr (requires { Component::ComponentEnableMember; })
        {
            const auto* value = world.try_get<Component>(entity);
            return value && (value->*Component::ComponentEnableMember);
        }
        return true;
    }

    template<typename T, typename U, typename... Others>
    bool IsComponentEnabled(const entt::registry& world, entt::entity entity)
    {
        return IsComponentEnabled<T>(world, entity) && IsComponentEnabled<U>(world, entity) &&
            (IsComponentEnabled<Others>(world, entity) && ...);
    }

    template<typename T, typename Registry>
    auto TryGetEnabledComponent(Registry& world, entt::entity entity) -> decltype(world.template try_get<T>(entity))
    {
        return world.valid(entity) && IsComponentEnabled<T>(world, entity) ? world.template try_get<T>(entity) : nullptr;
    }
}
