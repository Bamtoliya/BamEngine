#pragma once

#define REFLECTION_ENTT_DETAIL_STRINGIZE_IMPL(...) #__VA_ARGS__
#define REFLECTION_ENTT_DETAIL_STRINGIZE(...) REFLECTION_ENTT_DETAIL_STRINGIZE_IMPL(__VA_ARGS__)

// Markers are collected by the generator; normal compilation erases them.
#define CLASS(...)
#define STRUCT(...)
#define ENUM(...)
#define FUNCTION(...)
#define PROPERTY(...)

namespace reflection_entt::detail
{
    template<typename T>
    struct TypeRegistration;
}

#define REFLECT_BODY() \
    template<typename T> friend struct ::reflection_entt::detail::TypeRegistration;
