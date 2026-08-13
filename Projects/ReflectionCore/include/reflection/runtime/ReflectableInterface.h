#pragma once

#include "reflection/core/TypeInfo.h"

#include <type_traits>
#include <entt/entt.hpp>

namespace reflection
{
    class ReflectableInterface
    {
    public:
        virtual ~ReflectableInterface() = default;
        virtual entt::id_type GetTypeID() const noexcept = 0;
    };

}