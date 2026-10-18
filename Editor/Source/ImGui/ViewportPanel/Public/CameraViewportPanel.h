#pragma once

#include "BaseViewportPanel.h"

enum class EViewportCameraType : uint8
{
	Perspective,
	Orthographic,
};

struct CameraViewportPanelDesc : ViewportPanelDesc
{
	EViewportCameraType CameraType = EViewportCameraType::Orthographic;
	bool IsSceneCamera = false; // 씬 카메라로 사용할지 여부 (기본값: false)
	bool IsPerspective = false; // 카메라 초기 투영 모드 (기본값: Orthographic)
};

BEGIN(Editor)
class EditorCameraController;
class CameraViewportPanel : public BaseViewportPanel
{
protected:
	using DESC = CameraViewportPanelDesc;
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

#pragma region Camera Management
public:
	bool IsPerspective() const;
	bool IsOrthographic() const;
	void SetPerspective(bool perspective = true);
	void SetOrthographic(bool orthographic = true);
#pragma endregion

#pragma region Dimension
protected:
	void DrawDimensionToggleButton();
#pragma endregion

#pragma region Member Variable
protected:
	EditorCameraController* m_CameraController = { nullptr };
	bool m_OwnCamera = { false }; // true면 패널이 카메라를 소유, false면 외부에서 할당받은 카메라
#pragma endregion
};
END