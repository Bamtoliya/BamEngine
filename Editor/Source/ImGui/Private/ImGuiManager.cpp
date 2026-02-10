#pragma once

#pragma region Imgui Header
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_sdlgpu3.h"
#include "ImGuizmo.h"
#pragma endregion

#pragma region Third Library
#include <fmt/core.h>
#pragma endregion

#include "ImGuiManager.h"
#include "Application.h"
#include "RHI.h"

IMPLEMENT_SINGLETON(ImGuiManager)

#pragma region Constructor&Destructor
EResult ImGuiManager::Initialize(void* arg)
{
	CAST_DESC
	m_Window = desc->Window;
	m_RHI = desc->RHI;
	Safe_AddRef(m_RHI);
	m_RHIType = Renderer::Get().GetRHIType();

	if (!m_Window || !m_RHI) return EResult::Fail;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	std::string fontPath = "C:\\Windows\\Fonts\\malgun.ttf";
	vector<string> iconsFontPaths;

	iconsFontPaths.push_back("Resources/Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Regular-400.otf");
	iconsFontPaths.push_back("Resources\\Fonts\\fontawesome-free-7.1.0-desktop\\otfs\\Font Awesome 7 Free-Solid-900.otf");

	
	if (std::filesystem::exists(fontPath))
	{
		// static으로 선언하여 메모리 유지
		static ImVector<ImWchar> ranges;

		// [중요] 이미 만들어졌으면 다시 만들지 않음 (중복 방지 & 성능 최적화)
		if (ranges.empty())
		{
			ImFontGlyphRangesBuilder builder;

			// 1. 기본 한글 (완성형)
			builder.AddRanges(io.Fonts->GetGlyphRangesKorean());
			// 2. 영문 및 특수기호
			builder.AddRanges(io.Fonts->GetGlyphRangesDefault());

			// 3. [핵심] 한글 자음/모음(Jamo) 범위 직접 추가
			// 'ㄱ', 'ㅏ' 등 조합 중인 상태가 이 범위에 해당합니다. (0x3131 ~ 0x3163)
			static const ImWchar ranges_jamo[] = { 0x3131, 0x3163, 0 };
			builder.AddRanges(ranges_jamo);

			builder.BuildRanges(&ranges);
		}

		// 폰트 로드
		io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, nullptr, ranges.Data);
	}
	else
	{
		fmt::print(stderr, "Font file not found: {}\n", fontPath);
	}

	for (auto& filePath : iconsFontPaths)
	{
		if (filesystem::exists(filePath))
		{
			// 아이콘 폰트 로드
			ImFontConfig iconsConfig;
			iconsConfig.MergeMode = true;
			iconsConfig.PixelSnapH = true;
			static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
			io.Fonts->AddFontFromFileTTF(filePath.c_str(), 16.0f, &iconsConfig, icons_ranges);
		}
		else
		{
			fmt::print(stderr, "Icons font file not found: {}\n", filePath);
		}
	}

	// [2. 스타일 적용 함수 호출]
	SetCustomStyle();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	if (IsFailure(InitializeImGui()))
	{
		fmt::print(stderr, "ImGuiManager InitializeImGui Failed\n");
		return EResult::Fail;
	}	

	return EResult::Success;
}

EResult ImGuiManager::InitializeImGui()
{
	switch (m_RHIType)
	{
	case ERHIType::SDLRenderer:
		return InitializeImGuiSDLRenderer3();
	case ERHIType::SDLGPU:
		return InitializeImGuiSDLGPU3();
	default:
		break;
	}
	return EResult::Success;
}

void ImGuiManager::Free()
{
	Safe_Release(m_RHI);
	switch (m_RHIType)
	{
	case ERHIType::SDLRenderer:
		ShutdownImGuiSDLRenderer3();
		break;
	case ERHIType::SDLGPU:
		ShutdownImGuiSDLGPU3();
		break;
	default:
		break;
	}
	

	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}
#pragma endregion

#pragma region Frame

void ImGuiManager::Begin()
{
	// [Future RHI] 교체 포인트
	switch (m_RHIType)
	{
	case Engine::ERHIType::SDLRenderer:
		SDLRenderer3Begin();
		break;
	case Engine::ERHIType::SDLGPU:
		SDLGPU3Begin();
		break;
	case Engine::ERHIType::D3D11:
		break;
	case Engine::ERHIType::D3D12:
		break;
	case Engine::ERHIType::Vulkan:
		break;
	case Engine::ERHIType::OpenGL:
		break;
	case Engine::ERHIType::Metal:
		break;
	default:
		break;
	}

	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();
}

void ImGuiManager::End()
{
	// UI 렌더링 데이터 생성
	ImGui::Render();
	switch (m_RHIType)
	{
	case Engine::ERHIType::SDLRenderer:
		SDLRenderer3End();
		break;
	case Engine::ERHIType::SDLGPU:
		SDLGPU3End();
		break;
	case Engine::ERHIType::D3D11:
		break;
	case Engine::ERHIType::D3D12:
		break;
	case Engine::ERHIType::Vulkan:
		break;
	case Engine::ERHIType::OpenGL:
		break;
	case Engine::ERHIType::Metal:
		break;
	default:
		break;
	}
	
}

void ImGuiManager::ProcessEvent(const SDL_Event* event)
{
	ImGui_ImplSDL3_ProcessEvent(event);
}

#pragma endregion

#pragma region DrawUI

void ImGuiManager::Draw()
{
	MainDockspace();

	ImGui::ShowDemoWindow(nullptr); // 데모 윈도우 (테스트용, 나중에 제거)

	m_HierarchyPanel.Draw();
	m_InspectorPanel.Draw();
	m_ViewportPanel.Draw();
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

	m_ToolBar.Draw();

	ImGui::End(); // MainDockSpace 끝 (이제부터 그리는 창은 이 안에 도킹됨)
}


#pragma endregion

#pragma region Style
void ImGuiManager::SetCustomStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	// 1. 레이아웃 수치 조정 (둥글게, 여백 넉넉하게)
	style.WindowRounding = 5.0f;    // 창 모서리 둥글기
	style.FrameRounding = 4.0f;     // 입력 필드 둥글기
	style.PopupRounding = 4.0f;     // 팝업 둥글기
	style.GrabRounding = 4.0f;      // 슬라이더 손잡이 둥글기
	style.TabRounding = 4.0f;       // 탭 둥글기

	style.WindowPadding = ImVec2(8.0f, 8.0f);
	style.FramePadding = ImVec2(5.0f, 3.0f);
	style.ItemSpacing = ImVec2(8.0f, 4.0f);
	style.ScrollbarSize = 12.0f;

	// 2. 색상 팔레트 설정 (Modern Dark Theme)
	// 기본 배경
	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.00f); // 아주 어두운 회색
	colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

	// 헤더 (Inspector 등의 제목)
	colors[ImGuiCol_Header] = ImVec4(0.22f, 0.29f, 0.41f, 1.00f);       // 약간 푸른빛 도는 회색
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f); // 호버 시 밝은 파랑
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);

	// 타이틀 바 (창 제목 표시줄)
	colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

	// 버튼
	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f); // 파란색 하이라이트
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);

	// 프레임 (InputText, Checkbox 배경)
	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);

	// 탭 (Docking 탭)
	colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.22f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.18f, 0.22f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);

	// 기타
	colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
}
#pragma endregion

#pragma region SDLRenderer3 Helper
EResult ImGuiManager::InitializeImGuiSDLRenderer3()
{

	SDL_Renderer* NativeRenderer = reinterpret_cast<SDL_Renderer*>(m_RHI->GetNativeRHI());

	if (!NativeRenderer)
	{
		fmt::print(stderr, "ImGuiManager Initialize Failed: NativeRenderer is nullptr\n");
		return EResult::Fail;
	}

	if (!ImGui_ImplSDL3_InitForSDLRenderer(m_Window, NativeRenderer))
	{
		fmt::print(stderr, "ImGui_ImplSDL3_InitForSDLRenderer Failed\n");
		return EResult::Fail;
	}

	if (!ImGui_ImplSDLRenderer3_Init(NativeRenderer))
	{
		fmt::print(stderr, "ImGui_ImplSDLRenderer3_Init Failed\n");
		return EResult::Fail;
	}

	return EResult();
}

void ImGuiManager::SDLRenderer3Begin()
{
	ImGui_ImplSDLRenderer3_NewFrame();
}

void ImGuiManager::SDLRenderer3End()
{
	// 실제 그리기 명령 수행
	// [Future RHI] 나중에 이 부분은 Engine::Renderer::FlushUI(GetDrawData()) 같은 형태로 바뀝니다.
	if (!m_RHI) return;
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

	// [Multi-Viewport 처리]
	// 창 밖으로 UI를 뺐을 때, 별도의 OS 윈도우를 그려주는 처리
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void ImGuiManager::ShutdownImGuiSDLRenderer3()
{
	ImGui_ImplSDLRenderer3_Shutdown();
}
#pragma endregion

#pragma region SDLGPU3 Helper
EResult ImGuiManager::InitializeImGuiSDLGPU3()
{

	SDL_GPUDevice* NativeRenderer = reinterpret_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

	if (!ImGui_ImplSDL3_InitForSDLGPU(m_Window))
	{
		fmt::print(stderr, "ImGui_ImplSDL3_InitForSDLGPU Failed\n");
		return EResult::Fail;
	}

	SDL_GPUTextureFormat swapchainFormat = SDL_GetGPUSwapchainTextureFormat(NativeRenderer, m_Window);

	ImGui_ImplSDLGPU3_InitInfo initInfo = {};
	initInfo.Device = NativeRenderer;
	initInfo.ColorTargetFormat = swapchainFormat;
	initInfo.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
	initInfo.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
	// [Multi-Viewport 처리] 멀티 뷰포트 모드일 때 스왑체인 구성 설정
	initInfo.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

	if (!ImGui_ImplSDLGPU3_Init(&initInfo))
	{
		fmt::print(stderr, "ImGui_ImplSDLGPU3_Init Failed\n");
		return EResult::Fail;
	}

	return EResult();
}
void ImGuiManager::SDLGPU3Begin()
{
	ImGui_ImplSDLGPU3_NewFrame();
}
void ImGuiManager::SDLGPU3End()
{
	if (!m_RHI) return;

	SDL_GPUDevice* device = reinterpret_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());
	SDL_GPUCommandBuffer* commandBuffer = static_cast<SDL_GPUCommandBuffer*>(m_RHI->GetCurrentCommandBuffer());
	ImDrawData* drawData = ImGui::GetDrawData();
	SDL_GPUTexture* backBufferTexture = static_cast<SDL_GPUTexture*>(m_RHI->GetBackBuffer()->GetNativeHandle());

	ImGui_ImplSDLGPU3_PrepareDrawData(drawData, commandBuffer);

	SDL_GPUColorTargetInfo colorTargetInfo = {};
	colorTargetInfo.texture = backBufferTexture;
	colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;   // 이미 그려진 화면 유지
	colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

	SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commandBuffer, &colorTargetInfo, 1, nullptr);
	
	ImGui_ImplSDLGPU3_RenderDrawData(drawData, commandBuffer, pass, nullptr);
	SDL_EndGPURenderPass(pass);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}
void ImGuiManager::ShutdownImGuiSDLGPU3()
{
	SDL_GPUDevice* device = reinterpret_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());
	SDL_WaitForGPUIdle(device);
	ImGui_ImplSDLGPU3_Shutdown();
	// [Multi-Viewport 처리] 모든 추가 창들을 닫음
}
#pragma endregion

#pragma region DirectX12 Helper
EResult ImGuiManager::InitializeImGuiDirectX12()
{
	return EResult();
}
void ImGuiManager::ShutdownImGuiDirectX12()
{
}
#pragma endregion

#pragma region Vulkan Helper
EResult ImGuiManager::InitializeImGuiVulkan()
{
	return EResult();
}
void ImGuiManager::ShutdownImGuiVulkan()
{
}
#pragma endregion

#pragma region OpenGL Helper
EResult ImGuiManager::InitializeImGuiOpenGL()
{
	return EResult();
}
void ImGuiManager::ShutdownImGuiOpenGL()
{
}
#pragma endregion