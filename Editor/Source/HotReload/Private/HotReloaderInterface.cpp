#include "HotReloaderInterface.h"
#include "PlatformPath.h"
#include <cstdlib>
#include <sstream>

void IHotReloader::Free()
{
    ClearWatchPaths();
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

    m_PendingFiles[filePath] = std::chrono::steady_clock::now();
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

bool IHotReloader::AddWatchPath(const std::filesystem::path& pathOrUri)
{
    if (pathOrUri.empty())
        return false;

    // 가상 URI(Engine://, Project:// 등) 또는 상대경로를 절대경로로 해석
    const auto resolved = Engine::PlatformPath::ResolveUri(pathOrUri);
    std::error_code ec;
    if (!std::filesystem::is_directory(resolved, ec))
        return false;

    const auto normPath = Engine::PlatformPath::Normalize(resolved);

    // 중복 등록 방지
    for (const auto& existing : m_WatchPaths)
    {
        if (Engine::PlatformPath::Normalize(existing) == normPath)
            return false;
    }

    m_WatchPaths.push_back(normPath);
    OnWatchPathAdded(normPath);
    return true;
}

void IHotReloader::AddWatchPaths(const std::vector<std::filesystem::path>& pathsOrUris)
{
    for (const auto& path : pathsOrUris)
    {
        AddWatchPath(path);
    }
}

void IHotReloader::AddWatchPathsFromString(const std::string& pathList, char delimiter)
{
    if (pathList.empty())
        return;

    std::stringstream ss(pathList);
    std::string item;
    while (std::getline(ss, item, delimiter))
    {
        // 공백 트림
        size_t first = item.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        size_t last = item.find_last_not_of(" \t\r\n");
        std::string trimmed = item.substr(first, (last - first + 1));
        if (!trimmed.empty())
        {
            AddWatchPath(trimmed);
        }
    }
}

void IHotReloader::AddWatchPathsFromEnvironment(const std::string& envVarName, char delimiter)
{
    if (envVarName.empty())
        return;

#if defined(_WIN32)
    char* val = nullptr;
    size_t len = 0;
    if (_dupenv_s(&val, &len, envVarName.c_str()) == 0 && val != nullptr)
    {
        AddWatchPathsFromString(std::string(val), delimiter);
        free(val);
    }
#else
    const char* val = std::getenv(envVarName.c_str());
    if (val)
    {
        AddWatchPathsFromString(std::string(val), delimiter);
    }
#endif
}

bool IHotReloader::RemoveWatchPath(const std::filesystem::path& pathOrUri)
{
    const auto resolved = Engine::PlatformPath::ResolveUri(pathOrUri);
    const auto normPath = Engine::PlatformPath::Normalize(resolved);

    for (size_t i = 0; i < m_WatchPaths.size(); ++i)
    {
        if (Engine::PlatformPath::Normalize(m_WatchPaths[i]) == normPath)
        {
            const auto target = m_WatchPaths[i];
            m_WatchPaths.erase(m_WatchPaths.begin() + i);
            OnWatchPathRemoved(target);
            return true;
        }
    }
    return false;
}

void IHotReloader::ClearWatchPaths()
{
    const auto paths = m_WatchPaths;
    m_WatchPaths.clear();
    for (const auto& path : paths)
    {
        OnWatchPathRemoved(path);
    }
}

bool IHotReloader::HasWatchPath(const std::filesystem::path& pathOrUri) const
{
    const auto resolved = Engine::PlatformPath::ResolveUri(pathOrUri);
    const auto normPath = Engine::PlatformPath::Normalize(resolved);

    for (const auto& existing : m_WatchPaths)
    {
        if (Engine::PlatformPath::Normalize(existing) == normPath)
            return true;
    }
    return false;
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
