#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <archive/JsonArchive.h>

#include "PlatformPath.h"
#include "Config.h"
#include "ConfigManager.h"
#include "ReflectionRegistry.h"

#include "Runtime.h"
#include "Systems.h"
#include "ModuleManager.h"
#include "SceneManager.h"
#include "Components/Render/CameraComponent.h"
#include "Components/Core/FlagComponent.h"
#include "CartPoleEnvironment.h"
#include "RLServer.h"

BEGIN(Client)

class Application final
{
public:
    Application() = default;
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    ~Application()
    {
        if (m_Runtime)
            Runtime::Destroy();

        if (m_Window)
            SDL_DestroyWindow(m_Window);

        if (m_SDLInitialized)
            SDL_Quit();

        if (m_LoggerInitialized)
            Logger::Shutdown();
    }

    EResult Initialize()
    {
        // 1. 프로젝트 루트를 가장 먼저 확정
        m_ProjectRoot = ResolveProjectRoot();

        // 2. 로거를 프로젝트 루트 기준으로 안전하게 초기화
        const auto logPath = m_ProjectRoot / "Logs" / "Client.log";
        if (!Logger::Initialize(logPath))
            return EResult::Fail;

        m_LoggerInitialized = true;

        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            BAM_LOG(Error, "Client", "Application", "SDL_Init failed: {}", SDL_GetError());
            return EResult::Fail;
        }

        m_SDLInitialized = true;
        m_Window = SDL_CreateWindow("BamEngine Client", 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

        if (!m_Window)
        {
            BAM_LOG(Error, "Client", "Application", "SDL_CreateWindow failed: {}", SDL_GetError());
            return EResult::Fail;
        }

        int width = 0;
        int height = 0;
        if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height) || width <= 0 || height <= 0)
            return EResult::Fail;

        const SDL_PropertiesID properties = SDL_GetWindowProperties(m_Window);
        if (!properties)
            return EResult::Fail;

        HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!hwnd)
            return EResult::Fail;

        DirectX12RHIDesc rhiDesc = {};
        rhiDesc.windowHandle = hwnd;
        rhiDesc.width = static_cast<uint32>(width);
        rhiDesc.height = static_cast<uint32>(height);
        rhiDesc.isVSync = true;

        RUNTIMEDESC runtimeDesc = {};
        runtimeDesc.RendererDesc.rhiType = ERHIType::DirectX12;
        runtimeDesc.RendererDesc.rhiDesc = &rhiDesc;

        // 엔진 독립성을 지키며 호스트 경로 주입
        runtimeDesc.ProjectRoot = m_ProjectRoot;
        runtimeDesc.AssetRoot = "Resources";

        m_Runtime = Runtime::Create(&runtimeDesc);
        if (!m_Runtime)
            return EResult::Fail;

        // 엔진 초기화 후 ProjectSettings 로드
        LoadProjectSettings();

        if (IsFailure(InitializeSystems()))
            return EResult::Fail;

        InitializeRenderPasses();
        LoadGameModule();
        if (IsFailure(LoadStartupScene()))
            return EResult::Fail;

        BAM_LOG(Info, "Client", "Application", "Client initialization finished successfully");
        return EResult::Success;
    }

    EResult Run()
    {
        if (!m_Window || !m_Runtime)
            return EResult::InvalidArgument;

        auto& timeManager = TimeManager::Get();
        RHI* rhi = Renderer::Get().GetRHI();

        while (true)
        {
            bool shouldQuit = false;
            SDL_Event event = {};

            while (SDL_PollEvent(&event))
            {
                InputManager::Get().ProcessEvent(event);

                if (event.type == SDL_EVENT_QUIT)
                    shouldQuit = true;

                if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window))
                    shouldQuit = true;
            }

            if (shouldQuit)
                break;

            timeManager.Update();
            const f32 dt = timeManager.GetDeltaTime();

            if (SDL_GetWindowFlags(m_Window) & SDL_WINDOW_MINIMIZED)
                continue;

            int width = 0;
            int height = 0;
            if (!SDL_GetWindowSizeInPixels(m_Window, &width, &height))
                return EResult::Fail;

            if (width <= 0 || height <= 0)
                continue;

            const uint32 pixelWidth = static_cast<uint32>(width);
            const uint32 pixelHeight = static_cast<uint32>(height);

            if (pixelWidth != rhi->GetSwapChainWidth() || pixelHeight != rhi->GetSwapChainHeight())
            {
                const EResult result = rhi->Resize(pixelWidth, pixelHeight);
                if (result != EResult::Success)
                    return result;
            }

            while (timeManager.IsFixedUpdateReady())
                m_Runtime->FixedUpdate(timeManager.GetFixedStep());

            m_Runtime->Update(dt);
            m_Runtime->LateUpdate(dt);

            UpdateViewportCamera();

            const EResult result = m_Runtime->Render(dt);
            if (result != EResult::Success)
                return result;
        }

        return EResult::Success;
    }

private:
    EResult InitializeSystems()
    {
        auto& systemManager = SystemManager::Get();
        if (!systemManager.AddSystem<TransformSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<CameraSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<LightSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<MeshRenderSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<SpriteRenderSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<SpriteAnimationSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<AnimationSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<UIRenderSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<AudioSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<TerrainSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<PhysicsSystem>()) return EResult::Fail;
        if (!systemManager.AddSystem<GraphSystem>()) return EResult::Fail;
        return EResult::Success;
    }

    void InitializeRenderPasses()
    {
        // 스왑체인 백버퍼로 직접 출력하는 Forward 렌더 패스 등록
        m_ForwardPassID = RenderPassManager::Get().RegisterRenderPass(
            L"Client Forward Pass", {}, L"",
            ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
            ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
            vec4(0.08f, 0.12f, 0.18f, 1.0f), 0, ERenderSortType::FrontToBack,
            ERenderPassType::Forward, EBlendMode::Forward | EBlendMode::Opaque | EBlendMode::Masked);
    }

    void LoadGameModule()
    {
        const auto exeDir = PlatformPath::GetExecutableDirectory();
        std::filesystem::path modulePath = exeDir / L"GameModule.dll";
        if (!std::filesystem::exists(modulePath))
            modulePath = m_ProjectRoot / L"GameModule.dll";

        if (!std::filesystem::exists(modulePath))
        {
            BAM_LOG(Warn, "Client", "Module", "GameModule.dll not found at: {}", modulePath.string());
            return;
        }

        const EResult result = ModuleManager::Get().LoadModule(L"GameModule", modulePath);
        if (IsFailure(result))
            BAM_LOG(Error, "Client", "Module", "Failed to load GameModule: {}", ResultToString(result));
    }

    EResult LoadStartupScene()
    {
        // ProjectSettings에 지정된 StartupScenePath가 있으면 우선 사용, 비어있으면 기본 씬으로 폴백
        std::filesystem::path scenePath;
        if (!m_ProjectSettings.StartupScenePath.empty())
            scenePath = m_ProjectRoot / m_ProjectSettings.StartupScenePath;
        else
            scenePath = m_ProjectRoot / "Resources" / "Scenes" / "NewScene2.json";

        if (!std::filesystem::exists(scenePath))
        {
            BAM_LOG(Error, "Client", "Startup", "Startup scene file does not exist: {}", scenePath.string());
            return EResult::Fail;
        }

        archive::JsonArchive ar;
        const EResult result = SceneManager::Get().LoadScene(ar, scenePath.wstring());
        if (result != EResult::Success)
        {
            BAM_LOG(Error, "Client", "Startup", "Failed to load startup scene: {}", ResultToString(result));
            return result;
        }

        BAM_LOG(Info, "Client", "Startup", "Loaded startup scene: {}", scenePath.string());
        return EResult::Success;
    }

    void UpdateViewportCamera()
    {
        auto* renderPass = RenderPassManager::Get().GetRenderPassByID(m_ForwardPassID);
        if (!renderPass) return;

        auto* currentScene = SceneManager::Get().GetCurrentScene();
        if (!currentScene) return;

        auto& registry = currentScene->GetRegistry();
        auto cameraView = registry.view<CameraComponent, FlagComponent>();
        std::optional<CameraBuffer> selectedBuffer;

        for (auto entity : cameraView)
        {
            const auto& flag = cameraView.get<FlagComponent>(entity);
            if (!flag.IsActiveInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
                continue;

            const auto& camera = cameraView.get<CameraComponent>(entity);
            const auto buffer = camera.GetCameraBuffer();
            if (!buffer) continue;

            if (!selectedBuffer || camera.IsMainCamera)
                selectedBuffer = buffer;

            if (camera.IsMainCamera)
                break;
        }

        if (!selectedBuffer) return;

        selectedBuffer->time = static_cast<f32>(TimeManager::Get().GetTotalTime());
        Renderer::Get().RegisterViewportCamera(*selectedBuffer, renderPass);
    }

    private:
        std::filesystem::path ResolveProjectRoot() const
        {
            const auto exeDir = PlatformPath::GetExecutableDirectory();
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

        void LoadProjectSettings()
        {
            const auto projectJsonPath = m_ProjectRoot / "Config" / "Project.json";
            if (std::filesystem::exists(projectJsonPath))
            {
                const auto& registry = ReflectionRegistry::Get().GetRegistry();
                Engine::ConfigManager::Load(m_ProjectSettings, projectJsonPath, registry);
            }
        }    
private:
    SDL_Window* m_Window = nullptr;
    Runtime* m_Runtime = nullptr;
    RenderPassID m_ForwardPassID = 0;
    bool m_SDLInitialized = false;
    bool m_LoggerInitialized = false;
    std::filesystem::path m_ProjectRoot;
    Engine::ProjectSettings m_ProjectSettings;
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
    EResult result = application.Initialize();

    if (result == EResult::Success)
        result = application.Run();

    if (result != EResult::Success)
    {
        fmt::print(stderr, "Client failed: {}\n", ResultToString(result));
        return 1;
    }

    return 0;
}