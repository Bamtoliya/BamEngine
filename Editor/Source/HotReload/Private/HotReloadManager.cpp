#include "HotReloadManager.h"
#include <chrono>
#include <thread>

// efsw 파일 이벤트 리스너
class HotReloadFileListener final : public efsw::FileWatchListener
{
public:
    explicit HotReloadFileListener(HotReloadManager* manager, std::string targetFilename)
        : m_Manager(manager), m_TargetFilename(std::move(targetFilename)) {
    }

    void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
        efsw::Action action, std::string oldFilename) override
    {
        if (filename == m_TargetFilename && (action == efsw::Action::Modified || action == efsw::Action::Add))
        {
            if (m_Manager) m_Manager->NotifyFileModified();
        }
    }

private:
    HotReloadManager* m_Manager = nullptr;
    std::string m_TargetFilename;
};

IMPLEMENT_SINGLETON(HotReloadManager)

EResult HotReloadManager::Initialize(void* arg)
{
    std::error_code ec;
    std::filesystem::create_directories(m_ShadowDirectory, ec);
    CleanupShadowCopies();
    return EResult::Success;
}

void HotReloadManager::Free()
{
    StopWatching();
    UnloadModule();
    CleanupShadowCopies();
}

void HotReloadManager::NotifyFileModified()
{
    m_LastModifiedTime = std::chrono::steady_clock::now();
    m_PendingAutoReload = true;
}

void HotReloadManager::Update()
{
    if (!m_AutoReload || !m_PendingAutoReload || m_State == EHotReloadState::Reloading)
        return;

    // 링커가 파일 작성을 완전히 마칠 때까지 마지막 수정 이벤트 후 250ms 대기 (디바운스)
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_LastModifiedTime).count();
    if (elapsed >= 250)
    {
        m_PendingAutoReload = false;
        ENGINE_LOG_INFO("[HotReload] Auto-reload triggered by file change!");
        ReloadModule();
    }
}

void HotReloadManager::StartWatching(const std::filesystem::path& dllPath)
{
    StopWatching();

    m_WatchedFileName = dllPath.filename().string();
    const std::filesystem::path watchDir = dllPath.parent_path();

    m_FileWatcher = new efsw::FileWatcher();
    m_FileListener = new HotReloadFileListener(this, m_WatchedFileName);
    m_WatchID = m_FileWatcher->addWatch(watchDir.string(), m_FileListener, false);
    m_FileWatcher->watch();

    ENGINE_LOG_INFO("[HotReload] Watching for changes: {}", dllPath.string());
}

void HotReloadManager::StopWatching()
{
    if (m_FileWatcher)
    {
        if (m_WatchID > 0) m_FileWatcher->removeWatch(m_WatchID);
        delete m_FileWatcher;
        m_FileWatcher = nullptr;
    }
    if (m_FileListener)
    {
        delete m_FileListener;
        m_FileListener = nullptr;
    }
    m_WatchID = 0;
}

std::filesystem::path HotReloadManager::CreateShadowCopy(const std::filesystem::path& srcDllPath)
{
    std::error_code ec;
    if (!std::filesystem::exists(srcDllPath, ec))
    {
        ENGINE_LOG_ERROR("[HotReload] Original module not found: {}", srcDllPath.string());
        return {};
    }

    // 링커가 파일 쓰기를 완전히 마칠 때까지 최대 500ms(50ms x 10회) 대기
    bool fileReady = false;
    for (int retry = 0; retry < 10; ++retry)
    {
        HANDLE hFile = CreateFileW(srcDllPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hFile);
            fileReady = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (!fileReady)
    {
        ENGINE_LOG_ERROR("[HotReload] DLL file is locked by compiler/linker: {}", srcDllPath.string());
        return {};
    }

    m_ReloadIndex++;
    const std::wstring stem = srcDllPath.stem().wstring();
    const std::filesystem::path destDll = m_ShadowDirectory / (stem + L"_" + std::to_wstring(m_ReloadIndex) + L".dll");

    std::filesystem::copy_file(srcDllPath, destDll, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec)
    {
        ENGINE_LOG_ERROR("[HotReload] Failed to copy shadow DLL: {}", ec.message());
        return {};
    }

    // PDB 파일도 있으면 복사하여 디버깅 심볼 유지
    const std::filesystem::path srcPdb = srcDllPath.parent_path() / (stem + L".pdb");
    if (std::filesystem::exists(srcPdb, ec))
    {
        const std::filesystem::path destPdb = m_ShadowDirectory / (stem + L"_" + std::to_wstring(m_ReloadIndex) + L".pdb");
        std::filesystem::copy_file(srcPdb, destPdb, std::filesystem::copy_options::overwrite_existing, ec);
    }

    return destDll;
}

EResult HotReloadManager::LoadModule(const std::filesystem::path& dllPath)
{
    if (m_CurrentModule != nullptr)
        return EResult::AlreadyInitialized;

    m_OriginalDllPath = dllPath;
    m_CurrentShadowDllPath = CreateShadowCopy(m_OriginalDllPath);
    if (m_CurrentShadowDllPath.empty())
        return EResult::FileNotFound;

    m_ModuleHandle = LoadLibraryW(m_CurrentShadowDllPath.c_str());
    if (!m_ModuleHandle)
    {
        const DWORD err = GetLastError();
        ENGINE_LOG_ERROR("[HotReload] LoadLibraryW failed (code={}): {}", err, m_CurrentShadowDllPath.string());
        return EResult::Fail;
    }

    auto createFunc = reinterpret_cast<CreateModuleFunc>(GetProcAddress(m_ModuleHandle, "CreateModule"));
    m_DestroyFunc = reinterpret_cast<DestroyModuleFunc>(GetProcAddress(m_ModuleHandle, "DestroyModule"));

    if (!createFunc || !m_DestroyFunc)
    {
        ENGINE_LOG_ERROR("[HotReload] Failed to find module entry functions in: {}", m_CurrentShadowDllPath.string());
        FreeLibrary(m_ModuleHandle);
        m_ModuleHandle = nullptr;
        return EResult::Fail;
    }

    m_CurrentModule = createFunc();
    if (!m_CurrentModule)
    {
        ENGINE_LOG_ERROR("[HotReload] CreateModule returned nullptr.");
        FreeLibrary(m_ModuleHandle);
        m_ModuleHandle = nullptr;
        return EResult::Fail;
    }

    m_CurrentModule->OnLoad();
    ENGINE_LOG_INFO("[HotReload] Successfully loaded game module: {}", m_CurrentShadowDllPath.string());

    // 성공적으로 로드되면 해당 DLL 디렉터리 감시 시작
    StartWatching(m_OriginalDllPath);

    return EResult::Success;
}

EResult HotReloadManager::UnloadModule()
{
    if (!m_CurrentModule && !m_ModuleHandle)
        return EResult::Success;

    if (m_CurrentModule)
    {
        m_CurrentModule->OnUnload();
        if (m_DestroyFunc)
            m_DestroyFunc(m_CurrentModule);
        m_CurrentModule = nullptr;
        m_DestroyFunc = nullptr;
    }

    if (m_ModuleHandle)
    {
        FreeLibrary(m_ModuleHandle);
        m_ModuleHandle = nullptr;
    }

    std::error_code ec;
    if (!m_CurrentShadowDllPath.empty())
    {
        std::filesystem::remove(m_CurrentShadowDllPath, ec);
        m_CurrentShadowDllPath.clear();
    }

    return EResult::Success;
}

EResult HotReloadManager::ReloadModule()
{
    if (m_OriginalDllPath.empty())
        return EResult::AlreadyInitialized;

    m_State = EHotReloadState::Reloading;
    ENGINE_LOG_INFO("[HotReload] Reloading module: {}", m_OriginalDllPath.string());

    if (m_CurrentModule)
        m_CurrentModule->OnPreHotReload();

    UnloadModule();

    const EResult result = LoadModule(m_OriginalDllPath);
    if (result != EResult::Success)
    {
        m_State = EHotReloadState::Failed;
        ENGINE_LOG_ERROR("[HotReload] Reload failed: {}", ResultToString(result));
        return result;
    }

    if (m_CurrentModule)
        m_CurrentModule->OnPostHotReload();

    m_State = EHotReloadState::Idle;
    ENGINE_LOG_INFO("[HotReload] Module reloaded successfully (Version: {})", m_ReloadIndex);
    return EResult::Success;
}

void HotReloadManager::CleanupShadowCopies()
{
    std::error_code ec;
    if (!std::filesystem::exists(m_ShadowDirectory, ec)) return;

    for (const auto& entry : std::filesystem::directory_iterator(m_ShadowDirectory, ec))
    {
        if (entry.is_regular_file())
            std::filesystem::remove(entry.path(), ec);
    }
}