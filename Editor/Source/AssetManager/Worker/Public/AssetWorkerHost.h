#pragma once

#include "Editor_Includes.h"
#include "ModelConverter.h"
#include <condition_variable>
#include <functional>
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

    struct ActiveImportStatus
    {
        Engine::uint64 RequestId = 0;
        std::string AssetName;
        float Progress = 0.0f;
        const char* StepKey = "ModelImport.WorkerStarting";
    };

    enum class ModelImportResult
    {
        SnapshotReady,
        Unavailable,
        Failed,
        Cancelled
    };

    using ModelImportCompletion = std::function<void(ModelImportResult, const std::filesystem::path&)>;

private:
    struct PendingThumbnailTask
    {
        std::string AssetKey;
        Engine::uint64 RequestId = 0;
        std::filesystem::path SourcePath;
        std::filesystem::path OutputPngPath;
        int32 Resolution = 128;
    };

    struct PendingImportTask
    {
        Engine::uint64 RequestId = 0;
        std::filesystem::path SourcePath;
        Tools::ModelConversion::Options Options;
        ModelImportCompletion Complete;
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

    // The snapshot is available during the worker-thread completion callback.
    bool EnqueueImportModel(const std::filesystem::path& sourcePath, Tools::ModelConversion::Options options,
        ModelImportCompletion complete);
    bool GetActiveImportStatus(ActiveImportStatus& outStatus);
    void CancelActiveImport(Engine::uint64 requestId);

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
    std::deque<PendingImportTask> m_PendingImports;
    Engine::uint64 m_NextImportRequestId = 1;
    bool m_ImportInProgress = false;
    bool m_StopRequested = false;

    std::mutex m_CompletedMutex;
    std::vector<CompletedThumbnailTask> m_CompletedTasks;

    std::mutex m_ImportStatusMutex;
    ActiveImportStatus m_ActiveImportStatus;
    void* m_ActiveImportProcess = nullptr;
    bool m_ImportCancelled = false;

    std::mutex m_ActiveProcessMutex;
    std::vector<void*> m_ActiveProcesses;
    std::vector<std::thread> m_Workers;
};

END
