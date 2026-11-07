#pragma once

#include "ImGuiInterface.h"

BEGIN(Editor)

class HistoryPanel final : public IImGui
{
public:
	HistoryPanel();
	~HistoryPanel() override = default;
	void Draw() override;
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::SceneEditing; }
};

END