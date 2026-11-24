#include "TerrainLayerEditor.h"
#include "EditorUI.h"
#include "IconsFontAwesome7.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "ReflectionPropertyDrawer.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include "Functions.h"

bool TerrainLayerEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	return Engine::TerrainLayerAsset::IsTerrainLayerFile(assetPath);
}

void TerrainLayerEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	ResourceEditorInterface::SetTargetResource(assetPath);
	if (!LoadDocument())
	{
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.TerrainLayerTitle"),
			LOCAL_FMT("TerrainLayerEditor.LoadFailed", Engine::WStrToStr(assetPath.filename().wstring())), ELogLevel::Warn);
	}
}

bool TerrainLayerEditor::LoadDocument()
{
	m_Loaded = false;
	m_History.Clear();
	if (m_TargetResourcePath.empty()) return false;

	// 편집용 사본은 ResourceManager에 등록하지 않는 임시 에셋으로 읽는다(런타임 에셋과 분리).
	auto* asset = Engine::TerrainLayerAsset::CreateEmpty();
	if (!asset) return false;
	const bool loaded = asset->LoadFromFile(m_TargetResourcePath);
	if (loaded) m_Settings = asset->GetSettings();
	Engine::ResourceManager::Get().DestroyResource(asset);
	if (!loaded) return false;

	m_History.Reset(m_Settings);
	m_Loaded = true;
	return true;
}

bool TerrainLayerEditor::CommitDocumentEdit()
{
	return m_History.Record(m_Settings, [this](const Engine::TerrainLayerSettings& value) { m_Settings = value; return true; },
		LOCAL("History.Command.EditProperty"));
}

bool TerrainLayerEditor::CanSaveDocument() const
{
	return m_Open && m_Loaded && !Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath);
}

void TerrainLayerEditor::SaveResource()
{
	if (!CanSaveDocument() || !CommitDocumentEdit()) return;

	// 기존 파일을 읽은 뒤 설정만 바꿔 쓴다. 파일의 다른 필드(머리말 등)는 보존된다.
	auto* asset = Engine::TerrainLayerAsset::CreateEmpty();
	if (!asset) return;
	const bool loaded = asset->LoadFromFile(m_TargetResourcePath);
	asset->SetSettings(m_Settings);
	const bool saved = loaded && asset->SaveToFile(m_TargetResourcePath);
	Engine::ResourceManager::Get().DestroyResource(asset);

	const auto fileName = Engine::WStrToStr(m_TargetResourcePath.filename().wstring());
	if (!saved)
	{
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.TerrainLayerTitle"),
			LOCAL_FMT("TerrainLayerEditor.SaveFailed", fileName), ELogLevel::Warn);
		return;
	}
	m_History.MarkSaved();
}

bool TerrainLayerEditor::CanUndoDocument() const
{
	return m_Loaded && m_History.CanUndo() && (!m_History.HasPendingEdit() || !IsHistoryGestureActive());
}

bool TerrainLayerEditor::CanRedoDocument() const
{
	return m_Loaded && m_History.CanRedo();
}

void TerrainLayerEditor::UndoDocument()
{
	if (CanUndoDocument() && CommitDocumentEdit()) m_History.Undo();
}

void TerrainLayerEditor::RedoDocument()
{
	if (CanRedoDocument()) m_History.Redo();
}

void TerrainLayerEditor::ReleaseResourceDocument()
{
	m_Settings = {};
	m_History.Clear();
	m_Loaded = false;
}

bool TerrainLayerEditor::RestoreSavedResource()
{
	return LoadDocument();
}

void TerrainLayerEditor::DrawCustomToolbar()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Edit").c_str()))
	{
		DrawHistoryMenuItems("TerrainLayerEditor");
		ImGui::EndMenu();
	}
}

void TerrainLayerEditor::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	DrawCopyResourcePath();
	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "TerrainLayerEditor");
}

void TerrainLayerEditor::DrawCustomContent()
{
	if (!m_Loaded)
	{
		EditorUI::EmptyState("ResourceEditor.NoResource");
		return;
	}

	ImGui::TextDisabled("%s", LOCAL_CSTR("TerrainLayerEditor.Hint"));
	ImGui::Spacing();

	const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
	const auto object = reflection::ObjectView::From(m_Settings);
	const auto* type = registry.FindType(object);
	ImGui::BeginDisabled(Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath));
	if (type && ReflectionPropertyDrawer::DrawPropertyTable(*type, object, &registry))
		m_History.MarkEdited();
	ImGui::EndDisabled();

	// 드래그·입력이 끝난 프레임에 한 건으로 기록한다(다른 리소스 편집기와 같은 규칙).
	if (!IsHistoryGestureActive()) CommitDocumentEdit();
}
