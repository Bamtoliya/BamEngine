#include "Material.h"
#include "Mesh.h"
#include "PlatformPath.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include "ResourcePreloader.h"
#include "ResourceStreamer.h"
#include "ResourceUpload.h"
#include "Texture.h"
#include "TextureMipCache.h"

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

    // 정사각 24비트 BMP
    void WriteBitmap(const std::filesystem::path& path, unsigned size = 2)
    {
        const unsigned rowBytes = (size * 3 + 3) / 4 * 4;
        std::vector<unsigned char> bytes(54 + static_cast<size_t>(rowBytes) * size, 0x80);
        const auto put = [&](size_t at, unsigned value, int count)
            {
                for (int i = 0; i < count; ++i) bytes[at + i] = static_cast<unsigned char>(value >> (8 * i));
            };
        std::fill(bytes.begin(), bytes.begin() + 54, static_cast<unsigned char>(0));
        bytes[0] = 'B'; bytes[1] = 'M';
        put(2, static_cast<unsigned>(bytes.size()), 4); put(10, 54, 4); put(14, 40, 4); put(18, size, 4); put(22, size, 4);
        put(26, 1, 2); put(28, 24, 2); put(34, rowBytes * size, 4);
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

    // 스트리머가 있으면 대신 보여 줄 것이 있는 리소스는 자리만 등록하고 필요한 만큼 이어 읽는다.
    for (const auto* name : { L"Rock", L"Rock_LOD1", L"Rock_LOD2", L"Plain", L"Far", L"Far_LOD2" })
        WriteMesh(L"Project://" + std::wstring(name) + L".bammesh", 7.0f);
    WriteMesh(L"Project://Bad_LOD2.bammesh", 1.0f);
    WriteText(root / "Bad.bammesh", "not an archive");
    WriteBitmap(root / "Stream.bmp", 256);
    WriteBitmap(root / "StreamNormal.bmp", 256);
    WriteBitmap(root / "Tiny.bmp", 2);
    WriteText(root / "Stream.bammatinst.json",
        R"({"m_TextureBindings":[{"slot":0,"texture":{"Null":false,"AssetKey":"Project://Stream.bmp","AssetPath":"Project://Stream.bmp"}},{"slot":1,"texture":{"Null":false,"AssetKey":"Project://StreamNormal.bmp","AssetPath":"Project://StreamNormal.bmp"}},{"slot":3,"texture":{"Null":false,"AssetKey":"Project://Tiny.bmp","AssetPath":"Project://Tiny.bmp"}}]})");
    const auto find = [&](const wchar_t* key) { return manager.GetResource(manager.FindHandleByKey(key)); };
    std::vector<ResourceReference> streamed;
    for (const auto* name : { L"Rock", L"Rock_LOD1", L"Rock_LOD2", L"Plain", L"Far", L"Far_LOD2", L"Bad", L"Bad_LOD2" })
        streamed.push_back(Reference(L"Project://" + std::wstring(name) + L".bammesh"));
    streamed.push_back(Reference(L"Project://Stream.bammatinst.json"));
    if (!ResourceStreamer::Create()) return 1;
    {
        auto& streamer = ResourceStreamer::Get();
        ResourcePreloader preloader;
        preloader.Start(streamed);
        Finish(preloader);
        auto* rock = static_cast<Mesh*>(find(L"Project://Rock.bammesh"));
        auto* rock1 = static_cast<Mesh*>(find(L"Project://Rock_LOD1.bammesh"));
        auto* distant = static_cast<Mesh*>(find(L"Project://Far.bammesh"));
        auto* bad = static_cast<Mesh*>(find(L"Project://Bad.bammesh"));
        auto* albedo = static_cast<Texture*>(find(L"Project://Stream.bmp"));
        auto* normal = static_cast<Texture*>(find(L"Project://StreamNormal.bmp"));
        if (!rock || !rock1 || !distant || !bad || !albedo || !normal) { Expect(false, "deferred resources must be registered"); return 1; }
        Expect(rock->IsPending() && rock1->IsPending() && find(L"Project://Rock_LOD2.bammesh")->IsResident() && find(L"Project://Plain.bammesh")->IsResident(),
            "detailed LODs must be registered without contents");
        Expect(albedo->IsPending() && albedo->GetUploadByteSize() == 4 && find(L"Project://Tiny.bmp")->IsResident(), "material textures must start as a solid placeholder unless they are tiny");
        Expect(preloader.GetStats().PlaceholderCount == 6 && preloader.GetStats().CachedPlaceholderCount == 0, "every deferred resource must get a placeholder");
        Expect(streamer.GetStats().MeshCount == 4 && streamer.GetStats().TextureCount == 2 && streamer.HasEntries(), "deferred resources must be streamed");

        // 요청한 것만 읽는다.
        const auto run = [&](auto&& request)
            {
                for (int i = 0; i < 400; ++i)
                {
                    request();
                    streamer.Update(1000.0);
                    if (i > 2 && !streamer.IsBusy()) break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            };
        run([&] { streamer.RequestMesh(*rock, 1.0f); streamer.RequestMesh(*bad, 1.0f); ResourceStreamer::NoteBound(*albedo); });
        Expect(rock->IsResident() && rock->GetVertexCount() == 3 && rock1->IsPending() && distant->IsPending(), "only requested meshes must load");
        Expect(albedo->IsPending() && albedo->GetUploadByteSize() == 0 && streamer.GetStats().FullTextureCount == 1, "bound textures without a request must load every mip");
        Expect(!bad->IsResident() && !bad->IsPending() && streamer.GetStats().FailedCount == 1 && streamer.GetStats().MeshCount == 3, "broken contents must be reported once");
        Expect(!IsSuccess(manager.SaveToBinaryFile(bad, L"Project://Bad.bammesh")), "a mesh without contents must not be saved");

        // 밉은 화면 크기에 맞춰 읽는다.
        Material* material = Material::CreateEmpty();
        material->SetTextureBinding("Normal", 1, manager.GetResourceHandle<Texture>(L"Project://StreamNormal.bmp"));
        run([&] { streamer.RequestMesh(*rock, 1.0f); streamer.RequestMaterial(*material, 100.0f, 1.0f); });
        const auto halfUsed = streamer.GetStats().UsedBytes;
        Expect(normal->IsPending() && streamer.GetStats().FullTextureCount == 1, "distant textures must not load every mip");
        run([&] { streamer.RequestMesh(*rock, 1.0f); streamer.RequestMaterial(*material, 1000.0f, 1.0f); });
        Expect(streamer.GetStats().FullTextureCount == 2 && streamer.GetStats().UsedBytes > halfUsed, "nearby textures must load every mip");

        // 내용을 직접 쓰면 그 자리에서 읽고 더 관리하지 않는다.
        Expect(distant->GetVertexCount() == 3 && distant->IsResident(), "using a pending mesh must load it");
        streamer.Update();
        Expect(distant->GetStreamSlot() == 0 && streamer.GetStats().MeshCount == 2, "a mesh loaded by its user must no longer be streamed");
        bool otherThread = true;
        std::thread([&] { otherThread = !IsSuccess(rock1->EnsureResident()) && rock1->IsPending(); }).join();
        Expect(otherThread, "other threads must not load pending resources");
        Expect(albedo->GetCachedHeader().width == 256 && albedo->IsResident(), "using a streamed texture must load the original");
        streamer.Update();
        Expect(albedo->GetStreamSlot() == 0, "a texture loaded by its user must no longer be streamed");

        // 내린 메시는 같은 핸들로 다시 읽는다.
        const auto rockHandle = manager.FindHandleByKey(L"Project://Rock.bammesh");
        const auto geometryVersion = rock->GetGeometryVersion();
        rock->ReleaseContents();
        Expect(rock->IsPending() && rock->GetGeometryVersion() != geometryVersion && rock->GetUploadByteSize() == 0, "released meshes must drop their contents");
        Expect(rock->GetMin().x == 7.0f && manager.FindHandleByKey(L"Project://Rock.bammesh") == rockHandle, "released meshes must load again");
        material->Free();
        delete material;
    }
    ResourceStreamer::Destroy();
    Expect(!ResourcePreloader::IsAnyActive() && find(L"Project://Rock_LOD1.bammesh")->GetStreamSlot() == 0, "a destroyed streamer must leave resources alone");

    // 두 번째부터는 보관한 밉을 쓰고, 원본이 바뀌면 쓰지 않는다.
    {
        const auto source = root / "Stream.bmp";
        const auto layout = TextureMipCache::ReadLayout(L"Project://Stream.bmp", source);
        Expect(layout && layout->Width == 256 && layout->Height == 256 && layout->MipLevels == 9, "streamed textures must leave their mips");
        const auto smallMips = TextureMipCache::Load(L"Project://Stream.bmp", source, layout ? layout->FindMipForSize(TextureMipCache::PlaceholderSize) : 0);
        Expect(smallMips && smallMips->Header.width == 64 && smallMips->Header.mipLevels == 7 && smallMips->Pixels.size() == 21844, "small mips must be read without the large ones");
        Expect(!TextureMipCache::Load(L"Project://Stream.bmp", source, 9), "a missing mip must not be read");
        Expect(!TextureMipCache::ReadLayout(L"Project://Other.bmp", source), "another key must not use the cache");
        std::ofstream(source, std::ios::binary | std::ios::app) << "changed";
        Expect(!TextureMipCache::ReadLayout(L"Project://Stream.bmp", source), "a changed source must not use the cache");

        TextureBinaryHeader header{};
        header.width = 256;
        header.height = 128;
        header.depth = 1;
        header.mipLevels = Texture::CalculateMipLevels(256, 128);
        header.arraySize = 1;
        header.format = ETextureFormat::R8G8B8A8_UNORM;
        header.dimension = ETextureDimension::Texture2D;
        std::vector<uint8> pixels(static_cast<size_t>(TextureMipLayout{ 256, 128, header.mipLevels }.GetTailBytes(0)), 1);
        pixels[256 * 128 * 4 + 128 * 64 * 4] = 9;
        const auto sliced = TextureMipCache::Slice(header, pixels, 2);
        Expect(sliced && sliced->Header.width == 64 && sliced->Header.height == 32 && sliced->Header.mipLevels == 7 && sliced->Pixels.front() == 9,
            "a slice must start at the requested mip");
        const auto otherSource = root / "StreamNormal.bmp";
        Expect(TextureMipCache::Store(L"Sliced", otherSource, header, pixels), "mips must be stored");
        const auto stored = TextureMipCache::Load(L"Sliced", otherSource, 0);
        Expect(stored && stored->Pixels == pixels, "stored mips must keep their order");
        const auto storedSlice = TextureMipCache::Load(L"Sliced", otherSource, 2);
        Expect(storedSlice && sliced && storedSlice->Pixels == sliced->Pixels, "reading part of the mips must match a slice");
        pixels.pop_back();
        Expect(!TextureMipCache::Slice(header, pixels, 0) && !TextureMipCache::Store(L"Sliced", otherSource, header, pixels), "incomplete pixels must be rejected");
        Expect(TextureMipCache::MakeSolid(1).Pixels == std::vector<uint8>({ 128, 128, 255, 255 }), "normal maps must start flat");
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
