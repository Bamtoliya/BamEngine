#pragma once

#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <entt/entity/component.hpp>
#include <entt/entity/registry.hpp>

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Engine
{
    template<typename T>
    [[nodiscard]] archive::ArchiveResult WriteComponent(archive::GlazeArchiveBase& ar, std::string_view key,
        const entt::registry& world, entt::entity entity, const reflection::Registry& reflectionRegistry,
        const archive_reflection::PropertyCodecs* codecs = nullptr)
    {
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>> && std::is_class_v<T>);
        static_assert(entt::component_traits<T>::page_size != 0);

        if (!world.valid(entity))
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto* type = reflectionRegistry.FindType<T>();

        if (type == nullptr)
        {
            return { archive::ArchiveErrorCode::UnsupportedType, std::string{ key } };
        }

        const auto* component = world.try_get<T>(entity);

        if (component == nullptr)
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        return archive_reflection::WriteObject(ar, key, *type, reflection::ObjectView::From(*component),
            &reflectionRegistry, codecs);
    }

    
    template<typename T>
        requires (std::default_initializable<T>&& std::copy_constructible<T>&& std::assignable_from<T&, T>)
    [[nodiscard]] archive::ArchiveResult ReadComponent(
        const archive::GlazeArchiveBase& ar, std::string_view key, entt::registry& world, entt::entity entity,
        const reflection::Registry& reflectionRegistry, const archive_reflection::PropertyCodecs* codecs = nullptr,
        archive::ArchiveResult(*prepare)(T&) = nullptr)
    {
        static_assert(std::is_same_v<T, std::remove_cvref_t<T>> && std::is_class_v<T>);
        static_assert(entt::component_traits<T>::page_size != 0);

        if (!world.valid(entity))
        {
            return { archive::ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto* type = reflectionRegistry.FindType<T>();

        if (type == nullptr)
        {
            return { archive::ArchiveErrorCode::UnsupportedType, std::string{ key } };
        }

        auto* component = world.try_get<T>(entity);
        T candidate = component != nullptr ? *component : T{};

        auto result = archive_reflection::ReadObject(ar, key, *type, candidate, &reflectionRegistry, codecs);

        if (!result)
        {
            return result;
        }

        if (prepare != nullptr)
        {
            result = prepare(candidate);

            if (!result)
            {
                return result;
            }
        }

        if (component != nullptr)
        {
            *component = std::move(candidate);
        }
        else
        {
            world.emplace<T>(entity, std::move(candidate));
        }

        return {};
    }
}