#include "AnimatorGraphPanel.h"
#include "ComponentEnable.h"
#include "EditorUI.h"
#include "NodeEditorUI.h"
#include "AssetPathRelocation.h"
#include "IconsFontAwesome7.h"
#include "FileDialogs.h"
#include "SceneManager.h"
#include "AnimatorComponent.h"
#include "NameComponent.h"
#include "HierarchyComponent.h"
#include "SelectionManager.h"
#include "ResourcePicker.h"
#include "Entity.h"
#include "ResourceManager.h"
#include "Model.h"
#include "MaterialInterface.h"
#include "ShortcutManager.h"
#include "EditorSettings.h"
#include "Scene.h"
#include "PIEManager.h"
#include "NotificationManager.h"
#include <imgui_stdlib.h>
#include <imgui_node_editor.h>
#include <map>
#include <string_view>
#include <tuple>
#include <utility>
#include "InputManager.h"
#include <cmath>
#include <cstdio>

namespace ed = ax::NodeEditor;

AnimatorGraphPanel::AnimatorGraphPanel()
{
	m_Name = L"Animator Graph";
	m_DisplayNameKey = "AnimatorGraph.Title";
	ExecuteNew();
	InitializePreview();
}
AnimatorGraphPanel::~AnimatorGraphPanel()
{
	if (s_ActivePanel == this)
		s_ActivePanel = nullptr;

	if (m_EditorContext)
	{
		ed::DestroyEditor(m_EditorContext);
		m_EditorContext = nullptr;
	}

	m_PreviewWidget.Free();
}

ax::NodeEditor::EditorContext* AnimatorGraphPanel::CreateEditorContext()
{
	return NodeEditorUI::CreateDefaultEditor();
}

void AnimatorGraphPanel::ResetCanvasView()
{
	if (!m_EditorContext) return;
	auto* previous = ed::GetCurrentEditor();
	ed::SetCurrentEditor(m_EditorContext);
	ed::NavigateToContent(0.5f);
	ed::SetCurrentEditor(previous);
}

void AnimatorGraphPanel::AlignNodes(char direction)
{
	if (!CanUseCanvas() || !m_EditorContext) return;
	auto* previous = ed::GetCurrentEditor();
	ed::SetCurrentEditor(m_EditorContext);
	AlignSelectedNodes(direction);
	ed::SetCurrentEditor(previous);
}

void AnimatorGraphPanel::AlignSelectedNodes(char direction)
{
	if (!m_StateMachine) return;

	const int totalSelected = ed::GetSelectedObjectCount();
	if (totalSelected <= 0) return;

	std::vector<ed::NodeId> selectedNodes(totalSelected);
	const int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), totalSelected);
	selectedNodes.resize(nodeCount);

	struct NodeLayout
	{
		ed::NodeId EdId;
		std::string Name;
		ImVec2 Pos;
		ImVec2 Size;
	};
	std::vector<NodeLayout> selected;
	selected.reserve(selectedNodes.size());

	for (const auto& edId : selectedNodes)
	{
		const uintptr_t rawId = edId.Get();
		std::string name;
		if (rawId == 1) name = "__Entry__";
		else if (rawId == 2) name = "__AnyState__";
		else if (rawId >= 100)
		{
			const size_t idx = static_cast<size_t>(rawId - 100);
			if (idx < m_StateMachine->states.size())
				name = m_StateMachine->states[idx].name;
		}
		if (name.empty()) continue;

		const ImVec2 pos = ed::GetNodePosition(edId);
		const ImVec2 size = ed::GetNodeSize(edId);
		if (pos.x == FLT_MAX || size.x <= 0.0f) continue;
		selected.push_back({ edId, std::move(name), pos, size });
	}

	if (direction == 'H' || direction == 'V')
	{
		if (selected.size() < 3) return;
	}
	else if (selected.size() < 2)
	{
		return;
	}

	const auto moveNode = [&](const NodeLayout& item, const ImVec2& newPos)
		{
			ed::SetNodePosition(item.EdId, newPos);
			m_NodePositions[item.Name] = newPos;
		};

	switch (direction)
	{
	case 'W': // Align Top
	{
		float minY = FLT_MAX;
		for (const auto& item : selected) minY = std::min(minY, item.Pos.y);
		for (const auto& item : selected) moveNode(item, ImVec2(item.Pos.x, minY));
		break;
	}
	case 'S': // Align Bottom
	{
		float maxBottom = -FLT_MAX;
		for (const auto& item : selected) maxBottom = std::max(maxBottom, item.Pos.y + item.Size.y);
		for (const auto& item : selected) moveNode(item, ImVec2(item.Pos.x, maxBottom - item.Size.y));
		break;
	}
	case 'A': // Align Left
	{
		float minX = FLT_MAX;
		for (const auto& item : selected) minX = std::min(minX, item.Pos.x);
		for (const auto& item : selected) moveNode(item, ImVec2(minX, item.Pos.y));
		break;
	}
	case 'D': // Align Right
	{
		float maxRight = -FLT_MAX;
		for (const auto& item : selected) maxRight = std::max(maxRight, item.Pos.x + item.Size.x);
		for (const auto& item : selected) moveNode(item, ImVec2(maxRight - item.Size.x, item.Pos.y));
		break;
	}
	case 'H': // Distribute Horizontally
	{
		std::sort(selected.begin(), selected.end(), [](const NodeLayout& a, const NodeLayout& b) { return a.Pos.x < b.Pos.x; });
		float totalWidth = 0.0f;
		for (const auto& item : selected) totalWidth += item.Size.x;
		const float span = (selected.back().Pos.x + selected.back().Size.x) - selected.front().Pos.x;
		const float gap = (span - totalWidth) / static_cast<float>(selected.size() - 1);
		float currentX = selected.front().Pos.x;
		for (size_t i = 1; i + 1 < selected.size(); ++i)
		{
			currentX += selected[i - 1].Size.x + gap;
			moveNode(selected[i], ImVec2(currentX, selected[i].Pos.y));
		}
		break;
	}
	case 'V': // Distribute Vertically
	{
		std::sort(selected.begin(), selected.end(), [](const NodeLayout& a, const NodeLayout& b) { return a.Pos.y < b.Pos.y; });
		float totalHeight = 0.0f;
		for (const auto& item : selected) totalHeight += item.Size.y;
		const float span = (selected.back().Pos.y + selected.back().Size.y) - selected.front().Pos.y;
		const float gap = (span - totalHeight) / static_cast<float>(selected.size() - 1);
		float currentY = selected.front().Pos.y;
		for (size_t i = 1; i + 1 < selected.size(); ++i)
		{
			currentY += selected[i - 1].Size.y + gap;
			moveNode(selected[i], ImVec2(selected[i].Pos.x, currentY));
		}
		break;
	}
	default: return;
	}

	m_History.MarkEdited();
}

void AnimatorGraphPanel::SetStateMachine(const std::shared_ptr<Engine::AnimatorStateMachine>& sm)
{
	m_StateMachine = sm ? std::make_shared<Engine::AnimatorStateMachine>(*sm) : nullptr;
	m_NewDocument = false;
	m_TargetResourcePath.clear();
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_CurrentFilePath.clear();
	ResetHistoryFromStateMachine();
	m_NeedNavigateToContent = true;
	m_NeedNodePositionSync = true;
	SyncPreviewStateMachine();
}

void AnimatorGraphPanel::Update(f32 dt)
{
	UpdatePreview(dt);
}

string AnimatorGraphPanel::BuildResourceWindowTitle() const
{
	const auto caption = std::string(ICON_FA_DIAGRAM_PROJECT " ") + GetDisplayName();
	if (m_CurrentFilePath.empty()) return caption + "###AnimatorGraphPanel";
	return caption + " - " + WStrToStr(Engine::Utf8ToPath(m_CurrentFilePath).filename().wstring()) + "###AnimatorGraphPanel";
}

void AnimatorGraphPanel::OnResourceWindowVisibility(bool visible)
{
	if (visible && m_Focused) s_ActivePanel = this;
}

void AnimatorGraphPanel::DrawCustomContent()
{
	if (!m_StateMachine)
	{
		ExecuteNew();
	}

	const float totalWidth = ImGui::GetContentRegionAvail().x;
	const float contentHeight = ImGui::GetContentRegionAvail().y;
	if (totalWidth <= 0.0f || contentHeight <= 0.0f) return;

	const float splitterThickness = Editor::SplitterThickness;
	const float spacingX = ImGui::GetStyle().ItemSpacing.x;
	// 3단 패널(좌/중/우)과 2개 스플리터 사이에 총 4개의 ItemSpacing이 존재하므로 정확히 차감
	const float overhead = (splitterThickness * 2.0f) + (spacingX * 4.0f);

	const float minCanvasWidth = 200.0f;
	const float minPaneWidth = 160.0f;
	const float maxSideWidth = (std::max)(minPaneWidth, (totalWidth - minCanvasWidth - overhead) * 0.5f);

	m_LeftPaneWidth = std::clamp(m_LeftPaneWidth, minPaneWidth, maxSideWidth);
	m_InspectorWidth = std::clamp(m_InspectorWidth, minPaneWidth, maxSideWidth);
	float canvasWidth = (std::max)(minCanvasWidth, totalWidth - m_LeftPaneWidth - m_InspectorWidth - overhead);

	// ── [좌측 영역] 3D 프리뷰 뷰포트 + 파라미터 & 레이어 독 ──
	ImGui::BeginChild("LeftPane", ImVec2(m_LeftPaneWidth, contentHeight), true);
	if (m_ShowPreviewViewport)
	{
		const float previewHeight = std::clamp(m_LeftPaneWidth * 0.85f, 120.0f, std::max(120.0f, contentHeight * 0.35f));
		DrawPreviewSection(m_LeftPaneWidth - 16.0f, previewHeight);
		ImGui::Separator();
	}
	if (ImGui::BeginTabBar("LeftPaneTabs"))
	{
		if (ImGui::BeginTabItem(LOCAL_CSTR("Animator.Parameters")))
		{
			DrawParametersTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem(LOCAL_CSTR("PROP_ANIMATOR_LAYERS")))
		{
			DrawLayersTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::EndChild();
	ImGui::SameLine();

	// ── [좌측 스플리터: 좌측 ↔ 캔버스] ──
	Editor::SplitterV("##LeftSplitter", &m_LeftPaneWidth, &canvasWidth, minPaneWidth, minCanvasWidth, contentHeight, splitterThickness);
	ImGui::SameLine();

	// ── [중앙 영역] 상태 머신 노드 캔버스 및 레이어 탭 ──
	ImGui::BeginChild("CanvasPanel", ImVec2(canvasWidth, contentHeight), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	Engine::AnimatorComponent* canvasAnimator = nullptr;
	auto& pie = PIEManager::Get();
	if (PIEManager::IsCreated() && (pie.IsPlaying() || pie.IsPaused()))
	{
		auto* scene = Engine::SceneManager::Get().GetCurrentScene();
		const auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();
		if (scene && selected && selected->GetScene() == scene)
		{
			canvasAnimator = scene->GetRegistry().try_get<Engine::AnimatorComponent>(selected->GetEntityHandle());
		}
	}
	if (!canvasAnimator && m_PreviewModelEntity && m_PreviewWidget.GetScene())
	{
		canvasAnimator = m_PreviewWidget.GetRegistry().try_get<Engine::AnimatorComponent>(m_PreviewModelEntity->GetEntityHandle());
	}

	if (canvasAnimator && !canvasAnimator->layers.empty())
	{
		if (ImGui::BeginTabBar("CanvasLayerTabs"))
		{
			if (ImGui::BeginTabItem(LOCAL_CSTR("AnimatorGraph.Entry")))
			{
				m_SelectedLayerIndex = 0;
				ImGui::EndTabItem();
			}
			for (size_t i = 0; i < canvasAnimator->layers.size(); ++i)
			{
				const auto& layer = canvasAnimator->layers[i];
				std::string title = layer.name.empty() ? ("Layer " + std::to_string(i + 1)) : layer.name;
				title += " (" + std::to_string(static_cast<int>(layer.weight * 100.0f)) + "%)###CanvasTab_" + std::to_string(i);
				if (ImGui::BeginTabItem(title.c_str()))
				{
					m_SelectedLayerIndex = static_cast<int>(i + 1);
					ImGui::EndTabItem();
				}
			}
			ImGui::EndTabBar();
		}
	}
	else
	{
		m_SelectedLayerIndex = 0;
	}

	if (m_SelectedLayerIndex == 0)
	{
		DrawCanvas();
	}
	else if (canvasAnimator && m_SelectedLayerIndex - 1 < static_cast<int>(canvasAnimator->layers.size()))
	{
		auto& layer = canvasAnimator->layers[m_SelectedLayerIndex - 1];
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Layer Properties: %s", layer.name.c_str());
		ImGui::Separator();
		ImGui::InputText(LOCAL_CSTR("EditorCommon.Name"), &layer.name);
		ImGui::SliderFloat(LOCAL_LABEL("AnimatorGraph.Weight").c_str(), &layer.weight, 0.0f, 1.0f, "%.2f");

		std::string rootBoneStr(layer.mask.rootBoneName.begin(), layer.mask.rootBoneName.end());
		if (ImGui::InputText(LOCAL_LABEL("AnimatorGraph.MaskRootBone").c_str(), &rootBoneStr))
			layer.mask.rootBoneName = std::wstring(rootBoneStr.begin(), rootBoneStr.end());
		ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.RecursiveChildren").c_str(), &layer.mask.includeChildren);

		auto clip = layer.clip;
		if (DrawInspectorResourcePicker(clip, true))
			layer.clip = clip;

		ImGui::DragFloat(LOCAL_LABEL("AnimatorGraph.PlayRate").c_str(), &layer.playRate, 0.05f, 0.1f, 5.0f);
		ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.Looping").c_str(), &layer.isLooping);
		ImGui::SameLine();
		ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.Playing").c_str(), &layer.isPlaying);
	}
	ImGui::EndChild();
	ImGui::SameLine();

	// ── [우측 스플리터: 캔버스 ↔ 우측 인스펙터] ──
	Editor::SplitterV("##RightSplitter", &canvasWidth, &m_InspectorWidth, minCanvasWidth, minPaneWidth, contentHeight, splitterThickness);
	ImGui::SameLine();

	// ── [우측 영역] 인스펙터 패널 ──
	ImGui::BeginChild("InspectorPanel", ImVec2(m_InspectorWidth, contentHeight), true);
	DrawSelectionInspector();
	ImGui::EndChild();

	if (!IsHistoryGestureActive()) CommitDocumentEdit();
}

void AnimatorGraphPanel::ResetHistoryFromStateMachine()
{
	m_NodePositions.clear();
	if (!m_StateMachine)
	{
		m_History.Clear();
		return;
	}

	for (const auto& [name, position] : m_StateMachine->editorNodePositions)
		m_NodePositions.emplace(name, ImVec2(position[0], position[1]));
	m_NodePositions.try_emplace("__Entry__", 50.0f, 150.0f);
	m_NodePositions.try_emplace("__AnyState__", 50.0f, 60.0f);
	for (std::size_t i = 0; i < m_StateMachine->states.size(); ++i)
		m_NodePositions.try_emplace(m_StateMachine->states[i].name, 220.0f + static_cast<float>(i * 180), 150.0f);
	m_History.Reset({ *m_StateMachine, m_NodePositions });
}

bool AnimatorGraphPanel::CommitDocumentEdit()
{
	if (!m_History.HasPendingEdit()) return true;
	return m_StateMachine && m_History.Record({ *m_StateMachine, m_NodePositions },
		[this](const auto& snapshot) { return ApplyDocumentSnapshot(snapshot); }, LOCAL("History.Command.EditProperty"));
}

bool AnimatorGraphPanel::ApplyDocumentSnapshot(const DocumentSnapshot& snapshot)
{
	if (!m_StateMachine) return false;
	*m_StateMachine = snapshot.StateMachine;
	m_NodePositions = snapshot.Positions;
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	m_StateRenameDraft.clear();
	m_ParameterRenameDraft.clear();
	m_NeedNodePositionSync = true;
	if (m_EditorContext)
	{
		auto* previous = ed::GetCurrentEditor();
		ed::SetCurrentEditor(m_EditorContext);
		ed::ClearSelection();
		ed::SetCurrentEditor(previous);
	}
	SyncPreviewStateMachine();
	return true;
}

bool AnimatorGraphPanel::CanUndoDocument() const
{
	return m_History.CanUndo() && (!m_History.HasPendingEdit() || !IsHistoryGestureActive());
}

bool AnimatorGraphPanel::CanRedoDocument() const
{
	return m_History.CanRedo();
}

void AnimatorGraphPanel::UndoDocument()
{
	if (CanUndoDocument() && CommitDocumentEdit()) m_History.Undo();
}

void AnimatorGraphPanel::RedoDocument()
{
	if (CanRedoDocument()) m_History.Redo();
}

bool AnimatorGraphPanel::CanDeleteSelection() const
{
	if (!m_Open || !m_StateMachine) return false;

	const bool hasState = m_SelectedStateIndex >= 0 &&
		m_SelectedStateIndex < static_cast<int>(m_StateMachine->states.size());
	const bool hasTransition = m_SelectedTransitionIndex >= 0 &&
		m_SelectedTransitionIndex < static_cast<int>(m_StateMachine->transitions.size());

	return hasState || hasTransition;
}

void AnimatorGraphPanel::DeleteSelection()
{
	if (!CanDeleteSelection()) return;

	if (m_EditorContext)
	{
		auto* previous = ed::GetCurrentEditor();
		ed::SetCurrentEditor(m_EditorContext);

		const int totalSelected = ed::GetSelectedObjectCount();
		if (totalSelected > 0)
		{
			std::vector<ed::NodeId> selectedNodes(totalSelected);
			const int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), totalSelected);
			selectedNodes.resize(nodeCount);

			std::vector<ed::LinkId> selectedLinks(totalSelected);
			const int linkCount = ed::GetSelectedLinks(selectedLinks.data(), totalSelected);
			selectedLinks.resize(linkCount);

			ed::ClearSelection();
			ed::SetCurrentEditor(previous);

			// 선택된 링크 삭제
			for (const auto& linkId : selectedLinks)
			{
				const int linkIdx = static_cast<int>(linkId.Get()) - 100;
				if (linkIdx >= 0 && linkIdx < static_cast<int>(m_StateMachine->transitions.size()))
				{
					m_StateMachine->transitions.erase(m_StateMachine->transitions.begin() + linkIdx);
					m_History.MarkEdited();
				}
			}

			// 선택된 상태 노드 삭제 (내림차순 정렬하여 인덱스 안전 제거)
			std::vector<int> stateIndices;
			for (const auto& nodeId : selectedNodes)
			{
				const int stateIdx = static_cast<int>(nodeId.Get()) - 100;
				if (stateIdx >= 0 && stateIdx < static_cast<int>(m_StateMachine->states.size()))
					stateIndices.push_back(stateIdx);
			}
			std::sort(stateIndices.rbegin(), stateIndices.rend());
			for (int idx : stateIndices)
			{
				const std::string deletedName = m_StateMachine->states[idx].name;
				m_StateMachine->states.erase(m_StateMachine->states.begin() + idx);
				std::erase_if(m_StateMachine->transitions, [&](const Engine::AnimTransition& transition) {
					return transition.targetState == deletedName ||
						(!transition.fromAnyState && transition.sourceState == deletedName);
				});
				m_NodePositions.erase(deletedName);
				if (m_StateMachine->defaultState == deletedName)
				{
					m_StateMachine->defaultState = m_StateMachine->states.empty()
						? std::string{} : m_StateMachine->states.front().name;
				}
				m_History.MarkEdited();
			}

			m_SelectedStateIndex = -1;
			m_SelectedTransitionIndex = -1;
			m_ConnectingSourceIndex = -1;
			SyncPreviewStateMachine();
			return;
		}

		ed::SetCurrentEditor(previous);
	}

	const bool hasState = m_SelectedStateIndex >= 0 &&
		m_SelectedStateIndex < static_cast<int>(m_StateMachine->states.size());

	if (hasState)
	{
		const std::string deletedName = m_StateMachine->states[m_SelectedStateIndex].name;
		m_StateMachine->states.erase(m_StateMachine->states.begin() + m_SelectedStateIndex);

		std::erase_if(m_StateMachine->transitions, [&](const Engine::AnimTransition& transition) {
			return transition.targetState == deletedName ||
				(!transition.fromAnyState && transition.sourceState == deletedName);
			});

		m_NodePositions.erase(deletedName);

		if (m_StateMachine->defaultState == deletedName)
		{
			m_StateMachine->defaultState = m_StateMachine->states.empty()
				? std::string{} : m_StateMachine->states.front().name;
		}
	}
	else if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < static_cast<int>(m_StateMachine->transitions.size()))
	{
		m_StateMachine->transitions.erase(m_StateMachine->transitions.begin() + m_SelectedTransitionIndex);
	}

	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	m_History.MarkEdited();
	SyncPreviewStateMachine();
}

void AnimatorGraphPanel::DrawCustomToolbar()
{
	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
	{
		auto& sm = ShortcutManager::Get();
		if (ImGui::MenuItem((ICON_FA_FILE " " + LOCAL_LABEL("EditorCommon.New")).c_str(), sm.GetShortcutHint("Animator.New")))
			RequestNew();
		if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.Open")).c_str(), sm.GetShortcutHint("Animator.Open")))
			RequestOpen();
		DrawSaveMenuItem();
		if (ImGui::MenuItem((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.SaveAs")).c_str(), sm.GetShortcutHint("Animator.SaveAs")))
			SaveDocument(true);
		DrawCloseMenuItem();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.Edit")))
	{
		DrawHistoryMenuItems("Animator");
		ImGui::Separator();
		ACTION_MENU_ITEM((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(), "Animator.DeleteSelection");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.View")))
	{
		ACTION_MENU_ITEM((ICON_FA_MAXIMIZE " " + LOCAL_LABEL("AnimatorGraph.ResetView")).c_str(), "Animator.ResetView");
		ImGui::EndMenu();
	}
}

void AnimatorGraphPanel::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	if (toolbar.Icon("QuickNew", ICON_FA_FILE, "EditorCommon.New")) RequestNew();
	if (toolbar.Icon("QuickOpen", ICON_FA_FOLDER_OPEN, "EditorCommon.Open")) RequestOpen();
	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "Animator");

	const std::string fileName = m_CurrentFilePath.empty()
		? LOCAL("AnimatorGraph.Unsaved") : WStrToStr(Engine::Utf8ToPath(m_CurrentFilePath).filename().wstring());
	const std::string status = "[" + fileName + (HasUnsavedResourceChanges() ? " *]" : "]");
	toolbar.Text(status.c_str());

	EditorUI::IconButtonOptions previewOptions;
	previewOptions.Active = m_ShowPreviewViewport;
	if (toolbar.Icon("Preview", m_ShowPreviewViewport ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
		"AnimatorGraph.TogglePreview", previewOptions))
		m_ShowPreviewViewport = !m_ShowPreviewViewport;
	if (toolbar.Button("CreateState", ICON_FA_PLUS, "AnimatorGraph.CreateState"))
		AddState(ImVec2(250.0f, 150.0f));
	if (toolbar.Icon("ResetView", ICON_FA_MAXIMIZE, "AnimatorGraph.ResetView"))
		ResetCanvasView();

	if (m_ConnectingSourceIndex >= 0)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
		toolbar.Text(LOCAL_CSTR("AnimatorGraph.SelectTargetToConnect"), false);
		ImGui::PopStyleColor();
		if (toolbar.Button("CancelConnection", ICON_FA_XMARK, "AnimatorGraph.CancelEsc")) m_ConnectingSourceIndex = -1;
		if (Engine::InputManager::Get().IsKeyButtonDown(Engine::EKeyCode::Escape)) m_ConnectingSourceIndex = -1;
	}
}

bool AnimatorGraphPanel::IsSupported(const std::filesystem::path& assetPath) const
{
	const auto ext = assetPath.extension().wstring();
	const std::string lower = Engine::ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring()));
	return ext == L".bamanimator" || ext == L".animator" ||
		lower.ends_with(".bamanimator.json") || lower.ends_with(".animator.json");
}

void AnimatorGraphPanel::SetTargetResource(const std::filesystem::path& assetPath)
{
	LoadFromFile(assetPath);
}

bool AnimatorGraphPanel::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
{
	if (!PrepareTargetPathRelocation(relocation)) return false;
	relocation.AddUtf8(m_CurrentFilePath);
	return true;
}

bool AnimatorGraphPanel::LoadFromFile(const std::filesystem::path& path)
{
	const std::string targetPath = WStrToStr(path.wstring());
	auto tempStateMachine = std::make_shared<Engine::AnimatorStateMachine>();
	if (!tempStateMachine->LoadFromFile(targetPath))
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to load animator controller: {}", targetPath);
		return false;
	}

	m_StateMachine = std::move(tempStateMachine);
	m_NewDocument = false;
	m_CurrentFilePath = targetPath;
	m_TargetResourcePath = path;
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	ResetHistoryFromStateMachine();
	m_NeedNavigateToContent = true;
	m_NeedNodePositionSync = true;
	SyncPreviewStateMachine();
	return true;
}

bool AnimatorGraphPanel::OpenDocument()
{
	std::wstring selectedPath;
	const std::vector<FileDialogFilter> filters{
		{ StrToWStr(LOCAL("FileDialog.AnimatorFiles")), L"*.bamanimator.json;*.animator.json;*.bamanimator;*.animator" },
		{ StrToWStr(LOCAL("FileDialog.JsonFiles")), L"*.json" }
	};
	if (!FileDialogs::OpenFileDialog(selectedPath, filters)) return false;

	return LoadFromFile(std::filesystem::path(selectedPath));
}

bool AnimatorGraphPanel::SaveDocument(bool saveAs)
{
	std::string targetPath = m_CurrentFilePath;
	if (saveAs || targetPath.empty())
	{
		std::wstring selectedPath;
		const std::vector<FileDialogFilter> filters{
			{ StrToWStr(LOCAL("FileDialog.AnimatorFiles")), L"*.bamanimator.json;*.animator.json;*.bamanimator;*.animator" }
		};
		if (!FileDialogs::SaveFileDialog(selectedPath, filters, L"NewController.bamanimator.json", L"bamanimator.json")) return false;
		targetPath = WStrToStr(selectedPath);
	}

	if (targetPath.empty() || !m_StateMachine || !CommitDocumentEdit()) return false;

	m_StateMachine->editorNodePositions.clear();
	for (const auto& [name, position] : m_NodePositions)
		m_StateMachine->editorNodePositions.emplace(name, std::array<float, 2>{ position.x, position.y });

	if (!m_StateMachine->SaveToFile(targetPath))
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to save animator controller: {}", targetPath);
		return false;
	}

	m_CurrentFilePath = targetPath;
	m_TargetResourcePath = Engine::Utf8ToPath(targetPath);
	m_History.MarkSaved();
	m_NewDocument = false;
	return true;
}

void AnimatorGraphPanel::DrawParametersTab()
{
	if (!m_StateMachine) return;

	DrawPreviewParameters();
	ImGui::Separator();

	using ParamType = Engine::EAnimatorParamType;
	auto& parameters = m_StateMachine->defaultParameters;

	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
		"%s", LOCAL_CSTR("AnimatorGraph.DefaultParameters"));
	EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_PLUS, true).x);

	if (EditorUI::SmallIconButton("AddParameter", ICON_FA_PLUS, "EditorCommon.Add")) ImGui::OpenPopup("AddParamPopup");

	if (ImGui::BeginPopup("AddParamPopup"))
	{
		const auto addParameter = [&](const char* prefix, ParamType type)
			{
				size_t index = parameters.size();
				std::string name;
				do
				{
					name = prefix + std::to_string(index++);
				} while (parameters.contains(name));

				Engine::AnimatorParameter parameter{};
				parameter.type = type;
				parameters.emplace(name, parameter);
				m_History.MarkEdited();
			};

		if (ImGui::MenuItem(LOCAL_CSTR("AnimatorGraph.ParamFloat")))
			addParameter("NewFloat", ParamType::Float);
		if (ImGui::MenuItem(LOCAL_CSTR("AnimatorGraph.ParamInt")))
			addParameter("NewInt", ParamType::Int);
		if (ImGui::MenuItem(LOCAL_CSTR("AnimatorGraph.ParamBool")))
			addParameter("NewBool", ParamType::Bool);
		if (ImGui::MenuItem(LOCAL_CSTR("Animator.Trigger")))
			addParameter("NewTrigger", ParamType::Trigger);

		ImGui::EndPopup();
	}

	ImGui::Separator();
	// 표준 검색 바 (EditorUI::DrawSearchBar) 도입
	EditorUI::DrawSearchBar("AnimParamSearch", m_ParameterSearch, sizeof(m_ParameterSearch), "AnimatorGraph.SearchParameters");
	ImGui::Spacing();
	const std::string filterLower = Engine::ToLowerStr(m_ParameterSearch);
	std::optional<std::string> toRename;
	std::string toDelete;
	for (auto& [paramName, paramData] : parameters)
	{
		// 파라미터 이름 검색 필터링
		if (!filterLower.empty() && Engine::ToLowerStr(paramName).find(filterLower) == std::string::npos)
			continue;
		ImGui::PushID(paramName.c_str());

		if (EditorUI::SmallIconButton("RenameParameter", ICON_FA_PEN, "EditorCommon.Rename"))
		{
			m_ParameterRenameDraft = paramName;
			ImGui::OpenPopup("RenameParameter");
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Rename"));

		ImGui::SameLine();
		if (EditorUI::SmallIconButton("DeleteParameter", ICON_FA_TRASH, "EditorCommon.Delete"))
			toDelete = paramName;

		ImGui::SameLine();
		ImGui::TextUnformatted(paramName.c_str());

		ImGui::SetNextItemWidth(-1.0f);
		switch (paramData.type)
		{
		case ParamType::Float:
			if (ImGui::DragFloat("##Val", &paramData.floatValue, 0.05f)) m_History.MarkEdited();
			break;
		case ParamType::Int:
			if (ImGui::DragInt("##Val", &paramData.intValue)) m_History.MarkEdited();
			break;
		case ParamType::Bool:
		case ParamType::Trigger:
			if (ImGui::Checkbox("##Val", &paramData.boolValue)) m_History.MarkEdited();
			break;
		}

		if (ImGui::BeginPopup("RenameParameter"))
		{
			ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.Rename"));
			if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
			ImGui::InputText("##ParameterName", &m_ParameterRenameDraft);

			const bool changed = m_ParameterRenameDraft != paramName;
			const bool blank = m_ParameterRenameDraft.find_first_not_of(" \t\r\n") == std::string::npos;
			const bool duplicate = changed && parameters.contains(m_ParameterRenameDraft);

			if (blank || duplicate)
				ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.ParameterNameInvalid"));

			const EditorUI::DialogAction actions[] = {
				{ "EditorCommon.Cancel" }, { "EditorCommon.Rename", changed && !blank && !duplicate }
			};
			const int action = EditorUI::DialogButtonBar("RenameActions", actions, IM_ARRAYSIZE(actions));
			if (action == 1)
			{
				toRename = paramName;
				ImGui::CloseCurrentPopup();
			}
			if (action == 0)
				ImGui::CloseCurrentPopup();

			ImGui::EndPopup();
		}

		ImGui::PopID();
		ImGui::Spacing();
	}

	if (!toDelete.empty())
	{
		parameters.erase(toDelete);
		m_History.MarkEdited();
	}

	if (toRename)
		RenameParameter(*toRename, m_ParameterRenameDraft);
}

void AnimatorGraphPanel::RenameParameter(const std::string& oldName, const std::string& newName)
{
	auto& defaults = m_StateMachine->defaultParameters;
	const auto oldEntry = defaults.find(oldName);
	if (oldEntry == defaults.end()) return;

	const auto definition = oldEntry->second;
	if (!defaults.try_emplace(newName, definition).second) return;
	defaults.erase(oldName);

	for (auto& state : m_StateMachine->states)
	{
		if (state.blendTree.parameterName == oldName)
			state.blendTree.parameterName = newName;
		if (state.blendTree2D.parameterX == oldName)
			state.blendTree2D.parameterX = newName;
		if (state.blendTree2D.parameterY == oldName)
			state.blendTree2D.parameterY = newName;
	}

	const auto renameConditions = [&](std::vector<Engine::TransitionCondition>& conditions)
		{
			for (auto& condition : conditions)
			{
				if (condition.parameterName == oldName)
					condition.parameterName = newName;
			}
		};

	for (auto& transition : m_StateMachine->transitions)
	{
		renameConditions(transition.conditions);
		for (auto& group : transition.conditionGroups)
			renameConditions(group.conditions);
	}

	if (m_PreviewModelEntity && m_PreviewWidget.GetScene())
	{
		auto& world = m_PreviewWidget.GetRegistry();
		const auto entity = m_PreviewModelEntity->GetEntityHandle();
		auto* animator = world.try_get<Engine::AnimatorComponent>(entity);

		if (animator && animator->runtimeState.GetStateMachine() == m_StateMachine)
		{
			auto& values = animator->runtimeState.GetParameters().GetAll();
			const auto oldValue = values.find(oldName);
			if (oldValue != values.end())
			{
				const auto currentValue = oldValue->second;
				values.insert_or_assign(newName, currentValue);
				values.erase(oldName);
			}
		}
	}

	m_History.MarkEdited();
}

void AnimatorGraphPanel::DrawLayersTab()
{
	Engine::AnimatorComponent* targetAnimator = nullptr;
	const bool isPIE = PIEManager::IsCreated() && (PIEManager::Get().IsPlaying() || PIEManager::Get().IsPaused());
	if (isPIE)
	{
		auto* scene = Engine::SceneManager::Get().GetCurrentScene();
		const auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();
		if (scene && selected && selected->GetScene() == scene)
		{
			auto& world = scene->GetRegistry();
			auto entity = selected->GetEntityHandle();
			while (entity != entt::null && world.valid(entity))
			{
				if (auto* anim = world.try_get<Engine::AnimatorComponent>(entity))
				{
					targetAnimator = anim;
					break;
				}
				const auto* hierarchy = world.try_get<Engine::HierarchyComponent>(entity);
				entity = hierarchy ? hierarchy->parent : entt::null;
			}
		}

		if (!targetAnimator && scene)
		{
			auto& world = scene->GetRegistry();
			for (auto [entity, anim] : world.view<Engine::AnimatorComponent>().each())
			{
				if ((!m_CurrentFilePath.empty() && anim.controllerPath == m_CurrentFilePath) ||
					(anim.runtimeState.HasStateMachine() && anim.runtimeState.GetStateMachine() == m_StateMachine))
				{
					targetAnimator = &anim;
					break;
				}
			}
		}
	}

	if (!targetAnimator && m_PreviewModelEntity && m_PreviewWidget.GetScene())
	{
		auto& world = m_PreviewWidget.GetRegistry();
		targetAnimator = world.try_get<Engine::AnimatorComponent>(m_PreviewModelEntity->GetEntityHandle());
	}

	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("PROP_ANIMATOR_LAYERS"));

	if (!targetAnimator)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.PreviewUnavailable"));
		return;
	}

	ImGui::PushID("AnimatorLayersTab");

	// Base Layer
	if (ImGui::CollapsingHeader(LOCAL_LABEL("AnimatorGraph.BaseLayer").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("Weight: 1.00 (Base)");
		ImGui::TextDisabled("Mask: Full Body");
	}

	// Additional Layers
	int toDeleteIndex = -1;
	for (size_t i = 0; i < targetAnimator->layers.size(); ++i)
	{
		auto& layer = targetAnimator->layers[i];
		ImGui::PushID(static_cast<int>(i));

		std::string headerTitle = layer.name.empty() ? ("Layer " + std::to_string(i + 1)) : layer.name;
		headerTitle += " (" + std::to_string(static_cast<int>(layer.weight * 100.0f)) + "%)###LayerHeader";

		if (ImGui::CollapsingHeader(headerTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::InputText(LOCAL_CSTR("EditorCommon.Name"), &layer.name);
			ImGui::SliderFloat(LOCAL_LABEL("AnimatorGraph.Weight").c_str(), &layer.weight, 0.0f, 1.0f, "%.2f");

			std::string rootBoneStr(layer.mask.rootBoneName.begin(), layer.mask.rootBoneName.end());
			if (ImGui::InputText(LOCAL_LABEL("AnimatorGraph.MaskRootBone").c_str(), &rootBoneStr))
			{
				layer.mask.rootBoneName = std::wstring(rootBoneStr.begin(), rootBoneStr.end());
			}
			ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.RecursiveChildren").c_str(), &layer.mask.includeChildren);
			if (!layer.mask.boneWeights.empty())
			{
				ImGui::TextDisabled("Evaluated Bones: %zu", layer.mask.boneWeights.size());
			}

			auto clip = layer.clip;
			if (DrawInspectorResourcePicker(clip, true))
			{
				layer.clip = clip;
			}

			ImGui::DragFloat(LOCAL_LABEL("AnimatorGraph.PlayRate").c_str(), &layer.playRate, 0.05f, 0.1f, 5.0f);
			ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.Looping").c_str(), &layer.isLooping);
			ImGui::SameLine();
			ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.Playing").c_str(), &layer.isPlaying);

			if (ImGui::Button(LOCAL_CSTR("EditorCommon.Delete")))
			{
				toDeleteIndex = static_cast<int>(i);
			}
		}
		ImGui::PopID();
	}

	if (toDeleteIndex >= 0 && toDeleteIndex < static_cast<int>(targetAnimator->layers.size()))
	{
		targetAnimator->layers.erase(targetAnimator->layers.begin() + toDeleteIndex);
	}

	ImGui::Spacing();
	if (ImGui::Button(LOCAL_CSTR("EditorCommon.Add")))
	{
		targetAnimator->layers.push_back(Engine::AnimatorLayer{
			"UpperBody",
			1.0f,
			Engine::BoneMask{ L"Spine", true },
			{},
			1.0f,
			true,
			true,
			0.0f
		});
	}

	ImGui::PopID();
}

void AnimatorGraphPanel::DrawPreviewParameters()
{
	bool isPIE = false;
	if (PIEManager::IsCreated())
	{
		auto& pie = PIEManager::Get();
		isPIE = pie.IsPlaying() || pie.IsPaused();
	}

	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
		"%s", isPIE ? "[PIE Live Parameters]" : LOCAL_CSTR("AnimatorGraph.PreviewParameters"));

	Engine::AnimatorRuntimeState* activeRuntime = nullptr;
	Engine::AnimatorComponent* activeAnimator = nullptr;

	if (isPIE)
	{
		auto* scene = Engine::SceneManager::Get().GetCurrentScene();
		const auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();
		if (scene && selected && selected->GetScene() == scene)
		{
			auto& world = scene->GetRegistry();
			auto entity = selected->GetEntityHandle();
			while (entity != entt::null && world.valid(entity))
			{
				if (auto* anim = world.try_get<Engine::AnimatorComponent>(entity))
				{
					if (anim->runtimeState.HasStateMachine())
					{
						activeRuntime = &anim->runtimeState;
						activeAnimator = anim;
						break;
					}
				}
				const auto* hierarchy = world.try_get<Engine::HierarchyComponent>(entity);
				entity = hierarchy ? hierarchy->parent : entt::null;
			}
		}

		if (!activeRuntime && scene)
		{
			auto& world = scene->GetRegistry();
			for (auto [entity, anim] : world.view<Engine::AnimatorComponent>().each())
			{
				if (anim.runtimeState.HasStateMachine() &&
					((!m_CurrentFilePath.empty() && anim.controllerPath == m_CurrentFilePath) ||
					 anim.runtimeState.GetStateMachine() == m_StateMachine))
				{
					activeRuntime = &anim.runtimeState;
					activeAnimator = &anim;
					break;
				}
			}
		}
	}
	else if (m_ShowPreviewViewport && m_PreviewWidget.GetScene() && m_PreviewModelEntity)
	{
		auto& world = m_PreviewWidget.GetRegistry();
		const auto entity = m_PreviewModelEntity->GetEntityHandle();
		auto* animator = world.valid(entity) ? world.try_get<Engine::AnimatorComponent>(entity) : nullptr;
		if (animator && animator->runtimeState.GetStateMachine() == m_StateMachine)
		{
			activeRuntime = &animator->runtimeState;
			activeAnimator = animator;
		}
	}

	if (!m_StateMachine || !activeRuntime || !activeAnimator)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.PreviewUnavailable"));
		return;
	}

	auto& runtime = *activeRuntime;
	auto* animator = activeAnimator;

	ImGui::PushID("PreviewParameters");
	const auto& defaultState = m_StateMachine->defaultState;
	const bool canRestart = !defaultState.empty() && m_StateMachine->FindState(defaultState) != nullptr;

	if (!isPIE)
	{
		ImGui::BeginDisabled(!canRestart);
		if (ImGui::Button(LOCAL_LABEL("AnimatorGraph.PreviewRestart").c_str()))
			SyncPreviewStateMachine();
		ImGui::EndDisabled();

		if (!canRestart && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", LOCAL_CSTR("AnimatorGraph.PreviewRestartNeedsEntry"));
	}

	const auto& currentState = runtime.GetCurrentStateName();
	const auto targetState = runtime.GetTransitionTargetStateName();

	if (!animator->isPlaying)
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.PreviewPlaybackPaused"));
	}
	else if (currentState.empty() || !m_StateMachine->FindState(currentState))
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.PreviewStateMissing"));
	}
	else if (!targetState.empty())
	{
		const float percent = std::clamp(runtime.GetTransitionProgress(), 0.0f, 1.0f) * 100.0f;
		const auto status = LOCAL_FMT("AnimatorGraph.PreviewTransitionStatus", currentState, targetState, percent);
		ImGui::TextWrapped("%s", status.c_str());
	}
	else
	{
		ImGui::TextWrapped("%s", LOCAL_FMT("AnimatorGraph.PreviewCurrentState", currentState).c_str());
	}

	for (const auto& [name, parameter] : runtime.GetParameters().GetAll())
	{
		ImGui::PushID(name.c_str());
		ImGui::TextUnformatted(name.c_str());
		ImGui::SetNextItemWidth(-1.0f);

		switch (parameter.type)
		{
		case Engine::EAnimatorParamType::Float:
		{
			float value = parameter.floatValue;
			if (ImGui::DragFloat("##Value", &value, 0.05f))
				runtime.SetFloat(name, value);
			break;
		}
		case Engine::EAnimatorParamType::Int:
		{
			int32 value = parameter.intValue;
			if (ImGui::DragInt("##Value", &value))
				runtime.SetInt(name, value);
			break;
		}
		case Engine::EAnimatorParamType::Bool:
		{
			bool value = parameter.boolValue;
			if (ImGui::Checkbox("##Value", &value))
				runtime.SetBool(name, value);
			break;
		}
		case Engine::EAnimatorParamType::Trigger:
		{
			const std::string label = LOCAL("Animator.Trigger") + "###FireTrigger";
			if (ImGui::Button(label.c_str()))
				runtime.SetTrigger(name);
			break;
		}
		}

		ImGui::PopID();
		ImGui::Spacing();
	}
	ImGui::PopID();
}

void AnimatorGraphPanel::DrawCanvas()
{
	if (!m_StateMachine) return;
	if (!m_EditorContext) m_EditorContext = CreateEditorContext();

	auto* previousContext = ed::GetCurrentEditor();
	ed::SetCurrentEditor(m_EditorContext);
	ed::EnableShortcuts(false);

	auto& input = Engine::InputManager::Get();
	if (input.IsKeyButtonPressed(Engine::EKeyCode::Escape))
		m_ConnectingSourceIndex = -1;

	// ── 0. 현재 씬 또는 프리뷰에서 선택된 엔티티의 런타임 애니메이터 상태 조회 ──
	const Engine::AnimatorRuntimeState* observedRuntime = nullptr;

	bool isPIE = false;
	if (PIEManager::IsCreated())
	{
		auto& pie = PIEManager::Get();
		isPIE = pie.IsPlaying() || pie.IsPaused();
	}

	if (isPIE)
	{
		auto* scene = Engine::SceneManager::Get().GetCurrentScene();
		const auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();

		if (scene && selected && selected->GetScene() == scene)
		{
			auto& world = scene->GetRegistry();
			auto entity = selected->GetEntityHandle();

			while (entity != entt::null && world.valid(entity))
			{
				if (const auto* animator = world.try_get<Engine::AnimatorComponent>(entity))
				{
					if (animator->runtimeState.HasStateMachine())
					{
						observedRuntime = &animator->runtimeState;
						break;
					}
				}

				const auto* hierarchy = world.try_get<Engine::HierarchyComponent>(entity);
				entity = hierarchy ? hierarchy->parent : entt::null;
			}
		}

		if (!observedRuntime && scene)
		{
			auto& world = scene->GetRegistry();
			for (auto [entity, anim] : world.view<const Engine::AnimatorComponent>().each())
			{
				if (anim.runtimeState.HasStateMachine() &&
					((!m_CurrentFilePath.empty() && anim.controllerPath == m_CurrentFilePath) ||
					 anim.runtimeState.GetStateMachine() == m_StateMachine))
				{
					observedRuntime = &anim.runtimeState;
					break;
				}
			}
		}
	}
	else if (m_ShowPreviewViewport)
	{
		if (m_PreviewModelEntity && m_PreviewWidget.GetScene())
		{
			auto& world = m_PreviewWidget.GetRegistry();
			const auto entity = m_PreviewModelEntity->GetEntityHandle();
			const auto* animator = world.try_get<Engine::AnimatorComponent>(entity);

			if (animator && animator->runtimeState.GetStateMachine() == m_StateMachine)
				observedRuntime = &animator->runtimeState;
		}
	}
	else
	{
		auto* scene = Engine::SceneManager::Get().GetCurrentScene();
		const auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();

		if (scene && selected && selected->GetScene() == scene)
		{
			auto& world = scene->GetRegistry();
			auto entity = selected->GetEntityHandle();

			while (entity != entt::null && world.valid(entity))
			{
				if (const auto* animator = world.try_get<Engine::AnimatorComponent>(entity))
				{
					if (animator->runtimeState.HasStateMachine())
						observedRuntime = &animator->runtimeState;
					break;
				}

				const auto* hierarchy = world.try_get<Engine::HierarchyComponent>(entity);
				entity = hierarchy ? hierarchy->parent : entt::null;
			}
		}
	}

	const std::string liveActiveState = observedRuntime
		? observedRuntime->GetCurrentStateName() : std::string{};

	const bool isLiveTransitioning = observedRuntime && observedRuntime->IsTransitioning();
	const float liveTransitionProgress = isLiveTransitioning
		? std::clamp(observedRuntime->GetTransitionProgress(), 0.0f, 1.0f) : 0.0f;

	ed::Begin("##AnimatorCanvas", ImVec2(0.0f, 0.0f));

	// ── 0. 초기화 시 노드 위치 동기화 (최초 1회 설정) ──
	if (m_NeedNodePositionSync)
	{
		if (!m_NodePositions.contains("__Entry__"))
			m_NodePositions["__Entry__"] = ImVec2(50.0f, 150.0f);
		ed::SetNodePosition(1, m_NodePositions["__Entry__"]);

		if (!m_NodePositions.contains("__AnyState__"))
			m_NodePositions["__AnyState__"] = ImVec2(50.0f, 60.0f);
		ed::SetNodePosition(2, m_NodePositions["__AnyState__"]);

		for (size_t i = 0; i < m_StateMachine->states.size(); ++i)
		{
			const auto& state = m_StateMachine->states[i];
			if (!m_NodePositions.contains(state.name))
				m_NodePositions[state.name] = ImVec2(220.0f + static_cast<float>(i * 180), 150.0f);
			ed::SetNodePosition(100 + static_cast<uintptr_t>(i), m_NodePositions[state.name]);
		}
		m_NeedNodePositionSync = false;
	}

	// ── 1. Entry 노드 렌더링 (ID: 1, LeftOut: 11, RightOut: 10) ──
	{
		ed::BeginNode(1);
		ImGui::PushID("##EntryNode");
		ImGui::BeginGroup();

		const float entryHeaderBottom = NodeEditorUI::BeginNodeHeader(
			LOCAL_CSTR("AnimatorGraph.Entry"), 130.0f);

		ImGui::Spacing();
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.DefaultEntryHint"));
		const float centerY = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;

		ImGui::EndGroup();

		// ★ EndNode() 이전에 좌/우 양측 출력 핀 선언
		NodeEditorUI::DrawNodeBorderPins(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {
			{ 11, ed::PinKind::Output, centerY, NodeEditorUI::Colors::PinEntry, NodeEditorUI::BorderSide::Left },
			{ 10, ed::PinKind::Output, centerY, NodeEditorUI::Colors::PinEntry, NodeEditorUI::BorderSide::Right }
		});

		ImGui::PopID();
		ed::EndNode();

		const ImVec2 nodeMin = ImGui::GetItemRectMin();
		const ImVec2 nodeMax = ImGui::GetItemRectMax();
		NodeEditorUI::DrawNodeHeader(1, nodeMin, nodeMax, entryHeaderBottom, NodeEditorUI::Colors::HeaderEntry);
	}

	// ── 2. Any State 노드 렌더링 (ID: 2, LeftOut: 21, RightOut: 20) ──
	{
		ed::BeginNode(2);
		ImGui::PushID("##AnyStateNode");
		ImGui::BeginGroup();

		const float anyHeaderBottom = NodeEditorUI::BeginNodeHeader(
			LOCAL_CSTR("AnimatorGraph.AnyState"), 130.0f);

		ImGui::Spacing();
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.GlobalTransitions"));
		const float centerY = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;

		ImGui::EndGroup();

		// ★ EndNode() 이전에 좌/우 양측 출력 핀 선언
		NodeEditorUI::DrawNodeBorderPins(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {
			{ 21, ed::PinKind::Output, centerY, NodeEditorUI::Colors::PinAnyState, NodeEditorUI::BorderSide::Left },
			{ 20, ed::PinKind::Output, centerY, NodeEditorUI::Colors::PinAnyState, NodeEditorUI::BorderSide::Right }
		});

		ImGui::PopID();
		ed::EndNode();

		const ImVec2 nodeMin = ImGui::GetItemRectMin();
		const ImVec2 nodeMax = ImGui::GetItemRectMax();
		NodeEditorUI::DrawNodeHeader(2, nodeMin, nodeMax, anyHeaderBottom, NodeEditorUI::Colors::HeaderAnyState);
	}

	// ── 3. 상태 노드들 렌더링 (ID: 100 + i, 4-Way Pins: LeftIn/RightIn/LeftOut/RightOut) ──
	for (size_t i = 0; i < m_StateMachine->states.size(); ++i)
	{
		const auto& state = m_StateMachine->states[i];
		const bool isDefault = (state.name == m_StateMachine->defaultState);
		const bool isLiveActive = (!liveActiveState.empty() && liveActiveState == state.name);
		const uintptr_t nodeId = 100 + static_cast<uintptr_t>(i);

		ed::BeginNode(nodeId);
		ImGui::PushID(static_cast<int>(i));
		ImGui::BeginGroup();

		// 헤더 (상태 명칭 및 (Default) 배지)
		const char* badge = isDefault ? "(Default)" : nullptr;
		const ImVec4 badgeCol = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
		const float stateHeaderBottom = NodeEditorUI::BeginNodeHeader(state.name.c_str(), 160.0f, badge, badgeCol);

		ImGui::Spacing();

		// 상단 In 핀 Y좌표
		const float inCenterY = ImGui::GetCursorScreenPos().y + ImGui::GetFontSize() * 0.45f;

		// 본문 내용 (재생할 클립 또는 BlendTree 설명)
		std::string subText;
		if (state.IsBlendTree2D())
			subText = LOCAL("AnimatorGraph.BlendTree2D");
		else if (state.IsBlendTree())
			subText = LOCAL("AnimatorGraph.BlendTree1D");
		else
			subText = state.clip.Get() ? LOCAL("AnimatorGraph.Clip") : LOCAL("AnimatorGraph.NoMotion");

		ImGui::TextUnformatted(subText.c_str());

		if (isLiveActive)
		{
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.LiveActiveState"));
			if (isLiveTransitioning)
				ImGui::ProgressBar(liveTransitionProgress, ImVec2(140.0f, 4.0f), "");
		}

		// 하단 Out 핀과의 충분한 수직 간격 확보 (최소 18px 분리)
		const float currentY = ImGui::GetCursorScreenPos().y;
		const float outCenterY = (currentY - inCenterY < 18.0f) ? (inCenterY + 18.0f) : (currentY + ImGui::GetFontSize() * 0.25f);
		if (currentY < outCenterY)
			ImGui::Dummy(ImVec2(0.0f, outCenterY - currentY + 4.0f));
		else
			ImGui::Spacing();

		ImGui::EndGroup();

		// ★ 노드 테두리 양쪽에 In(상단)/Out(하단) 핀 쌍 선언
		// LeftIn: 1000 + i*4 + 0, RightIn: 1000 + i*4 + 1
		// LeftOut: 1000 + i*4 + 2, RightOut: 1000 + i*4 + 3
		const uintptr_t pinBase = 1000 + static_cast<uintptr_t>(i * 4);
		NodeEditorUI::DrawNodeBorderPins(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), {
			{ pinBase + 0, ed::PinKind::Input, inCenterY, NodeEditorUI::Colors::PinInput, NodeEditorUI::BorderSide::Left },
			{ pinBase + 1, ed::PinKind::Input, inCenterY, NodeEditorUI::Colors::PinInput, NodeEditorUI::BorderSide::Right },
			{ pinBase + 2, ed::PinKind::Output, outCenterY, NodeEditorUI::Colors::PinOutput, NodeEditorUI::BorderSide::Left },
			{ pinBase + 3, ed::PinKind::Output, outCenterY, NodeEditorUI::Colors::PinOutput, NodeEditorUI::BorderSide::Right }
		});

		ImGui::PopID();
		ed::EndNode();

		const ImVec2 nodeMin = ImGui::GetItemRectMin();
		const ImVec2 nodeMax = ImGui::GetItemRectMax();

		const ImU32 stateHeaderColor = isLiveActive ? NodeEditorUI::Colors::HeaderStateActive
			: (isDefault ? NodeEditorUI::Colors::HeaderDefaultState : NodeEditorUI::Colors::HeaderState);
		NodeEditorUI::DrawNodeHeader(nodeId, nodeMin, nodeMax, stateHeaderBottom, stateHeaderColor);

		// "Make Transition" 메뉴를 통한 대상 클릭 연결 지원
		if (m_ConnectingSourceIndex >= 0 && m_ConnectingSourceIndex != static_cast<int>(i))
		{
			if (ed::GetHoveredNode() == ed::NodeId(nodeId) &&
				input.IsMouseButtonDown(Engine::EMouseButton::Left))
			{
				Engine::AnimTransition newTr;
				newTr.sourceState = m_StateMachine->states[m_ConnectingSourceIndex].name;
				newTr.targetState = state.name;
				newTr.duration = 0.25f;
				m_StateMachine->transitions.push_back(std::move(newTr));
				m_SelectedTransitionIndex = static_cast<int>(m_StateMachine->transitions.size() - 1);
				m_SelectedStateIndex = -1;
				m_ConnectingSourceIndex = -1;
				m_History.MarkEdited();
			}
		}
	}

	// ── 4. 트랜지션(링크) 렌더링 ──
	DrawTransitions(observedRuntime);

	// ── 4-1. 모든 노드 및 링크 배치 완료 후 안전하게 뷰포트 내비게이션 ──
	if (m_NeedNavigateToContent)
	{
		ed::NavigateToContent(0.0f);
		m_NeedNavigateToContent = false;
	}

	// ── 5. 신규 와이어(트랜지션) 연결 인터랙션 ──
	if (ed::BeginCreate())
	{
		ed::PinId pinAId, pinBId;
		if (ed::QueryNewLink(&pinAId, &pinBId))
		{
			if (pinAId && pinBId)
			{
				const int idA = static_cast<int>(pinAId.Get());
				const int idB = static_cast<int>(pinBId.Get());

				auto isOutputPin = [](int id) -> bool {
					return id == 10 || id == 11 || id == 20 || id == 21 ||
					       (id >= 1000 && (((id - 1000) % 4 == 2) || ((id - 1000) % 4 == 3)));
				};
				auto isInputPin = [](int id) -> bool {
					return id >= 1000 && (((id - 1000) % 4 == 0) || ((id - 1000) % 4 == 1));
				};

				const int outPin = isOutputPin(idA) ? idA : (isOutputPin(idB) ? idB : 0);
				const int inPin = isInputPin(idA) ? idA : (isInputPin(idB) ? idB : 0);

				if (outPin != 0 && inPin != 0)
				{
					if (ed::AcceptNewItem(ImVec4(0.3f, 0.85f, 0.4f, 1.0f), 2.5f))
					{
						const int targetIdx = (inPin - 1000) / 4;
						if (targetIdx >= 0 && targetIdx < static_cast<int>(m_StateMachine->states.size()))
						{
							const auto& targetState = m_StateMachine->states[targetIdx];
							if (outPin == 10 || outPin == 11) // Entry 노드 -> DefaultState 지정
							{
								m_StateMachine->defaultState = targetState.name;
								m_History.MarkEdited();
							}
							else if (outPin == 20 || outPin == 21) // AnyState 노드 -> AnyState 전이
							{
								Engine::AnimTransition newTr;
								newTr.fromAnyState = true;
								newTr.targetState = targetState.name;
								newTr.duration = 0.25f;
								m_StateMachine->transitions.push_back(std::move(newTr));
								m_SelectedTransitionIndex = static_cast<int>(m_StateMachine->transitions.size() - 1);
								m_SelectedStateIndex = -1;
								m_ConnectingSourceIndex = -1;
								m_History.MarkEdited();
							}
							else // 일반 상태 노드 -> 상태 간 전이
							{
								const int srcIdx = (outPin - 1000) / 4;
								if (srcIdx >= 0 && srcIdx < static_cast<int>(m_StateMachine->states.size()))
								{
									Engine::AnimTransition newTr;
									newTr.sourceState = m_StateMachine->states[srcIdx].name;
									newTr.targetState = targetState.name;
									newTr.duration = 0.25f;
									m_StateMachine->transitions.push_back(std::move(newTr));
									m_SelectedTransitionIndex = static_cast<int>(m_StateMachine->transitions.size() - 1);
									m_SelectedStateIndex = -1;
									m_ConnectingSourceIndex = -1;
									m_History.MarkEdited();
								}
							}
						}
					}
				}
				else
				{
					ed::RejectNewItem(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 2.0f);
				}
			}
		}
		ed::EndCreate();
	}

	// ── 6. 링크/노드 삭제 인터랙션 ──
	if (ed::BeginDelete())
	{
		ed::NodeId delNodeId;
		while (ed::QueryDeletedNode(&delNodeId))
		{
			const uintptr_t rawId = delNodeId.Get();
			if (rawId >= 100)
			{
				if (ed::AcceptDeletedItem())
				{
					m_SelectedStateIndex = static_cast<int>(rawId - 100);
					m_SelectedTransitionIndex = -1;
					DeleteSelection();
				}
			}
			else
			{
				ed::RejectDeletedItem();
			}
		}

		ed::LinkId delLinkId;
		while (ed::QueryDeletedLink(&delLinkId))
		{
			const uintptr_t rawId = delLinkId.Get();
			if (rawId >= 100)
			{
				if (ed::AcceptDeletedItem())
				{
					m_SelectedTransitionIndex = static_cast<int>(rawId - 100);
					m_SelectedStateIndex = -1;
					DeleteSelection();
				}
			}
			else
			{
				ed::RejectDeletedItem(); // Entry 링크는 삭제 거부
			}
		}
		ed::EndDelete();
	}

	// ── 7. 선택 상태 읽기 ──
	if (ed::GetSelectedObjectCount() > 0)
	{
		std::vector<ed::NodeId> selNodes(ed::GetSelectedObjectCount());
		const int nodeCount = ed::GetSelectedNodes(selNodes.data(), static_cast<int>(selNodes.size()));
		selNodes.resize(nodeCount);

		std::vector<ed::LinkId> selLinks(ed::GetSelectedObjectCount());
		const int linkCount = ed::GetSelectedLinks(selLinks.data(), static_cast<int>(selLinks.size()));
		selLinks.resize(linkCount);

		if (!selNodes.empty())
		{
			const uintptr_t rawNodeId = selNodes[0].Get();
			if (rawNodeId >= 100)
			{
				m_SelectedStateIndex = static_cast<int>(rawNodeId - 100);
				m_SelectedTransitionIndex = -1;
			}
		}
		else if (!selLinks.empty())
		{
			const uintptr_t rawLinkId = selLinks[0].Get();
			if (rawLinkId >= 100)
			{
				m_SelectedTransitionIndex = static_cast<int>(rawLinkId - 100);
				m_SelectedStateIndex = -1;
			}
		}
	}

	// ── 8. 노드 위치 동기화 (프레임 종료 전 갱신) ──
	for (size_t i = 0; i < m_StateMachine->states.size(); ++i)
	{
		const auto& state = m_StateMachine->states[i];
		const ImVec2 curPos = ed::GetNodePosition(100 + static_cast<uintptr_t>(i));
		if (curPos.x != FLT_MAX && curPos.y != FLT_MAX)
		{
			auto& savedPos = m_NodePositions[state.name];
			if (std::abs(curPos.x - savedPos.x) > 0.5f || std::abs(curPos.y - savedPos.y) > 0.5f)
			{
				savedPos = curPos;
				m_History.MarkEdited();
			}
		}
	}
	for (const auto& [name, id] : { std::pair{ "__Entry__", 1 }, std::pair{ "__AnyState__", 2 } })
	{
		const ImVec2 position = ed::GetNodePosition(id);
		if (position.x == FLT_MAX || position.y == FLT_MAX) continue;
		auto& saved = m_NodePositions[name];
		if (std::abs(position.x - saved.x) > 0.5f || std::abs(position.y - saved.y) > 0.5f)
		{
			saved = position;
			m_History.MarkEdited();
		}
	}

	ed::End();

	// ── 9. 우클릭 컨텍스트 메뉴 처리 ──
	DrawCanvasContextMenu();

	ed::SetCurrentEditor(previousContext);
}

void AnimatorGraphPanel::DrawTransitions(const Engine::AnimatorRuntimeState* observedRuntime)
{
	if (!m_StateMachine) return;

	auto findStateIndex = [this](const std::string& name) -> int {
		for (size_t i = 0; i < m_StateMachine->states.size(); ++i)
		{
			if (m_StateMachine->states[i].name == name)
				return static_cast<int>(i);
		}
		return -1;
	};

	// 1. Entry -> DefaultState 링크 (LinkId: 1)
	const int defIdx = findStateIndex(m_StateMachine->defaultState);
	if (defIdx >= 0)
	{
		const ImVec2 entryPos = ed::GetNodePosition(1);
		const ImVec2 defPos = ed::GetNodePosition(100 + static_cast<uintptr_t>(defIdx));
		// DefaultState 노드가 Entry 노드보다 왼쪽에 있으면 Entry의 LeftOut(11) -> def의 RightIn(1)
		// 오른쪽에 있으면 Entry의 RightOut(10) -> def의 LeftIn(0)
		const bool isLeft = (entryPos.x != FLT_MAX && defPos.x != FLT_MAX) ? (defPos.x < entryPos.x) : false;
		const uintptr_t entryOutPin = isLeft ? 11 : 10;
		const uintptr_t defInPin = 1000 + static_cast<uintptr_t>(defIdx * 4) + (isLeft ? 1 : 0);

		NodeEditorUI::DrawLink(1, entryOutPin, defInPin, NodeEditorUI::Colors::WireEntry, 2.5f);
	}

	// 2. 전이 목록 링크 (LinkId: 100 + k)
	for (size_t k = 0; k < m_StateMachine->transitions.size(); ++k)
	{
		const auto& transition = m_StateMachine->transitions[k];
		const int targetIdx = findStateIndex(transition.targetState);
		if (targetIdx < 0) continue;

		uintptr_t startPin = 0;
		uintptr_t endPin = 0;

		const uintptr_t targetPinBase = 1000 + static_cast<uintptr_t>(targetIdx * 4);
		const ImVec2 targetPos = ed::GetNodePosition(100 + static_cast<uintptr_t>(targetIdx));

		if (transition.fromAnyState)
		{
			const ImVec2 anyPos = ed::GetNodePosition(2);
			// AnyState 노드보다 타겟이 왼쪽에 있으면 AnyState의 LeftOut(21) -> target의 RightIn(1)
			// 오른쪽에 있으면 AnyState의 RightOut(20) -> target의 LeftIn(0)
			const bool isLeft = (anyPos.x != FLT_MAX && targetPos.x != FLT_MAX) ? (targetPos.x < anyPos.x) : false;
			startPin = isLeft ? 21 : 20;
			endPin = targetPinBase + (isLeft ? 1 : 0);
		}
		else
		{
			const int srcIdx = findStateIndex(transition.sourceState);
			if (srcIdx < 0) continue;

			const uintptr_t srcPinBase = 1000 + static_cast<uintptr_t>(srcIdx * 4);
			const ImVec2 srcPos = ed::GetNodePosition(100 + static_cast<uintptr_t>(srcIdx));

			if (srcIdx == targetIdx)
			{
				// 자체 순환 전이 (Self Transition): 우측 하단 Out -> 우측 상단 In
				startPin = srcPinBase + 3; // RightOut
				endPin = targetPinBase + 1; // RightIn
			}
			else
			{
				// 반대 방향 전이가 존재하는지(양방향 상호 전이인지) 검사
				bool hasOpposite = false;
				for (size_t otherK = 0; otherK < m_StateMachine->transitions.size(); ++otherK)
				{
					if (otherK == k) continue;
					const auto& other = m_StateMachine->transitions[otherK];
					if (!other.fromAnyState && other.sourceState == transition.targetState && other.targetState == transition.sourceState)
					{
						hasOpposite = true;
						break;
					}
				}

				if (hasOpposite)
				{
					// 상호 전이일 때는 두 곡선이 교차하지 않도록 분리!
					// srcIdx < targetIdx인 방향은 우측 루프 (RightOut -> RightIn)
					// srcIdx > targetIdx인 방향은 좌측 루프 (LeftOut -> LeftIn)
					if (srcIdx < targetIdx)
					{
						startPin = srcPinBase + 3; // RightOut
						endPin = targetPinBase + 1; // RightIn
					}
					else
					{
						startPin = srcPinBase + 2; // LeftOut
						endPin = targetPinBase + 0; // LeftIn
					}
				}
				else
				{
					// 단방향 전이일 때는 기하학적 상대 위치에 따라 최단/자연스러운 핀 선택
					// 소스가 왼쪽에 있고 타겟이 오른쪽에 있는 경우: RightOut -> LeftIn
					// 소스가 오른쪽에 있고 타겟이 왼쪽에 있는 경우: LeftOut -> RightIn
					const bool isTargetLeft = (srcPos.x != FLT_MAX && targetPos.x != FLT_MAX) ? (srcPos.x > targetPos.x) : false;
					if (isTargetLeft)
					{
						startPin = srcPinBase + 2; // LeftOut
						endPin = targetPinBase + 1; // RightIn
					}
					else
					{
						startPin = srcPinBase + 3; // RightOut
						endPin = targetPinBase + 0; // LeftIn
					}
				}
			}
		}

		bool isTransitionActive = false;
		if (observedRuntime && observedRuntime->IsTransitioning())
		{
			if (transition.fromAnyState)
			{
				isTransitionActive = (transition.targetState == observedRuntime->GetTransitionTargetStateName());
			}
			else
			{
				isTransitionActive = (transition.sourceState == observedRuntime->GetCurrentStateName() &&
									  transition.targetState == observedRuntime->GetTransitionTargetStateName());
			}
		}

		const bool isSelected = (m_SelectedTransitionIndex == static_cast<int>(k));
		ImVec4 col = isSelected ? NodeEditorUI::Colors::WireSelected
			: (isTransitionActive ? ImVec4(0.0f, 1.0f, 0.8f, 1.0f)
			: (transition.fromAnyState ? NodeEditorUI::Colors::WireAnyState : NodeEditorUI::Colors::WireDefault));
		const float thickness = isSelected ? 3.5f : (isTransitionActive ? 3.0f : 2.0f);

		NodeEditorUI::DrawLink(100 + static_cast<uintptr_t>(k), startPin, endPin, col, thickness, isTransitionActive);
	}
}

void AnimatorGraphPanel::DrawCanvasContextMenu()
{
	auto& input = Engine::InputManager::Get();
	const bool canvasHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

	const ed::NodeId hoveredNode = ed::GetHoveredNode();
	const ed::LinkId hoveredLink = ed::GetHoveredLink();
	const ed::PinId hoveredPin = ed::GetHoveredPin();

	if (canvasHovered && input.IsMouseButtonDown(Engine::EMouseButton::Right))
	{
		const auto mouse = input.GetScreenMousePosition();
		m_ContextMenuPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));

		if (hoveredPin)
		{
			m_ContextTransitionPin = hoveredPin.Get();
			ImGui::OpenPopup("##AnimPinMenu");
		}
		else if (hoveredNode)
		{
			const int totalSelected = ed::GetSelectedObjectCount();
			bool alreadySelected = false;
			if (totalSelected > 0)
			{
				std::vector<ed::NodeId> selNodes(totalSelected);
				const int nodeCount = ed::GetSelectedNodes(selNodes.data(), totalSelected);
				selNodes.resize(nodeCount);
				alreadySelected = std::find(selNodes.begin(), selNodes.end(), hoveredNode) != selNodes.end();
			}
			if (!alreadySelected)
				ed::SelectNode(hoveredNode, false);

			const uintptr_t rawId = hoveredNode.Get();
			if (rawId >= 100)
			{
				m_SelectedStateIndex = static_cast<int>(rawId - 100);
				m_SelectedTransitionIndex = -1;
				ImGui::OpenPopup("##AnimNodeMenu");
			}
			else if (ed::GetSelectedObjectCount() >= 2)
			{
				m_SelectedStateIndex = -1;
				m_SelectedTransitionIndex = -1;
				ImGui::OpenPopup("##AnimNodeMenu");
			}
		}
		else if (hoveredLink)
		{
			const uintptr_t rawId = hoveredLink.Get();
			if (rawId >= 100)
			{
				m_SelectedTransitionIndex = static_cast<int>(rawId - 100);
				m_SelectedStateIndex = -1;
				ImGui::OpenPopup("##AnimLinkMenu");
			}
		}
		else
		{
			ImGui::OpenPopup("##AnimCanvasMenu");
		}
	}

	DrawTransitionPinMenu();

	// 1. 노드 우클릭 팝업
	if (ImGui::BeginPopup("##AnimNodeMenu"))
	{
		const bool validState = m_SelectedStateIndex >= 0 &&
			m_SelectedStateIndex < static_cast<int>(m_StateMachine->states.size());
		if (validState)
		{
			const auto& state = m_StateMachine->states[m_SelectedStateIndex];

			if (ImGui::MenuItem((ICON_FA_ARROW_RIGHT " " + LOCAL_LABEL("AnimatorGraph.MakeTransition")).c_str()))
			{
				m_ConnectingSourceIndex = m_SelectedStateIndex;
			}
			if (ImGui::MenuItem(LOCAL_LABEL("AnimatorGraph.MakeAnyStateTransition").c_str()))
			{
				Engine::AnimTransition transition{};
				transition.fromAnyState = true;
				transition.targetState = state.name;
				transition.duration = 0.25f;

				m_StateMachine->transitions.push_back(std::move(transition));
				m_SelectedTransitionIndex = static_cast<int>(m_StateMachine->transitions.size() - 1);
				m_SelectedStateIndex = -1;
				m_ConnectingSourceIndex = -1;
				m_History.MarkEdited();
			}
			if (ImGui::MenuItem(LOCAL_LABEL("AnimatorGraph.SetDefaultState").c_str()))
			{
				m_StateMachine->defaultState = state.name;
				m_History.MarkEdited();
			}
		}

		int selectedNodeCount = 0;
		const int totalSelected = ed::GetSelectedObjectCount();
		if (totalSelected > 0)
		{
			std::vector<ed::NodeId> selNodes(totalSelected);
			selectedNodeCount = ed::GetSelectedNodes(selNodes.data(), totalSelected);
		}
		if (selectedNodeCount >= 2)
		{
			if (validState) ImGui::Separator();
			if (ImGui::BeginMenu(LOCAL_LABEL("GraphPanel.Alignment").c_str()))
			{
				auto& sm = ShortcutManager::Get();
				if (ImGui::MenuItem(LOCAL_LABEL("Action.Graph.AlignTop").c_str(), sm.GetShortcutHint("Animator.AlignTop"))) AlignSelectedNodes('W');
				if (ImGui::MenuItem(LOCAL_LABEL("Action.Graph.AlignBottom").c_str(), sm.GetShortcutHint("Animator.AlignBottom"))) AlignSelectedNodes('S');
				if (ImGui::MenuItem(LOCAL_LABEL("Action.Graph.AlignLeft").c_str(), sm.GetShortcutHint("Animator.AlignLeft"))) AlignSelectedNodes('A');
				if (ImGui::MenuItem(LOCAL_LABEL("Action.Graph.AlignRight").c_str(), sm.GetShortcutHint("Animator.AlignRight"))) AlignSelectedNodes('D');
				const bool canDistribute = selectedNodeCount >= 3;
				if (ImGui::MenuItem(LOCAL_LABEL("Action.Graph.DistributeHoriz").c_str(), sm.GetShortcutHint("Animator.DistributeHoriz"), false, canDistribute))
					AlignSelectedNodes('H');
				if (ImGui::MenuItem(LOCAL_LABEL("Action.Graph.DistributeVert").c_str(), sm.GetShortcutHint("Animator.DistributeVert"), false, canDistribute))
					AlignSelectedNodes('V');
				ImGui::EndMenu();
			}
		}

		if (validState)
		{
			ImGui::Separator();
			ACTION_MENU_ITEM((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(), "Animator.DeleteSelection");
		}
		ImGui::EndPopup();
	}

	// 2. 링크(전이) 우클릭 팝업
	if (ImGui::BeginPopup("##AnimLinkMenu"))
	{
		if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < static_cast<int>(m_StateMachine->transitions.size()))
		{
			if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str()))
			{
				DeleteSelection();
			}
		}
		ImGui::EndPopup();
	}

	// 3. 빈 캔버스 우클릭 팝업
	if (ImGui::BeginPopup("##AnimCanvasMenu"))
	{
		if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("AnimatorGraph.CreateState")).c_str()))
		{
			AddState(m_ContextMenuPosition);
		}
		ACTION_MENU_ITEM((ICON_FA_MAXIMIZE " " + LOCAL_LABEL("AnimatorGraph.ResetView")).c_str(), "Animator.ResetView");
		ImGui::EndPopup();
	}
}

void AnimatorGraphPanel::DrawTransitionPinMenu()
{
	if (!ImGui::BeginPopup("##AnimPinMenu")) return;

	const auto id = m_ContextTransitionPin;
	const bool entry = id == 10 || id == 11;
	const bool anyState = id == 20 || id == 21;
	const auto index = id >= 1000 ? (id - 1000) / 4 : m_StateMachine->states.size();
	const bool isInput = id >= 1000 && (id - 1000) % 4 < 2;
	const auto& states = m_StateMachine->states;
	if (!entry && !anyState && index >= states.size())
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		return;
	}

	const std::string stateName = entry ? LOCAL("AnimatorGraph.Entry") :
		(anyState ? LOCAL("AnimatorGraph.AnyState") : states[index].name);
	const auto& direction = LOCAL(isInput ? "AnimatorGraph.IncomingPin" : "AnimatorGraph.OutgoingPin");
	ImGui::TextDisabled("%s", LOCAL_FMT("GraphPanel.PinSummary", stateName, direction).c_str());
	ImGui::Separator();

	auto& transitions = m_StateMachine->transitions;
	const auto matches = [&](const Engine::AnimTransition& transition)
	{
		if (anyState) return transition.fromAnyState;
		if (isInput) return transition.targetState == stateName;
		return !transition.fromAnyState && transition.sourceState == stateName;
	};
	bool edited = false;
	if (entry || anyState)
	{
		const char* menu = entry ? "AnimatorGraph.SetDefaultState" : "AnimatorGraph.SelectTransitionTarget";
		if (ImGui::BeginMenu(LOCAL_LABEL(menu).c_str(), !states.empty()))
		{
			for (size_t i = 0; i < states.size(); ++i)
			{
				const auto& target = states[i].name;
				const bool exists = entry ? m_StateMachine->defaultState == target :
					std::any_of(transitions.begin(), transitions.end(), [&](const Engine::AnimTransition& transition)
					{
						return transition.fromAnyState && transition.targetState == target;
					});
				ImGui::PushID(static_cast<int>(i));
				if (ImGui::MenuItem((target + "###TargetState").c_str(), nullptr, false, !exists))
				{
					if (entry)
						m_StateMachine->defaultState = target;
					else
					{
						Engine::AnimTransition transition{};
						transition.fromAnyState = true;
						transition.targetState = target;
						transition.duration = 0.25f;
						transitions.push_back(std::move(transition));
					}
					edited = true;
				}
				ImGui::PopID();
			}
			ImGui::EndMenu();
		}
	}
	else if (isInput)
	{
		if (ImGui::MenuItem(LOCAL_LABEL("AnimatorGraph.SetDefaultState").c_str(), nullptr, false,
			m_StateMachine->defaultState != stateName))
		{
			m_StateMachine->defaultState = stateName;
			edited = true;
		}
	}
	else if (ImGui::MenuItem(LOCAL_LABEL("AnimatorGraph.MakeTransition").c_str(), nullptr, false, states.size() > 1))
	{
		m_ConnectingSourceIndex = static_cast<int>(index);
	}

	if (!entry && ImGui::MenuItem(LOCAL_LABEL("AnimatorGraph.DisconnectPinTransitions").c_str(), nullptr, false,
		std::any_of(transitions.begin(), transitions.end(), matches)))
	{
		std::erase_if(transitions, matches);
		edited = true;
	}

	if (edited)
	{
		ed::ClearSelection();
		m_SelectedTransitionIndex = -1;
		m_ConnectingSourceIndex = -1;
		m_History.MarkEdited();
		SyncPreviewStateMachine();
	}
	ImGui::EndPopup();
}

void AnimatorGraphPanel::AddState(const ImVec2& position)
{
	if (!m_StateMachine) return;

	size_t index = m_StateMachine->states.size();
	std::string name;
	do
	{
		name = "State_" + std::to_string(index++);
	} while (m_StateMachine->FindState(name));

	const size_t newIdx = m_StateMachine->states.size();
	m_StateMachine->states.push_back(Engine::AnimState{ name });
	m_NodePositions.insert_or_assign(name, position);

	if (m_EditorContext)
	{
		auto* previous = ed::GetCurrentEditor();
		ed::SetCurrentEditor(m_EditorContext);
		ed::SetNodePosition(100 + static_cast<uintptr_t>(newIdx), position);
		ed::SetCurrentEditor(previous);
	}

	m_History.MarkEdited();
}

void AnimatorGraphPanel::DrawSelectionInspector()
{
	if (!m_StateMachine)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.NoNodeSelected"));
		return;
	}

	if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < static_cast<int>(m_StateMachine->transitions.size()))
	{
		DrawTransitionInspector(m_StateMachine->transitions[m_SelectedTransitionIndex]);
	}
	else if (m_SelectedStateIndex >= 0 && m_SelectedStateIndex < static_cast<int>(m_StateMachine->states.size()))
	{
		DrawStateInspector(m_StateMachine->states[m_SelectedStateIndex]);
	}
	else
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.NoNodeSelected"));
	}
}

void AnimatorGraphPanel::DrawStateInspector(Engine::AnimState& state)
{
	// ── 0. 노드 헤더 타이틀 및 인라인 이름 변경 ──
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.NodeInspector"));
	ImGui::SameLine();
	if (state.name == m_StateMachine->defaultState)
	{
		ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "[%s]", LOCAL_CSTR("AnimatorGraph.Entry"));
		ImGui::SameLine();
	}
	ImGui::Text("%s", state.name.c_str());
	ImGui::SameLine();
	if (EditorUI::SmallIconButton("RenameStateBtn", ICON_FA_PEN_TO_SQUARE, "EditorCommon.Rename"))
	{
		m_StateRenameDraft = state.name;
		ImGui::OpenPopup("RenameState");
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Rename"));

	if (ImGui::BeginPopup("RenameState"))
	{
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		ImGui::InputText("##StateName", &m_StateRenameDraft);

		const bool changed = m_StateRenameDraft != state.name;
		const bool blank = m_StateRenameDraft.find_first_not_of(" \t\r\n") == std::string::npos;
		const bool duplicate = changed && m_StateMachine->FindState(m_StateRenameDraft) != nullptr;

		if (blank || duplicate)
			ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.StateNameInvalid"));

		const EditorUI::DialogAction actions[] = {
			{ "EditorCommon.Cancel" }, { "EditorCommon.Rename", changed && !blank && !duplicate }
		};
		const int action = EditorUI::DialogButtonBar("RenameActions", actions, IM_ARRAYSIZE(actions));
		if (action == 1)
		{
			const std::string oldName = state.name;
			const bool ownsReferences = m_StateMachine->FindState(oldName) == &state;
			state.name = m_StateRenameDraft;

			if (ownsReferences)
			{
				if (m_StateMachine->defaultState == oldName)
					m_StateMachine->defaultState = state.name;

				if (const auto position = m_NodePositions.find(oldName); position != m_NodePositions.end())
				{
					const ImVec2 previousPosition = position->second;
					m_NodePositions.erase(position);
					m_NodePositions.insert_or_assign(state.name, previousPosition);
				}

				for (auto& transition : m_StateMachine->transitions)
				{
					if (!transition.fromAnyState && transition.sourceState == oldName)
						transition.sourceState = state.name;
					if (transition.targetState == oldName)
						transition.targetState = state.name;
				}
			}

			m_History.MarkEdited();
			SyncPreviewStateMachine();
			ImGui::CloseCurrentPopup();
		}
		if (action == 0)
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	ImGui::Separator();

	// ── 1. 상태 기본 속성 (State Properties) ──
	if (ImGui::CollapsingHeader((ICON_FA_SLIDERS " " + LOCAL("AnimatorGraph.StateProperties")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (EditorUI::BeginPropertyTable("StatePropertiesTable", EditorUI::InspectorPropertyTableStyle()))
		{
			EditorUI::PropertyRow("PROP_PLAY_RATE");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##StatePlayRate", &state.playRate, 0.05f, 0.0f, 10.0f, "%.2f")) m_History.MarkEdited();

			EditorUI::PropertyRow("PROP_IS_LOOPING");
			if (ImGui::Checkbox("##StateIsLooping", &state.isLooping)) m_History.MarkEdited();

			if (state.isLooping)
			{
				EditorUI::PropertyRow("PROP_LOOP_BLEND_DURATION");
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::SliderFloat("##StateLoopDuration", &state.loopBlendDuration, 0.0f, 0.5f, "%.2fs")) m_History.MarkEdited();
			}

			EditorUI::PropertyRow("AnimatorGraph.SetDefaultState");
			const bool isDefault = (state.name == m_StateMachine->defaultState);
			bool defaultToggle = isDefault;
			if (ImGui::Checkbox("##SetDefaultStateToggle", &defaultToggle) && defaultToggle)
			{
				m_StateMachine->defaultState = state.name;
				m_History.MarkEdited();
			}

			EditorUI::EndPropertyTable();
		}
	}

	ImGui::Spacing();

	// ── 2. 모션 설정 (Motion Configuration) ──
	if (ImGui::CollapsingHeader((ICON_FA_PERSON_RUNNING " " + LOCAL("AnimatorGraph.MotionConfiguration")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		const std::string motionTypes[] = {
			LOCAL("AnimatorGraph.Clip"),
			LOCAL("AnimatorGraph.BlendTree1D"),
			LOCAL("AnimatorGraph.BlendTree2D")
		};
		const int currentType = state.IsBlendTree2D() ? 2 : (state.IsBlendTree() ? 1 : 0);

		const auto drawFloatParameterCombo = [&](const char* comboId, std::string& name) -> bool
		{
			const auto& parameters = m_StateMachine->defaultParameters;
			const std::string preview = name.empty() ? LOCAL("EditorCommon.None") : name;

			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo(comboId, preview.c_str()))
			{
				if (ImGui::Selectable(LOCAL_LABEL("EditorCommon.None").c_str(), name.empty()) && !name.empty())
				{
					name.clear();
					m_History.MarkEdited();
				}

				for (const auto& [parameterName, parameter] : parameters)
				{
					if (parameter.type != Engine::EAnimatorParamType::Float) continue;

					ImGui::PushID(parameterName.c_str());
					const std::string label = parameterName + "###FloatParameter";
					const bool selected = (name == parameterName);
					if (ImGui::Selectable(label.c_str(), selected) && !selected)
					{
						name = parameterName;
						m_History.MarkEdited();
					}
					if (selected) ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			const auto it = parameters.find(name);
			return !name.empty() && it != parameters.end() && it->second.type == Engine::EAnimatorParamType::Float;
		};

		if (EditorUI::BeginPropertyTable("MotionConfigTable", EditorUI::InspectorPropertyTableStyle()))
		{
			EditorUI::PropertyRow("AnimatorGraph.MotionType");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo("##MotionTypeCombo", motionTypes[currentType].c_str()))
			{
				for (int i = 0; i < 3; ++i)
				{
					ImGui::PushID(i);
					const std::string label = motionTypes[i] + "###MotionTypeOption";
					const bool selected = currentType == i;
					if (ImGui::Selectable(label.c_str(), selected) && !selected)
					{
						state.isBlendTree = i != 0;
						state.useBlendTree2D = i == 2;
						m_History.MarkEdited();
					}
					if (selected) ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			if (!state.IsBlendTree())
			{
				EditorUI::PropertyRow("PROP_ANIMATION_CLIP");
				ImGui::PushID("StateMotionClip");
				if (Editor::DrawInspectorResourcePicker(state.clip, true)) m_History.MarkEdited();
				ImGui::PopID();
			}
			else if (!state.IsBlendTree2D())
			{
				EditorUI::PropertyRow("AnimatorGraph.Parameter");
				drawFloatParameterCombo("##Blend1DParameter", state.blendTree.parameterName);
			}

			EditorUI::EndPropertyTable();
		}

		if (state.IsBlendTree2D())
		{
			auto& tree = state.blendTree2D;
			const bool isFreeform = !tree.samples.empty();
			if (!isFreeform)
				DrawBlend2DPreview(state);
			else
				DrawBlend2DFreeformEditor(state);

			ImGui::Spacing();
			if (EditorUI::BeginPropertyTable("Blend2DParamsTable", EditorUI::InspectorPropertyTableStyle()))
			{
				EditorUI::PropertyRow("AnimatorGraph.Blend2DParameterX");
				const bool hasX = drawFloatParameterCombo("##Blend2DParamX", tree.parameterX);

				EditorUI::PropertyRow("AnimatorGraph.Blend2DParameterY");
				const bool hasY = drawFloatParameterCombo("##Blend2DParamY", tree.parameterY);

				if (!hasX || !hasY)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TableSetColumnIndex(1);
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.Blend2DAxesRequired"));
				}

				if (!isFreeform)
				{
					const auto drawRangeRow = [&](const char* labelKey, float& minimum, float& maximum)
					{
						EditorUI::PropertyRow(labelKey);
						float range[2] = { minimum, maximum };
						ImGui::SetNextItemWidth(-1.0f);
						if (ImGui::DragFloat2(("##" + std::string(labelKey)).c_str(), range, 0.05f))
						{
							minimum = range[0];
							maximum = range[1];
							m_History.MarkEdited();
						}
					};

					drawRangeRow("AnimatorGraph.Blend2DRangeX", tree.minX, tree.maxX);
					drawRangeRow("AnimatorGraph.Blend2DRangeY", tree.minY, tree.maxY);

					if (!std::isfinite(tree.minX) || !std::isfinite(tree.maxX) || tree.minX >= tree.maxX ||
						!std::isfinite(tree.minY) || !std::isfinite(tree.maxY) || tree.minY >= tree.maxY)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::TableSetColumnIndex(1);
						ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.Blend2DRangeInvalid"));
					}
				}

				EditorUI::EndPropertyTable();
			}

			if (!isFreeform)
			{
				// 2x2 Corner Clips Grid Layout
				ImGui::Spacing();
				ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.Blend2DWeights"));
				if (ImGui::BeginTable("##Blend2DCornersGrid", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp))
				{
					const auto drawCornerCell = [&](int id, float x, float y, Engine::ResourceHandle<Engine::Animation>& clip)
					{
						ImGui::PushID(id);
						const auto label = LOCAL_FMT("AnimatorGraph.Blend2DCorner", x, y);
						ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "%s", label.c_str());
						if (Editor::DrawInspectorResourcePicker(clip, true)) m_History.MarkEdited();
						ImGui::PopID();
					};

					// Top Row: (-X, +Y) and (+X, +Y)
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					drawCornerCell(2, tree.minX, tree.maxY, tree.clip01);
					ImGui::TableSetColumnIndex(1);
					drawCornerCell(3, tree.maxX, tree.maxY, tree.clip11);

					// Bottom Row: (-X, -Y) and (+X, -Y)
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					drawCornerCell(0, tree.minX, tree.minY, tree.clip00);
					ImGui::TableSetColumnIndex(1);
					drawCornerCell(1, tree.maxX, tree.minY, tree.clip10);

					ImGui::EndTable();
				}

				if (!tree.clip00.IsValid() || !tree.clip10.IsValid() || !tree.clip01.IsValid() || !tree.clip11.IsValid())
				{
					ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.Blend2DCornersIncomplete"));
				}

				ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.Blend2DFreeformEmptyHint"));
			}

			DrawBlend2DSamplesSection(state);
		}
		else
		{
			ImGui::Spacing();
			ImGui::Text("%s (%d)", LOCAL_CSTR("AnimatorGraph.Samples"),
				static_cast<int>(state.blendTree.samples.size()));
			EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_PLUS, true).x);

			if (EditorUI::SmallIconButton("AddBlendSample", ICON_FA_PLUS, "EditorCommon.Add"))
			{
				const float nextThreshold = state.blendTree.samples.empty()
					? 0.0f : state.blendTree.samples.back().threshold + 1.0f;
				state.blendTree.AddSample(nextThreshold, {});
				m_History.MarkEdited();
			}

			int sampleToRemove = -1;
			bool needSort = false;
			if (ImGui::BeginTable("##BlendSamplesTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.Threshold"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
				ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.Clip"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Delete"), ImGuiTableColumnFlags_WidthFixed, std::max(EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x, ImGui::CalcTextSize(LOCAL_CSTR("EditorCommon.Delete")).x));
				ImGui::TableHeadersRow();

				for (size_t i = 0; i < state.blendTree.samples.size(); ++i)
				{
					auto& sample = state.blendTree.samples[i];
					ImGui::PushID(static_cast<int>(i));

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					if (ImGui::DragFloat("##SampleThreshold", &sample.threshold, 0.05f))
					{
						needSort = true;
						m_History.MarkEdited();
					}

					ImGui::TableSetColumnIndex(1);
					if (Editor::DrawInspectorResourcePicker(sample.clip, true)) m_History.MarkEdited();

					ImGui::TableSetColumnIndex(2);
					if (EditorUI::SmallIconButton("DelSample", ICON_FA_TRASH, "EditorCommon.Delete")) sampleToRemove = static_cast<int>(i);

					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			if (sampleToRemove >= 0 && sampleToRemove < static_cast<int>(state.blendTree.samples.size()))
			{
				state.blendTree.samples.erase(state.blendTree.samples.begin() + sampleToRemove);
				m_History.MarkEdited();
			}
			if (needSort) state.blendTree.SortSamples();
		}
	}

	ImGui::Spacing();

	// ── 3. 애니메이션 노티파이 (Notifies) ──
	if (ImGui::CollapsingHeader((ICON_FA_BELL " " + LOCAL("AnimatorGraph.Notifies")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("%s: %d", LOCAL_CSTR("AnimatorGraph.Notifies"), static_cast<int>(state.notifies.size()));
		EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_PLUS, true).x);
		if (EditorUI::SmallIconButton("AddNotify", ICON_FA_PLUS, "EditorCommon.Add"))
		{
			state.notifies.push_back(Engine::AnimNotify{ "NewNotify", 0.0f, "" });
			m_History.MarkEdited();
		}

		int notifyToRemove = -1;
		if (!state.notifies.empty() && ImGui::BeginTable("##NotifiesTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.NotifyTime"), ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.NotifyName"), ImGuiTableColumnFlags_WidthFixed, 100.0f);
			ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.NotifyPayload"), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Delete"), ImGuiTableColumnFlags_WidthFixed, std::max(EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x, ImGui::CalcTextSize(LOCAL_CSTR("EditorCommon.Delete")).x));
			ImGui::TableHeadersRow();

			for (size_t i = 0; i < state.notifies.size(); ++i)
			{
				auto& notify = state.notifies[i];
				ImGui::PushID(static_cast<int>(i));

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (ImGui::DragFloat("##NotifyTime", &notify.triggerTime, 0.02f, 0.0f, 60.0f, "%.2fs")) m_History.MarkEdited();

				ImGui::TableSetColumnIndex(1);
				char nameBuf[64] = {};
				std::strncpy(nameBuf, notify.name.c_str(), sizeof(nameBuf) - 1);
				if (ImGui::InputText("##NotifyName", nameBuf, sizeof(nameBuf)))
				{
					notify.name = nameBuf;
					m_History.MarkEdited();
				}

				ImGui::TableSetColumnIndex(2);
				char payloadBuf[128] = {};
				std::strncpy(payloadBuf, notify.payload.c_str(), sizeof(payloadBuf) - 1);
				if (ImGui::InputText("##NotifyPayload", payloadBuf, sizeof(payloadBuf)))
				{
					notify.payload = payloadBuf;
					m_History.MarkEdited();
				}

				ImGui::TableSetColumnIndex(3);
				if (EditorUI::SmallIconButton("DelNotify", ICON_FA_TRASH, "EditorCommon.Delete")) notifyToRemove = static_cast<int>(i);

				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		if (notifyToRemove >= 0 && notifyToRemove < static_cast<int>(state.notifies.size()))
		{
			state.notifies.erase(state.notifies.begin() + notifyToRemove);
			m_History.MarkEdited();
		}
	}

	ImGui::Spacing();

	// ── 4. 트랜지션 목록 (Transitions) ──
	if (ImGui::CollapsingHeader((ICON_FA_ARROWS_SPLIT_UP_AND_LEFT " " + LOCAL("AnimatorGraph.Transitions")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		int transitionCount = 0;
		for (const auto& tr : m_StateMachine->transitions)
		{
			if (tr.sourceState == state.name || tr.targetState == state.name)
				transitionCount++;
		}

		ImGui::TextDisabled("%s: %d", LOCAL_CSTR("AnimatorGraph.Transitions"), transitionCount);
		EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_PLUS, true).x);
		if (EditorUI::SmallIconButton("AddTransitionBtn", ICON_FA_PLUS, "AnimatorGraph.MakeTransition"))
		{
			m_ConnectingSourceIndex = static_cast<int>(m_SelectedStateIndex);
		}
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_CSTR("AnimatorGraph.SelectTargetToConnect"));

		int transitionToRemove = -1;
		if (transitionCount > 0)
		{
			if (ImGui::BeginTable("##StateTransitionsTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
			{
				ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.TransitionDirection"), ImGuiTableColumnFlags_WidthFixed, 65.0f);
				ImGui::TableSetupColumn(LOCAL_CSTR("PROP_NAME"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.Duration"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
				ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Delete"), ImGuiTableColumnFlags_WidthFixed, std::max(EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x, ImGui::CalcTextSize(LOCAL_CSTR("EditorCommon.Delete")).x));
				ImGui::TableHeadersRow();

				for (size_t i = 0; i < m_StateMachine->transitions.size(); ++i)
				{
					const auto& tr = m_StateMachine->transitions[i];
					const bool isOutgoing = (!tr.fromAnyState && tr.sourceState == state.name);
					const bool isIncoming = (tr.targetState == state.name);

					if (!isOutgoing && !isIncoming) continue;

					ImGui::PushID(static_cast<int>(i));
					ImGui::TableNextRow();

					const bool isSelected = (m_SelectedTransitionIndex == static_cast<int>(i));

					// Column 0: 방향 (출력 / 입력 / Any)
					ImGui::TableSetColumnIndex(0);
					if (isOutgoing)
						ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), ICON_FA_ARROW_RIGHT " %s", LOCAL_CSTR("AnimatorGraph.Outgoing"));
					else if (tr.fromAnyState)
						ImGui::TextColored(ImVec4(0.75f, 0.5f, 0.95f, 1.0f), ICON_FA_BOLT " %s", LOCAL_CSTR("AnimatorGraph.AnyShort"));
					else
						ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), ICON_FA_ARROW_LEFT " %s", LOCAL_CSTR("AnimatorGraph.Incoming"));

					// Column 1: 상대 상태 명칭 (클릭 시 전이 선택)
					ImGui::TableSetColumnIndex(1);
					const std::string peerStateName = isOutgoing ? tr.targetState : (tr.fromAnyState ? LOCAL("AnimatorGraph.AnyState") : tr.sourceState);
					if (ImGui::Selectable(peerStateName.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
					{
						m_SelectedTransitionIndex = static_cast<int>(i);
						m_SelectedStateIndex = -1;
					}

					// Column 2: 전이 시간
					ImGui::TableSetColumnIndex(2);
					ImGui::TextDisabled("%.2fs", tr.duration);

					// Column 3: 삭제 버튼
					ImGui::TableSetColumnIndex(3);
					if (EditorUI::SmallIconButton("DelTrBtn", ICON_FA_TRASH, "EditorCommon.Delete"))
					{
						transitionToRemove = static_cast<int>(i);
					}

					ImGui::PopID();
				}

				ImGui::EndTable();
			}
		}
		else
		{
			ImGui::Spacing();
			ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.NoTransitions"));
		}

		if (transitionToRemove >= 0 && transitionToRemove < static_cast<int>(m_StateMachine->transitions.size()))
		{
			m_StateMachine->transitions.erase(m_StateMachine->transitions.begin() + transitionToRemove);
			if (m_SelectedTransitionIndex == transitionToRemove)
				m_SelectedTransitionIndex = -1;
			else if (m_SelectedTransitionIndex > transitionToRemove)
				m_SelectedTransitionIndex--;
			m_History.MarkEdited();
		}
	}
}

void AnimatorGraphPanel::DrawBlend2DPreview(const Engine::AnimState& state)
{
	if (!m_ShowPreviewViewport || !m_PreviewWidget.GetScene() || !m_PreviewModelEntity) return;

	auto& world = m_PreviewWidget.GetRegistry();
	const auto entity = m_PreviewModelEntity->GetEntityHandle();
	auto* animator = world.try_get<Engine::AnimatorComponent>(entity);
	if (!animator) return;

	auto& runtime = animator->runtimeState;
	if (runtime.GetStateMachine() != m_StateMachine || runtime.GetCurrentStateName() != state.name) return;

	const auto input = runtime.GetCurrentBlendInput();
	if (!input) return;

	const auto& tree = state.blendTree2D;
	std::array<Engine::BlendClipWeight2D, 4> weights{};
	if (!tree.Evaluate(input->x, input->y, weights)) return;

	const auto& parameters = runtime.GetParameters().GetAll();
	const auto x = parameters.find(tree.parameterX);
	const auto y = parameters.find(tree.parameterY);
	if (x == parameters.end() || y == parameters.end() ||
		x->second.type != Engine::EAnimatorParamType::Float ||
		y->second.type != Engine::EAnimatorParamType::Float ||
		!std::isfinite(x->second.floatValue) || !std::isfinite(y->second.floatValue))
	{
		return;
	}

	Engine::vec2 target(
		std::clamp(x->second.floatValue, tree.minX, tree.maxX),
		std::clamp(y->second.floatValue, tree.minY, tree.maxY));

	const bool sharedAxis = tree.parameterX == tree.parameterY;
	const bool canDrag = !sharedAxis && tree.clip00.Get() && tree.clip10.Get() &&
		tree.clip01.Get() && tree.clip11.Get();

	ImGui::TextUnformatted(LOCAL_CSTR("AnimatorGraph.Blend2DPreviewTitle"));

	const float side = (std::min)(ImGui::GetContentRegionAvail().x, 160.0f);
	if (side > 14.0f)
	{
		ImGui::PushID(state.name.c_str());
		if (canDrag)
			ImGui::InvisibleButton("##Blend2DInput", ImVec2(side, side), ImGuiButtonFlags_MouseButtonLeft);
		else
			ImGui::Dummy(ImVec2(side, side));
		const bool isDragging = canDrag && ImGui::IsItemActive();
		ImGui::PopID();

		if (ImGui::IsItemVisible())
		{
			const auto rectMin = ImGui::GetItemRectMin();
			const auto rectMax = ImGui::GetItemRectMax();
			const ImVec2 plotMin(rectMin.x + 7.0f, rectMin.y + 7.0f);
			const ImVec2 plotMax(rectMax.x - 7.0f, rectMax.y - 7.0f);
			const float plotWidth = plotMax.x - plotMin.x;
			const float plotHeight = plotMax.y - plotMin.y;

			auto& mouseInput = Engine::InputManager::Get();
			if (isDragging && mouseInput.IsMouseButtonPressed(Engine::EMouseButton::Left))
			{
				const auto mouse = mouseInput.GetScreenMousePosition();
				const float u = std::clamp((mouse.x - plotMin.x) / plotWidth, 0.0f, 1.0f);
				const float v = std::clamp((plotMax.y - mouse.y) / plotHeight, 0.0f, 1.0f);
				target.x = std::lerp(tree.minX, tree.maxX, u);
				target.y = std::lerp(tree.minY, tree.maxY, v);
				runtime.SetFloat(tree.parameterX, target.x);
				runtime.SetFloat(tree.parameterY, target.y);
			}

			const auto toPoint = [&](const Engine::vec2& value)
				{
					const double u = (static_cast<double>(value.x) - tree.minX) /
						(static_cast<double>(tree.maxX) - tree.minX);
					const double v = (static_cast<double>(value.y) - tree.minY) /
						(static_cast<double>(tree.maxY) - tree.minY);

					return ImVec2(
						plotMin.x + static_cast<float>(std::clamp(u, 0.0, 1.0)) * plotWidth,
						plotMax.y - static_cast<float>(std::clamp(v, 0.0, 1.0)) * plotHeight);
				};

			auto* drawList = ImGui::GetWindowDrawList();
			const ImU32 grid = IM_COL32(85, 95, 110, 255);
			const float middleX = (plotMin.x + plotMax.x) * 0.5f;
			const float middleY = (plotMin.y + plotMax.y) * 0.5f;

			drawList->AddRectFilled(rectMin, rectMax, IM_COL32(28, 32, 40, 255), 4.0f);
			drawList->AddRect(plotMin, plotMax, grid);
			drawList->AddLine(ImVec2(middleX, plotMin.y), ImVec2(middleX, plotMax.y), grid);
			drawList->AddLine(ImVec2(plotMin.x, middleY), ImVec2(plotMax.x, middleY), grid);
			drawList->AddCircle(toPoint(target), 5.0f, IM_COL32(255, 210, 80, 255), 0, 2.0f);
			drawList->AddCircleFilled(toPoint(*input), 3.5f, IM_COL32(0, 235, 200, 255));
		}

		if (canDrag)
			ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.Blend2DDragHint"));
		else if (sharedAxis)
			ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.Blend2DDragSharedAxis"));
	}

	ImGui::TextWrapped("%s", LOCAL_FMT("AnimatorGraph.Blend2DTarget", target.x, target.y).c_str());
	ImGui::TextWrapped("%s", LOCAL_FMT("AnimatorGraph.Blend2DSampled", input->x, input->y).c_str());
	ImGui::TextWrapped("%s", LOCAL_FMT("AnimatorGraph.Blend2DWeights",
		weights[0].Weight * 100.0f, weights[1].Weight * 100.0f,
		weights[2].Weight * 100.0f, weights[3].Weight * 100.0f).c_str());
}

void AnimatorGraphPanel::DrawBlend2DFreeformEditor(Engine::AnimState& state)
{
	auto& tree = state.blendTree2D;
	const int sampleCount = static_cast<int>(tree.samples.size());
	if (m_Blend2DSelectedSampleIndex >= sampleCount) m_Blend2DSelectedSampleIndex = -1;
	if (m_Blend2DDraggingSampleIndex >= sampleCount)
	{
		m_Blend2DDraggingSampleIndex = -1;
		m_Blend2DDragModified = false;
	}

	ImGui::TextUnformatted(LOCAL_CSTR("AnimatorGraph.Blend2DFreeformTitle"));

	// 드래그 중에는 좌표계가 흔들리지 않도록 잠근 범위 유지
	Blend2DBounds bounds;
	if (m_Blend2DDraggingSampleIndex >= 0)
	{
		bounds = m_Blend2DLockedBounds;
	}
	else
	{
		float sMinX = -1.0f, sMaxX = 1.0f;
		float sMinY = -1.0f, sMaxY = 1.0f;
		for (const auto& sample : tree.samples)
		{
			if (std::isfinite(sample.x))
			{
				sMinX = (std::min)(sMinX, sample.x);
				sMaxX = (std::max)(sMaxX, sample.x);
			}
			if (std::isfinite(sample.y))
			{
				sMinY = (std::min)(sMinY, sample.y);
				sMaxY = (std::max)(sMaxY, sample.y);
			}
		}
		if (std::isfinite(m_Blend2DTestPos.x))
		{
			sMinX = (std::min)(sMinX, m_Blend2DTestPos.x);
			sMaxX = (std::max)(sMaxX, m_Blend2DTestPos.x);
		}
		if (std::isfinite(m_Blend2DTestPos.y))
		{
			sMinY = (std::min)(sMinY, m_Blend2DTestPos.y);
			sMaxY = (std::max)(sMaxY, m_Blend2DTestPos.y);
		}

		const float padX = (std::max)(0.2f, (sMaxX - sMinX) * 0.1f);
		const float padY = (std::max)(0.2f, (sMaxY - sMinY) * 0.1f);
		bounds.minX = (sMinX < -1.0f) ? (sMinX - padX) : -1.0f;
		bounds.maxX = (sMaxX > 1.0f) ? (sMaxX + padX) : 1.0f;
		bounds.minY = (sMinY < -1.0f) ? (sMinY - padY) : -1.0f;
		bounds.maxY = (sMaxY > 1.0f) ? (sMaxY + padY) : 1.0f;
	}

	const float spanX = (std::max)(0.001f, bounds.maxX - bounds.minX);
	const float spanY = (std::max)(0.001f, bounds.maxY - bounds.minY);
	const float side = (std::min)(ImGui::GetContentRegionAvail().x, 240.0f);

	if (side > 32.0f)
	{
		ImGui::PushID(state.name.c_str());
		ImGui::InvisibleButton("##Blend2DFreeformCanvas", ImVec2(side, side),
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 rectMin = ImGui::GetItemRectMin();
		const ImVec2 rectMax = ImGui::GetItemRectMax();
		ImGui::PopID();

		const float pad = 12.0f;
		const ImVec2 plotMin(rectMin.x + pad, rectMin.y + pad);
		const ImVec2 plotMax(rectMax.x - pad, rectMax.y - pad);
		const float plotW = (std::max)(1.0f, plotMax.x - plotMin.x);
		const float plotH = (std::max)(1.0f, plotMax.y - plotMin.y);

		const auto worldToScreen = [&](float wx, float wy) -> ImVec2
		{
			const float u = (wx - bounds.minX) / spanX;
			const float v = (wy - bounds.minY) / spanY;
			return ImVec2(plotMin.x + u * plotW, plotMax.y - v * plotH);
		};

		const auto screenToWorld = [&](const ImVec2& sp) -> Engine::vec2
		{
			const float u = std::clamp((sp.x - plotMin.x) / plotW, 0.0f, 1.0f);
			const float v = std::clamp((plotMax.y - sp.y) / plotH, 0.0f, 1.0f);
			return Engine::vec2(bounds.minX + u * spanX, bounds.minY + v * spanY);
		};

		const ImGuiIO& io = ImGui::GetIO();
		const ImVec2 mousePos = io.MousePos;

		// 테스트 커서 이동: 우클릭 또는 Shift+좌클릭 드래그
		if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
			(io.KeyShift && ImGui::IsMouseClicked(ImGuiMouseButton_Left))))
		{
			m_Blend2DDraggingTestCursor = true;
		}
		if (m_Blend2DDraggingTestCursor)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
				(io.KeyShift && ImGui::IsMouseDown(ImGuiMouseButton_Left)) ||
				ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				m_Blend2DTestPos = screenToWorld(mousePos);
			}
			else
			{
				m_Blend2DDraggingTestCursor = false;
			}
		}

		// 샘플 포인트 선택 및 드래그 (일반 좌클릭)
		if (hovered && !io.KeyShift && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			int hitIndex = -1;
			constexpr float hitRadiusSq = 10.0f * 10.0f;
			for (int i = sampleCount - 1; i >= 0; --i)
			{
				const auto& sample = tree.samples[i];
				if (!std::isfinite(sample.x) || !std::isfinite(sample.y)) continue;
				const ImVec2 sp = worldToScreen(sample.x, sample.y);
				const float dx = mousePos.x - sp.x;
				const float dy = mousePos.y - sp.y;
				if (dx * dx + dy * dy <= hitRadiusSq)
				{
					hitIndex = i;
					break;
				}
			}

			m_Blend2DSelectedSampleIndex = hitIndex;
			if (hitIndex >= 0)
			{
				m_Blend2DDraggingSampleIndex = hitIndex;
				m_Blend2DDragModified = false;
				m_Blend2DLockedBounds = bounds;
			}
		}

		if (m_Blend2DDraggingSampleIndex >= 0 && m_Blend2DDraggingSampleIndex < sampleCount)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				const Engine::vec2 newPos = screenToWorld(mousePos);
				auto& dragged = tree.samples[m_Blend2DDraggingSampleIndex];
				if (std::abs(dragged.x - newPos.x) > 1e-4f || std::abs(dragged.y - newPos.y) > 1e-4f)
				{
					dragged.x = std::round(newPos.x * 100.0f) * 0.01f;
					dragged.y = std::round(newPos.y * 100.0f) * 0.01f;
					m_Blend2DDragModified = true;
				}
			}
			else
			{
				if (m_Blend2DDragModified) m_History.MarkEdited();
				m_Blend2DDraggingSampleIndex = -1;
				m_Blend2DDragModified = false;
			}
		}

		// 실시간 가중치 평가
		std::vector<Engine::BlendClipWeight2D> weights;
		tree.Evaluate(m_Blend2DTestPos.x, m_Blend2DTestPos.y, weights);

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(rectMin, rectMax, true);

		// 배경 및 테두리
		drawList->AddRectFilled(rectMin, rectMax, IM_COL32(26, 30, 38, 255), 4.0f);
		drawList->AddRect(plotMin, plotMax, IM_COL32(75, 85, 100, 255));

		// 그리드 라인 (범위에 따라 0.5 또는 1.0 간격 선택)
		const auto calcStep = [](float span) -> float
		{
			if (span <= 2.5f) return 0.5f;
			if (span <= 6.0f) return 1.0f;
			if (span <= 15.0f) return 2.0f;
			return 5.0f;
		};
		const float stepX = calcStep(spanX);
		const float stepY = calcStep(spanY);
		const ImU32 gridColor = IM_COL32(50, 56, 68, 255);
		const ImU32 axisColor = IM_COL32(105, 115, 135, 255);

		for (float gx = std::ceil(bounds.minX / stepX) * stepX; gx <= bounds.maxX + 1e-4f; gx += stepX)
		{
			const float sx = worldToScreen(gx, bounds.minY).x;
			const bool isOrigin = std::abs(gx) < 1e-3f;
			drawList->AddLine(ImVec2(sx, plotMin.y), ImVec2(sx, plotMax.y), isOrigin ? axisColor : gridColor, isOrigin ? 1.5f : 1.0f);
		}
		for (float gy = std::ceil(bounds.minY / stepY) * stepY; gy <= bounds.maxY + 1e-4f; gy += stepY)
		{
			const float sy = worldToScreen(bounds.minX, gy).y;
			const bool isOrigin = std::abs(gy) < 1e-3f;
			drawList->AddLine(ImVec2(plotMin.x, sy), ImVec2(plotMax.x, sy), isOrigin ? axisColor : gridColor, isOrigin ? 1.5f : 1.0f);
		}

		// 각 샘플 포인트 및 가중치 원 시각화
		int hoveredSampleIndex = -1;
		for (int i = 0; i < sampleCount; ++i)
		{
			const auto& sample = tree.samples[i];
			if (!std::isfinite(sample.x) || !std::isfinite(sample.y)) continue;

			const ImVec2 sp = worldToScreen(sample.x, sample.y);
			const auto* clipPtr = sample.clip.Get();

			float sampleWeight = 0.0f;
			if (clipPtr)
			{
				for (const auto& w : weights)
				{
					if (w.Clip == clipPtr) sampleWeight += w.Weight;
				}
			}
			sampleWeight = std::clamp(sampleWeight, 0.0f, 1.0f);

			const bool isSelected = (i == m_Blend2DSelectedSampleIndex);
			const float dx = mousePos.x - sp.x;
			const float dy = mousePos.y - sp.y;
			if (hovered && dx * dx + dy * dy <= 10.0f * 10.0f) hoveredSampleIndex = i;

			// 가중치 반경 원
			if (sampleWeight > 0.005f)
			{
				const float weightRadius = 6.0f + sampleWeight * 18.0f;
				const int fillAlpha = static_cast<int>(35.0f + sampleWeight * 95.0f);
				drawList->AddCircleFilled(sp, weightRadius, IM_COL32(70, 180, 255, fillAlpha));
				drawList->AddCircle(sp, weightRadius, IM_COL32(100, 210, 255, 220), 0, 1.5f);
			}

			// 샘플 중심 포인트
			const ImU32 pointColor = isSelected ? IM_COL32(255, 200, 60, 255)
				: (clipPtr ? IM_COL32(220, 230, 245, 255) : IM_COL32(160, 100, 100, 255));
			drawList->AddCircleFilled(sp, isSelected ? 6.0f : 4.5f, pointColor);
			if (isSelected)
				drawList->AddCircle(sp, 7.5f, IM_COL32(255, 255, 255, 230), 0, 1.5f);

			// 퍼센트 텍스트
			if (sampleWeight > 0.005f)
			{
				char pctBuf[16];
				std::snprintf(pctBuf, sizeof(pctBuf), "%.0f%%", sampleWeight * 100.0f);
				drawList->AddText(ImVec2(sp.x + 7.0f, sp.y - 14.0f), IM_COL32(235, 245, 255, 255), pctBuf);
			}
		}

		// 테스트 커서 시각화
		const ImVec2 cursorSp = worldToScreen(m_Blend2DTestPos.x, m_Blend2DTestPos.y);
		const ImU32 cursorColor = IM_COL32(0, 235, 200, 255);
		drawList->AddLine(ImVec2(cursorSp.x - 7.0f, cursorSp.y), ImVec2(cursorSp.x + 7.0f, cursorSp.y), cursorColor, 1.5f);
		drawList->AddLine(ImVec2(cursorSp.x, cursorSp.y - 7.0f), ImVec2(cursorSp.x, cursorSp.y + 7.0f), cursorColor, 1.5f);
		drawList->AddCircle(cursorSp, 5.0f, cursorColor, 0, 1.5f);

		drawList->PopClipRect();

		if (hoveredSampleIndex >= 0 && m_Blend2DDraggingSampleIndex < 0)
		{
			const auto& hs = tree.samples[hoveredSampleIndex];
			const std::string clipName = (hs.clip.IsValid() && hs.clip.Get())
				? Engine::WStrToStr(std::filesystem::path(hs.clip->GetPath()).filename().wstring())
				: LOCAL("AnimatorGraph.NoMotion");
			ImGui::SetTooltip("#%d %s (%.2f, %.2f)", hoveredSampleIndex, clipName.c_str(), hs.x, hs.y);
		}
	}

	ImGui::TextWrapped("%s", LOCAL_FMT("AnimatorGraph.Blend2DTestCursor", m_Blend2DTestPos.x, m_Blend2DTestPos.y).c_str());
	ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.Blend2DFreeformHint"));
}

void AnimatorGraphPanel::DrawBlend2DSamplesSection(Engine::AnimState& state)
{
	auto& tree = state.blendTree2D;

	ImGui::Spacing();
	ImGui::Text("%s (%d)", LOCAL_CSTR("AnimatorGraph.Samples"), static_cast<int>(tree.samples.size()));

	const std::string addLabel = std::string(ICON_FA_PLUS " ") + LOCAL("AnimatorGraph.Blend2DAddSample");
	EditorUI::SameLineIfFits(ImGui::CalcTextSize(addLabel.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f);
	if (ImGui::SmallButton(addLabel.c_str()))
	{
		tree.samples.push_back(Engine::BlendSample2D{});
		m_Blend2DSelectedSampleIndex = static_cast<int>(tree.samples.size()) - 1;
		m_History.MarkEdited();
	}

	if (tree.samples.empty()) return;

	int sampleToRemove = -1;
	if (ImGui::BeginTable("##Blend2DSamplesTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.Blend2DPosX"), ImGuiTableColumnFlags_WidthFixed, 54.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.Blend2DPosY"), ImGuiTableColumnFlags_WidthFixed, 54.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("AnimatorGraph.Clip"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Delete"), ImGuiTableColumnFlags_WidthFixed,
			(std::max)(EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x, ImGui::CalcTextSize(LOCAL_CSTR("EditorCommon.Delete")).x));
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < tree.samples.size(); ++i)
		{
			auto& sample = tree.samples[i];
			ImGui::PushID(static_cast<int>(i));

			ImGui::TableNextRow();
			if (static_cast<int>(i) == m_Blend2DSelectedSampleIndex)
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(60, 90, 130, 70));

			ImGui::TableSetColumnIndex(0);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##SampleX", &sample.x, 0.02f, -100.0f, 100.0f, "%.2f"))
			{
				m_Blend2DSelectedSampleIndex = static_cast<int>(i);
				m_History.MarkEdited();
			}
			if (ImGui::IsItemActivated()) m_Blend2DSelectedSampleIndex = static_cast<int>(i);

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##SampleY", &sample.y, 0.02f, -100.0f, 100.0f, "%.2f"))
			{
				m_Blend2DSelectedSampleIndex = static_cast<int>(i);
				m_History.MarkEdited();
			}
			if (ImGui::IsItemActivated()) m_Blend2DSelectedSampleIndex = static_cast<int>(i);

			ImGui::TableSetColumnIndex(2);
			if (Editor::DrawInspectorResourcePicker(sample.clip, true))
			{
				m_Blend2DSelectedSampleIndex = static_cast<int>(i);
				m_History.MarkEdited();
			}

			ImGui::TableSetColumnIndex(3);
			if (EditorUI::SmallIconButton("DelBlend2DSample", ICON_FA_TRASH, "EditorCommon.Delete"))
				sampleToRemove = static_cast<int>(i);

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (sampleToRemove >= 0 && sampleToRemove < static_cast<int>(tree.samples.size()))
	{
		tree.samples.erase(tree.samples.begin() + sampleToRemove);
		if (m_Blend2DSelectedSampleIndex == sampleToRemove)
			m_Blend2DSelectedSampleIndex = -1;
		else if (m_Blend2DSelectedSampleIndex > sampleToRemove)
			--m_Blend2DSelectedSampleIndex;

		if (m_Blend2DDraggingSampleIndex == sampleToRemove)
		{
			m_Blend2DDraggingSampleIndex = -1;
			m_Blend2DDragModified = false;
		}
		else if (m_Blend2DDraggingSampleIndex > sampleToRemove)
		{
			--m_Blend2DDraggingSampleIndex;
		}

		m_History.MarkEdited();
	}
}

void AnimatorGraphPanel::DrawTransitionInspector(Engine::AnimTransition& tr)
{
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.TransitionInspector"));
	const auto sourceName = tr.fromAnyState ? LOCAL("AnimatorGraph.AnyState") : tr.sourceState;
	ImGui::TextWrapped("%s → %s", sourceName.c_str(), tr.targetState.c_str());
	ImGui::Separator();

	if (EditorUI::BeginPropertyTable("TransitionPropertiesTable", EditorUI::InspectorPropertyTableStyle()))
	{
		if (!tr.fromAnyState)
		{
			EditorUI::PropertyRow("AnimatorGraph.SourceState");
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo("##TransitionSourceCombo", tr.sourceState.c_str()))
			{
				for (const auto& s : m_StateMachine->states)
				{
					const bool isSelected = (s.name == tr.sourceState);
					if (ImGui::Selectable(s.name.c_str(), isSelected))
					{
						if (tr.sourceState != s.name)
						{
							tr.sourceState = s.name;
							m_History.MarkEdited();
						}
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
		}

		EditorUI::PropertyRow("AnimatorGraph.TargetState");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::BeginCombo("##TransitionTargetCombo", tr.targetState.c_str()))
		{
			for (const auto& s : m_StateMachine->states)
			{
				const bool isSelected = (s.name == tr.targetState);
				if (ImGui::Selectable(s.name.c_str(), isSelected))
				{
					if (tr.targetState != s.name)
					{
						tr.targetState = s.name;
						m_History.MarkEdited();
					}
				}
				if (isSelected) ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		EditorUI::PropertyRow("AnimatorGraph.Duration");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##TransitionDuration", &tr.duration, 0.02f, 0.0f, 5.0f, "%.2fs")) m_History.MarkEdited();

		if (!tr.fromAnyState)
		{
			EditorUI::PropertyRow("AnimatorGraph.HasExitTime");
			if (ImGui::Checkbox("##TransitionHasExitTime", &tr.hasExitTime)) m_History.MarkEdited();

			if (tr.hasExitTime)
			{
				EditorUI::PropertyRow("AnimatorGraph.ExitTime");
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::SliderFloat("##TransitionExitTime", &tr.exitTime, 0.0f, 1.0f, "%.2f")) m_History.MarkEdited();
			}
		}

		EditorUI::EndPropertyTable();
	}

	if (tr.fromAnyState && tr.conditions.empty() && tr.conditionGroups.empty())
	{
		ImGui::Spacing();
		ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.AnyStateNeedsCondition"));
	}

	ImGui::Spacing();
	ImGui::Separator();

	using Logic = Engine::ETransitionConditionLogic;
	using Condition = Engine::TransitionCondition;

	const char* logicKeys[] = { "AnimatorGraph.ConditionAll", "AnimatorGraph.ConditionAny" };
	const char* modeKeys[] = {
		"AnimatorGraph.CondGreater", "AnimatorGraph.CondLess", "AnimatorGraph.CondEqual",
		"AnimatorGraph.CondNotEqual", "AnimatorGraph.CondIfTrue", "AnimatorGraph.CondIfFalse"
	};

	const auto drawLogic = [&](const char* titleKey, Logic& logic)
		{
			const int selected = logic == Logic::Any ? 1 : 0;
			const std::string id = std::string("##") + titleKey;

			ImGui::TextUnformatted(LOCAL_CSTR(titleKey));
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::BeginCombo(id.c_str(), LOCAL_CSTR(logicKeys[selected])))
			{
				for (int i = 0; i < 2; ++i)
				{
					if (ImGui::Selectable(LOCAL_LABEL(logicKeys[i]).c_str(), i == selected) && i != selected)
					{
						logic = i == 0 ? Logic::All : Logic::Any;
						m_History.MarkEdited();
					}
				}
				ImGui::EndCombo();
			}
		};

	const auto drawConditions = [&](std::vector<Condition>& conditions, Logic& logic)
		{
			drawLogic("AnimatorGraph.ConditionLogic", logic);

			if (EditorUI::SmallIconButton("AddCond", ICON_FA_PLUS, "EditorCommon.Add"))
			{
				Condition condition;
				if (!m_StateMachine->defaultParameters.empty())
					condition.parameterName = m_StateMachine->defaultParameters.begin()->first;

				conditions.push_back(condition);
				m_History.MarkEdited();
			}

			int toRemove = -1;
			for (size_t i = 0; i < conditions.size(); ++i)
			{
				auto& condition = conditions[i];
				ImGui::PushID(static_cast<int>(i));

				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##ParamCombo", condition.parameterName.c_str()))
				{
					for (const auto& [name, _] : m_StateMachine->defaultParameters)
					{
						const bool selected = condition.parameterName == name;
						if (ImGui::Selectable(name.c_str(), selected) && !selected)
						{
							condition.parameterName = name;
							m_History.MarkEdited();
						}
					}
					ImGui::EndCombo();
				}

				const int mode = static_cast<int>(condition.mode);
				const char* previewKey = mode >= 0 && mode < 6 ? modeKeys[mode] : "EditorCommon.None";

				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::BeginCombo("##ModeCombo", LOCAL_CSTR(previewKey)))
				{
					for (int m = 0; m < 6; ++m)
					{
						if (ImGui::Selectable(LOCAL_LABEL(modeKeys[m]).c_str(), mode == m) && mode != m)
						{
							condition.mode = static_cast<Engine::EConditionMode>(m);
							m_History.MarkEdited();
						}
					}
					ImGui::EndCombo();
				}

				if (condition.mode != Engine::EConditionMode::IfTrue && condition.mode != Engine::EConditionMode::IfFalse)
				{
					const auto& style = ImGui::GetStyle();
					const float deleteWidth = ImGui::CalcTextSize(ICON_FA_TRASH).x + style.FramePadding.x * 2.0f;
					const float available = ImGui::GetContentRegionAvail().x;
					const float valueWidth = (std::max)(1.0f, available - deleteWidth - style.ItemSpacing.x);

					ImGui::SetNextItemWidth(valueWidth);
					if (ImGui::DragFloat("##Threshold", &condition.threshold, 0.1f))
						m_History.MarkEdited();

					ImGui::SameLine();
				}

				if (EditorUI::SmallIconButton("DeleteCondition", ICON_FA_TRASH, "EditorCommon.Delete"))
					toRemove = static_cast<int>(i);

				ImGui::PopID();
			}

			if (toRemove >= 0)
			{
				conditions.erase(conditions.begin() + toRemove);
				m_History.MarkEdited();
			}
		};

	ImGui::TextUnformatted(LOCAL_CSTR("AnimatorGraph.Conditions"));
	if (ImGui::Button(LOCAL_LABEL("AnimatorGraph.AddConditionGroup").c_str()))
	{
		tr.conditionGroups.emplace_back();
		m_History.MarkEdited();
	}

	if (!tr.conditionGroups.empty())
		drawLogic("AnimatorGraph.GroupLogic", tr.groupLogic);

	ImGui::PushID(0);
	if (!tr.conditionGroups.empty())
		ImGui::TextUnformatted(LOCAL_FMT("AnimatorGraph.ConditionGroupTitle", 1).c_str());
	drawConditions(tr.conditions, tr.conditionLogic);
	ImGui::PopID();

	int groupToRemove = -1;
	for (size_t i = 0; i < tr.conditionGroups.size(); ++i)
	{
		auto& group = tr.conditionGroups[i];
		ImGui::PushID(static_cast<int>(i + 1));

		ImGui::Separator();
		ImGui::TextUnformatted(LOCAL_FMT("AnimatorGraph.ConditionGroupTitle", i + 2).c_str());
		if (EditorUI::SmallIconButton("DeleteGroup", ICON_FA_TRASH, "EditorCommon.Delete"))
			groupToRemove = static_cast<int>(i);
		if (group.conditions.empty())
			ImGui::TextWrapped("%s", LOCAL_CSTR("AnimatorGraph.EmptyConditionGroup"));

		drawConditions(group.conditions, group.conditionLogic);
		ImGui::PopID();
	}

	if (groupToRemove >= 0)
	{
		tr.conditionGroups.erase(tr.conditionGroups.begin() + groupToRemove);
		m_History.MarkEdited();
	}
}

void AnimatorGraphPanel::RequestNew()
{
	RunWithDiscardConfirmation([this] { ExecuteNew(); m_NewDocument = true; }, true);
}

void AnimatorGraphPanel::RequestOpen()
{
	RunWithDiscardConfirmation([this] { OpenDocument(); }, true);
}

void AnimatorGraphPanel::ExecuteNew()
{
	m_StateMachine = std::make_shared<Engine::AnimatorStateMachine>();
	m_StateMachine->defaultState = "Idle";
	m_StateMachine->states.push_back(Engine::AnimState{ "Idle" });
	m_CurrentFilePath.clear();
	m_TargetResourcePath.clear();
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	ResetHistoryFromStateMachine();
	m_NeedNavigateToContent = true;
	m_NeedNodePositionSync = true;
	SyncPreviewStateMachine();
}

void AnimatorGraphPanel::ReleaseResourceDocument()
{
	m_PendingPreviewModel.reset();
	ReplacePreviewModel({});
	m_CurrentFilePath.clear();
	m_TargetResourcePath.clear();
	m_NodePositions.clear();
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	m_ParameterRenameDraft.clear();
	m_StateRenameDraft.clear();
	m_History.Clear();
	m_NewDocument = false;
	if (m_EditorContext)
	{
		if (ed::GetCurrentEditor() == m_EditorContext) ed::SetCurrentEditor(nullptr);
		ed::DestroyEditor(m_EditorContext);
		m_EditorContext = nullptr;
	}
	ExecuteNew();
}

void AnimatorGraphPanel::InitializePreview()
{
	m_PreviewWidget.Initialize(L"AnimGraphPreview", 260, 240, true);
}

bool AnimatorGraphPanel::ReplacePreviewModel(const Engine::ResourceHandle<Engine::Model>& requested)
{
	auto* scene = m_PreviewWidget.GetScene();
	if (!scene) return false;

	Engine::Entity* replacement = nullptr;
	if (requested != Engine::ResourceHandle<Engine::Model>{})
	{
		const auto* model = requested.Get();
		if (!model || !model->GetSkeleton().IsValid())
		{
			EDITOR_LOG_ERROR("Animator preview requires a model with a loaded skeleton.");
			return false;
		}

		auto& manager = Engine::ResourceManager::Get();
		const Engine::ResourceHandle<Engine::MaterialInterface> material(
			manager.LoadFile(L"Resources/Material/SkinningMaterial.bammat.json"));

		const auto result = scene->InstantiateModel(*model, material, replacement);
		if (result != Engine::EResult::Success)
		{
			EDITOR_LOG_ERROR("Failed to instantiate animator preview model: {}", ResultToString(result));
			return false;
		}
	}

	if (m_PreviewModelEntity && !scene->DestroyEntity(*m_PreviewModelEntity))
	{
		EDITOR_LOG_ERROR("Failed to remove the current animator preview model.");
		if (replacement && !scene->DestroyEntity(*replacement))
		{
			EDITOR_LOG_ERROR("Failed to roll back the replacement animator preview model.");
		}
		return false;
	}

	m_PreviewModel = requested;
	m_PreviewModelEntity = replacement;
	SyncPreviewStateMachine();
	return true;
}

void AnimatorGraphPanel::SyncPreviewStateMachine()
{
	if (!m_PreviewModelEntity || !m_StateMachine) return;
	auto* animator = m_PreviewWidget.GetRegistry().try_get<Engine::AnimatorComponent>(m_PreviewModelEntity->GetEntityHandle());
	if (animator)
	{
		animator->runtimeState.BindStateMachine(m_StateMachine);
		animator->isPlaying = true;
	}
}

void AnimatorGraphPanel::UpdatePreview(f32 dt)
{
	if (!m_ShowPreviewViewport || !m_PreviewWidget.GetScene()) return;

	if (m_PendingPreviewModel)
	{
		auto requested = std::move(*m_PendingPreviewModel);
		m_PendingPreviewModel.reset();

		if (!ReplacePreviewModel(requested))
		{
			NotificationManager::Get().AddNotification(LOCAL("AnimatorGraph.Title"),
				LOCAL("AnimatorGraph.PreviewModelChangeFailed"), ELogLevel::Warn);
		}
	}

	if (m_PreviewModelEntity && m_StateMachine)
	{
		auto& world = m_PreviewWidget.GetRegistry();
		const auto entity = m_PreviewModelEntity->GetEntityHandle();
		auto* animator = Engine::TryGetEnabledComponent<Engine::AnimatorComponent>(world, entity);

		if (animator)
		{
			auto& runtime = animator->runtimeState;
			if (runtime.GetStateMachine() != m_StateMachine)
			{
				runtime.BindStateMachine(m_StateMachine);
			}

			const auto& defaults = m_StateMachine->defaultParameters;
			auto& parameters = runtime.GetParameters().GetAll();

			std::erase_if(parameters, [&](const auto& entry)
				{
					return !defaults.contains(entry.first);
				});

			for (const auto& [name, definition] : defaults)
			{
				auto [it, inserted] = parameters.try_emplace(name, definition);
				if (!inserted && it->second.type != definition.type)
				{
					it->second = definition;
				}
			}
		}
	}

	m_PreviewWidget.BeginFrame(dt);
	m_PreviewWidget.Submit(dt);
}

void AnimatorGraphPanel::DrawPreviewSection(float width, float height)
{
	ImGui::TextUnformatted(LOCAL_CSTR("AnimatorGraph.PreviewModel"));

	auto candidate = m_PreviewModel;
	if (DrawInspectorResourcePicker(candidate, true) && candidate != m_PreviewModel)
	{
		m_PendingPreviewModel = std::move(candidate);
	}

	m_PreviewWidget.Draw(ImVec2(width, height));
}

void AnimatorGraphPanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("Animator.LeftPaneWidth", m_LeftPaneWidth);
	workspace.SetSplitter("Animator.InspectorWidth", m_InspectorWidth);
}

void AnimatorGraphPanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_LeftPaneWidth = workspace.GetSplitter("Animator.LeftPaneWidth", 260.0f);
	m_InspectorWidth = workspace.GetSplitter("Animator.InspectorWidth", 280.0f);
}
