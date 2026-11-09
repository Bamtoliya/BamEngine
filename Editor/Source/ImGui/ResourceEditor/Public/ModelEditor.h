#pragma once

#include "ResourceEditorInterface.h"
#include "Model.h"
#include "PreviewViewportWidget.h"

BEGIN(Engine)
class Entity;
struct AnimatorComponent;
END

BEGIN(Editor)

class ModelEditor final : public ResourceEditorInterface
{
public:
	ModelEditor();
	virtual ~ModelEditor() override;

public:
	bool IsSupported(const std::filesystem::path& assetPath) const override;
	bool HasUnsavedResourceChanges() const override { return m_IsDirty; }
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	virtual void Update(f32 dt) override;
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

private:
	void SetupPreviewModel();
	void InstantiateModel();
	void DrawCustomToolbar() override;
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
	string m_StatusKey;
	bool m_IsDirty = false;
	int m_SelectedAnimIndex = 0;
	int m_ForcedLOD = -1; // -1: Auto, 0: LOD0, 1: LOD1, 2: LOD2

	// 3D 프리뷰 뷰포트 & 엔티티
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;

	// 리사이저블 분할 너비
	float m_LeftPaneWidth = 420.0f;
};

END
