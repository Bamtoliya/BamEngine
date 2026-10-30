#include "ModuleBuildManager.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include <windows.h>

BEGIN(Editor)

class ModuleSourceFileListener final : public efsw::FileWatchListener
{
public:
    explicit ModuleSourceFileListener(ModuleBuildManager* manager) : m_Manager(manager) {}

    void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
        efsw::Action action, std::string oldFilename) override
    {
        const std::filesystem::path fullPath = std::filesystem::path(dir) / filename;
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
EResult ModuleBuildManager::Initialize(const std::filesystem::path& projectRoot, const std::filesystem::path& buildDir)
{
    m_ProjectRoot = projectRoot;
    m_BuildDir = buildDir;
    m_SourceDir = m_ProjectRoot / "Projects" / "GameModule" / "Source";

    if (std::filesystem::exists(m_SourceDir))
    {
        StartWatching(m_SourceDir);
    }
    else
    {
        EDITOR_LOG_WARN("[ModuleBuildManager] GameModule source directory not found: {}", m_SourceDir.string());
    }

    return EResult::Success;
}

void ModuleBuildManager::Free()
{
    StopWatching();
    if (m_BuildThread.joinable()) m_BuildThread.join();
    __super::Free();
}

ModuleBuildManager* ModuleBuildManager::Create(const std::filesystem::path& projectRoot, const std::filesystem::path& buildDir)
{
    ModuleBuildManager* instance = new ModuleBuildManager();
    if (instance->Initialize(projectRoot, buildDir) != EResult::Success)
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
    m_WatchID = m_FileWatcher->addWatch(sourceDir.string(), m_FileListener, true);
    m_FileWatcher->watch();
    EDITOR_LOG_INFO("[ModuleBuildManager] Watching GameModule source directory: {}", sourceDir.string());
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
    // 1. IHotReloader의 디바운스 큐를 통해 준비된 파일 수집 (저장 후 500ms 경과 시 반환)
    const auto readyFiles = CollectReadyFiles(std::chrono::milliseconds(200));
    if (!readyFiles.empty() && m_Status != EModuleBuildStatus::Compiling)
    {
        RequestBuild();
    }

    // 2. 비동기 빌드 완료 처리 (메인 스레드에서 안전하게 콜백/알림 호출)
    if (m_HasNewResult.exchange(false))
    {
        if (m_BuildThread.joinable()) m_BuildThread.join();

        std::string outputCopy;
        {
            std::lock_guard<std::mutex> lock(m_OutputMutex);
            outputCopy = m_LastBuildOutput;
        }

        if (m_LastBuildSuccess)
        {
            m_Status = EModuleBuildStatus::Succeeded;
            EDITOR_LOG_INFO("[ModuleBuildManager] GameModule compiled successfully! Triggering hot-reload...");
            NotificationManager::Get().AddNotification(
                LOCAL("ToolBar.ModuleCompile"), LOCAL("Notification.CompileSuccess"), ELogLevel::Info, 3.0f);

            // 빌드가 완전히 끝났으므로 안전하게 모듈 핫로드 실행
            HotReloadManager::Get().GetModuleReloader().ReloadModule();
        }
        else
        {
            m_Status = EModuleBuildStatus::Failed;
            EDITOR_LOG_ERROR("[ModuleBuildManager] GameModule compile failed:\n{}", outputCopy);
            NotificationManager::Get().AddNotification(
                LOCAL("ToolBar.ModuleCompile"), LOCAL("Notification.CompileFailed"), ELogLevel::Error, 5.0f);
        }

        if (m_OnBuildCompleted) m_OnBuildCompleted(m_LastBuildSuccess, outputCopy);
    }
}

bool ModuleBuildManager::RequestBuild()
{
    if (m_Status == EModuleBuildStatus::Compiling) return false;

    if (m_BuildThread.joinable()) m_BuildThread.join();

    m_Status = EModuleBuildStatus::Compiling;
    EDITOR_LOG_INFO("[ModuleBuildManager] Starting background build for GameModule...");
    NotificationManager::Get().AddNotification(
        LOCAL("ToolBar.ModuleCompile"), LOCAL("Notification.CompilingModule"), ELogLevel::Info, 2.0f);

    m_BuildThread = std::thread(&ModuleBuildManager::ExecuteBuildAsync, this);
    return true;
}

void ModuleBuildManager::ExecuteBuildAsync()
{
    // 1. 디버거가 PDB를 잡고 있어 발생하는 LNK1201 에러 방지 (기존 PDB를 임시 이름으로 변경)
    const std::filesystem::path binDir = m_BuildDir / "bin" /
#ifdef _DEBUG
        "Debug";
#else
        "Release";
#endif
    std::error_code ec;
    const std::filesystem::path pdbPath = binDir / "GameModule.pdb";
    if (std::filesystem::exists(pdbPath, ec))
    {
        const std::filesystem::path oldPdb = binDir / (L"GameModule_old_" + std::to_wstring(GetTickCount64()) + L".pdb");
        std::filesystem::rename(pdbPath, oldPdb, ec);
    }
    // 2. cmake.exe --build <buildDir> --target GameModule --config Debug
    std::wstring cmd = L"cmake.exe --build \"" + m_BuildDir.wstring() + L"\" --target GameModule";
#ifdef _DEBUG
    cmd += L" --config Debug";
#else
    cmd += L" --config Release";
#endif

    // 핵심 최적화: 이미 빌드된 엔진/에디터 등 20개 프로젝트 의존성 검사를 스킵하고 멀티코어로 즉시 빌드
    cmd += L" -- /p:BuildProjectReferences=false /m";

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
    {
        m_LastBuildSuccess = false;
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
    const BOOL success = CreateProcessW(nullptr, cmdBuffer.data(), nullptr, nullptr, TRUE,
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

        m_LastBuildSuccess = (exitCode == 0);
    }
    else
    {
        capturedOutput = "Failed to launch cmake process.";
        m_LastBuildSuccess = false;
    }

    CloseHandle(hReadPipe);

    {
        std::lock_guard<std::mutex> lock(m_OutputMutex);
        m_LastBuildOutput = std::move(capturedOutput);
    }

    m_HasNewResult = true;
}

END