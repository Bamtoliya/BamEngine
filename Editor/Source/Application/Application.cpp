#pragma once
#include <fmt/core.h>
#include <SDL3/SDL.h>
#include "Application.h"
#include "TimeManager.h"
#include "ImGuiManager.h"

BEGIN(Editor)

IMPLEMENT_SINGLETON(Application)

#pragma region Constructor&Destructor
EResult Application::Initialize(void* arg)
{
	m_Runtime = Runtime::Create(arg);

	if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fmt::print(stderr, "SDL_Init Failed: {}\n", SDL_GetError());
        return EResult::Fail;
    }

    m_Window = SDL_CreateWindow(
        "BamEngine Editor",
        1280, 720,
        0 // 기본 윈도우 플래그 (SDL_WINDOW_RESIZABLE 등 필요시 추가)
	);

	m_Renderer = SDL_CreateRenderer(m_Window, NULL);

	if (!m_Window || !m_Renderer) return EResult::Fail;

	ImGuiManager::IMGUISDLDESC imguiDesc = {};
	imguiDesc.Window = m_Window;
	imguiDesc.Renderer = m_Renderer;

    if (!ImGuiManager::Create(&imguiDesc))
    {
        fmt::print(stderr, "ImGuiManager Creation Failed\n");
		return EResult::Fail;
    }

    return EResult::Success;
}

void Application::Free()
{
    ImGuiManager::Get().Destroy();

    if(m_Renderer)  SDL_DestroyRenderer(m_Renderer);
    if(m_Window) SDL_DestroyWindow(m_Window);
    SDL_Quit();

    if (m_Runtime) m_Runtime->Destroy();
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
		m_Runtime->RunFrame(dt);

        SDL_SetRenderDrawColor(m_Renderer, 50, 50, 50, 255);
        SDL_RenderClear(m_Renderer);
        m_Runtime->Render();

        ImGuiManager::Get().Begin();
        {
            // 원하는 UI 코드 작성
            ImGui::ShowDemoWindow();
        }
        ImGuiManager::Get().End();

        SDL_RenderPresent(m_Renderer);
    }
}

void Application::Shutdown()
{
    Free();
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

END