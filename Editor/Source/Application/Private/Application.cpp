#pragma once
#include <fmt/core.h>
#include <SDL3/SDL.h>
#include "Application.h"
#include "Runtime.h"
#include "ImGuiManager.h"
#include "SelectionManager.h"
#include "AssetManager.h"


#define RHI_TYPE ERHIType::DirectX12
#define GRAPHICS_BACKEND EGraphicsBackend::Vulkan

BEGIN(Editor)

IMPLEMENT_SINGLETON(Application)

#pragma region Constructor&Destructor
EResult Application::Initialize(void* arg)
{
//    _CrtSetBreakAlloc(4243);
	InitializeWindow(*(ApplicationCreateInfo*)arg);
	InitializeRuntime(*(ApplicationCreateInfo*)arg);

	tagImGuiManagerDesc imguiDesc = {};
	imguiDesc.Window = m_Window;
    imguiDesc.RHI = Renderer::Get().GetRHI();

    IntializeRenderer();
    InitializeResources();

    m_ImGuiManager = ImGuiManager::Create(&imguiDesc);
    m_SelectionManager = SelectionManager::Create();
    if (!m_ImGuiManager)
    {
        fmt::print(stderr, "ImGuiManager Creation Failed\n");
		return EResult::Fail;
    }


    RenderPassID uiPassID = RenderPassManager::Get().RegisterRenderPass(
        L"Editor UI",
        {L"FinalColor"},
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
    InitializeLocalization();
    
    return EResult::Success;
}

void Application::Free()
{
	AssetManager::Destroy();
    ImGuiManager::Destroy();
	SelectionManager::Destroy();

    if (m_Runtime)
    {
        m_Runtime->Destroy();
        m_Runtime = nullptr;
    }

    if(m_Window) SDL_DestroyWindow(m_Window);
    SDL_Quit();
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
    RUNTIMEDESC runtimeDesc = {};
    runtimeDesc.RendererDesc.rhiType = RHI_TYPE;
    switch (runtimeDesc.RendererDesc.rhiType)
    {
    case ERHIType::SDLGPU:
    {
        SDLGPURHIDesc sdlgpuDesc = {};
        sdlgpuDesc.backendType = GRAPHICS_BACKEND; // 원하는 그래픽 백엔드 설정
		sdlgpuDesc.windowHandle = m_Window;
        runtimeDesc.RendererDesc.rhiDesc = &sdlgpuDesc;
        break;
    }
    //case ERHIType::Vulkan:
    //{
    //    VulkanRHIDesc vulkanDesc = {};
    //    runtimeDesc.RendererDesc.rhiDesc = &vulkanDesc;
    //    break;
    //}
    case ERHIType::DirectX12:
    {
        DirectX12RHIDesc directX12Desc = {};
        SDL_PropertiesID propertiesID = SDL_GetWindowProperties(m_Window);
        HWND hwnd = (HWND)SDL_GetPointerProperty(propertiesID, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
        directX12Desc.windowHandle = hwnd;
        runtimeDesc.RendererDesc.rhiDesc = &directX12Desc;
        break;
    }
    //case ERHIType::Metal:
    //{
    //    MetalRHIDesc metalDesc = {};
    //    runtimeDesc.RendererDesc.rhiDesc = &metalDesc;
    //    break;
    //}
    default:
        break;
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
	SystemManager::Get().AddSystem<TransformSystem>();
    return EResult();
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
    InitializeFiles();
    InitializeShaders();
    InitializeMeshes();
    InitializeMaterials();
}

void Application::InitializeFiles()
{
    ResourceManager& resourceManager = ResourceManager::Get();
    wstring resourcesToLoad[] =
    {
        //Texture
        L"Resources/Texture/uv1.png",
        L"Resources/Texture/white1x1.png",
        L"Resources/Texture/black1x1.png",
        L"Resources/Texture/magenta1x1.png",
        L"Resources/Texture/green1x1.png",
        L"Resources/Texture/blue1x1.png",
        L"Resources/Texture/uv1.bamtex",

        ///Sprite
        L"Resources/Texture/uv1.bamsprite",
        L"Resources/Texture/uv1.bamsprite.json",

        //Shader
        L"Resources/Shader/default.vert.bamshader",
        L"Resources/Shader/default.frag.bamshader",
        L"Resources/Shader/sprite.vert.bamshader",
        L"Resources/Shader/sprite.frag.bamshader",

        //Material
        L"Resources/Material/DefaultMaterial.bammat",
        L"Resources/Material/SpriteMaterial.bammat",

        //MaterialInstance
        L"Resources/Material/SpriteMaterial.bammatinst",

        //Mesh
        //L"Resources/Model/Cube2_Cube.bammesh",
    };

    for (const auto& path : resourcesToLoad)
    {
        resourceManager.LoadFile(path);
    }

    //resourceManager.SaveToJsonFile(resourceManager.GetResourceHandle<Sprite>(L"Resources/Texture/uv1.bamsprite").Get(), L"Resources/Texture/uv1.bamsprite.json");
}

void Application::InitializeShaders()
{
    ResourceManager& rm = ResourceManager::Get();

	//ShaderDesc defaultVsDesc = {};
	//defaultVsDesc.Key = L"Resources/Shader/DefaultVS";
 //   defaultVsDesc.Path = L"Resources/Shader/default_vs.cso";
 //   defaultVsDesc.shaderType = EShaderType::Vertex;
 //   defaultVsDesc.entryPoint = "VSMain";
	//rm.LoadResource<Shader>(&defaultVsDesc);
	//{
	//	auto handle = rm.GetResourceHandle<Shader>(defaultVsDesc.Key);
	//	rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/default_vs.bamshader");
	//}

 //   ShaderDesc defaultPsDesc = {};
 //   defaultPsDesc.Key = L"Resources/Shader/DefaultPS";
 //   defaultPsDesc.Path = L"Resources/Shader/default_ps.cso";
 //   defaultPsDesc.shaderType = EShaderType::Pixel;
 //   defaultPsDesc.entryPoint = "PSMain";
	//rm.LoadResource<Shader>(&defaultPsDesc);
	//{
	//	auto handle = rm.GetResourceHandle<Shader>(defaultPsDesc.Key);
	//	rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/default_ps.bamshader");
	//}

    ShaderDesc defaultVsDesc = {};
    defaultVsDesc.Key = L"Resources/Shader/GBufferVS";
    defaultVsDesc.Path = L"Resources/Shader/gbuffer.vert.spv";
    defaultVsDesc.spirvPath = L"Resources/Shader/gbuffer.vert.spv";
    defaultVsDesc.shaderType = EShaderType::Vertex;
    rm.LoadResource<Shader>(&defaultVsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(defaultVsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/gbuffer.vert.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/gbuffer.vert.bamshader");

    ShaderDesc gbufferVsDesc = {};
    gbufferVsDesc.Key = L"Resources/Shader/GBufferVS";
    gbufferVsDesc.Path = L"Resources/Shader/gbuffer.vert.spv";
    gbufferVsDesc.spirvPath = L"Resources/Shader/gbuffer.vert.spv";
    gbufferVsDesc.shaderType = EShaderType::Vertex;
    rm.LoadResource<Shader>(&gbufferVsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(gbufferVsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/gbuffer.vert.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/gbuffer.vert.bamshader");
    ShaderDesc gbufferPsDesc = {};
    gbufferPsDesc.Key = L"Resources/Shader/GBufferPS";
    gbufferPsDesc.Path = L"Resources/Shader/gbuffer.frag.spv";
    gbufferPsDesc.spirvPath = L"Resources/Shader/gbuffer.frag.spv";
    gbufferPsDesc.shaderType = EShaderType::Pixel;
    rm.LoadResource<Shader>(&gbufferPsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(gbufferPsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/gbuffer.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/gbuffer.frag.bamshader");

    // Fullscreen Quad VS
    ShaderDesc fsQuadVsDesc = {};
    fsQuadVsDesc.Key = L"FullscreenQuadVS";
    fsQuadVsDesc.shaderType = EShaderType::Vertex;
    fsQuadVsDesc.Path = L"Resources/Shader/fullscreen_quad.vert.spv";
    fsQuadVsDesc.spirvPath = L"Resources/Shader/fullscreen_quad.vert.spv";
    fsQuadVsDesc.entryPoint = "main";
    rm.LoadResource<Shader>(&fsQuadVsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(fsQuadVsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/fullscreen_quad.vert.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/fullscreen_quad.vert.bamshader");

    // Lighting PS
    ShaderDesc lightingPsDesc = {};
    lightingPsDesc.Key = L"LightingPS";
    lightingPsDesc.shaderType = EShaderType::Pixel;
    lightingPsDesc.Path = L"Resources/Shader/lighting.frag.spv";
    lightingPsDesc.spirvPath = L"Resources/Shader/lighting.frag.spv";
    lightingPsDesc.entryPoint = "main";
    lightingPsDesc.numSamplers = 6;
    lightingPsDesc.numStorageBuffers = 1;
    lightingPsDesc.numUniformBuffers = 2;
    rm.LoadResource<Shader>(&lightingPsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(lightingPsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/lighting.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/lighting.frag.bamshader");

    // Shadow Depth VS (static mesh)
    ShaderDesc shadowDepthVsDesc = {};
    shadowDepthVsDesc.Key = L"ShadowDepthVS";
    shadowDepthVsDesc.shaderType = EShaderType::Vertex;
    shadowDepthVsDesc.Path = L"Resources/Shader/shadow_depth.vert.spv";
    shadowDepthVsDesc.spirvPath = L"Resources/Shader/shadow_depth.vert.spv";
    shadowDepthVsDesc.entryPoint = "main";
    rm.LoadResource<Shader>(&shadowDepthVsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(shadowDepthVsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/shadow_depth.vert.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/shadow_depth.vert.bamshader");

    // Shadow Depth VS (skinning)
    ShaderDesc shadowDepthSkinVsDesc = {};
    shadowDepthSkinVsDesc.Key = L"ShadowDepthSkinningVS";
    shadowDepthSkinVsDesc.shaderType = EShaderType::Vertex;
    shadowDepthSkinVsDesc.Path = L"Resources/Shader/shadow_depth_skinning.vert.spv";
    shadowDepthSkinVsDesc.spirvPath = L"Resources/Shader/shadow_depth_skinning.vert.spv";
    shadowDepthSkinVsDesc.entryPoint = "main";
    shadowDepthSkinVsDesc.numStorageBuffers = 1;
    rm.LoadResource<Shader>(&shadowDepthSkinVsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(shadowDepthSkinVsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/shadow_depth_skinning.vert.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/shadow_depth_skinning.vert.bamshader");

    // Shadow Depth PS (depth-only)
    ShaderDesc shadowDepthPsDesc = {};
    shadowDepthPsDesc.Key = L"ShadowDepthPS";
    shadowDepthPsDesc.shaderType = EShaderType::Pixel;
    shadowDepthPsDesc.Path = L"Resources/Shader/shadow_depth.frag.spv";
    shadowDepthPsDesc.spirvPath = L"Resources/Shader/shadow_depth.frag.spv";
    shadowDepthPsDesc.entryPoint = "main";
    rm.LoadResource<Shader>(&shadowDepthPsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(shadowDepthPsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/shadow_depth.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/shadow_depth.frag.bamshader");


    // Viewport Channel PS
    ShaderDesc viewportChannelPsDesc = {};
    viewportChannelPsDesc.Key = L"ViewportChannelPS";
    viewportChannelPsDesc.shaderType = EShaderType::Pixel;
    viewportChannelPsDesc.Path = L"Resources/Shader/viewport_channel.frag.spv";
    viewportChannelPsDesc.spirvPath = L"Resources/Shader/viewport_channel.frag.spv";
    viewportChannelPsDesc.entryPoint = "main";
    viewportChannelPsDesc.numSamplers = 1;
    viewportChannelPsDesc.numUniformBuffers = 1;
    rm.LoadResource<Shader>(&viewportChannelPsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(viewportChannelPsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/viewport_channel.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/viewport_channel.frag.bamshader");

    // PostProcess PS
    ShaderDesc postProcessPsDesc = {};
    postProcessPsDesc.Key = L"PostProcessPS";
    postProcessPsDesc.shaderType = EShaderType::Pixel;
    postProcessPsDesc.Path = L"Resources/Shader/postprocess.frag.spv";
    postProcessPsDesc.spirvPath = L"Resources/Shader/postprocess.frag.spv";
    postProcessPsDesc.entryPoint = "main";
    postProcessPsDesc.numSamplers = 1;
    postProcessPsDesc.numUniformBuffers = 1;
    rm.LoadResource<Shader>(&postProcessPsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(postProcessPsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/postprocess.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/postprocess.frag.bamshader");

    // PostProcess - Tone Mapping PS
    ShaderDesc ppToneMappingPsDesc = {};
    ppToneMappingPsDesc.Key = L"PostProcess_ToneMappingPS";
    ppToneMappingPsDesc.shaderType = EShaderType::Pixel;
    ppToneMappingPsDesc.Path = L"Resources/Shader/postprocess_tonemapping.frag.spv";
    ppToneMappingPsDesc.spirvPath = L"Resources/Shader/postprocess_tonemapping.frag.spv";
    ppToneMappingPsDesc.entryPoint = "main";
    ppToneMappingPsDesc.numSamplers = 1;
    ppToneMappingPsDesc.numUniformBuffers = 1;
    rm.LoadResource<Shader>(&ppToneMappingPsDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(ppToneMappingPsDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/postprocess_tonemapping.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/postprocess_tonemapping.frag.bamshader");


    // Sky VS
    ShaderDesc skyVSDesc = {};
    skyVSDesc.Key = L"SkyVS";
    skyVSDesc.shaderType = EShaderType::Vertex;
    skyVSDesc.Path = L"Resources/Shader/sky.vert.spv";
    skyVSDesc.spirvPath = L"Resources/Shader/sky.vert.spv";
    skyVSDesc.entryPoint = "main";
    skyVSDesc.numUniformBuffers = 1;
    rm.LoadResource<Shader>(&skyVSDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(skyVSDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/sky.vert.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/sky.vert.bamshader");

    // Skybox PS
    ShaderDesc skyPSDesc = {};
    skyPSDesc.Key = L"SkyboxPS";
    skyPSDesc.shaderType = EShaderType::Pixel;
    skyPSDesc.Path = L"Resources/Shader/sky.frag.spv";
    skyPSDesc.spirvPath = L"Resources/Shader/sky.frag.spv";
    skyPSDesc.entryPoint = "main";
    skyPSDesc.numUniformBuffers = 1;
    rm.LoadResource<Shader>(&skyPSDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(skyPSDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/sky.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/sky.frag.bamshader");

    // UI VS
    ShaderDesc uiVSDesc = {};
    uiVSDesc.Key = L"UIVS";
    uiVSDesc.shaderType = EShaderType::Vertex;
    uiVSDesc.Path = L"Resources/Shader/ui.vert.spv";
    uiVSDesc.spirvPath = L"Resources/Shader/ui.vert.spv";
    uiVSDesc.entryPoint = "main";
    uiVSDesc.numUniformBuffers = 1;
    rm.LoadResource<Shader>(&uiVSDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(uiVSDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/ui.vert.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/ui.vert.bamshader");

    // UI PS
    ShaderDesc uiPSDesc = {};
    uiPSDesc.Key = L"UIPS";
    uiPSDesc.shaderType = EShaderType::Pixel;
    uiPSDesc.Path = L"Resources/Shader/ui.frag.spv";
    uiPSDesc.spirvPath = L"Resources/Shader/ui.frag.spv";
    uiPSDesc.entryPoint = "main";
    uiPSDesc.numUniformBuffers = 1;
    rm.LoadResource<Shader>(&uiPSDesc);
    {
        auto handle = rm.GetResourceHandle<Shader>(uiPSDesc.Key);
        rm.SaveToBinaryFile(handle.Get(), L"Resources/Shader/ui.frag.bamshader");
    }
    rm.LoadFile(L"Resources/Shader/ui.frag.bamshader");
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
            mat.bitangent = glm::cross(v.normal, v.tangent); // 자동 계산
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
}

void Application::InitializeMaterials()
{
    ResourceManager& resourceManager = ResourceManager::Get();
#pragma region Basic Materials

    MaterialDesc defaultMaterialDesc = {};
    defaultMaterialDesc.Key = L"Resources/Material/DefaultMaterial";
    defaultMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/default.vert.bamshader");
    defaultMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/default.frag.bamshader");
    Material* material = resourceManager.LoadResource<Material>(&defaultMaterialDesc).Get();
    material->SetTextureBinding("Default", 0, resourceManager.GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png"));
    resourceManager.SaveToBinaryFile(material, L"Resources/Material/DefaultMaterial.bammat");
    
    MaterialDesc spriteMaterialDesc = {};
    spriteMaterialDesc.Key = L"Resources/Material/SpriteMaterial";
    spriteMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/sprite.vert.bamshader");
    spriteMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/sprite.frag.bamshader");
    spriteMaterialDesc.BlendMode = EBlendMode::AlphaBlend;
    spriteMaterialDesc.CullMode = ECullMode::None;
    spriteMaterialDesc.DepthMode = EDepthMode::ReadWrite;
    Material* spriteMaterial = resourceManager.LoadResource<Material>(&spriteMaterialDesc).Get();
    resourceManager.SaveToBinaryFile(spriteMaterial, L"Resources/Material/SpriteMaterial.bammat");

    MaterialInstanceDesc spriteMaterialInstanceDesc = {};
    spriteMaterialInstanceDesc.BaseMaterialHandle = resourceManager.GetResourceHandle<Material>(L"Resources/Material/SpriteMaterial.bammat");
    spriteMaterialInstanceDesc.Key = L"Resources/Material/SpriteMaterialInstance";
    ResourceHandle<MaterialInstance> spriteMaterialInstanceHandle = resourceManager.LoadResource<MaterialInstance>(&spriteMaterialInstanceDesc);
    MaterialInstance* spriteMaterialInstance = spriteMaterialInstanceHandle.Get();
    resourceManager.SaveToBinaryFile(spriteMaterialInstance, L"Resources/Material/SpriteMaterial.bammatinst");
    //resourceManager.DestroyResource(spriteMaterialInstanceHandle.GetRawHandle());


    ShaderDesc defaultSkinningShaderDesc = {};
    defaultSkinningShaderDesc.Key = L"Resources/Shader/Skinning";
    defaultSkinningShaderDesc.Path = L"Resources/Shader/skinning.vert.spv";
    defaultSkinningShaderDesc.spirvPath = L"Resources/Shader/skinning.vert.spv";
    defaultSkinningShaderDesc.shaderType = EShaderType::Vertex;
    defaultSkinningShaderDesc.numStorageBuffers = 1;
    Shader* skinningShader = resourceManager.LoadResource<Shader>(&defaultSkinningShaderDesc).Get();
    resourceManager.SaveToBinaryFile(skinningShader, L"Resources/Shader/skinning.vert.bamshader");
    resourceManager.LoadFile(L"Resources/Shader/skinning.vert.bamshader");

    MaterialDesc skinningMaterialDesc = {};
    skinningMaterialDesc.Key = L"Resources/Material/SkinningMaterial";
    skinningMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/skinning.vert.bamshader");
    skinningMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/default.frag.bamshader");
    skinningMaterialDesc.BlendMode = EBlendMode::AlphaBlend;
    skinningMaterialDesc.CullMode = ECullMode::None;
    skinningMaterialDesc.DepthMode = EDepthMode::ReadWrite;
    Material* skinningMaterial = resourceManager.LoadResource<Material>(&skinningMaterialDesc).Get();
    resourceManager.SaveToBinaryFile(skinningMaterial, L"Resources/Material/SkinningMaterial.bammat");
    resourceManager.LoadFile(L"Resources/Material/SkinningMaterial.bammat");

    MaterialInstanceDesc skinningMaterialInstanceDesc = {};
    skinningMaterialInstanceDesc.BaseMaterialHandle = resourceManager.GetResourceHandle<Material>(L"Resources/Material/SkinningMaterial.bammat");
    skinningMaterialInstanceDesc.Key = L"Resources/Material/SkinningMaterialInstance";
    MaterialInstance* skinningMaterialInstance = resourceManager.LoadResource<MaterialInstance>(&skinningMaterialInstanceDesc).Get();
    resourceManager.SaveToBinaryFile(skinningMaterialInstance, L"Resources/Material/SkinningMaterial.bammatinst");
    resourceManager.LoadFile(L"Resources/Material/SkinningMaterial.bammatinst");
#pragma endregion

    // ── G-Buffer Material 생성 ──
    MaterialDesc gbufferMatDesc = {};
    gbufferMatDesc.Key = L"Resources/Material/GBufferMaterial";
    gbufferMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/gbuffer.vert.bamshader");
    gbufferMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/gbuffer.frag.bamshader");
    gbufferMatDesc.BlendMode = EBlendMode::Opaque;
    gbufferMatDesc.CullMode = ECullMode::Back;
    gbufferMatDesc.DepthMode = EDepthMode::ReadWrite;
    Material* gbufferMat = resourceManager.LoadResource<Material>(&gbufferMatDesc).Get();
    resourceManager.SaveToBinaryFile(gbufferMat, L"Resources/Material/GBufferMaterial.bammat");
    resourceManager.LoadFile(L"Resources/Material/GBufferMaterial.bammat");


	// ── Shadow Depth Material 생성 ──
    MaterialDesc shadowDepthMatDesc = {};
    shadowDepthMatDesc.Key = L"Resources/Material/ShadowDepthMaterial";
    shadowDepthMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/shadow_depth.vert.bamshader");
    shadowDepthMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/shadow_depth.frag.bamshader");
    shadowDepthMatDesc.BlendMode = EBlendMode::Opaque;
    shadowDepthMatDesc.CullMode = ECullMode::Front;
    shadowDepthMatDesc.DepthMode = EDepthMode::ReadWrite;
    Material* shadowDepthMat = resourceManager.LoadResource<Material>(&shadowDepthMatDesc).Get();
    resourceManager.SaveToBinaryFile(shadowDepthMat, L"Resources/Material/ShadowDepthMaterial.bammat");
	resourceManager.LoadFile(L"Resources/Material/ShadowDepthMaterial.bammat");

	MaterialDesc shadowDepthSkinMatDesc = {};
    shadowDepthSkinMatDesc.Key = L"Resources/Material/ShadowDepthSkinningMaterial";
    shadowDepthSkinMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/shadow_depth_skinning.vert.bamshader");
    shadowDepthSkinMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/shadow_depth.frag.bamshader");
    shadowDepthSkinMatDesc.BlendMode = EBlendMode::Opaque;
    shadowDepthSkinMatDesc.CullMode = ECullMode::Front;
    shadowDepthSkinMatDesc.DepthMode = EDepthMode::ReadWrite;
    Material* shadowDepthSkinMat = resourceManager.LoadResource<Material>(&shadowDepthSkinMatDesc).Get();
	resourceManager.SaveToBinaryFile(shadowDepthSkinMat, L"Resources/Material/ShadowDepthSkinningMaterial.bammat");


	// ── Transparent Material 생성 ──
	MaterialDesc transparentMatDesc = {};
    transparentMatDesc.Key = L"Resources/Material/TransparentMaterial";
    transparentMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/default.vert.bamshader");
    transparentMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/default.frag.bamshader");
    transparentMatDesc.BlendMode = EBlendMode::AlphaBlend;
    transparentMatDesc.CullMode = ECullMode::None;
    transparentMatDesc.DepthMode = EDepthMode::ReadOnly;
	Material* transparentMat = resourceManager.LoadResource<Material>(&transparentMatDesc).Get();
	resourceManager.SaveToBinaryFile(transparentMat, L"Resources/Material/TransparentMaterial.bammat");


	// ── Sky Material 생성 ──
	MaterialDesc skyMatDesc = {};
    skyMatDesc.Key = L"Resources/Material/SkyMaterial";
    skyMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/sky.vert.bamshader");
    skyMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/sky.frag.bamshader");
    skyMatDesc.BlendMode = EBlendMode::Forward;
    skyMatDesc.CullMode = ECullMode::None; // 뒤집힌 큐브이므로 Front 면을 카울링
	skyMatDesc.DepthMode = EDepthMode::ReadOnly; // 깊이 테스트는 하지만 쓰지는 않음'
	skyMatDesc.DepthCompareOp = ECompareOp::LessOrEqual;
    Material* skyMat = resourceManager.LoadResource<Material>(&skyMatDesc).Get();
	resourceManager.SaveToBinaryFile(skyMat, L"Resources/Material/SkyMaterial.bammat");

    // ── UI Material 생성 ──
    MaterialDesc uiMatDesc = {};
    uiMatDesc.Key = L"Resources/Material/UIMaterial";
    uiMatDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/ui.vert.bamshader");
    uiMatDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(
        L"Resources/Shader/ui.frag.bamshader");
    uiMatDesc.BlendMode = EBlendMode::AlphaBlend;
    uiMatDesc.CullMode = ECullMode::None;
    uiMatDesc.DepthMode = EDepthMode::None;
    uiMatDesc.DepthCompareOp = ECompareOp::LessOrEqual;
    Material* uiMat = resourceManager.LoadResource<Material>(&uiMatDesc).Get();
    resourceManager.SaveToBinaryFile(uiMat, L"Resources/Material/UIMaterial.bammat");
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
    finalDesc.format = ETextureFormat::R8G8B8A8_UNORM;
    finalDesc.name = L"FinalColor";
    finalDesc.width = w;
    finalDesc.height = h;
    finalDesc.bindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
        | ERenderTargetBindFlag::RTBF_RenderTarget;
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

#pragma region PIE (Play In Editor)
void Application::EnterPlayMode()
{
    if (m_PlayState != EPlayState::Edit) return;

    SnapshotScene();
    m_PlayState = EPlayState::Play;
    fmt::print("[PIE] Enter Play\n");
}
void Application::PausePlayMode()
{
    if (m_PlayState != EPlayState::Play) return;

    m_PlayState = EPlayState::Pause;
    fmt::print("[PIE] Paused\n");
}
void Application::ResumePlayMode()
{
    if (m_PlayState != EPlayState::Pause) return;

    m_PlayState = EPlayState::Play;
    fmt::print("[PIE] Resumed\n");
}
void Application::StopPlayMode()
{
    if (m_PlayState == EPlayState::Edit) return;

    m_PlayState = EPlayState::Edit;
    RestoreScene();
    fmt::print("[PIE] Stopped — scene restored\n");
}

void Application::SnapshotScene()
{
    Scene* scene = SceneManager::Get().GetCurrentScene();
    if (!scene) return;

    JsonArchive archive(EArchiveMode::Write);
    if (archive.PushScope(entt::resolve(scene->GetTypeID()).info().name().data()))
    {
        scene->Serialize(archive);
        archive.PopScope();
    }
    archive.SaveToFile(WStrToStr(m_SnapshotPath));
}

void Application::RestoreScene()
{
    SelectionManager::Get().ClearSelection();

    JsonArchive archive(EArchiveMode::Read);
    if (!archive.LoadFromFile(WStrToStr(m_SnapshotPath)))
    {
        fmt::print(stderr, "[PIE] Failed to load snapshot: {}\n", WStrToStr(m_SnapshotPath));
        return;
    }

	SceneManager::Get().CloseScene();
    Scene* newScene = Scene::Create();
    if (!newScene) return;

    if (archive.PushScope(entt::resolve(newScene->GetTypeID()).info().name().data()))
    {
        newScene->Deserialize(archive);
        archive.PopScope();
    }

    SceneManager::Get().OpenScene(newScene);
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
                uint32 newWidth = (uint32)event.window.data1;
                uint32 newHeight = (uint32)event.window.data2;

                if (newWidth <= 0 || newHeight <= 0)
                    continue;

                Renderer::Get().GetRHI()->Resize(newWidth, newHeight);
            }
        }

        timeManager.Update();
		f32 dt = timeManager.GetDeltaTime();
		UpdateTitle(dt);
        ImGuiManager::Get().Update(dt);
        m_AssetManager->Update(dt);

        if (m_PlayState == EPlayState::Play)
        {
            m_Runtime->FixedUpdate(dt);
            m_Runtime->Update(dt);
            m_Runtime->LateUpdate(dt);
        }
        else
        {
            InputManager::Get().Update(dt);
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