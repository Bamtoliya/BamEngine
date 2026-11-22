#include "ResourcePreloader.h"
#include "Mesh.h"
#include "PlatformPath.h"
#include "ResourceManager.h"
#include "ResourceUpload.h"
#include "Texture.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace Engine
{
    namespace ResourcePreloaderDetail
    {
        constexpr uint32 MaxPreloadWorkers = 8;
        // 등록을 기다리는 CPU 데이터가 이보다 많으면 작업 스레드가 쉰다.
        constexpr size_t MaxPendingPreloadBytes = 256u * 1024u * 1024u;
        // 새로 읽을 리소스가 이보다 적으면 스레드를 만들지 않는다.
        constexpr size_t MinBlockingPreloadCount = 16;

        static std::atomic<int32> s_ActivePreloaders = 0;

        enum class EPreloadKind : uint8
        {
            Other,
            Mesh,
            Texture,
            Material
        };

        // LoadFile과 같은 규칙으로 로더 확장자를 정한다(.bammesh.json 같은 이중 확장자 포함).
        static std::wstring GetPreloadExtension(const std::filesystem::path& path)
        {
            auto extension = ToLowerWStr(path.extension().wstring());
            if (extension == L".json" || extension == L".beve")
            {
                const auto embedded = ToLowerWStr(path.stem().extension().wstring());
                if (embedded.rfind(L".bam", 0) == 0) extension = embedded;
            }
            return extension;
        }

        static EPreloadKind ClassifyPreloadFile(const std::filesystem::path& path)
        {
            const auto extension = GetPreloadExtension(path);
            if (extension == L".bammesh") return EPreloadKind::Mesh;
            if (extension == L".bammat" || extension == L".bammatinst") return EPreloadKind::Material;
            for (const auto* texture : { L".png", L".jpg", L".jpeg", L".bmp", L".tga", L".dds", L".bamtex" })
                if (extension == texture) return EPreloadKind::Texture;
            return EPreloadKind::Other;
        }

        // 참조가 가리키는 파일. 없으면 비어 있고, 그 참조는 호출자의 기존 로드가 처리한다.
        static std::filesystem::path ResolvePreloadFile(const ResourceReference& reference)
        {
            std::error_code error;
            for (const auto* candidate : { &reference.AssetKey, &reference.AssetPath })
            {
                if (candidate->empty()) continue;
                auto path = PlatformPath::ResolveUri(*candidate);
                if (std::filesystem::is_regular_file(path, error)) return path;
            }
            return {};
        }

        // LoadResource와 같은 규칙으로 아카이브 종류를 정한다.
        static bool LoadPreloadArchive(const std::filesystem::path& path, const std::function<void(archive::IArchive&)>& read)
        {
            if (ToLowerWStr(path.extension().wstring()) == L".json")
            {
                archive::JsonArchive json;
                if (!json.LoadFromFile(path)) return false;
                read(json);
                return true;
            }
            archive::BeveArchive beve;
            if (!beve.LoadFromFile(path)) return false;
            read(beve);
            return true;
        }

        template<typename T, typename Desc>
        Resource* PreparePreloadResource(const std::wstring& key, const std::filesystem::path& path)
        {
            const DeferredResourceUpload defer;
            const auto extension = ToLowerWStr(path.extension().wstring());
            if (extension != L".json" && extension != L".beve" && extension.rfind(L".bam", 0) != 0)
            {
                Desc desc;
                desc.Key = key;
                desc.Path = path.wstring();
                return T::Create(&desc);
            }

            T* resource = T::CreateEmpty();
            archive::ArchiveResult result{ archive::ArchiveErrorCode::InvalidDocument, {} };
            if (resource && LoadPreloadArchive(path, [&](archive::IArchive& ar) { result = resource->ReadArchive(ar); }) && result)
            {
                resource->SetKey(key);
                resource->SetPath(path.wstring());
                return resource;
            }
            if (resource)
            {
                resource->Free();
                delete static_cast<Resource*>(resource);
            }
            return nullptr;
        }

        static std::vector<ResourceReference> ScanMaterialTextures(const std::filesystem::path& path)
        {
            std::vector<ResourceReference> references;
            static_cast<void>(LoadPreloadArchive(path, [&](archive::IArchive& ar)
                {
                    bool exists = false;
                    if (!ar.HasField("m_TextureBindings", exists) || !exists) return;
                    static_cast<void>(ar.ReadObjectArray("m_TextureBindings", [&](archive::IArchive& binding, std::size_t) -> archive::ArchiveResult
                        {
                            bool hasTexture = false;
                            auto result = binding.HasField("texture", hasTexture);
                            if (!result || !hasTexture) return result;

                            ResourceReference reference;
                            result = binding.ReadObject("texture", reference, [](archive::IArchive& texture, ResourceReference& value) -> archive::ArchiveResult
                                {
                                    bool hasField = false;
                                    auto read = texture.HasField("AssetKey", hasField);
                                    if (read && hasField) read = texture.Read("AssetKey", value.AssetKey);
                                    if (read) read = texture.HasField("AssetPath", hasField);
                                    if (read && hasField) read = texture.Read("AssetPath", value.AssetPath);
                                    return read;
                                });
                            if (result) references.push_back(std::move(reference));
                            return result;
                        }));
                }));
            return references;
        }
    }

    using namespace ResourcePreloaderDetail;

    struct ResourcePreloader::State
    {
        struct FinishedJob
        {
            std::wstring Key;
            Resource* Prepared = nullptr;
            size_t Bytes = 0;
            EPreloadKind Kind = EPreloadKind::Other;
        };

        mutable std::mutex Mutex;
        std::condition_variable WorkerSignal;
        std::condition_variable MainSignal;
        std::deque<ResourceReference> Pending;
        std::unordered_set<std::wstring> ScheduledKeys;
        std::deque<FinishedJob> Finished;
        size_t PendingBytes = 0;
        uint32 Running = 0;
        bool Cancelled = false;
        std::vector<std::thread> Workers;
        Stats Values;
        bool Active = false;

        [[nodiscard]] bool IsWorkDone() const { return Pending.empty() && Running == 0; }

        void Run()
        {
            std::unique_lock lock(Mutex);
            while (true)
            {
                WorkerSignal.wait(lock, [this]
                    {
                        return Cancelled || IsWorkDone() || (!Pending.empty() && PendingBytes < MaxPendingPreloadBytes);
                    });
                if (Cancelled || IsWorkDone())
                {
                    WorkerSignal.notify_all();
                    MainSignal.notify_all();
                    return;
                }

                const ResourceReference reference = std::move(Pending.front());
                Pending.pop_front();
                ++Running;
                lock.unlock();

                FinishedJob job;
                std::vector<ResourceReference> discovered;
                const auto begin = std::chrono::steady_clock::now();
                try
                {
                    Process(reference, job, discovered);
                }
                catch (const std::exception& error)
                {
                    ENGINE_LOG_WARN("Resource preload failed: key=\"{}\", error={}", WStrToStr(reference.AssetKey), error.what());
                }
                const f64 elapsed = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - begin).count();

                lock.lock();
                --Running;
                Values.WorkerMilliseconds += elapsed;
                Values.SlowestWorkerMilliseconds = std::max(Values.SlowestWorkerMilliseconds, elapsed);
                ++Values.FinishedCount;
                // 재질이 참조하는 텍스처는 오래 걸리므로 먼저 읽는다.
                Values.ScheduledCount += discovered.size();
                for (auto& texture : discovered) Pending.push_front(std::move(texture));
                if (job.Prepared)
                {
                    PendingBytes += job.Bytes;
                    Finished.push_back(std::move(job));
                }
                WorkerSignal.notify_all();
                MainSignal.notify_all();
            }
        }

        void Process(const ResourceReference& reference, FinishedJob& job, std::vector<ResourceReference>& discovered)
        {
            const auto path = ResolvePreloadFile(reference);
            if (path.empty()) return;
            job.Kind = ClassifyPreloadFile(path);
            if (job.Kind == EPreloadKind::Other) return;

            job.Key = ResourceManager::ResolveFileKey(path);
            {
                const std::lock_guard lock(Mutex);
                if (!ScheduledKeys.insert(job.Key).second) return;
            }
            // 다른 키로 이미 등록된 같은 파일도 다시 읽지 않는다. 경로 비교는 작업 스레드에서 끝낸다.
            if (ResourceManager::Get().FindHandleByFilePath(path).IsValid()) return;

            switch (job.Kind)
            {
            case EPreloadKind::Mesh: job.Prepared = PreparePreloadResource<Mesh, MeshCreateDesc>(job.Key, path); break;
            case EPreloadKind::Texture: job.Prepared = PreparePreloadResource<Texture, TextureCreateDesc>(job.Key, path); break;
            case EPreloadKind::Material: discovered = ScanMaterialTextures(path); break;
            default: break;
            }
            if (job.Prepared) job.Bytes = job.Prepared->GetUploadByteSize();
        }

        void Stop()
        {
            {
                const std::lock_guard lock(Mutex);
                Cancelled = true;
            }
            WorkerSignal.notify_all();
            JoinWorkers();
        }

        void JoinWorkers()
        {
            for (auto& worker : Workers)
                if (worker.joinable()) worker.join();
            Workers.clear();
            if (Active)
            {
                Active = false;
                --s_ActivePreloaders;
            }
        }
    };

    ResourcePreloader::ResourcePreloader()
        : m_State(std::make_unique<State>())
    {
    }

    ResourcePreloader::~ResourcePreloader()
    {
        m_State->Stop();
        for (const auto& job : m_State->Finished)
        {
            job.Prepared->Free();
            delete job.Prepared;
        }
    }

    void ResourcePreloader::Start(std::span<const ResourceReference> references)
    {
        auto& state = *m_State;
        if (state.Active || !state.Workers.empty() || references.empty() || !ResourceManager::IsCreated()) return;

        // 재질을 먼저 훑어 텍스처를 일찍 읽기 시작한다.
        for (const auto& reference : references)
        {
            const std::filesystem::path path(reference.AssetPath.empty() ? reference.AssetKey : reference.AssetPath);
            if (ClassifyPreloadFile(path) == EPreloadKind::Material) state.Pending.push_front(reference);
            else state.Pending.push_back(reference);
        }
        state.Values.ScheduledCount = state.Pending.size();

        const uint32 hardware = std::max(1u, std::thread::hardware_concurrency());
        const uint32 workerCount = std::clamp(hardware > 2 ? hardware - 2 : 1u, 1u, MaxPreloadWorkers);
        state.Active = true;
        ++s_ActivePreloaders;
        try
        {
            for (uint32 i = 0; i < workerCount; ++i)
                state.Workers.emplace_back([&state] { state.Run(); });
        }
        catch (const std::system_error& error)
        {
            ENGINE_LOG_WARN("Resource preload worker could not start: {}", error.what());
        }
        state.Values.WorkerCount = static_cast<uint32>(state.Workers.size());
        if (state.Workers.empty())
        {
            // 호출자의 기존 로드가 모두 처리한다.
            state.Pending.clear();
            state.JoinWorkers();
        }
    }

    bool ResourcePreloader::Update(std::chrono::steady_clock::time_point deadline, const LoadedCallback& onLoaded)
    {
        auto& state = *m_State;
        if (!ResourceManager::IsCreated())
        {
            state.Stop();
            return true;
        }
        auto& manager = ResourceManager::Get();

        size_t uploadedBytes = 0;
        while (true)
        {
            State::FinishedJob job;
            {
                std::unique_lock lock(state.Mutex);
                state.MainSignal.wait_until(lock, deadline, [&state] { return !state.Finished.empty() || state.IsWorkDone(); });
                if (state.Finished.empty())
                {
                    if (!state.IsWorkDone()) return false;
                    break;
                }
                job = std::move(state.Finished.front());
                state.Finished.pop_front();
                state.PendingBytes -= job.Bytes;
            }
            state.WorkerSignal.notify_all();

            const auto begin = std::chrono::steady_clock::now();
            static_cast<void>(manager.RegisterPreparedResource(job.Prepared));
            const auto end = std::chrono::steady_clock::now();
            const f64 elapsed = std::chrono::duration<f64, std::milli>(end - begin).count();
            uploadedBytes += job.Bytes;
            {
                const std::lock_guard lock(state.Mutex);
                state.Values.MainMilliseconds += elapsed;
                (job.Kind == EPreloadKind::Mesh ? state.Values.MeshMainMilliseconds : state.Values.TextureMainMilliseconds) += elapsed;
                state.Values.UploadBytes += job.Bytes;
                ++(job.Kind == EPreloadKind::Mesh ? state.Values.MeshCount : state.Values.TextureCount);
            }
            if (onLoaded) onLoaded(job.Key, elapsed);
            if (end >= deadline || uploadedBytes >= MaxUploadBytesPerUpdate) return false;
        }

        state.JoinWorkers();
        return true;
    }

    ResourcePreloader::Stats ResourcePreloader::GetStats() const
    {
        const std::lock_guard lock(m_State->Mutex);
        return m_State->Values;
    }

    void ResourcePreloader::LoadBlocking(std::span<const ResourceReference> references)
    {
        if (!ResourceManager::IsCreated()) return;

        auto& manager = ResourceManager::Get();
        std::vector<ResourceReference> missing;
        for (const auto& reference : references)
        {
            if (!manager.FindHandleByKey(reference.AssetKey).IsValid() && !manager.FindHandleByKey(reference.AssetPath).IsValid())
                missing.push_back(reference);
        }
        if (missing.size() < MinBlockingPreloadCount) return;

        ResourcePreloader preloader;
        preloader.Start(missing);
        while (!preloader.Update(std::chrono::steady_clock::now() + std::chrono::milliseconds(100))) {}
    }

    bool ResourcePreloader::IsAnyActive()
    {
        return s_ActivePreloaders.load() > 0;
    }
}
