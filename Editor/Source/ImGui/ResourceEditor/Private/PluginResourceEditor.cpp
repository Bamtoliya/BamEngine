#include "PluginResourceEditor.h"
#include "HotReloadManager.h"
#include "ImGuiManager.h"
#include "ModuleBuildManager.h"
#include "NotificationManager.h"
#include "ShortcutManager.h"
#include <utility>

BEGIN(Editor)

namespace
{
	ResourcePanelPluginHost MakeResourcePluginHost()
	{
		ResourcePanelPluginHost host{};
		host.ImGui = ImGui::GetCurrentContext();
		ImGui::GetAllocatorFunctions(&host.Allocate, &host.Free, &host.AllocatorData);
		host.GetTextureId = [](Engine::RHITexture* texture) { return ImGuiManager::Get().GetImGuiTextureID(texture); };
		host.DrawActionMenuItem = [](const char* label, const char* action) { ACTION_MENU_ITEM(label, action); };
		host.GetShortcutHint = [](const char* action) { return ShortcutManager::Get().GetShortcutHint(action); };
		host.CanExecuteAction = [](const char* action) { return ShortcutManager::Get().CanExecuteAction(action); };
		host.ExecuteAction = [](const char* action) { return ShortcutManager::Get().ExecuteAction(action); };
		return host;
	}
}

PluginResourceEditor::PluginResourceEditor(std::string target, std::filesystem::path sourceDirectory)
	: m_Target(std::move(target)), m_SourceDirectory(std::move(sourceDirectory))
{
	m_Name.assign(m_Target.begin(), m_Target.end());
}

EResult PluginResourceEditor::OnInitialize(void*)
{
	Reload();
	return EResult::Success;
}

void PluginResourceEditor::Free()
{
	if (m_Api)
		m_Api->Destroy(m_Session);
	m_Api = nullptr;
	m_Session = nullptr;
	m_Module.Close();
}

void PluginResourceEditor::Reload()
{
	if (HotReloadManager::IsCreated())
	{
		const auto* builder = HotReloadManager::Get().GetPluginBuilder(m_Target);
		if (builder && builder->IsCompiling())
		{
			RequestReload();
			return;
		}
	}

	m_ReloadFailed = true;
	PluginModule candidate;
	const std::wstring dllName = std::wstring(m_Target.begin(), m_Target.end()) + L".dll";
	if (!candidate.Open(dllName.c_str()))
		return;

	const auto getApi = reinterpret_cast<GetResourcePanelPluginApi>(candidate.FindSymbol(ResourcePanelPluginEntry));
	const auto* api = getApi ? getApi() : nullptr;
	if (!api || api->ApiSize != sizeof(ResourcePanelPluginApi) || api->HostSize != sizeof(ResourcePanelPluginHost) ||
		api->StateSize == 0 || api->StateSize > 1024 * 1024 || !api->PanelName || !*api->PanelName || !api->TitleKey ||
		!api->DescriptionKey || !api->SaveActionId || !api->Supports || !api->Create || !api->Destroy || !api->Capture ||
		!api->Open || !api->IsDirty || !api->CanSave || !api->Save || !api->Update || !api->DrawToolbar ||
		!api->DrawQuickAccess || !api->DrawContent || !api->SaveWorkspace || !api->RestoreWorkspace)
	{
		EDITOR_LOG_ERROR("Editor plugin {} has an invalid V1 resource panel contract", m_Target);
		return;
	}

	if (m_Api && (m_Name != api->PanelName || m_SaveActionId != api->SaveActionId ||
		m_Api->StateVersion != api->StateVersion || m_Api->StateSize != api->StateSize))
	{
		EDITOR_LOG_ERROR("Editor plugin {} changed its panel identity or state contract; restart required", m_Target);
		return;
	}
	if (!m_Api)
	{
		const auto* existing = ImGuiManager::Get().FindRegisteredPanel(api->PanelName);
		if (existing && existing != this)
		{
			EDITOR_LOG_ERROR("Editor plugin {} declares a duplicate panel name", m_Target);
			return;
		}
	}

	std::vector<std::byte> state;
	if (m_Api)
	{
		state.resize(m_Api->StateSize);
		m_Api->Capture(m_Session, state.data());
	}
	auto* session = api->Create(state.empty() ? nullptr : state.data());
	if (!session)
	{
		EDITOR_LOG_ERROR("Editor plugin {} could not create or restore its editing session", m_Target);
		return;
	}

	if (m_Api)
		m_Api->Destroy(m_Session);
	m_Module.Swap(candidate);
	m_Api = api;
	m_Session = session;
	m_Name = api->PanelName;
	m_DisplayNameKey = api->TitleKey;
	m_SaveActionId = api->SaveActionId;
	m_ReloadFailed = false;
	if (m_ActionRegistrationReady)
		RegisterSaveAction();
}

void PluginResourceEditor::RegisterSaveAction()
{
	m_ActionRegistrationReady = true;
	if (m_ActionRegistered || m_SaveActionId.empty())
		return;

	m_ActionRegistered = ShortcutManager::Get().RegisterAction({
		.Id = m_SaveActionId,
		.DisplayNameKey = "EditorCommon.Save",
		.CategoryKey = m_DisplayNameKey,
		.Context = EShortcutContext::PluginResource,
		.DefaultChord = ShortcutChord(Engine::EKeyCode::S, EModifierKey::Ctrl),
		.ExecuteHandler = [target = m_Target]() {
			if (auto* panel = ImGuiManager::Get().GetPluginResourceEditor(target))
				panel->Save();
		},
		.CanExecute = [target = m_Target]() {
			const auto* panel = ImGuiManager::Get().GetPluginResourceEditor(target);
			return panel && panel->CanSave();
		},
		.PanelScope = m_Name,
	});
	if (!m_ActionRegistered)
		EDITOR_LOG_ERROR("Editor plugin {} declares a duplicate save action: {}", m_Target, m_SaveActionId);
}

bool PluginResourceEditor::CanSave() const
{
	return m_Open && m_Api && !m_TargetResourcePath.empty() && m_Api->CanSave(m_Session);
}

void PluginResourceEditor::Save()
{
	if (!CanSave())
		return;

	const auto path = m_TargetResourcePath.wstring();
	const bool saved = m_Api->Save(m_Session, path.c_str());
	NotificationManager::Get().AddNotification(GetDisplayName(),
		saved ? LOCAL_FMT("Notification.Saved", WStrToStr(m_TargetResourcePath.filename().wstring()))
			: LOCAL_FMT("Notification.SaveFailed", WStrToStr(m_TargetResourcePath.filename().wstring())),
		saved ? ELogLevel::Info : ELogLevel::Error);
}

bool PluginResourceEditor::IsSupported(const std::filesystem::path& path) const
{
	return m_Api && m_Api->Supports(path.wstring().c_str());
}

bool PluginResourceEditor::HasUnsavedResourceChanges() const
{
	return m_Api && m_Api->IsDirty(m_Session);
}

void PluginResourceEditor::SetTargetResource(const std::filesystem::path& path)
{
	ResourceEditorInterface::SetTargetResource(path);
	if (m_Api)
		m_Api->Open(m_Session, path.wstring().c_str());
}

std::string PluginResourceEditor::GetDescription() const
{
	return m_Api ? LOCAL(m_Api->DescriptionKey) : LOCAL("PluginManager.ResourcePluginUnavailable");
}

void PluginResourceEditor::Update(f32 dt)
{
	if (std::exchange(m_ReloadPending, false))
		Reload();
	if (m_Open && m_Api)
		m_Api->Update(m_Session, dt);
}

ImVec2 PluginResourceEditor::GetInitialResourceWindowSize() const
{
	return m_Api ? m_Api->InitialSize : ImVec2{};
}

void PluginResourceEditor::DrawCustomToolbar()
{
	if (m_Api)
		m_Api->DrawToolbar(m_Session, MakeResourcePluginHost());
}

void PluginResourceEditor::DrawCustomQuickAccess()
{
	if (m_Api)
		m_Api->DrawQuickAccess(m_Session, MakeResourcePluginHost());
}

void PluginResourceEditor::DrawCustomContent()
{
	if (m_Api)
		m_Api->DrawContent(m_Session, MakeResourcePluginHost());
	else
		EditorUI::EmptyState("PluginManager.ResourcePluginUnavailable");
}

void PluginResourceEditor::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	if (m_Api)
		m_Api->SaveWorkspace(m_Session, &workspace, [](void* context, const char* key, float value) {
			static_cast<EditorWorkspace*>(context)->SetSplitter(key, value);
		});
}

void PluginResourceEditor::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	if (m_Api)
		m_Api->RestoreWorkspace(m_Session, &workspace, [](const void* context, const char* key, float fallback) {
			return static_cast<const EditorWorkspace*>(context)->GetSplitter(key, fallback);
		});
}

END
