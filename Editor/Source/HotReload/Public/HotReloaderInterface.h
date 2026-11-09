#pragma once

#include "Editor_Includes.h"
#include <vector>
#include <string>

BEGIN(Editor)

class IHotReloader : public Base
{
protected:
    IHotReloader() = default;
    virtual ~IHotReloader() = default;

public:
    void Free() override;

    virtual void Update() BAM_PURE;
    virtual void NotifyFileModified(const std::filesystem::path& filePath) final;

    [[nodiscard]] bool IsAutoReloadEnabled() const;
    void SetAutoReload(bool enable);

    // =========================================================================
    // Watch Candidate Paths Management (가상 URI, 환경변수, 세미콜론 분리 목록 지원)
    // =========================================================================
    bool AddWatchPath(const std::filesystem::path& pathOrUri);
    void AddWatchPaths(const std::vector<std::filesystem::path>& pathsOrUris);
    void AddWatchPathsFromString(const std::string& pathList, char delimiter = ';');
    void AddWatchPathsFromEnvironment(const std::string& envVarName, char delimiter = ';');
    bool RemoveWatchPath(const std::filesystem::path& pathOrUri);
    void ClearWatchPaths();

    [[nodiscard]] const std::vector<std::filesystem::path>& GetWatchPaths() const { return m_WatchPaths; }
    [[nodiscard]] bool HasWatchPath(const std::filesystem::path& pathOrUri) const;

protected:
    virtual void OnWatchPathAdded(const std::filesystem::path& resolvedPath) {}
    virtual void OnWatchPathRemoved(const std::filesystem::path& resolvedPath) {}

    std::vector<std::filesystem::path> CollectReadyFiles(std::chrono::milliseconds debounce);
    void ClearPendingFiles();

private:
    mutable std::mutex m_QueueMutex;
    std::unordered_map<std::filesystem::path, std::chrono::steady_clock::time_point> m_PendingFiles;
    bool m_AutoReload = true;

    std::vector<std::filesystem::path> m_WatchPaths;
};

END
