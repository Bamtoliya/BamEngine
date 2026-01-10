#pragma once
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <fmt/core.h>
#include "ImGuiManager.h"

IMPLEMENT_SINGLETON(ImGuiManager)

#pragma region Constructor&Destructor
EResult ImGuiManager::Initialize(void* arg)
{
	IMGUISDLDESC* pDesc = reinterpret_cast<IMGUISDLDESC*>(arg);
	m_Window = pDesc->Window;
	m_Renderer = pDesc->Renderer;

	if (!m_Window || !m_Renderer) return EResult::Fail;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	if (!ImGui_ImplSDL3_InitForSDLRenderer(m_Window, m_Renderer))
	{
		fmt::print(stderr, "ImGui_ImplSDL3_InitForSDLRenderer Failed\n");
		return EResult::Fail;
	}

	if (!ImGui_ImplSDLRenderer3_Init(m_Renderer))
	{
		fmt::print(stderr, "ImGui_ImplSDLRenderer3_Init Failed\n");
		return EResult::Fail;
	}

	return EResult::Success;
}

void ImGuiManager::Free()
{
	// [Future RHI] 교체 포인트
	ImGui_ImplSDLRenderer3_Shutdown();

	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}
#pragma endregion

#pragma region

void ImGuiManager::Begin()
{
	// [Future RHI] 교체 포인트
	ImGui_ImplSDLRenderer3_NewFrame();

	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}

void ImGuiManager::End()
{
	// UI 렌더링 데이터 생성
	ImGui::Render();

	// 실제 그리기 명령 수행
	// [Future RHI] 나중에 이 부분은 Engine::Renderer::FlushUI(GetDrawData()) 같은 형태로 바뀝니다.
	if (m_Renderer)
	{
		ImGuiIO& io = ImGui::GetIO();

		// [SDL3] 중요: 렌더러의 현재 스케일을 저장해둡니다 (State Save)
		float oldScaleX, oldScaleY;
		SDL_GetRenderScale(m_Renderer, &oldScaleX, &oldScaleY);

		// [SDL3] ImGui용 스케일 적용 (High DPI 대응)
		// 함수명 변경됨: SDL_RenderSetScale -> SDL_SetRenderScale
		SDL_SetRenderScale(m_Renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

		// 그리기
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), m_Renderer);

		// [SDL3] 렌더러 스케일 복구 (State Restore)
		// 이걸 안 하면 다음 프레임 게임 화면이 이 스케일의 영향을 받습니다.
		SDL_SetRenderScale(m_Renderer, oldScaleX, oldScaleY);
	}

	// [Multi-Viewport 처리]
	// 창 밖으로 UI를 뺐을 때, 별도의 OS 윈도우를 그려주는 처리
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
		SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
	}
}

void ImGuiManager::ProcessEvent(const SDL_Event* event)
{
	ImGui_ImplSDL3_ProcessEvent(event);
}

#pragma endregion