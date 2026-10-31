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
#include "Components/Core/IDComponent.h"
#include "TerrainUtility.h"

#include "SceneDocument.h"
#include "EditorSettingsService.h"
#include "PrefabModeManager.h"

IMPLEMENT_SINGLETON(Application)

#pragma region Constructor&Destructor
EResult Application::Initialize(void* arg)
{
    if (!arg) return EResult::InvalidArgument;

    CAST_DESC;

    if(!InitializeLogger())
        return EResult::Fail; 

    m_LoggerInitialized = true;

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

    shortcutDesc.LegacyPath = editorService.GetProjectRoot() / "Resources" / "Config" / "EditorShortcuts.json";

    if (!ShortcutManager::Create(&shortcutDesc))
        return fail("ShortcutManager", EResult::Fail);
    if(!IntializeRenderer())
		return fail("IntializeRenderer", EResult::Fail);
    if (!InitializeResources())
        return fail("InitializeResources", EResult::Fail);

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

    m_InitializationCompleted = true;
    return EResult::Success;
}
void Application::Free()
{
    if (m_InitializationCompleted)
    {
        m_InitializationCompleted = false;

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
            const int titleBarHeight = 36; // ImGui 메뉴바 높이 + 여백
            const int leftMenuAreaWidth = 350;  // File, Edit, Scene 메뉴 등을 클릭할 공간
            const int rightButtonAreaWidth = 150; // 우측 제어 버튼(-ㅁX)을 클릭할 공간

            if (area->y < titleBarHeight)
            {
                // 메뉴와 우측 버튼 사이의 빈 공간만 드래그 가능하게 설정
                if (area->x > leftMenuAreaWidth && area->x < (winWidth - rightButtonAreaWidth))
                {
                    return SDL_HITTEST_DRAGGABLE;
                }

                return SDL_HITTEST_NORMAL; // 메뉴나 버튼 위에 마우스가 있음
            }

            // 위 영역들에 해당하지 않으면 일반적인 클라이언트 영역(게임 화면, 에디터 UI)
            return SDL_HITTEST_NORMAL;
            }, nullptr);

        SDL_Surface* iconSurface = SDL_LoadBMP("Resources/icon.bmp");

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
        TransformSystem, MeshRenderSystem, SpriteRenderSystem, SpriteAnimationSystem,
        CameraSystem, LightSystem, UIRenderSystem,
        AudioSystem, TerrainSystem, PhysicsSystem, GraphSystem > ();

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
    if(!localizationManager)
    {
        fmt::print(stderr, "LocalizationManager Creation Failed\n");
        return EResult::Fail;
    }
    if(IsFailure(localizationManager->RegisterLocalizationData(L"Resources/Data/Localization/English.json")))
    {
		BAM_LOG(Error, "Editor", "Application", "Failed to register English localization data");
        return EResult::Fail;
    }
    if(IsFailure(localizationManager->RegisterLocalizationData(L"Resources/Data/Localization/Korean.json")))
    {
        BAM_LOG(Error, "Editor", "Application", "Failed to register Korean localization data");
        return EResult::Fail;
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

    InitializeMeshes();

    const auto manifestPath =
        EditorSettingsService::Get().GetProjectRoot() / std::filesystem::path(EditorSettingsService::Get().GetProjectSettings().StartupManifestPath);

    return StartupResourceLoader::LoadPipeline(manifestPath);
}
void Application::InitializeMeshes()
{
    ResourceManager& resourceManager = ResourceManager::Get();
#pragma region Quad
    {
        vector<VertexPosition> positions = {
            { { -1.0f,  1.0f, 0.0f } },
            { {  1.0f,  1.0f, 0.0f } },
            { {  1.0f, -1.0f, 0.0f } },
            { { -1.0f, -1.0f, 0.0f } }
        };
        vector<VertexMaterial> materials = {
            { { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
            { { 0, 0, -1 }, { 1, 0 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
            { { 0, 0, -1 }, { 1, 1 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) },
            { { 0, 0, -1 }, { 0, 1 }, { 0, 0, 0 }, { 0, 1, 0 }, vec4(1.0f) }
        };
        vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

        MeshCreateDesc meshDesc = {};
        meshDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
        meshDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };
        meshDesc.IndexData = indices.data();
        meshDesc.IndexCount = static_cast<uint32>(indices.size());
        meshDesc.IndexStride = sizeof(uint32);

        resourceManager.AddResource<Mesh>(L"QuadMesh", Mesh::Create(&meshDesc));
    }
#pragma endregion

#pragma region Cube
    {
        // 1. 기존 통짜 Vertex 배열은 그대로 유지 (데이터 소스)
        vector<Vertex> vertices = {
            // ---- 기존 24개 정점 데이터를 그대로 두세요 (변경 없음) ----
            // 앞면, 뒷면, 윗면, 아랫면, 우측면, 좌측면
            {{ -1.0f,  1.0f,  1.0f }, { 0, 0, 1 }, { 0, 0 }, { 1, 0, 0 }},
            {{  1.0f,  1.0f,  1.0f }, { 0, 0, 1 }, { 1, 0 }, { 1, 0, 0 }},
            {{  1.0f, -1.0f,  1.0f }, { 0, 0, 1 }, { 1, 1 }, { 1, 0, 0 }},
            {{ -1.0f, -1.0f,  1.0f }, { 0, 0, 1 }, { 0, 1 }, { 1, 0, 0 }},
            {{  1.0f,  1.0f, -1.0f }, { 0, 0,-1 }, { 0, 0 }, {-1, 0, 0 }},
            {{ -1.0f,  1.0f, -1.0f }, { 0, 0,-1 }, { 1, 0 }, {-1, 0, 0 }},
            {{ -1.0f, -1.0f, -1.0f }, { 0, 0,-1 }, { 1, 1 }, {-1, 0, 0 }},
            {{  1.0f, -1.0f, -1.0f }, { 0, 0,-1 }, { 0, 1 }, {-1, 0, 0 }},
            {{ -1.0f,  1.0f, -1.0f }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 }},
            {{  1.0f,  1.0f, -1.0f }, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0 }},
            {{  1.0f,  1.0f,  1.0f }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 }},
            {{ -1.0f,  1.0f,  1.0f }, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0 }},
            {{ -1.0f, -1.0f,  1.0f }, { 0,-1, 0 }, { 0, 0 }, { 1, 0, 0 }},
            {{  1.0f, -1.0f,  1.0f }, { 0,-1, 0 }, { 1, 0 }, { 1, 0, 0 }},
            {{  1.0f, -1.0f, -1.0f }, { 0,-1, 0 }, { 1, 1 }, { 1, 0, 0 }},
            {{ -1.0f, -1.0f, -1.0f }, { 0,-1, 0 }, { 0, 1 }, { 1, 0, 0 }},
            {{  1.0f,  1.0f,  1.0f }, { 1, 0, 0 }, { 0, 0 }, { 0, 0,-1 }},
            {{  1.0f,  1.0f, -1.0f }, { 1, 0, 0 }, { 1, 0 }, { 0, 0,-1 }},
            {{  1.0f, -1.0f, -1.0f }, { 1, 0, 0 }, { 1, 1 }, { 0, 0,-1 }},
            {{  1.0f, -1.0f,  1.0f }, { 1, 0, 0 }, { 0, 1 }, { 0, 0,-1 }},
            {{ -1.0f,  1.0f, -1.0f }, {-1, 0, 0 }, { 0, 0 }, { 0, 0, 1 }},
            {{ -1.0f,  1.0f,  1.0f }, {-1, 0, 0 }, { 1, 0 }, { 0, 0, 1 }},
            {{ -1.0f, -1.0f,  1.0f }, {-1, 0, 0 }, { 1, 1 }, { 0, 0, 1 }},
            {{ -1.0f, -1.0f, -1.0f }, {-1, 0, 0 }, { 0, 1 }, { 0, 0, 1 }}
        };

        // 2. 루프를 돌며 Position과 Material로 분리 + 바운딩 박스 계산
        vector<VertexPosition> positions;
        vector<VertexMaterial> materials;
        vec3 minBound = vec3((numeric_limits<f32>::max)());
        vec3 maxBound = vec3(numeric_limits<f32>::lowest());

        positions.reserve(vertices.size());
        materials.reserve(vertices.size());

        for (const auto& v : vertices)
        {
            positions.push_back({ v.position });

            VertexMaterial mat;
            mat.normal = v.normal;
            mat.texCoord = v.texCoord;
            mat.tangent = v.tangent;
            mat.binormal = glm::cross(v.normal, v.tangent); // 자동 계산
            mat.color = vec4(1.0f);
            materials.push_back(mat);

            minBound = glm::min(minBound, v.position);
            maxBound = glm::max(maxBound, v.position);
        }

        // 3. 인덱스 생성 (기존과 동일)
        vector<uint32> indices;
        for (uint32 i = 0; i < 6; ++i)
        {
            uint32 s = i * 4;
            indices.push_back(s + 0); indices.push_back(s + 1); indices.push_back(s + 2);
            indices.push_back(s + 0); indices.push_back(s + 2); indices.push_back(s + 3);
        }

        // 4. 새로운 Streams 배열 기반 Desc 구성
        MeshCreateDesc meshDesc = {};
        meshDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), (uint32)positions.size(), sizeof(VertexPosition) };
        meshDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), (uint32)materials.size(), sizeof(VertexMaterial) };
        meshDesc.IndexData = indices.data();
        meshDesc.IndexCount = static_cast<uint32>(indices.size());
        meshDesc.IndexStride = sizeof(uint32);
        meshDesc.BoundingBoxMin = minBound;
        meshDesc.BoundingBoxMax = maxBound;

        resourceManager.AddResource<Mesh>(L"CubeMesh", Mesh::Create(&meshDesc));
    }
#pragma endregion
#pragma region Sphere
    {
        const uint32 slices = 24;
        const uint32 stacks = 16;
        const float radius = 1.0f;

        vector<VertexPosition> positions;
        vector<VertexMaterial> materials;
        vector<uint32> indices;

        for (uint32 i = 0; i <= stacks; ++i)
        {
            float phi = glm::pi<float>() * static_cast<float>(i) / static_cast<float>(stacks);
            float y = radius * std::cos(phi);
            float rSin = radius * std::sin(phi);

            for (uint32 j = 0; j <= slices; ++j)
            {
                float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / static_cast<float>(slices);
                float x = rSin * std::cos(theta);
                float z = rSin * std::sin(theta);

                vec3 pos(x, y, z);
                vec3 normal = glm::normalize(pos);
                vec2 uv(static_cast<float>(j) / static_cast<float>(slices), static_cast<float>(i) / static_cast<float>(stacks));
                vec3 tangent = glm::normalize(vec3(-std::sin(theta), 0.0f, std::cos(theta)));
                vec3 binormal = glm::cross(normal, tangent);

                positions.push_back({ pos });
                VertexMaterial mat{};
                mat.normal = normal;
                mat.texCoord = uv;
                mat.tangent = tangent;
                mat.binormal = binormal;
                mat.color = vec4(1.0f);
                materials.push_back(mat);
            }
        }

        for (uint32 i = 0; i < stacks; ++i)
        {
            for (uint32 j = 0; j < slices; ++j)
            {
                uint32 first = i * (slices + 1) + j;
                uint32 second = first + slices + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        MeshCreateDesc sphereDesc = {};
        sphereDesc.Streams[(uint32)EMeshStream::Position] = { positions.data(), static_cast<uint32>(positions.size()), sizeof(VertexPosition) };
        sphereDesc.Streams[(uint32)EMeshStream::Material] = { materials.data(), static_cast<uint32>(materials.size()), sizeof(VertexMaterial) };
        sphereDesc.IndexData = indices.data();
        sphereDesc.IndexCount = static_cast<uint32>(indices.size());
        sphereDesc.IndexStride = sizeof(uint32);
        sphereDesc.BoundingBoxMin = vec3(-radius);
        sphereDesc.BoundingBoxMax = vec3(radius);

        resourceManager.AddResource<Mesh>(L"SphereMesh", Mesh::Create(&sphereDesc));
    }
#pragma endregion

#pragma region Terrain
    Mesh* gridMesh = TerrainUtility::CreateGridMesh(128.0f, 128.0f, 128);
    ResourceManager::Get().AddResource<Mesh>(L"TerrainGridMesh", gridMesh);
#pragma endregion

}
#pragma endregion

#pragma region Loop
void Application::Run(int argc, char* argv[])
{
    bool bIsRunning = true;
    m_QuitApproved = false;
    SDL_Event event;
    auto& timeManager = Engine::TimeManager::Get();

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

        if (PIEManager::Get().IsPlaying())
        {
            while (timeManager.IsFixedUpdateReady())
                m_Runtime->FixedUpdate(timeManager.GetFixedStep());

            m_Runtime->Update(dt, false);
            m_Runtime->LateUpdate(dt);
        }
        else
        {
            while (timeManager.IsFixedUpdateReady())
            {
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
        auto path = std::filesystem::u8path(storedPath);
        if (path.is_relative())
            path = EditorSettingsService::Get().GetProjectRoot() / path;

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