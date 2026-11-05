#include "GraphPanel.h"
#include "ShortcutManager.h"
#include "GraphMessageLocalization.h"
#include "InputManager.h"
#include "LocalizationManager.h"
#include <imgui_node_editor.h>
namespace ed = ax::NodeEditor;

namespace
{
	struct RestoreEditorContext
	{
		ed::EditorContext* Previous = ed::GetCurrentEditor();
		~RestoreEditorContext() { ed::SetCurrentEditor(Previous); }
	};
}

#pragma region Context
void GraphPanel::ActivateDocument()
{
	auto& document = GetDocument();
	if (!document.Context)
	{
		ed::Config config;
		config.SettingsName = (m_Domain == Engine::EGraphDomain::Logic) ? "LogicGraph" : "ShaderGraph";
		config.NavigateButtonIndex = 2; // ★ 2 = 마우스 가운데 버튼(휠 클릭)으로 캔버스 화면 이동!
		document.Context = ed::CreateEditor(&config);

		// ── 언리얼 블루프린트 글로벌 테마 설정 ──
		ed::SetCurrentEditor(document.Context);
		auto& style = ed::GetStyle();
		style.NodeRounding = 6.0f;
		style.NodeBorderWidth = 1.2f;
		style.NodePadding = ImVec4(10.0f, 6.0f, 10.0f, 8.0f);
		style.Colors[ed::StyleColor_NodeBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.95f);
		style.Colors[ed::StyleColor_NodeBorder] = ImVec4(0.24f, 0.25f, 0.28f, 1.0f);
		style.Colors[ed::StyleColor_HovNodeBorder] = ImVec4(0.95f, 0.70f, 0.20f, 1.0f);
		style.Colors[ed::StyleColor_SelNodeBorder] = ImVec4(1.0f, 0.85f, 0.30f, 1.0f);
		style.Colors[ed::StyleColor_Flow] = ImVec4(1.0f, 0.95f, 0.35f, 1.0f);
		style.Colors[ed::StyleColor_FlowMarker] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		style.FlowSpeed = 160.0f;
		style.FlowMarkerDistance = 28.0f;
	}
	ed::SetCurrentEditor(document.Context);
	// Keyboard commands are owned by ShortcutManager, including the library's default Delete and F.
	ed::EnableShortcuts(false);
	ed::GetStyle().LinkStrength = (m_WireStyle == EWireStyle::Straight) ? 0.0f : 100.0f;
}

void GraphPanel::OpenAddNodeMenu()
{
	if (!CanUseCanvas()) return;
	const RestoreEditorContext restore;
	ActivateDocument();
	const auto mouse = Engine::InputManager::Get().GetScreenMousePosition();
	m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
	m_AddSource = {};
	PrepareNodeMenu();
	m_OpenNodePopup = true;
}

void GraphPanel::ResetCanvasView()
{
	if (!CanUseCanvas()) return;
	const RestoreEditorContext restore;
	ActivateDocument();
	ed::NavigateToContent(0.5f);
}

void GraphPanel::AlignNodes(char dir)
{
	if (!CanUseCanvas()) return;
	const RestoreEditorContext restore;
	ActivateDocument();
	AlignSelectedNodes(dir);
}

void GraphPanel::DuplicateSelectedNodes()
{
	if (!CanUseCanvas()) return;
	const RestoreEditorContext restore;
	ActivateDocument();
	DuplicateSelection();
}

void GraphPanel::CreateCommentBox()
{
	if (!CanUseCanvas()) return;
	const RestoreEditorContext restore;
	ActivateDocument();
	CreateCommentBoxFromSelection();
}

void GraphPanel::DeleteSelectedElements()
{
	if (!CanUseCanvas()) return;
	const RestoreEditorContext restore;
	ActivateDocument();
	DeleteSelection();
}
void GraphPanel::SuspendDocument()
{
	m_Executor.Pause();
	m_DragPin = 0;
	m_ContextClickStartedOnCanvas = false;
	m_OpenNodePopup = false;
	m_NodeMenuActive = false;
	m_AddSource = {};
	ed::SetCurrentEditor(nullptr);
}
#pragma endregion

#pragma region DrawUI
void GraphPanel::DrawCanvas(float height)
{
	ActivateDocument();
	auto& document = GetDocument();
	ed::Begin("##GraphCanvas", ImVec2(0.0f, height));
	// 1. 코멘트 노드를 먼저 그려서 배경 레이어로 배치 (일반 노드 클릭 방해 차단)
	for (const auto& node : document.Data.GetNodes())
	{
		if (node.Type == "Core.Comment") DrawNode(node);
	}
	// 2. 일반 노드를 전면에 배치
	for (const auto& node : document.Data.GetNodes())
	{
		if (node.Type != "Core.Comment") DrawNode(node);
	}
	DrawLinks();
	HandleConnections(ImVec2(0, 0), true);
	ReadSelection();
	ed::End();

	// ★ 와이어(링크) 좌클릭 더블클릭 시 리루트 노드 즉시 자동 삽입!
	// ── DrawCanvas() 내부의 와이어 더블클릭 감지 부분 교체 ──
	auto& input = Engine::InputManager::Get();
	const bool canvasHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);

	if (input.IsMouseButtonDown(Engine::EMouseButton::Right))
		m_ContextClickStartedOnCanvas = canvasHovered;

	const bool openContextMenu = canvasHovered && m_ContextClickStartedOnCanvas &&
		input.IsMouseButtonUp(Engine::EMouseButton::Right);

	if (!input.IsMouseButtonPressed(Engine::EMouseButton::Right))
		m_ContextClickStartedOnCanvas = false;

	const ed::LinkId hoveredLinkId = ed::GetHoveredLink();
	if (canvasHovered && hoveredLinkId && input.IsAltPressed() && input.IsMouseButtonDown(Engine::EMouseButton::Left))
	{
		// Preserve the pointer gesture while the library's keyboard shortcuts are disabled.
		ed::DeleteLink(hoveredLinkId);
	}
	else if (canvasHovered && hoveredLinkId && input.IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
	{
		const auto mouse = input.GetScreenMousePosition();
		const ImVec2 canvasPos = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
		InsertRerouteNodeOnLink(static_cast<uintptr_t>(hoveredLinkId.Get()), canvasPos);
	}

	// ── 우클릭 컨텍스트 메뉴 감지 (노드 / 와이어 / 빈 캔버스 분기) ──
	const ed::NodeId hoveredNodeId = ed::GetHoveredNode();
	const ed::PinId hoveredPinId = ed::GetHoveredPin();
	if (openContextMenu)
	{
		const auto mouse = InputManager::Get().GetScreenMousePosition();
		const ImVec2 canvasPos = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
		m_ContextPosition = canvasPos;
		if (hoveredPinId)
		{
			m_ContextPinId = static_cast<uintptr_t>(hoveredPinId.Get());
			ImGui::OpenPopup("##GraphPinMenu");
		}
		else if (hoveredNodeId)
		{
			m_ContextNodeId = static_cast<uintptr_t>(hoveredNodeId.Get());
			for (const auto& [nodeId, view] : document.Nodes)
			{
				if (view.Id == static_cast<int>(m_ContextNodeId))
				{
					if (std::find(document.SelectedNodes.begin(), document.SelectedNodes.end(), nodeId) == document.SelectedNodes.end())
						document.SelectedNodes = { nodeId };
					break;
				}
			}
			ImGui::OpenPopup("##GraphNodeMenu");
		}
		else if (hoveredLinkId)
		{
			m_ContextLinkId = static_cast<uintptr_t>(hoveredLinkId.Get());
			ImGui::OpenPopup("##GraphLinkMenu");
		}
		else
		{
			m_AddPosition = canvasPos;
			PrepareNodeMenu();
			ImGui::OpenPopup("##GraphCanvasMenu");
		}
	}
	if (m_OpenNodePopup)
	{
		m_OpenNodePopup = false;
		ImGui::OpenPopup("##GraphCanvasMenu");
	}
	// ── 1. 빈 캔버스 우클릭 메뉴 ──
	if (ImGui::BeginPopup("##GraphCanvasMenu"))
	{
		DrawNodeMenu();
		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.CreateCommentBox").c_str(), ShortcutManager::Get().GetShortcutHint("Graph.CreateComment"))) CreateCommentBoxFromSelection();
		const bool selected = !document.SelectedNodes.empty() || !document.SelectedLinks.empty();
		if (ImGui::MenuItem(LOCAL_CSTR("GraphPanel.Delete"), ShortcutManager::Get().GetShortcutHint("Graph.Delete"), false, selected)) DeleteSelection();
		ImGui::EndPopup();
	}
	// ── 2. 와이어(링크) 전용 컨텍스트 메뉴 ──
	if (ImGui::BeginPopup("##GraphLinkMenu"))
	{
		Engine::GraphLinkId targetLinkId = 0;
		for (const auto& [id, edId] : document.Links)
		{
			if (edId == static_cast<int>(m_ContextLinkId)) { targetLinkId = id; break; }
		}
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.AddReroute").c_str()))
		{
			InsertRerouteNodeOnLink(m_ContextLinkId, m_ContextPosition);
			ImGui::CloseCurrentPopup();
		}
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.BreakLink").c_str(), nullptr, false, targetLinkId != 0))
		{
			if (ApplyResult(document.Data.Disconnect(targetLinkId))) document.Links.erase(targetLinkId);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	// ── 3. 노드 전용 컨텍스트 메뉴 ──
	if (ImGui::BeginPopup("##GraphNodeMenu"))
	{
		Engine::GraphNodeId targetNodeId = 0;
		for (const auto& [nodeId, view] : document.Nodes)
		{
			if (view.Id == static_cast<int>(m_ContextNodeId)) { targetNodeId = nodeId; break; }
		}
		if (ImGui::MenuItem(LOCAL_LABEL("Hierarchy.Duplicate").c_str(), ShortcutManager::Get().GetShortcutHint("Graph.Duplicate"))) DuplicateSelection();
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.CreateCommentBox").c_str(), ShortcutManager::Get().GetShortcutHint("Graph.CreateComment"))) CreateCommentBoxFromSelection();
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.BreakAllPinLinks").c_str(), nullptr, false, targetNodeId != 0))
		{
			BreakAllNodeLinks(targetNodeId);
			ImGui::CloseCurrentPopup();
		}
		auto& sm = ShortcutManager::Get();
		if (targetNodeId != 0)
		{
			const bool isBp = HasBreakpoint(targetNodeId);
			const char* breakpointKey = isBp ? "GraphPanel.RemoveBreakpoint" : "GraphPanel.ToggleBreakpoint";
			if (ImGui::MenuItem(LOCAL_LABEL(breakpointKey).c_str(), sm.GetShortcutHint("Graph.ToggleBreakpoint")))
				ToggleBreakpoint(targetNodeId);
		}
		if (document.SelectedNodes.size() >= 2)
		{
			ImGui::Separator();
			if (ImGui::BeginMenu(LOCAL_LABEL("GraphPanel.Alignment").c_str()))
			{
				if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.AlignTop").c_str(), sm.GetShortcutHint("Graph.AlignTop"))) AlignSelectedNodes('W');
				if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.AlignBottom").c_str(), sm.GetShortcutHint("Graph.AlignBottom"))) AlignSelectedNodes('S');
				if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.AlignLeft").c_str(), sm.GetShortcutHint("Graph.AlignLeft"))) AlignSelectedNodes('A');
				if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.AlignRight").c_str(), sm.GetShortcutHint("Graph.AlignRight"))) AlignSelectedNodes('D');
				const bool canDistribute = document.SelectedNodes.size() >= 3;
				if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.DistributeHorizontal").c_str(), sm.GetShortcutHint("Graph.DistributeHoriz"), false, canDistribute))
					AlignSelectedNodes('H');
				if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.DistributeVertical").c_str(), sm.GetShortcutHint("Graph.DistributeVert"), false, canDistribute))
					AlignSelectedNodes('V');
				ImGui::EndMenu();
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_CSTR("GraphPanel.Delete"), sm.GetShortcutHint("Graph.Delete"))) DeleteSelection();
		ImGui::EndPopup();
	}
	// ── 핀 우클릭 메뉴 ──
	if (ImGui::BeginPopup("##GraphPinMenu"))
	{
		Engine::GraphNodeId targetNodeId = 0;
		std::string targetPinKey = "";
		for (const auto& [nodeId, view] : document.Nodes)
		{
			for (const auto& [pinKey, pinId] : view.Pins)
			{
				if (pinId == static_cast<int>(m_ContextPinId))
				{
					targetNodeId = nodeId;
					targetPinKey = pinKey;
					break;
				}
			}
			if (targetNodeId != 0) break;
		}

		const auto* targetNode = document.Data.FindNode(targetNodeId);
		const auto* schema = targetNode ? m_Registry.Find(targetNode->Type) : nullptr;
		const Engine::GraphPin* pinSchema = nullptr;
		if (schema)
		{
			for (const auto& pin : schema->Pins)
			{
				if (pin.Key == targetPinKey) { pinSchema = &pin; break; }
			}
		}

		if (targetNode && pinSchema)
		{
			ImGui::TextDisabled("%s", LOCAL_FMT("GraphPanel.PinSummary", targetPinKey, PinTypeName(pinSchema->Type)).c_str());
			ImGui::Separator();

			// 1. 해당 핀에 연결된 모든 와이어 분리
			const std::string breakLabel = ICON_FA_SCISSORS " " + LOCAL_FMT("GraphPanel.BreakPinLinks", targetPinKey) +
				"###GraphBreakPinLinks";
			if (ImGui::MenuItem(breakLabel.c_str()))
			{
				std::vector<Engine::GraphLinkId> linksToRemove;
				for (const auto& link : document.Data.GetLinks())
				{
					if ((link.From.Node == targetNodeId && link.From.Pin == targetPinKey) ||
						(link.To.Node == targetNodeId && link.To.Pin == targetPinKey))
					{
						linksToRemove.push_back(link.Id);
					}
				}
				for (auto linkId : linksToRemove)
				{
					if (ApplyResult(document.Data.Disconnect(linkId))) document.Links.erase(linkId);
				}
			}

			// 2. 오버라이드된 입력값을 기본값으로 복원 (입력 핀 전용)
			if (pinSchema->Direction == Engine::EGraphPinDirection::Input && pinSchema->Type != Engine::EGraphPinType::Execution)
			{
				if (ImGui::MenuItem((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("GraphPanel.ResetValue")).c_str()))
				{
					(void)document.Data.ResetInputValue(m_Registry, { targetNodeId, targetPinKey });
				}
			}

			// 3. 변수로 승격 (Promote to Variable - 데이터 핀 전용)
			if (pinSchema->Type != Engine::EGraphPinType::Execution)
			{
				if (ImGui::MenuItem((ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " " + LOCAL_LABEL("GraphPanel.PromoteVariable")).c_str()))
				{
					std::string varName = targetPinKey;
					int suffix = 1;
					while (document.Data.FindVariable(varName))
					{
						varName = targetPinKey + "_" + std::to_string(suffix++);
					}

					Engine::GraphValue defaultVal = pinSchema->DefaultValue;
					if (const auto it = targetNode->InputValues.find(targetPinKey); it != targetNode->InputValues.end())
					{
						defaultVal = it->second;
					}

					Engine::GraphVariable newVar{ varName, pinSchema->Type, defaultVal };
					if (document.Data.AddVariable(m_Registry, newVar).Succeeded())
					{
						document.Data.RegisterVariableNodes(m_Registry);
						const std::string varNodeType = "Variable.Get." + varName;
						m_AddPosition = ImVec2(m_ContextPosition.x - 180.0f, m_ContextPosition.y);
						GraphNodeCandidate candidate{ varNodeType, {} };
						if (AddNode(candidate))
						{
							const auto newNodeId = document.PendingSelection;
							const auto conn = document.Data.Connect(m_Registry, { newNodeId, "Value" }, { targetNodeId, targetPinKey });
							if (ApplyResult(conn))
							{
								document.Links.emplace(conn.Link, static_cast<int>(document.NextId++));
							}
						}
					}
				}
			}
		}

		ImGui::EndPopup();
	}
}

void GraphPanel::DrawNode(const GraphNode& node)
{
	if (node.Type == "Core.Comment")
	{
		DrawCommentNode(node);
		return;
	}
	if (node.Type.rfind("Core.Reroute", 0) == 0)
	{
		DrawRerouteNode(node);
		return;
	}

	const auto* definition = m_Registry.Find(node.Type);
	const auto found = GetDocument().Nodes.find(node.Id);
	if (!definition || found == GetDocument().Nodes.end()) return;

	auto& localization = LocalizationManager::Get();
	const auto nameText = localization.GetTextForDisplay(definition->DisplayName);
	const char* name = definition->DisplayName.empty() ? node.Type.c_str() : nameText.c_str();

	// 1. 핀 분류
	std::vector<const GraphPin*> execInputs, execOutputs, dataInputs, dataOutputs;
	for (const auto& pin : definition->Pins)
	{
		const bool isExec = (pin.Type == EGraphPinType::Execution);
		const bool isInput = (pin.Direction == EGraphPinDirection::Input);
		if (isExec) { if (isInput) execInputs.push_back(&pin); else execOutputs.push_back(&pin); }
		else { if (isInput) dataInputs.push_back(&pin); else dataOutputs.push_back(&pin); }
	}

	// 2. 컬럼 너비 정밀 계산 (핀 아이콘 실측 너비: 18.0f)
	constexpr float pinIconWidth = 18.0f;
	const float titleWidth = ImGui::CalcTextSize(name).x + 24.0f;

	float maxInWidth = 0.0f;
	for (const auto* pin : execInputs)
		maxInWidth = std::max(maxInWidth, ImGui::CalcTextSize(pin->Key.c_str()).x + pinIconWidth);
	for (const auto* pin : dataInputs)
	{
		const auto pinText = localization.GetTextForDisplay(pin->DisplayName);
		const char* label = pin->DisplayName.empty() ? pin->Key.c_str() : pinText.c_str();
		maxInWidth = std::max(maxInWidth, ImGui::CalcTextSize(label).x + pinIconWidth + 85.0f);
	}

	float maxOutWidth = 0.0f;
	for (const auto* pin : execOutputs)
		maxOutWidth = std::max(maxOutWidth, ImGui::CalcTextSize(pin->Key.c_str()).x + pinIconWidth);
	for (const auto* pin : dataOutputs)
	{
		const auto pinText = localization.GetTextForDisplay(pin->DisplayName);
		const char* label = pin->DisplayName.empty() ? pin->Key.c_str() : pinText.c_str();
		maxOutWidth = std::max(maxOutWidth, ImGui::CalcTextSize(label).x + pinIconWidth);
	}

	const float minNodeWidth = 140.0f;
	const float targetWidth = std::max({ minNodeWidth, titleWidth, maxInWidth + maxOutWidth + 20.0f });

	float middleSpacing = 16.0f;
	if (maxInWidth > 0.0f && maxOutWidth > 0.0f)
		middleSpacing = std::max(16.0f, targetWidth - maxInWidth - maxOutWidth);
	else if (maxInWidth == 0.0f && maxOutWidth > 0.0f)
		middleSpacing = std::max(0.0f, targetWidth - maxOutWidth);

	// 3. 노드 헤더 색상
	ImU32 headerColor = IM_COL32(40, 85, 145, 240);
	if (node.Type.rfind("Event.", 0) == 0) headerColor = IM_COL32(185, 40, 40, 240);
	else if (node.Type.rfind("Variable.Get.", 0) == 0) headerColor = IM_COL32(30, 130, 125, 240);
	else if (node.Type.rfind("Variable.Set.", 0) == 0) headerColor = IM_COL32(190, 100, 30, 240);
	else if (execInputs.empty() && execOutputs.empty()) headerColor = IM_COL32(40, 135, 65, 240);

	ed::BeginNode(found->second.Id);
	ImGui::PushID(static_cast<int>(node.Id));

	// ★ 노드 전체 단일 BeginGroup
	ImGui::BeginGroup();

	// ── 헤더 그룹 ──
	ImGui::BeginGroup();
	ImGui::TextUnformatted(name);
	if (ImGui::IsItemHovered() && !definition->Tooltip.empty())
	{
		ed::Suspend();
		ImGui::SetTooltip("%s", LOCAL_CSTR(definition->Tooltip));
		ed::Resume();
	}
	ImGui::Dummy(ImVec2(targetWidth, 2.0f));
	ImGui::EndGroup();
	const float headerBottom = ImGui::GetItemRectMax().y + 4.0f;
	ImGui::Spacing();

	const bool execStraight = (m_WireStyle == EWireStyle::ElectronicHybrid || m_WireStyle == EWireStyle::Straight);
	const bool dataStraight = (m_WireStyle == EWireStyle::Straight);
	// 1. 좌측 컬럼 렌더링 및 실제 차지한 가로폭(leftColWidth) 정밀 측정
	float leftColWidth = 0.0f;
	if (maxInWidth > 0.0f)
	{
		ImGui::BeginGroup();
		if (execStraight) ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
		for (const auto* inPin : execInputs)
		{
			const int id = found->second.Pins.at(inPin->Key);
			ed::BeginPin(id, ed::PinKind::Input);
			ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ICON_FA_PLAY);
			ed::EndPin();
			ImGui::SameLine(0.0f, 6.0f);
			ImGui::TextUnformatted(inPin->Key.c_str());
		}
		if (execStraight) ed::PopStyleVar();
		if (dataStraight) ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
		for (const auto* inPin : dataInputs)
		{
			const int id = found->second.Pins.at(inPin->Key);
			const ImVec4 pinColor = ImGui::ColorConvertU32ToFloat4(PinColor(inPin->Type));
			ed::BeginPin(id, ed::PinKind::Input);
			ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
			ImGui::TextColored(pinColor, ICON_FA_CIRCLE);
			ed::EndPin();
			ImGui::SameLine(0.0f, 6.0f);
			const auto pinText = localization.GetTextForDisplay(inPin->DisplayName);
			const char* label = inPin->DisplayName.empty() ? inPin->Key.c_str() : pinText.c_str();
			ImGui::TextUnformatted(label);
			DrawPinValue(node, *inPin, 80.0f);
		}
		if (dataStraight) ed::PopStyleVar();
		ImGui::EndGroup();
		leftColWidth = ImGui::GetItemRectSize().x; // ★ 좌측 컬럼의 실제 너비 캡처!
	}
	// 2. 우측 컬럼을 노드의 맨 오른쪽 테두리로 밀어내기
	if (maxOutWidth > 0.0f)
	{
		const float availWidth = std::max(targetWidth, leftColWidth + maxOutWidth + 24.0f);
		const float rightSpacing = std::max(16.0f, availWidth - leftColWidth - maxOutWidth);
		if (leftColWidth > 0.0f)
			ImGui::SameLine(0.0f, rightSpacing);
		else
			ImGui::Dummy(ImVec2(rightSpacing, 1.0f)), ImGui::SameLine(0.0f, 0.0f);
		ImGui::BeginGroup();
		if (execStraight) ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
		for (const auto* outPin : execOutputs)
		{
			const float textW = ImGui::CalcTextSize(outPin->Key.c_str()).x;
			const float actualRowW = textW + pinIconWidth;
			const float indent = std::max(0.0f, maxOutWidth - actualRowW);
			if (indent > 0.0f) ImGui::Indent(indent);
			ImGui::TextUnformatted(outPin->Key.c_str());
			ImGui::SameLine(0.0f, 6.0f);
			const int id = found->second.Pins.at(outPin->Key);
			ed::BeginPin(id, ed::PinKind::Output);
			ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ICON_FA_PLAY);
			ed::EndPin();
			if (indent > 0.0f) ImGui::Unindent(indent);
		}
		if (execStraight) ed::PopStyleVar();
		if (dataStraight) ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
		for (const auto* outPin : dataOutputs)
		{
			const auto pinText = localization.GetTextForDisplay(outPin->DisplayName);
			const char* label = outPin->DisplayName.empty() ? outPin->Key.c_str() : pinText.c_str();
			const float textW = ImGui::CalcTextSize(label).x;
			const float actualRowW = textW + pinIconWidth;
			const float indent = std::max(0.0f, maxOutWidth - actualRowW);
			if (indent > 0.0f) ImGui::Indent(indent);
			ImGui::TextUnformatted(label);
			ImGui::SameLine(0.0f, 6.0f);
			const int id = found->second.Pins.at(outPin->Key);
			const ImVec4 pinColor = ImGui::ColorConvertU32ToFloat4(PinColor(outPin->Type));
			ed::BeginPin(id, ed::PinKind::Output);
			ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
			ImGui::TextColored(pinColor, ICON_FA_CIRCLE);
			ed::EndPin();
			if (indent > 0.0f) ImGui::Unindent(indent);
		}
		if (dataStraight) ed::PopStyleVar();
		ImGui::EndGroup();
	}
	ImGui::EndGroup(); // 노드 전체 그룹 닫기

	ImGui::PopID();
	ed::EndNode();

	// ── 헤더 배경 ──
	if (ImGui::IsItemVisible())
	{
		const ImVec2 nodeMin = ImGui::GetItemRectMin();
		const ImVec2 nodeMax = ImGui::GetItemRectMax();
		const auto& style = ed::GetStyle();
		const float halfBorder = style.NodeBorderWidth * 0.5f;

		auto* drawList = ed::GetNodeBackgroundDrawList(found->second.Id);
		drawList->AddRectFilled(
			ImVec2(nodeMin.x + halfBorder, nodeMin.y + halfBorder),
			ImVec2(nodeMax.x - halfBorder, headerBottom),
			headerColor, style.NodeRounding, ImDrawFlags_RoundCornersTop);

		drawList->AddLine(
			ImVec2(nodeMin.x + halfBorder, headerBottom),
			ImVec2(nodeMax.x - halfBorder, headerBottom),
			IM_COL32(255, 255, 255, 35), 1.0f);
	}
}

void GraphPanel::DrawPinValue(const GraphNode& node, const GraphPin& pin, float width)
{
	if (pin.Type == EGraphPinType::Execution) return;

	ImGui::PushID(pin.Key.c_str());
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);

	auto& graph = GetDocument().Data;
	const GraphPinReference reference{ node.Id, pin.Key };
	const bool input = pin.Direction == EGraphPinDirection::Input;
	const bool connected = input && std::any_of(graph.GetLinks().begin(), graph.GetLinks().end(),
		[&](const GraphLink& link) { return link.To == reference; });

	if (input && !connected && pin.Type != EGraphPinType::Object)
	{
		const auto found = node.InputValues.find(pin.Key);
		const bool overridden = found != node.InputValues.end();
		GraphValue value = overridden ? found->second : pin.DefaultValue;
		const bool missing = std::holds_alternative<std::monostate>(value);
		if (missing) value = MakeDefaultValue(pin.Type);

		const float itemWidth = overridden ? (width - 24.0f) : width;
		ImGui::SetNextItemWidth(itemWidth);
		if (DrawValue(pin.Type, value)) ApplyResult(graph.SetInputValue(m_Registry, reference, value));

		if (overridden)
		{
			ImGui::SameLine(0.0f, 4.0f);
			if (ImGui::SmallButton(ICON_FA_ROTATE_LEFT "##Reset"))
				ApplyResult(graph.ResetInputValue(m_Registry, reference));

			// ★ 핵심: ed::Suspend() / Resume()으로 감싸 툴팁 위치 튐 해결!
			if (ImGui::IsItemHovered())
			{
				ed::Suspend();
				ImGui::SetTooltip("%s", LOCAL_CSTR("GraphPanel.ResetValue"));
				ed::Resume();
			}
		}

		ImGui::PopTextWrapPos();
		ImGui::PopID();
		return;
	}

	if (pin.Type == EGraphPinType::Object)
	{
		ImGui::TextDisabled("%s", pin.ObjectType.c_str());
	}
	else if (const auto* value = FindObservedValue(reference))
	{
		const auto text = TraceValueText(*value);
		ImGui::TextColored(ImVec4(0.95f, 0.8f, 0.45f, 1.0f), "= %s", text.c_str());
		if (ImGui::IsItemHovered())
		{
			ed::Suspend();
			ImGui::SetTooltip("%s", LOCAL_CSTR("GraphPanel.LastObserved"));
			ed::Resume();
		}
	}

	ImGui::PopTextWrapPos();
	ImGui::PopID();
}
void GraphPanel::DrawLinks()
{
	auto& document = GetDocument();
	for (const auto& link : document.Data.GetLinks())
	{
		const auto found = document.Links.find(link.Id);
		if (found == document.Links.end()) continue;

		const auto fromPin = FindPinId(link.From);
		const auto toPin = FindPinId(link.To);
		if (fromPin == 0 || toPin == 0) continue;

		const auto* fromNode = document.Data.FindNode(link.From.Node);
		const auto* pin = fromNode ? m_Registry.FindPin(fromNode->Type, link.From.Pin) : nullptr;
		const ImU32 color = pin ? PinColor(pin->Type) : IM_COL32(220, 220, 220, 255);

		// ★ Exec 실행선은 2.8px로 굵고 선명하게, Data선은 1.8px로 날렵하게 차별화!
		const bool isExec = pin && (pin->Type == Engine::EGraphPinType::Execution);
		const float thickness = isExec ? 2.8f : 1.8f;

		ed::Link(found->second, fromPin, toPin, ImGui::ColorConvertU32ToFloat4(color), thickness);
	}
}

#pragma endregion

#pragma region Connections&Selection
void GraphPanel::HandleConnections(const ImVec2& origin, bool canvasHovered)
{
	auto& document = GetDocument();

	// 1. 링크 생성 처리 (BeginCreate가 true일 때만 내부에서 처리 및 EndCreate 호출)
	if (ed::BeginCreate())
	{
		ed::PinId inputPinId, outputPinId;
		if (ed::QueryNewLink(&inputPinId, &outputPinId))
		{
			if (inputPinId && outputPinId)
			{
				if (ed::AcceptNewItem())
				{
					GraphPinReference from, to;
					GetConnectionPins(static_cast<int>(inputPinId.Get()), static_cast<int>(outputPinId.Get()), from, to);
					if (ReserveEditorIds(1))
					{
						// ★ [언리얼 블루프린트 스타일] 기존 와이어 자동 교체 (Replace Connection)
						const auto* fromNode = document.Data.FindNode(from.Node);
						const auto* fromPin = fromNode ? m_Registry.FindPin(fromNode->Type, from.Pin) : nullptr;
						const bool isExec = fromPin && (fromPin->Type == Engine::EGraphPinType::Execution);
						// 1) 대상 입력 핀(to)에 이미 연결된 선이 있거나, 2) 실행 출력 핀(from)에 이미 연결된 선이 있다면 먼저 끊기!
						std::vector<Engine::GraphLinkId> linksToRemove;
						for (const auto& l : document.Data.GetLinks())
						{
							if (l.To == to || (isExec && l.From == from))
								linksToRemove.push_back(l.Id);
						}
						for (auto linkId : linksToRemove)
						{
							if (ApplyResult(document.Data.Disconnect(linkId)))
								document.Links.erase(linkId);
						}
						// 새로운 링크 연결
						const auto result = document.Data.Connect(m_Registry, from, to);
						if (ApplyResult(result)) document.Links.emplace(result.Link, static_cast<int>(document.NextId++));
					}
				}
			}
		}

		ed::PinId droppedPinId;
		if (ed::QueryNewNode(&droppedPinId))
		{
			if (ed::AcceptNewItem())
			{
				const auto found = document.Pins.find(static_cast<int>(droppedPinId.Get()));
				if (found != document.Pins.end())
				{
					m_AddSource = found->second;
					const auto mouse = InputManager::Get().GetScreenMousePosition();
					m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
					PrepareNodeMenu(m_AddSource);
					m_OpenNodePopup = true;
				}
			}
		}

		ed::EndCreate(); // <--- if (ed::BeginCreate()) 블록 내부로 이동
	}

	// 2. 링크 및 노드 삭제 처리
	if (ed::BeginDelete())
	{
		std::vector<Engine::GraphLinkId> linksToDelete;
		ed::LinkId linkId;
		while (ed::QueryDeletedLink(&linkId))
		{
			if (ed::AcceptDeletedItem())
			{
				for (const auto& [id, edId] : document.Links)
				{
					if (edId == static_cast<int>(linkId.Get())) { linksToDelete.push_back(id); break; }
				}
			}
		}

		std::vector<Engine::GraphNodeId> nodesToDelete;
		ed::NodeId delNodeId;
		while (ed::QueryDeletedNode(&delNodeId))
		{
			if (ed::AcceptDeletedItem())
			{
				const int viewId = static_cast<int>(delNodeId.Get());
				for (const auto& [nodeId, view] : document.Nodes)
				{
					if (view.Id == viewId) { nodesToDelete.push_back(nodeId); break; }
				}
			}
		}
		ed::EndDelete();

		// ed::EndDelete() 트랜잭션 종료 후 안전하게 일괄 삭제 수행
		if (!linksToDelete.empty() || !nodesToDelete.empty())
		{
			document.SelectedLinks = std::move(linksToDelete);
			document.SelectedNodes = std::move(nodesToDelete);
			DeleteSelection();
		}
	}
}
void GraphPanel::ReadSelection()
{
	auto& document = GetDocument();
	document.SelectedNodes.clear();
	document.SelectedLinks.clear();
	const int nodeCount = ed::GetSelectedNodes(nullptr, 0);
	const int linkCount = ed::GetSelectedLinks(nullptr, 0);
	std::vector<ed::NodeId> nodes(nodeCount);
	std::vector<ed::LinkId> links(linkCount);
	if (nodeCount > 0) ed::GetSelectedNodes(nodes.data(), nodeCount);
	if (linkCount > 0) ed::GetSelectedLinks(links.data(), linkCount);
	for (auto id : nodes)
	{
		const int viewId = static_cast<int>(id.Get());
		for (const auto& [nodeId, view] : document.Nodes)
		{
			if (view.Id == viewId) { document.SelectedNodes.push_back(nodeId); break; }
		}
	}
	for (auto id : links)
	{
		const int linkEdId = static_cast<int>(id.Get());
		for (const auto& [linkId, link] : document.Links)
		{
			if (link == linkEdId) { document.SelectedLinks.push_back(linkId); break; }
		}
	}
}

bool GraphPanel::ReserveEditorIds(std::size_t count)
{
	const auto next = GetDocument().NextId;
	const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<int>::max());
	if (next > maximum || count > maximum - next + 1)
		return ApplyResult({ EGraphError::IdExhausted, 0, {}, 0, "GraphPanel.IdExhausted" });
	return true;
}

int GraphPanel::FindPinId(const GraphPinReference& reference) const
{
	const auto& nodes = GetDocument().Nodes;
	const auto node = nodes.find(reference.Node);
	if (node == nodes.end()) return 0;
	const auto pin = node->second.Pins.find(reference.Pin);
	return pin == node->second.Pins.end() ? 0 : pin->second;
}

void GraphPanel::GetConnectionPins(int first, int second, GraphPinReference& from, GraphPinReference& to) const
{
	const auto& document = GetDocument();
	const auto a = document.Pins.find(first);
	const auto b = document.Pins.find(second);
	if (a == document.Pins.end() || b == document.Pins.end()) return;
	const auto* node = document.Data.FindNode(a->second.Node);
	const auto* pin = node ? m_Registry.FindPin(node->Type, a->second.Pin) : nullptr;
	const bool output = pin && pin->Direction == EGraphPinDirection::Output;
	from = output ? a->second : b->second;
	to = output ? b->second : a->second;
}

ImU32 GraphPanel::PinColor(EGraphPinType type)
{
	switch (type)
	{
	case EGraphPinType::Execution: return IM_COL32(230, 230, 230, 255);
	case EGraphPinType::Bool: return IM_COL32(225, 90, 100, 255);
	case EGraphPinType::Int: return IM_COL32(80, 200, 210, 255);
	case EGraphPinType::Float: return IM_COL32(110, 210, 120, 255);
	case EGraphPinType::String: return IM_COL32(235, 95, 210, 255); // 언리얼 스타일의 핫핑크
	case EGraphPinType::Object: return IM_COL32(190, 130, 230, 255);
	default: return IM_COL32(110, 155, 240, 255);
	}
}

const char* GraphPanel::PinTypeName(EGraphPinType type)
{
	switch (type)
	{
	case EGraphPinType::Execution: return "Execution";
	case EGraphPinType::Bool: return "Bool";
	case EGraphPinType::Int: return "Int";
	case EGraphPinType::Float: return "Float";
	case EGraphPinType::Float2: return "Float2";
	case EGraphPinType::Float3: return "Float3";
	case EGraphPinType::Float4: return "Float4";
	case EGraphPinType::String: return "String";
	case EGraphPinType::Object: return "Object";
	default: return "Unknown";
	}
}
#pragma endregion

#pragma region Comment
void GraphPanel::DrawCommentNode(const GraphNode& node)
{
	const auto found = GetDocument().Nodes.find(node.Id);
	if (found == GetDocument().Nodes.end()) return;

	auto& document = GetDocument();

	// 1. 코멘트 텍스트 조회
	std::string commentText = "Comment";
	const auto itText = document.CommentTexts.find(node.Id);
	if (itText != document.CommentTexts.end() && !itText->second.empty()) commentText = itText->second;

	// 2. 크기(Size) 조회
	ImVec2 groupSize(400.0f, 250.0f);
	const auto foundSize = node.InputValues.find("Size");
	if (foundSize != node.InputValues.end() && std::holds_alternative<std::array<float, 2>>(foundSize->second))
	{
		const auto arr = std::get<std::array<float, 2>>(foundSize->second);
		groupSize = ImVec2(arr[0], arr[1]);
	}

	// 3. 언리얼 블루프린트 코멘트 박스 스타일 설정
	ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(255, 255, 255, 14));
	ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(255, 255, 255, 40));
	ed::PushStyleColor(ed::StyleColor_HovNodeBorder, ImColor(255, 255, 255, 120));
	ed::PushStyleColor(ed::StyleColor_SelNodeBorder, ImColor(255, 220, 60, 220));
	ed::PushStyleVar(ed::StyleVar_NodeRounding, 6.0f);
	ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 1.2f);
	ed::PushStyleVar(ed::StyleVar_GroupRounding, 6.0f);
	ed::PushStyleVar(ed::StyleVar_GroupBorderWidth, 1.2f);

	ed::BeginNode(found->second.Id);
	ImGui::PushID(static_cast<int>(node.Id));

	ImGui::BeginGroup();

	// 4. 헤더 텍스트 바 (더블클릭 시 인라인 이름 변경)
	const bool isEditing = (m_EditingCommentNode == node.Id);
	if (isEditing)
	{
		ImGui::SetNextItemWidth(std::max(120.0f, groupSize.x - 30.0f));
		if (ImGui::InputText("##CommentEdit", m_CommentEditBuffer, sizeof(m_CommentEditBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
		{
			document.CommentTexts[node.Id] = m_CommentEditBuffer;
			m_EditingCommentNode = 0;
		}
		if (!ImGui::IsItemActive() && Engine::InputManager::Get().IsMouseButtonDown(Engine::EMouseButton::Left))
		{
			document.CommentTexts[node.Id] = m_CommentEditBuffer;
			m_EditingCommentNode = 0;
		}
	}
	else
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
		ImGui::TextUnformatted(commentText.c_str());
		ImGui::PopStyleColor();

		if (ImGui::IsItemHovered() && Engine::InputManager::Get().IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
		{
			m_EditingCommentNode = node.Id;
			strncpy_s(m_CommentEditBuffer, commentText.c_str(), sizeof(m_CommentEditBuffer) - 1);
		}
	}

	// ★ 핵심: ed::Group을 호출하여 내부 노드 감싸기 & 드래그 연동!
	ed::Group(groupSize);

	ImGui::EndGroup();

	ImGui::PopID();
	ed::EndNode();
	ed::PopStyleVar(4);
	ed::PopStyleColor(4);

	// 마우스로 코멘트 박스 크기를 조절(Resize)했을 때 새 크기 동기화
	const ImVec2 currentSize = ed::GetNodeSize(found->second.Id);
	if (currentSize.x > 0.0f && currentSize.y > 0.0f &&
		(std::abs(currentSize.x - groupSize.x) > 2.0f || std::abs(currentSize.y - groupSize.y) > 2.0f))
	{
		(void)document.Data.SetInputValue(m_Registry, { node.Id, "Size" },
			std::array<float, 2>{ currentSize.x, currentSize.y });
	}
}

void GraphPanel::CreateCommentBoxFromSelection()
{
	auto& document = GetDocument();
	ImVec2 commentPos = m_AddPosition;
	ImVec2 commentSize(400.0f, 250.0f);

	// 1. 선택된 노드들이 있다면, 그 노드들을 모두 감싸는 Bounding Box 계산
	if (!document.SelectedNodes.empty())
	{
		ImVec2 minPos(FLT_MAX, FLT_MAX);
		ImVec2 maxPos(-FLT_MAX, -FLT_MAX);
		bool hasValidNode = false;

		for (auto selNodeId : document.SelectedNodes)
		{
			const auto it = document.Nodes.find(selNodeId);
			if (it == document.Nodes.end()) continue;

			// 기존 코멘트 노드는 Bounding Box 계산에서 제외
			const auto* node = document.Data.FindNode(selNodeId);
			if (node && node->Type == "Core.Comment") continue;

			const ImVec2 pos = ed::GetNodePosition(it->second.Id);
			const ImVec2 size = ed::GetNodeSize(it->second.Id);
			if (pos.x == FLT_MAX) continue;

			minPos.x = std::min(minPos.x, pos.x);
			minPos.y = std::min(minPos.y, pos.y);
			maxPos.x = std::max(maxPos.x, pos.x + size.x);
			maxPos.y = std::max(maxPos.y, pos.y + size.y);
			hasValidNode = true;
		}

		if (hasValidNode)
		{
			const float paddingX = 35.0f;
			const float paddingY = 25.0f;
			const float headerExtraY = 32.0f;

			commentPos = ImVec2(minPos.x - paddingX, minPos.y - paddingY - headerExtraY);
			commentSize = ImVec2(maxPos.x - minPos.x + paddingX * 2.0f,
				maxPos.y - minPos.y + paddingY * 2.0f + headerExtraY);
		}
	}
	else
	{
		const auto mouse = InputManager::Get().GetScreenMousePosition();
		commentPos = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
	}

	// 2. 코멘트 노드 생성 및 초기 크기/텍스트 주입
	m_AddPosition = commentPos;
	GraphNodeCandidate candidate;
	candidate.Type = "Core.Comment";
	if (AddNode(candidate))
	{
		const auto lastNodeId = document.PendingSelection;
		(void)document.Data.SetInputValue(m_Registry, { lastNodeId, "Size" },
			std::array<float, 2>{ commentSize.x, commentSize.y });
		document.CommentTexts[lastNodeId] = "Comment";
	}
}

void GraphPanel::DrawRerouteNode(const Engine::GraphNode& node)
{
	const auto found = GetDocument().Nodes.find(node.Id);
	if (found == GetDocument().Nodes.end()) return;
	const auto* inPin = m_Registry.FindPin(node.Type, "In");
	const auto* outPin = m_Registry.FindPin(node.Type, "Out");
	if (!inPin || !outPin) return;
	const bool isExec = (inPin->Type == Engine::EGraphPinType::Execution);
	const ImU32 pinCol = PinColor(inPin->Type);
	const ImVec4 pinColor = ImGui::ColorConvertU32ToFloat4(pinCol);
	const bool rerouteStraight = (isExec && (m_WireStyle == EWireStyle::ElectronicHybrid || m_WireStyle == EWireStyle::Straight))
		|| (!isExec && m_WireStyle == EWireStyle::Straight);
	if (rerouteStraight) ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
	// 네모난 테두리 및 배경 100% 완전 투명화
	ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ed::PushStyleColor(ed::StyleColor_HovNodeBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ed::PushStyleColor(ed::StyleColor_SelNodeBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 0.0f);
	ed::PushStyleVar(ed::StyleVar_HoveredNodeBorderWidth, 0.0f);
	ed::PushStyleVar(ed::StyleVar_SelectedNodeBorderWidth, 0.0f);
	ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ed::BeginNode(found->second.Id);
	ImGui::PushID(static_cast<int>(node.Id));
	// 1. 점 아이콘 렌더링
	const bool isSelected = ed::IsNodeSelected(found->second.Id);
	const ImVec4 dotColor = isSelected ? ImVec4(1.0f, 0.85f, 0.3f, 1.0f) : pinColor;
	ImGui::TextColored(dotColor, isExec ? ICON_FA_PLAY : ICON_FA_CIRCLE);
	// 2. 점 아이콘의 Bounding Box 및 정중앙 좌표(dotCenter) 계산
	const ImVec2 dotMin = ImGui::GetItemRectMin();
	const ImVec2 dotMax = ImGui::GetItemRectMax();
	const ImVec2 dotCenter = ImVec2((dotMin.x + dotMax.x) * 0.5f, (dotMin.y + dotMax.y) * 0.5f);
	// ★ 3. In 핀: 좌측 절반 영역을 잡되, 와이어 연결 피벗은 점의 완벽한 정중앙(dotCenter)!
	const int inPinId = found->second.Pins.at("In");
	ed::BeginPin(inPinId, ed::PinKind::Input);
	ed::PinRect(dotMin, ImVec2(dotCenter.x, dotMax.y));
	ed::PinPivotRect(dotCenter, dotCenter);
	ed::EndPin();
	// ★ 4. Out 핀: 우측 절반 영역을 잡되, 와이어 연결 피벗은 점의 완벽한 정중앙(dotCenter)!
	const int outPinId = found->second.Pins.at("Out");
	ed::BeginPin(outPinId, ed::PinKind::Output);
	ed::PinRect(ImVec2(dotCenter.x, dotMin.y), dotMax);
	ed::PinPivotRect(dotCenter, dotCenter);
	ed::EndPin();
	ImGui::PopID();
	ed::EndNode();
	ed::PopStyleVar(4);
	ed::PopStyleColor(4);
	if (rerouteStraight) ed::PopStyleVar();
}

void GraphPanel::InsertRerouteNodeOnLink(uintptr_t hoveredLinkEdId, const ImVec2& canvasPos)
{
	auto& document = GetDocument();
	Engine::GraphLinkId targetLinkId = 0;
	for (const auto& [id, edId] : document.Links)
	{
		if (edId == static_cast<int>(hoveredLinkEdId))
		{
			targetLinkId = id;
			break;
		}
	}
	if (targetLinkId == 0) return;

	const auto links = document.Data.GetLinks();
	const auto linkIt = std::find_if(links.begin(), links.end(),
		[targetLinkId](const Engine::GraphLink& l) { return l.Id == targetLinkId; });
	if (linkIt == links.end()) return;

	const auto originalFrom = linkIt->From;
	const auto originalTo = linkIt->To;

	const auto* fromNode = document.Data.FindNode(originalFrom.Node);
	const auto* fromPin = fromNode ? m_Registry.FindPin(fromNode->Type, originalFrom.Pin) : nullptr;
	if (!fromPin) return;

	std::string rerouteType = "Core.Reroute";
	switch (fromPin->Type)
	{
	case Engine::EGraphPinType::Execution: rerouteType = "Core.Reroute"; break;
	case Engine::EGraphPinType::Bool: rerouteType = "Core.Reroute.Bool"; break;
	case Engine::EGraphPinType::Int: rerouteType = "Core.Reroute.Int"; break;
	case Engine::EGraphPinType::Float: rerouteType = "Core.Reroute.Float"; break;
	case Engine::EGraphPinType::Float2: rerouteType = "Core.Reroute.Float2"; break;
	case Engine::EGraphPinType::Float3: rerouteType = "Core.Reroute.Float3"; break;
	case Engine::EGraphPinType::Float4: rerouteType = "Core.Reroute.Float4"; break;
	default: break;
	}

	if (!ApplyResult(document.Data.Disconnect(targetLinkId))) return;
	document.Links.erase(targetLinkId);

	// 마우스 클릭 위치에 노드 생성
	m_AddPosition = canvasPos;
	GraphNodeCandidate candidate{ rerouteType, {} };
	if (!AddNode(candidate)) return;
	const auto newNodeId = document.PendingSelection;

	// In / Out 양방향 자동 연결
	const auto connFrom = document.Data.Connect(m_Registry, originalFrom, { newNodeId, "In" });
	if (ApplyResult(connFrom)) document.Links.emplace(connFrom.Link, static_cast<int>(document.NextId++));

	const auto connTo = document.Data.Connect(m_Registry, { newNodeId, "Out" }, originalTo);
	if (ApplyResult(connTo)) document.Links.emplace(connTo.Link, static_cast<int>(document.NextId++));

	// ★ 핵심: 마우스 위치에 노드 위치를 강제 확정! (BuildDocumentViews 제거)
	ed::SetNodePosition(document.Nodes.at(newNodeId).Id, canvasPos);
}

void GraphPanel::AlignSelectedNodes(char direction)
{
	auto& document = GetDocument();
	struct NodeLayout { Engine::GraphNodeId Id; ed::NodeId EdId; ImVec2 Pos; ImVec2 Size; };
	std::vector<NodeLayout> selected;
	for (auto nodeId : document.SelectedNodes)
	{
		const auto* node = document.Data.FindNode(nodeId);
		if (!node || node->Type == "Core.Comment") continue;
		const auto it = document.Nodes.find(nodeId);
		if (it == document.Nodes.end()) continue;
		const ed::NodeId edId = it->second.Id;
		const ImVec2 pos = ed::GetNodePosition(edId);
		const ImVec2 size = ed::GetNodeSize(edId);
		if (pos.x == FLT_MAX || size.x <= 0.0f) continue;
		selected.push_back({ nodeId, edId, pos, size });
	}
	if (direction == 'H' || direction == 'V')
	{
		if (selected.size() < 3) return;
	}
	else
	{
		if (selected.size() < 2) return;
	}
	switch (direction)
	{
	case 'W': // Align Top: Y 최소값 기준
	{
		float minY = FLT_MAX;
		for (const auto& item : selected) minY = std::min(minY, item.Pos.y);
		for (const auto& item : selected) ed::SetNodePosition(item.EdId, ImVec2(item.Pos.x, minY));
		break;
	}
	case 'S': // Align Bottom: Y 최대 바텀값 기준
	{
		float maxBottom = -FLT_MAX;
		for (const auto& item : selected) maxBottom = std::max(maxBottom, item.Pos.y + item.Size.y);
		for (const auto& item : selected) ed::SetNodePosition(item.EdId, ImVec2(item.Pos.x, maxBottom - item.Size.y));
		break;
	}
	case 'A': // Align Left: X 최소값 기준
	{
		float minX = FLT_MAX;
		for (const auto& item : selected) minX = std::min(minX, item.Pos.x);
		for (const auto& item : selected) ed::SetNodePosition(item.EdId, ImVec2(minX, item.Pos.y));
		break;
	}
	case 'D': // Align Right: X 최대 라이트값 기준
	{
		float maxRight = -FLT_MAX;
		for (const auto& item : selected) maxRight = std::max(maxRight, item.Pos.x + item.Size.x);
		for (const auto& item : selected) ed::SetNodePosition(item.EdId, ImVec2(maxRight - item.Size.x, item.Pos.y));
		break;
	}
	case 'H': // Distribute Horizontally: 가로 균등 분배
	{
		std::sort(selected.begin(), selected.end(), [](const NodeLayout& a, const NodeLayout& b) { return a.Pos.x < b.Pos.x; });
		float totalWidth = 0.0f;
		for (const auto& item : selected) totalWidth += item.Size.x;
		const float span = (selected.back().Pos.x + selected.back().Size.x) - selected.front().Pos.x;
		const float totalGap = span - totalWidth;
		const float gap = totalGap / static_cast<float>(selected.size() - 1);
		float currentX = selected.front().Pos.x;
		for (size_t i = 1; i < selected.size() - 1; ++i)
		{
			currentX += selected[i - 1].Size.x + gap;
			ed::SetNodePosition(selected[i].EdId, ImVec2(currentX, selected[i].Pos.y));
		}
		break;
	}
	case 'V': // Distribute Vertically: 세로 균등 분배
	{
		std::sort(selected.begin(), selected.end(), [](const NodeLayout& a, const NodeLayout& b) { return a.Pos.y < b.Pos.y; });
		float totalHeight = 0.0f;
		for (const auto& item : selected) totalHeight += item.Size.y;
		const float span = (selected.back().Pos.y + selected.back().Size.y) - selected.front().Pos.y;
		const float totalGap = span - totalHeight;
		const float gap = totalGap / static_cast<float>(selected.size() - 1);
		float currentY = selected.front().Pos.y;
		for (size_t i = 1; i < selected.size() - 1; ++i)
		{
			currentY += selected[i - 1].Size.y + gap;
			ed::SetNodePosition(selected[i].EdId, ImVec2(selected[i].Pos.x, currentY));
		}
		break;
	}
	default: break;
	}
}
#pragma endregion

void GraphPanel::BreakAllNodeLinks(Engine::GraphNodeId nodeId)
{
	auto& document = GetDocument();
	std::vector<Engine::GraphLinkId> toRemove;
	for (const auto& link : document.Data.GetLinks())
	{
		if (link.From.Node == nodeId || link.To.Node == nodeId) toRemove.push_back(link.Id);
	}
	for (auto linkId : toRemove)
	{
		if (ApplyResult(document.Data.Disconnect(linkId))) document.Links.erase(linkId);
	}
}

void GraphPanel::DuplicateSelection()
{
	auto& document = GetDocument();
	if (document.SelectedNodes.empty()) return;

	std::vector<Engine::GraphNodeId> newSelected;
	const float offset = 30.0f;

	for (auto selNodeId : document.SelectedNodes)
	{
		const auto* origNode = document.Data.FindNode(selNodeId);
		if (!origNode) continue;

		const auto origView = document.Nodes.find(selNodeId);
		ImVec2 origPos = m_AddPosition;
		if (origView != document.Nodes.end())
		{
			const ImVec2 pos = ed::GetNodePosition(origView->second.Id);
			if (pos.x != FLT_MAX) origPos = ImVec2(pos.x + offset, pos.y + offset);
		}

		m_AddPosition = origPos;
		GraphNodeCandidate candidate{ origNode->Type, {} };
		if (AddNode(candidate))
		{
			const auto newNodeId = document.PendingSelection;
			newSelected.push_back(newNodeId);

			for (const auto& [key, val] : origNode->InputValues)
			{
				(void)document.Data.SetInputValue(m_Registry, { newNodeId, key }, val);
			}

			auto itComment = document.CommentTexts.find(selNodeId);
			if (itComment != document.CommentTexts.end())
			{
				document.CommentTexts[newNodeId] = itComment->second;
			}
		}
	}

	document.SelectedNodes = std::move(newSelected);
	BuildDocumentViews(document);
}
