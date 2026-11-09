#pragma once

#include "ImGuiInterface.h"
#include "PostProcess.h"

BEGIN(Editor)

class PostProcessPanel final : public IImGui
{
public:
	PostProcessPanel();
	~PostProcessPanel() override = default;

public:
	void Update(f32 dt) override;
	void Draw() override;
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::SceneEditing; }
private:
	void RefreshPresetList();
	void DrawPresetToolbar(PostProcessChain* chain);
	void DrawPipelineList(PostProcessChain* chain);
	void DrawEffectInspector(PostProcessChain& chain, Engine::IPostProcessEffect* effect);
	void DrawSceneApplyButton(PostProcessChain* chain);

private:
	static constexpr const char* PresetDirectory = "Resources/Config/PostProcess/Presets";

	bool m_PresetListNeedsRefresh = true;

	int32 m_SelectedEffectIndex = 0;
	int32 m_SelectedViewportIndex = 0;
	std::string m_SelectedPresetName;

	char m_NewPresetName[64] = "DefaultPreset";
	std::vector<std::string> m_AvailablePresets;
};

END
