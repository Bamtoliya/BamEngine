#include "GraphPanel.h"
#include "GraphMessageLocalization.h"
#include "InputManager.h"
#include "LocalizationManager.h"
#include <imgui_node_editor.h>
namespace ed = ax::NodeEditor;

#pragma region Context
void GraphPanel::ActivateDocument()
{
	auto& document = GetDocument();
	if (!document.Context)
	{
		ed::Config config;
		config.SettingsName = (m_Domain == Engine::EGraphDomain::Logic) ? "LogicGraph" : "ShaderGraph";
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
		style.Colors[ed::StyleColor_Flow] = ImVec4(1.0f, 0.95f, 0.35f, 1.0f); // 실행선 형광 펄스
		style.Colors[ed::StyleColor_FlowMarker] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		style.FlowSpeed = 160.0f;
		style.FlowMarkerDistance = 28.0f;
	}
	ed::SetCurrentEditor(document.Context);
}
void GraphPanel::SuspendDocument()
{
	m_Executor.Pause();
	m_DragPin = 0;
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
	// 빈 캔버스 배경 우클릭 시 노드 생성 메뉴 오픈
	const bool itemHovered = ed::GetHoveredNode() || ed::GetHoveredPin() || ed::GetHoveredLink();
	if (InputManager::Get().IsMouseButtonUp(EMouseButton::Right) && !itemHovered)
	{
		const auto mouse = InputManager::Get().GetScreenMousePosition();
		m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
		PrepareNodeMenu();
		ImGui::OpenPopup("##GraphCanvasMenu");
	}
	const bool menuOpen = ImGui::IsPopupOpen("##GraphCanvasMenu") || m_OpenNodePopup;
	const bool canvasFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	if (canvasFocused && !menuOpen && !ImGui::GetIO().WantTextInput && InputManager::Get().IsKeyButtonDown(EKeyCode::C))
	{
		CreateCommentBoxFromSelection();
	}

	if (ImGui::BeginPopup("##GraphCanvasMenu"))
	{
		DrawNodeMenu();
		ImGui::Separator();
		if (ImGui::MenuItem("Create Comment Box", "C")) CreateCommentBoxFromSelection();
		const bool selected = !document.SelectedNodes.empty() || !document.SelectedLinks.empty();
		if (ImGui::MenuItem(LOCAL_CSTR("GraphPanel.Delete"), "Delete", false, selected)) DeleteSelection();
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

	// 2. 노드별 최적 너비 계산 (최소 180px 보장)
	float nodeWidth = std::max(180.0f, ImGui::CalcTextSize(name).x + 40.0f);
	for (const auto& pin : definition->Pins)
	{
		const auto pinText = localization.GetTextForDisplay(pin.DisplayName);
		const char* label = pin.DisplayName.empty() ? pin.Key.c_str() : pinText.c_str();
		nodeWidth = std::max(nodeWidth, ImGui::CalcTextSize(label).x * 2.0f + 60.0f);
	}

	// 3. 노드 성격별 블루프린트 헤더 색상
	ImU32 headerColor = IM_COL32(40, 85, 145, 240); // 함수 호출: 블루
	if (node.Type.rfind("Event.", 0) == 0) headerColor = IM_COL32(185, 40, 40, 240); // 이벤트: 레드
	else if (node.Type.rfind("Variable.Get.", 0) == 0) headerColor = IM_COL32(30, 130, 125, 240); // 변수 Get: 청록
	else if (node.Type.rfind("Variable.Set.", 0) == 0) headerColor = IM_COL32(190, 100, 30, 240); // 변수 Set: 오렌지
	else if (execInputs.empty() && execOutputs.empty()) headerColor = IM_COL32(40, 135, 65, 240); // 순수 연산: 그린

	ed::BeginNode(found->second.Id);
	ImGui::PushID(static_cast<int>(node.Id));

	// ── 헤더 그룹 (노드 너비 확보 및 타이틀 출력) ──
	ImGui::BeginGroup();
	ImGui::TextUnformatted(name);
	if (ImGui::IsItemHovered() && !definition->Tooltip.empty())
	{
		ed::Suspend();
		ImGui::SetTooltip("%s", LOCAL_CSTR(definition->Tooltip));
		ed::Resume();
	}
	ImGui::Dummy(ImVec2(nodeWidth, 2.0f)); // 노드 최소 가로폭 보장
	ImGui::EndGroup();
	const float headerBottom = ImGui::GetItemRectMax().y + 4.0f;
	ImGui::Spacing();

	// ── 1. 최상단 Exec 행 (아이콘 정중앙 피벗 결합) ──
	const size_t execRows = std::max(execInputs.size(), execOutputs.size());
	for (size_t r = 0; r < execRows; ++r)
	{
		const GraphPin* inPin = (r < execInputs.size()) ? execInputs[r] : nullptr;
		const GraphPin* outPin = (r < execOutputs.size()) ? execOutputs[r] : nullptr;

		if (inPin)
		{
			const int id = found->second.Pins.at(inPin->Key);
			// ★ 아이콘만 핀으로 감싸고 중심 피벗 지정!
			ed::BeginPin(id, ed::PinKind::Input);
			ed::PinPivotAlignment(ImVec2(0.5f, 0.5f));
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ICON_FA_PLAY);
			ed::EndPin();

			ImGui::SameLine(0.0f, 6.0f);
			ImGui::TextUnformatted(inPin->Key.c_str());
		}

		if (outPin)
		{
			const float outWidth = ImGui::CalcTextSize(outPin->Key.c_str()).x + 30.0f;
			const float rightX = nodeWidth - outWidth;

			if (inPin) ImGui::SameLine();
			if (ImGui::GetCursorPosX() < rightX) ImGui::SetCursorPosX(rightX);

			ImGui::TextUnformatted(outPin->Key.c_str());
			ImGui::SameLine(0.0f, 6.0f);

			const int id = found->second.Pins.at(outPin->Key);
			// ★ 출력 Exec도 아이콘만 핀으로 감싸기!
			ed::BeginPin(id, ed::PinKind::Output);
			ed::PinPivotAlignment(ImVec2(0.5f, 0.5f));
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ICON_FA_PLAY);
			ed::EndPin();
		}
	}

	if (execRows > 0 && (!dataInputs.empty() || !dataOutputs.empty())) ImGui::Spacing();

	// ── 2. 하단 데이터 핀 (아이콘 정중앙 피벗 결합) ──
	const size_t dataRows = std::max(dataInputs.size(), dataOutputs.size());
	for (size_t r = 0; r < dataRows; ++r)
	{
		const GraphPin* inPin = (r < dataInputs.size()) ? dataInputs[r] : nullptr;
		const GraphPin* outPin = (r < dataOutputs.size()) ? dataOutputs[r] : nullptr;

		if (inPin)
		{
			const int id = found->second.Pins.at(inPin->Key);
			const ImVec4 pinColor = ImGui::ColorConvertU32ToFloat4(PinColor(inPin->Type));

			// ★ 아이콘만 핀으로 감싸기 (와이어가 원의 정중앙에 꽂힘!)
			ed::BeginPin(id, ed::PinKind::Input);
			ed::PinPivotAlignment(ImVec2(0.5f, 0.5f));
			ImGui::TextColored(pinColor, ICON_FA_CIRCLE);
			ed::EndPin();

			ImGui::SameLine(0.0f, 6.0f);
			ImGui::TextUnformatted(inPin->Key.c_str());

			DrawPinValue(node, *inPin, 80.0f);
		}

		if (outPin)
		{
			const float outWidth = ImGui::CalcTextSize(outPin->Key.c_str()).x + 30.0f;
			const float rightX = nodeWidth - outWidth;

			if (inPin) ImGui::SameLine();
			if (ImGui::GetCursorPosX() < rightX) ImGui::SetCursorPosX(rightX);

			ImGui::TextUnformatted(outPin->Key.c_str());
			ImGui::SameLine(0.0f, 6.0f);

			const int id = found->second.Pins.at(outPin->Key);
			const ImVec4 pinColor = ImGui::ColorConvertU32ToFloat4(PinColor(outPin->Type));
			ed::BeginPin(id, ed::PinKind::Output);
			ed::PinPivotAlignment(ImVec2(0.5f, 0.5f));
			ImGui::TextColored(pinColor, ICON_FA_CIRCLE);
			ed::EndPin();
		}
	}

	ImGui::PopID();
	ed::EndNode();

	// ── ★ 핵심: 노드의 실제 크기에 0.1px 오차 없이 완벽 밀착되는 헤더 배경 렌더링 ──
	if (ImGui::IsItemVisible())
	{
		const ImVec2 nodeMin = ImGui::GetItemRectMin();
		const ImVec2 nodeMax = ImGui::GetItemRectMax();
		const auto& style = ed::GetStyle();
		const float halfBorder = style.NodeBorderWidth * 0.5f;

		auto* drawList = ed::GetNodeBackgroundDrawList(found->second.Id);

		// 노드 상단 테두리에 딱 맞춘 헤더 사각형 채우기
		drawList->AddRectFilled(
			ImVec2(nodeMin.x + halfBorder, nodeMin.y + halfBorder),
			ImVec2(nodeMax.x - halfBorder, headerBottom),
			headerColor, style.NodeRounding, ImDrawFlags_RoundCornersTop);

		// 헤더와 본문 사이의 미려한 구분선
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
		const int from = FindPinId(link.From);
		const int to = FindPinId(link.To);
		if (from == 0 || to == 0) continue;
		// 핀 타입 조회
		const auto* fromNode = document.Data.FindNode(link.From.Node);
		const auto* pin = fromNode ? m_Registry.FindPin(fromNode->Type, link.From.Pin) : nullptr;
		const ImVec4 pinColor = ImGui::ColorConvertU32ToFloat4(PinColor(pin ? pin->Type : EGraphPinType::Float));
		ed::Link(found->second, from, to, pinColor, 2.5f);
		// 실행 중인 링크에는 실시간 와이어 펄스 애니메이션 적용
		const bool isFlowing = (m_Domain == EGraphDomain::Logic && m_TraceLink == link.Id);
		if (isFlowing) ed::Flow(found->second);
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

	// 2. 링크 삭제 처리 (BeginDelete가 true일 때만 내부에서 처리 및 EndDelete 호출)
	if (ed::BeginDelete())
	{
		ed::LinkId linkId;
		while (ed::QueryDeletedLink(&linkId))
		{
			if (ed::AcceptDeletedItem())
			{
				for (const auto& [id, edId] : document.Links)
				{
					if (edId == static_cast<int>(linkId.Get()))
					{
						if (ApplyResult(document.Data.Disconnect(id))) document.Links.erase(id);
						break;
					}
				}
			}
		}

		ed::EndDelete(); // <--- if (ed::BeginDelete()) 블록 내부로 이동
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
		if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
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

		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
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
		document.Data.SetInputValue(m_Registry, { node.Id, "Size" },
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
		document.Data.SetInputValue(m_Registry, { lastNodeId, "Size" },
			std::array<float, 2>{ commentSize.x, commentSize.y });
		document.CommentTexts[lastNodeId] = "Comment";
	}
}
#pragma endregion