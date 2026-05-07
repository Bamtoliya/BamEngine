#pragma once

#include "BaseViewportPanel.h"
#include "EditorCamera.h"

enum class EViewportCameraType : uint8
{
	Perspective,
	Orthographic,
};

struct tagCameraViewportPanelDesc : tagViewportPanelDesc
{
	EViewportCameraType CameraType = EViewportCameraType::Orthographic;
	bool IsSceneCamera = false; // 씬 카메라로 사용할지 여부 (기본값: false)
	bool IsPerspective = false; // 카메라 초기 투영 모드 (기본값: Orthographic)
};

BEGIN(Engine)
class Camera;
END

BEGIN(Editor)
class InspectorPanel;
class CameraViewportPanel : public BaseViewportPanel
{
	using DESC = tagCameraViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	CameraViewportPanel() { m_Name = L"Viewport Panel with Camera"; };
	virtual ~CameraViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr);
public:
	virtual void Free();
#pragma endregion

#pragma region Loop
public:
	virtual void Update(f32 dt) override;
public:
	void Draw();
#pragma endregion

#pragma region Custom Draws
protected:
	virtual void DrawCustomViewport() override;
	virtual void DrawCustomOptions() override;
#pragma endregion

#pragma region Draw Options Bar 
protected:
	void DrawCameraMenu();
#pragma endregion

#pragma region Camera Management
public:
	void SetCamera(Engine::Camera* camera) { m_Camera = camera; }
public:
	bool IsPerspective() const { return m_Camera->GetIsPerspective(); }
	bool IsOrthographic() const { return m_Camera->GetIsOrthographic(); }
	void SetPerspective(bool perspective = true) { m_Camera->SetPerspective(perspective); }
	void SetOrthographic(bool orthographic = true) { m_Camera->SetOrthographic(orthographic); }
#pragma endregion

#pragma region Dimension
protected:
	void DrawDimensionToggleButton();
#pragma endregion

#pragma region Member Variable
protected:
	EditorCamera* m_OwnedCamera = { nullptr }; // 씬 카메라로 사용할 경우 할당
	Engine::Camera* m_Camera = { nullptr };
	bool m_OwnCamera = { false }; // true면 패널이 카메라를 소유, false면 외부에서 할당받은 카메라
protected:
	InspectorPanel* m_InspectorPanel = { nullptr };
#pragma endregion
};
END