#include "HotReloaderInterface.h"

void IHotReloader::Free()
{
    ClearPendingFiles();
    __super::Free();
}

void IHotReloader::NotifyFileModified(const std::filesystem::path& filePath)
{
    if (filePath.empty())
        return;

    std::lock_guard<std::mutex> lock(m_QueueMutex);

    if (!m_AutoReload)
        return;

    m_PendingFiles[filePath.string()] = std::chrono::steady_clock::now();
}

bool IHotReloader::IsAutoReloadEnabled() const
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    return m_AutoReload;
}

void IHotReloader::SetAutoReload(bool enable)
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_AutoReload = enable;

    if (!enable)
        m_PendingFiles.clear();
}

std::vector<std::filesystem::path> IHotReloader::CollectReadyFiles(std::chrono::milliseconds debounce)
{
    std::vector<std::filesystem::path> readyFiles;
    std::lock_guard<std::mutex> lock(m_QueueMutex);

    if (!m_AutoReload)
        return readyFiles;

    const auto now = std::chrono::steady_clock::now();

    for (auto it = m_PendingFiles.begin(); it != m_PendingFiles.end(); )
    {
        if (now - it->second >= debounce)
        {
            readyFiles.emplace_back(it->first);
            it = m_PendingFiles.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return readyFiles;
}

void IHotReloader::ClearPendingFiles()
{
    std::lock_guard<std::mutex> lock(m_QueueMutex);
    m_PendingFiles.clear();
}