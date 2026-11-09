#include "AnimatorGraphPanel.h"
#include "EditorUI.h"
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
#include "NotificationManager.h"
#include <imgui_stdlib.h>
#include <map>
#include <string_view>
#include <tuple>
#include <utility>
#include "InputManager.h"
#include <cmath>


AnimatorGraphPanel::AnimatorGraphPanel()
{
	m_Name = L"Animator Graph";
	m_DisplayNameKey = "AnimatorGraph.Title";
	m_CanvasScrolling = ImVec2(0.0f, 0.0f);
	ExecuteNew();
	InitializePreview();
}
AnimatorGraphPanel::~AnimatorGraphPanel()
{
	if (s_ActivePanel == this)
		s_ActivePanel = nullptr;

	m_PreviewWidget.Free();
}

void AnimatorGraphPanel::SetStateMachine(const std::shared_ptr<Engine::AnimatorStateMachine>& sm)
{
	m_StateMachine = sm;
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_CurrentFilePath.clear();
	m_IsDirty = false;
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
	return caption + " - " + std::filesystem::path(m_CurrentFilePath).filename().string() + "###AnimatorGraphPanel";
}

void AnimatorGraphPanel::OnResourceWindowVisibility(bool visible)
{
	if (visible && m_Focused) s_ActivePanel = this;
}

void AnimatorGraphPanel::DrawCustomContent()
{
	const float totalWidth = ImGui::GetContentRegionAvail().x;
	const float contentHeight = ImGui::GetContentRegionAvail().y;
	const float splitterThickness = 4.0f;
	const float spacingX = ImGui::GetStyle().ItemSpacing.x;
	// 캔버스 가용 너비 계산 (전체 폭 - 좌측 - 우측 - 스플리터 2개 - 아이템 간격 2개)
	const float overhead = (splitterThickness * 2.0f) + (spacingX * 2.0f);
	float canvasWidth = (std::max)(200.0f, totalWidth - m_LeftPaneWidth - m_InspectorWidth - overhead);
	// ── [좌측 영역] 3D 프리뷰 뷰포트 + 파라미터 독 ──
	ImGui::BeginChild("LeftPane", ImVec2(m_LeftPaneWidth, contentHeight), true);
	if (m_ShowPreviewViewport)
	{
		const float previewHeight = std::min(m_LeftPaneWidth * 0.95f, contentHeight * 0.45f);
		DrawPreviewSection(m_LeftPaneWidth - 16.0f, previewHeight);
		ImGui::Separator();
	}
	DrawParametersTab();
	ImGui::EndChild();
	ImGui::SameLine();
	// ── [좌측 스플리터: 좌측 ↔ 캔버스] ──
	Editor::SplitterV("##LeftSplitter", &m_LeftPaneWidth, &canvasWidth, 180.0f, 200.0f, contentHeight, splitterThickness);
	ImGui::SameLine();
	// ── [중앙 영역] 상태 머신 노드 캔버스 ──
	ImGui::BeginChild("CanvasPanel", ImVec2(canvasWidth, contentHeight), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
	DrawCanvas();
	ImGui::EndChild();
	ImGui::SameLine();
	// ── [우측 스플리터: 캔버스 ↔ 우측 인스펙터] ──
	Editor::SplitterV("##RightSplitter", &canvasWidth, &m_InspectorWidth, 200.0f, 200.0f, contentHeight, splitterThickness);
	ImGui::SameLine();
	// ── [우측 영역] 인스펙터 패널 ──
	ImGui::BeginChild("InspectorPanel", ImVec2(m_InspectorWidth, contentHeight), true);
	DrawSelectionInspector();
	ImGui::EndChild();
}

bool AnimatorGraphPanel::CanDeleteSelection() const
{
	if (!m_Open || !m_StateMachine || m_PendingAction != EPendingAction::None) return false;

	const bool hasState = m_SelectedStateIndex >= 0 &&
		m_SelectedStateIndex < static_cast<int>(m_StateMachine->states.size());
	const bool hasTransition = m_SelectedTransitionIndex >= 0 &&
		m_SelectedTransitionIndex < static_cast<int>(m_StateMachine->transitions.size());

	return hasState || hasTransition;
}

void AnimatorGraphPanel::DeleteSelection()
{
	if (!CanDeleteSelection()) return;

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
	else
	{
		m_StateMachine->transitions.erase(m_StateMachine->transitions.begin() + m_SelectedTransitionIndex);
	}

	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	m_DraggingStateIndex = -1;
	m_IsDirty = true;
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
		if (ImGui::MenuItem((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.Save")).c_str(), sm.GetShortcutHint("Animator.Save")))
			SaveDocument(false);
		if (ImGui::MenuItem((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.SaveAs")).c_str(), sm.GetShortcutHint("Animator.SaveAs")))
			SaveDocument(true);
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.Edit")))
	{
		ACTION_MENU_ITEM((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(), "Animator.DeleteSelection");
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.View")))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("AnimatorGraph.ResetView").c_str()))
			m_CanvasScrolling = ImVec2(0.0f, 0.0f);
		ImGui::EndMenu();
	}
}

void AnimatorGraphPanel::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	if (toolbar.Icon("QuickNew", ICON_FA_FILE, "EditorCommon.New")) RequestNew();
	if (toolbar.Icon("QuickOpen", ICON_FA_FOLDER_OPEN, "EditorCommon.Open")) RequestOpen();
	if (toolbar.Icon("QuickSave", ICON_FA_FLOPPY_DISK, "EditorCommon.Save")) SaveDocument(false);

	const std::string fileName = m_CurrentFilePath.empty()
		? LOCAL("AnimatorGraph.Unsaved") : std::filesystem::path(m_CurrentFilePath).filename().string();
	const std::string status = "[" + fileName + (m_IsDirty ? " *]" : "]");
	toolbar.Text(status.c_str());

	EditorUI::IconButtonOptions previewOptions;
	previewOptions.Active = m_ShowPreviewViewport;
	if (toolbar.Icon("Preview", m_ShowPreviewViewport ? ICON_FA_EYE : ICON_FA_EYE_SLASH,
		"AnimatorGraph.TogglePreview", previewOptions))
		m_ShowPreviewViewport = !m_ShowPreviewViewport;
	if (toolbar.Button("CreateState", ICON_FA_PLUS, "AnimatorGraph.CreateState"))
		AddState(ImVec2(250.0f, 150.0f));

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
	ResourceEditorInterface::SetTargetResource(assetPath);
	LoadFromFile(assetPath);
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
	m_CurrentFilePath = targetPath;
	m_TargetResourcePath = path;
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	m_IsDirty = false;
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

	if (targetPath.empty() || !m_StateMachine) return false;

	if (!m_StateMachine->SaveToFile(targetPath))
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to save animator controller: {}", targetPath);
		return false;
	}

	m_CurrentFilePath = targetPath;
	m_TargetResourcePath = std::filesystem::path(targetPath);
	m_IsDirty = false;
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
				m_IsDirty = true;
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

	std::optional<std::string> toRename;
	std::string toDelete;

	for (auto& [paramName, paramData] : parameters)
	{
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
			if (ImGui::DragFloat("##Val", &paramData.floatValue, 0.05f)) m_IsDirty = true;
			break;
		case ParamType::Int:
			if (ImGui::DragInt("##Val", &paramData.intValue)) m_IsDirty = true;
			break;
		case ParamType::Bool:
		case ParamType::Trigger:
			if (ImGui::Checkbox("##Val", &paramData.boolValue)) m_IsDirty = true;
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
		m_IsDirty = true;
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

	m_IsDirty = true;
}

void AnimatorGraphPanel::DrawPreviewParameters()
{
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f),
		"%s", LOCAL_CSTR("AnimatorGraph.PreviewParameters"));

	if (!m_StateMachine || !m_ShowPreviewViewport ||
		!m_PreviewWidget.GetScene() || !m_PreviewModelEntity)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.PreviewUnavailable"));
		return;
	}

	auto& world = m_PreviewWidget.GetRegistry();
	const auto entity = m_PreviewModelEntity->GetEntityHandle();
	auto* animator = world.valid(entity) ? world.try_get<Engine::AnimatorComponent>(entity) : nullptr;

	if (!animator || animator->runtimeState.GetStateMachine() != m_StateMachine)
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.PreviewUnavailable"));
		return;
	}

	auto& runtime = animator->runtimeState;

	ImGui::PushID("PreviewParameters");
	const auto& defaultState = m_StateMachine->defaultState;
	const bool canRestart = !defaultState.empty() && m_StateMachine->FindState(defaultState) != nullptr;

	ImGui::BeginDisabled(!canRestart);
	if (ImGui::Button(LOCAL_LABEL("AnimatorGraph.PreviewRestart").c_str()))
		SyncPreviewStateMachine();
	ImGui::EndDisabled();

	if (!canRestart && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		ImGui::SetTooltip("%s", LOCAL_CSTR("AnimatorGraph.PreviewRestartNeedsEntry"));

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

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	const auto screenMouse = Engine::InputManager::Get().GetScreenMousePosition();
	const ImVec2 mousePos(screenMouse.x, screenMouse.y);
	// 캔버스 영역 위에서 마우스 휠 클릭(Middle 버튼) 드래그 시 캔버스 패닝 이동
	auto& input = Engine::InputManager::Get();
	const auto& io = ImGui::GetIO();
	const auto middle = Engine::EMouseButton::Middle;
	const bool canPan = !io.WantTextInput && !io.AppFocusLost;

	if (!canPan || !input.IsMouseButtonPressed(middle) || !ImGui::IsWindowFocused())
		m_IsCanvasPanning = false;

	if (canPan && ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && input.IsMouseButtonDown(middle))
	{
		ImGui::SetWindowFocus();
		m_IsCanvasPanning = true;
	}

	if (m_IsCanvasPanning)
	{
		const auto delta = input.GetMouseDelta();
		m_CanvasScrolling.x += delta.x;
		m_CanvasScrolling.y += delta.y;
	}
	// ── 0. 현재 씬에서 선택된 엔티티의 런타임 애니메이터 상태 조회 ──
	const Engine::AnimatorRuntimeState* observedRuntime = nullptr;

	if (m_ShowPreviewViewport)
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

	// 1. 그리드 배경
	const float gridStep = 32.0f;
	const ImU32 gridColor = IM_COL32(200, 200, 200, 20);
	for (float x = std::fmod(m_CanvasScrolling.x, gridStep); x < canvasSize.x; x += gridStep)
		drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), gridColor);
	for (float y = std::fmod(m_CanvasScrolling.y, gridStep); y < canvasSize.y; y += gridStep)
		drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), gridColor);
	// 2. 트랜지션 연결선 렌더링
	const ImVec2 anyStatePos(50.0f, 60.0f);
	DrawTransitions(drawList, canvasPos, anyStatePos);
	// 3. Entry 노드 렌더링
	const ImVec2 nodeSize = ImVec2(130.0f, 45.0f);
	const ImVec2 anyStateMin(
		canvasPos.x + m_CanvasScrolling.x + anyStatePos.x,
		canvasPos.y + m_CanvasScrolling.y + anyStatePos.y);
	const ImVec2 anyStateMax(anyStateMin.x + nodeSize.x, anyStateMin.y + nodeSize.y);
	drawList->AddRectFilled(anyStateMin, anyStateMax, IM_COL32(105, 66, 145, 255), 5.0f);
	drawList->AddText(ImVec2(anyStateMin.x + 15.0f, anyStateMin.y + 13.0f),
		IM_COL32(255, 255, 255, 255), LOCAL_CSTR("AnimatorGraph.AnyState"));
	const ImVec2 entryPos(canvasPos.x + m_CanvasScrolling.x + 50.0f, canvasPos.y + m_CanvasScrolling.y + 150.0f);
	drawList->AddRectFilled(entryPos, ImVec2(entryPos.x + nodeSize.x, entryPos.y + nodeSize.y), IM_COL32(30, 130, 60, 255), 5.0f);
	drawList->AddText(ImVec2(entryPos.x + 15.0f, entryPos.y + 13.0f), IM_COL32(255, 255, 255, 255), LOCAL_CSTR("AnimatorGraph.Entry"));
	// 4. 상태 노드들 렌더링
	for (size_t i = 0; i < m_StateMachine->states.size(); ++i)
	{
		const auto& state = m_StateMachine->states[i];
		if (!m_NodePositions.contains(state.name))
			m_NodePositions[state.name] = ImVec2(220.0f + static_cast<float>(i * 160), 150.0f);
		ImVec2& nodePosRef = m_NodePositions[state.name];
		const ImVec2 screenPos = ImVec2(canvasPos.x + nodePosRef.x + m_CanvasScrolling.x, canvasPos.y + nodePosRef.y + m_CanvasScrolling.y);
		const bool isSelected = (m_SelectedStateIndex == static_cast<int>(i));
		const bool isDefault = (state.name == m_StateMachine->defaultState);
		const bool isLiveActive = (!liveActiveState.empty() && liveActiveState == state.name);
		// 마우스 클릭 및 드래그 (InputManager 기준)
		const bool isHovered = (mousePos.x >= screenPos.x && mousePos.x <= screenPos.x + nodeSize.x &&
			mousePos.y >= screenPos.y && mousePos.y <= screenPos.y + nodeSize.y);
		if (isHovered && Engine::InputManager::Get().IsMouseButtonDown(Engine::EMouseButton::Left))
		{
			if (m_ConnectingSourceIndex >= 0 && m_ConnectingSourceIndex != static_cast<int>(i))
			{
				Engine::AnimTransition newTr;
				newTr.sourceState = m_StateMachine->states[m_ConnectingSourceIndex].name;
				newTr.targetState = state.name;
				newTr.duration = 0.25f;
				m_StateMachine->transitions.push_back(newTr);
				m_ConnectingSourceIndex = -1;
				m_IsDirty = true;
			}
			else
			{
				m_SelectedStateIndex = static_cast<int>(i);
				m_SelectedTransitionIndex = -1;
				m_DraggingStateIndex = static_cast<int>(i);
			}
		}
		if (m_DraggingStateIndex == static_cast<int>(i))
		{
			if (Engine::InputManager::Get().IsMouseButtonPressed(Engine::EMouseButton::Left))
			{
				const auto delta = Engine::InputManager::Get().GetMouseDelta();
				nodePosRef.x += delta.x;
				nodePosRef.y += delta.y;
				m_IsDirty = true;
			}
			if (Engine::InputManager::Get().IsMouseButtonUp(Engine::EMouseButton::Left)) m_DraggingStateIndex = -1;
		}
		// 노드 배경
		const ImU32 bgCol = isDefault ? IM_COL32(180, 100, 30, 255) : IM_COL32(50, 60, 75, 255);
		drawList->AddRectFilled(screenPos, ImVec2(screenPos.x + nodeSize.x, screenPos.y + nodeSize.y), bgCol, 6.0f);
		if (isLiveActive)
		{
			const ImU32 activeColor = IM_COL32(0, 255, 200, 255);
			drawList->AddRect(ImVec2(screenPos.x - 2.0f, screenPos.y - 2.0f),
				ImVec2(screenPos.x + nodeSize.x + 2.0f, screenPos.y + nodeSize.y + 2.0f),
				activeColor, 7.0f, 0, 3.5f);

			if (isLiveTransitioning)
			{
				const ImVec2 barMin(screenPos.x + 5.0f, screenPos.y + nodeSize.y - 6.0f);
				const ImVec2 barMax(screenPos.x + nodeSize.x - 5.0f, screenPos.y + nodeSize.y - 2.0f);
				const float fillX = barMin.x + (barMax.x - barMin.x) * liveTransitionProgress;

				drawList->AddRectFilled(barMin, barMax, IM_COL32(20, 20, 20, 220), 2.0f);
				drawList->AddRectFilled(barMin, ImVec2(fillX, barMax.y), activeColor, 2.0f);
			}
		}
		else
		{
			const ImU32 borderCol = isSelected ? IM_COL32(80, 200, 255, 255) : IM_COL32(20, 20, 20, 255);
			drawList->AddRect(screenPos, ImVec2(screenPos.x + nodeSize.x, screenPos.y + nodeSize.y), borderCol, 6.0f, 0, isSelected ? 2.5f : 1.0f);
		}
		drawList->AddText(ImVec2(screenPos.x + 10.0f, screenPos.y + 8.0f), IM_COL32(255, 255, 255, 255), state.name.c_str());
		std::string subText;
		if (state.IsBlendTree2D())
			subText = LOCAL("AnimatorGraph.BlendTree2D");
		else if (state.IsBlendTree())
			subText = LOCAL("AnimatorGraph.BlendTree1D");
		else
			subText = state.clip.Get() ? LOCAL("AnimatorGraph.Clip") : LOCAL("AnimatorGraph.NoMotion");
		drawList->AddText(ImVec2(screenPos.x + 10.0f, screenPos.y + 24.0f), IM_COL32(180, 180, 180, 255), subText.c_str());
	}
	// 5. 트랜지션 생성 중 안내선
	if (m_ConnectingSourceIndex >= 0 && m_ConnectingSourceIndex < static_cast<int>(m_StateMachine->states.size()))
	{
		const auto& srcState = m_StateMachine->states[m_ConnectingSourceIndex];
		const ImVec2 srcPos = ImVec2(canvasPos.x + m_NodePositions[srcState.name].x + m_CanvasScrolling.x + nodeSize.x * 0.5f,
			canvasPos.y + m_NodePositions[srcState.name].y + m_CanvasScrolling.y + nodeSize.y * 0.5f);
		drawList->AddLine(srcPos, mousePos, IM_COL32(255, 200, 50, 200), 2.0f);
	}
	DrawCanvasContextMenu();
}

void AnimatorGraphPanel::DrawTransitions(ImDrawList* drawList, const ImVec2& canvasPos, const ImVec2& anyStatePos)
{
	using StatePair = std::tuple<bool, std::string_view, std::string_view>;

	struct TransitionGroup
	{
		size_t Count = 0;
		size_t NextLane = 0;
	};

	const auto getPair = [](const Engine::AnimTransition& transition) -> StatePair
		{
			if (transition.fromAnyState)
				return { true, std::string_view{}, transition.targetState };

			if (transition.sourceState < transition.targetState)
				return { false, transition.sourceState, transition.targetState };

			return { false, transition.targetState, transition.sourceState };
		};

	const auto toImVec2 = [](const glm::vec2& value)
		{
			return ImVec2(value.x, value.y);
		};

	const auto& transitions = m_StateMachine->transitions;
	std::map<StatePair, TransitionGroup> groups;
	for (const auto& transition : transitions)
		++groups[getPair(transition)].Count;

	auto& input = Engine::InputManager::Get();
	const auto mouse = input.GetScreenMousePosition();
	const bool clicked = ImGui::IsWindowHovered() &&
		input.IsMouseButtonDown(Engine::EMouseButton::Left);

	const glm::vec2 origin(canvasPos.x + m_CanvasScrolling.x, canvasPos.y + m_CanvasScrolling.y);
	const glm::vec2 nodeHalfSize(65.0f, 22.5f);
	constexpr float laneSpacing = 32.0f;
	const glm::vec2 markerHalfSize(10.0f, 10.0f);

	for (size_t i = 0; i < transitions.size(); ++i)
	{
		const auto& transition = transitions[i];
		const auto target = m_NodePositions.find(transition.targetState);
		if (target == m_NodePositions.end()) continue;

		ImVec2 sourcePos = anyStatePos;
		if (!transition.fromAnyState)
		{
			const auto source = m_NodePositions.find(transition.sourceState);
			if (source == m_NodePositions.end()) continue;
			sourcePos = source->second;
		}

		const glm::vec2 p1 = origin + glm::vec2(sourcePos.x, sourcePos.y) + nodeHalfSize;
		const glm::vec2 p2 = origin + glm::vec2(target->second.x, target->second.y) + nodeHalfSize;
		const glm::vec2 delta = p2 - p1;
		const float distance = glm::length(delta);
		if (distance < 0.001f) continue;

		const glm::vec2 direction = delta / distance;
		const glm::vec2 normal(-direction.y, direction.x);

		auto& group = groups.at(getPair(transition));
		const float lane = static_cast<float>(group.NextLane++) -
			(static_cast<float>(group.Count) - 1.0f) * 0.5f;
		const float sign = (transition.fromAnyState || transition.sourceState < transition.targetState) ? 1.0f : -1.0f;
		const glm::vec2 offset = normal * (lane * laneSpacing * sign);

		// 두 제어점을 이동해 곡선 중앙이 선택 마커 위치를 지나게 한다.
		const glm::vec2 controlOffset = offset * (4.0f / 3.0f);
		const float middleX = (p1.x + p2.x) * 0.5f;
		const glm::vec2 cp1 = glm::vec2(middleX, p1.y) + controlOffset;
		const glm::vec2 cp2 = glm::vec2(middleX, p2.y) + controlOffset;
		const glm::vec2 middle = (p1 + p2) * 0.5f + offset;

		const bool selected = m_SelectedTransitionIndex == static_cast<int>(i);
		const ImU32 lineColor = selected ? IM_COL32(80, 200, 255, 255) : IM_COL32(200, 200, 200, 180);
		const ImU32 markerColor = selected ? IM_COL32(80, 200, 255, 255) : IM_COL32(80, 80, 90, 255);

		drawList->AddBezierCubic(toImVec2(p1), toImVec2(cp1), toImVec2(cp2), toImVec2(p2),
			lineColor, selected ? 3.0f : 2.0f);

		const glm::vec2 markerMin = middle - markerHalfSize;
		const glm::vec2 markerMax = middle + markerHalfSize;
		drawList->AddRectFilled(toImVec2(markerMin), toImVec2(markerMax), markerColor, 4.0f);

		const glm::vec2 tangent = glm::normalize(delta + cp2 - cp1);
		const glm::vec2 arrowSide(-tangent.y, tangent.x);
		const glm::vec2 arrowTip = middle + tangent * 6.0f;
		const glm::vec2 arrowBase = middle - tangent * 4.0f;
		drawList->AddTriangleFilled(toImVec2(arrowTip), toImVec2(arrowBase + arrowSide * 5.0f),
			toImVec2(arrowBase - arrowSide * 5.0f), IM_COL32(255, 255, 255, 255));

		if (clicked && mouse.x >= markerMin.x && mouse.x <= markerMax.x &&
			mouse.y >= markerMin.y && mouse.y <= markerMax.y)
		{
			m_SelectedTransitionIndex = static_cast<int>(i);
			m_SelectedStateIndex = -1;
		}
	}
}

void AnimatorGraphPanel::DrawCanvasContextMenu()
{
	if (ImGui::BeginPopupContextWindow("CanvasContextMenu", ImGuiPopupFlags_MouseButtonRight))
	{
		if (m_SelectedStateIndex >= 0 && m_SelectedStateIndex < static_cast<int>(m_StateMachine->states.size()))
		{
			if (ImGui::MenuItem((ICON_FA_ARROW_RIGHT " " + LOCAL_LABEL("AnimatorGraph.MakeTransition")).c_str()))
			{
				m_ConnectingSourceIndex = m_SelectedStateIndex;
			}
			if (ImGui::MenuItem(LOCAL_LABEL("AnimatorGraph.MakeAnyStateTransition").c_str()))
			{
				Engine::AnimTransition transition{};
				transition.fromAnyState = true;
				transition.targetState = m_StateMachine->states[m_SelectedStateIndex].name;

				auto& transitions = m_StateMachine->transitions;
				transitions.push_back(std::move(transition));

				m_SelectedTransitionIndex = static_cast<int>(transitions.size() - 1);
				m_SelectedStateIndex = -1;
				m_ConnectingSourceIndex = -1;
				m_IsDirty = true;
			}
			ACTION_MENU_ITEM((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(), "Animator.DeleteSelection");
		}
		else
		{
			if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("AnimatorGraph.CreateState")).c_str()))
			{
				const auto screenMouse = Engine::InputManager::Get().GetScreenMousePosition();
				const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
				AddState(ImVec2(screenMouse.x - canvasPos.x - m_CanvasScrolling.x, screenMouse.y - canvasPos.y - m_CanvasScrolling.y));
			}
		}
		ImGui::EndPopup();
	}
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

	m_StateMachine->states.push_back(Engine::AnimState{ name });
	m_NodePositions.insert_or_assign(name, position);
	m_IsDirty = true;
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

			m_IsDirty = true;
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
		if (ImGui::BeginTable("##StatePropertiesTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", LOCAL_CSTR("PROP_PLAY_RATE"));
			ImGui::TableSetColumnIndex(1);
			if (ImGui::DragFloat("##StatePlayRate", &state.playRate, 0.05f, 0.0f, 10.0f, "%.2f")) m_IsDirty = true;

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", LOCAL_CSTR("PROP_IS_LOOPING"));
			ImGui::TableSetColumnIndex(1);
			if (ImGui::Checkbox("##StateIsLooping", &state.isLooping)) m_IsDirty = true;

			if (state.isLooping)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("%s", LOCAL_CSTR("PROP_LOOP_BLEND_DURATION"));
				ImGui::TableSetColumnIndex(1);
				if (ImGui::SliderFloat("##StateLoopDuration", &state.loopBlendDuration, 0.0f, 0.5f, "%.2fs")) m_IsDirty = true;
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.SetDefaultState"));
			ImGui::TableSetColumnIndex(1);
			const bool isDefault = (state.name == m_StateMachine->defaultState);
			bool defaultToggle = isDefault;
			if (ImGui::Checkbox("##SetDefaultStateToggle", &defaultToggle) && defaultToggle)
			{
				m_StateMachine->defaultState = state.name;
				m_IsDirty = true;
			}

			ImGui::EndTable();
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

		ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.MotionType"));
		ImGui::SameLine();
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
					m_IsDirty = true;
				}
				if (selected) ImGui::SetItemDefaultFocus();
				ImGui::PopID();
			}
			ImGui::EndCombo();
		}

		const auto drawFloatParameter = [&](const char* labelKey, std::string& name)
		{
			const auto& parameters = m_StateMachine->defaultParameters;
			const std::string preview = name.empty() ? LOCAL("EditorCommon.None") : name;

			if (ImGui::BeginCombo(LOCAL_LABEL(labelKey).c_str(), preview.c_str()))
			{
				if (ImGui::Selectable(LOCAL_LABEL("EditorCommon.None").c_str(), name.empty()) && !name.empty())
				{
					name.clear();
					m_IsDirty = true;
				}

				for (const auto& [parameterName, parameter] : parameters)
				{
					if (parameter.type != Engine::EAnimatorParamType::Float) continue;

					ImGui::PushID(parameterName.c_str());
					const std::string label = parameterName + "###FloatParameter";
					const bool selected = name == parameterName;
					if (ImGui::Selectable(label.c_str(), selected) && !selected)
					{
						name = parameterName;
						m_IsDirty = true;
					}
					if (selected) ImGui::SetItemDefaultFocus();
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}

			const auto it = parameters.find(name);
			return !name.empty() && it != parameters.end() && it->second.type == Engine::EAnimatorParamType::Float;
		};

		if (!state.IsBlendTree())
		{
			ImGui::Spacing();
			ImGui::TextDisabled("%s", LOCAL_CSTR("PROP_ANIMATION_CLIP"));
			ImGui::PushID("StateMotionClip");
			if (Editor::DrawInspectorResourcePicker(state.clip, true)) m_IsDirty = true;
			ImGui::PopID();
		}
		else if (state.IsBlendTree2D())
		{
			auto& tree = state.blendTree2D;
			DrawBlend2DPreview(state);

			ImGui::Spacing();
			if (ImGui::BeginTable("##Blend2DParamsTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.Blend2DParameterX"));
				ImGui::TableSetColumnIndex(1);
				const bool hasX = drawFloatParameter("AnimatorGraph.Blend2DParameterX", tree.parameterX);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.Blend2DParameterY"));
				ImGui::TableSetColumnIndex(1);
				const bool hasY = drawFloatParameter("AnimatorGraph.Blend2DParameterY", tree.parameterY);

				if (!hasX || !hasY)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TableSetColumnIndex(1);
					ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.Blend2DAxesRequired"));
				}

				const auto drawRangeRow = [&](const char* labelKey, float& minimum, float& maximum)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextDisabled("%s", LOCAL_CSTR(labelKey));
					ImGui::TableSetColumnIndex(1);
					float range[2] = { minimum, maximum };
					if (ImGui::DragFloat2(LOCAL_LABEL(labelKey).c_str(), range, 0.05f))
					{
						minimum = range[0];
						maximum = range[1];
						m_IsDirty = true;
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

				ImGui::EndTable();
			}

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
					if (Editor::DrawInspectorResourcePicker(clip, true)) m_IsDirty = true;
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
		}
		else
		{
			drawFloatParameter("AnimatorGraph.Parameter", state.blendTree.parameterName);

			ImGui::Spacing();
			ImGui::Text("%s (%d)", LOCAL_CSTR("AnimatorGraph.Samples"),
				static_cast<int>(state.blendTree.samples.size()));
			EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_PLUS, true).x);

			if (EditorUI::SmallIconButton("AddBlendSample", ICON_FA_PLUS, "EditorCommon.Add"))
			{
				const float nextThreshold = state.blendTree.samples.empty()
					? 0.0f : state.blendTree.samples.back().threshold + 1.0f;
				state.blendTree.AddSample(nextThreshold, {});
				m_IsDirty = true;
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
						m_IsDirty = true;
					}

					ImGui::TableSetColumnIndex(1);
					if (Editor::DrawInspectorResourcePicker(sample.clip, true)) m_IsDirty = true;

					ImGui::TableSetColumnIndex(2);
					if (EditorUI::SmallIconButton("DelSample", ICON_FA_TRASH, "EditorCommon.Delete")) sampleToRemove = static_cast<int>(i);

					ImGui::PopID();
				}
				ImGui::EndTable();
			}

			if (sampleToRemove >= 0 && sampleToRemove < static_cast<int>(state.blendTree.samples.size()))
			{
				state.blendTree.samples.erase(state.blendTree.samples.begin() + sampleToRemove);
				m_IsDirty = true;
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
			m_IsDirty = true;
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
				if (ImGui::DragFloat("##NotifyTime", &notify.triggerTime, 0.02f, 0.0f, 60.0f, "%.2fs")) m_IsDirty = true;

				ImGui::TableSetColumnIndex(1);
				char nameBuf[64] = {};
				std::strncpy(nameBuf, notify.name.c_str(), sizeof(nameBuf) - 1);
				if (ImGui::InputText("##NotifyName", nameBuf, sizeof(nameBuf)))
				{
					notify.name = nameBuf;
					m_IsDirty = true;
				}

				ImGui::TableSetColumnIndex(2);
				char payloadBuf[128] = {};
				std::strncpy(payloadBuf, notify.payload.c_str(), sizeof(payloadBuf) - 1);
				if (ImGui::InputText("##NotifyPayload", payloadBuf, sizeof(payloadBuf)))
				{
					notify.payload = payloadBuf;
					m_IsDirty = true;
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
			m_IsDirty = true;
		}
	}

	ImGui::Spacing();

	// ── 4. 트랜지션 목록 (Transitions) ──
	if (ImGui::CollapsingHeader((ICON_FA_ARROWS_SPLIT_UP_AND_LEFT " " + LOCAL("AnimatorGraph.Transitions")).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
	{
		int count = 0;
		for (size_t i = 0; i < m_StateMachine->transitions.size(); ++i)
		{
			const auto& tr = m_StateMachine->transitions[i];
			if (tr.sourceState == state.name || tr.targetState == state.name)
			{
				count++;
				ImGui::PushID(static_cast<int>(i));
				const bool isSelected = (m_SelectedTransitionIndex == static_cast<int>(i));
				const std::string label = (tr.fromAnyState ? "[Any] " : (tr.sourceState + " ")) + "-> " + tr.targetState;
				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					m_SelectedTransitionIndex = static_cast<int>(i);
					m_SelectedStateIndex = -1;
				}
				ImGui::PopID();
			}
		}
		if (count == 0)
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("AnimatorGraph.NoNodeSelected"));
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

void AnimatorGraphPanel::DrawTransitionInspector(Engine::AnimTransition& tr)
{
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.TransitionInspector"));
	const auto sourceName = tr.fromAnyState ? LOCAL("AnimatorGraph.AnyState") : tr.sourceState;
	ImGui::TextWrapped("%s → %s", sourceName.c_str(), tr.targetState.c_str());
	ImGui::Separator();

	if (ImGui::DragFloat(LOCAL_LABEL("AnimatorGraph.Duration").c_str(), &tr.duration, 0.02f, 0.0f, 5.0f, "%.2fs")) m_IsDirty = true;
	if (!tr.fromAnyState)
	{
		if (ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.HasExitTime").c_str(), &tr.hasExitTime))
			m_IsDirty = true;

		if (tr.hasExitTime &&
			ImGui::SliderFloat(LOCAL_LABEL("AnimatorGraph.ExitTime").c_str(), &tr.exitTime, 0.0f, 1.0f, "%.2f"))
		{
			m_IsDirty = true;
		}
	}
	else if (tr.conditions.empty() && tr.conditionGroups.empty())
	{
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
						m_IsDirty = true;
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
				m_IsDirty = true;
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
							m_IsDirty = true;
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
							m_IsDirty = true;
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
						m_IsDirty = true;

					ImGui::SameLine();
				}

				if (EditorUI::SmallIconButton("DeleteCondition", ICON_FA_TRASH, "EditorCommon.Delete"))
					toRemove = static_cast<int>(i);

				ImGui::PopID();
			}

			if (toRemove >= 0)
			{
				conditions.erase(conditions.begin() + toRemove);
				m_IsDirty = true;
			}
		};

	ImGui::TextUnformatted(LOCAL_CSTR("AnimatorGraph.Conditions"));
	if (ImGui::Button(LOCAL_LABEL("AnimatorGraph.AddConditionGroup").c_str()))
	{
		tr.conditionGroups.emplace_back();
		m_IsDirty = true;
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
		m_IsDirty = true;
	}
}

void AnimatorGraphPanel::RequestNew()
{
	if (m_IsDirty) m_PendingAction = EPendingAction::New;
	else ExecuteNew();
}

void AnimatorGraphPanel::RequestOpen()
{
	if (m_IsDirty) m_PendingAction = EPendingAction::Open;
	else ExecuteOpen();
}

void AnimatorGraphPanel::RequestClose()
{
	if (m_IsDirty) m_PendingAction = EPendingAction::Close;
	else ExecuteClose();
}

void AnimatorGraphPanel::ExecuteNew()
{
	m_StateMachine = std::make_shared<Engine::AnimatorStateMachine>();
	m_StateMachine->defaultState = "Idle";
	m_StateMachine->states.push_back(Engine::AnimState{ "Idle" });
	m_NodePositions.clear();
	m_NodePositions["Idle"] = ImVec2(220.0f, 150.0f);
	m_CurrentFilePath.clear();
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	m_DraggingStateIndex = -1;
	m_IsDirty = false;
	SyncPreviewStateMachine();
}

void AnimatorGraphPanel::ExecuteOpen()
{
	OpenDocument();
}

void AnimatorGraphPanel::ExecuteClose()
{
	m_Open = false;
}

void AnimatorGraphPanel::ExecutePendingAction()
{
	const EPendingAction action = m_PendingAction;
	m_PendingAction = EPendingAction::None;

	switch (action)
	{
	case EPendingAction::New: ExecuteNew(); break;
	case EPendingAction::Open: ExecuteOpen(); break;
	case EPendingAction::Close: ExecuteClose(); break;
	case EPendingAction::None: break;
	}
}

void AnimatorGraphPanel::DrawConfirmModal()
{
	const std::string modalTitle = LOCAL("EditorDocument.ConfirmTitle") + "###AnimatorUnsavedChanges";

	if (m_PendingAction != EPendingAction::None && !ImGui::IsPopupOpen(modalTitle.c_str()))
	{
		ImGui::OpenPopup(modalTitle.c_str());
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal(modalTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const std::string docName = m_CurrentFilePath.empty()
			? LOCAL("EditorDocument.Untitled")
			: std::filesystem::path(m_CurrentFilePath).filename().string();

		ImGui::TextUnformatted(LOCAL_FMT("EditorDocument.UnsavedMessage", docName).c_str());
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		const EditorUI::DialogAction actions[] = {
			{ "EditorCommon.Cancel" }, { "EditorDocument.Discard", true, true }, { "EditorDocument.Save" }
		};
		const int action = EditorUI::DialogButtonBar("AnimatorDocumentActions", actions, IM_ARRAYSIZE(actions));
		if (action == 2)
		{
			if (SaveDocument(false))
			{
				ExecutePendingAction();
				ImGui::CloseCurrentPopup();
			}
			else
			{
				m_PendingAction = EPendingAction::None;
				ImGui::CloseCurrentPopup();
			}
		}

		if (action == 1)
		{
			ExecutePendingAction();
			ImGui::CloseCurrentPopup();
		}

		if (action == 0)
		{
			m_PendingAction = EPendingAction::None;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
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
		auto* animator = world.valid(entity) ? world.try_get<Engine::AnimatorComponent>(entity) : nullptr;

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
