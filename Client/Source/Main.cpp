#include <SDL3/SDL_main.h>
#include "Macro.h"
#include "PlatformPath.h"
#include "ModuleManager.h"
#include "ProjectDescriptor.h"
#include "Functions.h"
#include <algorithm>
#include <cctype>
#include "InputManager.h"
#include "Logger.h"
#if !BAM_SHIPPING
#include "CartPoleEnvironment.h"
#include "RLServer.h"
#endif
#include "ClientGameRuntime.h"
#include "ClientStartupSplash.h"
#include "DirectX12RHI.h"
#include "Renderer.h"
#include "TimeManager.h"
#include <chrono>
#if defined(_WIN32)
#include "CrashHandler.h"
#endif

BEGIN(Client)

class Application final
{
public:
    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    ~Application()
    {
        m_ClientRuntime.Free();

        if (m_Window)
            SDL_DestroyWindow(m_Window);

        if (m_SDLInitialized)
            SDL_Quit();

        if (m_LoggerInitialized)
        {
#if defined(_WIN32)
            Engine::CrashHandler::Shutdown();
#endif
            Engine::Logger::Shutdown();
        }
    }

    Engine::EResult Initialize()
    {
        const auto startupBegan = std::chrono::steady_clock::now();
        ClientStartupSplash splash;
        const bool splashVisible = splash.Show();

        // 1. 프로젝트 루트를 가장 먼저 확정하고 PlatformPath/CWD를 고정
        m_ProjectRoot = ResolveProjectRoot();
        Engine::PlatformPath::SetProjectRoot(m_ProjectRoot);
        std::error_code rootEc;
        if (std::filesystem::exists(m_ProjectRoot / ".bam-client-package", rootEc) && !rootEc)
            Engine::PlatformPath::SetEngineRoot(m_ProjectRoot);
        rootEc.clear();
        std::filesystem::current_path(m_ProjectRoot, rootEc);

        // 2. 로거를 프로젝트 루트 기준으로 안전하게 초기화
        const auto logPath = m_ProjectRoot / "Logs" / "Client.log";
        if (!Engine::Logger::Initialize(logPath))
            return Engine::EResult::Fail;

        m_LoggerInitialized = true;
        if (rootEc)
            BAM_LOG(Warn, "Client", "Startup", "Could not set working directory to {}: {}", m_ProjectRoot.string(), rootEc.message());
        if (!splashVisible)
            BAM_LOG(Warn, "Client", "Startup", "The BamEngine startup screen could not be shown.");
#if defined(_WIN32)
        Engine::CrashHandler::Initialize(m_ProjectRoot / "Logs" / "Crashes");
        Engine::CrashHandler::SetCustomContext("Host", "Client");
#endif

        if (const auto result = m_ClientRuntime.LoadProject(m_ProjectRoot); result != Engine::EResult::Success)
        {
            BAM_LOG(Error, "Client", "Startup", "LoadProject failed for '{}': {}", m_ProjectRoot.string(), Engine::ResultToString(result));
            return result;
        }

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            BAM_LOG(Error, "Client", "Application", "SDL_Init failed: {}", SDL_GetError());
            return Engine::EResult::Fail;
        }

        m_SDLInitialized = true;
        const auto& settings = m_ClientRuntime.GetProjectSettings();
        const char* title = settings.WindowTitle.empty() ? "BamEngine Client" : settings.WindowTitle.c_str();
        m_Window = SDL_CreateWindow(title, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN);

        if (!m_Window)
        {
            BAM_LOG(Error, "Client", "Application", "SDL_CreateWindow failed: {}", SDL_GetError());
            return Engine::EResult::Fail;
        }

        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height) || width <= 0 || height <= 0)
            return Engine::EResult::Fail;

        const SDL_PropertiesID properties = SDL_GetWindowProperties(m_Window);
        if (!properties)
            return Engine::EResult::Fail;

        HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!hwnd)
            return Engine::EResult::Fail;
        splash.SetBounds(hwnd);

        DirectX12RHIDesc rhiDesc = {};
        rhiDesc.windowHandle = hwnd;
        rhiDesc.width = static_cast<Engine::uint32>(width);
        rhiDesc.height = static_cast<Engine::uint32>(height);
        rhiDesc.isVSync = true;

        Engine::RendererDesc rendererDesc = {};
        rendererDesc.rhiType = Engine::ERHIType::DirectX12;
        rendererDesc.rhiDesc = &rhiDesc;
        Engine::EResult moduleLoadResult = Engine::EResult::Success;
        const auto initialized = m_ClientRuntime.Initialize(rendererDesc,
            [this, &moduleLoadResult] { moduleLoadResult = LoadGameModule(); },
            [&splash](ClientStartupStage stage) { splash.SetStage(stage); });
        if (Engine::IsFailure(moduleLoadResult))
        {
            BAM_LOG(Error, "Client", "Startup", "Aborting startup because GameModule failed to load: {}",
                Engine::ResultToString(moduleLoadResult));
            return moduleLoadResult;
        }
        if (initialized != Engine::EResult::Success)
            return initialized;

        splash.SetStage(ClientStartupStage::FirstFrame);
        if (!SDL_ShowWindow(m_Window))
        {
            BAM_LOG(Error, "Client", "Startup", "SDL_ShowWindow failed: {}", SDL_GetError());
            return Engine::EResult::Fail;
        }
        splash.SetBounds(hwnd);

        if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height) || width <= 0 || height <= 0)
        {
            BAM_LOG(Error, "Client", "Startup", "Could not determine the visible game window size: {}", SDL_GetError());
            SDL_HideWindow(m_Window);
            return Engine::EResult::Fail;
        }

        auto& time = Engine::TimeManager::Get();
        time.ResetFrameClock();
        const auto firstFrame = m_ClientRuntime.RunFrame(static_cast<Engine::uint32>(width),
            static_cast<Engine::uint32>(height), {}, false, false);
        if (firstFrame != Engine::EResult::Success)
        {
            BAM_LOG(Error, "Client", "Startup", "First game frame failed: {}", Engine::ResultToString(firstFrame));
            SDL_HideWindow(m_Window);
            return firstFrame;
        }

        // Keep the splash visible while the initial D3D12 frame finishes on the GPU.
        auto* dx12 = static_cast<Engine::DirectX12RHI*>(Engine::Renderer::Get().GetRHI());
        const auto presented = dx12->WaitForGPU();
        if (presented != Engine::EResult::Success)
        {
            BAM_LOG(Error, "Client", "Startup", "The GPU did not complete the first game frame: {}", Engine::ResultToString(presented));
            SDL_HideWindow(m_Window);
            return presented;
        }

        splash.Hide();
        time.ResetFrameClock();
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startupBegan);
        BAM_LOG(Info, "Client", "Startup", "First game frame completed after {} ms.", millis.count());
        return Engine::EResult::Success;
    }

    Engine::EResult Run()
    {
        bool shouldQuit = false;
        while (!shouldQuit)
        {
            const Engine::EResult result = RunFrame(shouldQuit);
            if (result != Engine::EResult::Success)
                return result;
        }

        return Engine::EResult::Success;
    }

    Engine::EResult RunFrame(bool& shouldQuit)
    {
        shouldQuit = false;
        if (!m_Window)
            return Engine::EResult::InvalidArgument;

        SDL_Event event = {};
        while (SDL_PollEvent(&event))
        {
            Engine::InputManager::Get().ProcessEvent(event);

            if (event.type == SDL_EVENT_QUIT)
                shouldQuit = true;

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window))
                shouldQuit = true;
        }

        if (shouldQuit)
            return Engine::EResult::Success;

        if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED)
            return m_ClientRuntime.RunFrame(0, 0, {}, false, false);

        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height))
            return Engine::EResult::Fail;

        if (width <= 0 || height <= 0)
            return m_ClientRuntime.RunFrame(0, 0, {}, false, false);

        int logicalWidth = 0;
        int logicalHeight = 0;
        if (!SDL_GetWindowSize(m_Window, &logicalWidth, &logicalHeight))
            return Engine::EResult::Fail;

        return m_ClientRuntime.RunFrame(static_cast<Engine::uint32>(width), static_cast<Engine::uint32>(height),
            glm::vec2(logicalWidth, logicalHeight), SDL_GetKeyboardFocus() == m_Window, SDL_GetMouseFocus() == m_Window);
    }

private:
    Engine::EResult LoadGameModule()
    {
        const auto exeDir = Engine::PlatformPath::GetExecutableDirectory();
        std::error_code ec;

        // 1. .bamproject에 선언된 프로젝트 전용 Runtime 모듈 우선 로드
        auto projectFile = Engine::ProjectDescriptor::FindProjectFile(m_ProjectRoot);
        if (projectFile.empty() && m_ProjectRoot != exeDir)
            projectFile = Engine::ProjectDescriptor::FindProjectFile(exeDir);

        Engine::ProjectDescriptor desc;
        if (!projectFile.empty() && Engine::ProjectDescriptor::LoadFromFile(projectFile, desc) && !desc.Modules.empty())
        {
            for (const auto& mod : desc.ResolveModules(m_ProjectRoot))
            {
                std::string lowerType = mod.Type;
                std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (!lowerType.empty() && lowerType != "runtime")
                    continue;

                const std::wstring modNameW = Engine::StrToWStr(mod.Name);
                const std::filesystem::path dllName = modNameW + L".dll";
                std::filesystem::path modulePath;
                for (const auto& candidate : { exeDir / dllName, mod.BinaryPath, m_ProjectRoot / dllName })
                {
                    if (!candidate.empty() && std::filesystem::exists(candidate, ec) && !ec)
                    {
                        modulePath = candidate;
                        break;
                    }
                    ec.clear();
                }

                if (modulePath.empty())
                {
                    BAM_LOG(Warn, "Client", "Module", "Project module '{}' DLL not found (optional); continuing.", mod.Name);
                    continue;
                }

                const Engine::EResult result = Engine::ModuleManager::Get().LoadModule(modNameW, modulePath);
                if (Engine::IsFailure(result))
                {
                    BAM_LOG(Error, "Client", "Module", "Failed to load project module '{}' from {}: {}",
                        mod.Name, modulePath.string(), Engine::ResultToString(result));
                    return result;
                }
                BAM_LOG(Info, "Client", "Module", "Loaded project module '{}' from {}", mod.Name, modulePath.string());
            }
            return Engine::EResult::Success;
        }

        // 2. 하위 호환: 기본 GameModule.dll 로드
        std::filesystem::path modulePath = exeDir / L"GameModule.dll";
        bool exists = std::filesystem::exists(modulePath, ec);
        if (ec)
        {
            BAM_LOG(Error, "Client", "Module", "Failed to inspect GameModule path {}: {}", modulePath.string(), ec.message());
            return Engine::EResult::Fail;
        }

        if (!exists && m_ProjectRoot != exeDir)
        {
            modulePath = m_ProjectRoot / L"GameModule.dll";
            exists = std::filesystem::exists(modulePath, ec);
            if (ec)
            {
                BAM_LOG(Error, "Client", "Module", "Failed to inspect GameModule path {}: {}", modulePath.string(), ec.message());
                return Engine::EResult::Fail;
            }
        }

        if (!exists)
        {
            BAM_LOG(Warn, "Client", "Module", "GameModule.dll not found (optional); continuing without game module.");
            return Engine::EResult::Success;
        }

        const Engine::EResult result = Engine::ModuleManager::Get().LoadModule(L"GameModule", modulePath);
        if (Engine::IsFailure(result))
        {
            BAM_LOG(Error, "Client", "Module", "Failed to load GameModule from {}: {}",
                modulePath.string(), Engine::ResultToString(result));
            return result;
        }

        BAM_LOG(Info, "Client", "Module", "Loaded GameModule from {}", modulePath.string());
        return Engine::EResult::Success;
    }

private:
    std::filesystem::path ResolveProjectRoot() const
    {
        auto exeDir = Engine::PlatformPath::GetExecutableDirectory();
        while (!exeDir.empty() && exeDir != exeDir.root_path() && exeDir.filename().empty())
            exeDir = exeDir.parent_path();

        std::error_code ec;
        // 1. 배포 환경: 실행 파일 옆에 패키지 마커(.bam-client-package) 또는 Config/Project.json이 있는 경우
        if (std::filesystem::exists(exeDir / ".bam-client-package", ec) ||
            std::filesystem::exists(exeDir / "Config" / "Project.json", ec))
        {
            return exeDir;
        }
        // 2. 개발 환경: 상위 부모 디렉터리를 거슬러 올라가며 마커(Config/Project.json 또는 .bam) 탐색
        std::filesystem::path cur = exeDir;
        for (int i = 0; i < 7; ++i)
        {
            ec.clear();
            if (std::filesystem::exists(cur / "Config" / "Project.json", ec) ||
                std::filesystem::exists(cur / ".bam", ec))
            {
                return cur;
            }
            if (!cur.has_parent_path() || cur.parent_path() == cur) break;
            cur = cur.parent_path();
        }
        return exeDir;
    }
private:
    SDL_Window* m_Window = nullptr;
    ClientGameRuntime m_ClientRuntime;
    bool m_SDLInitialized = false;
    bool m_LoggerInitialized = false;
    std::filesystem::path m_ProjectRoot;
};

END

int main(int argc, char* argv[])
{
#if !BAM_SHIPPING
    // ── 강화학습 헤드리스 모드 (--rl 또는 --headless-rl) ──
    for (int i = 1; i < argc; ++i)
    {
        if (std::string_view(argv[i]) == "--rl" || std::string_view(argv[i]) == "--headless-rl")
        {
            auto env = std::make_shared<Engine::CartPoleEnvironment>();
            Engine::RLServer server(env);
            if (server.Start(9999))
            {
                fmt::print("=====================================================\n");
                fmt::print("  BamEngine RLServer running on 127.0.0.1:9999\n");
                fmt::print("  Waiting for Python gymnasium client... (Ctrl+C to stop)\n");
                fmt::print("=====================================================\n");

                while (server.IsRunning())
                {
                    if (server.AcceptClient())
                        server.ServeClient();
                }
            }
            return 0;
        }
    }
#else
    (void)argc;
    (void)argv;
#endif

    SDL_SetMainReady();

    Client::Application application;
    Engine::EResult result = application.Initialize();

    if (result == Engine::EResult::Success)
        result = application.Run();

    if (result != Engine::EResult::Success)
    {
        fmt::print(stderr, "Client failed: {}\n", Engine::ResultToString(result));
        return 1;
    }

    return 0;
}
