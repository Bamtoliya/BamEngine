#include "AnimatorGraphPanel.h"
#include "IconsFontAwesome7.h"
#include "FileDialogs.h"
#include "SceneManager.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Core/NameComponent.h"
#include "SelectionManager.h"
#include "Entity.h"

AnimatorGraphPanel::AnimatorGraphPanel()
{
	m_Name = L"AnimatorGraphPanel";
	m_DisplayNameKey = "AnimatorGraph.Title";
	m_Open = false;

	m_StateMachine = std::make_shared<Engine::AnimatorStateMachine>();
	m_StateMachine->defaultState = "Idle";
	m_StateMachine->states.push_back(Engine::AnimState{ "Idle", {}, nullptr, 1.0f, true });
	m_StateMachine->states.push_back(Engine::AnimState{ "Walk", {}, nullptr, 1.0f, true });

	Engine::AnimTransition tr;
	tr.sourceState = "Idle";
	tr.targetState = "Walk";
	tr.duration = 0.25f;
	m_StateMachine->transitions.push_back(tr);

	m_NodePositions["Entry"] = ImVec2(50.0f, 150.0f);
	m_NodePositions["Idle"] = ImVec2(220.0f, 150.0f);
	m_NodePositions["Walk"] = ImVec2(420.0f, 150.0f);
}

void AnimatorGraphPanel::SetStateMachine(const std::shared_ptr<Engine::AnimatorStateMachine>& sm)
{
	m_StateMachine = sm ? sm : std::make_shared<Engine::AnimatorStateMachine>();
	m_SelectedStateIndex = -1;
	m_SelectedTransitionIndex = -1;
	m_ConnectingSourceIndex = -1;
}

void AnimatorGraphPanel::Draw()
{
	if (!m_Open) return;

	ImGui::SetNextWindowSize(ImVec2(1000.0f, 600.0f), ImGuiCond_FirstUseEver);
	const std::string title = ICON_FA_DIAGRAM_PROJECT " " + GetDisplayName() + "###AnimatorGraphPanel";

	if (ImGui::Begin(title.c_str(), &m_Open, ImGuiWindowFlags_MenuBar))
	{
		// 1. 상단 메뉴바 (File, Edit, View)
		DrawMenuBar();

		// 2. 메뉴바 바로 아래 1줄 퀵 액세스 툴바
		DrawToolbar();
		ImGui::Separator();

		const float totalWidth = ImGui::GetContentRegionAvail().x;
		const float paramWidth = 220.0f;
		const float inspectorWidth = 280.0f;
		const float canvasWidth = std::max(200.0f, totalWidth - paramWidth - inspectorWidth - 16.0f);
		const float contentHeight = ImGui::GetContentRegionAvail().y;

		// 3. 3분할 작업 공간
		ImGui::BeginChild("ParamsPanel", ImVec2(paramWidth, contentHeight), true);
		DrawParametersTab();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("CanvasPanel", ImVec2(canvasWidth, contentHeight), true,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		DrawCanvas();
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("InspectorPanel", ImVec2(inspectorWidth, contentHeight), true);
		DrawSelectionInspector();
		ImGui::EndChild();
	}
	ImGui::End();
}

void AnimatorGraphPanel::DrawMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(LOCAL_CSTR("EditorCommon.File")))
		{
			if (ImGui::MenuItem((ICON_FA_FILE " " + LOCAL_LABEL("EditorCommon.New")).c_str(), "Ctrl+N"))
			{
				m_StateMachine = std::make_shared<Engine::AnimatorStateMachine>();
				m_StateMachine->defaultState = "Idle";
				m_StateMachine->states.push_back(Engine::AnimState{ "Idle", {}, nullptr, 1.0f, true });
				m_CurrentFilePath.clear();
				m_SelectedStateIndex = -1;
				m_SelectedTransitionIndex = -1;
				m_IsDirty = false;
			}
			if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.Open")).c_str(), "Ctrl+O"))
				OpenDocument();
			if (ImGui::MenuItem((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.Save")).c_str(), "Ctrl+S"))
				SaveDocument(false);
			if (ImGui::MenuItem((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.SaveAs")).c_str(), "Ctrl+Shift+S"))
				SaveDocument(true);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(LOCAL_CSTR("EditorCommon.Edit")))
		{
			if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(), "Delete", false, m_SelectedStateIndex >= 0 || m_SelectedTransitionIndex >= 0))
			{
				if (m_SelectedStateIndex >= 0 && m_SelectedStateIndex < static_cast<int>(m_StateMachine->states.size()))
				{
					const std::string delName = m_StateMachine->states[m_SelectedStateIndex].name;
					m_StateMachine->states.erase(m_StateMachine->states.begin() + m_SelectedStateIndex);
					std::erase_if(m_StateMachine->transitions, [&](const Engine::AnimTransition& tr) {
						return tr.sourceState == delName || tr.targetState == delName;
						});
					m_SelectedStateIndex = -1;
					m_IsDirty = true;
				}
				else if (m_SelectedTransitionIndex >= 0 && m_SelectedTransitionIndex < static_cast<int>(m_StateMachine->transitions.size()))
				{
					m_StateMachine->transitions.erase(m_StateMachine->transitions.begin() + m_SelectedTransitionIndex);
					m_SelectedTransitionIndex = -1;
					m_IsDirty = true;
				}
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(LOCAL_CSTR("UI_VIEW")))
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
	if (ImGui::Button(ICON_FA_FILE "##QuickNew"))
	{
		m_StateMachine = std::make_shared<Engine::AnimatorStateMachine>();
		m_CurrentFilePath.clear();
		m_IsDirty = false;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.New"));

	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_FOLDER_OPEN "##QuickOpen")) OpenDocument();
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

	if (ImGui::Button((ICON_FA_PLUS " " + LOCAL_LABEL("AnimatorGraph.CreateState")).c_str()))
	{
		const std::string name = "State_" + std::to_string(m_StateMachine->states.size());
		m_StateMachine->states.push_back(Engine::AnimState{ name, {}, nullptr, 1.0f, true });
		m_NodePositions[name] = ImVec2(250.0f, 150.0f);
		m_IsDirty = true;
	}

	// 트랜지션 연결 중 안내 문구 및 취소 버튼
	if (m_ConnectingSourceIndex >= 0)
	{
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", LOCAL_CSTR("AnimatorGraph.SelectTargetToConnect"));
		ImGui::SameLine();
		if (ImGui::SmallButton("Cancel (ESC)")) m_ConnectingSourceIndex = -1;
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) m_ConnectingSourceIndex = -1;
	}
}

void AnimatorGraphPanel::OpenDocument()
{
	std::wstring selectedPath;
	const std::vector<FileDialogFilter> filters{
		{ L"Animator Controller Files (*.animator)", L"*.animator" },
		{ L"JSON Files (*.json)", L"*.json" }
	};
	if (FileDialogs::OpenFileDialog(selectedPath, filters))
	{
		m_CurrentFilePath = WStrToStr(selectedPath);
		m_StateMachine->LoadFromFile(m_CurrentFilePath);
		m_SelectedStateIndex = -1;
		m_SelectedTransitionIndex = -1;
		m_ConnectingSourceIndex = -1;
		m_IsDirty = false;
	}
}

void AnimatorGraphPanel::SaveDocument(bool saveAs)
{
	if (saveAs || m_CurrentFilePath.empty())
	{
		std::wstring selectedPath;
		const std::vector<FileDialogFilter> filters{
			{ L"Animator Controller Files (*.animator)", L"*.animator" }
		};
		if (FileDialogs::SaveFileDialog(selectedPath, filters, L"NewController", L"animator"))
			m_CurrentFilePath = WStrToStr(selectedPath);
		else return;
	}
	if (!m_CurrentFilePath.empty())
	{
		m_StateMachine->SaveToFile(m_CurrentFilePath);
		m_IsDirty = false;
	}
}

void AnimatorGraphPanel::DrawParametersTab()
{
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Animator.Parameters"));
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);

	if (ImGui::Button(ICON_FA_PLUS)) ImGui::OpenPopup("AddParamPopup");

	if (ImGui::BeginPopup("AddParamPopup"))
	{
		if (ImGui::MenuItem("Float"))
		{
			const std::string name = "NewFloat" + std::to_string(m_StateMachine->defaultParameters.size());
			m_StateMachine->defaultParameters[name] = Engine::AnimatorParameter{ Engine::EAnimatorParamType::Float, 0.0f, 0, false };
			m_IsDirty = true;
		}
		if (ImGui::MenuItem("Int"))
		{
			const std::string name = "NewInt" + std::to_string(m_StateMachine->defaultParameters.size());
			m_StateMachine->defaultParameters[name] = Engine::AnimatorParameter{ Engine::EAnimatorParamType::Int, 0.0f, 0, false };
			m_IsDirty = true;
		}
		if (ImGui::MenuItem("Bool"))
		{
			const std::string name = "NewBool" + std::to_string(m_StateMachine->defaultParameters.size());
			m_StateMachine->defaultParameters[name] = Engine::AnimatorParameter{ Engine::EAnimatorParamType::Bool, 0.0f, 0, false };
			m_IsDirty = true;
		}
		if (ImGui::MenuItem("Trigger"))
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
			if (ImGui::Checkbox(paramData.type == Engine::EAnimatorParamType::Trigger ? "Trigger" : "Bool", &paramData.boolValue)) m_IsDirty = true;
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
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	const ImVec2 mousePos = ImGui::GetMousePos();
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
		const entt::entity handle = selectedEntity->GetEntityHandle();
		if (handle != entt::null && reg.valid(handle))
		{
			auto* animComp = reg.try_get<Engine::AnimatorComponent>(handle);
			if (animComp && animComp->runtimeState.HasStateMachine())
			{
				liveActiveState = animComp->runtimeState.GetCurrentStateName();
				isLiveTransitioning = animComp->runtimeState.IsTransitioning();
				liveTransitionProgress = animComp->runtimeState.GetTransitionProgress();
			}
		}
	}

	// 1. 그리드 배경
	const float gridStep = 32.0f;
	const ImU32 gridColor = IM_COL32(200, 200, 200, 20);
	for (float x = std::fmod(m_CanvasScrolling.x, gridStep); x < canvasSize.x; x += gridStep)
		drawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y), ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), gridColor);
	for (float y = std::fmod(m_CanvasScrolling.y, gridStep); y < canvasSize.y; y += gridStep)
		drawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), gridColor);

	// 2. 트랜지션 연결선 렌더링 (실시간 진행률 전달)
	DrawTransitions(drawList, canvasPos);

	// 3. Entry 노드 렌더링
	const ImVec2 nodeSize = ImVec2(130.0f, 45.0f);
	const ImVec2 entryPos = ImVec2(canvasPos.x + m_NodePositions["Entry"].x + m_CanvasScrolling.x, canvasPos.y + m_NodePositions["Entry"].y + m_CanvasScrolling.y);
	drawList->AddRectFilled(entryPos, ImVec2(entryPos.x + nodeSize.x, entryPos.y + nodeSize.y), IM_COL32(30, 130, 60, 255), 5.0f);
	drawList->AddText(ImVec2(entryPos.x + 15.0f, entryPos.y + 13.0f), IM_COL32(255, 255, 255, 255), "ENTRY ->");

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

		// 마우스 드래그 이동
		const bool isHovered = (mousePos.x >= screenPos.x && mousePos.x <= screenPos.x + nodeSize.x &&
			mousePos.y >= screenPos.y && mousePos.y <= screenPos.y + nodeSize.y);

		if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
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
			if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			{
				const ImVec2 delta = ImGui::GetIO().MouseDelta;
				nodePosRef.x += delta.x;
				nodePosRef.y += delta.y;
				m_IsDirty = true;
			}
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) m_DraggingStateIndex = -1;
		}

		// 노드 배경
		const ImU32 bgCol = isDefault ? IM_COL32(180, 100, 30, 255) : IM_COL32(50, 60, 75, 255);
		drawList->AddRectFilled(screenPos, ImVec2(screenPos.x + nodeSize.x, screenPos.y + nodeSize.y), bgCol, 6.0f);

		// 실시간 런타임 활성 노드 발광 강조 (Cyan 네온 테두리)
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
		const std::string subText = state.IsBlendTree() ? "[BlendTree1D]" : (state.clip.Get() ? "Clip" : "(No Motion)");
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
	const ImVec2 mousePos = ImGui::GetMousePos();

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

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
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
			if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str()))
			{
				const std::string deletedName = m_StateMachine->states[m_SelectedStateIndex].name;
				m_StateMachine->states.erase(m_StateMachine->states.begin() + m_SelectedStateIndex);
				std::erase_if(m_StateMachine->transitions, [&](const Engine::AnimTransition& tr) {
					return tr.sourceState == deletedName || tr.targetState == deletedName;
					});
				m_SelectedStateIndex = -1;
				m_IsDirty = true;
			}
		}
		else
		{
			if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("AnimatorGraph.CreateState")).c_str()))
			{
				const std::string name = "State_" + std::to_string(m_StateMachine->states.size());
				m_StateMachine->states.push_back(Engine::AnimState{ name, {}, nullptr, 1.0f, true });
				const ImVec2 mousePos = ImGui::GetMousePos();
				const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
				m_NodePositions[name] = ImVec2(mousePos.x - canvasPos.x - m_CanvasScrolling.x, mousePos.y - canvasPos.y - m_CanvasScrolling.y);
				m_IsDirty = true;
			}
		}
		ImGui::EndPopup();
	}
}

void AnimatorGraphPanel::DrawSelectionInspector()
{
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
		state.name = nameBuf;
		m_IsDirty = true;
	}

	if (ImGui::DragFloat(LOCAL_LABEL("PROP_PLAY_RATE").c_str(), &state.playRate, 0.05f, 0.0f, 10.0f)) m_IsDirty = true;
	if (ImGui::Checkbox(LOCAL_LABEL("PROP_IS_LOOPING").c_str(), &state.isLooping)) m_IsDirty = true;

	const bool isDefault = (state.name == m_StateMachine->defaultState);
	if (ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.SetDefaultState").c_str(), const_cast<bool*>(&isDefault)))
	{
		m_StateMachine->defaultState = state.name;
		m_IsDirty = true;
	}

	ImGui::Spacing();
	ImGui::Separator();

	bool isBlendTree = state.IsBlendTree();
	if (ImGui::Checkbox(LOCAL_LABEL("AnimatorGraph.UseBlendTree").c_str(), &isBlendTree))
	{
		if (isBlendTree && !state.blendTree) state.blendTree = std::make_shared<Engine::BlendTree1D>();
		else if (!isBlendTree) state.blendTree = nullptr;
		m_IsDirty = true;
	}

	if (state.IsBlendTree())
	{
		char paramBuf[64] = {};
		std::strncpy(paramBuf, state.blendTree->parameterName.c_str(), sizeof(paramBuf) - 1);
		if (ImGui::InputText(LOCAL_LABEL("AnimatorGraph.Parameter").c_str(), paramBuf, sizeof(paramBuf)))
		{
			state.blendTree->parameterName = paramBuf;
			m_IsDirty = true;
		}

		ImGui::Text("%s: %d", LOCAL_CSTR("AnimatorGraph.Samples"), static_cast<int>(state.blendTree->samples.size()));
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

		const char* modes[] = { "Greater (>)", "Less (<)", "Equal (==)", "NotEqual (!=)", "IfTrue", "IfFalse" };
		int currentMode = static_cast<int>(cond.mode);
		ImGui::SetNextItemWidth(110.0f);
		if (ImGui::Combo("##ModeCombo", &currentMode, modes, IM_ARRAYSIZE(modes)))
		{
			cond.mode = static_cast<Engine::EConditionMode>(currentMode);
			m_IsDirty = true;
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