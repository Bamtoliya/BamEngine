#include "AnimatorGraphPanel.h"
#include "IconsFontAwesome7.h"
#include "FileDialogs.h"
#include "SceneManager.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Core/NameComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "SelectionManager.h"
#include "ResourcePicker.h"
#include "Entity.h"
#include "ResourceManager.h"
#include "Model.h"
#include "MaterialInterface.h"
#include "ShortcutManager.h"
#include "EditorSettings.h"


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

void AnimatorGraphPanel::Draw()
{
	m_Focused = false;
	m_Hovered = false;
	if (!m_Open) return;

	ImGui::SetNextWindowSize(ImVec2(1000.0f, 600.0f), ImGuiCond_FirstUseEver);
	const std::string title = ICON_FA_DIAGRAM_PROJECT " " + GetDisplayName() + "###AnimatorGraphPanel";

	ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar;
	if (m_IsDirty) windowFlags |= ImGuiWindowFlags_UnsavedDocument;

	bool openState = m_Open;
	const bool isVisible = ImGui::Begin(title.c_str(), &openState, windowFlags);

	if (!openState)
	{
		RequestClose();
	}

	if (isVisible)
	{
		m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (m_Focused) s_ActivePanel = this;
		m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

		// 메뉴바 및 퀵 툴바
		DrawMenuBar();
		DrawToolbar();
		ImGui::Separator();

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
	ImGui::End();

	// 2. 미저장 보호 모달은 패널의 Begin/End 외부 최상위 뷰포트에서 독립적으로 렌더링
	DrawConfirmModal();
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
			return transition.sourceState == deletedName || transition.targetState == deletedName;
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

void AnimatorGraphPanel::DrawMenuBar()
{
	if (ImGui::BeginMenuBar())
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

		ImGui::EndMenuBar();
	}
}

void AnimatorGraphPanel::DrawToolbar()
{
	// 1줄 퀵 액세스 툴바: [New] [Open] [Save] | [파일상태 배지] | [새 상태]
	if (ImGui::Button(ICON_FA_FILE "##QuickNew")) RequestNew();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.New"));
	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_FOLDER_OPEN "##QuickOpen")) RequestOpen();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Open"));

	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_FLOPPY_DISK "##QuickSave")) SaveDocument(false);
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Save"));

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 파일 상태 배지
	const std::string fileName = m_CurrentFilePath.empty()
		? LOCAL("AnimatorGraph.Unsaved") : std::filesystem::path(m_CurrentFilePath).filename().string();
	ImGui::TextDisabled("[%s%s]", fileName.c_str(), m_IsDirty ? " *" : "");

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	const char* vpIcon = m_ShowPreviewViewport ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
	if (ImGui::Button(vpIcon))
	{
		m_ShowPreviewViewport = !m_ShowPreviewViewport;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip(LOCAL_CSTR("AnimatorGraph.TogglePreview"));

	if (ImGui::Button((ICON_FA_PLUS " " + LOCAL_LABEL("AnimatorGraph.CreateState")).c_str()))
	{
		const std::string name = "State_" + std::to_string(m_StateMachine->states.size());
		m_StateMachine->states.push_back(Engine::AnimState{ name });
		m_NodePositions[name] = ImVec2(250.0f, 150.0f);
		m_IsDirty = true;
	}

	// 트랜지션 연결 중 안내 문구 및 취소 버튼
	if (m_ConnectingSourceIndex >= 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.SelectTargetToConnect"));
		ImGui::SameLine();
		if (ImGui::SmallButton(LOCAL_LABEL("AnimatorGraph.CancelEsc").c_str())) m_ConnectingSourceIndex = -1;
		if (Engine::InputManager::Get().IsKeyButtonDown(Engine::EKeyCode::Escape)) m_ConnectingSourceIndex = -1;
	}
}

bool AnimatorGraphPanel::OpenDocument()
{
	std::wstring selectedPath;
	const std::vector<FileDialogFilter> filters{
		{ StrToWStr(LOCAL("FileDialog.AnimatorFiles")), L"*.animator" },
		{ StrToWStr(LOCAL("FileDialog.JsonFiles")), L"*.json" }
	};
	if (!FileDialogs::OpenFileDialog(selectedPath, filters)) return false;

	const std::string targetPath = WStrToStr(selectedPath);
	auto tempStateMachine = std::make_shared<Engine::AnimatorStateMachine>();
	if (!tempStateMachine->LoadFromFile(targetPath))
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to load animator controller: {}", targetPath);
		return false;
	}

	m_StateMachine = std::move(tempStateMachine);
	m_CurrentFilePath = targetPath;
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
	m_IsDirty = false;
	SyncPreviewStateMachine();
	return true;
}

bool AnimatorGraphPanel::SaveDocument(bool saveAs)
{
	std::string targetPath = m_CurrentFilePath;
	if (saveAs || targetPath.empty())
	{
		std::wstring selectedPath;
		const std::vector<FileDialogFilter> filters{
			{ StrToWStr(LOCAL("FileDialog.AnimatorFiles")), L"*.animator" }
		};
		if (!FileDialogs::SaveFileDialog(selectedPath, filters, L"NewController", L"animator")) return false;
		targetPath = WStrToStr(selectedPath);
	}

	if (targetPath.empty() || !m_StateMachine) return false;

	if (!m_StateMachine->SaveToFile(targetPath))
	{
		BAM_LOG(Error, "Editor", "Asset", "Failed to save animator controller: {}", targetPath);
		return false;
	}

	m_CurrentFilePath = targetPath;
	m_IsDirty = false;
	return true;
}

void AnimatorGraphPanel::DrawParametersTab()
{
	if (!m_StateMachine) return;

	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Animator.Parameters"));
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);

	if (ImGui::Button(ICON_FA_PLUS)) ImGui::OpenPopup("AddParamPopup");

	if (ImGui::BeginPopup("AddParamPopup"))
	{
		if (ImGui::MenuItem(LOCAL_CSTR("AnimatorGraph.ParamFloat")))
		{
			const std::string name = "NewFloat" + std::to_string(m_StateMachine->defaultParameters.size());
			m_StateMachine->defaultParameters[name] = Engine::AnimatorParameter{ Engine::EAnimatorParamType::Float, 0.0f, 0, false };
			m_IsDirty = true;
		}
		if (ImGui::MenuItem(LOCAL_CSTR("AnimatorGraph.ParamInt")))
		{
			const std::string name = "NewInt" + std::to_string(m_StateMachine->defaultParameters.size());
			m_StateMachine->defaultParameters[name] = Engine::AnimatorParameter{ Engine::EAnimatorParamType::Int, 0.0f, 0, false };
			m_IsDirty = true;
		}
		if (ImGui::MenuItem(LOCAL_CSTR("AnimatorGraph.ParamBool")))
		{
			const std::string name = "NewBool" + std::to_string(m_StateMachine->defaultParameters.size());
			m_StateMachine->defaultParameters[name] = Engine::AnimatorParameter{ Engine::EAnimatorParamType::Bool, 0.0f, 0, false };
			m_IsDirty = true;
		}
		if (ImGui::MenuItem(LOCAL_CSTR("Animator.Trigger")))
		{
			const std::string name = "NewTrigger" + std::to_string(m_StateMachine->defaultParameters.size());
			m_StateMachine->defaultParameters[name] = Engine::AnimatorParameter{ Engine::EAnimatorParamType::Trigger, 0.0f, 0, false };
			m_IsDirty = true;
		}
		ImGui::EndPopup();
	}

	ImGui::Separator();

	std::string toDelete;
	for (auto& [paramName, paramData] : m_StateMachine->defaultParameters)
	{
		ImGui::PushID(paramName.c_str());
		ImGui::TextDisabled("[%s]", paramData.type == Engine::EAnimatorParamType::Float ? "F" : paramData.type == Engine::EAnimatorParamType::Int ? "I" : paramData.type == Engine::EAnimatorParamType::Bool ? "B" : "T");
		ImGui::SameLine();
		ImGui::Text("%s", paramName.c_str());

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
		if (ImGui::Button(ICON_FA_TRASH)) toDelete = paramName;

		switch (paramData.type)
		{
		case Engine::EAnimatorParamType::Float:
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragFloat("##Val", &paramData.floatValue, 0.05f)) m_IsDirty = true;
			break;
		case Engine::EAnimatorParamType::Int:
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::DragInt("##Val", &paramData.intValue)) m_IsDirty = true;
			break;
		case Engine::EAnimatorParamType::Bool:
		case Engine::EAnimatorParamType::Trigger:
			if (ImGui::Checkbox(paramData.type == Engine::EAnimatorParamType::Trigger ? LOCAL_CSTR("Animator.Trigger") : LOCAL_CSTR("AnimatorGraph.ParamBool"), &paramData.boolValue)) m_IsDirty = true;
			break;
		}
		ImGui::PopID();
		ImGui::Spacing();
	}
	if (!toDelete.empty())
	{
		m_StateMachine->defaultParameters.erase(toDelete);
		m_IsDirty = true;
	}
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
	if (ImGui::IsWindowHovered() && Engine::InputManager::Get().IsMouseButtonPressed(Engine::EMouseButton::Middle))
	{
		const auto delta = Engine::InputManager::Get().GetMouseDelta();
		m_CanvasScrolling.x += delta.x;
		m_CanvasScrolling.y += delta.y;
	}
	// ── 0. 현재 씬에서 선택된 엔티티의 런타임 애니메이터 상태 조회 ──
	std::string liveActiveState;
	bool isLiveTransitioning = false;
	std::string liveTargetState;
	float liveTransitionProgress = 0.0f;
	auto* currentScene = Engine::SceneManager::Get().GetCurrentScene();
	auto* selectedEntity = SelectionManager::Get().GetPrimarySelectedEntity();
	if (currentScene && selectedEntity)
	{
		auto& reg = currentScene->GetRegistry();
		entt::entity cur = selectedEntity->GetEntityHandle();
		const Engine::AnimatorComponent* animComp = nullptr;
		while (cur != entt::null && reg.valid(cur))
		{
			if ((animComp = reg.try_get<Engine::AnimatorComponent>(cur)) != nullptr) break;
			const auto* hierarchy = reg.try_get<Engine::HierarchyComponent>(cur);
			cur = hierarchy ? hierarchy->parent : entt::null;
		}
		if (animComp && animComp->runtimeState.HasStateMachine())
		{
			liveActiveState = animComp->runtimeState.GetCurrentStateName();
			isLiveTransitioning = animComp->runtimeState.IsTransitioning();
			liveTransitionProgress = animComp->runtimeState.GetTransitionProgress();
		}
	}

	// 1. 그리드 배경
	const float gridStep = 32.0f;
	const ImU32 gridColor = IM_COL32(200, 200, 200, 20);
	for (float x = std::fmod(m_CanvasScrolling.x, gridStep); x < canvasSize.x; x += gridStep)
		drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), gridColor);
	for (float y = std::fmod(m_CanvasScrolling.y, gridStep); y < canvasSize.y; y += gridStep)
		drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), gridColor);
	// 2. 트랜지션 연결선 렌더링
	DrawTransitions(drawList, canvasPos);
	// 3. Entry 노드 렌더링
	const ImVec2 nodeSize = ImVec2(130.0f, 45.0f);
	const ImVec2 entryPos = ImVec2(canvasPos.x + m_NodePositions["Entry"].x + m_CanvasScrolling.x, canvasPos.y + m_NodePositions["Entry"].y + m_CanvasScrolling.y);
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
			drawList->AddRect(ImVec2(screenPos.x - 2.0f, screenPos.y - 2.0f),
				ImVec2(screenPos.x + nodeSize.x + 2.0f, screenPos.y + nodeSize.y + 2.0f),
				IM_COL32(0, 255, 200, 255), 7.0f, 0, 3.5f);
		}
		else
		{
			const ImU32 borderCol = isSelected ? IM_COL32(80, 200, 255, 255) : IM_COL32(20, 20, 20, 255);
			drawList->AddRect(screenPos, ImVec2(screenPos.x + nodeSize.x, screenPos.y + nodeSize.y), borderCol, 6.0f, 0, isSelected ? 2.5f : 1.0f);
		}
		drawList->AddText(ImVec2(screenPos.x + 10.0f, screenPos.y + 8.0f), IM_COL32(255, 255, 255, 255), state.name.c_str());
		const std::string subText = state.IsBlendTree() ? "[BlendTree1D]" : (state.clip.Get() ? LOCAL("AnimatorGraph.Clip") : LOCAL("AnimatorGraph.NoMotion"));
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

void AnimatorGraphPanel::DrawTransitions(ImDrawList* drawList, const ImVec2& canvasPos)
{
	const ImVec2 nodeSize = ImVec2(130.0f, 45.0f);
	const auto screenMouse = Engine::InputManager::Get().GetScreenMousePosition();
	const ImVec2 mousePos(screenMouse.x, screenMouse.y);

	for (size_t i = 0; i < m_StateMachine->transitions.size(); ++i)
	{
		const auto& tr = m_StateMachine->transitions[i];
		if (!m_NodePositions.contains(tr.sourceState) || !m_NodePositions.contains(tr.targetState)) continue;

		const ImVec2 p1 = ImVec2(canvasPos.x + m_NodePositions[tr.sourceState].x + m_CanvasScrolling.x + nodeSize.x * 0.5f,
			canvasPos.y + m_NodePositions[tr.sourceState].y + m_CanvasScrolling.y + nodeSize.y * 0.5f);
		const ImVec2 p2 = ImVec2(canvasPos.x + m_NodePositions[tr.targetState].x + m_CanvasScrolling.x + nodeSize.x * 0.5f,
			canvasPos.y + m_NodePositions[tr.targetState].y + m_CanvasScrolling.y + nodeSize.y * 0.5f);

		const bool isSelected = (m_SelectedTransitionIndex == static_cast<int>(i));
		const ImU32 lineColor = isSelected ? IM_COL32(80, 200, 255, 255) : IM_COL32(200, 200, 200, 180);

		const ImVec2 cp1 = ImVec2(p1.x + (p2.x - p1.x) * 0.5f, p1.y);
		const ImVec2 cp2 = ImVec2(p1.x + (p2.x - p1.x) * 0.5f, p2.y);
		drawList->AddBezierCubic(p1, cp1, cp2, p2, lineColor, isSelected ? 3.0f : 2.0f);

		const ImVec2 mid = ImVec2((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);
		const ImVec2 markerSize = ImVec2(20.0f, 20.0f);
		const ImVec2 markerMin = ImVec2(mid.x - markerSize.x * 0.5f, mid.y - markerSize.y * 0.5f);
		const ImVec2 markerMax = ImVec2(mid.x + markerSize.x * 0.5f, mid.y + markerSize.y * 0.5f);

		drawList->AddRectFilled(markerMin, markerMax, isSelected ? IM_COL32(80, 200, 255, 255) : IM_COL32(80, 80, 90, 255), 4.0f);
		drawList->AddTriangleFilled(ImVec2(markerMin.x + 5.0f, markerMin.y + 4.0f),
			ImVec2(markerMin.x + 5.0f, markerMax.y - 4.0f),
			ImVec2(markerMax.x - 4.0f, mid.y), IM_COL32(255, 255, 255, 255));

		if (Engine::InputManager::Get().IsMouseButtonDown(Engine::EMouseButton::Left))
		{
			if (mousePos.x >= markerMin.x && mousePos.x <= markerMax.x &&
				mousePos.y >= markerMin.y && mousePos.y <= markerMax.y)
			{
				m_SelectedTransitionIndex = static_cast<int>(i);
				m_SelectedStateIndex = -1;
			}
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
			ACTION_MENU_ITEM((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(), "Animator.DeleteSelection");
		}
		else
		{
			if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("AnimatorGraph.CreateState")).c_str()))
			{
				const std::string name = "State_" + std::to_string(m_StateMachine->states.size());
				m_StateMachine->states.push_back(Engine::AnimState{ name });
				const auto screenMouse = Engine::InputManager::Get().GetScreenMousePosition();
				const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
				m_NodePositions[name] = ImVec2(screenMouse.x - canvasPos.x - m_CanvasScrolling.x, screenMouse.y - canvasPos.y - m_CanvasScrolling.y);
				m_IsDirty = true;
			}
		}
		ImGui::EndPopup();
	}
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
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.NodeInspector"));
	ImGui::Separator();

	char nameBuf[128] = {};
	std::strncpy(nameBuf, state.name.c_str(), sizeof(nameBuf) - 1);
	if (ImGui::InputText(LOCAL_LABEL("EditorCommon.Name").c_str(), nameBuf, sizeof(nameBuf)))
	{
		const std::string oldName = state.name;
		state.name = nameBuf;
		if (m_StateMachine->defaultState == oldName) m_StateMachine->defaultState = state.name;
		if (m_NodePositions.contains(oldName))
		{
			m_NodePositions[state.name] = m_NodePositions[oldName];
			m_NodePositions.erase(oldName);
		}
		for (auto& tr : m_StateMachine->transitions)
		{
			if (tr.sourceState == oldName) tr.sourceState = state.name;
			if (tr.targetState == oldName) tr.targetState = state.name;
		}
		m_IsDirty = true;
	}

	if (ImGui::DragFloat(LOCAL_LABEL("PROP_PLAY_RATE").c_str(), &state.playRate, 0.05f, 0.0f, 10.0f)) m_IsDirty = true;
	if (ImGui::Checkbox(LOCAL_LABEL("PROP_IS_LOOPING").c_str(), &state.isLooping)) m_IsDirty = true;
	if (state.isLooping)
	{
		if (ImGui::SliderFloat(LOCAL_LABEL("PROP_LOOP_BLEND_DURATION").c_str(), &state.loopBlendDuration, 0.0f, 0.5f, "%.2fs")) m_IsDirty = true;
	}

	const bool isDefault = (state.name == m_StateMachine->defaultState);
	bool defaultToggle = isDefault;
	if (ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.SetDefaultState").c_str(), &defaultToggle) && defaultToggle)
	{
		m_StateMachine->defaultState = state.name;
		m_IsDirty = true;
	}

	ImGui::Spacing();
	ImGui::Separator();

	if (ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.UseBlendTree").c_str(), &state.isBlendTree))
	{
		m_IsDirty = true;
	}

	if (!state.IsBlendTree())
	{
		ImGui::Text("%s", LOCAL_CSTR("PROP_ANIMATION_CLIP"));
		ImGui::PushID("StateMotionClip");
		if (Editor::DrawInspectorResourcePicker(state.clip, true)) m_IsDirty = true;
		ImGui::PopID();
	}
	else
	{
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.BlendTree1D"));
		ImGui::Spacing();

		if (ImGui::BeginCombo(LOCAL_LABEL("AnimatorGraph.Parameter").c_str(), state.blendTree.parameterName.c_str()))
		{
			for (const auto& [paramName, paramData] : m_StateMachine->defaultParameters)
			{
				if (paramData.type != Engine::EAnimatorParamType::Float) continue;
				const bool isSelected = (state.blendTree.parameterName == paramName);
				if (ImGui::Selectable(paramName.c_str(), isSelected))
				{
					state.blendTree.parameterName = paramName;
					m_IsDirty = true;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		ImGui::Text("%s (%d)", LOCAL_CSTR("AnimatorGraph.Samples"), static_cast<int>(state.blendTree.samples.size()));
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);
		if (ImGui::Button(ICON_FA_PLUS "##AddBlendSample"))
		{
			const float nextThreshold = state.blendTree.samples.empty()
				? 0.0f : state.blendTree.samples.back().threshold + 1.0f;
			state.blendTree.AddSample(nextThreshold, {});
			m_IsDirty = true;
		}

		int sampleToRemove = -1;
		bool needSort = false;
		for (size_t i = 0; i < state.blendTree.samples.size(); ++i)
		{
			auto& sample = state.blendTree.samples[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::SetNextItemWidth(75.0f);
			if (ImGui::DragFloat("##SampleThreshold", &sample.threshold, 0.05f))
			{
				needSort = true;
				m_IsDirty = true;
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(140.0f);
			if (Editor::DrawInspectorResourcePicker(sample.clip, true)) m_IsDirty = true;
			ImGui::SameLine();
			if (ImGui::SmallButton(ICON_FA_TRASH "##DelSample")) sampleToRemove = static_cast<int>(i);
			ImGui::PopID();
		}
		if (sampleToRemove >= 0 && sampleToRemove < static_cast<int>(state.blendTree.samples.size()))
		{
			state.blendTree.samples.erase(state.blendTree.samples.begin() + sampleToRemove);
			m_IsDirty = true;
		}
		if (needSort) state.blendTree.SortSamples();
	}
	// 애니메이션 노티파이 (Notifies) 저작 섹션
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s (%d)", LOCAL_CSTR("AnimatorGraph.Notifies"), static_cast<int>(state.notifies.size()));
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);
	if (ImGui::Button(ICON_FA_PLUS "##AddNotify"))
	{
		state.notifies.push_back(Engine::AnimNotify{ "NewNotify", 0.0f, "" });
		m_IsDirty = true;
	}
	int notifyToRemove = -1;
	for (size_t i = 0; i < state.notifies.size(); ++i)
	{
		auto& notify = state.notifies[i];
		ImGui::PushID(static_cast<int>(i));
		// 발동 시간 (초)
		ImGui::SetNextItemWidth(65.0f);
		if (ImGui::DragFloat("##NotifyTime", &notify.triggerTime, 0.02f, 0.0f, 60.0f, "%.2fs")) m_IsDirty = true;
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("AnimatorGraph.NotifyTime"));
		// 노티파이 식별 이름
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100.0f);
		char nameBuf[64] = {};
		std::strncpy(nameBuf, notify.name.c_str(), sizeof(nameBuf) - 1);
		if (ImGui::InputText("##NotifyName", nameBuf, sizeof(nameBuf)))
		{
			notify.name = nameBuf;
			m_IsDirty = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("AnimatorGraph.NotifyName"));
		// 페이로드 (추가 데이터)
		ImGui::SameLine();
		ImGui::SetNextItemWidth(90.0f);
		char payloadBuf[128] = {};
		std::strncpy(payloadBuf, notify.payload.c_str(), sizeof(payloadBuf) - 1);
		if (ImGui::InputText("##NotifyPayload", payloadBuf, sizeof(payloadBuf)))
		{
			notify.payload = payloadBuf;
			m_IsDirty = true;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("AnimatorGraph.NotifyPayload"));
		// 삭제 버튼
		ImGui::SameLine();
		if (ImGui::SmallButton(ICON_FA_TRASH "##DelNotify")) notifyToRemove = static_cast<int>(i);
		ImGui::PopID();
	}
	if (notifyToRemove >= 0 && notifyToRemove < static_cast<int>(state.notifies.size()))
	{
		state.notifies.erase(state.notifies.begin() + notifyToRemove);
		m_IsDirty = true;
	}
}

void AnimatorGraphPanel::DrawTransitionInspector(Engine::AnimTransition& tr)
{
	ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.TransitionInspector"));
	ImGui::Text("%s -> %s", tr.sourceState.c_str(), tr.targetState.c_str());
	ImGui::Separator();

	if (ImGui::DragFloat(LOCAL_LABEL("AnimatorGraph.Duration").c_str(), &tr.duration, 0.02f, 0.0f, 5.0f, "%.2fs")) m_IsDirty = true;
	if (ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.HasExitTime").c_str(), &tr.hasExitTime)) m_IsDirty = true;
	if (tr.hasExitTime)
	{
		if (ImGui::SliderFloat(LOCAL_LABEL("AnimatorGraph.ExitTime").c_str(), &tr.exitTime, 0.0f, 1.0f, "%.2f")) m_IsDirty = true;
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.Conditions"));
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);

	if (ImGui::Button(ICON_FA_PLUS "##AddCond"))
	{
		Engine::TransitionCondition cond;
		if (!m_StateMachine->defaultParameters.empty())
			cond.parameterName = m_StateMachine->defaultParameters.begin()->first;
		tr.conditions.push_back(cond);
		m_IsDirty = true;
	}

	int toRemove = -1;
	for (size_t i = 0; i < tr.conditions.size(); ++i)
	{
		auto& cond = tr.conditions[i];
		ImGui::PushID(static_cast<int>(i));

		if (ImGui::BeginCombo("##ParamCombo", cond.parameterName.c_str()))
		{
			for (const auto& [name, _] : m_StateMachine->defaultParameters)
			{
				const bool isSelected = (cond.parameterName == name);
				if (ImGui::Selectable(name.c_str(), isSelected))
				{
					cond.parameterName = name;
					m_IsDirty = true;
				}
			}
			ImGui::EndCombo();
		}

		const std::string modes[] = {
			LOCAL("AnimatorGraph.CondGreater"),
			LOCAL("AnimatorGraph.CondLess"),
			LOCAL("AnimatorGraph.CondEqual"),
			LOCAL("AnimatorGraph.CondNotEqual"),
			LOCAL("AnimatorGraph.CondIfTrue"),
			LOCAL("AnimatorGraph.CondIfFalse")
		};
		int currentMode = static_cast<int>(cond.mode);
		ImGui::SetNextItemWidth(125.0f);
		if (ImGui::BeginCombo("##ModeCombo", modes[currentMode].c_str()))
		{
			for (int m = 0; m < 6; ++m)
			{
				const bool isSelected = (currentMode == m);
				if (ImGui::Selectable(modes[m].c_str(), isSelected))
				{
					cond.mode = static_cast<Engine::EConditionMode>(m);
					m_IsDirty = true;
				}
			}
			ImGui::EndCombo();
		}

		if (cond.mode != Engine::EConditionMode::IfTrue && cond.mode != Engine::EConditionMode::IfFalse)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(65.0f);
			if (ImGui::DragFloat("##Threshold", &cond.threshold, 0.1f)) m_IsDirty = true;
		}

		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_TRASH)) toRemove = static_cast<int>(i);

		ImGui::PopID();
	}
	if (toRemove >= 0 && toRemove < static_cast<int>(tr.conditions.size()))
	{
		tr.conditions.erase(tr.conditions.begin() + toRemove);
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
	m_NodePositions["Entry"] = ImVec2(50.0f, 150.0f);
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

		if (ImGui::Button(LOCAL_CSTR("EditorDocument.Save"), ImVec2(100.0f, 0.0f)))
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

		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button(LOCAL_CSTR("EditorDocument.Discard"), ImVec2(100.0f, 0.0f)))
		{
			ExecutePendingAction();
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button(LOCAL_CSTR("EditorCommon.Cancel"), ImVec2(100.0f, 0.0f)))
		{
			m_PendingAction = EPendingAction::None;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void AnimatorGraphPanel::InitializePreview()
{
	// 1. 공통 3D 프리뷰 위젯 초기화 (바닥 그리드 On)
	m_PreviewWidget.Initialize(L"AnimGraphPreview", 260, 240, true);

	// 2. 기본 프리뷰 캐릭터 모델(Fox) 및 스키닝 머티리얼 디스크 로드
	auto& rm = Engine::ResourceManager::Get();
	auto modelHandle = Engine::ResourceHandle<Engine::Model>(rm.LoadFile(L"Resources/Model/Fox.bammodel"));
	if (modelHandle.IsValid() && modelHandle.Get() && m_PreviewWidget.GetScene())
	{
		const wchar_t* defaultMatPath = modelHandle->GetSkeleton().IsValid()
			? L"Resources/Material/SkinningMaterial.bammat.json"
			: L"Resources/Material/GBufferMaterial.bammat.json";
		auto matHandle = Engine::ResourceHandle<Engine::MaterialInterface>(rm.LoadFile(defaultMatPath));
		// 프리뷰 씬에 캐릭터 모델 인스턴스화
		const Engine::EResult result = m_PreviewWidget.GetScene()->InstantiateModel(*modelHandle.Get(), matHandle, m_PreviewModelEntity);
		if (result != Engine::EResult::Success)
		{
			EDITOR_LOG_ERROR("Failed to instantiate Fox preview model: {}", ResultToString(result));
		}
	}

	// 3. 카메라 초기 시점 (캐릭터 중심 쿼터뷰)
	m_PreviewWidget.FocusTarget(Engine::vec3(0.0f, 0.4f, 0.0f), 2.2f);
	SyncPreviewStateMachine();
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
	if (!m_ShowPreviewViewport) return;

	// 파라미터 변경 사항을 프리뷰 런타임 상태 머신에 실시간 동기화
	if (m_PreviewModelEntity && m_StateMachine)
	{
		auto* animator = m_PreviewWidget.GetRegistry().try_get<Engine::AnimatorComponent>(m_PreviewModelEntity->GetEntityHandle());
		if (animator && animator->runtimeState.GetStateMachine() != m_StateMachine)
		{
			animator->runtimeState.BindStateMachine(m_StateMachine);
		}
	}

	m_PreviewWidget.BeginFrame(dt);
	m_PreviewWidget.Submit(dt);
}

void AnimatorGraphPanel::DrawPreviewSection(float width, float height)
{
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