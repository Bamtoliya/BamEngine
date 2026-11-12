#pragma once

#include "BaseViewportPanel.h"
#include "ViewportDeferredPipeline.h"
#include "PostProcess.h"

BEGIN(Engine)
class InputManager;
END

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
	virtual void Free() override;

public:
	virtual void Update(f32 dt) override;
	virtual void Draw() override;
	virtual PostProcessChain* GetPostProcessChain() override { return &m_PostProcessChain; }
	virtual const PostProcessChain* GetPostProcessChain() const override { return &m_PostProcessChain; }

public:
	bool HitTestGameplayImage(const vec2& screenPosition) const;
	uint32 UpdateGameplayMouseButtons(Engine::InputManager& input, bool enabled);
protected:
	EResult OnInitialize(void* arg) override;
	virtual void DrawCustomViewport() override;
	virtual void DrawCustomOptions() override;
	EResult ResizeRenderTargets(uint32 width, uint32 height) override;

private:
	std::optional<CameraBuffer> FindActiveGameCameraBuffer();
	void SubmitPostProcessPass(f32 dt, std::wstring& currentRT);

private:
	ViewportDeferredPipeline m_Pipeline;
	PostProcessChain m_PostProcessChain;
	RenderPassID m_UIOverlayPassID = INVALID_PASS_ID;
	string m_DefaultPostProcessSettings;
	string m_LastPostProcessPreset;
	string m_LastPostProcessSettings;
	entt::entity m_LastPostProcessEntity = entt::null;
	uint64 m_LastSyncedScenePipelineVersion = 0;
	bool m_PostProcessStateUsable = true;
	bool m_HasActiveCamera = false;
	ImGuiID m_GameplayImageWindowId = 0;
	uint32 m_GameplayMouseButtons = 0;
	bool m_UsePixelPerfectView = false;
};

END