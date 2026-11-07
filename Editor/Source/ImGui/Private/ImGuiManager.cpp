#pragma once
#include "InputManager.h"
#include "ShortcutManager.h"
#include <archive/JsonArchive.h>
#include "EditorSettingsService.h"
#include "ModelImportModal.h"

#pragma region Third Library
#include <algorithm>
#include <fmt/core.h>
#include <utility>
#pragma endregion

#include "PlatformPath.h"
#include "ImGuiManager.h"
#include "Application.h"
#include "SceneManager.h"
#include "SelectionManager.h"
#include "RHIs.h"
#include "PIEManager.h"

#include "ImGuiPanels.h"
#include "NotificationManager.h"
#include "ResourceEditors.h"
#include "ResourceManager.h"
#include "ResourceEditorInterface.h"
#include "TerrainEditor.h"

#include "DirectX12Texture.h"
#include "SceneDocument.h"
#include "ConfigManager.h"
#include "PreviewViewportWidget.h"

#pragma region Imgui Header
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "imgui_impl_sdlgpu3.h"
#include "imgui_impl_dx12.h"
#pragma push_macro("COLOR")
#undef COLOR

#include "ImGuizmo.h"

#pragma pop_macro("COLOR")
#pragma endregion

IMPLEMENT_SINGLETON(ImGuiManager)

#pragma region Constructor&Destructor
EResult ImGuiManager::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;

	CAST_DESC
	if (!desc->Window || !desc->RHI)
		return EResult::InvalidArgument;

	auto fail = [this](const char* stage)
	{
		fmt::print(stderr, "ImGuiManager initialization failed: {}\n", stage);
		Free();
		return EResult::Fail;
	};

	m_Window = desc->Window;
	m_RHI = desc->RHI;
	Safe_AddRef(m_RHI);
	m_RHIType = Renderer::Get().GetRHIType();

	IMGUI_CHECKVERSION();
	m_ImGuiContext = ImGui::CreateContext();
	if (!m_ImGuiContext) return fail("CreateContext");

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	// 1. 시스템 폰트 디렉터리 동적 취득
	std::filesystem::path fontPath = Engine::PlatformPath::GetSystemFontDirectory() / "malgun.ttf";
	float fontSize = 18.0f; // 기본 크기
	if (EditorSettingsService::IsCreated())
	{
		const auto& prefs = EditorSettingsService::Get().GetPreferences();
		const auto& customFont = prefs.CustomFontPath;
		if (!customFont.empty() && std::filesystem::exists(customFont))
			fontPath = customFont;
		if (prefs.FontSize >= 10.0f && prefs.FontSize <= 48.0f)
			fontSize = prefs.FontSize; // 설정된 폰트 크기 반영
	}
	const auto projectRoot = Engine::Runtime::IsCreated() ? Engine::Runtime::Get().GetProjectRoot() : Engine::PlatformPath::GetExecutableDirectory();
	vector<std::filesystem::path> iconsFontPaths;
	iconsFontPaths.push_back(projectRoot / "Resources/Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Solid-900.otf");
	iconsFontPaths.push_back(projectRoot / "Resources/Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Regular-400.otf");

	
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

		ImFontConfig fontConfig;
		fontConfig.OversampleH = 1;
		fontConfig.OversampleV = 1;
		fontConfig.PixelSnapH = true;
		// UTF-8 문자열로 변환하여 ImGui에 전달 (한글 경로 완벽 보장)
		const auto utf8FontPath = fontPath.u8string();
		io.Fonts->AddFontFromFileTTF(reinterpret_cast<const char*>(utf8FontPath.c_str()), fontSize, &fontConfig, ranges.Data);
	}
	else
	{
		fmt::print(stderr, "Font file not found: {}\n", fontPath.string());
	}
	for (const auto& filePath : iconsFontPaths)
	{
		if (std::filesystem::exists(filePath))
		{
			ImFontConfig iconsConfig;
			iconsConfig.MergeMode = true;
			iconsConfig.PixelSnapH = true;
			static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
			const auto utf8IconPath = filePath.u8string();
			io.Fonts->AddFontFromFileTTF(reinterpret_cast<const char*>(utf8IconPath.c_str()), 16.0f, &iconsConfig, icons_ranges);
		}
		else
		{
			fmt::print(stderr, "Icons font file not found: {}\n", filePath.string());
		}
	}

	// [2. 스타일 적용 함수 호출]
	SetCustomStyle();
	m_BaseStyle = ImGui::GetStyle();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	if (InitializeImGui() != EResult::Success)
		return fail("InitializeImGui");

	if (CreateDefaultPanels() != EResult::Success)
		return fail("CreateDefaultPanels");

	m_DocumentController = std::make_unique<EditorDocumentController>(Application::Get().GetDocuments());
	EditorActionRegistry::RegisterAll();

	return EResult::Success;
}

EResult ImGuiManager::InitializeImGui()
{
	switch (m_RHIType)
	{
	case ERHIType::SDLGPU:
		return InitializeImGuiSDLGPU3();
	case ERHIType::DirectX12:
		return InitializeImGuiDirectX12();
	default:
		return EResult::Fail;
	}
	return EResult::Success;
}

void ImGuiManager::Free()
{
	ReleaseFrameTextures();
	if (m_ImGuiContext)
		ImGui::SetCurrentContext(m_ImGuiContext);
	m_WorkspacePanels.clear();
	m_PendingRemovePanels.clear();

	for (auto& panel : m_PendingAddPanels)
		Safe_Release(panel);
	m_PendingAddPanels.clear();

	for (auto& panel : m_ImGuiPanels)
		Safe_Release(panel);
	m_ImGuiPanels.clear();

	if (m_RendererBackendInitialized)
	{
		switch (m_RHIType)
		{
		case ERHIType::SDLGPU:
			ShutdownImGuiSDLGPU3();
			break;
		case ERHIType::DirectX12:
			ShutdownImGuiDirectX12();
			break;
		default:
			break;
		}
		m_RendererBackendInitialized = false;
	}

	if (m_PlatformBackendInitialized)
	{
		ImGui_ImplSDL3_Shutdown();
		m_PlatformBackendInitialized = false;
	}

	if (m_ImGuiOwnsSrvHeap && m_ImGuiSrvDescHeap)
		m_ImGuiSrvDescHeap->Release();
	m_ImGuiSrvDescHeap = nullptr;
	m_ImGuiOwnsSrvHeap = false;
	m_ImGuiSrvDescriptorSize = 0;

	if (m_ImGuiContext)
	{
		ImGui::DestroyContext(m_ImGuiContext);
		m_ImGuiContext = nullptr;
	}

	Safe_Release(m_RHI);
	m_Window = nullptr;
	m_RHIType = ERHIType::Unknown;
}

#pragma endregion

#pragma region Frame

void ImGuiManager::Update(f32 dt)
{
	auto& settings = EditorSettingsService::Get();
	settings.ProcessPendingViewportResolutionChange();

	for (auto& panel : m_ImGuiPanels)
	{
		if (!panel->IsOpen() && !panel->ShouldUpdateWhenClosed())
			continue;

		panel->Update(dt);
	}

	settings.CompleteViewportResolutionChange();
}

void ImGuiManager::Begin()
{
	// Apply language/style changes before any ImGui window is submitted for this frame.
	m_ToolBar.ApplyPendingLanguageChange();
	ReleaseFrameTextures();
	// [Future RHI] 교체 포인트
	switch (m_RHIType)
	{
	case Engine::ERHIType::SDLGPU:
		SDLGPU3Begin();
		break;
	case Engine::ERHIType::DirectX12:
		DirectX12Begin();
		break;
	case Engine::ERHIType::Vulkan:
	case Engine::ERHIType::Metal:
	default:
		break;
	}

	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	PreviewViewportWidget::BeginShortcutFrame();
	ImGuizmo::BeginFrame();
}

void ImGuiManager::End()
{
	// UI 렌더링 데이터 생성
	ImGui::Render();
	switch (m_RHIType)
	{
	case Engine::ERHIType::SDLGPU:
		SDLGPU3End();
		break;
	case Engine::ERHIType::DirectX12:
		DirectX12End();
		break;
	case Engine::ERHIType::Vulkan:
	case Engine::ERHIType::Metal:
	default:
		break;
	}
	
}

void ImGuiManager::ProcessEvent(const SDL_Event* event)
{
	ImGui_ImplSDL3_ProcessEvent(event);
	for (auto& panel : m_ImGuiPanels)
	{
		panel->ProcessEvent(event);
	}
}

#pragma endregion

#pragma region DrawUI
void ImGuiManager::Draw()
{
	auto& history = Application::Get().GetEditHistory();
	auto& pieManager = PIEManager::Get();
	history.Synchronize(SceneManager::Get().GetCurrentScene(), PIEManager::Get().CanEdit());
	SynchronizeSceneDocumentAccess();
	history.BeginFrame();
	DrawDocumentConfirmation();
	MainDockspace();
	history.Synchronize(SceneManager::Get().GetCurrentScene(), PIEManager::Get().CanEdit());

	//ImGui::ShowDemoWindow(nullptr); // 데모 윈도우 (테스트용, 나중에 제거)

	for (auto& panel : m_ImGuiPanels)
	{
		panel->Draw();
	}

	for (auto* panel : m_PendingAddPanels)
        m_ImGuiPanels.push_back(panel);
    m_PendingAddPanels.clear();
	for (auto* panel : m_PendingRemovePanels)
	{
		auto it = std::find(m_ImGuiPanels.begin(), m_ImGuiPanels.end(), panel);
		if (it != m_ImGuiPanels.end())
		{
			m_ImGuiPanels.erase(it);
			Safe_Release(panel);
		}
	}
	m_PendingRemovePanels.clear();
	OpenPendingResourceEditor();
	ImGui::ShowMetricsWindow();
	SynchronizeSceneDocumentAccess();
	if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing() &&
		!InputManager::Get().IsMouseButtonPressed(EMouseButton::Left))
	{
		history.Flush();

		const bool suppressed = ImGui::GetIO().WantTextInput || m_DocumentController->HasPendingAction() ||
			EditorSettingsService::Get().IsWindowChangePending() || ShortcutManager::Get().IsCapturing() ||
			ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
		EShortcutContext activeContext = EShortcutContext::Global;
		for (const auto* panel : m_ImGuiPanels)
		{
			if (panel && panel->IsOpen() && panel->IsFocused())
			{
				const auto ctx = panel->GetShortcutContext();
				if (ctx != EShortcutContext::Global)
				{
					activeContext = ctx;
					break;
				}
			}
		}
		ShortcutManager::Get().ProcessShortcuts(activeContext, suppressed);

		if (const auto result = m_DocumentController->ProcessPendingSave())
		{
			auto& application = Application::Get();

			if (result->Status == EDocumentSaveStatus::Saved)
			{
				const auto* document = application.GetDocuments().Find(result->Id);

				if (document && result->Id == application.GetSceneDocumentId())
					application.RememberEditedScene(document->GetPath());

				const auto name = document && !document->GetPath().empty()
					? WStrToStr(document->GetPath().wstring()) : LOCAL("EditorDocument.Untitled");

				NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
					LOCAL_FMT("EditorDocument.Saved", name), ELogLevel::Info);
			}
			else if (result->Status == EDocumentSaveStatus::Failed)
			{
				NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
					LOCAL("EditorDocument.SaveFailed"), ELogLevel::Error);
			}
			else if (result->Status == EDocumentSaveStatus::Unavailable)
			{
				NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
					LOCAL("EditorDocument.SaveUnavailable"), ELogLevel::Warn);
			}

			m_DocumentController->NotifySaveResult(*result);
		}

		if (const auto result = m_DocumentController->ProcessPendingAction())
		{
			if (*result != EResult::Success)
			{
				BAM_LOG(Error, "Editor", "Document", "Document action failed: {}", ResultToString(*result));
				NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
					LOCAL("EditorDocument.OperationFailed"), ELogLevel::Error);
			}
		}
	}

	NotificationManager::Get().DrawNotificationUI(m_ToolBar.GetFooterBarHeight());
}

void ImGuiManager::MainDockspace()
{
	// [1. Fullscreen DockSpace 생성] ----------------------------------
	// 메인 뷰포트 전체를 덮는 "DockSpace" 윈도우를 만듭니다.
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float topReserve = m_ToolBar.GetPlayBarHeight();
	const float bottomReserve = m_ToolBar.GetFooterBarHeight();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + topReserve));
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, std::max(1.0f, viewport->WorkSize.y - topReserve - bottomReserve)));
	ImGui::SetNextWindowViewport(viewport->ID);

	// 스타일: 라운딩 없음, 테두리 없음, 패딩 없음
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	// 플래그: 제목 표시줄 없음, 이동/크기조절 불가, 뒤로 보내기(배경역할), 메뉴바 포함
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	window_flags |= ImGuiWindowFlags_NoBackground; // 투명하게 해서 뒤의 게임 화면이 보이게 할 수도 있음 (선택사항)
	//window_flags |= ImGuiWindowFlags_MenuBar;

	// 메인 독스페이스 컨테이너 시작
	ImGui::Begin("##MainDockSpace", nullptr, window_flags);

	// 스타일 복구
	ImGui::PopStyleVar(3);

	// DockSpace ID 제출
	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	// PassthruCentralNode: 중앙 노드를 투명하게 만들어서, 도킹되지 않은 영역에 게임 씬을 그릴 수 있게 함
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

	m_ToolBar.Draw(m_Window);

	ImGui::End(); // MainDockSpace 끝 (이제부터 그리는 창은 이 안에 도킹됨)
}
#pragma endregion

#pragma region ImGui Management
namespace
{
	bool InitializePanel(IImGui* panel, void* arg = nullptr)
	{
		const EResult result = panel->Initialize(arg);
		if (result == EResult::Success)
			return true;

		EDITOR_LOG_ERROR("Panel initialization failed: panel={}, result={}", WStrToStr(panel->GetName()),
			ResultToString(result));
		Safe_Release(panel);
		return false;
	}
}

EResult ImGuiManager::AddImGuiPanel(IImGui* panel)
{
	if (!panel)
		return EResult::InvalidArgument;

	if (std::find(m_ImGuiPanels.begin(), m_ImGuiPanels.end(), panel) != m_ImGuiPanels.end() ||
		std::find(m_PendingAddPanels.begin(), m_PendingAddPanels.end(), panel) != m_PendingAddPanels.end())
		return EResult::AlreadyInitialized;

	m_PendingAddPanels.push_back(panel);
	return EResult::Success;
}

EResult ImGuiManager::RemoveImGuiPanel(IImGui* panel)
{
	if (!panel)
		return EResult::InvalidArgument;

	std::erase(m_WorkspacePanels, panel);
	m_PendingRemovePanels.push_back(panel);
	return EResult::Success;
}
IImGui* ImGuiManager::GetImGuiPanel(const wstring& name)
{
	auto it = std::find_if(m_ImGuiPanels.begin(), m_ImGuiPanels.end(),
		[&name](IImGui* panel) { return panel->GetName() == name; });
	if (it != m_ImGuiPanels.end())
	{
		return *it;
	}
	return nullptr;
}
vector<IImGui*> ImGuiManager::GetImGuiPanels(const wstring& name)
{
	vector<IImGui*> panels;
	for (const auto& panel : m_ImGuiPanels)
	{
		if (panel->GetName() == name)
		{
			panels.push_back(panel);
		}
	}
	return panels;
}
ResourceEditorInterface* ImGuiManager::FindResourceEditor(const std::filesystem::path& path) const
{
	for (auto* panel : m_ImGuiPanels)
	{
		auto* editor = dynamic_cast<ResourceEditorInterface*>(panel);
		if (editor && editor->IsSupported(path))
			return editor;
	}
	return nullptr;
}

std::filesystem::path ImGuiManager::ResolveResourceEditorTarget(const std::filesystem::path& path) const
{
	if (path.empty())
		return {};

	std::error_code ec;

	// 1. 이미 존재하는 정규 파일인지 확인
	auto direct = Engine::PlatformPath::ResolveProjectRelative(path);
	if (std::filesystem::is_regular_file(direct, ec))
		return direct;

	// 2. Resources/ 접두사가 누락된 경우 확인
	const bool startsWithResources = (path.native().rfind(L"Resources", 0) == 0) ||
		(path.generic_wstring().rfind(L"Resources/", 0) == 0);
	if (!startsWithResources)
	{
		auto withRes = Engine::PlatformPath::ResolveProjectRelative(std::filesystem::path("Resources") / path);
		if (std::filesystem::is_regular_file(withRes, ec))
			return withRes;
	}

	// 3. 확장자가 생략되었거나 다른 확장자로 디스크에 존재하는 경우 (.bammat.json, .bammatinst.json 등)
	std::vector<std::filesystem::path> bases;
	bases.push_back(direct);
	if (!startsWithResources)
		bases.push_back(Engine::PlatformPath::ResolveProjectRelative(std::filesystem::path("Resources") / path));

	for (const auto& base : bases)
	{
		const wchar_t* extensions[] = {
			L".json",
			L".bammat.json", L".bammat",
			L".bammatinst.json", L".bammatinst", L".mat",
			L".bammodel.json", L".bammodel",
			L".bamtex.json", L".bamtex",
			L".bamsprite.json", L".bamsprite",
			L".bamshader.json", L".bamshader",
			L".bamprefab.json", L".bamprefab",
			L".png", L".jpg", L".jpeg", L".dds", L".tga", L".bmp", L".hdr",
			L".fbx", L".obj", L".gltf", L".glb"
		};
		for (const wchar_t* ext : extensions)
		{
			auto cand = std::filesystem::path(base.wstring() + ext);
			if (std::filesystem::is_regular_file(cand, ec))
				return cand;
		}

		auto candJson = std::filesystem::path(base.wstring() + L".json");
		if (std::filesystem::is_regular_file(candJson, ec))
			return candJson;

		// 메쉬(.bammesh / .bammesh.json)인 경우: 같은 디렉터리의 부모 .bammodel 탐색
		const auto extStr = Engine::ToLowerStr(Engine::WStrToStr(base.extension().wstring()));
		const auto filenameStr = Engine::ToLowerStr(Engine::WStrToStr(base.filename().wstring()));
		if (extStr == ".bammesh" || filenameStr.ends_with(".bammesh.json"))
		{
			auto stem = base.stem().wstring();
			if (filenameStr.ends_with(".bammesh.json"))
			{
				std::filesystem::path p(stem);
				stem = p.stem().wstring();
			}

			auto candModel1 = base.parent_path() / (stem + L".bammodel");
			if (std::filesystem::is_regular_file(candModel1, ec)) return candModel1;
			auto candModel2 = base.parent_path() / (stem + L".bammodel.json");
			if (std::filesystem::is_regular_file(candModel2, ec)) return candModel2;

			auto underscorePos = stem.find(L'_');
			if (underscorePos != std::wstring::npos)
			{
				auto modelStem = stem.substr(0, underscorePos);
				auto candModel3 = base.parent_path() / (modelStem + L".bammodel");
				if (std::filesystem::is_regular_file(candModel3, ec)) return candModel3;
				auto candModel4 = base.parent_path() / (modelStem + L".bammodel.json");
				if (std::filesystem::is_regular_file(candModel4, ec)) return candModel4;
				auto candModel5 = base.parent_path() / (modelStem + L".glb");
				if (std::filesystem::is_regular_file(candModel5, ec)) return candModel5;
				auto candModel6 = base.parent_path() / (modelStem + L".fbx");
				if (std::filesystem::is_regular_file(candModel6, ec)) return candModel6;
			}
		}
	}

	// 4. ResourceManager에서 키로 등록된 리소스의 실제 경로 탐색
	if (Engine::ResourceManager::IsCreated())
	{
		auto& rm = Engine::ResourceManager::Get();
		auto handle = rm.FindHandleByKey(path.wstring());
		if (handle.IsValid())
		{
			auto* res = rm.GetResource(handle);
			if (res && !res->GetPath().empty() && res->GetPath() != path.wstring())
			{
				auto recursiveTarget = ResolveResourceEditorTarget(res->GetPath());
				if (!recursiveTarget.empty())
					return recursiveTarget;
			}
		}
	}

	return {};
}

bool ImGuiManager::CanOpenResourceEditor(const std::filesystem::path& path) const
{
	if (path.empty())
		return false;

	const auto target = ResolveResourceEditorTarget(path);
	const auto effectivePath = target.empty() ? Engine::PlatformPath::ResolveProjectRelative(path) : target;
	std::error_code error;
	return std::filesystem::is_regular_file(effectivePath, error) && FindResourceEditor(effectivePath) != nullptr;
}

bool ImGuiManager::RequestOpenResourceEditor(const std::filesystem::path& path)
{
	const auto target = ResolveResourceEditorTarget(path);
	const auto effectivePath = target.empty() ? Engine::PlatformPath::ResolveProjectRelative(path) : target;
	if (!CanOpenResourceEditor(effectivePath))
		return false;

	SelectionManager::Get().SetSelectedAsset(effectivePath);
	m_PendingResourceEditorPath = effectivePath;
	return true;
}

void ImGuiManager::OpenPendingResourceEditor()
{
	if (m_PendingResourceEditorPath.empty())
		return;

	const auto path = std::exchange(m_PendingResourceEditorPath, {});
	auto* editor = FindResourceEditor(path);
	if (!editor)
		return;

	const auto& target = editor->GetTargetResource();
	std::error_code error;
	const auto current = Engine::PlatformPath::ResolveProjectRelative(target);
	const bool sameTarget = !target.empty() && (current == path || std::filesystem::equivalent(current, path, error));

	if (!sameTarget)
	{
		if (editor->HasUnsavedResourceChanges())
		{
			NotificationManager::Get().AddNotification(LOCAL("ResourcePicker.OpenInEditor"),
				LOCAL_FMT("ResourceEditor.OpenBlocked", Engine::WStrToStr(target.filename().wstring())), ELogLevel::Warn);
			return;
		}

		editor->SetTargetResource(path);
	}

	editor->Focus();
	if (auto* window = ImGui::FindWindowByID(editor->m_PanelWindowId))
		ImGui::FocusWindow(window, ImGuiFocusRequestFlags_UnlessBelowModal);
}

EResult ImGuiManager::CreateDefaultPanels()
{
	SceneViewportPanelDesc scenePanelDesc;
	scenePanelDesc.Name = L"Scene View";
	scenePanelDesc.DisplayNameKey = "Viewport.SceneTitle";
	scenePanelDesc.RenderTargetWidth = g_WindowWidth;
	scenePanelDesc.RenderTargetHeight = g_WindowHeight;
	scenePanelDesc.IsSceneCamera = false;
	scenePanelDesc.IsPerspective = true;
	scenePanelDesc.CameraType = EViewportCameraType::Orthographic;
	if (!InitializePanel(new SceneViewportPanel(), &scenePanelDesc))
		return EResult::Fail;

	ViewportPanelDesc UIViewportPanelDesc;
	UIViewportPanelDesc.Name = L"UI View";
	UIViewportPanelDesc.DisplayNameKey = "Viewport.UITitle";
	UIViewportPanelDesc.RenderTargetWidth = g_WindowWidth;
	UIViewportPanelDesc.RenderTargetHeight = g_WindowHeight;
	if (!InitializePanel(new UIViewportPanel(), &UIViewportPanelDesc))
		return EResult::Fail;

	GameViewportPanelDesc gamePanelDesc;
	gamePanelDesc.Name = L"Game View";
	gamePanelDesc.DisplayNameKey = "Viewport.GameTitle";
	gamePanelDesc.RenderTargetWidth = g_WindowWidth;
	gamePanelDesc.RenderTargetHeight = g_WindowHeight;
	if (!InitializePanel(new GameViewportPanel(), &gamePanelDesc))
		return EResult::Fail;

	if (!InitializePanel(new InspectorPanel()) ||
		!InitializePanel(new HierarchyPanel()) ||
		!InitializePanel(new ContentBrowserPanel()) ||
		!InitializePanel(new ConsolePanel()) ||
		!InitializePanel(new MetricsPanel()) ||
		!InitializePanel(new GraphPanel()) ||
		!InitializePanel(new ResourceManagerInspector()) ||
		!InitializePanel(new TerrainEditor()) ||
		!InitializePanel(new PostProcessPanel()) ||
		!InitializePanel(new AnimatorGraphPanel()) ||
		!InitializePanel(new HistoryPanel()) ||
		!InitializePanel(new UnifiedSettingsPanel()) ||
		!InitializePanel(new AssetReferencePanel()) ||
		!InitializePanel(new AnimationTimelinePanel()) ||
		!InitializePanel(new NetworkProfilerPanel()) ||
		!InitializePanel(new ModelImportModal()))
		return EResult::Fail;

	if (const EResult result = CreateResourceEditors(); result != EResult::Success)
		return result;

	m_WorkspacePanels = m_PendingAddPanels;
	return EResult::Success;
}
EResult ImGuiManager::CreateResourceEditors()
{
	const auto initializeClosed = [](IImGui* panel)
	{
		panel->Close();
		return InitializePanel(panel);
	};

	if (!initializeClosed(new TextureEditor()) ||
		!initializeClosed(new ModelEditor()) ||
		!initializeClosed(new ShaderEditor()) ||
		!initializeClosed(new SpriteEditor()) ||
		!initializeClosed(new SpriteAnimationEditor()) ||
		!initializeClosed(new MaterialEditor()) ||
		!initializeClosed(new PrefabEditorPanel()))
		return EResult::Fail;

	return EResult::Success;
}
#pragma endregion

#pragma region Style
void ImGuiManager::SetUIScale(float scale)
{
	if (!std::isfinite(scale))
		return;
	m_UIScale = std::clamp(scale, 0.5f, 3.0f);
	ImGuiStyle& style = ImGui::GetStyle();
	style = m_BaseStyle;
	style.ScaleAllSizes(m_UIScale);
	float fontSize = 18.0f;
	if (EditorSettingsService::IsCreated())
		fontSize = EditorSettingsService::Get().GetPreferences().FontSize;
	const float fontRatio = (fontSize >= 10.0f && fontSize <= 48.0f) ? (fontSize / 18.0f) : 1.0f;
	ImGui::GetIO().FontGlobalScale = m_UIScale * fontRatio;
}
void ImGuiManager::UpdateFontScale(float fontSize)
{
	const float baseSize = 18.0f;
	const float fontRatio = (fontSize >= 10.0f && fontSize <= 48.0f) ? (fontSize / baseSize) : 1.0f;
	ImGui::GetIO().FontGlobalScale = m_UIScale * fontRatio; // 실시간 폰트 크기 즉각 반영
}
void ImGuiManager::SetCustomStyle()
{
	ImGuiStyle newStyle;                  // 1. 항상 1.0f 기준의 깨끗한 새 스타일 인스턴스로 시작
	ImGui::StyleColorsDark(&newStyle);    // 2. ImGui 기본 다크 색상으로 전체 초기화
	// 레이아웃 수치 정의 (1.0f 기본 배율 기준)
	newStyle.WindowRounding = 5.0f;
	newStyle.FrameRounding = 4.0f;
	newStyle.PopupRounding = 4.0f;
	newStyle.GrabRounding = 4.0f;
	newStyle.TabRounding = 4.0f;
	newStyle.WindowPadding = ImVec2(8.0f, 8.0f);
	newStyle.FramePadding = ImVec2(5.0f, 3.0f);
	newStyle.ItemSpacing = ImVec2(8.0f, 4.0f);
	newStyle.ScrollbarSize = 12.0f;
	// BamEngine 고유 Modern Dark 색상 팔레트 적용
	ImVec4* colors = newStyle.Colors;
	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.12f, 0.13f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_Header] = ImVec4(0.22f, 0.29f, 0.41f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.11f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.13f, 0.14f, 0.16f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.21f, 0.22f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.18f, 0.22f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.18f, 0.22f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.52f, 0.88f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	m_BaseStyle = newStyle;   // 1.0f 기준 순수 스타일을 원본으로 저장
	SetUIScale(m_UIScale);    // 현재 UIScale에 맞춰 1회만 정확하게 스케일링
}
std::filesystem::path ImGuiManager::GetCustomStylePath() const
{
	if (EditorSettingsService::IsCreated())
	{
		const auto& prefPath = EditorSettingsService::Get().GetPreferencesPath();
		if (!prefPath.empty())
			return prefPath.parent_path() / "EditorStyle.json";
	}
	return std::filesystem::path();
}

void ImGuiManager::ApplyDefaultDarkStyle()
{
	SetCustomStyle();
}

void ImGuiManager::ApplyTheme(const std::string& theme)
{
	if (theme == "Light")
	{
		ImGuiStyle newStyle;
		ImGui::StyleColorsLight(&newStyle);
		newStyle.WindowRounding = 5.0f;
		newStyle.FrameRounding = 4.0f;
		newStyle.PopupRounding = 4.0f;
		newStyle.WindowPadding = ImVec2(8.0f, 8.0f);
		newStyle.FramePadding = ImVec2(5.0f, 3.0f);
		newStyle.ItemSpacing = ImVec2(8.0f, 4.0f);
		m_BaseStyle = newStyle;
		SetUIScale(m_UIScale);
	}
	else if (theme == "Classic")
	{
		ImGuiStyle newStyle;
		ImGui::StyleColorsClassic(&newStyle);
		newStyle.WindowRounding = 5.0f;
		newStyle.FrameRounding = 4.0f;
		newStyle.PopupRounding = 4.0f;
		newStyle.WindowPadding = ImVec2(8.0f, 8.0f);
		newStyle.FramePadding = ImVec2(5.0f, 3.0f);
		newStyle.ItemSpacing = ImVec2(8.0f, 4.0f);
		m_BaseStyle = newStyle;
		SetUIScale(m_UIScale);
	}
	else if (theme == "Custom")
	{
		const auto customPath = GetCustomStylePath();
		if (customPath.empty() || !LoadCustomStyle(customPath))
			ApplyDefaultDarkStyle();
	}
	else
	{
		ApplyDefaultDarkStyle();
	}
}

bool ImGuiManager::SaveCustomStyle(const std::filesystem::path& path)
{
	if (path.empty())
		return false;

	const ImGuiStyle& style = ImGui::GetStyle();
	archive::JsonArchive writer;

	const float invScale = (m_UIScale > 0.0f) ? (1.0f / m_UIScale) : 1.0f;
	(void)writer.Write("WindowRounding", style.WindowRounding * invScale);
	(void)writer.Write("ChildRounding", style.ChildRounding * invScale);
	(void)writer.Write("FrameRounding", style.FrameRounding * invScale);
	(void)writer.Write("PopupRounding", style.PopupRounding * invScale);
	(void)writer.Write("ScrollbarRounding", style.ScrollbarRounding * invScale);
	(void)writer.Write("GrabRounding", style.GrabRounding * invScale);
	(void)writer.Write("TabRounding", style.TabRounding * invScale);

	(void)writer.Write("WindowPaddingX", style.WindowPadding.x * invScale);
	(void)writer.Write("WindowPaddingY", style.WindowPadding.y * invScale);
	(void)writer.Write("FramePaddingX", style.FramePadding.x * invScale);
	(void)writer.Write("FramePaddingY", style.FramePadding.y * invScale);
	(void)writer.Write("ItemSpacingX", style.ItemSpacing.x * invScale);
	(void)writer.Write("ItemSpacingY", style.ItemSpacing.y * invScale);

	(void)writer.Write("WindowBorderSize", style.WindowBorderSize);
	(void)writer.Write("FrameBorderSize", style.FrameBorderSize);
	(void)writer.Write("PopupBorderSize", style.PopupBorderSize);
	(void)writer.Write("ScrollbarSize", style.ScrollbarSize * invScale);
	(void)writer.Write("GrabMinSize", style.GrabMinSize * invScale);

	if (writer.BeginWriteObject("Colors"))
	{
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			const char* name = ImGui::GetStyleColorName(static_cast<ImGuiCol>(i));
			const ImVec4& c = style.Colors[i];
			const std::string rgba = fmt::format("{:.4f},{:.4f},{:.4f},{:.4f}", c.x, c.y, c.z, c.w);
			(void)writer.Write(name, rgba);
		}
		(void)writer.EndObject();
	}

	const auto saveResult = Engine::ConfigManager::SaveDocument(writer, path);
	if (!saveResult)
	{
		BAM_LOG(Error, "Editor", "Style", "Failed to save custom style to {}", path.string());
		return false;
	}

	m_BaseStyle = style;
	BAM_LOG(Info, "Editor", "Style", "Saved custom style to {}", path.string());
	return true;
}

bool ImGuiManager::LoadCustomStyle(const std::filesystem::path& path)
{
	if (path.empty() || !std::filesystem::exists(path))
		return false;
	archive::JsonArchive reader;
	const auto loadResult = reader.LoadFromFile(path);
	if (!loadResult)
	{
		BAM_LOG(Warn, "Editor", "Style", "Failed to load custom style from {}", path.string());
		return false;
	}
	ImGuiStyle unscaledStyle; // 깨끗한 1.0f 인스턴스에 먼저 로드
	float fVal = 0.0f;
	if (reader.Read("WindowRounding", fVal)) unscaledStyle.WindowRounding = fVal;
	if (reader.Read("ChildRounding", fVal)) unscaledStyle.ChildRounding = fVal;
	if (reader.Read("FrameRounding", fVal)) unscaledStyle.FrameRounding = fVal;
	if (reader.Read("PopupRounding", fVal)) unscaledStyle.PopupRounding = fVal;
	if (reader.Read("ScrollbarRounding", fVal)) unscaledStyle.ScrollbarRounding = fVal;
	if (reader.Read("GrabRounding", fVal)) unscaledStyle.GrabRounding = fVal;
	if (reader.Read("TabRounding", fVal)) unscaledStyle.TabRounding = fVal;
	if (reader.Read("WindowPaddingX", fVal)) unscaledStyle.WindowPadding.x = fVal;
	if (reader.Read("WindowPaddingY", fVal)) unscaledStyle.WindowPadding.y = fVal;
	if (reader.Read("FramePaddingX", fVal)) unscaledStyle.FramePadding.x = fVal;
	if (reader.Read("FramePaddingY", fVal)) unscaledStyle.FramePadding.y = fVal;
	if (reader.Read("ItemSpacingX", fVal)) unscaledStyle.ItemSpacing.x = fVal;
	if (reader.Read("ItemSpacingY", fVal)) unscaledStyle.ItemSpacing.y = fVal;
	if (reader.Read("WindowBorderSize", fVal)) unscaledStyle.WindowBorderSize = fVal;
	if (reader.Read("FrameBorderSize", fVal)) unscaledStyle.FrameBorderSize = fVal;
	if (reader.Read("PopupBorderSize", fVal)) unscaledStyle.PopupBorderSize = fVal;
	if (reader.Read("ScrollbarSize", fVal)) unscaledStyle.ScrollbarSize = fVal;
	if (reader.Read("GrabMinSize", fVal)) unscaledStyle.GrabMinSize = fVal;
	if (reader.BeginReadObject("Colors"))
	{
		for (int i = 0; i < ImGuiCol_COUNT; ++i)
		{
			const char* name = ImGui::GetStyleColorName(static_cast<ImGuiCol>(i));
			std::string rgbaStr;
			if (reader.Read(name, rgbaStr))
			{
				float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
				if (sscanf_s(rgbaStr.c_str(), "%f,%f,%f,%f", &r, &g, &b, &a) == 4)
					unscaledStyle.Colors[i] = ImVec4(r, g, b, a);
			}
		}
		(void)reader.EndObject();
	}
	m_BaseStyle = unscaledStyle; // 1.0f 원본으로 저장
	SetUIScale(m_UIScale);       // m_UIScale(1.25)을 정확히 1번만 적용
	BAM_LOG(Info, "Editor", "Style", "Loaded custom style from {}", path.string());
	return true;
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

	m_PlatformBackendInitialized = true;

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
	m_RendererBackendInitialized = true;

	return EResult::Success;
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
	if (!m_RHI) return EResult::Fail;

	DirectX12RHI* dx12RHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12Device* device = static_cast<ID3D12Device*>(dx12RHI->GetNativeRHI());

	if (!ImGui_ImplSDL3_InitForD3D(m_Window))
	{
		fmt::print(stderr, "ImGui_ImplSDL3_InitForD3D Failed\n");
		return EResult::Fail;
	}

	m_PlatformBackendInitialized = true;

	// [핵심] ImGui 전용 heap 생성 금지, 엔진 SRV heap 공유
	auto* srvAllocator = dx12RHI->GetSRVAllocator();
	if (!srvAllocator || !srvAllocator->GetHeap())
	{
		fmt::print(stderr, "DX12 SRV allocator/heap is null\n");
		return EResult::Fail;
	}

	m_ImGuiSrvDescHeap = srvAllocator->GetHeap();
	m_ImGuiOwnsSrvHeap = false;
	m_ImGuiSrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	IM_ASSERT(m_ImGuiSrvDescriptorSize > 0);

	ImGui_ImplDX12_InitInfo initInfo = {};
	initInfo.Device = device;
	initInfo.CommandQueue = dx12RHI->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(m_RHI->GetFrameResourceCount());;
	initInfo.SrvDescriptorHeap = m_ImGuiSrvDescHeap;
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
	initInfo.UserData = this;

	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu)
		{
			ImGuiManager* self = static_cast<ImGuiManager*>(info->UserData);
			IM_ASSERT(self && self->m_RHI);

			auto* dx12RHI = static_cast<DirectX12RHI*>(self->m_RHI);
			auto* allocator = dx12RHI->GetSRVAllocator();
			IM_ASSERT(allocator);

			uint32 index = 0;
			if (!allocator->Allocate(*out_cpu, index))
			{
				IM_ASSERT(false && "SRV allocator is out of descriptors.");
				*out_cpu = {};
				*out_gpu = {};
				return;
			}

			*out_gpu = allocator->GetGPUHandle(index);
		};

	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			ImGuiManager* self = static_cast<ImGuiManager*>(info->UserData);
			IM_ASSERT(self && self->m_RHI);

			auto* dx12RHI = static_cast<DirectX12RHI*>(self->m_RHI);
			auto* allocator = dx12RHI->GetSRVAllocator();
			IM_ASSERT(allocator);

			auto* heap = allocator->GetHeap();
			D3D12_CPU_DESCRIPTOR_HANDLE start = heap->GetCPUDescriptorHandleForHeapStart();

			if (cpu_desc.ptr < start.ptr || self->m_ImGuiSrvDescriptorSize == 0)
				return;

			SIZE_T delta = cpu_desc.ptr - start.ptr;
			uint32 index = static_cast<uint32>(delta / self->m_ImGuiSrvDescriptorSize);
			dx12RHI->DeferDescriptorFree(allocator, index);
		};

	if (!ImGui_ImplDX12_Init(&initInfo))
	{
		fmt::print(stderr, "ImGui_ImplDX12_Init Failed\n");
		return EResult::Fail;
	}

	m_RendererBackendInitialized = true;

	return EResult::Success;
}
void ImGuiManager::DirectX12Begin()
{
	ImGui_ImplDX12_NewFrame();
}
void ImGuiManager::DirectX12End()
{
	DirectX12RHI* dx12RHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12GraphicsCommandList* commandList = dx12RHI->GetCommandList();

	DirectX12Texture* backBuffer = static_cast<DirectX12Texture*>(m_RHI->GetBackBuffer());
	if (backBuffer && backBuffer->GetCurrentState() != D3D12_RESOURCE_STATE_RENDER_TARGET)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = static_cast<ID3D12Resource*>(backBuffer->GetNativeHandle());
		barrier.Transition.StateBefore = backBuffer->GetCurrentState();
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		commandList->ResourceBarrier(1, &barrier);
		backBuffer->SetCurrentState(D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = backBuffer->GetRTVHandle();
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// ImGui용 Descriptor Heap 바인딩
	ID3D12DescriptorHeap* heaps[] = { m_ImGuiSrvDescHeap };
	commandList->SetDescriptorHeaps(1, heaps);
	// ImGui 그리기 명령을 커맨드 리스트에 기록
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	// 다중 뷰포트(도킹 창 분리) 처리를 위한 업데이트 
	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, (void*)commandList);
	}
}
void ImGuiManager::ShutdownImGuiDirectX12()
{
	auto* dxRHI = static_cast<Engine::DirectX12RHI*>(m_RHI);
	if (!dxRHI || Engine::IsFailure(dxRHI->WaitForGPU()))
	{
		fmt::print(stderr, "GPU synchronization failed before ImGui DX12 shutdown.\n");
		std::terminate();
	}

	ImGui_ImplDX12_Shutdown();
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

#pragma region Metal Helper
EResult ImGuiManager::InitializeImGuiMetal()
{
	return EResult();
}
void ImGuiManager::ShutdownImGuiMetal()
{
}
#pragma endregion

#pragma region Helper
ImTextureID ImGuiManager::GetImGuiTextureID(Engine::RHITexture* texture)
{
	if (texture == nullptr) return ImTextureID{};

	ImTextureID textureID{};

	switch (m_RHIType)
	{
	case ERHIType::SDLGPU:
		textureID = (ImTextureID)texture->GetNativeHandle();
		break;
	case ERHIType::DirectX12:
	{
		const auto* dxTexture = static_cast<const Engine::DirectX12Texture*>(texture);
		textureID = dxTexture->GetSRVGPUHandle().ptr;
		break;
	}
	default:
		break;
	}

	if (textureID != ImTextureID{} && m_FrameTextures.insert(texture).second)
		texture->AddRef();

	return textureID;
}
void ImGuiManager::ReleaseFrameTextures()
{
	for (auto* texture : m_FrameTextures)
		texture->Release();

	m_FrameTextures.clear();
}
#pragma endregion

#pragma region Settings
void ImGuiManager::NotifySettingsChange(const EditorSettingsChangeResult& result)
{
	const char* key = "EditorSettings.ApplyFailed";
	auto level = ELogLevel::Error;

	if (result.Applied && result.SaveStatus == EEditorSettingsSaveStatus::Saved)
	{
		key = "EditorSettings.Saved";
		level = ELogLevel::Info;
	}
	else if (result.Applied)
	{
		key = result.SaveStatus == EEditorSettingsSaveStatus::SessionOnly
			? "EditorSettings.SessionOnly" : "EditorSettings.SaveFailed";
		level = ELogLevel::Warn;
	}

	NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL(key), level);
}
#pragma endregion

#pragma region Workspace
void ImGuiManager::RestoreWorkspace(const EditorWorkspace& workspace)
{
	if (!m_ImGuiContext)
		return;

	auto* previousContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_ImGuiContext);

	if (!workspace.ImGuiLayout.empty())
		ImGui::LoadIniSettingsFromMemory(workspace.ImGuiLayout.data(), workspace.ImGuiLayout.size());

	for (auto* panel : m_WorkspacePanels)
		panel->RestoreState(workspace);

	ImGui::SetCurrentContext(previousContext);
}

void ImGuiManager::CaptureWorkspace(EditorWorkspace& workspace)
{
	if (!m_ImGuiContext)
		return;

	auto* previousContext = ImGui::GetCurrentContext();
	ImGui::SetCurrentContext(m_ImGuiContext);

	for (const auto* panel : m_WorkspacePanels)
		panel->CaptureState(workspace);

	std::size_t size = 0;
	const char* layout = ImGui::SaveIniSettingsToMemory(&size);

	if (layout)
		workspace.ImGuiLayout.assign(layout, size);

	ImGui::SetCurrentContext(previousContext);
}
#pragma endregion

#pragma region Document
bool ImGuiManager::RequestNewScene()
{
	return RequestSceneAction(ESceneDocumentAction::New);
}

bool ImGuiManager::RequestOpenScene(const std::filesystem::path& path)
{
	return RequestSceneAction(ESceneDocumentAction::Open, path);
}

bool ImGuiManager::RequestCloseScene()
{
	return RequestSceneAction(ESceneDocumentAction::Close);
}

bool ImGuiManager::RequestSceneAction(ESceneDocumentAction action, std::filesystem::path path)
{
	if (!PIEManager::Get().IsEditing())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.SceneActionUnavailable"), ELogLevel::Warn);
		return false;
	}

	if (m_DocumentController->IsBusy())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.OperationBusy"), ELogLevel::Warn);
		return false;
	}

	if (action == ESceneDocumentAction::Close && !SceneManager::Get().GetCurrentScene())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.NoSceneLoaded"), ELogLevel::Warn);
		return false;
	}

	if (action == ESceneDocumentAction::Open)
	{
		std::error_code error;
		path = std::filesystem::absolute(path, error);
		if (error)
		{
			BAM_LOG(Error, "Editor", "Document", "Cannot resolve scene path: {}", error.message());

			NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
				LOCAL("EditorDocument.OperationFailed"), ELogLevel::Error);
			return false;
		}

		path = path.lexically_normal();
	}

	auto& application = Application::Get();
	std::vector<EditorDocumentId> documents;

	if (application.GetSceneDocumentId() != InvalidEditorDocumentId)
		documents.push_back(application.GetSceneDocumentId());

	return m_DocumentController->RequestAction(std::move(documents),
		[action, path = std::move(path)](const std::vector<EditorDocumentId>&) -> EResult
		{
			if (!PIEManager::Get().IsEditing())
				return EResult::AccessDenied;

			auto& scenes = SceneManager::Get();
			EResult result = EResult::Fail;

			switch (action)
			{
			case ESceneDocumentAction::New:
				result = scenes.NewScene();
				break;
			case ESceneDocumentAction::Open:
			{
				archive::JsonArchive reader;
				result = scenes.LoadScene(reader, path.wstring());
				break;
			}
			case ESceneDocumentAction::Close:
				result = scenes.CloseScene();
				break;
			}

			if (result != EResult::Success)
				return result;

			SelectionManager::Get().ClearSelection(true);
			Application::Get().AdoptSceneDocument(
				action == ESceneDocumentAction::Open ? path : std::filesystem::path{});

			std::string message;

			if (action == ESceneDocumentAction::Open)
				message = LOCAL_FMT("EditorDocument.SceneOpened", WStrToStr(path.wstring()));
			else if (action == ESceneDocumentAction::New)
				message = LOCAL("EditorDocument.SceneCreated");
			else
				message = LOCAL("EditorDocument.SceneClosed");

			NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"), message, ELogLevel::Info);

			return EResult::Success;
		});
}

bool ImGuiManager::RequestQuit()
{
	if (m_DocumentController->IsBusy())
		return false;

	if (!PIEManager::Get().IsEditing())
	{
		PIEManager::Get().Stop();
		if (!PIEManager::Get().IsEditing())
		{
			NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
				LOCAL("EditorDocument.OperationFailed"), ELogLevel::Error);
			return false;
		}
	}

	auto& application = Application::Get();

	return m_DocumentController->RequestAction(application.GetDocuments().GetDocumentIds(),
		[](const std::vector<EditorDocumentId>&) -> EResult
		{
			Application::Get().CompleteQuitRequest();
			return EResult::Success;
		});
}
bool ImGuiManager::PreparePendingSceneSave()
{
	return false;
}
void ImGuiManager::DrawDocumentConfirmation()
{
	auto& controller = *m_DocumentController;
	const auto title = LOCAL("EditorDocument.ConfirmTitle") + "###DocumentActionConfirm";

	if (controller.HasPendingAction() && !ImGui::IsPopupOpen(title.c_str()))
		ImGui::OpenPopup(title.c_str());

	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	if (!controller.HasPendingAction())
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	const auto id = controller.GetConfirmationDocumentId();
	if (id == InvalidEditorDocumentId)
	{
		ImGui::TextUnformatted(LOCAL_CSTR("EditorDocument.Processing"));
		ImGui::EndPopup();
		return;
	}

	const auto* document = Application::Get().GetDocuments().Find(id);
	if (!document)
	{
		controller.CancelForDocument(id);
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	const auto name = document->GetPath().empty()
		? LOCAL("EditorDocument.Untitled") : WStrToStr(document->GetPath().wstring());

	ImGui::TextUnformatted(LOCAL_FMT("EditorDocument.UnsavedMessage", name).c_str());
	ImGui::Spacing();

	ImGui::BeginDisabled(!document->CanSave());
	if (ImGui::Button(LOCAL_LABEL("EditorDocument.Save").c_str()))
		controller.ResolveConfirmation(EDocumentActionChoice::Save);
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("EditorDocument.Discard").c_str()))
		controller.ResolveConfirmation(EDocumentActionChoice::Discard);

	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("EditorCommon.Cancel").c_str()))
	{
		controller.ResolveConfirmation(EDocumentActionChoice::Cancel);
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}
void ImGuiManager::SynchronizeSceneDocumentAccess()
{
	auto& application = Application::Get();

	if (auto* document = application.GetSceneDocument())
	{
		const bool readOnly = !PIEManager::Get().IsEditing();
		document->SetReadOnly(readOnly);

		if (readOnly)
			m_DocumentController->CancelForDocument(application.GetSceneDocumentId());
	}
}
#pragma endregion

#pragma region MyRegion
IImGui* ImGuiManager::GetFocusedClosablePanel() const
{
	const auto* context = ImGui::GetCurrentContext();
	if (!context || !context->NavWindow)
		return nullptr;

	const auto* window = context->NavWindow->RootWindow;
	if (window->LastFrameActive != ImGui::GetFrameCount() || !window->HasCloseButton)
		return nullptr;

	for (auto* panel : m_ImGuiPanels)
	{
		if (panel && panel->IsOpen() && panel->m_PanelWindowId == window->ID)
			return panel;
	}

	return nullptr;
}

void ImGuiManager::RequestCloseFocusedPanel()
{
	if (auto* panel = GetFocusedClosablePanel())
		panel->m_CloseRequestFrame = ImGui::GetFrameCount() + 1;
}
#pragma endregion
