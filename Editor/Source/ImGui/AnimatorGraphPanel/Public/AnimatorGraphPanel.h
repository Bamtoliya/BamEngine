#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceDocumentHistory.h"
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

namespace ax::NodeEditor { struct EditorContext; }

BEGIN(Editor)

class AnimatorGraphPanel : public ResourceEditorInterface
{
public:
	AnimatorGraphPanel();
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override;
	virtual ~AnimatorGraphPanel() override;

	virtual void Update(f32 dt) override;

	void SetStateMachine(const std::shared_ptr<Engine::AnimatorStateMachine>& sm);
	std::shared_ptr<Engine::AnimatorStateMachine> GetStateMachine() const { return m_StateMachine; }
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;
public:
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::GraphAnimation; }
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::AnimatorGraph; }
	static AnimatorGraphPanel* GetActivePanel() { return s_ActivePanel; }
	bool CanDeleteSelection() const;
	void DeleteSelection();
	void ResetCanvasView();
private:
	// GraphPanel 스타일 2단 메뉴/툴바
	void DrawCustomToolbar() override;
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	ImGuiWindowFlags GetResourceWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(1000.0f, 600.0f); }
	void OnResourceWindowVisibility(bool visible) override;
	string BuildResourceWindowTitle() const override;
	void ReleaseResourceDocument() override;
	const char* GetSaveActionId() const override { return "Animator.Save"; }

	// 3D 프리뷰 및 작업 공간
	void InitializePreview();
	void UpdatePreview(f32 dt);
	void DrawPreviewSection(float width, float height);
	void SyncPreviewStateMachine();
	bool ReplacePreviewModel(const Engine::ResourceHandle<Engine::Model>& requested);

	void DrawParametersTab();
	void DrawPreviewParameters();
	void DrawCanvas();
	void DrawTransitions();
	void DrawCanvasContextMenu();
	void DrawSelectionInspector();
	void DrawStateInspector(Engine::AnimState& state);
	void DrawBlend2DPreview(const Engine::AnimState& state);
	void DrawTransitionInspector(Engine::AnimTransition& transition);
	void RenameParameter(const std::string& oldName, const std::string& newName);
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual bool HasUnsavedResourceChanges() const override { return m_NewDocument || m_History.IsDirty(); }
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override;
	bool LoadFromFile(const std::filesystem::path& path);

	void RequestNew();
	void RequestOpen();
	bool CanSaveDocument() const { return m_Open && m_StateMachine != nullptr; }
	bool SaveDocument(bool saveAs);
	bool OpenDocument();
private:
	void ExecuteNew();

	void AddState(const ImVec2& position);
	ax::NodeEditor::EditorContext* CreateEditorContext();

private:
	std::shared_ptr<Engine::AnimatorStateMachine> m_StateMachine;
	std::string m_CurrentFilePath;
	struct DocumentSnapshot
	{
		Engine::AnimatorStateMachine StateMachine;
		std::unordered_map<std::string, ImVec2> Positions;
	};
	ResourceDocumentHistory<DocumentSnapshot> m_History;
	bool m_NewDocument = false;
	void ResetHistoryFromStateMachine();
	bool CommitDocumentEdit();
	bool ApplyDocumentSnapshot(const DocumentSnapshot& snapshot);
	bool CanUndoDocument() const override;
	bool CanRedoDocument() const override;
	void UndoDocument() override;
	void RedoDocument() override;
	char m_ParameterSearch[64] = {};

	// 3D 프리뷰 뷰포트 & 모델
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewModelEntity = nullptr;
	Engine::ResourceHandle<Engine::Model> m_PreviewModel;
	std::optional<Engine::ResourceHandle<Engine::Model>> m_PendingPreviewModel;
	bool m_ShowPreviewViewport = true;

	// imgui-node-editor 캔버스
	ax::NodeEditor::EditorContext* m_EditorContext = nullptr;
	bool m_NeedNavigateToContent = false;
	bool m_NeedNodePositionSync = true;

	// UI 선택 및 조작 상태
	int m_SelectedStateIndex = -1;
	int m_SelectedTransitionIndex = -1;
	int m_ConnectingSourceIndex = -1;
	ImVec2 m_ContextMenuPosition{ 0.0f, 0.0f };

	// 노드 위치
	std::unordered_map<std::string, ImVec2> m_NodePositions;

	// 창 분할 너비
	float m_LeftPaneWidth = 260.0f;
	float m_InspectorWidth = 280.0f;

	inline static AnimatorGraphPanel* s_ActivePanel = nullptr;
	std::string m_ParameterRenameDraft;
	std::string m_StateRenameDraft;
};

END
