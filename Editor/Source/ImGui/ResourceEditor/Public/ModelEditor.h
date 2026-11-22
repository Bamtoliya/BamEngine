#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceDocumentHistory.h"
#include "Model.h"
#include "PreviewViewportWidget.h"

BEGIN(Engine)
class Entity;
class Mesh;
struct AnimatorComponent;
END

BEGIN(Editor)

class ModelEditor final : public ResourceEditorInterface
{
public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override
	{
		return PrepareTargetPathRelocation(relocation);
	}
	ModelEditor();
	virtual ~ModelEditor() override;

public:
	bool IsSupported(const std::filesystem::path& assetPath) const override;
	bool HasUnsavedResourceChanges() const override { return m_History.IsDirty(); }
	void SaveResource() override { SaveModel(); }
	bool CanSaveDocument() const;
	EShortcutContext GetShortcutContext() const override { return EShortcutContext::ModelEditor; }
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	bool BeginAsyncOpen(const std::filesystem::path& assetPath) override;
	bool StepAsyncOpen(float maxTimeSeconds = 0.002f) override;
	bool IsAsyncLoading() const override { return m_AsyncStage != EAsyncStage::None; }
	void DrawAsyncLoadingOverlay() override;
	void CancelAsyncOpen() override;
	virtual void Update(f32 dt) override;
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

private:
	void SetupPreview();
	void InstantiateModel();
	void DrawCustomToolbar() override;
	void ReleaseResourceDocument() override;
	bool RestoreSavedResource() override;
	const char* GetSaveActionId() const override { return "ModelEditor.Save"; }
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	void DrawModelInfo(Engine::Model& model);
	void DrawMaterialSlots(Engine::Model& model, bool canEdit);
	void DrawAnimations(Engine::Model& model);
	void DrawLODInfo(Engine::Model& model);
	void SetForcedLOD(int lod);
	void SaveModel();
	[[nodiscard]] Engine::AnimatorComponent* GetPreviewAnimator() const;
	void SelectAnimation(int index, bool autoPlay = true);

private:
	Engine::ResourceHandle<Engine::Model> m_ModelHandle;
	Engine::ResourceHandle<Engine::Mesh> m_MeshHandle;
	string m_StatusKey;
	using MaterialSlots = std::vector<Engine::ResourceHandle<Engine::MaterialInterface>>;
	ResourceDocumentHistory<MaterialSlots> m_History;
	bool CommitDocumentEdit();
	bool ApplyDocumentSnapshot(const MaterialSlots& snapshot);
	bool CanUndoDocument() const override;
	bool CanRedoDocument() const override;
	void UndoDocument() override;
	void RedoDocument() override;
	int m_SelectedAnimIndex = 0;
	int m_ForcedLOD = -1; // -1: Auto, 0: LOD0, 1: LOD1, 2: LOD2

	// 3D 프리뷰 뷰포트 & 엔티티
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;

	// 리사이저블 분할 너비
	float m_LeftPaneWidth = 420.0f;
	char m_MaterialSlotSearch[64] = {};

	// 비동기 열기 상태
	enum class EAsyncStage
	{
		None,
		WaitingBackground,
		LoadingResource,
		SettingUpPreview,
		Finalizing,
		Failed
	};
	EAsyncStage m_AsyncStage = EAsyncStage::None;
	float m_AsyncProgress = 0.0f;
	std::filesystem::path m_AsyncTargetPath;
	std::chrono::steady_clock::time_point m_AsyncStartTime{};
	std::chrono::steady_clock::time_point m_AsyncLoadStart{};
	double m_AsyncLoadDurationMs = 0.0;
};

END
