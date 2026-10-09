#pragma once

#include <entt/meta/meta.hpp>
#include <entt/meta/resolve.hpp>
#include <string>
#include <string_view>

namespace reflection_entt
{
    enum class RegistrationError
    {
        None,
        DuplicateType,
        IdentifierCollision
    };

    struct RegistrationResult
    {
        RegistrationError Error = RegistrationError::None;
        std::string Subject;

        [[nodiscard]] bool Succeeded() const noexcept { return Error == RegistrationError::None; }
        explicit operator bool() const noexcept { return Succeeded(); }
    };

    namespace detail
    {
        // Preflight happens before modifying the context. Registered types are never overwritten.
        template<typename T>
        bool CheckType(const entt::meta_ctx& context, const char* name, RegistrationResult& result)
        {
            if (entt::resolve(context, entt::type_id<T>()))
            {
                result = {RegistrationError::DuplicateType, name};
                return false;
            }
            if (entt::resolve(context, entt::hashed_string::value(name)))
            {
                result = {RegistrationError::IdentifierCollision, name};
                return false;
            }
            return true;
        }
    }
}
