#pragma once

#include "Editor_Includes.h"

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

protected:
    std::vector<std::filesystem::path> CollectReadyFiles(std::chrono::milliseconds debounce);
    void ClearPendingFiles();

private:
    mutable std::mutex m_QueueMutex;
    std::unordered_map<std::filesystem::path, std::chrono::steady_clock::time_point> m_PendingFiles;
    bool m_AutoReload = true;
};

END