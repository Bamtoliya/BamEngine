#pragma once
#include <fmt/core.h>
#include <SDL3/SDL.h>
#include "Application.h"
#include "Runtime.h"
#include "ImGuiManager.h"
#include "SelectionManager.h"
#include "RHI.h"
#include "AssetManager.h"

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

    m_AssetManager = AssetManager::Create();
	if (!m_AssetManager) return EResult::Fail;

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


    RenderPassID uiPassID = RenderPassManager::Get().RegisterRenderPass(L"Editor UI", {L"FinalColor"}, L"", ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store, vec4(0.0f, 0.0f, 0.0f, -1.0f), 1000, ERenderSortType::None);
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
}

void Application::InitializeShaders()
{
    ResourceManager& rm = ResourceManager::Get();
    // Fullscreen Quad VS
    tagShaderDesc fsQuadVsDesc = {};
    fsQuadVsDesc.Key = L"FullscreenQuadVS";
    fsQuadVsDesc.ShaderType = EShaderType::Vertex;
    fsQuadVsDesc.Path = L"Resources/Shader/fullscreen_quad.vert.spv";
    fsQuadVsDesc.SpirvPath = L"Resources/Shader/fullscreen_quad.vert.spv";
    fsQuadVsDesc.EntryPoint = "main";
    rm.LoadResource<Shader>(&fsQuadVsDesc);
    // Lighting PS
    tagShaderDesc lightingPsDesc = {};
    lightingPsDesc.Key = L"LightingPS";
    lightingPsDesc.ShaderType = EShaderType::Pixel;
    lightingPsDesc.Path = L"Resources/Shader/lighting.frag.spv";
    lightingPsDesc.SpirvPath = L"Resources/Shader/lighting.frag.spv";
    lightingPsDesc.EntryPoint = "main";
    lightingPsDesc.NumStorageBuffers = 1;
    rm.LoadResource<Shader>(&lightingPsDesc);
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

        tagMeshCreateDesc meshDesc = {};
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
        tagMeshCreateDesc meshDesc = {};
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

    //tagMaterialDesc defaultMaterialDesc = {};
    //defaultMaterialDesc.Key = L"Resources/Material/DefaultMaterial";
    //defaultMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/default.vert.bamshader");
    //defaultMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/default.frag.bamshader");
    //Material* material = resourceManager.LoadResource<Material>(&defaultMaterialDesc).Get();
    //material->SetTextureBinding("Default", 0, resourceManager.GetResourceHandle<Texture>(L"Resources/Texture/magenta1x1.png"));
    //resourceManager.SaveToBinaryFile(material, L"Resources/Material/DefaultMaterial.bammat");
    //
    tagMaterialDesc spriteMaterialDesc = {};
    spriteMaterialDesc.Key = L"Resources/Material/SpriteMaterial";
    spriteMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/sprite.vert.bamshader");
    spriteMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/sprite.frag.bamshader");
    spriteMaterialDesc.BlendMode = EBlendMode::AlphaBlend;
    spriteMaterialDesc.CullMode = ECullMode::None;
    spriteMaterialDesc.DepthMode = EDepthMode::ReadWrite;
    Material* spriteMaterial = resourceManager.LoadResource<Material>(&spriteMaterialDesc).Get();
    resourceManager.SaveToBinaryFile(spriteMaterial, L"Resources/Material/SpriteMaterial.bammat");

    tagMaterialInstanceDesc spriteMaterialInstanceDesc = {};
    spriteMaterialInstanceDesc.BaseMaterialHandle = resourceManager.GetResourceHandle<Material>(L"Resources/Material/SpriteMaterial.bammat");
    spriteMaterialInstanceDesc.Key = L"Resources/Material/SpriteMaterialInstance";
    ResourceHandle<MaterialInstance> spriteMaterialInstanceHandle = resourceManager.LoadResource<MaterialInstance>(&spriteMaterialInstanceDesc);
    MaterialInstance* spriteMaterialInstance = spriteMaterialInstanceHandle.Get();
    resourceManager.SaveToBinaryFile(spriteMaterialInstance, L"Resources/Material/SpriteMaterial.bammatinst");
    //resourceManager.DestroyResource(spriteMaterialInstanceHandle.GetRawHandle());


    tagShaderDesc defaultSkinningShaderDesc = {};
    defaultSkinningShaderDesc.Key = L"Resources/Shader/Skinning";
    defaultSkinningShaderDesc.Path = L"Resources/Shader/skinning.vert.spv";
    defaultSkinningShaderDesc.SpirvPath = L"Resources/Shader/skinning.vert.spv";
    defaultSkinningShaderDesc.ShaderType = EShaderType::Vertex;
    defaultSkinningShaderDesc.NumStorageBuffers = 1;
    Shader* skinningShader = resourceManager.LoadResource<Shader>(&defaultSkinningShaderDesc).Get();
    resourceManager.SaveToBinaryFile(skinningShader, L"Resources/Shader/skinning.vert.bamshader");
    resourceManager.LoadFile(L"Resources/Shader/skinning.vert.bamshader");

    tagMaterialDesc skinningMaterialDesc = {};
    skinningMaterialDesc.Key = L"Resources/Material/SkinningMaterial";
    skinningMaterialDesc.VertexShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/skinning.vert.bamshader");
    skinningMaterialDesc.PixelShaderHandle = resourceManager.GetResourceHandle<Shader>(L"Resources/Shader/default.frag.bamshader");
    skinningMaterialDesc.BlendMode = EBlendMode::AlphaBlend;
    skinningMaterialDesc.CullMode = ECullMode::None;
    skinningMaterialDesc.DepthMode = EDepthMode::ReadWrite;
    Material* skinningMaterial = resourceManager.LoadResource<Material>(&skinningMaterialDesc).Get();
    resourceManager.SaveToBinaryFile(skinningMaterial, L"Resources/Material/SkinningMaterial.bammat");
    resourceManager.LoadFile(L"Resources/Material/SkinningMaterial.bammat");

    tagMaterialInstanceDesc skinningMaterialInstanceDesc = {};
    skinningMaterialInstanceDesc.BaseMaterialHandle = resourceManager.GetResourceHandle<Material>(L"Resources/Material/SkinningMaterial.bammat");
    skinningMaterialInstanceDesc.Key = L"Resources/Material/SkinningMaterialInstance";
    MaterialInstance* skinningMaterialInstance = resourceManager.LoadResource<MaterialInstance>(&skinningMaterialInstanceDesc).Get();
    resourceManager.SaveToBinaryFile(skinningMaterialInstance, L"Resources/Material/SkinningMaterial.bammatinst");
    resourceManager.LoadFile(L"Resources/Material/SkinningMaterial.bammatinst");
#pragma endregion

    tagShaderDesc gbufferVsDesc = {};
    gbufferVsDesc.Key = L"Resources/Shader/GBufferVS";
    gbufferVsDesc.Path = L"Resources/Shader/gbuffer.vert.spv";
    gbufferVsDesc.SpirvPath = L"Resources/Shader/gbuffer.vert.spv";
    gbufferVsDesc.ShaderType = EShaderType::Vertex;
    resourceManager.LoadResource<Shader>(&gbufferVsDesc);
    resourceManager.SaveToBinaryFile(
        resourceManager.GetResourceHandle<Shader>(gbufferVsDesc.Key).Get(),
        L"Resources/Shader/gbuffer.vert.bamshader");
    resourceManager.LoadFile(L"Resources/Shader/gbuffer.vert.bamshader");
    tagShaderDesc gbufferPsDesc = {};
    gbufferPsDesc.Key = L"Resources/Shader/GBufferPS";
    gbufferPsDesc.Path = L"Resources/Shader/gbuffer.frag.spv";
    gbufferPsDesc.SpirvPath = L"Resources/Shader/gbuffer.frag.spv";
    gbufferPsDesc.ShaderType = EShaderType::Pixel;
    resourceManager.LoadResource<Shader>(&gbufferPsDesc);
    resourceManager.SaveToBinaryFile(
        resourceManager.GetResourceHandle<Shader>(gbufferPsDesc.Key).Get(),
        L"Resources/Shader/gbuffer.frag.bamshader");
    resourceManager.LoadFile(L"Resources/Shader/gbuffer.frag.bamshader");
    // ── G-Buffer Material 생성 ──
    tagMaterialDesc gbufferMatDesc = {};
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
    tagRenderTargetDesc finalDesc = {};
    finalDesc.Name = L"FinalColor";
    finalDesc.Width = w;
    finalDesc.Height = h;
    finalDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
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
		m_AssetManager->Update(dt);
		//m_Runtime->RunFrame(dt);
        m_Runtime->FixedUpdate(dt);
        m_Runtime->Update(dt);
        m_Runtime->LateUpdate(dt);
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

void Application::Test(f32 dt)
{
    
}

END