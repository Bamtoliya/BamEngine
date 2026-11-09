#include "ModuleBuildManager.h"
#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include <mutex>
#include <windows.h>

BEGIN(Editor)

class ModuleSourceFileListener final : public efsw::FileWatchListener
{
public:
    explicit ModuleSourceFileListener(ModuleBuildManager* manager) : m_Manager(manager) {}

    void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
        efsw::Action action, std::string oldFilename) override
    {
        const std::filesystem::path fullPath = std::filesystem::path(reinterpret_cast<const char8_t*>(dir.c_str())) / reinterpret_cast<const char8_t*>(filename.c_str());
        const auto ext = fullPath.extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".inl" || filename == "CMakeLists.txt")
        {
            EDITOR_LOG_INFO("[ModuleBuildManager] Source modified detected: {}", filename);
            if (m_Manager) m_Manager->NotifyFileModified(fullPath);
        }
    }

private:
    ModuleBuildManager* m_Manager = nullptr;
};

#pragma region Constructor&Destructor
EResult ModuleBuildManager::Initialize(const std::filesystem::path& projectRoot, const std::filesystem::path& buildDir,
    const std::filesystem::path& sourceDir, const std::wstring& targetName)
{
    m_ProjectRoot = projectRoot;
    m_BuildDir = buildDir;
    m_SourceDir = m_ProjectRoot / sourceDir;
    m_TargetName = targetName;

    if (std::filesystem::exists(m_SourceDir))
    {
        StartWatching(m_SourceDir);
    }
    else
    {
        EDITOR_LOG_WARN("[ModuleBuildManager] Source directory not found: {}", WStrToStr(m_SourceDir.wstring()));
    }

    return EResult::Success;
}

void ModuleBuildManager::Free()
{
    StopWatching();
    if (m_BuildThread.joinable()) m_BuildThread.join();
    __super::Free();
}

ModuleBuildManager* ModuleBuildManager::Create(const std::filesystem::path& projectRoot,
    const std::filesystem::path& buildDir, const std::filesystem::path& sourceDir, const std::wstring& targetName)
{
    ModuleBuildManager* instance = new ModuleBuildManager();
    if (instance->Initialize(projectRoot, buildDir, sourceDir, targetName) != EResult::Success)
    {
        instance->Free();
        delete instance;
        return nullptr;
    }
    return instance;
}
#pragma endregion

void ModuleBuildManager::StartWatching(const std::filesystem::path& sourceDir)
{
    StopWatching();
    m_FileWatcher = new efsw::FileWatcher();
    m_FileListener = new ModuleSourceFileListener(this);
    const auto sourceUtf8 = sourceDir.u8string();
    m_WatchID = m_FileWatcher->addWatch(std::string(sourceUtf8.begin(), sourceUtf8.end()), m_FileListener, true);
    if (m_WatchID > 0)
    {
        m_FileWatcher->watch();
        EDITOR_LOG_INFO("[ModuleBuildManager] Watching {}", WStrToStr(sourceDir.wstring()));
    }
    else
        EDITOR_LOG_WARN("[ModuleBuildManager] Watch failed: {}", WStrToStr(sourceDir.wstring()));
}

void ModuleBuildManager::StopWatching()
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

void ModuleBuildManager::Update()
{
    if (m_HasNewResult.exchange(false))
    {
        if (m_BuildThread.joinable()) m_BuildThread.join();

        m_LastBuildOutput = std::move(m_PendingBuildOutput);
        m_LastBuildSuccess = m_PendingBuildSuccess;

        if (m_LastBuildSuccess)
        {
            m_Status = EModuleBuildStatus::Succeeded;
            EDITOR_LOG_INFO("[ModuleBuildManager] {} build succeeded", WStrToStr(m_TargetName));
            NotificationManager::Get().AddNotification(
                LOCAL("ToolBar.ModuleCompile"), LOCAL("Notification.CompileSuccess"), ELogLevel::Info, 3.0f);

            // 빌드가 완전히 끝났으므로 안전하게 모듈 핫로드 실행
            if (m_TargetName == L"GameModule")
                HotReloadManager::Get().GetModuleReloader().ReloadModule();
        }
        else
        {
            m_Status = EModuleBuildStatus::Failed;
            EDITOR_LOG_ERROR("[ModuleBuildManager] {} build failed:\n{}", WStrToStr(m_TargetName), m_LastBuildOutput);
            NotificationManager::Get().AddNotification(
                LOCAL("ToolBar.ModuleCompile"), LOCAL("Notification.CompileFailed"), ELogLevel::Error, 5.0f);
        }

        if (m_OnBuildCompleted) m_OnBuildCompleted(m_LastBuildSuccess, m_LastBuildOutput);
        return;
    }

    // Leave saves queued while the current build is running.
    if (!IsCompiling() && !CollectReadyFiles(std::chrono::milliseconds(200)).empty())
        RequestBuild();
}

bool ModuleBuildManager::RequestBuild()
{
    if (m_Status == EModuleBuildStatus::Compiling) return false;

    if (m_BuildThread.joinable()) m_BuildThread.join();

    m_Status = EModuleBuildStatus::Compiling;
    EDITOR_LOG_INFO("[ModuleBuildManager] Starting background build for {}", WStrToStr(m_TargetName));
    NotificationManager::Get().AddNotification(
        LOCAL("ToolBar.ModuleCompile"), LOCAL("Notification.CompilingModule"), ELogLevel::Info, 2.0f);

    m_BuildThread = std::thread(&ModuleBuildManager::ExecuteBuildAsync, this);
    return true;
}

void ModuleBuildManager::ExecuteBuildAsync()
{
    // Both builders share a generated build tree.
    static std::mutex buildTreeMutex;
    const std::lock_guard buildLock(buildTreeMutex);

#ifdef _DEBUG
    constexpr auto config = L"Debug";
#else
    constexpr auto config = L"Release";
#endif
    if (m_TargetName == L"GameModule")
    {
        const auto binDir = m_BuildDir / "bin" / config;
        std::error_code ec;
        const auto pdbPath = binDir / "GameModule.pdb";
        if (std::filesystem::exists(pdbPath, ec))
        {
            const auto oldPdb = binDir / (L"GameModule_old_" + std::to_wstring(GetTickCount64()) + L".pdb");
            std::filesystem::rename(pdbPath, oldPdb, ec);
        }
    }
    std::wstring cmd = L"\"";
    cmd += BAM_EDITOR_CMAKE_COMMAND;
    cmd += L"\" --build \"" + m_BuildDir.wstring() + L"\" --target " + m_TargetName + L" --config " + config;

#if defined(BAM_EDITOR_MSBUILD)
    // The loaded host already has the dependencies for a plugin-only build.
    cmd += L" -- /p:BuildProjectReferences=false /m";
#endif

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        m_PendingBuildOutput = "Failed to create the cmake output pipe.";
        m_PendingBuildSuccess = false;
        m_HasNewResult = true;
        return;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si{};
    si.cb = sizeof(STARTUPINFOW);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> cmdBuffer(cmd.begin(), cmd.end());
    cmdBuffer.push_back(L'\0');

    std::string capturedOutput;
    const BOOL success = CreateProcessW(BAM_EDITOR_CMAKE_COMMAND, cmdBuffer.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, m_ProjectRoot.c_str(), &si, &pi);

    CloseHandle(hWritePipe);

    if (success)
    {
        char buffer[512];
        DWORD bytesRead = 0;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            capturedOutput += buffer;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 1;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        m_PendingBuildSuccess = (exitCode == 0);
    }
    else
    {
        capturedOutput = "Failed to launch cmake process.";
        m_PendingBuildSuccess = false;
    }

    CloseHandle(hReadPipe);

    m_PendingBuildOutput = std::move(capturedOutput);
    m_HasNewResult = true;
}

END
