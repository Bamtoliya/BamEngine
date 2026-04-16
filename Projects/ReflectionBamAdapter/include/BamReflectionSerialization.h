#pragma once

#include <reflection/core/PropertyInfo.h>
#include <reflection/core/TypeInfo.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace Engine
{
    class Archive;
}

namespace bam_reflection
{
    class ReflectionSerialization
    {
    public:
        using ObjectInstantiatorFunction = bool(*)(std::string_view qualified_name, Engine::Archive& ar, void** out_instance);

        using ResourceHandleWriteCallback = bool(*)(
            const void* erased_handle,
            std::size_t handle_size,
            std::string& out_asset_key,
            std::string& out_asset_path
            );

        using ResourceHandleReadCallback = bool(*)(
            std::string_view asset_key,
            std::string_view asset_path,
            void* erased_handle,
            std::size_t handle_size
            );

        struct ResourceHandleCallbacks
        {
            ResourceHandleWriteCallback Write = nullptr;
            ResourceHandleReadCallback Read = nullptr;
        };

    public:
        static void SetObjectInstantiator(ObjectInstantiatorFunction func);
        static ObjectInstantiatorFunction GetObjectInstantiator();

        static void SetResourceHandleCallbacks(const ResourceHandleCallbacks& callbacks);
        static const ResourceHandleCallbacks& GetResourceHandleCallbacks();

        static const reflection::TypeInfo* ResolveTypeInfo(std::string_view type_name);

        static void* CreateInstanceByQualifiedName(std::string_view qualified_name);
        static void DestroyInstanceByQualifiedName(std::string_view qualified_name, void* instance);

        static void SerializeProperty(Engine::Archive& ar, void* instance, const reflection::PropertyInfo& property_info);
        static void SerializeProperties(Engine::Archive& ar, void* instance, const reflection::TypeInfo& type_info);

    private:
        static void ProcessProperty(Engine::Archive& ar, const reflection::PropertyInfo& property_info, void* value_ptr);
    };
}