#pragma once

#include "BaseViewportPanel.h"

BEGIN(Editor)
class SceneViewportPanel : public BaseViewportPanel
{
#pragma region Constructor&Destructor
public:
	SceneViewportPanel() { m_Name = L"Scene Viewport"; }
	virtual ~SceneViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr) override;
public:
	virtual void Free();
#pragma endregion

public:
	virtual void Update(f32 dt) override;
public:
	virtual void Draw() override;

	void DrawImViewGuizmo(const ImVec2& imageScreenPos, const ImVec2& imageSize);
	void DrawImGuizmo(const ImVec2& imageScreenPos, const ImVec2& imageSize);

};
END