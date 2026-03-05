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

    RenderPassID uiPassID = RenderPassManager::Get().RegisterRenderPass(L"Editor UI", {L"RenderTarget_1"}, L"", ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store, vec4(0.0f, 0.0f, 0.0f, -1.0f), 1000, ERenderSortType::None);
    DelegateHandle uiHandle = Renderer::Get().GetRenderPassDelegate(uiPassID).AddLambda([](f32 dt) {
        ImGuiManager::Get().Begin();
        ImGuiManager::Get().Draw();
        ImGuiManager::Get().End();
        });
    InitializeLocalization();
    InitializeResources();
    //Renderer::Get().GetRenderPassDelegate(uiPassID).Remove(uiHandle);

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

#pragma region Basic Textures
    ResourceManager::Get().LoadTexture(L"SampleTexture", L"Resources/Texture/uv1.png");
    ResourceManager::Get().LoadTexture(L"White1x1", L"Resources/Texture/white1x1.png");
    ResourceManager::Get().LoadTexture(L"Black1x1", L"Resources/Texture/black1x1.png");
    ResourceManager::Get().LoadTexture(L"Magenta1x1", L"Resources/Texture/magenta1x1.png");
    ResourceManager::Get().LoadTexture(L"Green1x1", L"Resources/Texture/green1x1.png");
    ResourceManager::Get().LoadTexture(L"Blue1x1", L"Resources/Texture/blue1x1.png");
#pragma endregion

#pragma region Quad
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

#pragma region Cube
    {
        vector<Vertex> vertices = {
            // 앞면 (Z+)
            { { -1.0f,  1.0f,  1.0f }, { 0, 0, 1 }, { 0, 0 }, { 1, 0, 0 } },
            { {  1.0f,  1.0f,  1.0f }, { 0, 0, 1 }, { 1, 0 }, { 1, 0, 0 } },
            { {  1.0f, -1.0f,  1.0f }, { 0, 0, 1 }, { 1, 1 }, { 1, 0, 0 } },
            { { -1.0f, -1.0f,  1.0f }, { 0, 0, 1 }, { 0, 1 }, { 1, 0, 0 } },

            // 뒷면 (Z-)
            { {  1.0f,  1.0f, -1.0f }, { 0, 0, -1 }, { 0, 0 }, { -1, 0, 0 } },
            { { -1.0f,  1.0f, -1.0f }, { 0, 0, -1 }, { 1, 0 }, { -1, 0, 0 } },
            { { -1.0f, -1.0f, -1.0f }, { 0, 0, -1 }, { 1, 1 }, { -1, 0, 0 } },
            { {  1.0f, -1.0f, -1.0f }, { 0, 0, -1 }, { 0, 1 }, { -1, 0, 0 } },

            // 윗면 (Y+)
            { { -1.0f,  1.0f, -1.0f }, { 0, 1, 0 }, { 0, 0 }, { 1, 0, 0 } },
            { {  1.0f,  1.0f, -1.0f }, { 0, 1, 0 }, { 1, 0 }, { 1, 0, 0 } },
            { {  1.0f,  1.0f,  1.0f }, { 0, 1, 0 }, { 1, 1 }, { 1, 0, 0 } },
            { { -1.0f,  1.0f,  1.0f }, { 0, 1, 0 }, { 0, 1 }, { 1, 0, 0 } },

            // 아랫면 (Y-)
            { { -1.0f, -1.0f,  1.0f }, { 0, -1, 0 }, { 0, 0 }, { 1, 0, 0 } },
            { {  1.0f, -1.0f,  1.0f }, { 0, -1, 0 }, { 1, 0 }, { 1, 0, 0 } },
            { {  1.0f, -1.0f, -1.0f }, { 0, -1, 0 }, { 1, 1 }, { 1, 0, 0 } },
            { { -1.0f, -1.0f, -1.0f }, { 0, -1, 0 }, { 0, 1 }, { 1, 0, 0 } },

            // 우측면 (X+)
            { {  1.0f,  1.0f,  1.0f }, { 1, 0, 0 }, { 0, 0 }, { 0, 0, -1 } },
            { {  1.0f,  1.0f, -1.0f }, { 1, 0, 0 }, { 1, 0 }, { 0, 0, -1 } },
            { {  1.0f, -1.0f, -1.0f }, { 1, 0, 0 }, { 1, 1 }, { 0, 0, -1 } },
            { {  1.0f, -1.0f,  1.0f }, { 1, 0, 0 }, { 0, 1 }, { 0, 0, -1 } },

            // 좌측면 (X-)
            { { -1.0f,  1.0f, -1.0f }, { -1, 0, 0 }, { 0, 0 }, { 0, 0, 1 } },
            { { -1.0f,  1.0f,  1.0f }, { -1, 0, 0 }, { 1, 0 }, { 0, 0, 1 } },
            { { -1.0f, -1.0f,  1.0f }, { -1, 0, 0 }, { 1, 1 }, { 0, 0, 1 } },
            { { -1.0f, -1.0f, -1.0f }, { -1, 0, 0 }, { 0, 1 }, { 0, 0, 1 } }
        };

        vec3 minBound = vec3((numeric_limits<f32>::max)());
        vec3 maxBound = vec3(numeric_limits<f32>::lowest());

        for (const auto& v : vertices)
        {
            // Vertex 구조체의 위치(Position) 변수명에 맞추어 접근하세요.
            // 여기서는 v.position.x 와 같이 가정했습니다. (코드에 맞춰 수정 필요)
            minBound.x = std::min(minBound.x, v.position.x);
            minBound.y = std::min(minBound.y, v.position.y);
            minBound.z = std::min(minBound.z, v.position.z);

            maxBound.x = std::max(maxBound.x, v.position.x);
            maxBound.y = std::max(maxBound.y, v.position.y);
            maxBound.z = std::max(maxBound.z, v.position.z);
        }

        // 2. 인덱스 데이터 정의 (36개 인덱스 - 6면 * 2삼각형 * 3정점)
        vector<uint32> indices;
        for (uint32 i = 0; i < 6; ++i)
        {
            uint32 startIdx = i * 4;
            // 첫 번째 삼각형
            indices.push_back(startIdx + 0);
            indices.push_back(startIdx + 1);
            indices.push_back(startIdx + 2);
            // 두 번째 삼각형
            indices.push_back(startIdx + 0);
            indices.push_back(startIdx + 2);
            indices.push_back(startIdx + 3);
        }

        // 3. Mesh 생성 정보 설정
        tagMeshCreateInfo meshDesc = {};
        meshDesc.VertexData = vertices.data();
        meshDesc.VertexCount = static_cast<uint32>(vertices.size());
        meshDesc.VertexStride = sizeof(Vertex);
        meshDesc.IndexData = indices.data();
        meshDesc.IndexStride = sizeof(uint32);
        meshDesc.IndexCount = static_cast<uint32>(indices.size());
        meshDesc.BoundingBoxMin = minBound;
		meshDesc.BoundingBoxMax = maxBound;

        // 4. ResourceManager를 통한 로드
        ResourceManager::Get().LoadMesh(L"CubeMesh", &meshDesc);
    }
#pragma endregion

#pragma region Shader
    tagShaderDesc vsDesc;
    vsDesc.ShaderType = EShaderType::Vertex;
    vsDesc.FilePath = L"Resources/Shader/default.vert.spv";
    vsDesc.EntryPoint = "main";
    ResourceManager::Get().LoadShader(L"DefaultVS", &vsDesc);

    tagShaderDesc psDesc;
    psDesc.ShaderType = EShaderType::Pixel;
    psDesc.FilePath = L"Resources/Shader/default.frag.spv";
    psDesc.EntryPoint = "main";
    ResourceManager::Get().LoadShader(L"DefaultPS", &psDesc);

	tagShaderDesc spriteVSDesc;
	spriteVSDesc.ShaderType = EShaderType::Vertex;
	spriteVSDesc.FilePath = L"Resources/Shader/sprite.vert.spv";
	spriteVSDesc.EntryPoint = "main";
	ResourceManager::Get().LoadShader(L"SpriteVS", &spriteVSDesc);

    tagShaderDesc spritePsDesc;
    spritePsDesc.ShaderType = EShaderType::Pixel;
    spritePsDesc.FilePath = L"Resources/Shader/sprite.frag.spv";
    spritePsDesc.EntryPoint = "main";
    ResourceManager::Get().LoadShader(L"SpritePS", &spritePsDesc);
#pragma endregion

    tagMaterialDesc materialDesc;
	materialDesc.VertexShader = ResourceManager::Get().GetShader(L"DefaultVS");
    materialDesc.PixelShader = ResourceManager::Get().GetShader(L"DefaultPS");
    materialDesc.DepthMode = EDepthMode::ReadWrite;
    ResourceManager::Get().LoadMaterial(L"DefaultMaterial", &materialDesc);
	ResourceManager::Get().GetMaterial(L"DefaultMaterial")->SetTextureBySlot(0, ResourceManager::Get().GetTexture(L"Magenta1x1")->GetRHITexture());

	tagMaterialDesc spriteMaterialDesc;
	spriteMaterialDesc.VertexShader = ResourceManager::Get().GetShader(L"DefaultVS");
	spriteMaterialDesc.PixelShader = ResourceManager::Get().GetShader(L"DefaultPS");
	spriteMaterialDesc.BlendMode = EBlendMode::AlphaBlend;
	spriteMaterialDesc.CullMode = ECullMode::None;
	spriteMaterialDesc.DepthMode = EDepthMode::ReadWrite;
	ResourceManager::Get().LoadMaterial(L"SpriteMaterial", &spriteMaterialDesc);



    



#ifdef _DEBUG
	
#endif
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

                Renderer::Get().GetRHI()->Resize(newWidth, newHeight);
            }
        }

        timeManager.Update();
		f32 dt = timeManager.GetDeltaTime();
		UpdateTitle(dt);
		ImGuiManager::Get().Update(dt);
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
    static uint32 lastFPS = 0;

    uint32 currentFPS = Engine::TimeManager::Get().GetFPS();

    if (lastFPS != currentFPS)
    {
        lastFPS = currentFPS;
        std::string title = fmt::format("BamEngine Editor - FPS: {}", currentFPS);
        SDL_SetWindowTitle(m_Window, title.c_str());
    }
}

void Application::Test(f32 dt)
{
    
}

END