#pragma once

#include "ResourceEditorInterface.h"
#include "Model.h"
#include "PreviewViewportWidget.h"

BEGIN(Engine)
class Entity;
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
	void Draw() override;
	virtual void Update(f32 dt) override;
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

private:
	void SetupPreviewModel();
	void InstantiateModel();
	void DrawMaterialSlots(Engine::Model& model, bool canEdit);
	void SaveModel();

private:
	Engine::ResourceHandle<Engine::Model> m_ModelHandle;
	string m_StatusKey;
	bool m_IsDirty = false;

	// 3D 프리뷰 뷰포트 & 엔티티
	PreviewViewportWidget m_PreviewWidget;
	Engine::Entity* m_PreviewEntity = nullptr;

	// 리사이저블 분할 너비
	float m_LeftPaneWidth = 420.0f;
};

END