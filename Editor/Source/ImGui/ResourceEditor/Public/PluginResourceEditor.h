#pragma once

#include "ResourceEditorInterface.h"
#include "EditorPluginModule.h"
#include "ResourcePanelPluginApi.h"

BEGIN(Editor)

class PluginResourceEditor final : public ResourceEditorInterface
{
public:
	PluginResourceEditor(std::string target, std::filesystem::path sourceDirectory);
	void Free() override;
	bool IsSupported(const std::filesystem::path& path) const override;
	bool HasUnsavedResourceChanges() const override;
	void SetTargetResource(const std::filesystem::path& path) override;
	void CaptureState(EditorWorkspace& workspace) const override;
	void RestoreState(const EditorWorkspace& workspace) override;
	void Update(f32 dt) override;
	bool ShouldUpdateWhenClosed() const override { return m_ReloadPending; }
	EShortcutContext GetShortcutContext() const override { return EShortcutContext::PluginResource; }
	void RequestReload() { m_ReloadPending = true; }
	bool IsLoaded() const { return m_Api != nullptr; }
	bool ReloadFailed() const { return m_ReloadFailed; }
	bool HasSaveAction() const { return !m_SaveActionId.empty(); }
	const std::string& GetTarget() const { return m_Target; }
	const std::filesystem::path& GetSourceDirectory() const { return m_SourceDirectory; }
	std::string GetDescription() const;
	void RegisterSaveAction();

private:
	EResult OnInitialize(void*) override;
	void Reload();
	bool CanSave() const;
	void Save();
	ImVec2 GetInitialResourceWindowSize() const override;
	void DrawCustomToolbar() override;
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;

	std::string m_Target;
	std::filesystem::path m_SourceDirectory;
	EditorPluginModule m_Module;
	const ResourcePanelPluginApi* m_Api = nullptr;
	void* m_Session = nullptr;
	std::string m_SaveActionId;
	bool m_ReloadPending = false;
	bool m_ReloadFailed = false;
	bool m_ActionRegistrationReady = false;
	bool m_ActionRegistered = false;
};

END
