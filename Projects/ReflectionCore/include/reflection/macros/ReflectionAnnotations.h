#pragma once

#include "reflection/core/TypeInfo.h"
#include "reflection/runtime/ReflectableInterface.h"

#ifdef __clang__
#define CLASS(...)       __attribute__((annotate("reflect_class:" #__VA_ARGS__)))
#define STRUCT(...)      __attribute__((annotate("reflect_struct:" #__VA_ARGS__)))
#define PROPERTY(...)    __attribute__((annotate("reflect_property:" #__VA_ARGS__)))
#define FUNCTION(...)    __attribute__((annotate("reflect_function:" #__VA_ARGS__)))
#define ENUM(...)        __attribute__((annotate("reflect_enum:" #__VA_ARGS__)))
#else
#define PROPERTY(...)
#define CLASS()
#define STRUCT()
#define ENUM(...)
#define FUNCTION(...)
#endif

namespace entt { template<std::size_t Len, std::size_t Align> class basic_any; }
namespace Engine { void RegisterReflection_EnTT(); }

#define REFLECT_COMMON_DECLS() \
    friend void ::Engine::RegisterReflection_EnTT(); \
    template<std::size_t Len, std::size_t Align> friend class ::entt::basic_any;


#define REFLECT_STRUCT() \
    REFLECT_COMMON_DECLS()

#define REFLECT_CLASS() \
public: \
    REFLECT_COMMON_DECLS() \
    virtual entt::id_type GetTypeID() const noexcept override { return entt::type_hash<std::remove_pointer_t<decltype(this)>>::value(); }

#define REFLECT_BASE() \
    REFLECT_CLASS()