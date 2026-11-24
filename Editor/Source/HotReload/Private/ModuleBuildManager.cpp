#include "ModuleBuildManager.h"
#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include "PlatformPath.h"
#include "Functions.h"
#include <mutex>
#include <windows.h>
#include <fstream>
#include <algorithm>
#include <string_view>

BEGIN(Editor)

namespace
{
    // Ninja 빌드의 cl.exe는 INCLUDE·LIB 환경 변수로 표준 헤더와 라이브러리를 찾는다.
    bool HasMsvcEnvironment()
    {
        return GetEnvironmentVariableW(L"INCLUDE", nullptr, 0) > 0;
    }

    std::wstring GetCommandProcessorPath()
    {
        wchar_t buffer[MAX_PATH]{};
        const DWORD length = GetEnvironmentVariableW(L"ComSpec", buffer, MAX_PATH);
        return (length > 0 && length < MAX_PATH) ? std::wstring(buffer) : std::wstring(L"C:\\Windows\\System32\\cmd.exe");
    }

    // CMake가 실제로 선택한 컴파일러에서 VS 설치 경로와 도구 버전을 찾는다.
    std::wstring FindDevEnvironmentCommand(const std::filesystem::path& buildDir)
    {
        std::ifstream cache(buildDir / "CMakeCache.txt");
        std::string compiler;
        for (std::string line; std::getline(cache, line);)
        {
            const size_t equal = line.find('=');
            if (line.starts_with("CMAKE_CXX_COMPILER:") && equal != std::string::npos)
            {
                compiler = line.substr(equal + 1);
                break;
            }
        }

        if (compiler.find_first_of("/\\") == std::string::npos)
        {
            const auto compilerFile = buildDir / "CMakeFiles" / BAM_EDITOR_CMAKE_VERSION / "CMakeCXXCompiler.cmake";
            std::ifstream configuredCompiler(compilerFile);
            constexpr std::string_view prefix = "set(CMAKE_CXX_COMPILER \"";
            for (std::string line; std::getline(configuredCompiler, line);)
            {
                if (!line.starts_with(prefix)) continue;
                const auto end = line.find('"', prefix.size());
                if (end != std::string::npos)
                    compiler = line.substr(prefix.size(), end - prefix.size());
                break;
            }
        }

        std::ranges::replace(compiler, '\\', '/');
        const std::string marker = "/VC/Tools/MSVC/";
        const size_t markerPos = compiler.find(marker);
        const size_t versionBegin = markerPos == std::string::npos ? std::string::npos : markerPos + marker.size();
        const size_t versionEnd = versionBegin == std::string::npos ? std::string::npos : compiler.find('/', versionBegin);
        if (versionEnd == std::string::npos) return {};

        const std::string binMarker = "/bin/Host";
        const size_t hostBegin = compiler.find(binMarker, versionEnd);
        const size_t hostEnd = hostBegin == std::string::npos ? std::string::npos : compiler.find('/', hostBegin + binMarker.size());
        const size_t archEnd = hostEnd == std::string::npos ? std::string::npos : compiler.find('/', hostEnd + 1);
        if (archEnd == std::string::npos) return {};
        const auto devArch = [](std::string_view arch) -> std::wstring
        {
            if (arch == "x64") return L"amd64";
            if (arch == "x86") return L"x86";
            if (arch == "arm64") return L"arm64";
            return {};
        };
        const auto hostArch = devArch(std::string_view(compiler).substr(hostBegin + binMarker.size(), hostEnd - hostBegin - binMarker.size()));
        const auto targetArch = devArch(std::string_view(compiler).substr(hostEnd + 1, archEnd - hostEnd - 1));
        if (hostArch.empty() || targetArch.empty()) return {};

        const auto devCmd = Engine::Utf8ToPath(compiler.substr(0, markerPos)) / "Common7" / "Tools" / "VsDevCmd.bat";
        std::error_code ec;
        if (!std::filesystem::is_regular_file(devCmd, ec)) return {};

        const std::string toolsVersion = compiler.substr(versionBegin, versionEnd - versionBegin);
        return L"call \"" + devCmd.wstring() + L"\" -arch=" + targetArch + L" -host_arch=" + hostArch +
            L" -vcvars_ver=" + StrToWStr(toolsVersion) + L" -no_logo";
    }
}

class ModuleSourceFileListener final : public efsw::FileWatchListener
{
public:
    explicit ModuleSourceFileListener(ModuleBuildManager* manager) : m_Manager(manager) {}

    void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename,
        efsw::Action action, std::string oldFilename) override
    {
        if (action != efsw::Actions::Modified && action != efsw::Actions::Add && action != efsw::Actions::Moved)
            return;

        try
        {
            const auto fullPath = Engine::Utf8ToPath(dir) / Engine::Utf8ToPath(filename);
            const auto ext = fullPath.extension().string();
            if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".inl" || filename == "CMakeLists.txt")
            {
                EDITOR_LOG_INFO("[ModuleBuildManager] Source modified detected: {} (action: {})",
                    filename, static_cast<int>(action));
                if (m_Manager) m_Manager->NotifyFileModified(fullPath);
            }
        }
        catch (const std::exception& error)
        {
            EDITOR_LOG_WARN("[ModuleBuildManager] Ignored file event: {}, {}", filename, error.what());
        }
        catch (...)
        {
            EDITOR_LOG_WARN("[ModuleBuildManager] Ignored file event: {}", filename);
        }
    }

private:
    ModuleBuildManager* m_Manager = nullptr;
};

#pragma region Constructor&Destructor
EResult ModuleBuildManager::Initialize(const std::filesystem::path& projectRoot, const std::filesystem::path& buildDir,
    const std::filesystem::path& sourceDir, const std::wstring& targetName)
{
    m_TriggersGameModuleReload = (targetName == L"GameModule");
    Reconfigure(projectRoot, buildDir, sourceDir, targetName);
    return EResult::Success;
}

void ModuleBuildManager::Quiesce()
{
    StopWatching();
    ClearPendingFiles();
    if (m_BuildThread.joinable()) m_BuildThread.join();
    m_HasNewResult.store(false);
    m_Status.store(EModuleBuildStatus::Idle);
}

void ModuleBuildManager::Reconfigure(const std::filesystem::path& projectRoot, const std::filesystem::path& buildDir,
    const std::filesystem::path& sourceDir, const std::wstring& targetName,
    const std::filesystem::path& cmakeSourceDir, const std::filesystem::path& outputBinDir)
{
    Quiesce();

    m_ProjectRoot = projectRoot;
    m_BuildDir = buildDir;
    m_SourceDir = sourceDir.empty() ? std::filesystem::path{} : (sourceDir.is_absolute() ? sourceDir : (m_ProjectRoot / sourceDir));
    m_CMakeSourceDir = cmakeSourceDir;
    m_OutputBinDir = outputBinDir;
    m_TargetName = targetName;

    std::error_code ec;
    if (!m_SourceDir.empty() && std::filesystem::exists(m_SourceDir, ec))
    {
        StartWatching(m_SourceDir);
    }
    else if (!m_SourceDir.empty())
    {
        EDITOR_LOG_WARN("[ModuleBuildManager] Source directory not found: {}", WStrToStr(m_SourceDir.wstring()));
    }
}

bool ModuleBuildManager::HasValidTarget() const
{
    if (m_TargetName.empty() || m_BuildDir.empty())
        return false;
    std::error_code ec;
    if (std::filesystem::is_regular_file(m_BuildDir / "CMakeCache.txt", ec))
        return true;
    return !m_CMakeSourceDir.empty() && std::filesystem::is_regular_file(m_CMakeSourceDir / "CMakeLists.txt", ec);
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
                LOCAL("ToolBar.ModuleCompile"), LOCAL_FMT("Notification.NamedModuleBuildSucceeded", WStrToStr(m_TargetName)), ELogLevel::Info, 3.0f);

            // 빌드가 완전히 끝났으므로 안전하게 모듈 핫로드 실행
            if (m_TriggersGameModuleReload)
                HotReloadManager::Get().GetModuleReloader().ReloadModule();
        }
        else
        {
            m_Status = EModuleBuildStatus::Failed;
            EDITOR_LOG_ERROR("[ModuleBuildManager] {} build failed:\n{}", WStrToStr(m_TargetName), m_LastBuildOutput);
            NotificationManager::Get().AddNotification(
                LOCAL("ToolBar.ModuleCompile"), LOCAL_FMT("Notification.NamedModuleBuildFailed", WStrToStr(m_TargetName)), ELogLevel::Error, 5.0f);
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
    if (!HasValidTarget())
    {
        EDITOR_LOG_WARN("[ModuleBuildManager] No buildable C++ module target configured for the active project.");
        return false;
    }

    if (m_BuildThread.joinable()) m_BuildThread.join();

    m_Status = EModuleBuildStatus::Compiling;
    EDITOR_LOG_INFO("[ModuleBuildManager] Starting background build for {}", WStrToStr(m_TargetName));
    NotificationManager::Get().AddNotification(
        LOCAL("ToolBar.ModuleCompile"), LOCAL_FMT("Notification.CompilingNamedModule", WStrToStr(m_TargetName)), ELogLevel::Info, 2.0f);

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
    if (m_TriggersGameModuleReload && !m_TargetName.empty())
    {
        // 디버거가 잡고 있는 PDB 때문에 링크가 막히지 않도록 이전 PDB 이름을 바꾼다.
        // 독립 프로젝트 모듈(Binaries/Win64), 다중 구성(Visual Studio bin/<Config>), 단일 구성(Ninja bin)을 모두 확인한다.
        std::error_code ec;
        const std::wstring pdbName = m_TargetName + L".pdb";
        for (const auto& binDir : { m_OutputBinDir, m_BuildDir / "bin" / config, m_BuildDir / "bin" })
        {
            if (binDir.empty()) continue;
            const auto pdbPath = binDir / pdbName;
            if (!std::filesystem::exists(pdbPath, ec)) continue;
            const auto oldPdb = binDir / (m_TargetName + L"_old_" + std::to_wstring(GetTickCount64()) + L".pdb");
            std::filesystem::rename(pdbPath, oldPdb, ec);
            break;
        }
    }

    const auto runCommand = [&](const std::wstring& appPath, const std::wstring& commandLine,
        std::string& outCaptured, DWORD& outExitCode) -> bool
    {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;

        HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        {
            outCaptured += "Failed to create the cmake output pipe.\n";
            outExitCode = 1;
            return false;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;
        si.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi{};
        std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
        cmdBuffer.push_back(L'\0');

        const BOOL launched = CreateProcessW(appPath.c_str(), cmdBuffer.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, m_ProjectRoot.c_str(), &si, &pi);
        CloseHandle(hWritePipe);

        if (launched)
        {
            char buffer[512];
            DWORD bytesRead = 0;
            while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                buffer[bytesRead] = '\0';
                outCaptured += buffer;
            }
            WaitForSingleObject(pi.hProcess, INFINITE);
            outExitCode = 1;
            GetExitCodeProcess(pi.hProcess, &outExitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else
        {
            outCaptured += "Failed to launch cmake process.\n";
            outExitCode = 1;
        }

        CloseHandle(hReadPipe);
        return launched != FALSE;
    };

    std::string capturedOutput;
    std::error_code cacheEc;
    if (!std::filesystem::is_regular_file(m_BuildDir / "CMakeCache.txt", cacheEc) &&
        !m_CMakeSourceDir.empty() && std::filesystem::is_regular_file(m_CMakeSourceDir / "CMakeLists.txt", cacheEc))
    {
        std::wstring cfgCmd = L"\"";
        cfgCmd += BAM_EDITOR_CMAKE_COMMAND;
        cfgCmd += L"\" -S \"" + m_CMakeSourceDir.wstring() + L"\" -B \"" + m_BuildDir.wstring() + L"\"";
        const auto engineRoot = Engine::PlatformPath::GetEngineRoot();
        if (!engineRoot.empty())
            cfgCmd += L" -DBAMENGINE_ROOT=\"" + engineRoot.wstring() + L"\"";
        const auto exeDir = Engine::PlatformPath::GetExecutableDirectory();
        if (std::filesystem::is_regular_file(exeDir.parent_path().parent_path() / "CMakeCache.txt", cacheEc))
            cfgCmd += L" -DBAMENGINE_BUILD_DIR=\"" + exeDir.parent_path().parent_path().wstring() + L"\"";
        else if (std::filesystem::is_regular_file(exeDir.parent_path() / "CMakeCache.txt", cacheEc))
            cfgCmd += L" -DBAMENGINE_BUILD_DIR=\"" + exeDir.parent_path().wstring() + L"\"";

        DWORD cfgExitCode = 1;
        if (!runCommand(BAM_EDITOR_CMAKE_COMMAND, cfgCmd, capturedOutput, cfgExitCode) || cfgExitCode != 0)
        {
            m_PendingBuildOutput = std::move(capturedOutput);
            m_PendingBuildSuccess = false;
            m_HasNewResult = true;
            return;
        }
    }

    std::wstring application = BAM_EDITOR_CMAKE_COMMAND;
    std::wstring cmd = L"\"";
    cmd += BAM_EDITOR_CMAKE_COMMAND;
    cmd += L"\" --build \"" + m_BuildDir.wstring() + L"\" --target " + m_TargetName + L" --config " + config;

#if defined(BAM_EDITOR_MSBUILD)
    // The loaded host already has the dependencies for a plugin-only build.
    cmd += L" -- /p:BuildProjectReferences=false /m";
#else
    // VS에서 실행한 에디터에는 MSVC 환경 변수가 없다. 빌드 트리와 같은 도구 집합의 개발자 환경을 먼저 적용한다.
    if (!HasMsvcEnvironment())
    {
        const std::wstring devEnv = FindDevEnvironmentCommand(m_BuildDir);
        if (devEnv.empty())
        {
            m_PendingBuildOutput = "MSVC environment (INCLUDE) is missing and the configured compiler or VsDevCmd.bat could not be found in the build tree. "
                "Start the editor from a Developer PowerShell or build the module in Visual Studio.";
            m_PendingBuildSuccess = false;
            m_HasNewResult = true;
            return;
        }
        application = GetCommandProcessorPath();
        cmd = L"\"" + application + L"\" /d /s /c \"" + devEnv + L" && " + cmd + L"\"";
    }
#endif

    DWORD buildExitCode = 1;
    const bool launched = runCommand(application, cmd, capturedOutput, buildExitCode);
    m_PendingBuildSuccess = launched && (buildExitCode == 0);
    m_PendingBuildOutput = std::move(capturedOutput);
    m_HasNewResult = true;
}

END
