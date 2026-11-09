#pragma once

#include "ImGuiInterface.h"
#include "LocalizationManager.h"
#include "EditorPanelPluginApi.h"
#include "EditorPluginModule.h"
BEGIN(Editor)

class PanelLifecycleProbe final : public IImGui
{
public:
	static constexpr const wchar_t* Id = L"Editor.BuiltIn.PanelLifecycleProbe";

	PanelLifecycleProbe()
	{
		m_Name = Id;
		m_DisplayNameKey = "PanelLifecycleProbe.Title";
	}

	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Diagnostics; }

	void Draw() override;
	void Update(f32 dt) override;
	void Free() override;
	bool ShouldUpdateWhenClosed() const override { return true; }
	void RequestReload() { m_ReloadPending = true; }

private:
	EResult OnInitialize(void* arg) override;
	EResult ReloadModule();

	EditorPluginModule m_Module;
	DrawPanelV1 m_Draw = nullptr;
	bool m_ReloadPending = false;
};

END
