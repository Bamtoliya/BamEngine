#include <SDL3/SDL_main.h>
#include "PlatformPath.h"
#include "ModuleManager.h"
#include "InputManager.h"
#include "Logger.h"
#include "CartPoleEnvironment.h"
#include "RLServer.h"
#include "ClientGameRuntime.h"

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
            Engine::Logger::Shutdown();
    }

    Engine::EResult Initialize()
    {
        // 1. 프로젝트 루트를 가장 먼저 확정
        m_ProjectRoot = ResolveProjectRoot();

        // 2. 로거를 프로젝트 루트 기준으로 안전하게 초기화
        const auto logPath = m_ProjectRoot / "Logs" / "Client.log";
        if (!Engine::Logger::Initialize(logPath))
            return Engine::EResult::Fail;

        m_LoggerInitialized = true;

        if (const auto result = m_ClientRuntime.LoadProject(m_ProjectRoot); result != Engine::EResult::Success)
            return result;

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            BAM_LOG(Error, "Client", "Application", "SDL_Init failed: {}", SDL_GetError());
            return Engine::EResult::Fail;
        }

        m_SDLInitialized = true;
        const auto& settings = m_ClientRuntime.GetProjectSettings();
        const char* title = settings.WindowTitle.empty() ? "BamEngine Client" : settings.WindowTitle.c_str();
        m_Window = SDL_CreateWindow(title, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

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

        DirectX12RHIDesc rhiDesc = {};
        rhiDesc.windowHandle = hwnd;
        rhiDesc.width = static_cast<Engine::uint32>(width);
        rhiDesc.height = static_cast<Engine::uint32>(height);
        rhiDesc.isVSync = true;

        Engine::RendererDesc rendererDesc = {};
        rendererDesc.rhiType = Engine::ERHIType::DirectX12;
        rendererDesc.rhiDesc = &rhiDesc;
        return m_ClientRuntime.Initialize(rendererDesc, [this] { LoadGameModule(); });
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
            return m_ClientRuntime.RunFrame(0, 0, false, true);

        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height))
            return Engine::EResult::Fail;

        if (width <= 0 || height <= 0)
            return m_ClientRuntime.RunFrame(0, 0, false, false);

        return m_ClientRuntime.RunFrame(static_cast<Engine::uint32>(width), static_cast<Engine::uint32>(height),
            SDL_GetKeyboardFocus() == m_Window, false);
    }

private:
    void LoadGameModule()
    {
        const auto exeDir = Engine::PlatformPath::GetExecutableDirectory();
        std::filesystem::path modulePath = exeDir / L"GameModule.dll";
        if (!std::filesystem::exists(modulePath))
            modulePath = m_ProjectRoot / L"GameModule.dll";

        if (!std::filesystem::exists(modulePath))
        {
            BAM_LOG(Warn, "Client", "Module", "GameModule.dll not found at: {}", modulePath.string());
            return;
        }

        const Engine::EResult result = Engine::ModuleManager::Get().LoadModule(L"GameModule", modulePath);
        if (Engine::IsFailure(result))
            BAM_LOG(Error, "Client", "Module", "Failed to load GameModule: {}", Engine::ResultToString(result));
    }

private:
    std::filesystem::path ResolveProjectRoot() const
        {
            const auto exeDir = Engine::PlatformPath::GetExecutableDirectory();
            std::error_code ec;

            // 1. 배포 환경: 실행 파일 바로 옆에 Resources 폴더나 Config 폴더가 있는 경우
            if (std::filesystem::exists(exeDir / "Resources", ec) || std::filesystem::exists(exeDir / "Config", ec))
                return exeDir;

            // 2. 개발 환경: 상위 부모 디렉터리를 거슬러 올라가며 마커(Config/Project.json 또는 .bam) 탐색
            std::filesystem::path cur = exeDir;
            for (int i = 0; i < 7; ++i)
            {
                if (std::filesystem::exists(cur / "Config" / "Project.json", ec) ||
                    std::filesystem::exists(cur / ".bam", ec))
                {
                    return cur;
                }
                if (!cur.has_parent_path()) break;
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
