#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceDocumentHistory.h"
#include "TerrainLayerAsset.h"

BEGIN(Editor)

// 지형 레이어 에셋(.bamterrainlayer.json) 편집기. 설정 구조체 하나를 사본으로 편집하고, 저장하면 파일을 쓴다.
// 이미 불러온 런타임 에셋은 에셋 핫 리로드가 파일 변경을 받아 갱신하므로 이 에셋을 참조하는 모든 지형에 반영된다.
class TerrainLayerEditor final : public ResourceEditorInterface
{
public:
	TerrainLayerEditor() { m_Name = L"Terrain Layer Editor"; m_DisplayNameKey = "ResourceEditor.TerrainLayerTitle"; }
	virtual ~TerrainLayerEditor() = default;

	bool IsSupported(const std::filesystem::path& assetPath) const override;
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override { return PrepareTargetPathRelocation(relocation); }

	bool HasUnsavedResourceChanges() const override { return m_Loaded && m_History.IsDirty(); }
	void SaveResource() override;
	bool CanSaveDocument() const;
	bool CanUndoDocument() const override;
	bool CanRedoDocument() const override;
	void UndoDocument() override;
	void RedoDocument() override;
	EShortcutContext GetShortcutContext() const override { return EShortcutContext::TerrainLayerEditor; }

protected:
	void DrawCustomToolbar() override;
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	void ReleaseResourceDocument() override;
	bool RestoreSavedResource() override;
	const char* GetSaveActionId() const override { return "TerrainLayerEditor.Save"; }

private:
	bool LoadDocument();
	bool CommitDocumentEdit();

	Engine::TerrainLayerSettings m_Settings;
	ResourceDocumentHistory<Engine::TerrainLayerSettings> m_History;
	bool m_Loaded = false;
};

END
