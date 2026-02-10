#pragma once
#include <fmt/core.h>
#include <SDL3/SDL.h>
#include "Application.h"
#include "Runtime.h"
#include "ImGuiManager.h"
#include "SelectionManager.h"
#include "RHI.h"

BEGIN(Editor)

IMPLEMENT_SINGLETON(Application)

#pragma region Constructor&Destructor
EResult Application::Initialize(void* arg)
{
	if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fmt::print(stderr, "SDL_Init Failed: {}\n", SDL_GetError());
        return EResult::Fail;
    }

    m_Window = SDL_CreateWindow(
        "BamEngine Editor",
        g_WindowWidth, g_WindowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN // 기본 윈도우 플래그 (SDL_WINDOW_RESIZABLE 등 필요시 추가)
	);

	RUNTIMEDESC runtimeDesc = {};
	runtimeDesc.RendererDesc.RHIType = ERHIType::SDLGPU;
	runtimeDesc.RendererDesc.RHIDesc.WindowHandle = m_Window;
	runtimeDesc.RendererDesc.RHIDesc.Width = g_WindowWidth;
	runtimeDesc.RendererDesc.RHIDesc.Height = g_WindowHeight;
    runtimeDesc.RendererDesc.RHIDesc.IsVSync = true;

    m_Runtime = Runtime::Create(&runtimeDesc);
    if (!m_Runtime) return EResult::Fail;

	tagImGuiManagerDesc imguiDesc = {};
	imguiDesc.Window = m_Window;
    imguiDesc.RHI = Renderer::Get().GetRHI();

    m_ImGuiManager = ImGuiManager::Create(&imguiDesc);
    m_SelectionManager = SelectionManager::Create();
    if (!m_ImGuiManager)
    {
        fmt::print(stderr, "ImGuiManager Creation Failed\n");
		return EResult::Fail;
    }

    RenderPassID uiPassID = RenderPassManager::Get().RegisterRenderPass(L"Editor UI", 1000, ERenderSortType::None);
    DelegateHandle uiHandle = Renderer::Get().GetRenderPassDelegate(uiPassID).AddLambda([](f32 dt) {
        ImGuiManager::Get().Begin();
        ImGuiManager::Get().Draw();
        ImGuiManager::Get().End();
        });
    InitializeLocalization();
    InitializeResources();
    //Renderer::Get().GetRenderPassDelegate(uiPassID).Remove(uiHandle);

	ResourceManager::Get().LoadTexture(L"SampleTexture", L"Resources/Texture/uv1.png");

    return EResult::Success;
}

void Application::Free()
{
    ImGuiManager::Destroy();
	SelectionManager::Destroy();

    if (m_Runtime) m_Runtime->Destroy();

    if(m_Window) SDL_DestroyWindow(m_Window);
    SDL_Quit();
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
#pragma region Test
    {
        // 1. 사각형 정점 데이터 정의 (화면 좌표계: Top-Left가 0,0 가정 시 대략적인 중앙 배치)
        // 색상은 SDLRendererRHI 구현에 따라 다를 수 있으나 Vertex 구조체에 맞춰 설정
        vector<Vertex> vertices = {
            // position (x, y, z)          // normal       // texCoord // tangent
            { { -1.0f,  1.0f, 0.0f }, { 0, 0, -1 }, { 0, 0 }, { 0, 0, 0 } }, // Top-Left
            { {  1.0f,  1.0f, 0.0f }, { 0, 0, -1 }, { 1, 0 }, { 0, 0, 0 } }, // Top-Right
            { {  1.0f, -1.0f, 0.0f }, { 0, 0, -1 }, { 1, 1 }, { 0, 0, 0 } }, // Bottom-Right
            { { -1.0f, -1.0f, 0.0f }, { 0, 0, -1 }, { 0, 1 }, { 0, 0, 0 } }  // Bottom-Left
        };

        // 인덱스 (사각형을 구성하는 두 개의 삼각형)
        vector<uint32> indices = {
            0, 1, 2, // 첫 번째 삼각형
            0, 2, 3  // 두 번째 삼각형
        };

        // 2. Mesh 생성
        tagMeshCreateInfo meshDesc = {};
        meshDesc.VertexData = vertices.data();
        meshDesc.VertexCount = static_cast<uint32>(vertices.size());
        meshDesc.VertexStride = sizeof(Vertex);
        meshDesc.IndexData = indices.data();
        meshDesc.IndexStride = sizeof(uint32);
        meshDesc.IndexCount = static_cast<uint32>(indices.size());

        ResourceManager::Get().LoadMesh(L"QuadMesh", &meshDesc);
    }
#pragma endregion

    tagShaderDesc vsDesc;
    vsDesc.ShaderType = EShaderType::Vertex;
    vsDesc.FilePath = L"Resources/Shader/sprite.vert.spv";
    vsDesc.EntryPoint = "main";
    ResourceManager::Get().LoadShader(L"DefaultVS", &vsDesc);
    
    tagShaderDesc psDesc;
    psDesc.ShaderType = EShaderType::Pixel;
    psDesc.FilePath = L"Resources/Shader/sprite.frag.spv";
    psDesc.EntryPoint = "main";
    ResourceManager::Get().LoadShader(L"DefaultPS", &psDesc);
    
    tagRHIPipelineDesc pipelineDesc;
    pipelineDesc.VertexShader = ResourceManager::Get().GetShader(L"DefaultVS")->GetRHIShader();
    pipelineDesc.PixelShader = ResourceManager::Get().GetShader(L"DefaultPS")->GetRHIShader();
    PipelineManager::Get().CreatePipeline(L"Default", pipelineDesc);
    
    tagMaterialDesc materialDesc;
    materialDesc.PipelineKey = L"Default";
    ResourceManager::Get().LoadMaterial(L"DefaultMaterial", &materialDesc);
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

                Renderer::Get().GetRHI()->Resize(newWidth, newHeight);
            }
        }

        timeManager.Update();
		f32 dt = timeManager.GetDeltaTime();
		UpdateTitle(dt);
        Test(dt);
		m_Runtime->RunFrame(dt);
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
    m_FPSTimer += dt;
    if (m_FPSTimer >= 1.0f)
    {
        uint32 fps = Engine::TimeManager::Get().GetFPS();
        std::string title = fmt::format("BamEngine Editor - FPS: {}", fps);
        SDL_SetWindowTitle(m_Window, title.c_str());
        m_FPSTimer -= 1.0f;
	}
}

void Application::Test(f32 dt)
{
    
}

END