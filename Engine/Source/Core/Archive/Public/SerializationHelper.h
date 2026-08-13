#pragma once

#include "Archive.h"
#include <entt/entt.hpp>
#include <functional>

namespace Engine
{
    class ENGINE_API SerializationHelper
    {
    public:
        static void SerializeEnTT(Archive& ar, entt::meta_any& instance);



        template<typename T>
        static void SerializeStaticType(Archive& ar, T& instance)
        {
            entt::meta_any meta_instance{ std::ref(instance) };
            SerializeEnTT(ar, meta_instance);
        }
    };
}