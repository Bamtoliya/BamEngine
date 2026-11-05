#pragma once

#include "ImGuiInterface.h"
#include "AnimatorStateMachine.h"
#include "PreviewViewportWidget.h"
#include <memory>
#include <string>
#include <unordered_map>
#include "ResourceHandle.h"
#include <optional>
#include <tuple>

BEGIN(Engine)
class Entity;
class Model;
END

BEGIN(Editor)

class AnimatorGraphPanel : public IImGui
{
public:
	AnimatorGraphPanel();
	virtual ~AnimatorGraphPanel() override;

	virtual void Draw() override;
	virtual void Update(f32 dt) override;

	void SetStateMachine(const std::shared_ptr<Engine::AnimatorStateMachine>& sm);
	std::shared_ptr<Engine::AnimatorStateMachine> GetStateMachine() const { return m_StateMachine; }
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;
public:
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::AnimatorGraph; }
	static AnimatorGraphPanel* GetActivePanel() { return s_ActivePanel; }
	bool CanDeleteSelection() const;
	void DeleteSelection();
private:
	// GraphPanel 스타일 2단 메뉴/툴바
	void DrawMenuBar();
	void DrawToolbar();

	// 3D 프리뷰 및 작업 공간
	void InitializePreview();
	void UpdatePreview(f32 dt);
	void DrawPreviewSection(float width, float height);
	void SyncPreviewStateMachine();
	bool ReplacePreviewModel(const Engine::ResourceHandle<Engine::Model>& requested);

	void DrawParametersTab();
	void DrawPreviewParameters();
	void DrawCanvas();
	void DrawTransitions(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& anyStatePos);
	void DrawCanvasContextMenu();
	void DrawSelectionInspector();
	void DrawStateInspector(Engine::AnimState& state);
	void DrawBlend2DPreview(const Engine::AnimState& state);
	void DrawTransitionInspector(Engine::AnimTransition& transition);
	void RenameParameter(const std::string& oldName, const std::string& newName);
public:
	enum class EPendingAction { None, New, Open, Close };
	void RequestNew();
	void RequestOpen();
	void RequestClose();
	bool SaveDocument(bool saveAs);
	bool OpenDocument();
private:
	void ExecuteNew();
	void ExecuteOpen();
	void ExecuteClose();
	void ExecutePendingAction();
	void DrawConfirmModal();

	void AddState(const ImVec2& position);

private:
	std::shared_ptr<Engine::AnimatorStateMachine> m_StateMachine;
	std::string m_CurrentFilePath;
	bool m_IsDirty = false;

	// 3D 프리뷰 뷰포트 & 모델
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewModelEntity = nullptr;
	Engine::ResourceHandle<Engine::Model> m_PreviewModel;
	std::optional<Engine::ResourceHandle<Engine::Model>> m_PendingPreviewModel;
	bool m_ShowPreviewViewport = true;

	// UI 선택 및 조작 상태
	int m_SelectedStateIndex = -1;
	int m_SelectedTransitionIndex = -1;
	int m_ConnectingSourceIndex = -1;
	int m_DraggingStateIndex = -1;

	// 노드 위치 및 스크롤
	ImVec2 m_CanvasScrolling = ImVec2(0.0f, 0.0f);
	std::unordered_map<std::string, ImVec2> m_NodePositions;

	// 창 분할 너비
	float m_LeftPaneWidth = 260.0f;
	float m_InspectorWidth = 280.0f;

	inline static AnimatorGraphPanel* s_ActivePanel = nullptr;
	EPendingAction m_PendingAction = EPendingAction::None;
	std::string m_ParameterRenameDraft;
	std::string m_StateRenameDraft;
};

END
