#include <BamReflectionSerialization.h>

#include "ResourceManager.h"

#include <cstring>
#include <string_view>

namespace
{
    bool WriteResourceHandleCallback(
        const void* erased_handle,
        std::size_t handle_size,
        std::string& out_asset_key,
        std::string& out_asset_path)
    {
        if (!erased_handle || handle_size < sizeof(Engine::Handle))
        {
            return false;
        }

        Engine::Handle handle;
        std::memcpy(&handle, erased_handle, sizeof(handle));

        if (!handle.IsValid())
        {
            out_asset_key.clear();
            out_asset_path.clear();
            return true;
        }

        Engine::Resource* resource = Engine::ResourceManager::Get().GetResource(handle);
        if (!resource)
        {
            out_asset_key.clear();
            out_asset_path.clear();
            return true;
        }

        out_asset_key = Engine::WStrToStr(resource->GetKey());
        out_asset_path = Engine::WStrToStr(resource->GetPath());
        return true;
    }

    bool ReadResourceHandleCallback(
        std::string_view asset_key,
        std::string_view asset_path,
        void* erased_handle,
        std::size_t handle_size)
    {
        if (!erased_handle || handle_size < sizeof(Engine::Handle))
        {
            return false;
        }

        Engine::Handle resolved_handle;

        if (!asset_key.empty())
        {
            resolved_handle = Engine::ResourceManager::Get().FindHandleByKey(
                Engine::StrToWStr(std::string(asset_key))
            );
        }

        if (!resolved_handle.IsValid() && !asset_path.empty())
        {
            resolved_handle = Engine::ResourceManager::Get().LoadFile(
                Engine::StrToWStr(std::string(asset_path))
            );
        }

        Engine::Handle* target_handle = static_cast<Engine::Handle*>(erased_handle);

        if (target_handle->IsValid())
        {
            Engine::ResourceManager::Get().ReleaseResource(*target_handle);
        }

        *target_handle = resolved_handle;

        if (target_handle->IsValid())
        {
            Engine::ResourceManager::Get().AddRefResource(*target_handle);
        }
        return true;
    }

    struct ReflectionSerializationRegistrar
    {
        ReflectionSerializationRegistrar()
        {
            bam_reflection::ReflectionSerialization::ResourceHandleCallbacks callbacks = {};
            callbacks.Write = &WriteResourceHandleCallback;
            callbacks.Read = &ReadResourceHandleCallback;

            bam_reflection::ReflectionSerialization::SetResourceHandleCallbacks(callbacks);
        }
    };

    ReflectionSerializationRegistrar g_reflection_serialization_registrar;
}