#pragma once

#include "BaseViewportPanel.h"

BEGIN(Editor)
class GameViewportPanel : public BaseViewportPanel
{
public:
	GameViewportPanel() { m_Name = L"Game Viewport Panel"; }
	virtual ~GameViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr) override;
	virtual void Free() override;
public:
	void Update(f32 dt) override;
	void Draw() override;
};
END