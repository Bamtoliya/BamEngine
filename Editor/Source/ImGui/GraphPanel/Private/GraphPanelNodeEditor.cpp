#include "GraphPanel.h"
#include "LocalizationManager.h"
#include <imnodes.h>
#include <algorithm>
#include <limits>

#pragma region Context
void GraphPanel::ActivateDocument()
{
	auto& document = GetDocument();
	std::string layout;
	if (document.Context && document.ResetContext)
	{
		ImNodes::SetCurrentContext(document.Context);
		size_t length = 0;
		const char* data = ImNodes::SaveCurrentEditorStateToIniString(&length);
		layout.assign(data, length);
		ImNodes::DestroyContext(document.Context);
		document.Context = nullptr;
		document.RestoreSelection = true;
	}
	if (!document.Context) document.Context = ImNodes::CreateContext();
	ImNodes::SetCurrentContext(document.Context);
	if (!layout.empty()) ImNodes::LoadCurrentEditorStateFromIniString(layout.data(), layout.size());
	document.ResetContext = false;
}

void GraphPanel::SuspendDocument()
{
	m_Executor.Pause();
	// A hidden editor does not observe mouse releases. Reset its transient interaction on return.
	GetDocument().ResetContext = true;
	m_DragPin = 0;
	m_OpenNodePopup = false;
	m_NodeMenuActive = false;
	m_AddSource = {};
}
#pragma endregion

#pragma region DrawUI
void GraphPanel::DrawCanvas(float height)
{
	ActivateDocument();
	auto& document = GetDocument();
	const auto flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGui::BeginChild("##GraphCanvasHost", ImVec2(0.0f, height), ImGuiChildFlags_Borders, flags))
	{
		auto& style = ImNodes::GetStyle();
		style.Colors[ImNodesCol_GridBackground] = ImGui::GetColorU32(ImGuiCol_WindowBg);
		style.Colors[ImNodesCol_NodeBackground] = ImGui::GetColorU32(ImGuiCol_FrameBg);
		style.Colors[ImNodesCol_TitleBar] = ImGui::GetColorU32(ImGuiCol_Header);
		style.Colors[ImNodesCol_TitleBarSelected] = ImGui::GetColorU32(ImGuiCol_HeaderActive);

		ImNodes::BeginNodeEditor();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		for (const auto& node : document.Data.GetNodes()) DrawNode(node);
		DrawLinks();
		if (document.Data.GetNodes().empty()) ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.Empty"));
		const bool hovered = ImNodes::IsEditorHovered() && ImGui::IsWindowHovered();
		const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		ImNodes::EndNodeEditor();

		if (document.RestoreSelection)
		{
			for (auto id : document.SelectedNodes)
			{
				const auto found = document.Nodes.find(id);
				if (found != document.Nodes.end()) ImNodes::SelectNode(found->second.Id);
			}
			for (auto id : document.SelectedLinks)
			{
				const auto found = document.Links.find(id);
				if (found != document.Links.end()) ImNodes::SelectLink(found->second);
			}
			document.RestoreSelection = false;
		}
		if (document.PendingSelection != 0)
		{
			const auto found = document.Nodes.find(document.PendingSelection);
			if (found != document.Nodes.end())
			{
				ImNodes::ClearNodeSelection();
				ImNodes::ClearLinkSelection();
				ImNodes::SelectNode(found->second.Id);
			}
			document.PendingSelection = 0;
		}
		HandleConnections(origin, hovered);
		ReadSelection();
		const bool menuOpen = m_OpenNodePopup || ImGui::IsPopupOpen("##AddGraphNode") ||
			ImGui::IsPopupOpen("##GraphCanvasMenu");
		if (focused && !menuOpen && !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
			DeleteSelection();

		if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
		{
			int id = 0;
			ImNodes::ClearNodeSelection();
			ImNodes::ClearLinkSelection();
			if (ImNodes::IsNodeHovered(&id)) ImNodes::SelectNode(id);
			else if (ImNodes::IsLinkHovered(&id)) ImNodes::SelectLink(id);
			ReadSelection();
			const auto mouse = ImGui::GetIO().MousePos;
			const auto pan = ImNodes::EditorContextGetPanning();
			m_AddPosition = ImVec2(mouse.x - origin.x - pan.x, mouse.y - origin.y - pan.y);
			PrepareNodeMenu();
			ImGui::OpenPopup("##GraphCanvasMenu");
		}
		if (m_OpenNodePopup)
		{
			ImGui::OpenPopup("##AddGraphNode");
			m_OpenNodePopup = false;
		}
		if (ImGui::BeginPopup("##AddGraphNode"))
		{
			DrawNodeMenu();
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("##GraphCanvasMenu"))
		{
			DrawNodeMenu();
			ImGui::Separator();
			const bool selected = !document.SelectedNodes.empty() || !document.SelectedLinks.empty();
			if (ImGui::MenuItem(LOCAL_CSTR("GraphPanel.Delete"), "Delete", false, selected)) DeleteSelection();
			ImGui::EndPopup();
		}
		if (!ImGui::IsPopupOpen("##AddGraphNode") && !ImGui::IsPopupOpen("##GraphCanvasMenu"))
		{
			m_NodeMenuActive = false;
			m_AddSource = {};
		}
	}
	else
	{
		SuspendDocument();
	}
	ImGui::EndChild();
}

void GraphPanel::DrawNode(const GraphNode& node)
{
	const auto* definition = m_Registry.Find(node.Type);
	const auto found = GetDocument().Nodes.find(node.Id);
	if (!definition || found == GetDocument().Nodes.end()) return;
	const string name = definition->DisplayName.empty() ? node.Type : LOCAL(definition->DisplayName);
	float width = std::max(200.0f, ImGui::CalcTextSize(name.c_str()).x);
	for (const auto& pin : definition->Pins)
	{
		width = std::max(width, ImGui::CalcTextSize((pin.DisplayName.empty() ? pin.Key : LOCAL(pin.DisplayName)).c_str()).x);
		if (pin.Type == EGraphPinType::Float2 || pin.Type == EGraphPinType::Float3 || pin.Type == EGraphPinType::Float4)
			width = std::max(width, ImGui::GetFontSize() * 20.0f);
	}
	const bool traced = m_Domain == EGraphDomain::Logic && m_TraceNode == node.Id;
	if (traced)
	{
		const bool error = m_Executor.GetState() == EGraphExecutionState::Failed &&
			m_Executor.GetDiagnostic().Node == node.Id;
		const ImU32 color = error ? IM_COL32(190, 55, 55, 255) : IM_COL32(175, 115, 25, 255);
		ImNodes::PushColorStyle(ImNodesCol_TitleBar, color);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarHovered, color);
		ImNodes::PushColorStyle(ImNodesCol_TitleBarSelected, color);
		ImNodes::PushColorStyle(ImNodesCol_NodeOutline, IM_COL32(255, 190, 65, 255));
	}
	ImNodes::BeginNode(found->second.Id);
	ImNodes::BeginNodeTitleBar();
	ImGui::TextUnformatted(name.c_str());
	if (ImGui::IsItemHovered() && !definition->Tooltip.empty())
		ImGui::SetTooltip("%s", LOCAL_CSTR(definition->Tooltip));
	ImGui::Dummy(ImVec2(width, 0.0f));
	ImNodes::EndNodeTitleBar();
	for (const auto& pin : definition->Pins)
	{
		const int id = found->second.Pins.at(pin.Key);
		const bool input = pin.Direction == EGraphPinDirection::Input;
		const auto shape = pin.Type == EGraphPinType::Execution ?
			ImNodesPinShape_TriangleFilled : ImNodesPinShape_CircleFilled;
		ImNodes::PushColorStyle(ImNodesCol_Pin, PinColor(pin.Type));
		ImNodes::PushColorStyle(ImNodesCol_PinHovered, PinColor(pin.Type));
		if (input) ImNodes::BeginInputAttribute(id, shape);
		else ImNodes::BeginOutputAttribute(id, shape);
		const string label = pin.DisplayName.empty() ? pin.Key : LOCAL(pin.DisplayName);
		const float indent = input ? 0.0f : std::max(0.0f, width - ImGui::CalcTextSize(label.c_str()).x);
		if (indent > 0.0f) ImGui::Indent(indent);
		ImGui::TextUnformatted(label.c_str());
		if (indent > 0.0f) ImGui::Unindent(indent);
		DrawPinValue(node, pin, width);
		if (input) ImNodes::EndInputAttribute();
		else ImNodes::EndOutputAttribute();
		ImNodes::PopColorStyle();
		ImNodes::PopColorStyle();
	}
	ImNodes::EndNode();
	if (traced) for (int i = 0; i < 4; ++i) ImNodes::PopColorStyle();
}

void GraphPanel::DrawPinValue(const GraphNode& node, const GraphPin& pin, float width)
{
	if (pin.Type == EGraphPinType::Execution) return;
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
		if (missing)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.3f, 1.0f), "%s", LOCAL_CSTR("GraphPanel.RequiredValue"));
			value = MakeDefaultValue(pin.Type);
		}
		ImGui::SetNextItemWidth(width);
		if (DrawValue(pin.Type, value)) ApplyResult(graph.SetInputValue(m_Registry, reference, value));
		if (missing && ImGui::SmallButton(LOCAL_CSTR("GraphPanel.UseValue")))
			ApplyResult(graph.SetInputValue(m_Registry, reference, value));
		if (overridden && ImGui::SmallButton(LOCAL_CSTR("GraphPanel.ResetValue")))
			ApplyResult(graph.ResetInputValue(m_Registry, reference));
		ImGui::PopTextWrapPos();
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
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("GraphPanel.LastObserved"));
	}
	else
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.NoObservedValue"));
	}
	ImGui::PopTextWrapPos();
}

void GraphPanel::DrawLinks()
{
	const auto& document = GetDocument();
	for (const auto& link : document.Data.GetLinks())
	{
		const int from = FindPinId(link.From);
		const int to = FindPinId(link.To);
		const auto found = document.Links.find(link.Id);
		const auto* node = document.Data.FindNode(link.From.Node);
		const auto* pin = node ? m_Registry.FindPin(node->Type, link.From.Pin) : nullptr;
		if (from == 0 || to == 0 || found == document.Links.end() || !pin) continue;
		const bool traced = m_Domain == EGraphDomain::Logic && m_TraceLink == link.Id;
		ImNodes::PushColorStyle(ImNodesCol_Link, traced ? IM_COL32(255, 195, 65, 255) : PinColor(pin->Type));
		if (traced) ImNodes::PushStyleVar(ImNodesStyleVar_LinkThickness, 4.0f);
		ImNodes::Link(found->second, from, to);
		if (traced) ImNodes::PopStyleVar();
		ImNodes::PopColorStyle();
	}
}
#pragma endregion

#pragma region Connections&Selection
void GraphPanel::HandleConnections(const ImVec2& origin, bool canvasHovered)
{
	auto& document = GetDocument();
	int first = 0, second = 0;
	if (ImNodes::IsLinkStarted(&first)) m_DragPin = first;
	if (m_DragPin != 0 && ImNodes::IsPinHovered(&second) && second != m_DragPin)
	{
		GraphPinReference from, to;
		GetConnectionPins(m_DragPin, second, from, to);
		const auto result = document.Data.CanConnect(m_Registry, from, to);
		if (!result.Succeeded()) ImGui::SetTooltip("%s", LOCAL_CSTR(result.Message));
	}
	else if (m_DragPin == 0 && ImNodes::IsPinHovered(&second))
	{
		const auto binding = document.Pins.find(second);
		const auto* node = binding != document.Pins.end() ? document.Data.FindNode(binding->second.Node) : nullptr;
		const auto* pin = node ? m_Registry.FindPin(node->Type, binding->second.Pin) : nullptr;
		if (pin)
		{
			ImGui::BeginTooltip();
			ImGui::Text("%s : %s", pin->Key.c_str(),
				pin->Type == EGraphPinType::Object ? pin->ObjectType.c_str() : PinTypeName(pin->Type));
			if (const auto* observed = FindObservedValue(binding->second))
			{
				const auto value = TraceValueText(*observed);
				ImGui::Text("%s: %s", LOCAL_CSTR("GraphPanel.LastObserved"), value.c_str());
			}
			ImGui::EndTooltip();
		}
	}
	if (ImNodes::IsLinkCreated(&first, &second))
	{
		GraphPinReference from, to;
		GetConnectionPins(first, second, from, to);
		if (ReserveEditorIds(1))
		{
			const auto result = document.Data.Connect(m_Registry, from, to);
			if (ApplyResult(result)) document.Links.emplace(result.Link, static_cast<int>(document.NextId++));
		}
		m_DragPin = 0;
	}
	int hovered = 0;
	if (ImNodes::IsLinkDropped(&first, false) && ImGui::IsMouseReleased(ImGuiMouseButton_Left) &&
		canvasHovered && !ImGui::IsKeyDown(ImGuiKey_Escape) &&
		!ImNodes::IsNodeHovered(&hovered) && !ImNodes::IsPinHovered(&hovered) && !ImNodes::IsLinkHovered(&hovered))
	{
		const auto source = document.Pins.find(first);
		if (source != document.Pins.end())
		{
			const auto mouse = ImGui::GetIO().MousePos;
			const auto pan = ImNodes::EditorContextGetPanning();
			m_AddPosition = ImVec2(mouse.x - origin.x - pan.x, mouse.y - origin.y - pan.y);
			PrepareNodeMenu(source->second);
			m_OpenNodePopup = true;
		}
	}
	if (ImNodes::IsLinkDropped() || !ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_DragPin = 0;
}

void GraphPanel::ReadSelection()
{
	auto& document = GetDocument();
	document.SelectedNodes.clear();
	document.SelectedLinks.clear();
	std::vector<int> nodes(ImNodes::NumSelectedNodes());
	std::vector<int> links(ImNodes::NumSelectedLinks());
	if (!nodes.empty()) ImNodes::GetSelectedNodes(nodes.data());
	if (!links.empty()) ImNodes::GetSelectedLinks(links.data());
	for (const auto& [id, view] : document.Nodes)
		if (std::find(nodes.begin(), nodes.end(), view.Id) != nodes.end()) document.SelectedNodes.push_back(id);
	for (const auto& [id, view] : document.Links)
		if (std::find(links.begin(), links.end(), view) != links.end()) document.SelectedLinks.push_back(id);
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
