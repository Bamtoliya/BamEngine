#include "BamReflectionSerialization.h"

#include "BamReflectionTypes.h"

#include <reflection/core/Metadata.h>
#include <reflection/runtime/ContainerAccessor.h>
#include <reflection/runtime/PropertyAccessor.h>
#include <reflection/runtime/Registry.h>

#include "Archive.h"
#include "Handle.h"
#include "Functions.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    constexpr std::uint64_t kNoSerializeHash = reflection::CompileTimeHash("NoSerialize");
    constexpr std::uint64_t kTransientHash = reflection::CompileTimeHash("Transient");

    auto& ObjectInstantiatorSlot()
    {
        static bam_reflection::ReflectionSerialization::ObjectInstantiatorFunction value = nullptr;
        return value;
    }

    auto& ResourceHandleCallbacksSlot()
    {
        static bam_reflection::ReflectionSerialization::ResourceHandleCallbacks value = {};
        return value;
    }

    std::string NormalizeTypeName(std::string_view type_name)
    {
        std::string result(type_name);

        auto erase_prefix = [&](std::string_view prefix)
            {
                if (result.starts_with(prefix))
                {
                    result.erase(0, prefix.size());
                }
            };

        erase_prefix("class ");
        erase_prefix("struct ");
        erase_prefix("enum ");
        erase_prefix("const ");

        while (!result.empty() && (result.back() == '*' || result.back() == '&' || result.back() == ' '))
        {
            result.pop_back();
        }

        while (result.ends_with(" const"))
        {
            result.erase(result.size() - 6);
        }

        while (result.ends_with(" volatile"))
        {
            result.erase(result.size() - 9);
        }

        return result;
    }

    bool ShouldSkipProperty(const reflection::PropertyInfo& property_info)
    {
        return
            reflection::MetadataEntry::Find(property_info.Metadata, kNoSerializeHash) != nullptr ||
            reflection::MetadataEntry::Find(property_info.Metadata, kTransientHash) != nullptr;
    }

    const reflection::TypeInfo* ResolveReflectionType(std::string_view type_name)
    {
        if (type_name.empty())
        {
            return nullptr;
        }

        const std::string normalized = NormalizeTypeName(type_name);
        if (normalized.empty())
        {
            return nullptr;
        }

        return reflection::Registry::Get().ResolveTypeName(normalized);
    }

    const reflection::TypeInfo* ResolveDynamicObjectType(
        const reflection::VariableInfo& variable_info,
        void* object_instance)
    {
        if (!object_instance)
        {
            return nullptr;
        }

        const reflection::TypeInfo* declared_type_info = ResolveReflectionType(variable_info.Name);
        if (!declared_type_info)
        {
            return nullptr;
        }

        if (declared_type_info->GetDynamicType)
        {
            return &declared_type_info->GetDynamicType(object_instance);
        }

        return declared_type_info;
    }

    std::size_t GetKnownValueSize(const reflection::VariableInfo& variable_info, std::size_t explicit_size)
    {
        if (explicit_size != 0)
        {
            return explicit_size;
        }

        using reflection::EPropertyType;

        switch (variable_info.Type)
        {
        case EPropertyType::Int8: return sizeof(bam_reflection::int8);
        case EPropertyType::Int16: return sizeof(bam_reflection::int16);
        case EPropertyType::Int32: return sizeof(bam_reflection::int32);
        case EPropertyType::Int64: return sizeof(bam_reflection::int64);

        case EPropertyType::UInt8: return sizeof(bam_reflection::uint8);
        case EPropertyType::UInt16: return sizeof(bam_reflection::uint16);
        case EPropertyType::UInt32: return sizeof(bam_reflection::uint32);
        case EPropertyType::UInt64: return sizeof(bam_reflection::uint64);

        case EPropertyType::Float32: return sizeof(bam_reflection::f32);
        case EPropertyType::Float64: return sizeof(bam_reflection::f64);
        case EPropertyType::Bool: return sizeof(bool);

        case EPropertyType::String: return sizeof(Engine::string);
        case EPropertyType::WString: return sizeof(Engine::wstring);

        case EPropertyType::Enum:
        case EPropertyType::BitFlag:
            return sizeof(std::int64_t);
        case EPropertyType::ResourceHandle:
            return sizeof(Engine::Handle);
        default:
            break;
        }

        const std::string normalized = NormalizeTypeName(variable_info.Name);
        if (normalized == "vec2" || normalized == "glm::vec2") return sizeof(bam_reflection::vec2);
        if (normalized == "vec3" || normalized == "glm::vec3") return sizeof(bam_reflection::vec3);
        if (normalized == "vec4" || normalized == "glm::vec4") return sizeof(bam_reflection::vec4);
        if (normalized == "quat" || normalized == "glm::quat") return sizeof(bam_reflection::quat);
        if (normalized == "mat3" || normalized == "glm::mat3") return sizeof(bam_reflection::mat3);
        if (normalized == "mat4" || normalized == "glm::mat4") return sizeof(bam_reflection::mat4);

        return 0;
    }

    bool TrySerializeBuiltinValue(
        Engine::Archive& ar,
        std::string_view property_name,
        const reflection::VariableInfo& variable_info,
        void* value_ptr,
        std::size_t enum_size)
    {
        using reflection::EPropertyType;

        switch (variable_info.Type)
        {
        case EPropertyType::Bool:
            ar.Process(property_name, *static_cast<bool*>(value_ptr));
            return true;

        case EPropertyType::Int8:
            ar.Process(property_name, *static_cast<bam_reflection::int8*>(value_ptr));
            return true;
        case EPropertyType::Int16:
            ar.Process(property_name, *static_cast<bam_reflection::int16*>(value_ptr));
            return true;
        case EPropertyType::Int32:
            ar.Process(property_name, *static_cast<bam_reflection::int32*>(value_ptr));
            return true;
        case EPropertyType::Int64:
            ar.Process(property_name, *static_cast<bam_reflection::int64*>(value_ptr));
            return true;

        case EPropertyType::UInt8:
            ar.Process(property_name, *static_cast<bam_reflection::uint8*>(value_ptr));
            return true;
        case EPropertyType::UInt16:
            ar.Process(property_name, *static_cast<bam_reflection::uint16*>(value_ptr));
            return true;
        case EPropertyType::UInt32:
            ar.Process(property_name, *static_cast<bam_reflection::uint32*>(value_ptr));
            return true;
        case EPropertyType::UInt64:
            ar.Process(property_name, *static_cast<bam_reflection::uint64*>(value_ptr));
            return true;

        case EPropertyType::Float32:
            ar.Process(property_name, *static_cast<bam_reflection::f32*>(value_ptr));
            return true;
        case EPropertyType::Float64:
            ar.Process(property_name, *static_cast<bam_reflection::f64*>(value_ptr));
            return true;

        case EPropertyType::String:
            ar.Process(property_name, *static_cast<Engine::string*>(value_ptr));
            return true;
        case EPropertyType::WString:
            ar.Process(property_name, *static_cast<Engine::wstring*>(value_ptr));
            return true;

        case EPropertyType::Enum:
        case EPropertyType::BitFlag:
            ar.ProcessEnum(property_name, value_ptr, enum_size);
            return true;

        default:
            break;
        }

        const std::string normalized = NormalizeTypeName(variable_info.Name);

        if (normalized == "vec2" || normalized == "glm::vec2")
        {
            ar.Process(property_name, *static_cast<bam_reflection::vec2*>(value_ptr));
            return true;
        }

        if (normalized == "vec3" || normalized == "glm::vec3")
        {
            ar.Process(property_name, *static_cast<bam_reflection::vec3*>(value_ptr));
            return true;
        }

        if (normalized == "vec4" || normalized == "glm::vec4")
        {
            ar.Process(property_name, *static_cast<bam_reflection::vec4*>(value_ptr));
            return true;
        }

        if (normalized == "quat" || normalized == "glm::quat")
        {
            ar.Process(property_name, *static_cast<bam_reflection::quat*>(value_ptr));
            return true;
        }

        if (normalized == "mat3" || normalized == "glm::mat3")
        {
            ar.Process(property_name, *static_cast<bam_reflection::mat3*>(value_ptr));
            return true;
        }

        if (normalized == "mat4" || normalized == "glm::mat4")
        {
            ar.Process(property_name, *static_cast<bam_reflection::mat4*>(value_ptr));
            return true;
        }

        return false;
    }

    std::string KeyToString(reflection::EPropertyType type, const void* key_ptr)
    {
        using reflection::EPropertyType;

        switch (type)
        {
        case EPropertyType::String:
            return *static_cast<const Engine::string*>(key_ptr);

        case EPropertyType::WString:
        {
            const auto& wide = *static_cast<const Engine::wstring*>(key_ptr);
            return Engine::WStrToStr(wide);
        }

        case EPropertyType::Bool:
            return *static_cast<const bool*>(key_ptr) ? "true" : "false";

        case EPropertyType::Int8:
            return std::to_string(*static_cast<const bam_reflection::int8*>(key_ptr));
        case EPropertyType::Int16:
            return std::to_string(*static_cast<const bam_reflection::int16*>(key_ptr));
        case EPropertyType::Int32:
            return std::to_string(*static_cast<const bam_reflection::int32*>(key_ptr));
        case EPropertyType::Int64:
            return std::to_string(*static_cast<const bam_reflection::int64*>(key_ptr));

        case EPropertyType::UInt8:
            return std::to_string(*static_cast<const bam_reflection::uint8*>(key_ptr));
        case EPropertyType::UInt16:
            return std::to_string(*static_cast<const bam_reflection::uint16*>(key_ptr));
        case EPropertyType::UInt32:
            return std::to_string(*static_cast<const bam_reflection::uint32*>(key_ptr));
        case EPropertyType::UInt64:
            return std::to_string(*static_cast<const bam_reflection::uint64*>(key_ptr));

        case EPropertyType::Enum:
        case EPropertyType::BitFlag:
            return std::to_string(*static_cast<const std::int64_t*>(key_ptr));

        default:
            return {};
        }
    }

    template<typename TCallback>
    void WithParsedKey(reflection::EPropertyType type, const std::string& key_string, TCallback&& callback)
    {
        using reflection::EPropertyType;

        switch (type)
        {
        case EPropertyType::String:
        {
            Engine::string value = key_string;
            callback(&value);
            return;
        }

        case EPropertyType::WString:
        {
            Engine::wstring value(key_string.begin(), key_string.end());
            callback(&value);
            return;
        }

        case EPropertyType::Bool:
        {
            bool value = (key_string == "true" || key_string == "1");
            callback(&value);
            return;
        }

        case EPropertyType::Int8:
        {
            bam_reflection::int8 value = static_cast<bam_reflection::int8>(std::stoi(key_string));
            callback(&value);
            return;
        }
        case EPropertyType::Int16:
        {
            bam_reflection::int16 value = static_cast<bam_reflection::int16>(std::stoi(key_string));
            callback(&value);
            return;
        }
        case EPropertyType::Int32:
        {
            bam_reflection::int32 value = std::stoi(key_string);
            callback(&value);
            return;
        }
        case EPropertyType::Int64:
        {
            bam_reflection::int64 value = std::stoll(key_string);
            callback(&value);
            return;
        }

        case EPropertyType::UInt8:
        {
            bam_reflection::uint8 value = static_cast<bam_reflection::uint8>(std::stoul(key_string));
            callback(&value);
            return;
        }
        case EPropertyType::UInt16:
        {
            bam_reflection::uint16 value = static_cast<bam_reflection::uint16>(std::stoul(key_string));
            callback(&value);
            return;
        }
        case EPropertyType::UInt32:
        {
            bam_reflection::uint32 value = static_cast<bam_reflection::uint32>(std::stoul(key_string));
            callback(&value);
            return;
        }
        case EPropertyType::UInt64:
        {
            bam_reflection::uint64 value = static_cast<bam_reflection::uint64>(std::stoull(key_string));
            callback(&value);
            return;
        }

        case EPropertyType::Enum:
        case EPropertyType::BitFlag:
        {
            std::int64_t value = std::stoll(key_string);
            callback(&value);
            return;
        }

        default:
            callback(nullptr);
            return;
        }
    }

    void SerializeValue(
        Engine::Archive& ar,
        std::string_view property_name,
        const reflection::VariableInfo& variable_info,
        void* value_ptr,
        std::size_t size,
        const reflection::ContainerInfo* container_data);

    void SerializeStructValue(
        Engine::Archive& ar,
        std::string_view property_name,
        const reflection::VariableInfo& variable_info,
        void* value_ptr)
    {
        const reflection::TypeInfo* type_info = ResolveReflectionType(variable_info.Name);
        if (!type_info)
        {
            return;
        }

        const bool is_inline = property_name.empty();
        if (!is_inline && !ar.PushScope(property_name))
        {
            return;
        }

        bam_reflection::ReflectionSerialization::SerializeProperties(ar, value_ptr, *type_info);

        if (!is_inline)
        {
            ar.PopScope();
        }
    }

    void SerializeObjectValue(
        Engine::Archive& ar,
        std::string_view property_name,
        const reflection::VariableInfo& variable_info,
        void* value_ptr)
    {
        void** object_storage = static_cast<void**>(value_ptr);
        const bool is_inline = property_name.empty();

        const reflection::TypeInfo* declared_type_info = ResolveReflectionType(variable_info.Name);

        if (!is_inline && !ar.PushScope(property_name))
        {
            if (ar.IsReading())
            {
                *object_storage = nullptr;
            }
            return;
        }

        if (ar.IsWriting())
        {
            std::string actual_type_name;
            const reflection::TypeInfo* actual_type_info = nullptr;

            if (*object_storage)
            {
                actual_type_info = ResolveDynamicObjectType(variable_info, *object_storage);
                if (!actual_type_info)
                {
                    actual_type_info = declared_type_info;
                }

                if (actual_type_info)
                {
                    actual_type_name = std::string(actual_type_info->QualifiedName);
                }
            }

            ar.Process("__Type__", actual_type_name);

            if (actual_type_info)
            {
                bam_reflection::ReflectionSerialization::SerializeProperties(ar, *object_storage, *actual_type_info);
            }
        }
        else
        {
            std::string loaded_type_name;
            ar.Process("__Type__", loaded_type_name);

            if (loaded_type_name.empty())
            {
                *object_storage = nullptr;
            }
            else
            {
                const reflection::TypeInfo* actual_type_info = ResolveReflectionType(loaded_type_name);
                if (!actual_type_info)
                {
                    *object_storage = nullptr;
                }
                else
                {
                    if (*object_storage)
                    {
                        const reflection::TypeInfo* existing_type_info = ResolveDynamicObjectType(variable_info, *object_storage);
                        if (!existing_type_info)
                        {
                            existing_type_info = declared_type_info;
                        }

                        if (existing_type_info && existing_type_info->QualifiedName != actual_type_info->QualifiedName)
                        {
                            bam_reflection::ReflectionSerialization::DestroyInstanceByQualifiedName(
                                existing_type_info->QualifiedName,
                                *object_storage
                            );
                            *object_storage = nullptr;
                        }
                    }

                    bool custom_instantiated = false;

                    if (!*object_storage)
                    {
                        if (auto object_instantiator = bam_reflection::ReflectionSerialization::GetObjectInstantiator())
                        {
                            custom_instantiated = object_instantiator(
                                actual_type_info->QualifiedName,
                                ar,
                                object_storage
                            );
                        }

                        if (!custom_instantiated && !*object_storage)
                        {
                            *object_storage = bam_reflection::ReflectionSerialization::CreateInstanceByQualifiedName(
                                actual_type_info->QualifiedName
                            );
                        }
                    }

                    if (*object_storage)
                    {
                        bam_reflection::ReflectionSerialization::SerializeProperties(ar, *object_storage, *actual_type_info);
                    }
                }
            }
        }

        if (!is_inline)
        {
            ar.PopScope();
        }
    }

    void SerializeSequenceValue(
        Engine::Archive& ar,
        std::string_view property_name,
        void* container_ptr,
        const reflection::ContainerInfo* container_data)
    {
        if (!container_data || !container_data->Accessor)
        {
            return;
        }

        const std::size_t inner_size = GetKnownValueSize(container_data->Inner, 0);

        if (ar.IsWriting())
        {
            ar.BeginArray(property_name);

            if (container_data->Accessor->GetElements)
            {
                std::vector<void*> elements = container_data->Accessor->GetElements(container_ptr);
                for (void* element_ptr : elements)
                {
                    ar.BeginArrayElement();
                    SerializeValue(ar, "", container_data->Inner, element_ptr, inner_size, container_data->InnerContainerData);
                    ar.EndArrayElement();
                }
            }

            ar.EndArray();
            return;
        }

        const std::size_t scope_before = ar.GetScopeDepth();
        const std::size_t count = ar.BeginArray(property_name);
        const bool opened = ar.GetScopeDepth() > scope_before;
        if (!opened)
        {
            return;
        }

        if (container_data->Accessor->Clear)
        {
            container_data->Accessor->Clear(container_ptr);
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            ar.BeginArrayElement();

            void* element_ptr = nullptr;
            if (container_data->Accessor->Add)
            {
                container_data->Accessor->Add(container_ptr, nullptr);
            }

            if (container_data->Accessor->GetElements)
            {
                std::vector<void*> elements = container_data->Accessor->GetElements(container_ptr);
                if (!elements.empty())
                {
                    element_ptr = elements.back();
                }
            }

            if (element_ptr)
            {
                SerializeValue(ar, "", container_data->Inner, element_ptr, inner_size, container_data->InnerContainerData);
            }

            ar.EndArrayElement();
        }

        ar.EndArray();
    }

    void SerializeMapValue(
        Engine::Archive& ar,
        std::string_view property_name,
        void* container_ptr,
        const reflection::ContainerInfo* container_data)
    {
        if (!container_data || !container_data->Accessor)
        {
            return;
        }

        if (ar.IsWriting())
        {
            ar.BeginMap(property_name);

            struct MapWriteContext
            {
                Engine::Archive* Archive = nullptr;
                const reflection::ContainerInfo* Container = nullptr;
            };

            MapWriteContext context{ &ar, container_data };

            auto callback = [](void* element, void* key, void* user_data)
                {
                    auto* ctx = static_cast<MapWriteContext*>(user_data);
                    const std::string key_string = KeyToString(ctx->Container->Key.Type, key);
                    if (key_string.empty())
                    {
                        return;
                    }

                    ctx->Archive->BeginMapElement(const_cast<std::string&>(key_string));
                    SerializeValue(
                        *ctx->Archive,
                        "",
                        ctx->Container->Inner,
                        element,
                        GetKnownValueSize(ctx->Container->Inner, 0),
                        ctx->Container->InnerContainerData
                    );
                    ctx->Archive->EndMapElement();
                };

            container_data->Accessor->ForEach(container_ptr, callback, &context);
            ar.EndMap();
            return;
        }

        const std::size_t scope_before = ar.GetScopeDepth();
        const std::size_t count = ar.BeginMap(property_name);
        const bool opened = ar.GetScopeDepth() > scope_before;
        if (!opened)
        {
            return;
        }

        if (container_data->Accessor->Clear)
        {
            container_data->Accessor->Clear(container_ptr);
        }

        for (std::size_t index = 0; index < count; ++index)
        {
            std::string key_string;
            ar.BeginMapElement(key_string);

            WithParsedKey(container_data->Key.Type, key_string, [&](void* key_ptr)
                {
                    if (!key_ptr || !container_data->Accessor->AddAndGetElement)
                    {
                        return;
                    }

                    void* value_slot = container_data->Accessor->AddAndGetElement(container_ptr, key_ptr);
                    if (value_slot)
                    {
                        SerializeValue(
                            ar,
                            "",
                            container_data->Inner,
                            value_slot,
                            GetKnownValueSize(container_data->Inner, 0),
                            container_data->InnerContainerData
                        );
                    }
                });

            ar.EndMapElement();
        }

        ar.EndMap();
    }

    bool TryReadErasedResourceHandle(
        void* value_ptr,
        std::size_t size,
        Engine::Handle& out_handle)
    {
        const std::size_t handle_size = sizeof(Engine::Handle);
        if (!value_ptr || (size != 0 && size < handle_size))
        {
            out_handle = Engine::Handle{};
            return false;
        }

        std::memcpy(&out_handle, value_ptr, handle_size);
        return true;
    }

    void WriteErasedResourceHandle(
        void* value_ptr,
        std::size_t size,
        const Engine::Handle& handle)
    {
        const std::size_t handle_size = sizeof(Engine::Handle);
        if (!value_ptr || (size != 0 && size < handle_size))
        {
            return;
        }

        std::memcpy(value_ptr, &handle, handle_size);
    }

    void SerializeResourceHandleValue(
        Engine::Archive& ar,
        std::string_view property_name,
        void* value_ptr,
        std::size_t size)
    {
        const bool is_inline = property_name.empty();
        const bool scope_pushed = is_inline ? true : ar.PushScope(property_name);
        if (!scope_pushed)
        {
            if (ar.IsReading())
            {
                WriteErasedResourceHandle(value_ptr, size, Engine::Handle{});
            }
            return;
        }

        Engine::string asset_key;
        Engine::string asset_path;

        const auto& callbacks = bam_reflection::ReflectionSerialization::GetResourceHandleCallbacks();

        if (ar.IsWriting())
        {
            if (callbacks.Write)
            {
                callbacks.Write(value_ptr, size, asset_key, asset_path);
            }

            ar.Process("AssetKey", asset_key);
            ar.Process("AssetPath", asset_path);
        }
        else
        {
            ar.Process("AssetKey", asset_key);
            ar.Process("AssetPath", asset_path);

            bool restored = false;
            if (callbacks.Read)
            {
                restored = callbacks.Read(asset_key, asset_path, value_ptr, size);
            }

            if (!restored)
            {
                WriteErasedResourceHandle(value_ptr, size, Engine::Handle{});
            }
        }

        if (!is_inline)
        {
            ar.PopScope();
        }
    }

    void SerializeValue(
        Engine::Archive& ar,
        std::string_view property_name,
        const reflection::VariableInfo& variable_info,
        void* value_ptr,
        std::size_t size,
        const reflection::ContainerInfo* container_data)
    {
        if (!value_ptr)
        {
            return;
        }

        if (TrySerializeBuiltinValue(ar, property_name, variable_info, value_ptr, size))
        {
            return;
        }

        switch (variable_info.Type)
        {
        case reflection::EPropertyType::Object:
            SerializeObjectValue(ar, property_name, variable_info, value_ptr);
            return;

        case reflection::EPropertyType::Struct:
        case reflection::EPropertyType::UserDefined:
            SerializeStructValue(ar, property_name, variable_info, value_ptr);
            return;

        case reflection::EPropertyType::Array:
        case reflection::EPropertyType::List:
        case reflection::EPropertyType::Set:
            SerializeSequenceValue(ar, property_name, value_ptr, container_data);
            return;

        case reflection::EPropertyType::Map:
            SerializeMapValue(ar, property_name, value_ptr, container_data);
            return;

        case reflection::EPropertyType::ResourceHandle:
            SerializeResourceHandleValue(ar, property_name, value_ptr, size);
            return;

        default:
            return;
        }
    }

    
}

namespace bam_reflection
{
    void ReflectionSerialization::SetObjectInstantiator(ObjectInstantiatorFunction func)
    {
        ObjectInstantiatorSlot() = func;
    }

    ReflectionSerialization::ObjectInstantiatorFunction ReflectionSerialization::GetObjectInstantiator()
    {
        return ObjectInstantiatorSlot();
    }

    void ReflectionSerialization::SetResourceHandleCallbacks(const ResourceHandleCallbacks& callbacks)
    {
        ResourceHandleCallbacksSlot() = callbacks;
    }

    const ReflectionSerialization::ResourceHandleCallbacks& ReflectionSerialization::GetResourceHandleCallbacks()
    {
        return ResourceHandleCallbacksSlot();
    }

    const reflection::TypeInfo* ReflectionSerialization::ResolveTypeInfo(std::string_view type_name)
    {
        return ResolveReflectionType(type_name);
    }

    void* ReflectionSerialization::CreateInstanceByQualifiedName(std::string_view qualified_name)
    {
        return reflection::Registry::Get().CreateInstanceByQualifiedName(qualified_name);
    }

    void ReflectionSerialization::DestroyInstanceByQualifiedName(std::string_view qualified_name, void* instance)
    {
        reflection::Registry::Get().DestroyInstanceByQualifiedName(qualified_name, instance);
    }

    void ReflectionSerialization::SerializeProperty(
        Engine::Archive& ar,
        void* instance,
        const reflection::PropertyInfo& property_info)
    {
        if (!instance || ShouldSkipProperty(property_info))
        {
            return;
        }

        void* value_ptr = reflection::PropertyAccessor::GetValuePtr(instance, property_info);
        if (!value_ptr)
        {
            return;
        }

        ProcessProperty(ar, property_info, value_ptr);
    }

    void ReflectionSerialization::SerializeProperties(
        Engine::Archive& ar,
        void* instance,
        const reflection::TypeInfo& type_info)
    {
        if (!instance)
        {
            return;
        }

        if (!type_info.ParentQualifiedName.empty())
        {
            if (const reflection::TypeInfo* parent = reflection::Registry::Get().ResolveTypeName(type_info.ParentQualifiedName))
            {
                SerializeProperties(ar, instance, *parent);
            }
        }

        for (const reflection::PropertyInfo& property_info : type_info.Properties)
        {
            SerializeProperty(ar, instance, property_info);
        }
    }

    void ReflectionSerialization::ProcessProperty(
        Engine::Archive& ar,
        const reflection::PropertyInfo& property_info,
        void* value_ptr)
    {
        SerializeValue(
            ar,
            property_info.Name,
            property_info.TypeInfo,
            value_ptr,
            property_info.Size,
            property_info.ContainerData
        );
    }
}