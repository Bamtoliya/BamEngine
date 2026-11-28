#include "CloudResources.h"
#include "CloudTextureGenerator.h"
#include "Material.h"
#include "PlatformPath.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Texture.h"

namespace
{
    using CloudTextureFactory = Engine::Texture* (*)(Engine::RHI* rhi, const std::wstring& key);

    Engine::ResourceHandle<Engine::Texture> GetOrCreateCloudTexture(const std::wstring& key, CloudTextureFactory create, bool& attempted)
    {
        auto& resources = Engine::ResourceManager::Get();
        auto handle = resources.GetResourceHandle<Engine::Texture>(key);
        if (handle.IsValid() || attempted) return handle;

        // 실패해도 매 프레임 다시 시도하지 않는다.
        attempted = true;
        std::error_code error;
        if (std::filesystem::is_regular_file(Engine::PlatformPath::ResolveUri(key), error))
        {
            handle = Engine::ResourceHandle<Engine::Texture>(resources.LoadFile(key));
            if (handle.IsValid()) return handle;
        }

        auto* rhi = Engine::Renderer::IsCreated() ? Engine::Renderer::Get().GetRHI() : nullptr;
        auto* texture = rhi ? create(rhi, key) : nullptr;
        if (!texture)
        {
            ENGINE_LOG_ERROR("Cloud texture could not be created: {}", Engine::Unicode::WideToUtf8(key));
            return {};
        }

        // 다음 실행에서 다시 만들지 않도록 저장한다.
        handle = resources.AddResource<Engine::Texture>(key, texture);

        if (handle.IsValid())
            resources.SaveToBinaryFile(handle.Get(), key);

        return handle;
    }
}

bool Engine::CloudResources::Prepare()
{
    if (!ResourceManager::IsCreated()) return false;

    static bool s_BaseShapeAttempted = false;
    static bool s_WeatherMapAttempted = false;
    const auto baseShape = GetOrCreateCloudTexture(BaseShapeKey,
        [](RHI* rhi, const std::wstring& key) { return CloudTextureGenerator::CreateBaseShapeTexture(rhi, key); }, s_BaseShapeAttempted);
    const auto weatherMap = GetOrCreateCloudTexture(WeatherMapKey,
        [](RHI* rhi, const std::wstring& key) { return CloudTextureGenerator::CreateWeatherMapTexture(rhi, key); }, s_WeatherMapAttempted);
    if (!baseShape.IsValid() || !weatherMap.IsValid()) return false;

    auto& resources = ResourceManager::Get();
    auto skyMaterial = resources.GetResourceHandle<Material>(SkyMaterialKey);
    if (!skyMaterial.IsValid())
        skyMaterial = ResourceHandle<Material>(resources.FindHandleByFilePath(SkyMaterialKey));
    auto* material = skyMaterial.Get();
    if (!material) return false;

    // 재질을 다시 불러오면 연결이 지워지므로 호출할 때마다 확인한다.
    if (material->GetTextureHandleBySlot(0).Get() != baseShape.Get())
        material->SetTextureBinding("CloudBaseShape3D", 0, baseShape);
    if (material->GetTextureHandleBySlot(1).Get() != weatherMap.Get())
        material->SetTextureBinding("CloudWeatherMap2D", 1, weatherMap);
    return true;
}
