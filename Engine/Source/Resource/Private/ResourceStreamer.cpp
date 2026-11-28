#include "ResourceStreamer.h"
#include "MaterialInstance.h"
#include "Mesh.h"
#include "ResourceManager.h"
#include "ResourcePreloader.h"
#include "StreamingPolicy.h"
#include "Texture.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>
#include <thread>

IMPLEMENT_SINGLETON(ResourceStreamer)

namespace Engine
{
    namespace ResourceStreamerDetail
    {
        constexpr uint32 MaxStreamWorkers = 4;
        constexpr size_t MaxStreamJobs = 8;
        // 잠깐 그리지 않은 프레임(뷰포트 갱신 제한 등) 때문에 내리지 않도록 마지막 요청을 유지한다.
        constexpr f64 StreamRequestHoldSeconds = 2.0;

        // 재질과, 덮어쓰지 않은 기본 재질의 텍스처를 한 번씩 방문한다.
        template<typename Visitor>
        void ForEachMaterialTexture(const MaterialInterface& material, Visitor&& visit)
        {
            for (const auto& binding : material.GetTextureBindings())
                if (auto* texture = binding.texture.Get()) visit(*texture);

            const auto* instance = dynamic_cast<const MaterialInstance*>(&material);
            const auto* base = instance ? instance->GetBaseMaterial() : nullptr;
            if (!base) return;
            for (const auto& binding : base->GetTextureBindings())
            {
                if (material.HasTextureBindingBySlot(binding.slot)) continue;
                if (auto* texture = binding.texture.Get()) visit(*texture);
            }
        }
    }

    using namespace ResourceStreamerDetail;

    struct ResourceStreamer::State
    {
        using Clock = std::chrono::steady_clock;

        struct Entry
        {
            Handle ResourceHandle;
            Resource* Target = nullptr;     // 핸들이 유효한 동안에만 쓴다
            std::wstring Key;
            std::filesystem::path Path;
            bool IsMesh = false;
            TextureMipLayout Layout;
            std::vector<uint64> LevelBytes;
            uint32 Level = 0;
            uint32 JobLevel = 0;
            bool Busy = false;
            uint32 Generation = 0;          // 번호를 다시 쓸 때 늦은 결과를 버린다
            bool Active = false;
            uint64 GeometryVersion = 0;     // 올린 뒤 밖에서 바꾼 메시를 구분한다

            uint64 RequestFrame = 0;
            uint64 BoundFrame = 0;
            Clock::time_point RequestTime{};
            bool EverRequested = false;
            uint32 Wanted = 0;
            f32 Priority = 0.0f;
        };

        struct Job
        {
            uint32 Slot = 0;
            uint32 Generation = 0;
            std::wstring Key;
            std::filesystem::path Path;
            bool IsMesh = false;
            uint32 Level = 0;
            TextureMipLayout Layout;
        };

        struct Result
        {
            uint32 Slot = 0;
            uint32 Generation = 0;
            uint32 Level = 0;
            Resource* Prepared = nullptr;
            std::optional<TextureMipData> Mips;
            uint64 Bytes = 0;
        };

        std::vector<Entry> Entries; // 번호 = 위치 + 1
        std::vector<uint32> FreeEntries;
        uint64 Frame = 1;
        uint64 BudgetBytes = static_cast<uint64>(DefaultBudgetMB) * 1024 * 1024;
        Stats Values;
        size_t InFlight = 0;

        uint64 BurstLoads = 0;
        uint64 BurstReductions = 0;
        Clock::time_point BurstStart{};

        mutable std::mutex Mutex;
        std::condition_variable Signal;
        std::deque<Job> Jobs;
        std::deque<Result> Results;
        std::vector<std::thread> Workers;
        bool Stopping = false;

        Entry* Find(const Resource& resource)
        {
            const uint32 slot = resource.GetStreamSlot();
            if (slot == 0 || slot > Entries.size()) return nullptr;
            auto& entry = Entries[slot - 1];
            return entry.Active && entry.Target == &resource ? &entry : nullptr;
        }

        Entry& Add(const Handle& handle, Resource& resource, const std::filesystem::path& path)
        {
            uint32 index = 0;
            if (FreeEntries.empty())
            {
                index = static_cast<uint32>(Entries.size());
                Entries.emplace_back();
            }
            else
            {
                index = FreeEntries.back();
                FreeEntries.pop_back();
            }
            auto& entry = Entries[index];
            const uint32 generation = entry.Generation + 1;
            entry = {};
            entry.Generation = generation;
            entry.Active = true;
            entry.ResourceHandle = handle;
            entry.Target = &resource;
            entry.Key = resource.GetKey();
            entry.Path = path;
            resource.SetStreamSlot(index + 1);
            return entry;
        }

        void Remove(uint32 index, bool resourceAlive)
        {
            auto& entry = Entries[index];
            if (resourceAlive) entry.Target->SetStreamSlot(0);
            entry.Active = false;
            entry.Target = nullptr;
            ++entry.Generation;
            FreeEntries.push_back(index);
        }

        void Request(Entry& entry, uint32 level, f32 priority)
        {
            if (entry.RequestFrame != Frame)
            {
                entry.RequestFrame = Frame;
                entry.Wanted = level;
                entry.Priority = priority;
                return;
            }
            entry.Wanted = (std::min)(entry.Wanted, level);
            entry.Priority = (std::max)(entry.Priority, priority);
        }

        // 사라졌거나, 다른 쪽이 내용을 직접 읽거나 바꾼 리소스는 더 관리하지 않는다.
        void Prune()
        {
            auto& manager = ResourceManager::Get();
            for (uint32 index = 0; index < Entries.size(); ++index)
            {
                auto& entry = Entries[index];
                if (!entry.Active) continue;
                if (manager.GetResource(entry.ResourceHandle) != entry.Target)
                {
                    Remove(index, false);
                    continue;
                }
                bool taken = false;
                if (entry.IsMesh && entry.Level == 0)
                    taken = !entry.Target->IsResident() || static_cast<const Mesh*>(entry.Target)->GetGeometryVersion() != entry.GeometryVersion;
                else
                    taken = !entry.Target->IsPending();
                if (taken) Remove(index, true);
            }
        }

        void Work()
        {
            std::unique_lock lock(Mutex);
            while (true)
            {
                Signal.wait(lock, [this] { return Stopping || !Jobs.empty(); });
                if (Stopping) return;
                const Job job = std::move(Jobs.front());
                Jobs.pop_front();
                lock.unlock();

                Result result{ job.Slot, job.Generation, job.Level };
                try
                {
                    if (job.IsMesh)
                    {
                        result.Prepared = ResourcePreloader::PrepareFile(job.Key, job.Path);
                        if (result.Prepared)
                        {
                            result.Bytes = result.Prepared->GetUploadByteSize();
                            // 메인 스레드에서 처음 쓸 때 계산하지 않게 한다.
                            static_cast<void>(static_cast<const Mesh*>(result.Prepared)->GetUVDensity());
                        }
                    }
                    else
                    {
                        result.Mips = TextureMipCache::Load(job.Key, job.Path, job.Level);
                        if (!result.Mips) result.Mips = DecodeTexture(job);
                        if (result.Mips && (result.Mips->Header.width != job.Layout.GetWidth(job.Level) || result.Mips->Header.height != job.Layout.GetHeight(job.Level)))
                            result.Mips.reset(); // 그 사이 파일이 바뀌었다
                        if (result.Mips) result.Bytes = result.Mips->Pixels.size();
                    }
                }
                catch (const std::exception& error)
                {
                    ENGINE_LOG_WARN("Resource streaming failed: key=\"{}\", error={}", Engine::Unicode::WideToUtf8(job.Key), error.what());
                }

                lock.lock();
                Results.push_back(std::move(result));
            }
        }

        static std::optional<TextureMipData> DecodeTexture(const Job& job)
        {
            auto* texture = static_cast<Texture*>(ResourcePreloader::PrepareFile(job.Key, job.Path));
            if (!texture) return std::nullopt;
            static_cast<void>(TextureMipCache::Store(job.Key, job.Path, texture->GetCachedHeader(), texture->GetTempData()));
            auto mips = TextureMipCache::Slice(texture->GetCachedHeader(), texture->GetTempData(), job.Level);
            texture->Free();
            delete texture;
            return mips;
        }

        // 끝난 작업 하나를 올린다. 올린 바이트 수를 돌려준다.
        uint64 Apply(Result& result)
        {
            --InFlight;
            uint64 uploaded = 0;
            if (result.Slot < Entries.size() && Entries[result.Slot].Active && Entries[result.Slot].Generation == result.Generation)
            {
                auto& entry = Entries[result.Slot];
                entry.Busy = false;
                EResult applied = EResult::Fail;
                if (entry.IsMesh && entry.Target->IsPending())
                {
                    applied = entry.Target->Fulfill(result.Prepared);
                    entry.GeometryVersion = static_cast<const Mesh*>(entry.Target)->GetGeometryVersion();
                    if (IsSuccess(applied)) entry.LevelBytes[0] = result.Bytes;
                }
                else if (!entry.IsMesh && result.Mips)
                    applied = static_cast<Texture*>(entry.Target)->ApplyStreamedMips(*result.Mips);

                if (IsSuccess(applied))
                {
                    const bool loaded = result.Level < entry.Level;
                    entry.Level = result.Level;
                    // 한 번 원본을 읽었으면 단색으로 돌아가지 않는다.
                    if (!entry.IsMesh) entry.LevelBytes.resize(entry.Layout.FindMipForSize(TextureMipCache::PlaceholderSize) + 1);
                    ++(loaded ? Values.LoadCount : Values.ReduceCount);
                    ++(loaded ? BurstLoads : BurstReductions);
                    uploaded = result.Bytes;
                }
                else
                {
                    // 지금 보이는 상태를 유지하고 다시 시도하지 않는다.
                    ++Values.FailedCount;
                    ENGINE_LOG_ERROR("Resource streaming could not load: key=\"{}\", path=\"{}\"", Engine::Unicode::WideToUtf8(entry.Key), Engine::Unicode::WideToUtf8(entry.Path.wstring()));
                    Remove(result.Slot, true);
                }
            }
            if (result.Prepared)
            {
                result.Prepared->Free();
                delete result.Prepared;
            }
            return uploaded;
        }

        void Start(uint32 index, uint32 level)
        {
            auto& entry = Entries[index];
            entry.Busy = true;
            entry.JobLevel = level;
            ++InFlight;
            const std::lock_guard lock(Mutex);
            Jobs.push_back({ index, entry.Generation, entry.Key, entry.Path, entry.IsMesh, level, entry.Layout });
        }
    };

    ResourceStreamer::ResourceStreamer()
        : m_State(std::make_unique<State>())
    {
    }

    ResourceStreamer::~ResourceStreamer() = default;

    EResult ResourceStreamer::Initialize(void*)
    {
        auto& state = *m_State;
        const uint32 hardware = std::max(1u, std::thread::hardware_concurrency());
        const uint32 workerCount = std::clamp(hardware / 2, 1u, MaxStreamWorkers);
        try
        {
            for (uint32 i = 0; i < workerCount; ++i)
                state.Workers.emplace_back([&state] { state.Work(); });
        }
        catch (const std::system_error& error)
        {
            ENGINE_LOG_WARN("Resource streaming worker could not start: {}", error.what());
        }
        if (state.Workers.empty()) return EResult::Fail;
        return EResult::Success;
    }

    void ResourceStreamer::Free()
    {
        auto& state = *m_State;
        {
            const std::lock_guard lock(state.Mutex);
            state.Stopping = true;
        }
        state.Signal.notify_all();
        for (auto& worker : state.Workers)
            if (worker.joinable()) worker.join();
        state.Workers.clear();
        for (auto& result : state.Results)
        {
            if (!result.Prepared) continue;
            result.Prepared->Free();
            delete result.Prepared;
        }
        state.Results.clear();
        state.Jobs.clear();
        // 리소스가 다음 스트리머의 번호로 오해되지 않게 한다.
        if (ResourceManager::IsCreated())
        {
            for (auto& entry : state.Entries)
                if (entry.Active && ResourceManager::Get().GetResource(entry.ResourceHandle) == entry.Target) entry.Target->SetStreamSlot(0);
        }
        state.Entries.clear();
        state.FreeEntries.clear();
    }

    void ResourceStreamer::SetBudgetMB(uint32 megabytes)
    {
        m_State->BudgetBytes = static_cast<uint64>(std::clamp(megabytes, MinBudgetMB, MaxBudgetMB)) * 1024 * 1024;
    }

    ResourceStreamer::Stats ResourceStreamer::GetStats() const
    {
        const auto& state = *m_State;
        Stats stats = state.Values;
        stats.BudgetBytes = state.BudgetBytes;
        stats.InFlightCount = static_cast<uint32>(state.InFlight);
        for (const auto& entry : state.Entries)
        {
            if (!entry.Active) continue;
            stats.UsedBytes += entry.LevelBytes[entry.Level];
            ++(entry.IsMesh ? stats.MeshCount : stats.TextureCount);
            if (entry.Level == 0) ++(entry.IsMesh ? stats.LoadedMeshCount : stats.FullTextureCount);
        }
        return stats;
    }

    bool ResourceStreamer::IsBusy() const
    {
        return m_State->InFlight > 0;
    }

    bool ResourceStreamer::HasEntries() const
    {
        return m_State->Entries.size() > m_State->FreeEntries.size();
    }

    void ResourceStreamer::AddMesh(const Handle& handle, const std::filesystem::path& path, uint64 estimatedBytes)
    {
        auto* resource = ResourceManager::Get().GetResource(handle);
        if (!resource || !resource->IsPending() || m_State->Find(*resource)) return;

        auto& entry = m_State->Add(handle, *resource, path);
        entry.IsMesh = true;
        entry.LevelBytes = { estimatedBytes, 0 };
        entry.Level = 1;
    }

    void ResourceStreamer::AddTexture(const Handle& handle, const std::filesystem::path& path, const TextureInfo& info)
    {
        auto* resource = ResourceManager::Get().GetResource(handle);
        if (!resource || !resource->IsPending() || !info.Layout.IsValid() || m_State->Find(*resource)) return;

        auto& entry = m_State->Add(handle, *resource, path);
        entry.Layout = info.Layout;
        // 단계 n은 밉 n부터 올린 상태다. 마지막 단계는 처음에 보여 주는 크기이며 그 아래로는 내리지 않는다.
        const uint32 floor = info.Layout.FindMipForSize(TextureMipCache::PlaceholderSize);
        for (uint32 level = 0; level <= floor; ++level) entry.LevelBytes.push_back(info.Layout.GetTailBytes(level));
        entry.Level = floor;
        if (info.FirstMip > floor)
        {
            // 단색은 가장 낮은 단계보다 거칠므로 한 단계 더 둔다.
            entry.LevelBytes.push_back(4);
            entry.Level = floor + 1;
        }
    }

    void ResourceStreamer::RequestMesh(const Mesh& mesh, f32 priority)
    {
        if (auto* entry = m_State->Find(mesh)) m_State->Request(*entry, 0, priority);
    }

    void ResourceStreamer::RequestMaterial(const MaterialInterface& material, f32 pixelsPerUV, f32 priority)
    {
        auto& state = *m_State;
        ForEachMaterialTexture(material, [&](const Texture& texture)
            {
                auto* entry = state.Find(texture);
                if (!entry) return;
                uint32 level = 0;
                if (pixelsPerUV > 0.0f && std::isfinite(pixelsPerUV))
                {
                    const f32 ratio = static_cast<f32>((std::max)(entry->Layout.Width, entry->Layout.Height)) / pixelsPerUV;
                    if (ratio > 1.0f) level = static_cast<uint32>(std::log2(ratio));
                }
                // 단색을 위한 추가 단계는 요청하지 않는다.
                const uint32 floor = entry->Layout.FindMipForSize(TextureMipCache::PlaceholderSize);
                state.Request(*entry, (std::min)(level, floor), priority);
            });
    }

    void ResourceStreamer::NoteBound(const Texture& texture)
    {
        if (texture.GetStreamSlot() == 0 || !IsCreated()) return;
        auto& state = *Get().m_State;
        if (auto* entry = state.Find(texture)) entry->BoundFrame = state.Frame;
    }

    void ResourceStreamer::EnsureMaterialTextures(const MaterialInterface& material)
    {
        ForEachMaterialTexture(material, [](Texture& texture) { static_cast<void>(texture.EnsureResident()); });
    }

    void ResourceStreamer::Update(f64 budgetMilliseconds, uint64 maxUploadBytes)
    {
        if (!ResourceManager::IsCreated()) return;
        auto& state = *m_State;
        const auto begin = State::Clock::now();
        state.Prune();

        // 1. 끝난 작업을 올린다.
        uint64 uploaded = 0;
        while (uploaded < maxUploadBytes && std::chrono::duration<f64, std::milli>(State::Clock::now() - begin).count() < budgetMilliseconds)
        {
            State::Result result;
            {
                const std::lock_guard lock(state.Mutex);
                if (state.Results.empty()) break;
                result = std::move(state.Results.front());
                state.Results.pop_front();
            }
            uploaded += state.Apply(result);
        }

        // 2. 지난 프레임의 요청으로 계획을 세운다.
        std::vector<StreamingItem> items;
        for (uint32 index = 0; index < state.Entries.size(); ++index)
        {
            auto& entry = state.Entries[index];
            if (!entry.Active) continue;
            if (entry.BoundFrame == state.Frame && entry.RequestFrame != state.Frame) state.Request(entry, 0, 1.0f);
            if (entry.RequestFrame == state.Frame)
            {
                entry.RequestTime = begin;
                entry.EverRequested = true;
            }

            StreamingItem item;
            item.Id = index;
            item.LevelBytes = entry.LevelBytes;
            item.Level = entry.Level;
            item.Wanted = entry.Wanted;
            item.Priority = entry.Priority;
            item.Needed = entry.EverRequested && std::chrono::duration<f64>(begin - entry.RequestTime).count() < StreamRequestHoldSeconds;
            item.Busy = entry.Busy;
            item.Target = entry.JobLevel;
            items.push_back(std::move(item));
        }
        ++state.Frame;

        const size_t freeJobs = state.InFlight < MaxStreamJobs ? MaxStreamJobs - state.InFlight : 0;
        const auto plan = PlanStreaming(items, state.BudgetBytes, freeJobs);
        state.Values.UnmetCount = plan.UnmetCount;

        bool started = false;
        for (const auto& action : plan.Reductions)
        {
            auto& entry = state.Entries[action.Id];
            if (!entry.IsMesh)
            {
                state.Start(action.Id, action.Level);
                started = true;
                continue;
            }
            static_cast<Mesh*>(entry.Target)->ReleaseContents();
            entry.Level = action.Level;
            ++state.Values.ReduceCount;
            ++state.BurstReductions;
        }
        for (const auto& action : plan.Loads)
        {
            state.Start(action.Id, action.Level);
            started = true;
        }
        if (started) state.Signal.notify_all();

        // 3. 몰아서 읽기를 마치면 한 번 기록한다.
        if (state.InFlight > 0 && state.BurstStart == State::Clock::time_point{}) state.BurstStart = begin;
        if (state.InFlight == 0 && state.BurstStart != State::Clock::time_point{})
        {
            if (state.BurstLoads + state.BurstReductions >= 8)
            {
                const auto stats = GetStats();
                constexpr f64 bytesPerMB = 1024.0 * 1024.0;
                BAM_LOG(Info, "Engine", "Streaming", "Resource streaming settled: {} load(s), {} reduction(s) in {:.0f} ms | {:.1f} / {:.0f} MB, meshes {}/{}, full textures {}/{}, unmet {}, failed {}",
                    state.BurstLoads, state.BurstReductions, std::chrono::duration<f64, std::milli>(begin - state.BurstStart).count(),
                    static_cast<f64>(stats.UsedBytes) / bytesPerMB, static_cast<f64>(stats.BudgetBytes) / bytesPerMB,
                    stats.LoadedMeshCount, stats.MeshCount, stats.FullTextureCount, stats.TextureCount, stats.UnmetCount, stats.FailedCount);
            }
            state.BurstStart = {};
            state.BurstLoads = 0;
            state.BurstReductions = 0;
        }
    }
}
