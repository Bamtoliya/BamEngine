#pragma once

#include "ImGuiInterface.h"

BEGIN(Editor)

class HistoryPanel final : public IImGui
{
public:
	HistoryPanel();
	~HistoryPanel() override = default;

	EResult Initialize(void* arg = nullptr);
	void Draw() override;
};

END