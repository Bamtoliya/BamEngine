#pragma once

#include "Editor_Includes.h"
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

BEGIN(Editor)

class AssetWorkerHost : public Base
{
    DECLARE_SINGLETON(AssetWorkerHost)

public:
    struct CompletedThumbnailTask
    {
        std::string AssetKey;
        Engine::uint64 RequestId = 0;
        std::filesystem::path OutputPngPath;
        int32 Width = 0;
        int32 Height = 0;
        std::vector<uint8_t> RgbaData;
        bool Success = false;
    };

private:
    struct PendingThumbnailTask
    {
        std::string AssetKey;
        Engine::uint64 RequestId = 0;
        std::filesystem::path SourcePath;
        std::filesystem::path OutputPngPath;
        int32 Resolution = 128;
    };

private:
    AssetWorkerHost() = default;
    virtual ~AssetWorkerHost() = default;

public:
    EResult Initialize(void* arg = nullptr);
    void Free() override;

    [[nodiscard]] bool IsAvailable() const { return m_Available; }

    bool EnqueueThumbnail(const std::string& assetKey, Engine::uint64 requestId,
        const std::filesystem::path& sourcePath, const std::filesystem::path& outputPngPath, int32 resolution = 128);

    [[nodiscard]] std::vector<CompletedThumbnailTask> PollCompletedTasks();
    void ClearPendingTasks();

private:
    void WorkerThreadLoop(size_t workerIndex);

private:
    static constexpr size_t MAX_WORKERS = 2;
    static constexpr size_t MAX_QUEUE_SIZE = 2048;

    std::filesystem::path m_WorkerExePath;
    std::filesystem::path m_ProjectRoot;
    void* m_JobObject = nullptr;
    bool m_Available = false;

    std::mutex m_QueueMutex;
    std::condition_variable m_QueueCv;
    std::deque<PendingThumbnailTask> m_PendingQueue;
    bool m_StopRequested = false;

    std::mutex m_CompletedMutex;
    std::vector<CompletedThumbnailTask> m_CompletedTasks;

    std::mutex m_ActiveProcessMutex;
    std::vector<void*> m_ActiveProcesses;
    std::vector<std::thread> m_Workers;
};

END
