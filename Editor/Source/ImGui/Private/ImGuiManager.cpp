#pragma once
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <fmt/core.h>
#include "ImGuiManager.h"
#include "Application.h"
#include "RHI.h"

IMPLEMENT_SINGLETON(ImGuiManager)

#pragma region Constructor&Destructor
EResult ImGuiManager::Initialize(void* arg)
{
	IMGUISDLDESC* pDesc = reinterpret_cast<IMGUISDLDESC*>(arg);
	m_Window = pDesc->Window;
	m_RHI = pDesc->RHI;

	if (!m_Window || !m_RHI) return EResult::Fail;

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

	if (!ImGui_ImplSDL3_InitForSDLRenderer(m_Window, nullptr))
	{
		fmt::print(stderr, "ImGui_ImplSDL3_InitForSDLRenderer Failed\n");
		return EResult::Fail;
	}

	SDL_Renderer* NativeRenderer = reinterpret_cast<SDL_Renderer*>(m_RHI->GetNativeRHI());

	if (!NativeRenderer)
	{
		fmt::print(stderr, "ImGuiManager Initialize Failed: NativeRenderer is nullptr\n");
		return EResult::Fail;
	}

	if (!ImGui_ImplSDLRenderer3_Init(NativeRenderer))
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
	if (m_RHI)
	{
		SDL_Renderer* NativeRenderer = reinterpret_cast<SDL_Renderer*>(m_RHI->GetNativeRHI());
		ImGuiIO& io = ImGui::GetIO();

		// [SDL3] 중요: 렌더러의 현재 스케일을 저장해둡니다 (State Save)
		float oldScaleX, oldScaleY;
		SDL_GetRenderScale(NativeRenderer, &oldScaleX, &oldScaleY);

		// [SDL3] ImGui용 스케일 적용 (High DPI 대응)
		// 함수명 변경됨: SDL_RenderSetScale -> SDL_SetRenderScale
		SDL_SetRenderScale(NativeRenderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);

		// 그리기
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), NativeRenderer);

		// [SDL3] 렌더러 스케일 복구 (State Restore)
		// 이걸 안 하면 다음 프레임 게임 화면이 이 스케일의 영향을 받습니다.
		SDL_SetRenderScale(NativeRenderer, oldScaleX, oldScaleY);
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

void ImGuiManager::Draw()
{
	MainDockspace();
	if (ImGui::Begin("Display Settings")) // 창 이름
	{
		static int width = g_WindowWidth;
		static int height = g_WindowHeight;
		static bool isFullscreen = false;

		// 현재 상태 가져오기 (처음 한 번만 동기화하거나, 매번 갱신하거나 선택)
		// 여기서는 UI 조작 값을 우선하도록 단순화했습니다.

		ImGui::Text("Resolution");
		ImGui::InputInt("Width", &width);
		ImGui::InputInt("Height", &height);

		ImGui::Checkbox("Fullscreen", &isFullscreen);

		if (ImGui::Button("Apply Resolution"))
		{
			// 실제 적용
			Application::Get().SetResolution(width, height, isFullscreen);
		}

		ImGui::Separator();

		// [옵션] UI 스케일 조절 (4K 모니터 대응용)
		static float uiScale = 1.0f;
		if (ImGui::DragFloat("UI Scale", &uiScale, 0.01f, 0.5f, 3.0f))
		{
			ImGui::GetIO().FontGlobalScale = uiScale; // 간단한 스케일링 방법
		}
	}
	ImGui::End();

	ImGui::ShowDemoWindow();
}

void ImGuiManager::MainDockspace()
{
	// [1. Fullscreen DockSpace 생성] ----------------------------------
	// 메인 뷰포트 전체를 덮는 "DockSpace" 윈도우를 만듭니다.
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);

	// 스타일: 라운딩 없음, 테두리 없음, 패딩 없음
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	// 플래그: 제목 표시줄 없음, 이동/크기조절 불가, 뒤로 보내기(배경역할), 메뉴바 포함
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground; // 투명하게 해서 뒤의 게임 화면이 보이게 할 수도 있음 (선택사항)

	// 메인 독스페이스 컨테이너 시작
	ImGui::Begin("MainDockSpace", nullptr, window_flags);

	// 스타일 복구
	ImGui::PopStyleVar(3);

	// DockSpace ID 제출
	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	// PassthruCentralNode: 중앙 노드를 투명하게 만들어서, 도킹되지 않은 영역에 게임 씬을 그릴 수 있게 함
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);


	// [2. Main Toolbar (Menu Bar) 생성] --------------------------------
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Settings"))
			{
				// ConfigManager::Get().SaveSettings();
			}
			if (ImGui::MenuItem("Exit", "Alt+F4"))
			{
				// SDL Quit 이벤트 발생 시킴
				SDL_Event quit_event;
				quit_event.type = SDL_EVENT_QUIT;
				SDL_PushEvent(&quit_event);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			ImGui::MenuItem("Display Settings", nullptr); // 체크박스 연동 가능
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	ImGui::End(); // MainDockSpace 끝 (이제부터 그리는 창은 이 안에 도킹됨)
}

#pragma endregion