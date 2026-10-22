#pragma once
#include <fmt/core.h>
#include <SDL3/SDL.h>
#include "Application.h"
#include "Runtime.h"
#include "ImGuiManager.h"
#include "SelectionManager.h"
#include "CoreComponents.h"
#include "AssetManager.h"
#include <archive/JsonArchive.h>
#include "Systems.h"
#include "PIEManager.h"
#include "ModuleHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "AssetHotReloadManager.h"
#include "TerrainUtility.h"
#include "TerrainComponent.h" 
#include "StartupResourceLoader.h"
#include "ShortcutManager.h"

BEGIN(Editor)

IMPLEMENT_SINGLETON(Application)

#pragma region Constructor&Destructor
EResult Application::Initialize(void* arg)
{
    if (!arg)
        return EResult::InvalidArgument;

    const auto& createInfo = *static_cast<const ApplicationCreateInfo*>(arg);

    EResult result = InitializeLogger();
    if (result != EResult::Success)
        return result;

    m_LoggerInitialized = true;

    const auto fail = [this](const char* stage, EResult result)
        {
            BAM_LOG(Error, "Editor", "Application", "{} failed: {}", stage, ResultToString(result));
            Free();
            return result;
        };

    result = InitializeWindow(createInfo);
    if (result != EResult::Success)
        return fail("InitializeWindow", result);

    result = InitializeRuntime(createInfo);
    if (result != EResult::Success)
        return fail("InitializeRuntime", result);

    ShortcutManager::Create();
    IntializeRenderer();
    InitializeResources();

    result = InitializeImGui();
    if (result != EResult::Success)
        return fail("InitializeImGui", result);

    InitializeLocalization();

    result = InitializeSystem();
    if (result != EResult::Success)
        return fail("InitializeSystem", result);

    ModuleHotReloadManager::Create();
    ShaderHotReloadManager::Create();
    AssetHotReloadManager::Create();
    // Editor.exe 실행 파일과 같은 디렉터리에 있는 GameModule.dll 로드
    wchar_t exePathBuf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    const std::filesystem::path exeDir = std::filesystem::path(exePathBuf).parent_path();
    const std::filesystem::path modulePath = exeDir / L"GameModule.dll";
    if (std::filesystem::exists(modulePath))
    {
        ModuleHotReloadManager::Get().LoadModule(modulePath);
    }
    else
    {
        BAM_LOG(Warn, "Editor", "HotReload", "GameModule.dll not found at: {}", modulePath.string());
    }
    ShortcutManager::Get().LoadFromFile();
    return EResult::Success;
}

void Application::Free()
{
    m_EditHistory.BeforeReplay = {};
    m_EditHistory.AfterReplay = {};
    m_HistorySelection.clear();
    m_EditHistory.Commands().OnHistoryChanged().Clear();
    m_EditHistory.Clear();

    if (m_AssetManager)
    {
        AssetManager::Destroy();
        m_AssetManager = nullptr;
    }

    if (m_ImGuiManager)
    {
        ImGuiManager::Destroy();
        m_ImGuiManager = nullptr;
    }

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

    AssetHotReloadManager::Destroy();
    ShaderHotReloadManager::Destroy();
    ModuleHotReloadManager::Destroy();
    ShortcutManager::Get().SaveToFile();
    ShortcutManager::Destroy();

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

    if (m_SDLInitialized)
    {
        SDL_Quit();
        m_SDLInitialized = false;
    }

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

    uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;

    // SDL3에서 OS 기본 IME(한글 조합 등) 창을 띄우기 위한 힌트 설정
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "1");

    m_Window = SDL_CreateWindow(
        "BamEngine Editor", g_WindowWidth, g_WindowHeight,
        windowFlags
    );

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
    return EResult::Success;
}

EResult Application::InitializeRuntime(const ApplicationCreateInfo& createInfo)
{
    if (!m_Window)
        return EResult::InvalidArgument;

    RUNTIMEDESC runtimeDesc = {};
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

    runtimeDesc.RendererDesc.rhiDesc->width = g_WindowWidth;
    runtimeDesc.RendererDesc.rhiDesc->height = g_WindowHeight;
    runtimeDesc.RendererDesc.rhiDesc->isVSync = true;

    m_Runtime = Runtime::Create(&runtimeDesc);
    if (!m_Runtime)
        return EResult::Fail;

    m_AssetManager = AssetManager::Create();
    if (!m_AssetManager)
        return EResult::Fail;

    return EResult::Success;
}

EResult Application::InitializeSystem()
{
	auto& systemManager = SystemManager::Get();

	if(!systemManager.AddSystem<TransformSystem>())
		return EResult::Fail;
	if(!systemManager.AddSystem<MeshRenderSystem>())
		return EResult::Fail;
    if(!systemManager.AddSystem<SpriteRenderSystem>())
		return EResult::Fail;
    if (!systemManager.AddSystem<CameraSystem>())
        return EResult::Fail;
    if (!systemManager.AddSystem<LightSystem>())
        return EResult::Fail;
    if (!systemManager.AddSystem<UIRenderSystem>())
        return EResult::Fail;
    if (!systemManager.AddSystem<AudioSystem>())
        return EResult::Fail;
    if (!systemManager.AddSystem<TerrainSystem>())
        return EResult::Fail;
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
    m_EditHistory.BeforeReplay = [this]
    {
        m_HistorySelection.clear();
        for (auto* entity : m_SelectionManager->GetSelectedEntities())
        {
            if (!entity || !entity->GetScene()) continue;
            auto* scene = entity->GetScene();
            const auto* id = scene->GetRegistry().try_get<Engine::IDComponent>(entity->GetEntityHandle());
            if (id) m_HistorySelection.emplace_back(scene, id->id);
        }
        m_SelectionManager->ClearSelection(true);
    };
    m_EditHistory.AfterReplay = [this]
        {
            const bool locked = m_SelectionManager->IsPrimarySelectionLocked();
            m_SelectionManager->SetPrimarySelectionLock(false);
            for (const auto& [scene, id] : m_HistorySelection)
                if (auto* entity = scene->FindEntityById(id)) m_SelectionManager->AddToEntitySelection(entity);
            m_SelectionManager->SetPrimarySelectionLock(locked);
            m_HistorySelection.clear();

            // Undo / Redo 후 복원된 터레인의 GPU 높이맵 동기화 보장
            for (auto* scene : SceneManager::Get().GetActiveScenes())
            {
                if (!scene) continue;
                for (auto [entity, terrain] : scene->GetRegistry().view<Engine::TerrainComponent>().each())
                    terrain.IsDirty = true;
            }
        };
    if (!m_SelectionManager)
    {
        fmt::print(stderr, "SelectionManager Creation Failed\n");
        return EResult::Fail;
    }

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
#pragma endregion

#pragma region Localization
void Application::InitializeLocalization()
{
    LocalizationManager::Get().RegisterLocalizationData(L"Resources/Data/Localization/English.json");
    LocalizationManager::Get().RegisterLocalizationData(L"Resources/Data/Localization/Korean.json");
    LocalizationManager::Get().LoadData();
}
#pragma endregion

#pragma region Resources
void Application::InitializeResources()
{
    InitializeMeshes();
    StartupResourceLoader::LoadPipeline(STARTUP_MANIFEST_PATH);
}

void Application::InitializeFiles()
{
    ResourceManager& resourceManager = ResourceManager::Get();
    wstring resourcesToLoad[] =
    {
        //Texture
        RESOURCE_PATH L"Texture/uv1.png",
        RESOURCE_PATH L"Texture/white1x1.png",
        RESOURCE_PATH L"Texture/black1x1.png",
        RESOURCE_PATH L"Texture/magenta1x1.png",
        RESOURCE_PATH L"Texture/green1x1.png",
        RESOURCE_PATH L"Texture/blue1x1.png",

        ///Sprite
        RESOURCE_PATH L"Texture/uv1.bamsprite.json",
    };

    for (const auto& path : resourcesToLoad)
    {
        resourceManager.LoadFile(path);
    }
}

void Application::InitializeShaders()
{
    ResourceManager& rm = ResourceManager::Get();

	ShaderDesc defaultVsDesc = {};
    {
	    defaultVsDesc.Key   = RESOURCE_PATH L"Shader/DefaultVS";
        defaultVsDesc.Path  = RESOURCE_PATH L"Shader/bin/dxil/defaultVS.cso";
        defaultVsDesc.shaderType = EShaderType::Vertex;
        defaultVsDesc.entryPoint = "main";
	    auto handle = rm.LoadResource<Shader>(&defaultVsDesc);
	    //rm.SaveToBinaryFile(handle.Get(), RESOURCE_PATH L"Shader/default_vs.bamshader");
	}

    ShaderDesc defaultPsDesc = {};
    {
        defaultPsDesc.Key   = RESOURCE_PATH L"Shader/DefaultPS";
        defaultPsDesc.Path  = RESOURCE_PATH L"Shader/bin/dxil/defaultPS.cso";
        defaultPsDesc.shaderType = EShaderType::Fragment;
        defaultPsDesc.entryPoint = "main";
	    auto handle = rm.LoadResource<Shader>(&defaultPsDesc);
	    //rm.SaveToBinaryFile(handle.Get(), RESOURCE_PATH L"Shader/default_ps.bamshader");
	}

    ShaderDesc gbufferVsDesc = {};
    {
        gbufferVsDesc.Key = RESOURCE_PATH L"Shader/GBufferVS";
        gbufferVsDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/gbufferVS.cso";
        gbufferVsDesc.shaderType = EShaderType::Vertex;
        gbufferVsDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&gbufferVsDesc);
    }

    ShaderDesc gbufferPsDesc = {};
    {
        gbufferPsDesc.Key = RESOURCE_PATH L"Shader/GBufferPS";
        gbufferPsDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/gbufferPS.cso";
        gbufferPsDesc.shaderType = EShaderType::Fragment;
        gbufferPsDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&gbufferPsDesc);
    }
    ShaderDesc previewPsDesc = {};
    {
        previewPsDesc.Key = RESOURCE_PATH L"Shader/MaterialPreviewPS";
        previewPsDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/materialPreviewPS.cso";
        if (Renderer::Get().GetRHIType() == ERHIType::SDLGPU)
            previewPsDesc.Path = RESOURCE_PATH L"Shader/bin/spirv/materialPreviewPS.spv";
        previewPsDesc.shaderType = EShaderType::Fragment;
        previewPsDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&previewPsDesc);
    }

    //ShaderDesc defaultSkinningShaderDesc = {};
    //{
    //    defaultSkinningShaderDesc.Key = L"Resources/Shader/Skinning";
    //    defaultSkinningShaderDesc.Path = L"Resources/Shader/skinning.vert.spv";
    //    defaultSkinningShaderDesc.spirvPath = L"Resources/Shader/skinning.vert.spv";
    //    defaultSkinningShaderDesc.shaderType = EShaderType::Vertex;
    //    defaultSkinningShaderDesc.numStorageBuffers = 1;
    //    auto handle = rm.LoadResource<Shader>(&defaultSkinningShaderDesc);
    //}
    
    //rm.SaveToBinaryFile(skinningShader, L"Resources/Shader/skinning.vert.bamshader");
    //rm.LoadFile(L"Resources/Shader/skinning.vert.bamshader");

    //ShaderDesc defaultVsDesc = {};
    //defaultVsDesc.Key = L"Resources/Shader/GBufferVS";
    //defaultVsDesc.Path = L"Resources/Shader/gbuffer.vert.spv";
    //defaultVsDesc.spirvPath = L"Resources/Shader/gbuffer.vert.spv";
    //defaultVsDesc.shaderType = EShaderType::Vertex;
    //rm.LoadResource<Shader>(&defaultVsDesc);
    //{
    //    auto handle = rm.GetResourceHandle<Shader>(defaultVsDesc.Key);
    //    rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/gbuffer.vert.bamshader");
    //}
    //rm.LoadFile(L"Resources/Shader/gbuffer.vert.bamshader");

    //ShaderDesc gbufferVsDesc = {};
    //gbufferVsDesc.Key = L"Resources/Shader/GBufferVS";
    //gbufferVsDesc.Path = L"Resources/Shader/gbuffer.vert.spv";
    //gbufferVsDesc.spirvPath = L"Resources/Shader/gbuffer.vert.spv";
    //gbufferVsDesc.shaderType = EShaderType::Vertex;
    //rm.LoadResource<Shader>(&gbufferVsDesc);
    //{
    //    auto handle = rm.GetResourceHandle<Shader>(gbufferVsDesc.Key);
    //    rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/gbuffer.vert.bamshader");
    //}
    //rm.LoadFile(L"Resources/Shader/gbuffer.vert.bamshader");
    //ShaderDesc gbufferPsDesc = {};
    //gbufferPsDesc.Key = L"Resources/Shader/GBufferPS";
    //gbufferPsDesc.Path = L"Resources/Shader/gbuffer.frag.spv";
    //gbufferPsDesc.spirvPath = L"Resources/Shader/gbuffer.frag.spv";
    //gbufferPsDesc.shaderType = EShaderType::Pixel;
    //rm.LoadResource<Shader>(&gbufferPsDesc);
    //{
    //    auto handle = rm.GetResourceHandle<Shader>(gbufferPsDesc.Key);
    //    rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/gbuffer.frag.bamshader");
    //}
    //rm.LoadFile(L"Resources/Shader/gbuffer.frag.bamshader");

    // Fullscreen Quad VS
    ShaderDesc fsQuadVsDesc = {};
    {
        fsQuadVsDesc.Key        = L"FullscreenQuadVS";
        fsQuadVsDesc.Path       = RESOURCE_PATH L"Shader/bin/dxil/fullscreenQuadVS.cso";
        //fsQuadVsDesc.spirvPath  = RESOURCE_PATH L"Shader/bin/spirv/fullscreenQuadVS.spv";
        fsQuadVsDesc.shaderType = EShaderType::Vertex;
        fsQuadVsDesc.entryPoint = "main";
        auto handle = rm.LoadResource<Shader>(&fsQuadVsDesc);
        rm.SaveToJsonFile(handle.Get(), L"Resources/Shader/fullscreen_quad.vert.bamshader.json");
    }
    
    // Lighting PS
    ShaderDesc lightingPsDesc = {};
    {
        lightingPsDesc.Key          = L"LightingPS";
        lightingPsDesc.Path         = RESOURCE_PATH L"Shader/bin/dxil/lightingPS.cso";
        //lightingPsDesc.spirvPath = RESOURCE_PATH L"Shader/bin/spirv/lightingPS.spv";

        lightingPsDesc.shaderType = EShaderType::Fragment;
        lightingPsDesc.entryPoint = "main";
        lightingPsDesc.numSamplers = 6;
        lightingPsDesc.numStorageBuffers = 1;
        lightingPsDesc.numUniformBuffers = 2;
        auto handle = rm.LoadResource<Shader>(&lightingPsDesc);;
        //rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/lighting.frag.bamshader");
        //rm.LoadFile(L"Resources/Shader/lighting.frag.bamshader");
    }
    

    // Shadow Depth VS (static mesh)
    ShaderDesc shadowDepthVsDesc = {};
    {
        shadowDepthVsDesc.Key = RESOURCE_PATH L"Shader/ShadowDepthVS";
        shadowDepthVsDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/shadowDepthVS.cso";
        shadowDepthVsDesc.shaderType = EShaderType::Vertex;
        shadowDepthVsDesc.entryPoint = "main";
        shadowDepthVsDesc.numUniformBuffers = 2;
        rm.LoadResource<Shader>(&shadowDepthVsDesc);
    }

    // Shadow Depth VS (skinning)
    //ShaderDesc shadowDepthSkinVsDesc = {};
    //{
    //    shadowDepthSkinVsDesc.Key = L"ShadowDepthSkinningVS";
    //    shadowDepthSkinVsDesc.shaderType = EShaderType::Vertex;
    //    shadowDepthSkinVsDesc.Path = L"Resources/Shader/shadow_depth_skinning.vert.spv";
    //    shadowDepthSkinVsDesc.spirvPath = L"Resources/Shader/shadow_depth_skinning.vert.spv";
    //    shadowDepthSkinVsDesc.entryPoint = "main";
    //    shadowDepthSkinVsDesc.numStorageBuffers = 1;
    //    rm.LoadResource<Shader>(&shadowDepthSkinVsDesc);
    //    {
    //        auto handle = rm.GetResourceHandle<Shader>(shadowDepthSkinVsDesc.Key);
    //        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/shadow_depth_skinning.vert.bamshader");
    //    }
    //    rm.LoadFile(L"Resources/Shader/shadow_depth_skinning.vert.bamshader");
    //}

    // Shadow Depth PS (depth-only)
    ShaderDesc shadowDepthPsDesc = {};
    {
        shadowDepthPsDesc.Key = RESOURCE_PATH L"Shader/ShadowDepthPS";
        shadowDepthPsDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/shadowDepthPS.cso";
        shadowDepthPsDesc.shaderType = EShaderType::Fragment;
        shadowDepthPsDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&shadowDepthPsDesc);
    }
    // Viewport Channel PS
    ShaderDesc viewportChannelPsDesc = {};
    {
        viewportChannelPsDesc.Key           = L"ViewportChannelPS";
        viewportChannelPsDesc.Path          = RESOURCE_PATH L"Shader/bin/dxil/viewport_channelPS.cso";
        //viewportChannelPsDesc.spirvPath   = RESOURCE_PATH L"Shader/bin/spirv/viewport_channelPS.spv";
        viewportChannelPsDesc.shaderType = EShaderType::Fragment;
        viewportChannelPsDesc.entryPoint = "main";
        viewportChannelPsDesc.numSamplers = 1;
        viewportChannelPsDesc.numUniformBuffers = 1;
        auto handle = rm.LoadResource<Shader>(&viewportChannelPsDesc);
        //rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/viewport_channel.frag.bamshader");
        //rm.LoadFile(L"Resources/Shader/viewport_channel.frag.bamshader");
    }
    
    //// PostProcess PS
    //ShaderDesc postProcessPsDesc = {};
    //postProcessPsDesc.Key = L"PostProcessPS";
    //postProcessPsDesc.shaderType = EShaderType::Pixel;
    //postProcessPsDesc.Path = L"Resources/Shader/postprocess.frag.spv";
    //postProcessPsDesc.spirvPath = L"Resources/Shader/postprocess.frag.spv";
    //postProcessPsDesc.entryPoint = "main";
    //postProcessPsDesc.numSamplers = 1;
    //postProcessPsDesc.numUniformBuffers = 1;
    //rm.LoadResource<Shader>(&postProcessPsDesc);
    //{
    //    auto handle = rm.GetResourceHandle<Shader>(postProcessPsDesc.Key);
    //    rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/postprocess.frag.bamshader");
    //}
    //rm.LoadFile(L"Resources/Shader/postprocess.frag.bamshader");

    // PostProcess - Tone Mapping PS
    ShaderDesc ppToneMappingPsDesc = {};
    {
        ppToneMappingPsDesc.Key         = L"PostProcess_ToneMappingPS";
        ppToneMappingPsDesc.Path        = RESOURCE_PATH L"Shader/bin/dxil/postprocess_tonemappingPS.cso";
        //ppToneMappingPsDesc.spirvPath   = RESOURCE_PATH L"Shader/bin/spirv/postprocess_tonemappingPS.spv";

        ppToneMappingPsDesc.shaderType = EShaderType::Fragment;

        ppToneMappingPsDesc.entryPoint = "main";
        ppToneMappingPsDesc.numSamplers = 1;
        ppToneMappingPsDesc.numUniformBuffers = 1;
        auto handle = rm.LoadResource<Shader>(&ppToneMappingPsDesc);
        //rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/postprocess_tonemapping.frag.bamshader");
        //rm.LoadFile(L"Resources/Shader/postprocess_tonemapping.frag.bamshader");
    }


    // ── Sky Shaders ──
    ShaderDesc skyVsDesc = {};
    {
        skyVsDesc.Key = RESOURCE_PATH L"Shader/SkyVS";
        skyVsDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/skyVS.cso";
        skyVsDesc.shaderType = EShaderType::Vertex;
        skyVsDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&skyVsDesc);
    }
    
    ShaderDesc skyPsDesc = {};
    {
        skyPsDesc.Key = RESOURCE_PATH L"Shader/SkyPS";
        skyPsDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/atmosphereSkyPS.cso";
        skyPsDesc.shaderType = EShaderType::Fragment;
        skyPsDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&skyPsDesc);
    }

    //// UI VS
    //ShaderDesc uiVSDesc = {};
    //uiVSDesc.Key = L"UIVS";
    //uiVSDesc.shaderType = EShaderType::Vertex;
    //uiVSDesc.Path = L"Resources/Shader/ui.vert.spv";
    //uiVSDesc.spirvPath = L"Resources/Shader/ui.vert.spv";
    //uiVSDesc.entryPoint = "main";
    //uiVSDesc.numUniformBuffers = 1;
    //rm.LoadResource<Shader>(&uiVSDesc);
    //{
    //    auto handle = rm.GetResourceHandle<Shader>(uiVSDesc.Key);
    //    rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/ui.vert.bamshader");
    //}
    //rm.LoadFile(L"Resources/Shader/ui.vert.bamshader");

    //// UI PS
    //ShaderDesc uiPSDesc = {};
    //uiPSDesc.Key = L"UIPS";
    //uiPSDesc.shaderType = EShaderType::Pixel;
    //uiPSDesc.Path = L"Resources/Shader/ui.frag.spv";
    //uiPSDesc.spirvPath = L"Resources/Shader/ui.frag.spv";
    //uiPSDesc.entryPoint = "main";
    //uiPSDesc.numUniformBuffers = 1;
    //rm.LoadResource<Shader>(&uiPSDesc);
    //{
    //    auto handle = rm.GetResourceHandle<Shader>(uiPSDesc.Key);
    //    rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/ui.frag.bamshader");
    //}
    //rm.LoadFile(L"Resources/Shader/ui.frag.bamshader");

    for (const auto& stage : { std::pair{ L"UIVS", EShaderType::Vertex }, std::pair{ L"UIPS", EShaderType::Fragment } })
    {
        ShaderDesc uiShader{};
        uiShader.Key = RESOURCE_PATH L"Shader/" + std::wstring(stage.first);
        uiShader.Path = stage.second == EShaderType::Vertex ? RESOURCE_PATH L"Shader/bin/dxil/uiVS.cso" :
            RESOURCE_PATH L"Shader/bin/dxil/uiPS.cso";
        if (Renderer::Get().GetRHIType() == ERHIType::SDLGPU)
            uiShader.Path = stage.second == EShaderType::Vertex ? RESOURCE_PATH L"Shader/bin/spirv/uiVS.spv" :
                RESOURCE_PATH L"Shader/bin/spirv/uiPS.spv";
        uiShader.shaderType = stage.second;
        uiShader.entryPoint = "main";
        rm.LoadResource<Shader>(&uiShader);
    }
    ShaderDesc spriteVS{};
    {
        spriteVS.Key = RESOURCE_PATH L"Shader/GBufferSpriteVS";
        spriteVS.Path = RESOURCE_PATH L"Shader/bin/dxil/gbufferSpriteVS.cso";
        spriteVS.shaderType = EShaderType::Vertex;
        spriteVS.entryPoint = "main";
        rm.LoadResource<Shader>(&spriteVS);
    }

    ShaderDesc spritePS{};
    {
        spritePS.Key = RESOURCE_PATH L"Shader/GBufferSpritePS";
        spritePS.Path = RESOURCE_PATH L"Shader/bin/dxil/gbufferSpritePS.cso";
        spritePS.shaderType = EShaderType::Fragment;
        spritePS.entryPoint = "main";
        rm.LoadResource<Shader>(&spritePS);
    }

    ShaderDesc terrainVSDesc = {};
    {
        terrainVSDesc.Key = RESOURCE_PATH L"Shader/TerrainVS";
        terrainVSDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/terrainVS.cso";
        if (Renderer::Get().GetRHIType() == ERHIType::SDLGPU)
            terrainVSDesc.Path = RESOURCE_PATH L"Shader/bin/spirv/terrainVS.spv";
        terrainVSDesc.shaderType = EShaderType::Vertex;
        terrainVSDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&terrainVSDesc);
    }

    ShaderDesc terrainPSDesc = {};
    {
        terrainPSDesc.Key = RESOURCE_PATH L"Shader/TerrainPS";
        terrainPSDesc.Path = RESOURCE_PATH L"Shader/bin/dxil/terrainPS.cso";
        if (Renderer::Get().GetRHIType() == ERHIType::SDLGPU)
            terrainPSDesc.Path = RESOURCE_PATH L"Shader/bin/spirv/terrainPS.spv";
        terrainPSDesc.shaderType = EShaderType::Fragment;
        terrainPSDesc.entryPoint = "main";
        rm.LoadResource<Shader>(&terrainPSDesc);
    }
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

void Application::InitializeMaterials()
{
    ResourceManager& resourceManager = ResourceManager::Get();
#pragma region Basic Materials

    MaterialDesc defaultMaterialDesc = {};
    defaultMaterialDesc.Key = L"Resources/Material/DefaultMaterial";
    defaultMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/DefaultVS");
    defaultMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/DefaultPS");
    Material* material = resourceManager.LoadResource<Material>(&defaultMaterialDesc).Get();
    material->SetTextureBinding("Default", 0, resourceManager.GetResourceHandle<Texture>(RESOURCE_PATH L"Texture/magenta1x1.png"));
    resourceManager.SaveToJsonFile(material, RESOURCE_PATH L"Material/DefaultMaterial.bammat.json");
    
    MaterialDesc spriteMaterialDesc = {};
    spriteMaterialDesc.Key = RESOURCE_PATH L"Material/SpriteMaterial";
    spriteMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/GBufferSpriteVS");
    spriteMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/GBufferSpritePS");
    // The GBuffer sprite path uses alpha cutout; blending would also corrupt its picking ID.
    spriteMaterialDesc.BlendMode = EBlendMode::Masked;
    spriteMaterialDesc.CullMode = ECullMode::None;
    spriteMaterialDesc.DepthMode = EDepthMode::ReadWrite;
    Material* spriteMaterial = resourceManager.LoadResource<Material>(&spriteMaterialDesc).Get();
    resourceManager.SaveToJsonFile(spriteMaterial, RESOURCE_PATH L"Material/SpriteMaterial.bammat.json");

    MaterialInstanceDesc spriteMaterialInstanceDesc = {};
    spriteMaterialInstanceDesc.BaseMaterialHandle = resourceManager.GetResourceHandle<Material>(RESOURCE_PATH L"Material/SpriteMaterial");
    spriteMaterialInstanceDesc.Key = L"Resources/Material/SpriteMaterialInstance";
    ResourceHandle<MaterialInstance> spriteMaterialInstanceHandle = resourceManager.LoadResource<MaterialInstance>(&spriteMaterialInstanceDesc);
    MaterialInstance* spriteMaterialInstance = spriteMaterialInstanceHandle.Get();
    resourceManager.SaveToJsonFile(spriteMaterialInstance, L"Resources/Material/SpriteMaterialInstance.bammatinst.json");

    MaterialDesc skinningMaterialDesc = {};
    skinningMaterialDesc.Key = L"Resources/Material/SkinningMaterial";
    skinningMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/skinning.vert.bamshader");
    skinningMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/default.frag.bamshader");
    skinningMaterialDesc.BlendMode = EBlendMode::AlphaBlend;
    skinningMaterialDesc.CullMode = ECullMode::None;
    skinningMaterialDesc.DepthMode = EDepthMode::ReadWrite;
    resourceManager.LoadResource<Material>(&skinningMaterialDesc);

    MaterialInstanceDesc skinningMaterialInstanceDesc = {};
    skinningMaterialInstanceDesc.BaseMaterialHandle = resourceManager.GetResourceHandle<Material>(L"Resources/Material/SkinningMaterial");
    skinningMaterialInstanceDesc.Key = L"Resources/Material/SkinningMaterialInstance";
    resourceManager.LoadResource<MaterialInstance>(&skinningMaterialInstanceDesc);
#pragma endregion

    // ── G-Buffer Material 생성 ──
    MaterialDesc gbufferMatDesc = {};
    gbufferMatDesc.Key = RESOURCE_PATH L"Material/GBufferMaterial";
    gbufferMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/GBufferVS");
    gbufferMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/GBufferPS");
    gbufferMatDesc.BlendMode = EBlendMode::Opaque;
    gbufferMatDesc.CullMode = ECullMode::Back;
    gbufferMatDesc.DepthMode = EDepthMode::ReadWrite;
    Material* gbufferMat = resourceManager.LoadResource<Material>(&gbufferMatDesc).Get();
    gbufferMat->SetTextureBinding("Diffuse", 0, resourceManager.GetResourceHandle<Texture>(RESOURCE_PATH L"Texture/white1x1.png"));
    gbufferMat->SetPassShader(
        ERenderPassType::Shadow,
        resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthVS"),
        resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthPS")
    );
    resourceManager.SaveToJsonFile(gbufferMat, L"Resources/Material/GBufferMaterial.bammat.json");

    // ── Terrain Material 생성 ──
    MaterialDesc terrainMatDesc = {};
    terrainMatDesc.Key = RESOURCE_PATH L"Material/TerrainMaterial";
    terrainMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/TerrainVS");
    terrainMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/TerrainPS"); // <-- TerrainPS로 교체
    terrainMatDesc.BlendMode = EBlendMode::Opaque;
    terrainMatDesc.CullMode = ECullMode::None;
    terrainMatDesc.DepthMode = EDepthMode::ReadWrite;
    Material* terrainMat = resourceManager.LoadResource<Material>(&terrainMatDesc).Get();
    terrainMat->SetTextureBinding("Diffuse", 0, resourceManager.GetResourceHandle<Texture>(RESOURCE_PATH L"Texture/white1x1.png"));
    terrainMat->SetTextureBinding("RockDiffuse", 2, resourceManager.GetResourceHandle<Texture>(RESOURCE_PATH L"Texture/white1x1.png"));
    terrainMat->SetTextureBinding("DirtDiffuse", 3, resourceManager.GetResourceHandle<Texture>(RESOURCE_PATH L"Texture/white1x1.png"));
    terrainMat->SetPassShader(
        ERenderPassType::Shadow,
        resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthVS"),
        resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthPS")
    );
    resourceManager.SaveToJsonFile(terrainMat, L"Resources/Material/TerrainMaterial.bammat.json");


	// ── Shadow Depth Material 생성 ──
    MaterialDesc shadowDepthMatDesc = {};
    shadowDepthMatDesc.Key = RESOURCE_PATH L"Material/ShadowDepthMaterial";
    shadowDepthMatDesc.VertexShaderHandle =
        resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthVS");
    shadowDepthMatDesc.PixelShaderHandle =
        resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/ShadowDepthPS");
    shadowDepthMatDesc.BlendMode = EBlendMode::Opaque;
    shadowDepthMatDesc.CullMode = ECullMode::Front;
    shadowDepthMatDesc.DepthMode = EDepthMode::ReadWrite;
    resourceManager.LoadResource<Material>(&shadowDepthMatDesc);

	MaterialDesc shadowDepthSkinMatDesc = {};
    shadowDepthSkinMatDesc.Key = L"Resources/Material/ShadowDepthSkinningMaterial";
    shadowDepthSkinMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/shadow_depth_skinning.vert.bamshader");
    shadowDepthSkinMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/shadow_depth.frag.bamshader");
    shadowDepthSkinMatDesc.BlendMode = EBlendMode::Opaque;
    shadowDepthSkinMatDesc.CullMode = ECullMode::Front;
    shadowDepthSkinMatDesc.DepthMode = EDepthMode::ReadWrite;
    resourceManager.LoadResource<Material>(&shadowDepthSkinMatDesc);


	// ── Transparent Material 생성 ──
	MaterialDesc transparentMatDesc = {};
    transparentMatDesc.Key = L"Resources/Material/TransparentMaterial";
    transparentMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        RESOURCE_PATH L"Shader/DefaultVS");
    transparentMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        RESOURCE_PATH L"Shader/DefaultPS");
    transparentMatDesc.BlendMode = EBlendMode::AlphaBlend;
    transparentMatDesc.CullMode = ECullMode::None;
    transparentMatDesc.DepthMode = EDepthMode::ReadOnly;
	Material* transparentMat = resourceManager.LoadResource<Material>(&transparentMatDesc).Get();
	resourceManager.SaveToJsonFile(transparentMat, L"Resources/Material/TransparentMaterial.bammat.json");


    // ── Sky Material 생성 ──
    MaterialDesc skyMatDesc = {};
    skyMatDesc.Key = RESOURCE_PATH L"Material/SkyMaterial";
    skyMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/SkyVS");
    skyMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(RESOURCE_PATH L"Shader/SkyPS");
    skyMatDesc.BlendMode = EBlendMode::Forward; // ForwardPass에서만 렌더링되도록 보장
    skyMatDesc.CullMode = ECullMode::None;
    skyMatDesc.DepthMode = EDepthMode::ReadOnly;
    skyMatDesc.DepthCompareOp = ECompareOp::LessOrEqual;
    Material* skyMat = resourceManager.LoadResource<Material>(&skyMatDesc).Get();
    resourceManager.SaveToJsonFile(skyMat, RESOURCE_PATH L"Material/SkyMaterial.bammat.json");

    // ── UI Material 생성 ──
    MaterialDesc uiMatDesc = {};
    uiMatDesc.Key = L"Resources/Material/UIMaterial";
    uiMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/UIVS");
    uiMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/UIPS");
    uiMatDesc.BlendMode = EBlendMode::NonPremultiplied;
    uiMatDesc.CullMode = ECullMode::None;
    uiMatDesc.DepthMode = EDepthMode::None;
    uiMatDesc.DepthCompareOp = ECompareOp::LessOrEqual;
    Material* uiMat = resourceManager.LoadResource<Material>(&uiMatDesc).Get();
    resourceManager.SaveToJsonFile(uiMat, L"Resources/Material/UIMaterial.bammat.json");
}

#pragma endregion

#pragma region Rendering
void Application::IntializeRenderer()
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

}
void Application::SubmitRenderPasses()
{
    //Renderer::Get().SubmitCustomCommand([](f32 dt, RenderPass* pass) -> EResult
    //    {
    //        auto* rhi = Renderer::Get().GetRHI();
    //        auto& rtMgr = RenderTargetManager::Get();
    //        RHISampler* sampler = SamplerManager::Get().GetDefaultSampler();
    //        rhi->BindTextureSampler(
    //            rtMgr.GetRenderTarget(L"GBuffer_Diffuse")->GetTexture(),
    //            sampler, 0);
    //        rhi->BindTextureSampler(
    //            rtMgr.GetRenderTarget(L"GBuffer_Normal")->GetTexture(),
    //            sampler, 1);
    //        rhi->BindTextureSampler(
    //            rtMgr.GetRenderTarget(L"GBuffer_PBR")->GetTexture(),
    //            sampler, 2);
    //        rhi->BindTextureSampler(
    //            rtMgr.GetRenderTarget(L"GBuffer_Emission")->GetTexture(),
    //            sampler, 3);
    //        rhi->BindTextureSampler(
    //            rtMgr.GetRenderTarget(L"GBuffer_Position")->GetTexture(),
    //            sampler, 4);
    //        return EResult::Success;
    //    }, m_LightingPassID);
}
#pragma endregion

void Application::Run(int argc, char* argv[])
{
    bool bIsRunning = true;
    SDL_Event event;
    auto& timeManager = Engine::TimeManager::Get();

    while (bIsRunning)
    {
        while (SDL_PollEvent(&event)) {
            ImGuiManager::Get().ProcessEvent(&event);
			InputManager::Get().ProcessEvent(event);
            if (event.type == SDL_EVENT_TEXT_EDITING || event.type == SDL_EVENT_TEXT_EDITING_CANDIDATES)
            {
                int a = 10;
                
            }
            if (event.type == SDL_EVENT_QUIT) {
                bIsRunning = false;
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
		f32 dt = timeManager.GetDeltaTime();
		UpdateTitle(dt);
        InputManager::Get().Update(dt);
        ImGuiManager::Get().Update(dt);
        m_AssetManager->Update(dt);
        ModuleHotReloadManager::Get().Update();
        ShaderHotReloadManager::Get().Update();
        AssetHotReloadManager::Get().Update();

        // 기존: if (m_PlayState == EPlayState::Play)
        if (PIEManager::Get().IsPlaying())
        {
            m_Runtime->FixedUpdate(dt);
            m_Runtime->Update(dt, false);
            m_Runtime->LateUpdate(dt);
        }
        else
        {
            m_Runtime->LateUpdate(dt);
        }
		m_Runtime->Render(dt);
    }
}

void Application::Shutdown()
{
    Free();
}

void Application::SetResolution(uint32 width, uint32 height, bool fullscreen)
{
    if (!m_Window) return;

    if(fullscreen)
    {
		SDL_SetWindowFullscreen(m_Window, true);
    }
    else
    {
        SDL_SetWindowFullscreen(m_Window, false);
        SDL_SetWindowSize(m_Window, width, height);
        SDL_SetWindowPosition(m_Window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
}

void Application::GetWindowSize(int32* w, int32* h) const
{
    if (m_Window)
        SDL_GetWindowSize(m_Window, w, h);
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
END
