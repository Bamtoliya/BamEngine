#include "Material.h"
#include "Mesh.h"
#include "PlatformPath.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include "ResourcePreloader.h"
#include "ResourceUpload.h"
#include "Texture.h"
#include "TexturePlaceholderCache.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "ResourcePreloaderCheck failed: %s\n", message);
        ++g_Failures;
    }

    std::filesystem::path PrepareRoot()
    {
        const auto root = std::filesystem::temp_directory_path() / "BamEngine_ResourcePreloaderCheck";
        std::error_code error;
        std::filesystem::remove_all(root, error);
        std::filesystem::create_directories(root, error);
        return PlatformPath::Normalize(root);
    }

    void WriteMesh(const std::wstring& key, float offset)
    {
        vec3 positions[] = { vec3(offset, 0.0f, 0.0f), vec3(offset + 1.0f, 0.0f, 0.0f), vec3(offset, 1.0f, 0.0f) };
        uint32 indices[] = { 0, 1, 2 };
        Mesh* mesh = Mesh::CreateEmpty();
        Expect(mesh->SetStreamBuffer(EMeshStream::Position, positions, 3, sizeof(vec3)) == EResult::Success &&
            mesh->SetIndexBuffer(indices, 3, sizeof(uint32)) == EResult::Success, "test mesh must be created");
        Expect(ResourceManager::Get().SaveToBinaryFile(mesh, key) == EResult::Success, "test mesh must be saved");
        mesh->Free();
        delete mesh;
    }

    void WriteText(const std::filesystem::path& path, const std::string& text)
    {
        std::ofstream(path, std::ios::binary) << text;
    }

    // 2x2 24비트 BMP
    void WriteBitmap(const std::filesystem::path& path)
    {
        std::vector<unsigned char> bytes(54 + 16, 0x80);
        const auto put = [&](size_t at, unsigned value, int count)
            {
                for (int i = 0; i < count; ++i) bytes[at + i] = static_cast<unsigned char>(value >> (8 * i));
            };
        std::fill(bytes.begin(), bytes.begin() + 54, static_cast<unsigned char>(0));
        bytes[0] = 'B'; bytes[1] = 'M';
        put(2, 70, 4); put(10, 54, 4); put(14, 40, 4); put(18, 2, 4); put(22, 2, 4); put(26, 1, 2); put(28, 24, 2); put(34, 16, 4);
        std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }

    ResourceReference Reference(const std::wstring& key) { return { key, key }; }

    std::wstring MeshKey(int index) { return L"Project://Mesh" + std::to_wstring(index) + L".bammesh"; }

    void Finish(ResourcePreloader& preloader)
    {
        while (!preloader.Update(std::chrono::steady_clock::now() + std::chrono::milliseconds(50))) {}
    }
}

int main()
{
    if (!ReflectionRegistry::Create() || !ResourceManager::Create()) return 1;
    auto& manager = ResourceManager::Get();
    const auto root = PrepareRoot();
    PlatformPath::SetProjectRoot(root);
    PlatformPath::SetProjectAssetRoot(root);

    constexpr int meshCount = 40;
    for (int i = 0; i < meshCount; ++i) WriteMesh(MeshKey(i), static_cast<float>(i));
    WriteBitmap(root / "Albedo.bmp");
    WriteText(root / "Broken.bammesh", "not an archive");
    WriteText(root / "Wall.bammatinst.json",
        R"({"m_TextureBindings":[{"slot":0,"name":"Albedo","texture":{"Null":false,"AssetKey":"Project://Albedo.bmp","AssetPath":"Project://Albedo.bmp"}},{"slot":1,"texture":{"Null":true}}]})");

    // 이미 로드한 리소스는 그대로 쓴다.
    const auto loadedFirst = manager.LoadFile(MeshKey(0));
    Expect(loadedFirst.IsValid(), "the reference mesh must load");

    std::vector<ResourceReference> references;
    for (int i = 0; i < meshCount; ++i) references.push_back(Reference(MeshKey(i)));
    references.push_back(Reference(MeshKey(1)));
    references.push_back(Reference(L"Project://Broken.bammesh"));
    references.push_back(Reference(L"Project://Missing.bammesh"));
    references.push_back(Reference(L"Project://Wall.bammatinst.json"));

    {
        ResourcePreloader preloader;
        size_t callbacks = 0;
        preloader.Start(references);
        Expect(ResourcePreloader::IsAnyActive(), "running workers must be reported");
        while (!preloader.Update(std::chrono::steady_clock::now() + std::chrono::milliseconds(50),
            [&](const std::wstring&, f64) { ++callbacks; })) {}

        const auto stats = preloader.GetStats();
        Expect(!ResourcePreloader::IsAnyActive(), "finished workers must be reported");
        Expect(stats.MeshCount == meshCount - 1 && stats.TextureCount == 1 && callbacks == meshCount, "every new mesh and texture must be prepared once");
        Expect(stats.FinishedCount == stats.ScheduledCount, "every scheduled reference must finish");
    }

    Expect(manager.FindHandleByKey(MeshKey(0)) == loadedFirst, "loaded resources must keep their handle");
    for (int i = 0; i < meshCount; ++i)
    {
        const auto mesh = manager.GetResourceHandle<Mesh>(MeshKey(i));
        const bool valid = mesh.IsValid() && mesh->GetVertexCount() == 3 && mesh->GetIndexCount() == 3 &&
            mesh->GetMin().x == static_cast<float>(i) && mesh->GetKey() == MeshKey(i);
        if (!valid) { Expect(false, "prepared meshes must keep their data and key"); break; }
    }
    const auto texture = manager.GetResourceHandle<Texture>(L"Project://Albedo.bmp");
    Expect(texture.IsValid() && texture->GetCachedHeader().width == 2 && texture->GetCachedHeader().mipLevels == 2, "material textures must be prepared with mips");
    Expect(!manager.FindHandleByKey(L"Project://Broken.bammesh").IsValid(), "broken files must be left to the regular load");

    // 같은 참조를 다시 읽으면 새로 만들지 않는다.
    {
        ResourcePreloader preloader;
        preloader.Start(references);
        Finish(preloader);
        Expect(preloader.GetStats().MeshCount == 0 && preloader.GetStats().TextureCount == 0, "loaded resources must not be prepared again");
    }

    // 같은 키를 다시 등록하면 기존 리소스를 유지하고 새 것을 버린다.
    Material* duplicate = Material::CreateEmpty();
    duplicate->SetKey(MeshKey(3));
    Expect(manager.RegisterPreparedResource(duplicate) == manager.FindHandleByKey(MeshKey(3)) &&
        manager.GetResourceHandle<Mesh>(MeshKey(3))->GetMin().x == 3.0f, "registering a known key must keep the existing resource");
    Expect(!manager.RegisterPreparedResource(nullptr).IsValid(), "a missing resource must not be registered");

    // 다른 키로 등록된 같은 파일도 다시 읽지 않는다.
    WriteMesh(L"Project://Named.bammesh", 5.0f);
    Mesh* named = Mesh::CreateEmpty();
    named->SetKey(L"SemanticMesh");
    named->SetPath(L"Project://Named.bammesh");
    const auto namedHandle = manager.RegisterPreparedResource(named);
    {
        const std::vector<ResourceReference> references2{ Reference(L"Project://Named.bammesh") };
        ResourcePreloader preloader;
        preloader.Start(references2);
        Finish(preloader);
        Expect(namedHandle.IsValid() && preloader.GetStats().MeshCount == 0 && !manager.FindHandleByKey(L"Project://Named.bammesh").IsValid(),
            "a file registered under another key must not be loaded twice");
    }

    // 도중에 그만두어도 남은 작업과 데이터를 정리한다.
    for (int i = 0; i < meshCount; ++i) WriteMesh(L"Project://Cancel" + std::to_wstring(i) + L".bammesh", 0.0f);
    {
        std::vector<ResourceReference> cancelled;
        for (int i = 0; i < meshCount; ++i) cancelled.push_back(Reference(L"Project://Cancel" + std::to_wstring(i) + L".bammesh"));
        ResourcePreloader preloader;
        preloader.Start(cancelled);
    }
    Expect(!ResourcePreloader::IsAnyActive(), "a cancelled preload must stop its workers");

    // 동기 경로도 같은 리소스를 등록한다.
    {
        std::vector<ResourceReference> blocking;
        for (int i = 0; i < meshCount; ++i) blocking.push_back(Reference(L"Project://Cancel" + std::to_wstring(i) + L".bammesh"));
        ResourcePreloader::LoadBlocking(blocking);
        Expect(manager.FindHandleByKey(L"Project://Cancel7.bammesh").IsValid(), "blocking preload must register resources");
    }

    // 대신 보여 줄 것이 있는 리소스는 자리만 먼저 등록하고 내용은 뒤로 미룬다.
    for (const auto* name : { L"Rock", L"Rock_LOD1", L"Rock_LOD2", L"Plain" })
        WriteMesh(L"Project://" + std::wstring(name) + L".bammesh", 7.0f);
    WriteMesh(L"Project://Bad_LOD2.bammesh", 1.0f);
    WriteText(root / "Bad.bammesh", "not an archive");
    WriteBitmap(root / "Stream.bmp");
    WriteBitmap(root / "StreamNormal.bmp");
    WriteText(root / "Stream.bammatinst.json",
        R"({"m_TextureBindings":[{"slot":0,"texture":{"Null":false,"AssetKey":"Project://Stream.bmp","AssetPath":"Project://Stream.bmp"}},{"slot":1,"texture":{"Null":false,"AssetKey":"Project://StreamNormal.bmp","AssetPath":"Project://StreamNormal.bmp"}}]})");
    {
        std::vector<ResourceReference> deferred;
        for (const auto* name : { L"Rock", L"Rock_LOD1", L"Rock_LOD2", L"Plain", L"Bad", L"Bad_LOD2" })
            deferred.push_back(Reference(L"Project://" + std::wstring(name) + L".bammesh"));
        deferred.push_back(Reference(L"Project://Stream.bammatinst.json"));

        ResourcePreloader preloader;
        ResourcePreloader::Options options;
        options.DeferDetail = true;
        preloader.Start(deferred, options);
        bool finished = false;
        while (!finished && !preloader.IsInitialReady())
            finished = preloader.Update(std::chrono::steady_clock::now() + std::chrono::milliseconds(50));
        const auto find = [&](const wchar_t* key) { return manager.GetResource(manager.FindHandleByKey(key)); };
        const auto* rock = find(L"Project://Rock.bammesh");
        const auto* normal = find(L"Project://StreamNormal.bmp");
        Expect(!finished && rock && rock->IsPending() && find(L"Project://Rock_LOD1.bammesh")->IsPending(), "detailed LODs must be registered without contents");
        Expect(find(L"Project://Rock_LOD2.bammesh")->IsResident() && find(L"Project://Plain.bammesh")->IsResident(), "the coarsest LOD and meshes without LODs must be loaded first");
        Expect(normal && normal->IsPending() && normal->GetUploadByteSize() == 4, "material textures must start as a solid placeholder");
        Expect(preloader.GetStats().PlaceholderCount == 5 && preloader.GetStats().CachedPlaceholderCount == 0, "every deferred resource must get a placeholder");

        // 내용이 필요한 접근은 그 자리에서 읽고, 뒤늦게 도착한 내용은 버린다.
        const auto rockHandle = manager.FindHandleByKey(L"Project://Rock.bammesh");
        const auto* rockMesh = static_cast<const Mesh*>(rock);
        const auto version = rockMesh->GetGeometryVersion();
        Expect(rockMesh->GetVertexCount() == 3 && rockMesh->GetMin().x == 7.0f && rock->IsResident() && rockMesh->GetGeometryVersion() != version,
            "using a pending mesh must load it");

        bool otherThread = true;
        auto* pendingTexture = static_cast<Texture*>(manager.GetResource(manager.FindHandleByKey(L"Project://StreamNormal.bmp")));
        std::thread([&] { otherThread = !IsSuccess(pendingTexture->EnsureResident()) && pendingTexture->IsPending(); }).join();
        Expect(otherThread, "other threads must not load pending resources");

        while (!preloader.Update(std::chrono::steady_clock::now() + std::chrono::milliseconds(5)))
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Expect(!ResourcePreloader::IsAnyActive() && manager.FindHandleByKey(L"Project://Rock.bammesh") == rockHandle, "deferred contents must keep the handle");
        Expect(find(L"Project://Rock_LOD1.bammesh")->IsResident() && normal->IsResident() && static_cast<const Texture*>(normal)->GetCachedHeader().width == 2,
            "deferred contents must be loaded");
        const auto* bad = find(L"Project://Bad.bammesh");
        Expect(bad && !bad->IsResident() && !bad->IsPending() && preloader.GetStats().DetailFailedCount == 1, "broken contents must be reported once");
        Expect(!IsSuccess(manager.SaveToBinaryFile(const_cast<Resource*>(bad), L"Project://Bad.bammesh")), "a mesh without contents must not be saved");
        Expect(preloader.GetStats().DetailScheduledCount == 5 && preloader.GetStats().DetailFinishedCount == 5, "every deferred resource must finish");
    }

    // 두 번째부터는 보관한 작은 텍스처를 쓰고, 원본이 바뀌면 쓰지 않는다.
    {
        const auto source = root / "Stream.bmp";
        const auto cached = TexturePlaceholderCache::Load(L"Project://Stream.bmp", source);
        Expect(cached && cached->Header.width == 2 && cached->Header.mipLevels == 2 && cached->Pixels.size() == 20, "streamed textures must leave a small copy");
        Expect(!TexturePlaceholderCache::Load(L"Project://Other.bmp", source), "another key must not use the copy");
        std::ofstream(source, std::ios::binary | std::ios::app) << "changed";
        Expect(!TexturePlaceholderCache::Load(L"Project://Stream.bmp", source), "a changed source must not use the copy");

        TextureBinaryHeader header{};
        header.width = 256;
        header.height = 128;
        header.depth = 1;
        header.mipLevels = Texture::CalculateMipLevels(256, 128);
        header.arraySize = 1;
        header.format = ETextureFormat::R8G8B8A8_UNORM;
        header.dimension = ETextureDimension::Texture2D;
        size_t bytes = 0;
        for (uint32 i = 0; i < header.mipLevels; ++i) bytes += static_cast<size_t>(std::max(1u, 256u >> i)) * std::max(1u, 128u >> i) * 4;
        std::vector<uint8> pixels(bytes, 1);
        pixels[256 * 128 * 4 + 128 * 64 * 4] = 9;
        const auto extracted = TexturePlaceholderCache::Extract(header, pixels);
        Expect(extracted && extracted->Header.width == 64 && extracted->Header.height == 32 && extracted->Header.mipLevels == 7 && extracted->Pixels.front() == 9,
            "the copy must start at the first small mip");
        pixels.pop_back();
        Expect(!TexturePlaceholderCache::Extract(header, pixels), "incomplete pixels must be rejected");
        Expect(TexturePlaceholderCache::MakeSolid(1).Pixels == std::vector<uint8>({ 128, 128, 255, 255 }), "normal maps must start flat");
    }

    // 업로드를 미루는 범위는 그 스레드에만 적용된다.
    {
        const DeferredResourceUpload defer;
        bool otherThread = true;
        std::thread([&] { otherThread = GetResourceUploadRHI() == nullptr; }).join();
        Expect(GetResourceUploadRHI() == nullptr && otherThread, "headless runs must not upload");
    }

    ResourceManager::Destroy();
    ReflectionRegistry::Destroy();
    std::error_code error;
    std::filesystem::remove_all(root, error);
    return g_Failures == 0 ? 0 : 1;
}
