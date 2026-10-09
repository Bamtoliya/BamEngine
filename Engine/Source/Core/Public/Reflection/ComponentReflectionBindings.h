#pragma once

#include <reflection/Registry.h>
#include <entt/entity/component.hpp>
#include <entt/entity/registry.hpp>

#include <functional>
#include <type_traits>
#include <vector>

namespace Engine
{
    enum class ComponentVisitResult
    {
        Completed,
        Stopped,
        InvalidEntity,
        NotInitialized
    };

    class ComponentReflectionBindings
    {
    public:
        explicit ComponentReflectionBindings(const reflection::Registry& registry)
            : m_registry(registry)
        {
        }

        ComponentReflectionBindings(const reflection::Registry&&) = delete;

        template<typename T>
        [[nodiscard]]
        bool Register()
        {
            static_assert(std::is_same_v<T, std::remove_cvref_t<T>>,
                "Register an unqualified component type.");
            static_assert(std::is_class_v<T>, "The component must be a class or struct.");
            static_assert(entt::component_traits<T>::page_size != 0,
                "Components without stored instances cannot provide ObjectView.");

            const auto* type = m_registry.FindType<T>();

            if (type == nullptr)
            {
                return false;
            }

            for (const auto& binding : m_bindings)
            {
                if (binding.Type->GetCppType() == type->GetCppType())
                {
                    return false;
                }
            }

            Binding binding;
            binding.Type = type;

            binding.GetMutable = [](entt::registry& world, entt::entity entity)
                {
                    auto* component = world.try_get<T>(entity);
                    return component != nullptr ? reflection::ObjectView::From(*component) : reflection::ObjectView{};
                };

            binding.GetConst = [](const entt::registry& world, entt::entity entity)
                {
                    const auto* component = world.try_get<T>(entity);
                    return component != nullptr ? reflection::ObjectView::From(*component) : reflection::ObjectView{};
                };

            m_bindings.push_back(binding);
            return true;
        }

        template<typename Visitor>
        [[nodiscard]]
        ComponentVisitResult ForEach(entt::registry& world, entt::entity entity, Visitor&& visitor) const
        {
            return Visit(world, entity, visitor);
        }

        template<typename Visitor>
        [[nodiscard]]
        ComponentVisitResult ForEach(const entt::registry& world, entt::entity entity, Visitor&& visitor) const
        {
            return Visit(world, entity, visitor);
        }

    private:
        struct Binding
        {
            const reflection::TypeInfo* Type = nullptr;
            reflection::ObjectView(*GetMutable)(entt::registry&, entt::entity) = nullptr;
            reflection::ObjectView(*GetConst)(const entt::registry&, entt::entity) = nullptr;
        };

        template<typename World, typename Visitor>
        ComponentVisitResult Visit(World& world, entt::entity entity, Visitor& visitor) const
        {
            static_assert(std::is_invocable_r_v<bool, Visitor&,
                const reflection::TypeInfo&, const reflection::ObjectView&>,
                "The visitor must return bool and accept TypeInfo and ObjectView.");

            if (!world.valid(entity))
            {
                return ComponentVisitResult::InvalidEntity;
            }

            for (const auto& binding : m_bindings)
            {
                reflection::ObjectView object;

                if constexpr (std::is_const_v<World>)
                {
                    object = binding.GetConst(world, entity);
                }
                else
                {
                    object = binding.GetMutable(world, entity);
                }

                if (!object.IsValid())
                {
                    continue;
                }

                if (!std::invoke(visitor, *binding.Type, object))
                {
                    return ComponentVisitResult::Stopped;
                }
            }

            return ComponentVisitResult::Completed;
        }

        const reflection::Registry& m_registry;
        std::vector<Binding> m_bindings;
    };
}