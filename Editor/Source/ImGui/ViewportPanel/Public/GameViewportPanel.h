#pragma once

#include "BaseViewportPanel.h"
#include "ViewportDeferredPipeline.h"
#include <optional>

BEGIN(Editor)

struct GameViewportPanelDesc : public ViewportPanelDesc
{
};

class GameViewportPanel : public BaseViewportPanel
{
	using DESC = GameViewportPanelDesc;
public:
	GameViewportPanel() { m_Name = L"Game View"; }
	virtual ~GameViewportPanel() = default;

public:
	virtual void Initialize(void* arg = nullptr) override;
	virtual void Free() override;

public:
	virtual void Update(f32 dt) override;
	virtual void Draw() override;

protected:
	virtual void DrawCustomViewport() override;
	virtual void DrawCustomOptions() override;
	virtual void ResizeRenderTargets(uint32 width, uint32 height) override;

private:
	std::optional<CameraBuffer> FindActiveGameCameraBuffer();

private:
	ViewportDeferredPipeline m_Pipeline;
	bool m_HasActiveCamera = false;
};

END