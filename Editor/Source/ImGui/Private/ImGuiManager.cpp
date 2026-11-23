#pragma once
#include "InputManager.h"
#include "EditorUI.h"
#include "AssetPathRelocation.h"
#include "EntityClipboard.h"
#include "ShortcutManager.h"
#include <archive/JsonArchive.h>
#include "EditorSettingsService.h"
#include "ModelImportModal.h"

#pragma region Third Library
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <fmt/core.h>
#include <fstream>
#include <map>
#include <thread>
#include <utility>
#pragma endregion

#include "PlatformPath.h"
#include "GraphSystem.h"
#include "SystemManager.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "PickerAssetCatalog.h"
#include "Application.h"
#include "SceneManager.h"
#include "SelectionManager.h"
#include "RHIs.h"
#include "PIEManager.h"

#include "ImGuiPanels.h"
#include "NotificationManager.h"
#include "ResourceEditors.h"
#include "ResourceManager.h"
#include "ResourceHandleArchiveAdapter.h"
#include "ResourcePreloader.h"
#include "ResourceStreamer.h"
#include "Scene.h"
#include "ResourceEditorInterface.h"
#include "AsyncResourceOpenService.h"
#include "PluginResourceEditor.h"
#include "TerrainEditor.h"
#include "TilePalettePanel.h"
#include "PluginManagerPanel.h"
#include "RecoveryManager.h"

#include "DirectX12Texture.h"
#include "SceneDocument.h"
#include "PrefabModeManager.h"
#include "ConfigManager.h"
#include "PreviewViewportWidget.h"
#include "EntityFactory.h"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "LightComponent.h"
#include "PostProcessComponent.h"
#include "PluginDiscovery.h"

#pragma region Imgui Header
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#pragma push_macro("COLOR")
#undef COLOR

#include "ImGuizmo.h"

#pragma pop_macro("COLOR")
#pragma endregion

BEGIN(Editor)
struct SceneLoadState
{
	enum class EStage
	{
		ReadingArchive,
		PreloadingResources,
		FinalizingScene
	};

	std::filesystem::path RequestedPath;
	std::filesystem::path ResolvedPath;
	std::string DisplayName;
	EStage Stage = EStage::ReadingArchive;
	std::chrono::steady_clock::time_point StartTime{};
	bool ModalVisible = false;
	bool FinalizingFrameDrawn = false;
	std::unique_ptr<archive::JsonArchive> Archive;
	archive::ArchiveResult ArchiveReadResult;
	std::vector<Engine::ResourceReference> Resources;
	size_t NextResourceIndex = 0;
	// 메시·텍스처를 작업 스레드에서 먼저 읽는다. 끝나면 나머지 리소스를 순서대로 로드한다.
	std::unique_ptr<Engine::ResourcePreloader> Preloader;
	bool Preloaded = false;

	// 로드 구간 계측. 리소스 시간은 메인 스레드에서 쓴 시간을 종류별로 합친 값이다.
	struct ResourceTiming
	{
		size_t Count = 0;
		double Milliseconds = 0.0;
	};
	double ArchiveMilliseconds = 0.0;
	double ResourceMilliseconds = 0.0;
	std::chrono::steady_clock::time_point PreloadStart{};
	double PreloadWallMilliseconds = 0.0;
	double PreparedWallMilliseconds = 0.0;
	Engine::RHIBufferUploadStats PreparedBufferBaseline;
	Engine::RHIBufferUploadStats PreparedBufferStats;
	std::map<std::string, ResourceTiming> ResourceTimings;
	std::vector<std::pair<double, std::string>> SlowestResources;

	void RecordResource(const Engine::ResourceReference& reference, double milliseconds)
	{
		const std::filesystem::path path(reference.AssetPath.empty() ? reference.AssetKey : reference.AssetPath);
		auto extension = path.extension();
		if (extension == L".json" || extension == L".beve")
		{
			if (const auto embedded = path.stem().extension(); !embedded.empty())
				extension = embedded;
		}
		auto& timing = ResourceTimings[extension.empty() ? std::string("(none)") : Engine::ToLowerStr(Engine::WStrToStr(extension.wstring()))];
		++timing.Count;
		timing.Milliseconds += milliseconds;
		ResourceMilliseconds += milliseconds;

		constexpr size_t SlowestCount = 5;
		SlowestResources.emplace_back(milliseconds, Engine::WStrToStr(path.wstring()));
		std::sort(SlowestResources.begin(), SlowestResources.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
		if (SlowestResources.size() > SlowestCount)
			SlowestResources.resize(SlowestCount);
	}

	std::atomic<bool> ArchiveReady = false;
	std::thread ArchiveThread;

	~SceneLoadState()
	{
		if (ArchiveThread.joinable())
			ArchiveThread.join();
	}
};
END

IMPLEMENT_SINGLETON(ImGuiManager)

ImGuiManager::ImGuiManager() = default;
ImGuiManager::~ImGuiManager() = default;

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
	io.ConfigWindowsMoveFromTitleBarOnly = true;

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
	vector<std::filesystem::path> iconsFontPaths;
	iconsFontPaths.push_back(Engine::PlatformPath::ResolveUri(L"Engine://Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Solid-900.otf"));
	iconsFontPaths.push_back(Engine::PlatformPath::ResolveUri(L"Engine://Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Regular-400.otf"));

	
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
	for (auto* panel : m_PluginResourceEditors)
		panel->RegisterSaveAction();

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
	AsyncResourceOpenService::Get().Shutdown();
	m_SceneLoadState.reset();
	ReleaseFrameTextures();
	if (m_ImGuiContext)
		ImGui::SetCurrentContext(m_ImGuiContext);
	m_WorkspacePanels.clear();
	m_PluginResourceEditors.clear();
	m_PendingRemovePanels.clear();

	for (auto& panel : m_PendingAddPanels)
		Safe_Release(panel);
	m_PendingAddPanels.clear();

	for (auto& panel : m_ImGuiPanels)
		Safe_Release(panel);
	m_ImGuiPanels.clear();
	m_PluginHost.Clear();

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
	AsyncResourceOpenService::Get().Update(0.002f);
	Editor::Detail::UpdatePickerAssetCatalog();
	m_PluginHost.Update(*this);
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
	if (PrefabModeManager::IsCreated()) PrefabModeManager::Get().DrawExitConfirmModal();
	DrawResourceQuitConfirmation();
	DrawDocumentConfirmation();
	DrawNewSceneDialog();
	if (Engine::ResourceStreamer::IsCreated() && EditorSettingsService::IsCreated())
	{
		const auto budget = EditorSettingsService::Get().GetProjectSettings().ResourceStreamingBudgetMB;
		Engine::ResourceStreamer::Get().SetBudgetMB(static_cast<uint32>((std::max)(budget, 0)));
	}
	UpdateSceneLoad();
	DrawSceneLoadingModal();
	// 시작 씬 로드가 끝나기 전에 복원하면 뒤늦은 로드가 복원 결과를 덮어쓴다.
	if (RecoveryManager::IsCreated() && !IsSceneLoading())
	{
		RecoveryManager::Get().DrawRecoveryModal();
	}
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
	// ImGui::ShowMetricsWindow(); // ImGui 내부 디버그 창: 상시 호출하지 않는다(필요 시 일시적으로 활성화)
	SynchronizeSceneDocumentAccess();
	if (!ImGui::IsAnyItemActive() && !ImGuizmo::IsUsing() &&
		!InputManager::Get().IsMouseButtonPressed(EMouseButton::Left))
	{
		history.Flush();

		const bool hasPopup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
		const bool blockedByEditorState = ImGui::GetIO().WantTextInput || m_DocumentController->HasPendingAction() ||
			IsSceneLoading() || EditorSettingsService::Get().IsWindowChangePending() || ShortcutManager::Get().IsCapturing();
		const bool suppressed = blockedByEditorState || hasPopup;
		EShortcutContext activeContext = EShortcutContext::Global;
		std::wstring_view activePanel;
		const auto* navWindow = ImGui::GetCurrentContext()->NavWindow;
		const auto* focusedWindow = navWindow ? navWindow->RootWindow : nullptr;

		for (const auto* panel : m_ImGuiPanels)
		{
			if (!panel || !panel->IsOpen() || !focusedWindow || panel->m_PanelWindowId != focusedWindow->ID)
				continue;

			activeContext = panel->GetShortcutContext();
			activePanel = panel->GetName();
			break;
		}

		// Panel draws can change focus before we evaluate the shortcuts for this frame.
		if (activeContext == EShortcutContext::ContentBrowser)
			history.BeginFrame();
		else
			history.AllowShortcuts(activeContext == EShortcutContext::Hierarchy);
		ShortcutManager::Get().ProcessShortcuts(activeContext, suppressed, activePanel);

		// 모달 팝업 안의 프리뷰 뷰포트가 호버·포커스되어 있으면, 액션 정의에서 허용한 프리뷰 단축키만 중앙 경로로 평가한다
		if (hasPopup && !blockedByEditorState && PreviewViewportWidget::GetShortcutTarget())
			ShortcutManager::Get().ProcessModalPreviewShortcuts();

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
	const float playBarBottom = m_ToolBar.Draw(m_Window);
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float dockBottom = viewport->WorkPos.y + viewport->WorkSize.y - m_ToolBar.GetFooterBarHeight();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, playBarBottom));
	ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x, std::max(1.0f, dockBottom - playBarBottom)));
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
	window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	// [수정] ImGuiWindowFlags_NoBackground를 제거하여 기본 창 배경색(ImGuiCol_WindowBg)이 칠해지도록 함

	ImGui::Begin("##MainDockSpace", nullptr, window_flags);
	ImGui::PopStyleVar(3);

	ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
	// [수정] ImGuiDockNodeFlags_PassthruCentralNode 대신 ImGuiDockNodeFlags_None 사용
	ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

	ImGui::End();
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

IImGui* ImGuiManager::FindRegisteredPanel(const wstring& id) const
{
	const auto matches = [&id](const IImGui* panel) { return panel->GetName() == id; };
	const auto active = std::find_if(m_ImGuiPanels.begin(), m_ImGuiPanels.end(), matches);
	if (active != m_ImGuiPanels.end())
		return *active;

	const auto pending = std::find_if(m_PendingAddPanels.begin(), m_PendingAddPanels.end(), matches);
	return pending != m_PendingAddPanels.end() ? *pending : nullptr;
}

EResult ImGuiManager::RegisterImGuiPanel(const wstring& id, IImGui* (*createPanel)())
{
	if (id.empty() || !createPanel)
		return EResult::InvalidArgument;
	if (FindRegisteredPanel(id))
		return EResult::AlreadyInitialized;

	auto* panel = createPanel();
	if (!panel)
		return EResult::Fail;
	if (panel->GetName() != id)
	{
		Safe_Release(panel);
		return EResult::InvalidArgument;
	}

	return InitializePanel(panel) ? EResult::Success : EResult::Fail;
}

EResult ImGuiManager::RemoveImGuiPanel(IImGui* panel)
{
	if (!panel)
		return EResult::InvalidArgument;

	const bool isActive = std::find(m_ImGuiPanels.begin(), m_ImGuiPanels.end(), panel) != m_ImGuiPanels.end();
	const bool isPendingAdd = std::find(m_PendingAddPanels.begin(), m_PendingAddPanels.end(), panel) !=
		m_PendingAddPanels.end();
	if (!isActive && !isPendingAdd)
		return EResult::InvalidArgument;

	if (std::find(m_PendingRemovePanels.begin(), m_PendingRemovePanels.end(), panel) != m_PendingRemovePanels.end())
		return EResult::Success;

	std::erase(m_WorkspacePanels, panel);
	if (auto* plugin = dynamic_cast<PluginResourceEditor*>(panel))
		std::erase(m_PluginResourceEditors, plugin);
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
	const bool sameTarget = editor->FocusRetainedResource(path) ||
		(!target.empty() && (current == path || std::filesystem::equivalent(current, path, error)));

	if (sameTarget)
	{
		if (editor->IsAsyncLoading())
		{
			AsyncResourceOpenService::Get().CancelRequestsForEditor(editor);
			editor->CancelAsyncOpen();
		}
	}
	else
	{
		const auto openTarget = [editor, path] {
			const auto started = std::chrono::steady_clock::now();
			if (!AsyncResourceOpenService::Get().RequestOpen(editor, path))
			{
				editor->SetTargetResource(path);
				const auto elapsed = std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - started).count();
				EDITOR_LOG_INFO("[ResourceOpenTiming] Editor {}: path={}, attempt={:.2f}ms",
					WStrToStr(editor->GetName()), WStrToStr(path.wstring()), elapsed);
			}
		};
		if (editor->HasUnsavedResourceChanges())
		{
			editor->Open();
			editor->Focus();
			if (auto* window = ImGui::FindWindowByID(editor->m_PanelWindowId))
				ImGui::FocusWindow(window, ImGuiFocusRequestFlags_UnlessBelowModal);

			editor->RunWithDiscardConfirmation(openTarget);
			return;
		}

		openTarget();
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
		!InitializePanel(new GraphPanel()) ||
		!InitializePanel(new ResourceManagerInspector()) ||
		!InitializePanel(new TerrainEditor()) ||
		!InitializePanel(new PostProcessPanel()) ||
		!InitializePanel(new AnimatorGraphPanel()) ||
		!InitializePanel(new HistoryPanel()) ||
		!InitializePanel(new UnifiedSettingsPanel()) ||
		!InitializePanel(new BuildProfilePanel()) ||
		!InitializePanel(new AssetReferencePanel()) ||
		!InitializePanel(new AnimationTimelinePanel()) ||
		!InitializePanel(new TilePalettePanel()) ||
		!InitializePanel(new ModelImportModal()))
		return EResult::Fail;

	if (const EResult result = CreateResourceEditors(); result != EResult::Success)
		return result;

	const auto& settings = EditorSettingsService::Get();
	m_PluginHost.Initialize(Engine::PlatformPath::GetExecutableDirectory(), settings.GetProjectRoot(),
		settings.GetPreferencesPath());
	m_PluginHost.Update(*this);

	if (!InitializePanel(new PluginManagerPanel()))
		return EResult::Fail;

	m_WorkspacePanels = m_PendingAddPanels;
	return EResult::Success;
}
EResult ImGuiManager::CreateResourceEditors()
{
	const auto initializeClosed = [](IImGui* panel)
	{
		panel->IImGui::SetOpen(false);
		return InitializePanel(panel);
	};

	if (!initializeClosed(new ModelEditor()) ||
		!initializeClosed(new ShaderEditor()) ||
		!initializeClosed(new TextureEditor()) ||
		!initializeClosed(new SpriteEditor()) ||
		!initializeClosed(new MaterialEditor()) ||
		!initializeClosed(new PrefabEditorPanel()) ||
		!initializeClosed(new ParticleEditorPanel()) ||
		!initializeClosed(new VFXEditorPanel()))
		return EResult::Fail;

	const auto catalog = Engine::PlatformPath::GetExecutableDirectory() / "EditorResourcePlugins.txt";
	for (const auto& entry : ReadPluginDevelopmentList(catalog))
	{
		auto* plugin = new PluginResourceEditor(entry.Target, entry.SourceDirectory);
		if (initializeClosed(plugin))
			m_PluginResourceEditors.push_back(plugin);
	}

	return EResult::Success;
}

PluginResourceEditor* ImGuiManager::GetPluginResourceEditor(std::string_view target) const
{
	const auto found = std::find_if(m_PluginResourceEditors.begin(), m_PluginResourceEditors.end(),
		[target](const PluginResourceEditor* panel) { return panel->GetTarget() == target; });
	return found != m_PluginResourceEditors.end() ? *found : nullptr;
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
	return SaveCustomStyle(path, ImGui::GetStyle(), m_UIScale);
}

bool ImGuiManager::SaveCustomStyle(const std::filesystem::path& path, const ImGuiStyle& style, float styleScale)
{
	if (path.empty() || !std::isfinite(styleScale) || styleScale <= 0.0f)
		return false;

	archive::JsonArchive writer;

	const float invScale = 1.0f / styleScale;
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

void ImGuiManager::ApplyCameraPreferences(float speed, float fov)
{
	for (auto* panel : m_ImGuiPanels)
	{
		if (auto* cameraPanel = dynamic_cast<CameraViewportPanel*>(panel))
			cameraPanel->ApplyCameraPreferences(speed, fov);
	}
	PreviewViewportWidget::ApplyGlobalCameraPreferences(speed, fov);
}

// 프리뷰 카메라는 대상 크기로 Near·Far를 자동 설정하므로 뷰포트 패널에만 적용한다.
void ImGuiManager::ApplyCameraClipPlanes(float nearPlane, float farPlane)
{
	for (auto* panel : m_ImGuiPanels)
	{
		auto* cameraPanel = dynamic_cast<CameraViewportPanel*>(panel);
		if (cameraPanel && !cameraPanel->SetCameraClipPlanes(nearPlane, farPlane))
			BAM_LOG(Warn, "Editor", "Viewport", "Camera clip planes could not be applied: near={}, far={}", nearPlane, farPlane);
	}
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
namespace
{
	void SetupBasicSceneEntities(Engine::Scene* scene)
	{
		if (!scene) return;

		// 1. Main Camera
		Entity& camEntity = EntityFactory::CreateEmptyEntity(scene, L"Main Camera");
		auto& cam = scene->AddComponent<CameraComponent>(camEntity);
		cam.CameraType = ECameraType::Perspective;
		cam.FOV = 60.0f;
		cam.NearPlane = 0.1f;
		cam.FarPlane = 1000.0f;
		cam.IsMainCamera = true;
		auto& camTrans = scene->GetComponent<TransformComponent>(camEntity);
		camTrans.position = vec3(0.0f, 1.3f, -4.5f);
		camTrans.eulerRotation = vec3(0.0f, 0.0f, 0.0f);
		camTrans.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);

		// 2. Directional Light
		Entity& lightEntity = EntityFactory::CreateLightEntity(scene, L"Directional Light", L"Directional");
		auto& lightTrans = scene->GetComponent<TransformComponent>(lightEntity);
		lightTrans.position = vec3(0.0f, 5.0f, -5.0f);
		lightTrans.eulerRotation = vec3(45.0f, -30.0f, 0.0f);
		lightTrans.rotation = glm::quat(glm::radians(lightTrans.eulerRotation));

		// 3. PostProcessVolume (Bloom & ToneMapping)
		Entity& ppEntity = EntityFactory::CreateEmptyEntity(scene, L"PostProcessVolume");
		auto& ppComp = scene->AddComponent<PostProcessComponent>(ppEntity);
		ppComp.IsEnabled = true;
		ppComp.IsUnbound = true;
		ppComp.PresetName = "DefaultPreset";
		ppComp.PipelineSettings = "{\n    \"Effects\": [\n        {\n            \"Enabled\": true,\n            \"Name\": \"Bloom\",\n            \"Parameters\": {\n                \"Intensity\": 1,\n                \"Scatter\": 0.699999988079071,\n                \"SoftKnee\": 0.5,\n                \"Threshold\": 0.800000011920929\n            }\n        },\n        {\n            \"Enabled\": true,\n            \"Name\": \"Tone Mapping (ACES)\",\n            \"Parameters\": {\n                \"Exposure\": 1,\n                \"Gamma\": 2.200000047683716\n            }\n        }\n    ]\n}";

		// 4. Ground Plane
		Entity& planeEntity = EntityFactory::CreatePrimitiveEntity(scene, L"Ground Plane", L"PlaneMesh", L"Engine://Material/DefaultMaterial.bammat.json");
		auto& planeTrans = scene->GetComponent<TransformComponent>(planeEntity);
		planeTrans.position = vec3(0.0f, 0.0f, 0.0f);
		planeTrans.scale = vec3(10.0f, 1.0f, 10.0f);
	}
}

bool ImGuiManager::RequestNewScene()
{
	if (!PIEManager::Get().IsEditing())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.SceneActionUnavailable"), ELogLevel::Warn);
		return false;
	}

	if (m_DocumentController->IsBusy() || IsSceneLoading())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.OperationBusy"), ELogLevel::Warn);
		return false;
	}

	m_ShowNewSceneDialog = true;
	return true;
}

bool ImGuiManager::ExecuteNewScene(ESceneTemplate templateType)
{
	if (!PIEManager::Get().IsEditing())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.SceneActionUnavailable"), ELogLevel::Warn);
		return false;
	}

	if (m_DocumentController->IsBusy() || IsSceneLoading())
	{
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.OperationBusy"), ELogLevel::Warn);
		return false;
	}

	auto& application = Application::Get();
	std::vector<EditorDocumentId> documents;

	if (application.GetSceneDocumentId() != InvalidEditorDocumentId)
		documents.push_back(application.GetSceneDocumentId());

	return m_DocumentController->RequestAction(std::move(documents),
		[templateType](const std::vector<EditorDocumentId>&) -> EResult
		{
			if (!PIEManager::Get().IsEditing())
				return EResult::AccessDenied;

			auto& scenes = SceneManager::Get();
			EResult result = scenes.NewScene();
			if (result != EResult::Success)
				return result;

			Scene* currentScene = scenes.GetCurrentScene();
			if (currentScene && templateType == ESceneTemplate::Basic)
			{
				SetupBasicSceneEntities(currentScene);
			}

			SelectionManager::Get().ClearSelection(true);
			Application::Get().AdoptSceneDocument(std::filesystem::path{});

			NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
				LOCAL("EditorDocument.SceneCreated"), ELogLevel::Info);

			return EResult::Success;
		});
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

	if (m_DocumentController->IsBusy() || IsSceneLoading())
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
		[this, action, path = std::move(path)](const std::vector<EditorDocumentId>&) -> EResult
		{
			if (!PIEManager::Get().IsEditing())
				return EResult::AccessDenied;

			if (action == ESceneDocumentAction::Open)
				return BeginSceneLoad(path) ? EResult::Success : EResult::Fail;

			auto& scenes = SceneManager::Get();
			EResult result = EResult::Fail;

			switch (action)
			{
			case ESceneDocumentAction::New:
				result = scenes.NewScene();
				break;
			case ESceneDocumentAction::Close:
				result = scenes.CloseScene();
				break;
			default:
				break;
			}

			if (result != EResult::Success)
				return result;

			SelectionManager::Get().ClearSelection(true);
			Application::Get().AdoptSceneDocument(std::filesystem::path{});

			const std::string message = action == ESceneDocumentAction::New
				? LOCAL("EditorDocument.SceneCreated") : LOCAL("EditorDocument.SceneClosed");

			NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"), message, ELogLevel::Info);

			return EResult::Success;
		});
}

bool ImGuiManager::RestoreSceneOnStartup(const std::filesystem::path& path)
{
	return BeginSceneLoad(path);
}

bool ImGuiManager::BeginSceneLoad(const std::filesystem::path& path)
{
	if (m_SceneLoadState)
		return false;

	std::filesystem::path resolvedPath(path);
	if (resolvedPath.is_relative())
		resolvedPath = Engine::PlatformPath::ResolveProjectRelative(resolvedPath);

	std::error_code ec;
	if (!std::filesystem::exists(resolvedPath, ec))
	{
		const std::string ext = resolvedPath.extension().string();
		if (ext == ".json")
		{
			const std::string stem = resolvedPath.stem().string();
			if (!stem.ends_with(".bamscene"))
			{
				const auto candidatePath = resolvedPath.parent_path() / (stem + ".bamscene.json");
				ec.clear();
				if (std::filesystem::exists(candidatePath, ec))
					resolvedPath = candidatePath;
			}
		}
	}

	auto state = std::make_unique<SceneLoadState>();
	state->RequestedPath = path;
	state->ResolvedPath = std::move(resolvedPath);
	state->DisplayName = WStrToStr(path.filename().wstring());
	state->StartTime = std::chrono::steady_clock::now();
	state->Archive = std::make_unique<archive::JsonArchive>();

	try
	{
		state->ArchiveThread = std::thread([rawState = state.get()]()
		{
			const auto readStart = std::chrono::steady_clock::now();
			rawState->ArchiveReadResult = rawState->Archive->LoadFromFile(rawState->ResolvedPath);
			if (rawState->ArchiveReadResult)
			{
				bool unused = false;
				Engine::ResourceReferenceSet refs;
				const auto refResult = rawState->Archive->ReadObject("Scene", unused,
					[&refs](archive::IArchive& object, bool&) -> archive::ArchiveResult
					{
						return archive_resource::ReadResourceReferences(object, "ResourceReferences", refs);
					});
				if (refResult)
				{
					rawState->Resources.assign(refs.begin(), refs.end());
					// 모델은 참조하는 메시를 한 번에 모두 로드한다. 메시를 먼저 나눠 로드하고 모델을 뒤에 두어 한 프레임에 몰리지 않게 한다.
					const auto isModel = [](const Engine::ResourceReference& reference)
						{
							const auto& path = reference.AssetPath.empty() ? reference.AssetKey : reference.AssetPath;
							return Engine::ToLowerStr(Engine::WStrToStr(path)).find(".bammodel") != std::string::npos;
						};
					std::stable_partition(rawState->Resources.begin(), rawState->Resources.end(),
						[&isModel](const Engine::ResourceReference& reference) { return !isModel(reference); });
				}
			}
			rawState->ArchiveMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count();
			rawState->ArchiveReady.store(true, std::memory_order_release);
		});
	}
	catch (const std::exception& exception)
	{
		BAM_LOG(Error, "Editor", "Document", "Failed to start scene load worker: {}", exception.what());
		return false;
	}

	m_SceneLoadState = std::move(state);
	return true;
}

void ImGuiManager::UpdateSceneLoad()
{
	if (!m_SceneLoadState)
		return;

	if (!PIEManager::Get().IsEditing())
	{
		m_SceneLoadState.reset();
		return;
	}

	auto& state = *m_SceneLoadState;
	const auto failLoad = [this](std::string_view logContext, const archive::ArchiveResult& archiveError)
	{
		BAM_LOG(Error, "Editor", "Document", "Scene load failed ({}): code={}, field={}",
			logContext, static_cast<int32>(archiveError.Code), archiveError.Field);
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL("EditorDocument.OperationFailed"), ELogLevel::Error);
		m_SceneLoadState.reset();
	};

	if (state.Stage == SceneLoadState::EStage::ReadingArchive)
	{
		if (!state.ArchiveReady.load(std::memory_order_acquire))
			return;

		if (state.ArchiveThread.joinable())
			state.ArchiveThread.join();

		if (!state.ArchiveReadResult)
		{
			failLoad("ReadArchive", state.ArchiveReadResult);
			return;
		}

		state.Stage = state.Resources.empty()
			? SceneLoadState::EStage::FinalizingScene
			: SceneLoadState::EStage::PreloadingResources;
	}

	if (state.Stage == SceneLoadState::EStage::PreloadingResources && ResourceManager::IsCreated())
	{
		// 로딩 창이 뜬 뒤에는 다른 조작을 할 수 없으므로 화면이 갱신되는 범위에서 예산을 늘린다.
		const auto frameBudget = std::chrono::milliseconds(state.ModalVisible ? 24 : 8);
		const auto batchStart = std::chrono::steady_clock::now();
		if (!state.Preloader)
		{
			state.PreloadStart = batchStart;
			state.PreparedBufferBaseline = Engine::Renderer::Get().GetRHI()->GetBufferUploadStats();
			state.Preloader = std::make_unique<Engine::ResourcePreloader>();
			state.Preloader->Start(state.Resources);
		}
		if (!state.Preloaded)
		{
			state.Preloaded = state.Preloader->Update(batchStart + frameBudget, [&state](const std::wstring& key, double milliseconds)
				{
					state.RecordResource({ key, key }, milliseconds);
				});
			if (!state.Preloaded)
				return;
			state.PreparedBufferStats = Engine::Renderer::Get().GetRHI()->GetBufferUploadStats();
			state.PreparedWallMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - state.PreloadStart).count();
		}
		auto& manager = ResourceManager::Get();

		while (state.NextResourceIndex < state.Resources.size())
		{
			const auto& ref = state.Resources[state.NextResourceIndex++];
			if (!ref.AssetKey.empty() || !ref.AssetPath.empty())
			{
				const auto resourceStart = std::chrono::steady_clock::now();
				try
				{
					(void)archive_resource::ResolveResourceHandle(manager, ref.AssetKey, ref.AssetPath);
				}
				catch (const std::filesystem::filesystem_error& error)
				{
					BAM_LOG(Warn, "Editor", "Document", "Resource preload path error: {}", error.what());
				}
				state.RecordResource(ref, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resourceStart).count());
			}

			if (std::chrono::steady_clock::now() - batchStart >= frameBudget)
				break;
		}

		if (state.NextResourceIndex < state.Resources.size())
			return;

		state.PreloadWallMilliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - state.PreloadStart).count();
		state.Stage = SceneLoadState::EStage::FinalizingScene;
		if (state.ModalVisible)
			return;
	}
	else if (state.Stage == SceneLoadState::EStage::PreloadingResources)
	{
		state.Stage = SceneLoadState::EStage::FinalizingScene;
	}

	if (state.Stage == SceneLoadState::EStage::FinalizingScene)
	{
		if (state.ModalVisible && !state.FinalizingFrameDrawn)
			return;

		const auto createStart = std::chrono::steady_clock::now();
		Scene* candidate = nullptr;
		const auto result = Engine::Scene::CreateFromArchive(*state.Archive, candidate);
		const auto openStart = std::chrono::steady_clock::now();
		if (!result || !candidate)
		{
			Safe_Release(candidate);
			failLoad("CreateFromArchive", result);
			return;
		}

		const EResult openResult = SceneManager::Get().OpenScene(candidate);
		Safe_Release(candidate);
		if (openResult != EResult::Success)
		{
			BAM_LOG(Error, "Editor", "Document", "Scene open failed: result={}", ResultToString(openResult));
			NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
				LOCAL("EditorDocument.OperationFailed"), ELogLevel::Error);
			m_SceneLoadState.reset();
			return;
		}

		const auto openEnd = std::chrono::steady_clock::now();
		const auto elapsedMs = [](auto begin, auto end) { return std::chrono::duration<double, std::milli>(end - begin).count(); };
		BAM_LOG(Info, "Editor", "Document", "Scene load timing: {} | total {:.0f} ms (archive {:.0f}, resource work {:.0f} over {} item(s) / wall {:.0f}, entities {:.0f}, open {:.0f})",
			state.DisplayName, elapsedMs(state.StartTime, openEnd), state.ArchiveMilliseconds, state.ResourceMilliseconds,
			state.Resources.size(), state.PreloadWallMilliseconds, elapsedMs(createStart, openStart), elapsedMs(openStart, openEnd));
		if (state.Preloader)
		{
			const auto stats = state.Preloader->GetStats();
			BAM_LOG(Info, "Editor", "Document", "Scene load workers: {} thread(s), {} mesh(es), {} texture(s), {} deferred ({} cached), {:.0f} MB | worker {:.0f} ms (slowest {:.0f}), main {:.0f} ms (mesh {:.0f}, texture {:.0f}), wall {:.0f} ms",
				stats.WorkerCount, stats.MeshCount, stats.TextureCount, stats.PlaceholderCount, stats.CachedPlaceholderCount, static_cast<double>(stats.UploadBytes) / (1024.0 * 1024.0),
				stats.WorkerMilliseconds, stats.SlowestWorkerMilliseconds, stats.MainMilliseconds,
				stats.MeshMainMilliseconds, stats.TextureMainMilliseconds, state.PreparedWallMilliseconds);
		}
		{
			const auto& before = state.PreparedBufferBaseline;
			const auto& after = state.PreparedBufferStats;
			BAM_LOG(Info, "Editor", "Document", "Scene load GPU buffers: {} created in {:.0f} ms, {} upload(s) in {:.0f} ms, {} upload heap(s), {} geometry heap(s)",
				after.CreateCount - before.CreateCount, after.CreateMilliseconds - before.CreateMilliseconds,
				after.UploadCount - before.UploadCount, after.UploadMilliseconds - before.UploadMilliseconds,
				after.UploadHeapCount - before.UploadHeapCount, after.GeometryHeapCount - before.GeometryHeapCount);
		}
		for (const auto& [extension, timing] : state.ResourceTimings)
			BAM_LOG(Info, "Editor", "Document", "Scene load resources: {} x{} = {:.0f} ms", extension, timing.Count, timing.Milliseconds);
		for (const auto& [milliseconds, resourcePath] : state.SlowestResources)
			BAM_LOG(Info, "Editor", "Document", "Scene load slowest: {:.0f} ms {}", milliseconds, resourcePath);

		SelectionManager::Get().ClearSelection(true);
		Application::Get().AdoptSceneDocument(state.RequestedPath);
		NotificationManager::Get().AddNotification(LOCAL("EditorDocument.Title"),
			LOCAL_FMT("EditorDocument.SceneOpened", WStrToStr(state.RequestedPath.wstring())), ELogLevel::Info);
		m_SceneLoadState.reset();
	}
}

void ImGuiManager::DrawSceneLoadingModal()
{
	const std::string modalTitle = LOCAL("EditorDocument.LoadingSceneTitle") + "###SceneLoadingModal";
	if (m_SceneLoadState)
	{
		constexpr auto kModalDelay = std::chrono::milliseconds(150);
		if (!m_SceneLoadState->ModalVisible &&
			std::chrono::steady_clock::now() - m_SceneLoadState->StartTime >= kModalDelay)
		{
			m_SceneLoadState->ModalVisible = true;
		}

		if (m_SceneLoadState->ModalVisible && !ImGui::IsPopupOpen(modalTitle.c_str()))
			ImGui::OpenPopup(modalTitle.c_str());
	}

	if (!ImGui::IsPopupOpen(modalTitle.c_str()))
		return;

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Always);

	constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings;
	if (!ImGui::BeginPopupModal(modalTitle.c_str(), nullptr, flags))
		return;

	if (!m_SceneLoadState)
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	auto& state = *m_SceneLoadState;
	ImGui::TextUnformatted(state.DisplayName.c_str());
	ImGui::Spacing();

	float progress = 0.0f;
	std::string stageText;
	switch (state.Stage)
	{
	case SceneLoadState::EStage::ReadingArchive:
		stageText = LOCAL("EditorDocument.LoadingSceneReading");
		progress = 0.08f + 0.12f * (0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f));
		break;
	case SceneLoadState::EStage::PreloadingResources:
	{
		// 작업 스레드 단계(70%)와 남은 리소스 단계(20%)를 이어서 표시한다.
		const bool preparing = state.Preloader && !state.Preloaded;
		const auto stats = state.Preloader ? state.Preloader->GetStats() : Engine::ResourcePreloader::Stats{};
		const size_t finished = preparing ? stats.FinishedCount : state.NextResourceIndex;
		const size_t total = preparing ? stats.ScheduledCount : state.Resources.size();
		const float fraction = static_cast<float>(finished) / static_cast<float>((std::max)(size_t{ 1 }, total));
		stageText = LOCAL_FMT("EditorDocument.LoadingSceneResources", finished, total);
		progress = preparing ? 0.20f + 0.50f * fraction : 0.70f + 0.20f * fraction;
		break;
	}
	case SceneLoadState::EStage::FinalizingScene:
		stageText = LOCAL("EditorDocument.LoadingSceneFinalizing");
		progress = 0.95f;
		state.FinalizingFrameDrawn = true;
		break;
	}

	ImGui::TextDisabled("%s", stageText.c_str());
	ImGui::Spacing();
	ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
	ImGui::EndPopup();
}

bool ImGuiManager::RequestQuit()
{
	if (m_DocumentController->IsBusy() || IsSceneLoading() || m_ResourceQuitConfirmPending)
		return false;
	for (auto* panel : m_ImGuiPanels)
	{
		if (const auto* editor = dynamic_cast<const ResourceEditorInterface*>(panel);
			editor && editor->m_PendingDiscardAction)
			return false;
	}
	if (auto* settings = dynamic_cast<UnifiedSettingsPanel*>(GetImGuiPanel(L"UnifiedSettings"));
		settings && !settings->ResolveDraftBeforeQuit())
		return true;
	if (PrefabModeManager::IsCreated() && !PrefabModeManager::Get().ResolvePrefabModeBeforeQuit())
		return true;

	// 리소스 편집기는 EditorDocumentManager에 등록되지 않으므로 씬 문서 확인보다 먼저 따로 확인한다.
	if (!CollectUnsavedResourceEditors().empty())
	{
		m_ResourceQuitConfirmPending = true;
		return true;
	}

	return ContinueQuitRequest();
}

bool ImGuiManager::ContinueQuitRequest()
{
	if (m_DocumentController->IsBusy() || IsSceneLoading())
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

vector<ResourceEditorInterface*> ImGuiManager::CollectUnsavedResourceEditors() const
{
	// 닫힌 편집기도 편집 상태를 유지하므로 열림 여부와 관계없이 모은다.
	vector<ResourceEditorInterface*> editors;
	for (auto* panel : m_ImGuiPanels)
	{
		auto* editor = dynamic_cast<ResourceEditorInterface*>(panel);
		if (editor && editor->HasUnsavedResourceChanges())
			editors.push_back(editor);
	}
	return editors;
}

bool ImGuiManager::CanRepairAssetReferenceSource(const std::filesystem::path& sourceFile) const
{
	if (!PIEManager::Get().IsEditing() || m_DocumentController->IsBusy() || IsSceneLoading() ||
		(PrefabModeManager::IsCreated() && PrefabModeManager::Get().IsInPrefabMode()))
		return false;

	using Path = Engine::PlatformPath;
	const auto source = Path::Normalize(Path::GetProjectRoot() / sourceFile);
	const auto retainsSource = [&source](const std::filesystem::path& path)
	{
		if (path.empty())
			return false;

		const auto resolved = Path::ResolveUri(path);
		std::error_code error;
		return Path::IsSamePath(source, resolved) || std::filesystem::equivalent(source, resolved, error);
	};

	auto& documents = Application::Get().GetDocuments();
	for (const auto id : documents.GetDocumentIds())
	{
		const auto* document = documents.Find(id);
		if (document && retainsSource(document->GetPath()))
			return false;
	}
	for (const auto* panel : m_ImGuiPanels)
	{
		const auto* editor = dynamic_cast<const ResourceEditorInterface*>(panel);
		if (editor && std::ranges::any_of(editor->GetRetainedResourcePaths(), retainsSource))
			return false;
	}
	return true;
}

bool ImGuiManager::PrepareAssetFileChanges(AssetPathRelocation& relocation, AssetMoveBlock* outBlock) const
{
	if (outBlock) *outBlock = {};
	const auto block = [outBlock](EAssetMoveBlockReason reason, std::string subject = {})
	{
		if (outBlock) *outBlock = { reason, std::move(subject) };
		return false;
	};
	const auto documentName = [](const IEditorDocument& document)
	{
		const auto& path = document.GetPath();
		return path.empty() ? LOCAL("EditorDocument.Untitled") : WStrToStr(path.filename().wstring());
	};

	if (!PIEManager::Get().IsEditing())
		return block(EAssetMoveBlockReason::NotEditing);
	if (m_DocumentController->IsBusy())
		return block(EAssetMoveBlockReason::DocumentBusy);
	if (IsSceneLoading())
		return block(EAssetMoveBlockReason::SceneLoading);
	if (const auto editors = CollectUnsavedResourceEditors(); !editors.empty())
	{
		std::string names;
		for (const auto* editor : editors)
			names += (names.empty() ? "" : ", ") + editor->GetDisplayName();
		return block(EAssetMoveBlockReason::UnsavedResourceEditor, names);
	}
	if (PrefabModeManager::IsCreated() && PrefabModeManager::Get().IsInPrefabMode())
		return block(EAssetMoveBlockReason::PrefabMode);
	auto& documents = Application::Get().GetDocuments();
	for (const auto id : documents.GetDocumentIds())
	{
		auto* document = documents.Find(id);
		if (!document) continue;
		if (document->HasUnsavedChanges())
			return block(EAssetMoveBlockReason::UnsavedDocument, documentName(*document));
		if (!document->PrepareAssetPathRelocation(relocation))
			return block(EAssetMoveBlockReason::DocumentRejected, documentName(*document));
		if (dynamic_cast<SceneDocument*>(document) && relocation.Affects(document->GetPath()))
		{
			relocation.AfterCommit([document]() { Application::Get().RememberEditedScene(document->GetPath()); });
		}
	}
	// Closed panels also retain editing state; each owner must opt in to safe synchronization.
	for (auto* panel : m_ImGuiPanels)
	{
		auto* editor = dynamic_cast<ResourceEditorInterface*>(panel);
		if (editor && !editor->PrepareAssetPathRelocation(relocation))
			return block(EAssetMoveBlockReason::EditorRetainsPath, editor->GetDisplayName());
		if (auto* references = dynamic_cast<AssetReferencePanel*>(panel))
			references->PrepareAssetPathRelocation(relocation);
	}
	if (Engine::SystemManager::IsCreated())
	{
		if (auto* graphSystem = Engine::SystemManager::Get().GetSystem<Engine::GraphSystem>())
		{
			const auto affects = [&relocation](const std::filesystem::path& path) { return relocation.Affects(path); };
			relocation.AfterCommit([graphSystem, affects]() { graphSystem->InvalidateGraphCache(affects); });
		}
	}
	if (EntityClipboard::HasData())
	{
		relocation.AfterCommit([]()
		{
			EntityClipboard::Clear();
			NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
				LOCAL("ContentBrowser.MoveEntityClipboardCleared"), ELogLevel::Info);
		});
	}
	return true;
}

void ImGuiManager::DrawResourceQuitConfirmation()
{
	const auto title = LOCAL("ResourceEditor.QuitConfirmTitle") + "###ResourceQuitConfirm";

	if (m_ResourceQuitConfirmPending && !ImGui::IsPopupOpen(title.c_str()))
		ImGui::OpenPopup(title.c_str());

	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	const auto editors = CollectUnsavedResourceEditors();
	if (!m_ResourceQuitConfirmPending || editors.empty())
	{
		// 확인 도중 미저장 편집기가 사라졌다면 남은 종료 절차(씬 문서 확인)를 이어간다.
		const bool continueQuit = std::exchange(m_ResourceQuitConfirmPending, false);
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		if (continueQuit)
			ContinueQuitRequest();
		return;
	}

	ImGui::TextUnformatted(LOCAL_CSTR("ResourceEditor.QuitUnsavedMessage"));
	ImGui::Spacing();

	ResourceEditorInterface* showTarget = nullptr;
	constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("UnsavedResourceEditors", 3, tableFlags))
	{
		for (auto* editor : editors)
		{
			const auto& target = editor->GetTargetResource();
			const auto targetName = target.empty() ? LOCAL("EditorDocument.Untitled") : WStrToStr(target.filename().wstring());

			ImGui::PushID(editor);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(editor->GetDisplayName().c_str());
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(targetName.c_str());
			if (!target.empty() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", WStrToStr(target.wstring()).c_str());
			ImGui::TableNextColumn();
			if (ImGui::SmallButton(LOCAL_CSTR("ResourceEditor.QuitShowEditor")))
				showTarget = editor;
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	ImGui::Spacing();
	const EditorUI::DialogAction actions[] = { { "EditorCommon.Cancel" }, { "ResourceEditor.QuitDiscard", true, true } };
	const int action = EditorUI::DialogButtonBar("ResourceQuitActions", actions, IM_ARRAYSIZE(actions));

	bool continueQuit = false;
	if (showTarget || action == 0)
	{
		m_ResourceQuitConfirmPending = false;
		ImGui::CloseCurrentPopup();
	}
	else if (action == 1)
	{
		// 확정한 폐기는 씬 확인과 별도로 완료한다.
		m_ResourceQuitConfirmPending = false;
		continueQuit = true;
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();

	if (showTarget)
	{
		showTarget->Focus();
		if (auto* window = ImGui::FindWindowByID(showTarget->m_PanelWindowId))
			ImGui::FocusWindow(window, ImGuiFocusRequestFlags_UnlessBelowModal);
	}

	if (continueQuit)
	{
		for (auto* editor : editors) editor->CloseResourceDocument();
		if (CollectUnsavedResourceEditors().empty()) ContinueQuitRequest();
	}
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

	const EditorUI::DialogAction actions[] = {
		{ "EditorCommon.Cancel" }, { "EditorDocument.Discard", true, true }, { "EditorDocument.Save", document->CanSave() }
	};
	const int action = EditorUI::DialogButtonBar("DocumentActions", actions, IM_ARRAYSIZE(actions));
	if (action == 2)
		controller.ResolveConfirmation(EDocumentActionChoice::Save);
	else if (action == 1)
		controller.ResolveConfirmation(EDocumentActionChoice::Discard);
	else if (action == 0)
	{
		controller.ResolveConfirmation(EDocumentActionChoice::Cancel);
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}
void ImGuiManager::DrawNewSceneDialog()
{
	const std::string modalTitle = LOCAL("EditorDocument.NewSceneDialogTitle") + "###NewSceneTemplateModal";

	if (m_ShowNewSceneDialog)
	{
		ImGui::OpenPopup(modalTitle.c_str());
		m_ShowNewSceneDialog = false;
	}

	ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Appearing);
	if (!ImGui::BeginPopupModal(modalTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextUnformatted(LOCAL_CSTR("EditorDocument.NewSceneTemplatePrompt"));
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 1. Basic (3D) Template
	const bool isBasic = (m_SelectedSceneTemplate == ESceneTemplate::Basic);
	if (ImGui::Selectable(LOCAL_CSTR("EditorDocument.TemplateBasic"), isBasic, ImGuiSelectableFlags_DontClosePopups))
	{
		m_SelectedSceneTemplate = ESceneTemplate::Basic;
	}
	if (ImGui::IsItemHovered() && Engine::InputManager::Get().IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
	{
		m_SelectedSceneTemplate = ESceneTemplate::Basic;
		ExecuteNewScene(m_SelectedSceneTemplate);
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped("  %s", LOCAL_CSTR("EditorDocument.TemplateBasicDesc"));
	ImGui::PopStyleColor();

	ImGui::Spacing();

	// 2. Empty Scene Template
	const bool isEmpty = (m_SelectedSceneTemplate == ESceneTemplate::Empty);
	if (ImGui::Selectable(LOCAL_CSTR("EditorDocument.TemplateEmpty"), isEmpty, ImGuiSelectableFlags_DontClosePopups))
	{
		m_SelectedSceneTemplate = ESceneTemplate::Empty;
	}
	if (ImGui::IsItemHovered() && Engine::InputManager::Get().IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
	{
		m_SelectedSceneTemplate = ESceneTemplate::Empty;
		ExecuteNewScene(m_SelectedSceneTemplate);
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}
	ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]);
	ImGui::TextWrapped("  %s", LOCAL_CSTR("EditorDocument.TemplateEmptyDesc"));
	ImGui::PopStyleColor();

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	const EditorUI::DialogAction actions[] = {
		{ "EditorCommon.Cancel" }, { "EditorDocument.Create" }
	};
	const int action = EditorUI::DialogButtonBar("NewSceneActions", actions, IM_ARRAYSIZE(actions));
	if (action == 1)
	{
		ExecuteNewScene(m_SelectedSceneTemplate);
		ImGui::CloseCurrentPopup();
	}
	else if (action == 0)
		ImGui::CloseCurrentPopup();

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
