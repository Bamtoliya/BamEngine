#include "ResourcePreloader.h"
#include "Mesh.h"
#include "PlatformPath.h"
#include "ResourceManager.h"
#include "ResourceUpload.h"
#include "Texture.h"
#include "TexturePlaceholderCache.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_set>

namespace Engine
{
    namespace ResourcePreloaderDetail
    {
        constexpr uint32 MaxPreloadWorkers = 8;
        // 등록을 기다리는 CPU 데이터가 이보다 많으면 작업 스레드가 쉰다.
        constexpr size_t MaxPendingPreloadBytes = 256u * 1024u * 1024u;
        constexpr size_t MaxPendingDetailBytes = 128u * 1024u * 1024u;
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

        static Resource* PreparePreloadFile(EPreloadKind kind, const std::wstring& key, const std::filesystem::path& path)
        {
            if (kind == EPreloadKind::Mesh) return PreparePreloadResource<Mesh, MeshCreateDesc>(key, path);
            if (kind == EPreloadKind::Texture) return PreparePreloadResource<Texture, TextureCreateDesc>(key, path);
            return nullptr;
        }

        static void DiscardPreparedResource(Resource* resource)
        {
            if (!resource) return;
            resource->Free();
            delete resource;
        }

        // 항상 2D RGBA8로 읽는 형식. 3D일 수 있는 .dds·.bamtex는 임시 텍스처로 대신할 수 없다.
        static bool IsImageTexture(const std::filesystem::path& path)
        {
            const auto extension = GetPreloadExtension(path);
            for (const auto* image : { L".png", L".jpg", L".jpeg", L".bmp", L".tga" })
                if (extension == image) return true;
            return false;
        }

        // 임포터는 LOD 메시를 "이름_LOD1.bammesh"로 저장한다. 앞부분(이름)과 LOD 번호, 확장자를 나눈다.
        struct MeshLODName
        {
            std::wstring Stem;
            std::wstring Extension;
            int32 LOD = 0;
        };

        static std::optional<MeshLODName> ParseMeshLODName(const std::wstring& key)
        {
            const auto name = ToLowerWStr(key);
            const size_t dot = name.rfind(L".bammesh");
            if (dot == std::wstring::npos) return std::nullopt;
            MeshLODName result{ name.substr(0, dot), name.substr(dot), 0 };
            for (int32 lod = 1; lod <= 2; ++lod)
            {
                const auto suffix = L"_lod" + std::to_wstring(lod);
                if (!result.Stem.ends_with(suffix)) continue;
                result.Stem.resize(result.Stem.size() - suffix.size());
                result.LOD = lod;
                break;
            }
            return result;
        }

        struct DiscoveredTexture
        {
            ResourceReference Reference;
            uint32 Slot = 0;
        };

        static std::vector<DiscoveredTexture> ScanMaterialTextures(const std::filesystem::path& path)
        {
            std::vector<DiscoveredTexture> references;
            static_cast<void>(LoadPreloadArchive(path, [&](archive::IArchive& ar)
                {
                    bool exists = false;
                    if (!ar.HasField("m_TextureBindings", exists) || !exists) return;
                    static_cast<void>(ar.ReadObjectArray("m_TextureBindings", [&](archive::IArchive& binding, std::size_t) -> archive::ArchiveResult
                        {
                            bool hasTexture = false;
                            auto result = binding.HasField("texture", hasTexture);
                            if (!result || !hasTexture) return result;

                            DiscoveredTexture discovered;
                            bool hasSlot = false;
                            result = binding.HasField("slot", hasSlot);
                            if (result && hasSlot) result = binding.Read("slot", discovered.Slot);
                            if (!result) return result;
                            result = binding.ReadObject("texture", discovered.Reference, [](archive::IArchive& texture, ResourceReference& value) -> archive::ArchiveResult
                                {
                                    bool hasField = false;
                                    auto read = texture.HasField("AssetKey", hasField);
                                    if (read && hasField) read = texture.Read("AssetKey", value.AssetKey);
                                    if (read) read = texture.HasField("AssetPath", hasField);
                                    if (read && hasField) read = texture.Read("AssetPath", value.AssetPath);
                                    return read;
                                });
                            if (result) references.push_back(std::move(discovered));
                            return result;
                        }));
                }));
            return references;
        }
    }

    using namespace ResourcePreloaderDetail;

    struct ResourcePreloader::State
    {
        struct Job
        {
            ResourceReference Reference;
            uint32 MaterialSlot = 0;
            bool FromMaterial = false;
        };

        // 자리만 등록한 리소스의 내용 읽기
        struct DetailJob
        {
            std::wstring Key;
            std::filesystem::path Path;
            EPreloadKind Kind = EPreloadKind::Other;
            bool StorePlaceholder = false;
        };

        struct FinishedJob
        {
            std::wstring Key;
            Resource* Prepared = nullptr;
            size_t Bytes = 0;
            EPreloadKind Kind = EPreloadKind::Other;
            bool Placeholder = false;
            bool CachedPlaceholder = false;
        };

        mutable std::mutex Mutex;
        std::condition_variable WorkerSignal;
        std::condition_variable MainSignal;
        std::deque<Job> Pending;
        std::deque<DetailJob> Details;
        std::deque<FinishedJob> FinishedDetails;
        std::unordered_set<std::wstring> MeshKeys; // 소문자. DeferDetail일 때만 채우고 작업 중에는 바꾸지 않는다.
        size_t DetailBytes = 0;
        uint32 RunningInitial = 0;
        bool DeferDetail = false;
        std::unordered_set<std::wstring> ScheduledKeys;
        std::deque<FinishedJob> Finished;
        size_t PendingBytes = 0;
        uint32 Running = 0;
        bool Cancelled = false;
        std::vector<std::thread> Workers;
        Stats Values;
        bool Active = false;

        [[nodiscard]] bool IsWorkDone() const { return Pending.empty() && Details.empty() && Running == 0; }
        [[nodiscard]] bool IsInitialWorkDone() const { return Pending.empty() && RunningInitial == 0; }
        [[nodiscard]] bool IsInitialDone() const { return IsInitialWorkDone() && Finished.empty(); }
        [[nodiscard]] bool IsAllDone() const { return IsWorkDone() && Finished.empty() && FinishedDetails.empty(); }
        [[nodiscard]] bool CanRunInitial() const { return !Pending.empty() && PendingBytes < MaxPendingPreloadBytes; }
        [[nodiscard]] bool CanRunDetail() const { return Pending.empty() && !Details.empty() && DetailBytes < MaxPendingDetailBytes; }

        void Run()
        {
            std::unique_lock lock(Mutex);
            while (true)
            {
                WorkerSignal.wait(lock, [this]
                    {
                        return Cancelled || IsWorkDone() || CanRunInitial() || CanRunDetail();
                    });
                if (Cancelled || IsWorkDone())
                {
                    WorkerSignal.notify_all();
                    MainSignal.notify_all();
                    return;
                }

                if (CanRunDetail())
                {
                    RunDetail(lock);
                    continue;
                }

                const Job request = std::move(Pending.front());
                Pending.pop_front();
                ++Running;
                ++RunningInitial;
                lock.unlock();

                FinishedJob job;
                std::vector<DiscoveredTexture> discovered;
                std::optional<DetailJob> detail;
                const auto begin = std::chrono::steady_clock::now();
                try
                {
                    Process(request, job, discovered, detail);
                }
                catch (const std::exception& error)
                {
                    ENGINE_LOG_WARN("Resource preload failed: key=\"{}\", error={}", WStrToStr(request.Reference.AssetKey), error.what());
                    detail.reset();
                }
                const f64 elapsed = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - begin).count();

                lock.lock();
                --Running;
                --RunningInitial;
                Values.WorkerMilliseconds += elapsed;
                Values.SlowestWorkerMilliseconds = std::max(Values.SlowestWorkerMilliseconds, elapsed);
                ++Values.FinishedCount;
                // 재질이 참조하는 텍스처는 오래 걸리므로 먼저 읽는다.
                Values.ScheduledCount += discovered.size();
                for (auto& texture : discovered) Pending.push_front({ std::move(texture.Reference), texture.Slot, true });
                if (job.Prepared)
                {
                    if (detail)
                    {
                        ++Values.DetailScheduledCount;
                        Details.push_back(std::move(*detail));
                    }
                    PendingBytes += job.Bytes;
                    Finished.push_back(std::move(job));
                }
                WorkerSignal.notify_all();
                MainSignal.notify_all();
            }
        }

        void RunDetail(std::unique_lock<std::mutex>& lock)
        {
            const DetailJob request = std::move(Details.front());
            Details.pop_front();
            ++Running;
            lock.unlock();

            FinishedJob job;
            job.Key = request.Key;
            job.Kind = request.Kind;
            const auto begin = std::chrono::steady_clock::now();
            try
            {
                job.Prepared = PreparePreloadFile(request.Kind, request.Key, request.Path);
                if (job.Prepared && request.StorePlaceholder)
                {
                    const auto& texture = static_cast<const Texture&>(*job.Prepared);
                    if (const auto placeholder = TexturePlaceholderCache::Extract(texture.GetCachedHeader(), texture.GetTempData()))
                        static_cast<void>(TexturePlaceholderCache::Store(request.Key, request.Path, *placeholder));
                }
            }
            catch (const std::exception& error)
            {
                ENGINE_LOG_WARN("Resource detail load failed: key=\"{}\", error={}", WStrToStr(request.Key), error.what());
            }
            if (job.Prepared) job.Bytes = job.Prepared->GetUploadByteSize();
            const f64 elapsed = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - begin).count();

            lock.lock();
            --Running;
            ++Values.DetailFinishedCount;
            Values.WorkerMilliseconds += elapsed;
            Values.SlowestWorkerMilliseconds = std::max(Values.SlowestWorkerMilliseconds, elapsed);
            DetailBytes += job.Bytes;
            // 읽지 못해도 메인 스레드가 자리를 '읽지 못함'으로 바꾸도록 알린다.
            FinishedDetails.push_back(std::move(job));
            WorkerSignal.notify_all();
            MainSignal.notify_all();
        }

        // 이 메시를 읽는 동안 대신 그릴, 더 단순한 LOD가 함께 있는지
        [[nodiscard]] bool HasCoarserLOD(const std::wstring& key) const
        {
            const auto name = ParseMeshLODName(key);
            if (!name) return false;
            for (int32 lod = name->LOD + 1; lod <= 2; ++lod)
                if (MeshKeys.contains(name->Stem + L"_lod" + std::to_wstring(lod) + name->Extension)) return true;
            return false;
        }

        void PreparePlaceholder(const Job& request, const std::filesystem::path& path, FinishedJob& job, std::optional<DetailJob>& detail)
        {
            DetailJob next{ job.Key, path, job.Kind, false };
            if (job.Kind == EPreloadKind::Mesh)
            {
                job.Prepared = Mesh::CreateEmpty();
                job.Prepared->SetKey(job.Key);
            }
            else
            {
                auto placeholder = TexturePlaceholderCache::Load(job.Key, path);
                job.CachedPlaceholder = placeholder.has_value();
                next.StorePlaceholder = !placeholder;
                if (!placeholder) placeholder = TexturePlaceholderCache::MakeSolid(request.MaterialSlot);
                job.Prepared = Texture::Create(placeholder->Header, placeholder->Pixels);
                job.Prepared->SetKey(job.Key);
            }
            job.Prepared->SetPath(path.wstring());
            job.Prepared->MarkPending();
            job.Placeholder = true;
            detail = std::move(next);
        }

        void Process(const Job& request, FinishedJob& job, std::vector<DiscoveredTexture>& discovered, std::optional<DetailJob>& detail)
        {
            const auto path = ResolvePreloadFile(request.Reference);
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

            if (job.Kind == EPreloadKind::Material)
                discovered = ScanMaterialTextures(path);
            else if (DeferDetail && (job.Kind == EPreloadKind::Mesh ? HasCoarserLOD(job.Key) : request.FromMaterial && IsImageTexture(path)))
                PreparePlaceholder(request, path, job, detail);
            else
                job.Prepared = PreparePreloadFile(job.Kind, job.Key, path);
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
        for (const auto* jobs : { &m_State->Finished, &m_State->FinishedDetails })
            for (const auto& job : *jobs) DiscardPreparedResource(job.Prepared);
    }

    void ResourcePreloader::Start(std::span<const ResourceReference> references)
    {
        Start(references, Options{});
    }

    void ResourcePreloader::Start(std::span<const ResourceReference> references, const Options& options)
    {
        auto& state = *m_State;
        if (state.Active || !state.Workers.empty() || references.empty() || !ResourceManager::IsCreated()) return;

        state.DeferDetail = options.DeferDetail;
        if (state.DeferDetail)
        {
            // 참조 이름만 비교한다. 파일 키와 표기가 다르면 미루지 않고 처음에 모두 읽는다.
            for (const auto& reference : references)
            {
                if (ClassifyPreloadFile(reference.AssetKey) == EPreloadKind::Mesh)
                    state.MeshKeys.insert(ToLowerWStr(NormalizePath(reference.AssetKey)));
            }
        }

        // 재질을 먼저 훑어 텍스처를 일찍 읽기 시작한다.
        for (const auto& reference : references)
        {
            const std::filesystem::path path(reference.AssetPath.empty() ? reference.AssetKey : reference.AssetPath);
            if (ClassifyPreloadFile(path) == EPreloadKind::Material) state.Pending.push_front({ reference });
            else state.Pending.push_back({ reference });
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

    bool ResourcePreloader::Update(std::chrono::steady_clock::time_point deadline, const LoadedCallback& onLoaded, size_t maxUploadBytes)
    {
        auto& state = *m_State;
        if (!ResourceManager::IsCreated())
        {
            state.Stop();
            return true;
        }
        auto& manager = ResourceManager::Get();

        const bool wasInitialReady = IsInitialReady();
        size_t uploadedBytes = 0;
        while (true)
        {
            State::FinishedJob job;
            bool isDetail = false;
            {
                std::unique_lock lock(state.Mutex);
                if (!state.IsInitialDone())
                {
                    state.MainSignal.wait_until(lock, deadline, [&state] { return !state.Finished.empty() || state.IsInitialWorkDone(); });
                    if (state.Finished.empty() && !state.IsInitialWorkDone()) return false;
                }
                // 처음 준비가 끝난 호출에서는 미룬 내용을 처리하지 않고 돌아가 화면을 먼저 열게 한다.
                isDetail = state.Finished.empty();
                auto& finished = isDetail ? state.FinishedDetails : state.Finished;
                if (isDetail && (finished.empty() || !wasInitialReady))
                {
                    if (!state.IsAllDone()) return false;
                    break;
                }
                job = std::move(finished.front());
                finished.pop_front();
                (isDetail ? state.DetailBytes : state.PendingBytes) -= job.Bytes;
            }
            state.WorkerSignal.notify_all();

            const auto begin = std::chrono::steady_clock::now();
            bool failed = false;
            if (isDetail)
                failed = !IsSuccess(manager.FulfillPendingResource(job.Key, job.Prepared));
            else
                static_cast<void>(manager.RegisterPreparedResource(job.Prepared));
            const auto end = std::chrono::steady_clock::now();
            const f64 elapsed = std::chrono::duration<f64, std::milli>(end - begin).count();
            uploadedBytes += job.Bytes;
            {
                const std::lock_guard lock(state.Mutex);
                state.Values.MainMilliseconds += elapsed;
                (job.Kind == EPreloadKind::Mesh ? state.Values.MeshMainMilliseconds : state.Values.TextureMainMilliseconds) += elapsed;
                state.Values.UploadBytes += job.Bytes;
                if (failed) ++state.Values.DetailFailedCount;
                else if (job.Placeholder) ++state.Values.PlaceholderCount;
                else ++(job.Kind == EPreloadKind::Mesh ? state.Values.MeshCount : state.Values.TextureCount);
                if (job.CachedPlaceholder) ++state.Values.CachedPlaceholderCount;
            }
            if (onLoaded) onLoaded(job.Key, elapsed);
            if (end >= deadline || uploadedBytes >= maxUploadBytes) return false;
        }

        state.JoinWorkers();
        return true;
    }

    bool ResourcePreloader::IsInitialReady() const
    {
        const std::lock_guard lock(m_State->Mutex);
        return m_State->IsInitialDone();
    }

    EResult ResourcePreloader::LoadDetail(Resource& resource)
    {
        if (!ResourceManager::IsCreated() || !ResourceManager::Get().IsOwnerThread())
        {
            ENGINE_LOG_WARN("Pending resource was used outside the main thread: key=\"{}\"", WStrToStr(resource.GetKey()));
            return EResult::Fail;
        }

        Resource* prepared = nullptr;
        try
        {
            const auto path = PlatformPath::ResolveUri(resource.GetPath());
            prepared = PreparePreloadFile(ClassifyPreloadFile(path), resource.GetKey(), path);
        }
        catch (const std::exception& error)
        {
            ENGINE_LOG_WARN("Resource detail load failed: key=\"{}\", error={}", WStrToStr(resource.GetKey()), error.what());
        }
        const EResult result = resource.Fulfill(prepared);
        DiscardPreparedResource(prepared);
        return result;
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
