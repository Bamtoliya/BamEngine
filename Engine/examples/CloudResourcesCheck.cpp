#include "CloudResources.h"
#include "Material.h"
#include "PlatformPath.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include "Texture.h"

#include <cstdio>
#include <filesystem>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "CloudResourcesCheck failed: %s\n", message);
        ++g_Failures;
    }

    // 시작 목록은 "Resources/..." 경로로 로드하고 ResourceManager는 가상 URI로 등록한다.
    void CheckKeys()
    {
        const auto engineFile = PlatformPath::GetEngineResourceRoot() / L"Material" / L"SkyMaterial.bammat.json";
        Expect(PlatformPath::ToVirtualUri(engineFile) == CloudResources::SkyMaterialKey, "sky material key must match the loaded key");
        const auto textures = PlatformPath::GetProjectAssetRoot() / L"Texture";
        Expect(PlatformPath::ToVirtualUri(textures / L"CloudBaseShape3D.bamtex") == CloudResources::BaseShapeKey, "base shape key must match the loaded key");
        Expect(PlatformPath::ToVirtualUri(textures / L"CloudWeatherMap2D.bamtex") == CloudResources::WeatherMapKey, "weather map key must match the loaded key");
    }

    void CheckBinding()
    {
        auto& resources = ResourceManager::Get();
        const auto shape = resources.AddResource<Texture>(CloudResources::BaseShapeKey, Texture::CreateEmpty());
        const auto weather = resources.AddResource<Texture>(CloudResources::WeatherMapKey, Texture::CreateEmpty());
        const auto material = resources.AddResource<Material>(CloudResources::SkyMaterialKey, Material::CreateEmpty());
        auto* sky = material.Get();
        Expect(sky != nullptr, "sky material must be registered");
        if (!sky) return;

        Expect(CloudResources::Prepare(), "prepared textures must be bound to the sky material");
        Expect(sky->GetTextureBySlot(0) == shape.Get() && sky->GetTextureBySlot(1) == weather.Get(), "cloud textures must use slots 0 and 1");
        Expect(CloudResources::Prepare() && sky->GetTextureBindings().size() == 2, "preparing again must not add bindings");

        // 재질을 다시 불러와 연결이 바뀌어도 복구해야 한다.
        sky->SetTextureBySlot(0, ResourceHandle<Texture>());
        Expect(CloudResources::Prepare() && sky->GetTextureBySlot(0) == shape.Get(), "a lost binding must be restored");
    }
}

int main()
{
    if (!ReflectionRegistry::Create() || !ResourceManager::Create()) return 1;
    CheckKeys();
    CheckBinding();
    ResourceManager::Destroy();
    ReflectionRegistry::Destroy();
    return g_Failures == 0 ? 0 : 1;
}
