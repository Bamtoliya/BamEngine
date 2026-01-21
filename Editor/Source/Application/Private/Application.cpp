#pragma once
#include <fmt/core.h>
#include <SDL3/SDL.h>
#include "Application.h"
#include "Runtime.h"
#include "ImGuiManager.h"
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
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY  // 기본 윈도우 플래그 (SDL_WINDOW_RESIZABLE 등 필요시 추가)
	);

	RUNTIMEDESC runtimeDesc = {};
	runtimeDesc.RendererDesc.RHIType = ERHIType::SDLRenderer;
	runtimeDesc.RendererDesc.WindowHandle = m_Window;
	runtimeDesc.RendererDesc.Width = g_WindowWidth;
	runtimeDesc.RendererDesc.Height = g_WindowHeight;
    runtimeDesc.RendererDesc.IsVSync = true;

    m_Runtime = Runtime::Create(&runtimeDesc);
    if (!m_Runtime) return EResult::Fail;

	ImGuiManager::IMGUISDLDESC imguiDesc = {};
	imguiDesc.Window = m_Window;
    imguiDesc.RHI = Renderer::Get().GetRHI();

    m_ImGuiManager = ImGuiManager::Create(&imguiDesc);
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
    Renderer::Get().GetRenderPassDelegate(uiPassID).Remove(uiHandle);

    return EResult::Success;
}

void Application::Free()
{
    ImGuiManager::Destroy();

    if (m_Runtime) m_Runtime->Destroy();

    if(m_Window) SDL_DestroyWindow(m_Window);
    SDL_Quit();
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
            if (event.type == SDL_EVENT_QUIT) {
                bIsRunning = false;
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