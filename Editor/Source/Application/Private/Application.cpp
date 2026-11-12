#pragma once
#include "Application.h"
#include <SDL3/SDL.h>
#include <archive/JsonArchive.h>
#include "Runtime.h"

#include "SelectionManager.h"

#include "AssetManager.h"

#include "Systems.h"
#include "PIEManager.h"

#include "ImGuiManager.h"
#include <imgui_internal.h> 

#include "ShortcutManager.h"
#include "ConfigManager.h"
#include "ReflectionRegistry.h"
#include "BamEditorSettings.gen.h"
#include <SDL3/SDL_filesystem.h>
#include "TimeManager.h"
#include "Logger.h"


#include "StartupResourceLoader.h"
#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "TerrainComponent.h"
#include "NotificationManager.h"
#include "CrashHandler.h"
#include "RecoveryManager.h"
#include "IDComponent.h"
#include "TerrainUtility.h"

#include "SceneDocument.h"
#include "EditorSettingsService.h"
#include "PrefabModeManager.h"
#include "SceneViewportPanel.h"
#include "ThumbnailRenderer.h"
#include "BuiltInPrimitiveMeshes.h"
#include "GameViewportPanel.h"
#include "PlatformPath.h"

IMPLEMENT_SINGLETON(Application)

#pragma region Constructor&Destructor
EResult Application::Initialize(void* arg)
{
    if (!arg) return EResult::InvalidArgument;

    CAST_DESC;

    if(!InitializeLogger())
        return EResult::Fail; 

    m_LoggerInitialized = true;
    Engine::CrashHandler::Initialize();

    const auto fail = [this](const char* stage, EResult result)
        {
            BAM_LOG(Error, "Editor", "Application", "{} failed: {}", stage, ResultToString(result));
            Free();
            return result;
        };

    if (!NotificationManager::Create())
        return fail("NotificationManager", EResult::Fail);

    if (!InitializeWindow(*desc))
        return fail("InitializeWindow", EResult::Fail);

    if (!InitializeRuntime(*desc))
        return fail("InitializeRuntime", EResult::Fail);

    ShortcutManagerDesc shortcutDesc;

    EditorSettingsService& editorService = EditorSettingsService::Get();


    const auto& preferencesPath = editorService.GetPreferencesPath();
    if (!preferencesPath.empty())
        shortcutDesc.ConfigPath = preferencesPath.parent_path() / "EditorShortcuts.json";

    shortcutDesc.LegacyPath = Engine::PlatformPath::ResolveUri("Config/EditorShortcuts.json");

    if (!ShortcutManager::Create(&shortcutDesc))
        return fail("ShortcutManager", EResult::Fail);
    if(!IntializeRenderer())
		return fail("IntializeRenderer", EResult::Fail);
    if (!InitializeResources())
        return fail("InitializeResources", EResult::Fail);

    if (!ThumbnailRenderer::Create())
        BAM_LOG(Warn, "Editor", "Asset", "Thumbnail renderer is unavailable; asset icons will be used.");

    if (!InitializeImGui())
        return fail("InitializeImGui", EResult::Fail);

    if(!InitializeLocalization())
        return fail("InitializeLocalization", EResult::Fail);

    auto* rhi = Renderer::Get().GetRHI();
    if (!rhi)
        return fail("BindRuntime", EResult::Fail);

    EditorSettingsService::Get().BindRuntime(*m_ImGuiManager, TimeManager::Get(), LocalizationManager::Get(), *rhi);

    if (!EditorSettingsService::Get().RestoreEditorState())
        return fail("RestoreEditorState", EResult::Fail);

    if (!InitializeSystem())
        return fail("InitializeSystem", EResult::Fail);

    if (!HotReloadManager::Create())
        return fail("HotReloadManager", EResult::Fail);

    if (HotReloadManager::IsCreated())
    {
        HotReloadManager::Get().GetModuleReloader().RegisterPostReloadCallback([]()
            {
                if (auto* graphSystem = SystemManager::Get().GetSystem<GraphSystem>())
                {
                    graphSystem->ReloadReflectedNodes(SceneManager::Get().GetActiveScenes());
                }
            });
    }

    // Editor.exe 실행 파일과 같은 디렉터리에 있는 GameModule.dll 로드
    wchar_t exePathBuf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    const std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();
    const std::filesystem::path modulePath = exeDir / L"GameModule.dll";
    if (std::filesystem::exists(modulePath))
    {
        HotReloadManager::Get().GetModuleReloader().LoadModule(modulePath);
    }
    else
    {
        BAM_LOG(Warn, "Editor", "HotReload", "GameModule.dll not found at: {}", modulePath.string());
    }
    ShortcutManager::Get().LoadFromFile();

    RestoreLastEditedScene();
    if (m_SceneDocumentId == InvalidEditorDocumentId && SceneManager::Get().GetCurrentScene())
        AdoptSceneDocument({});

    RecoveryManagerDesc recoveryDesc{ EditorSettingsService::Get().GetProjectRoot()};
    RecoveryManager::Create(&recoveryDesc);
    m_InitializationCompleted = true;
    return EResult::Success;
}
void Application::Free()
{
    if (m_InitializationCompleted)
    {
        m_InitializationCompleted = false;

        if (RecoveryManager::IsCreated())
        {
            RecoveryManager::Get().Free();
            RecoveryManager::Destroy();
        }

        if (EditorSettingsService::IsCreated())
            CaptureAndSaveEditorWorkspace();
    }

    if (EditorSettingsService::IsCreated())
        EditorSettingsService::Get().UnbindRuntime();

    if (PrefabModeManager::IsCreated())
        PrefabModeManager::Destroy();

    m_Documents.Clear();
    m_SceneDocumentId = InvalidEditorDocumentId;
    m_EditContext.Shutdown();

    if (m_ImGuiManager)
    {
        ImGuiManager::Destroy();
        m_ImGuiManager = nullptr;
    }

    AssetManager::Destroy();

    if (m_SelectionManager)
    {
        SelectionManager::Destroy();
        m_SelectionManager = nullptr;
    }

    if (m_PIEManager)
    {
        PIEManager::Destroy();
        m_PIEManager = nullptr;
    }

    HotReloadManager::Destroy();

    if (ShortcutManager::IsCreated())
    {
        ShortcutManager::Get().SaveToFile();
        ShortcutManager::Destroy();
    }

    if (m_Runtime)
    {
        Runtime::Destroy();
        m_Runtime = nullptr;
    }

    if (m_Window)
    {
        SDL_DestroyWindow(m_Window);
        m_Window = nullptr;
    }

    EditorSettingsService::Destroy();

    if (m_SDLInitialized)
    {
        SDL_Quit();
        m_SDLInitialized = false;
    }

    NotificationManager::Destroy();

    if (m_LoggerInitialized)
    {
        BAM_LOG(Info, "Editor", "Application", "Application cleanup finished");
        Engine::CrashHandler::Shutdown();
        Engine::Logger::Shutdown();
        m_LoggerInitialized = false;
    }
}
#pragma endregion

#pragma region Initialize
EResult Application::InitializeWindow(const ApplicationCreateInfo& createInfo)
{
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fmt::print(stderr, "SDL_Init Failed: {}\n", SDL_GetError());
        return EResult::Fail;
    }

    m_SDLInitialized = true;

#pragma region EditorSettingsServiceInitialization
    if (!EditorSettingsService::Create())
        return EResult::Fail;

    auto& settings = EditorSettingsService::Get();

    if (!settings.LoadForStartup(createInfo.width, createInfo.height))
        return EResult::Fail;

    const auto& workspace = settings.GetWorkspace();
#pragma endregion

    SDL_WindowFlags windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS |
        SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;

    if (workspace.WindowFullscreen)
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    else if (workspace.WindowMaximized)
        windowFlags |= SDL_WINDOW_MAXIMIZED;

    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "1");

    m_Window = SDL_CreateWindow(
        createInfo.AppName, workspace.WindowWidth, workspace.WindowHeight, windowFlags);

    if (!m_Window)
    {
        BAM_LOG(Error, "Editor", "Application", "Window creation failed: {}", SDL_GetError());
        return EResult::Fail;
    }

    SDL_SetWindowMinimumSize(m_Window, 320, 240);

    if (m_Window)
    {
        SDL_SetWindowHitTest(m_Window, [](SDL_Window* win, const SDL_Point* area, void* data) -> SDL_HitTestResult {
            int winWidth, winHeight;
            SDL_GetWindowSize(win, &winWidth, &winHeight);

            // [핵심 디테일 1] 창이 최대화(Maximized) 상태일 때는 크기 조절을 막아야 합니다.
            bool isMaximized = (SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) != 0;

            // 크기 조절을 인식할 테두리 두께 (일반적으로 6~8픽셀이 적당합니다)
            const int resizeBorder = 6;

            // --- 1. 크기 조절 영역 판정 (최대화 상태가 아닐 때만) ---
            if (!isMaximized)
            {
                // 1-1. 모서리 4방향 (우선순위가 가장 높아야 함)
                if (area->x < resizeBorder && area->y < resizeBorder) return SDL_HITTEST_RESIZE_TOPLEFT;
                if (area->x > winWidth - resizeBorder && area->y < resizeBorder) return SDL_HITTEST_RESIZE_TOPRIGHT;
                if (area->x < resizeBorder && area->y > winHeight - resizeBorder) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
                if (area->x > winWidth - resizeBorder && area->y > winHeight - resizeBorder) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

                // 1-2. 상/하/좌/우 테두리
                if (area->x < resizeBorder) return SDL_HITTEST_RESIZE_LEFT;
                if (area->x > winWidth - resizeBorder) return SDL_HITTEST_RESIZE_RIGHT;
                if (area->y > winHeight - resizeBorder) return SDL_HITTEST_RESIZE_BOTTOM;
                if (area->y < resizeBorder) return SDL_HITTEST_RESIZE_TOP; // 상단 테두리
            }

            // --- 2. 타이틀 바 (드래그) 판정 ---
            if (ImGui::GetCurrentContext() && ImGuiManager::IsCreated() &&
                ImGuiManager::Get().IsMainWindowDragArea(*area))
            {
                // [개선] 상단 영역 위에 다른 ImGui 패널 창(플로팅 창/팝업 등)이 올라와 있다면,
                // 패널 조작이 최우선이므로 메인 윈도우 드래그를 양보합니다.
                if (ImGui::GetCurrentContext())
                {
                    ImGuiContext& g = *ImGui::GetCurrentContext();
                    if (g.HoveredWindow != nullptr)
                    {
                        const std::string_view winName = g.HoveredWindow->Name;
                        // 메인 메뉴바(##MainMenuBar)나 최상위 도킹 뷰포트 배경이 아닌 패널 창이 마우스 아래에 있다면 조작 양보
                        const bool isMainWindowChrome = (winName == "##MainMenuBar" || winName.starts_with("DockSpaceViewport_"));
                        if (!isMainWindowChrome)
                        {
                            return SDL_HITTEST_NORMAL; // ImGui 패널에 입력 전달
                        }
                    }
                }
                // ToolBar measures the actual menu/button boundaries each frame, including locale and scale changes.
                return SDL_HITTEST_DRAGGABLE;
            }
            return SDL_HITTEST_NORMAL;
            }, nullptr);

        const auto iconPath = Engine::PlatformPath::ResolveUri("Icon.bmp").u8string();
        SDL_Surface* iconSurface = SDL_LoadBMP(reinterpret_cast<const char*>(iconPath.c_str()));

        if (iconSurface)
        {
            SDL_SetWindowIcon(m_Window, iconSurface);
            SDL_DestroySurface(iconSurface);
        }
    }

    if (!SDL_SyncWindow(m_Window))
    {
        BAM_LOG(Error, "Editor", "Window", "Initial window synchronization failed: {}", SDL_GetError());
        return EResult::Fail;
    }

    return EResult::Success;
}
EResult Application::InitializeRuntime(const ApplicationCreateInfo& createInfo)
{
    if (!m_Window) return EResult::InvalidArgument;

    RUNTIMEDESC runtimeDesc = {};

	if (GRAPHICS_BACKEND == EGraphicsBackend::DirectX12)
		runtimeDesc.RendererDesc.rhiType = ERHIType::DirectX12;
	else if (GRAPHICS_BACKEND == EGraphicsBackend::Vulkan)
		runtimeDesc.RendererDesc.rhiType = ERHIType::SDLGPU;
	else
		return EResult::NotImplemented;
    SDLGPURHIDesc sdlgpuDesc = {};
    DirectX12RHIDesc directX12Desc = {};

    runtimeDesc.RendererDesc.rhiType = RHI_TYPE;

    // EditorSettingsService가 파악한 프로젝트 루트와 설정을 주입
    runtimeDesc.ProjectRoot = EditorSettingsService::Get().GetProjectRoot();
    runtimeDesc.AssetRoot = EditorSettingsService::Get().GetProjectSettings().AssetDirectory;
    runtimeDesc.EngineResourceRoot = EditorSettingsService::Get().GetProjectSettings().EngineResourceDirectory;

    switch (runtimeDesc.RendererDesc.rhiType)
    {
    case ERHIType::SDLGPU:
    {
        sdlgpuDesc.backendType = GRAPHICS_BACKEND;
        sdlgpuDesc.windowHandle = m_Window;
        runtimeDesc.RendererDesc.rhiDesc = &sdlgpuDesc;
        break;
    }
    case ERHIType::DirectX12:
    {
        SDL_PropertiesID propertiesID = SDL_GetWindowProperties(m_Window);
        if (!propertiesID)
            return EResult::Fail;

        HWND hwnd = static_cast<HWND>(
            SDL_GetPointerProperty(propertiesID, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        if (!hwnd)
            return EResult::Fail;

        directX12Desc.windowHandle = hwnd;
        runtimeDesc.RendererDesc.rhiDesc = &directX12Desc;
        break;
    }
    default:
        return EResult::NotImplemented;
    }

    int pixelWidth = 0;
    int pixelHeight = 0;
    if (!SDL_GetWindowSizeInPixels(m_Window, &pixelWidth, &pixelHeight) || pixelWidth <= 0 || pixelHeight <= 0)
        return EResult::Fail;

    runtimeDesc.RendererDesc.rhiDesc->width = static_cast<uint32>(pixelWidth);
    runtimeDesc.RendererDesc.rhiDesc->height = static_cast<uint32>(pixelHeight);
    runtimeDesc.RendererDesc.rhiDesc->isVSync = true;

    m_Runtime = Runtime::Create(&runtimeDesc);
    if (!m_Runtime)
        return EResult::Fail;

    auto* rhi = Renderer::Get().GetRHI();
    if (!rhi)
        return EResult::Fail;

    EditorSettingsService::Get().RestoreStartupVSync(*rhi);

    if(!AssetManager::Create())
        return EResult::Fail;

    return EResult::Success;
}
EResult Application::InitializeSystem()
{
    auto& systemManager = SystemManager::Get();

    const auto addSystems = [&systemManager]<typename... SystemTypes>()
    {
        return (... && static_cast<bool>(systemManager.AddSystem<SystemTypes>()));
    };

    const bool success = addSystems.operator() <
        TransformSystem, MeshRenderSystem, SpriteRenderSystem, SpriteAnimationSystem, TilemapRenderSystem, AnimationSystem,
        ThirdPersonControllerSystem,
        ParticleSystem, ActionTimelineSystem, CompositeVFXSystem, NetworkSystem, KinematicCharacterSystem,
        CameraSystem, SpringArmSystem, LightSystem, UIRenderSystem,
        AudioSystem, TerrainSystem, PhysicsSystem, GraphSystem, SplineFollowSystem > ();

    return success ? EResult::Success : EResult::Fail;
}
EResult Application::IntializeRenderer()
{
    auto& rtMgr = RenderTargetManager::Get();
    auto* rhi = Renderer::Get().GetRHI();
    uint32 w = rhi->GetSwapChainWidth();
    uint32 h = rhi->GetSwapChainHeight();
    // ── 에디터 최종 출력용 RT (ImGui가 여기에 렌더) ──
    RenderTargetDesc finalDesc = {};
    finalDesc.textureDesc.format = ETextureFormat::R8G8B8A8_UNORM;
    finalDesc.name = L"FinalColor";
    finalDesc.textureDesc.width = w;
    finalDesc.textureDesc.height = h;
    finalDesc.textureDesc.usage = ETextureUsage::RenderTarget | ETextureUsage::Sampler;
    rtMgr.CreateRenderTarget(&finalDesc);

    return EResult::Success;
}
EResult Application::InitializeImGui()
{
    ImGuiManagerDesc imguiDesc = {};
    imguiDesc.Window = m_Window;
    imguiDesc.RHI = Renderer::Get().GetRHI();

    m_ImGuiManager = ImGuiManager::Create(&imguiDesc);
    if (!m_ImGuiManager)
    {
        fmt::print(stderr, "ImGuiManager Creation Failed\n");
        return EResult::Fail;
    }

    m_SelectionManager = SelectionManager::Create();
    m_PIEManager = PIEManager::Create();
    if (!m_SelectionManager)
    {
        fmt::print(stderr, "SelectionManager Creation Failed\n");
        return EResult::Fail;
    }

    const auto editResult = m_EditContext.Initialize(*m_SelectionManager);
    if (editResult != EResult::Success)
        return editResult;

    RenderPassID uiPassID = RenderPassManager::Get().RegisterRenderPass(
        L"Editor UI",
        { L"FinalColor" },
        L"",
        ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
        ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
        vec4(0.0f, 0.0f, 0.0f, -1.0f),
        1000,
        ERenderSortType::None);
    DelegateHandle uiHandle = Renderer::Get().GetRenderPassDelegate(uiPassID).AddLambda([](f32 dt) {
        ImGuiManager::Get().Begin();
        ImGuiManager::Get().Draw();
        ImGuiManager::Get().End();
        });

    return EResult::Success;
}
EResult Application::InitializeLogger()
{
	if (!Engine::Logger::Initialize(LOG_PATH))
	{
		fmt::print(stderr, "Logger Initialization Failed\n");
		return EResult::Fail;
	}
    BAM_LOG(Info, "Editor", "Application", "Logger initialization finished");
    return EResult::Success;
}
EResult Application::InitializeLocalization()
{
	LocalizationManager* localizationManager = LocalizationManager::Create();

    // 1. Engine 코어 로캘 카탈로그 등록
    const auto engEngine = Engine::PlatformPath::ResolveUri(L"Engine://Data/Localization/English.json");
    if (IsFailure(localizationManager->RegisterLocalizationData(engEngine.wstring())))
    {
        BAM_LOG(Error, "Editor", "Application", "Failed to register Engine English localization data: {}", engEngine.string());
        return EResult::Fail;
    }
    const auto korEngine = Engine::PlatformPath::ResolveUri(L"Engine://Data/Localization/Korean.json");
    if (IsFailure(localizationManager->RegisterLocalizationData(korEngine.wstring())))
    {
        BAM_LOG(Error, "Editor", "Application", "Failed to register Engine Korean localization data: {}", korEngine.string());
        return EResult::Fail;
    }

    // 2. Editor 전용 로캘 카탈로그 등록
    const auto engEditor = Engine::PlatformPath::ResolveUri(L"Editor://Data/Localization/English.json");
    if (std::filesystem::exists(engEditor))
    {
        if (IsFailure(localizationManager->RegisterLocalizationData(engEditor.wstring())))
        {
            BAM_LOG(Error, "Editor", "Application", "Failed to register Editor English localization data: {}", engEditor.string());
            return EResult::Fail;
        }
    }
    const auto korEditor = Engine::PlatformPath::ResolveUri(L"Editor://Data/Localization/Korean.json");
    if (std::filesystem::exists(korEditor))
    {
        if (IsFailure(localizationManager->RegisterLocalizationData(korEditor.wstring())))
        {
            BAM_LOG(Error, "Editor", "Application", "Failed to register Editor Korean localization data: {}", korEditor.string());
            return EResult::Fail;
        }
    }

    if(IsFailure(localizationManager->LoadData()))
    {
        BAM_LOG(Error, "Editor", "Application", "Failed to load localization data");
        return EResult::Fail;
    }
    return EResult::Success;
}
#pragma endregion

#pragma region Resource
bool Application::InitializeResources()
{
    if (!EditorSettingsService::Get().LoadProjectSettings(ReflectionRegistry::Get().GetRegistry()))
        return false;

    const auto& projectSettings = EditorSettingsService::Get().GetProjectSettings();
    const auto& projectRoot = EditorSettingsService::Get().GetProjectRoot();
    if (!projectSettings.AssetDirectory.empty())
    {
        Engine::PlatformPath::SetProjectAssetRoot(
            std::filesystem::path(projectSettings.AssetDirectory).is_absolute()
                ? projectSettings.AssetDirectory
                : projectRoot / projectSettings.AssetDirectory);
    }
    if (!projectSettings.EngineResourceDirectory.empty())
    {
        Engine::PlatformPath::SetEngineResourceRoot(
            std::filesystem::path(projectSettings.EngineResourceDirectory).is_absolute()
                ? projectSettings.EngineResourceDirectory
                : projectRoot / projectSettings.EngineResourceDirectory);
    }

    InitializeMeshes();

    const auto manifestPath =
        Engine::PlatformPath::ResolveUri(EditorSettingsService::Get().GetProjectSettings().StartupManifestPath);

    return StartupResourceLoader::LoadPipeline(manifestPath);
}
void Application::InitializeMeshes()
{
    auto& resourceManager = Engine::ResourceManager::Get();
    Engine::RegisterBuiltInPrimitiveMeshes(resourceManager);

    Mesh* gridMesh = TerrainUtility::CreateGridMesh(128.0f, 128.0f, 128);
    resourceManager.AddResource<Mesh>(L"TerrainGridMesh", gridMesh);

    if (Mesh* strip = TerrainUtility::CreateGridMesh(0.25f, 4.0f, 1, 128, /*reverseWinding=*/true))
    {
        resourceManager.AddResource<Mesh>(L"SplineTestStrip", strip);
    }
}
#pragma endregion

#pragma region Loop
void Application::Run(int argc, char* argv[])
{
    bool bIsRunning = true;
    m_QuitApproved = false;
    SDL_Event event;
    auto& timeManager = Engine::TimeManager::Get();

    bool wasPointerOverGameImage = false;
    while (bIsRunning)
    {

        auto& settings = EditorSettingsService::Get();
        settings.ProcessPendingVSyncChange();

        if (settings.IsWindowChangePending())
        {
            settings.ProcessPendingWindowChange(CaptureApplicationState(), [this](const EditorWindowRequest& request)
                {
                    return SubmitWindowRequest(request);
                });
        }

        while (SDL_PollEvent(&event)) {
            ImGuiManager::Get().ProcessEvent(&event);
            InputManager::Get().ProcessEvent(event);
            if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_MAXIMIZED ||
                event.type == SDL_EVENT_WINDOW_RESTORED || event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ||
                event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN)
            {
                if (event.window.windowID == SDL_GetWindowID(m_Window))
                    CaptureEditorWindowState();
            }
            if (event.type == SDL_EVENT_TEXT_EDITING || event.type == SDL_EVENT_TEXT_EDITING_CANDIDATES)
            {
                int a = 10;

            }
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                    event.window.windowID == SDL_GetWindowID(m_Window)))
            {
                ImGuiManager::Get().RequestQuit();
            }
            else if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                if (event.window.windowID != SDL_GetWindowID(m_Window))
                    continue;

                int pixelWidth = 0;
                int pixelHeight = 0;
                if (!SDL_GetWindowSizeInPixels(m_Window, &pixelWidth, &pixelHeight))
                    continue;

                if (pixelWidth <= 0 || pixelHeight <= 0)
                    continue;

                RHI* rhi = Renderer::Get().GetRHI();
                const EResult result = rhi->Resize(static_cast<uint32>(pixelWidth), static_cast<uint32>(pixelHeight));

                if (result != EResult::Success)
                {
                    EDITOR_LOG_ERROR("Window back buffer resize failed.");
                    bIsRunning = false;
                    break;
                }
            }
        }

        if (!bIsRunning)
            break;

        timeManager.Update();

        if (settings.IsWindowChangePending())
        {
            int pixelWidth = 0;
            int pixelHeight = 0;

            const bool hasPixelSize = SDL_GetWindowSizeInPixels(m_Window, &pixelWidth, &pixelHeight);
            auto* rhi = Renderer::Get().GetRHI();

            const bool outputReady = hasPixelSize && pixelWidth > 0 && pixelHeight > 0 && rhi &&
                rhi->GetSwapChainWidth() == static_cast<uint32>(pixelWidth) &&
                rhi->GetSwapChainHeight() == static_cast<uint32>(pixelHeight);

            settings.ObserveWindowChange(CaptureApplicationState(), outputReady);
        }

        f32 dt = timeManager.GetDeltaTime();
        UpdateTitle(dt);
        InputManager::Get().Update(dt);
        ImGuiManager::Get().Update(dt);
        AssetManager::Get().Update(dt);
        HotReloadManager::Get().Update();
        NotificationManager::Get().Update(dt);
        if (RecoveryManager::IsCreated())
            RecoveryManager::Get().Update(dt);

        auto& input = InputManager::Get();
        auto& imgui = ImGuiManager::Get();
        auto* gameView = dynamic_cast<GameViewportPanel*>(imgui.GetImGuiPanel(L"Game View"));
        const auto leftClickPosition = input.GetMouseButtonDownScreenPosition(EMouseButton::Left);
        const bool leftClickOutsideGame = input.IsMouseButtonDown(EMouseButton::Left) &&
            (!gameView || !leftClickPosition || !gameView->HitTestGameplayImage(*leftClickPosition));

        const bool allowGameplay = PIEManager::Get().IsPlaying() &&
            gameView && gameView->IsOpen() && gameView->IsFocused() &&
            input.HasKeyboardFocus() && !leftClickOutsideGame &&
            !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
            !imgui.GetDocumentController().HasPendingAction() &&
            !settings.IsWindowChangePending() && !ShortcutManager::Get().IsCapturing() &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

        const uint32 mouseButtons = gameView ? gameView->UpdateGameplayMouseButtons(input, allowGameplay) : 0;
        f32 gameScrollDeltaY = 0.0f;
        if (allowGameplay)
        {
            for (const auto& sample : input.GetMouseWheelSamples())
            {
                if (gameView->HitTestGameplayImage(sample.ScreenPosition))
                    gameScrollDeltaY += sample.DeltaY;
            }
        }

        const bool pointerOverGameImage =
            allowGameplay && gameView->HitTestGameplayImage(input.GetScreenMousePosition());

        Engine::InputActionMapMouseInput mouseInput;
        mouseInput.AllowedButtons = mouseButtons;
        mouseInput.ScrollDeltaY = gameScrollDeltaY;
        if (pointerOverGameImage && wasPointerOverGameImage)
            mouseInput.MotionDelta = input.GetMouseDelta();

        wasPointerOverGameImage = pointerOverGameImage;
        input.EvaluateActionMap("Gameplay", allowGameplay, mouseInput);

        if (PIEManager::Get().IsPlaying())
        {
            constexpr int maxSubSteps = 2;
            int subStepsRan = 0;
            while (timeManager.IsFixedUpdateReady() && subStepsRan < maxSubSteps)
            {
                m_Runtime->FixedUpdate(timeManager.GetFixedStep());
                subStepsRan++;
            }
            if (subStepsRan >= maxSubSteps)
            {
                while (timeManager.IsFixedUpdateReady()) {}
            }

            m_Runtime->Update(dt, false);
            m_Runtime->LateUpdate(dt);
        }
        else
        {
            while (timeManager.IsFixedUpdateReady())
            {
            }

            // [하이브리드 프리뷰] 뷰포트의 Realtime 토글이 켜져 있을 때만 시각 효과(Presentation) 시스템을 선별 틱
            bool shouldSimulate = false;
            if (auto* viewport = SceneViewportPanel::GetActiveSceneViewport())
            {
                shouldSimulate = viewport->IsRealtime();
            }

            if (shouldSimulate)
            {
                m_Runtime->Update(dt, false, static_cast<uint8>(Engine::ESystemPhase::Presentation));
            }

            m_Runtime->LateUpdate(dt);
        }
        m_Runtime->Render(dt);

        if (m_QuitApproved)
            bIsRunning = false;
    }
}
void Application::UpdateTitle(f32 dt)
{
    static uint32 lastFPS = 0;

    uint32 currentFPS = Engine::TimeManager::Get().GetFPS();

    if (lastFPS != currentFPS)
    {
        lastFPS = currentFPS;
        std::string title = fmt::format("BamEngine Editor - FPS: {}", currentFPS);
        SDL_SetWindowTitle(m_Window, title.c_str());
    }
}
#pragma endregion

#pragma region Settings
EditorApplicationState Application::CaptureApplicationState() const
{
    EditorApplicationState state;

    if (TimeManager::IsCreated())
    {
        state.HasTargetFPS = true;
        state.TargetFPS = TimeManager::Get().GetTargetFPS();
    }

    if (!m_Window)
        return state;

    state.HasWindow = true;

    const auto flags = SDL_GetWindowFlags(m_Window);
    state.WindowMinimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
    state.WindowMaximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    state.WindowFullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;

    int width = 0;
    int height = 0;

    if (SDL_GetWindowSize(m_Window, &width, &height))
    {
        state.HasWindowSize = true;
        state.WindowWidth = width;
        state.WindowHeight = height;
    }

    return state;
}

void Application::CaptureAndSaveEditorWorkspace()
{
    EditorSettingsService::Get().CaptureWindowState(CaptureApplicationState());

    if (m_ImGuiManager)
        EditorSettingsService::Get().CaptureWorkspace(*m_ImGuiManager);

    EditorSettingsService::Get().SaveWorkspace();
}

void Application::RestoreLastEditedScene()
{
    const auto& workspace = EditorSettingsService::Get().GetWorkspace();
    const auto& project = EditorSettingsService::Get().GetProjectSettings();
    const auto& storedPath = workspace.LastScenePath.empty()
        ? project.StartupScenePath : workspace.LastScenePath;

    if (storedPath.empty())
        return;

    try
    {
        auto path = Engine::PlatformPath::ResolveUri(std::filesystem::u8path(storedPath));

        path = path.lexically_normal();

        archive::JsonArchive reader;
        if (SceneManager::Get().LoadScene(reader, path.wstring()) != EResult::Success)
        {
            BAM_LOG(Warn, "Editor", "Workspace", "Cannot restore scene: {}", storedPath);
            return;
        }

        AdoptSceneDocument(path);
    }
    catch (const std::filesystem::filesystem_error& error)
    {
        BAM_LOG(Warn, "Editor", "Workspace", "Invalid startup scene path: {}", error.what());
    }
}

EResult Application::SubmitWindowRequest(const EditorWindowRequest& request)
{
    if (!m_Window)
        return EResult::AccessDenied;

    if (!EditorWorkspace::HasValidWindowSize(request.Width, request.Height))
        return EResult::InvalidArgument;

    const auto fail = [](const char* operation)
        {
            BAM_LOG(Error, "Editor", "Window", "{} failed: {}", operation, SDL_GetError());
            return EResult::Fail;
        };

    if (!SDL_SetWindowFullscreen(m_Window, false))
        return fail("SDL_SetWindowFullscreen");

    if (!SDL_SyncWindow(m_Window))
        return fail("SDL_SyncWindow");

    if (SDL_GetWindowFlags(m_Window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED))
    {
        if (!SDL_RestoreWindow(m_Window))
            return fail("SDL_RestoreWindow");

        if (!SDL_SyncWindow(m_Window))
            return fail("SDL_SyncWindow");
    }

    if (!SDL_SetWindowSize(m_Window, request.Width, request.Height))
        return fail("SDL_SetWindowSize");

    if (!SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED))
        return fail("SDL_SetWindowPosition");

    if (!SDL_SyncWindow(m_Window))
        return fail("SDL_SyncWindow");

    int width = 0;
    int height = 0;

    if (!SDL_GetWindowSize(m_Window, &width, &height))
        return fail("SDL_GetWindowSize");

    if (width != request.Width || height != request.Height)
    {
        BAM_LOG(Error, "Editor", "Window", "Requested windowed size was not accepted");
        return EResult::Fail;
    }

    if (request.Maximized)
    {
        if (!SDL_MaximizeWindow(m_Window))
            return fail("SDL_MaximizeWindow");

        if (!SDL_SyncWindow(m_Window))
            return fail("SDL_SyncWindow");
    }

    if (request.Fullscreen)
    {
        if (!SDL_SetWindowFullscreen(m_Window, true))
            return fail("SDL_SetWindowFullscreen");

        if (!SDL_SyncWindow(m_Window))
            return fail("SDL_SyncWindow");
    }

    return EResult::Success;
}

void Application::CaptureEditorWindowState()
{
    EditorSettingsService::Get().CaptureWindowState(CaptureApplicationState());
}

void Application::RememberEditedScene(const std::filesystem::path& path)
{
    if (PIEManager::IsCreated() && !PIEManager::Get().IsEditing())
        return;

    EditorSettingsService::Get().RememberEditedScene(path);
}
#pragma endregion

#pragma region Document
SceneDocument* Application::GetSceneDocument()
{
    return dynamic_cast<SceneDocument*>(m_Documents.Find(m_SceneDocumentId));
}

void Application::AdoptSceneDocument(const std::filesystem::path& path)
{
    if (!PIEManager::Get().IsEditing())
        return;

    auto* scene = SceneManager::Get().GetCurrentScene();
    auto nextId = InvalidEditorDocumentId;

    if (scene)
    {
        auto document = std::make_unique<SceneDocument>(SceneManager::Get(), GetEditHistory(), path);
        document->SetReadOnly(false);
        nextId = m_Documents.Add(std::move(document));

        if (nextId == InvalidEditorDocumentId)
            throw std::runtime_error("Scene document registration failed");
    }

    m_Documents.Remove(m_SceneDocumentId);
    m_SceneDocumentId = nextId;
    GetEditHistory().ResetDocument(scene, true);

    if (nextId != InvalidEditorDocumentId)
        m_Documents.SetActive(nextId);

    RememberEditedScene(path);
}

void Application::RebindSceneDocumentAfterPlay()
{
    if (auto* document = GetSceneDocument())
    {
        const auto result = document->RebindRestoredScene();
        document->SetReadOnly(result != EResult::Success);

        if (result != EResult::Success)
            BAM_LOG(Error, "Editor", "Document", "Cannot rebind restored scene document");

        return;
    }

    GetEditHistory().Synchronize(SceneManager::Get().GetCurrentScene(), true);
}
#pragma endregion