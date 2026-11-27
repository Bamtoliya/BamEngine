#pragma once

#include "ResourceEditorInterface.h"
#include "DataTable.h"
#include <optional>
#include <string>

BEGIN(Editor)

// CSV 기반 DataTable 에셋을 표 형태로 조회·편집하고 RFC 4180 형식으로 저장하는 리소스 편집기.
class DataTableEditor final : public ResourceEditorInterface
{
public:
	DataTableEditor() { m_Name = L"DataTable Editor"; }
	~DataTableEditor() override = default;

	bool IsSupported(const std::filesystem::path& assetPath) const override;
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override { return PrepareTargetPathRelocation(relocation); }

	bool HasUnsavedResourceChanges() const override { return m_Loaded && m_Dirty; }
	void SaveResource() override;

	bool OpenFile(const std::filesystem::path& path);
	bool SaveFile();

	const Engine::DataTable& GetDataTable() const noexcept { return m_Table; }
	bool IsLoaded() const noexcept { return m_Loaded; }
	bool IsDirty() const noexcept { return m_Dirty; }

protected:
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(900.0f, 600.0f); }
	string BuildResourceWindowTitle() const override;
	void DrawCustomToolbar() override;
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	void ReleaseResourceDocument() override;
	bool RestoreSavedResource() override;

private:
	Engine::DataTable m_Table;
	char m_SearchFilter[256] = {};
	std::string m_LastError;
	std::optional<std::size_t> m_PendingRemoveRow;
	bool m_Loaded = false;
	bool m_Dirty = false;
};

END
