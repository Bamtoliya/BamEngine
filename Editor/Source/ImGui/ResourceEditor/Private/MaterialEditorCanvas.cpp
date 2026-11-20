#include "MaterialEditor.h"
#include "NodeEditorUI.h"
#include "ShortcutManager.h"
#include "GraphMessageLocalization.h"
#include "InputManager.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include "ShaderGraphCompiler.h"
#include "GraphNodePalette.h"
#include <imgui_node_editor.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace ed = ax::NodeEditor;

namespace
{
	struct RestoreMaterialContext
	{
		ed::EditorContext* Previous = ed::GetCurrentEditor();
		~RestoreMaterialContext() { ed::SetCurrentEditor(Previous); }
	};

	float GetMaterialPinValueWidth(Engine::EGraphPinType type)
	{
		switch (type)
		{
		case Engine::EGraphPinType::Bool: return 24.0f;
		case Engine::EGraphPinType::Int: return 60.0f;
		case Engine::EGraphPinType::Float: return 60.0f;
		case Engine::EGraphPinType::Float2: return 120.0f;
		case Engine::EGraphPinType::Float3: return 170.0f;
		case Engine::EGraphPinType::Float4: return 220.0f;
		case Engine::EGraphPinType::String: return 120.0f;
		default: return 80.0f;
		}
	}
}

namespace Editor
{
	void MaterialEditor::DrawCenterCanvasPane(float width, float height)
	{
		if (ImGui::BeginChild("##MaterialCenterCanvasArea", ImVec2(width, height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			// 1. 상단 툴바
			EditorUI::ToolbarLayout toolbar;
			DrawShaderGraphToolbar(toolbar);

			// 2. 캔버스 및 하단 상태바 높이 계산
			const float statusBarHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
			const float canvasHeight = (std::max)(64.0f, ImGui::GetContentRegionAvail().y - statusBarHeight);

			// 3. 셰이더 그래프 캔버스
			DrawShaderGraphCanvas(canvasHeight);

			// 4. 하단 상태 바
			DrawShaderGraphStatusBar(statusBarHeight);
		}
		ImGui::EndChild();
	}

	void MaterialEditor::DrawShaderGraphToolbar(EditorUI::ToolbarLayout& toolbar)
	{
		// ToolbarLayout은 다음 항목을 자동으로 같은 줄에 두거나 줄을 바꾼다.
		// Button(id, icon, labelKey)는 아이콘과 로캘 라벨을 합쳐 그리므로 아이콘 자리에 문구를 넣지 않고, 그 앞에 수동 SameLine을 두지 않는다.
		if (toolbar.Button("CompileHotReload", ICON_FA_BOLT, "MaterialEditor.CompileHotReload"))
			LiveHotReloadShader();

		// 자동 컴파일 체크박스: 남은 폭에 들어갈 때만 같은 줄에 둔다.
		const auto autoCompileLabel = LOCAL("MaterialEditor.AutoCompile");
		EditorUI::SameLineIfFits(ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(autoCompileLabel.c_str()).x);
		if (ImGui::Checkbox(autoCompileLabel.c_str(), &m_AutoCompileEnabled) && m_AutoCompileEnabled)
			m_LiveCompileCooldown = 0.05f;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.AutoCompileTooltip"));

		// 컴파일 상태 표시
		const auto& build = m_ShaderGraph.ShaderBuild;
		const auto statusText = LOCAL(build.StatusKey.c_str());
		EditorUI::SameLineIfFits(ImGui::CalcTextSize(statusText.c_str()).x);
		ImGui::AlignTextToFramePadding();
		if (build.StatusKey == "MaterialEditor.CompilePassed")
			ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%s", statusText.c_str());
		else if (build.StatusKey == "MaterialEditor.CompileFailed")
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", statusText.c_str());
		else
			ImGui::TextDisabled("%s", statusText.c_str());
		if (ImGui::IsItemHovered() && !build.Details.empty())
		{
			// 컴파일러 메시지가 길어 툴팁이 화면 밖으로 늘어나지 않게 폭을 제한해 줄바꿈한다.
			ImGui::BeginTooltip();
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 45.0f);
			ImGui::TextUnformatted(build.Details.c_str());
			ImGui::PopTextWrapPos();
			ImGui::EndTooltip();
		}

		if (toolbar.Button("AddNode", ICON_FA_PLUS, "MaterialEditor.AddNode"))
			OpenAddNodeMenu();

		if (toolbar.Icon("FocusCanvas", ICON_FA_CROSSHAIRS, "MaterialEditor.ResetView"))
			ResetCanvasView();

		const char* wireIcons[] = { ICON_FA_BEZIER_CURVE, ICON_FA_MICROCHIP, ICON_FA_ARROWS_LEFT_RIGHT };
		if (toolbar.Icon("WireStyle", wireIcons[static_cast<int>(m_WireStyle)], "GraphPanel.WireStyleTooltip"))
		{
			m_WireStyle = static_cast<EWireStyle>((static_cast<int>(m_WireStyle) + 1) % 3);
		}
	}

	void MaterialEditor::DrawShaderGraphCanvas(float height)
	{
		if (!m_ShaderGraph.Context)
		{
			ed::Config config;
			config.SettingsName = nullptr;
			config.NavigateButtonIndex = 2; // 마우스 휠 클릭 드래그로 팬
			m_ShaderGraph.Context = NodeEditorUI::CreateDefaultEditor(&config);

			const RestoreMaterialContext restore;
			ed::SetCurrentEditor(m_ShaderGraph.Context);
			for (const auto& node : m_ShaderGraph.Data.GetNodes())
			{
				ed::SetNodePosition(ed::NodeId(node.Id), ImVec2(node.PositionX, node.PositionY));
			}
			m_NeedsNavigateToContent = true;
		}

		const RestoreMaterialContext restore;
		ed::SetCurrentEditor(m_ShaderGraph.Context);
		ed::EnableShortcuts(false);
		ed::GetStyle().LinkStrength = (m_WireStyle == EWireStyle::Straight) ? 0.0f : 100.0f;

		ed::Begin("##MaterialShaderCanvas", ImVec2(0.0f, height));

		// 저장된 위치는 노드를 그리기 전에 적용하고, 화면 맞춤은 노드 크기가 정해진 뒤(이번 프레임의 노드를 모두 그린 뒤)에 한다.
		// 노드를 그리기 전에 맞추면 노드 영역이 0이라 카메라가 엉뚱한 곳으로 가서 캔버스가 비어 보인다.
		const bool navigateToContent = m_NeedsNavigateToContent;
		if (navigateToContent)
		{
			for (const auto& node : m_ShaderGraph.Data.GetNodes())
			{
				ed::SetNodePosition(ed::NodeId(node.Id), ImVec2(node.PositionX, node.PositionY));
			}
			m_NeedsNavigateToContent = false;
		}

		ReserveEditorIds();

		for (const auto& node : m_ShaderGraph.Data.GetNodes())
		{
			if (node.Type == "System.Comment" || node.Type == "Core.Comment")
				DrawCommentNode(node);
			else if (node.Type == "System.Reroute" || node.Type.rfind("Core.Reroute", 0) == 0)
				DrawRerouteNode(node);
			else
				DrawNode(node);
		}

		DrawLinks();
		HandleConnections();
		ReadSelection();

		if (navigateToContent)
			ed::NavigateToContent(0.0f);

		// 컨텍스트 메뉴 감지는 ed::Begin과 ed::End 사이에서 수행
		bool openNodePopup = false;
		bool openBgPopup = false;
		ed::NodeId contextNodeId;
		if (ed::ShowNodeContextMenu(&contextNodeId))
		{
			m_SelectedNode = static_cast<Engine::GraphNodeId>(contextNodeId.Get());
			openNodePopup = true;
		}
		else if (ed::ShowBackgroundContextMenu())
		{
			const auto mouse = Engine::InputManager::Get().GetScreenMousePosition();
			m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
			m_MenuSourcePin = {};
			PrepareNodeMenu(m_MenuSourcePin);
			openBgPopup = true;
		}

		// 툴바·단축키로 요청된 노드 메뉴는 마우스 위치를 캔버스 좌표로 바꿔 추가 위치로 쓴다.
		if (m_OpenAddNodePopup && m_AddNodeAtMouse)
		{
			const auto mouse = Engine::InputManager::Get().GetScreenMousePosition();
			m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
		}
		m_AddNodeAtMouse = false;

		ed::Suspend();
		if (openNodePopup) ImGui::OpenPopup("##MaterialNodePopup");
		if (openBgPopup || m_OpenAddNodePopup) ImGui::OpenPopup("##MaterialAddNodePopup");
		m_OpenAddNodePopup = false;
		DrawNodeMenu();
		ed::Resume();

		ed::End();
	}

	void MaterialEditor::DrawShaderGraphStatusBar(float height)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
		if (ImGui::BeginChild("##MaterialStatusBar", ImVec2(0.0f, height), false, ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::Separator();
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.StatusHelp"));

			// 오른쪽 문구는 창 기준 오른쪽 끝에 맞춘다. 왼쪽 문구와 겹칠 만큼 좁으면 그리지 않는다(상태바는 한 줄 고정).
			const auto saveNote = LOCAL("MaterialEditor.StatusSaveNote");
			const float noteX = ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(saveNote.c_str()).x;
			const float afterHelpX = ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x + ImGui::GetStyle().ItemSpacing.x;
			if (noteX >= afterHelpX)
			{
				ImGui::SameLine(noteX);
				ImGui::TextDisabled("%s", saveNote.c_str());
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

	void MaterialEditor::DrawNode(const Engine::GraphNode& node)
	{
		const auto* schema = m_GraphRegistry.Find(node.Type);
		if (!schema) return;

		const bool isMaterialOutput = (node.Type == "Shader.MaterialOutput");
		auto& localization = LocalizationManager::Get();
		const std::string title = isMaterialOutput
			? LOCAL("ResourceType.Material")
			: (schema->DisplayName.empty() ? node.Type : localization.GetTextForDisplay(schema->DisplayName));
		std::vector<const Engine::GraphPin*> inputs;
		std::vector<const Engine::GraphPin*> outputs;
		bool hasExecutionPin = false;
		for (const auto& pin : schema->Pins)
		{
			hasExecutionPin |= pin.Type == Engine::EGraphPinType::Execution;
			if (pin.Direction == Engine::EGraphPinDirection::Input)
				inputs.push_back(&pin);
			else if (pin.Direction == Engine::EGraphPinDirection::Output)
				outputs.push_back(&pin);
		}

		const auto& links = m_ShaderGraph.Data.GetLinks();
		const auto isConnected = [&](const Engine::GraphPin& pin)
		{
			return std::any_of(links.begin(), links.end(),
				[&](const auto& link) { return link.To.Node == node.Id && link.To.Pin == pin.Key; });
		};
		const auto hasValueEditor = [](Engine::EGraphPinType type)
		{
			switch (type)
			{
			case Engine::EGraphPinType::Bool:
			case Engine::EGraphPinType::Float:
			case Engine::EGraphPinType::Float2:
			case Engine::EGraphPinType::Float3:
			case Engine::EGraphPinType::Float4:
			case Engine::EGraphPinType::String:
				return true;
			default:
				return false;
			}
		};

		constexpr float pinIconWidth = 18.0f;
		float maxInputWidth = 0.0f;
		for (const auto* pin : inputs)
		{
			const auto pinText = localization.GetTextForDisplay(pin->DisplayName);
			const char* label = pin->DisplayName.empty() ? pin->Key.c_str() : pinText.c_str();
			float width = ImGui::CalcTextSize(label).x + pinIconWidth + 85.0f;
			if (hasValueEditor(pin->Type))
				width = std::max(width, GetMaterialPinValueWidth(pin->Type));
			maxInputWidth = std::max(maxInputWidth, width);
		}

		float maxOutputWidth = 0.0f;
		for (const auto* pin : outputs)
		{
			const auto pinText = localization.GetTextForDisplay(pin->DisplayName);
			const char* label = pin->DisplayName.empty() ? pin->Key.c_str() : pinText.c_str();
			maxOutputWidth = std::max(maxOutputWidth, ImGui::CalcTextSize(label).x + pinIconWidth);
		}

		const float minNodeWidth = 140.0f;
		const float titleWidth = ImGui::CalcTextSize(title.c_str()).x + 24.0f;
		const float targetWidth = std::max({ minNodeWidth, titleWidth, maxInputWidth + maxOutputWidth + 20.0f });
		const ImU32 headerColor = hasExecutionPin ? NodeEditorUI::Colors::HeaderDefault : NodeEditorUI::Colors::HeaderFunction;
		std::vector<NodeEditorUI::BorderPin> borderPins;

		ed::BeginNode(ed::NodeId(node.Id));
		ImGui::PushID(static_cast<int>(node.Id));
		ImGui::BeginGroup();
		const float headerBottom = NodeEditorUI::BeginNodeHeader(title.c_str(), targetWidth);
		ImGui::Spacing();

		float leftColumnWidth = 0.0f;
		if (maxInputWidth > 0.0f)
		{
			ImGui::BeginGroup();
			for (const auto* pin : inputs)
			{
				const auto pinText = localization.GetTextForDisplay(pin->DisplayName);
				const char* label = pin->DisplayName.empty() ? pin->Key.c_str() : pinText.c_str();
				ImGui::TextUnformatted(label);

				if (!pin->LiteralOnly)
				{
					const float centerY = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
					borderPins.push_back({ FindPinId({ node.Id, pin->Key }), ed::PinKind::Input, centerY, PinColor(pin->Type) });
				}

				if (!isConnected(*pin))
					DrawPinValue(node, *pin);
			}
			ImGui::EndGroup();
			leftColumnWidth = ImGui::GetItemRectSize().x;
		}

		if (maxOutputWidth > 0.0f)
		{
			const float availableWidth = std::max(targetWidth, leftColumnWidth + maxOutputWidth + 24.0f);
			const float rightSpacing = std::max(16.0f, availableWidth - leftColumnWidth - maxOutputWidth);
			if (leftColumnWidth > 0.0f)
				ImGui::SameLine(0.0f, rightSpacing);
			else
			{
				ImGui::Dummy(ImVec2(rightSpacing, 1.0f));
				ImGui::SameLine(0.0f, 0.0f);
			}

			ImGui::BeginGroup();
			for (const auto* pin : outputs)
			{
				const auto pinText = localization.GetTextForDisplay(pin->DisplayName);
				const char* label = pin->DisplayName.empty() ? pin->Key.c_str() : pinText.c_str();
				const float indent = std::max(0.0f, maxOutputWidth - ImGui::CalcTextSize(label).x);
				if (indent > 0.0f) ImGui::Indent(indent);
				ImGui::TextUnformatted(label);
				const float centerY = (ImGui::GetItemRectMin().y + ImGui::GetItemRectMax().y) * 0.5f;
				borderPins.push_back({ FindPinId({ node.Id, pin->Key }), ed::PinKind::Output,
					centerY, PinColor(pin->Type) });
				if (indent > 0.0f) ImGui::Unindent(indent);
			}
			ImGui::EndGroup();
		}

		ImGui::EndGroup();
		if (!borderPins.empty())
			NodeEditorUI::DrawNodeBorderPins(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderPins);

		ImGui::PopID();
		ed::EndNode();

		NodeEditorUI::DrawNodeHeader(ed::NodeId(node.Id), ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), headerBottom, headerColor);
	}

	void MaterialEditor::DrawPinValue(const Engine::GraphNode& node, const Engine::GraphPin& pin)
	{
		const auto it = node.InputValues.find(pin.Key);
		const Engine::GraphValue& value = (it != node.InputValues.end()) ? it->second : pin.DefaultValue;

		ImGui::PushID(pin.Key.c_str());
		ImGui::SetNextItemWidth(GetMaterialPinValueWidth(pin.Type));

		switch (pin.Type)
		{
		case Engine::EGraphPinType::Bool:
		{
			bool val = std::holds_alternative<bool>(value) ? std::get<bool>(value) : false;
			if (ImGui::Checkbox("##pVal", &val))
			{
				(void)m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, val);
				CommitShaderGraphEdit();
			}
			break;
		}
		case Engine::EGraphPinType::Float:
		{
			float val = std::holds_alternative<float>(value) ? std::get<float>(value) : 0.0f;
			if (ImGui::DragFloat("##pVal", &val, 0.01f, 0.0f, 0.0f, "%.2f"))
			{
				(void)m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, val);
				CommitShaderGraphEdit();
			}
			break;
		}
		case Engine::EGraphPinType::Float2:
		{
			auto val = std::holds_alternative<std::array<float, 2>>(value) ? std::get<std::array<float, 2>>(value) : std::array<float, 2>{ 0.0f, 0.0f };
			if (ImGui::DragFloat2("##pVal", val.data(), 0.01f, 0.0f, 0.0f, "%.2f"))
			{
				(void)m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, val);
				CommitShaderGraphEdit();
			}
			break;
		}
		case Engine::EGraphPinType::Float3:
		{
			auto val = std::holds_alternative<std::array<float, 3>>(value) ? std::get<std::array<float, 3>>(value) : std::array<float, 3>{ 0.0f, 0.0f, 0.0f };
			if (ImGui::DragFloat3("##pVal", val.data(), 0.01f, 0.0f, 0.0f, "%.2f"))
			{
				(void)m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, val);
				CommitShaderGraphEdit();
			}
			break;
		}
		case Engine::EGraphPinType::Float4:
		{
			auto val = std::holds_alternative<std::array<float, 4>>(value) ? std::get<std::array<float, 4>>(value) : std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 0.0f };
			if (ImGui::DragFloat4("##pVal", val.data(), 0.01f, 0.0f, 0.0f, "%.2f"))
			{
				(void)m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, val);
				CommitShaderGraphEdit();
			}
			break;
		}
		case Engine::EGraphPinType::String:
		{
			std::string val = std::holds_alternative<std::string>(value) ? std::get<std::string>(value) : "";
			char buf[128];
			strncpy_s(buf, val.c_str(), sizeof(buf) - 1);
			if (ImGui::InputText("##pVal", buf, sizeof(buf)))
			{
				(void)m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, std::string(buf));
				CommitShaderGraphEdit();
			}
			break;
		}
		default: break;
		}

		ImGui::PopID();
	}

	void MaterialEditor::DrawLinks()
	{
		for (const auto& link : m_ShaderGraph.Data.GetLinks())
		{
			const auto fromPin = FindPinId(link.From);
			const auto toPin = FindPinId(link.To);
			if (fromPin != 0 && toPin != 0)
			{
				const auto* fromNode = m_ShaderGraph.Data.FindNode(link.From.Node);
				const auto* pin = fromNode ? m_GraphRegistry.FindPin(fromNode->Type, link.From.Pin) : nullptr;
				const ImU32 color = pin ? PinColor(pin->Type) : IM_COL32(200, 200, 200, 255);
				ed::Link(ed::LinkId(link.Id), ed::PinId(fromPin), ed::PinId(toPin), ImColor(color), 2.5f);
			}
		}
	}

	void MaterialEditor::HandleConnections()
	{
		// 노드 편집기의 핀 ID를 그래프 핀 참조와 방향으로 바꾼다.
		const auto resolvePin = [this](ed::PinId id, Engine::GraphPinReference& reference, Engine::EGraphPinDirection& direction)
		{
			for (const auto& node : m_ShaderGraph.Data.GetNodes())
			{
				const auto* definition = m_GraphRegistry.Find(node.Type);
				if (!definition) continue;
				for (const auto& pin : definition->Pins)
				{
					if (FindPinId({ node.Id, pin.Key }) != static_cast<int>(id.Get())) continue;
					reference = { node.Id, pin.Key };
					direction = pin.Direction;
					return true;
				}
			}
			return false;
		};

		// 1. 새 링크 생성. EndCreate()는 BeginCreate()가 true일 때만 호출해야 한다(라이브러리 assert).
		if (ed::BeginCreate())
		{
			// QueryNewLink의 첫 핀은 입력 핀이 아니라 드래그를 시작한 핀이다. 방향은 핀 정의로 판정한다.
			ed::PinId startPinId, endPinId;
			if (ed::QueryNewLink(&startPinId, &endPinId) && startPinId && endPinId)
			{
				Engine::GraphPinReference start, end;
				Engine::EGraphPinDirection startDirection{}, endDirection{};
				const bool resolved = resolvePin(startPinId, start, startDirection) && resolvePin(endPinId, end, endDirection);
				const bool startIsOutput = startDirection == Engine::EGraphPinDirection::Output;
				const auto& from = startIsOutput ? start : end;
				const auto& to = startIsOutput ? end : start;

				// 입력 핀에는 링크 하나만 둔다. 기존 연결을 끊고 새로 잇는 작업을 사본에서 먼저 확인해 실패하면 원본을 건드리지 않는다.
				Engine::Graph candidate = m_ShaderGraph.Data;
				bool valid = resolved && startDirection != endDirection && start.Node != end.Node;
				if (valid)
				{
					std::vector<Engine::GraphLinkId> replaced;
					for (const auto& link : candidate.GetLinks())
						if (link.To == to) replaced.push_back(link.Id);
					for (const auto linkId : replaced)
						valid = valid && candidate.Disconnect(linkId).Succeeded();
					valid = valid && candidate.Connect(m_GraphRegistry, from, to).Succeeded();
				}

				if (!valid)
				{
					ed::RejectNewItem(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 2.0f);
				}
				else if (ed::AcceptNewItem())
				{
					m_ShaderGraph.Data = std::move(candidate);
					CommitShaderGraphEdit();
				}
			}

			// 핀을 빈 곳에 놓으면 그 핀을 기준으로 노드 메뉴를 연다. 팝업은 생성 트랜잭션이 끝난 뒤 캔버스에서 연다.
			ed::PinId droppedPinId;
			if (ed::QueryNewNode(&droppedPinId) && ed::AcceptNewItem())
			{
				Engine::GraphPinReference source;
				Engine::EGraphPinDirection direction{};
				m_MenuSourcePin = resolvePin(droppedPinId, source, direction) ? source : Engine::GraphPinReference{};
				const auto mouse = Engine::InputManager::Get().GetScreenMousePosition();
				m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
				PrepareNodeMenu(m_MenuSourcePin);
				m_OpenAddNodePopup = true;
			}

			ed::EndCreate();
		}

		// 2. 링크·노드 삭제. 삭제 트랜잭션 안에서는 대상만 모으고, EndDelete() 뒤에 한 번에 지운 다음 이력 한 건으로 기록한다.
		if (ed::BeginDelete())
		{
			std::vector<Engine::GraphLinkId> linksToDelete;
			std::vector<Engine::GraphNodeId> nodesToDelete;

			ed::LinkId linkId;
			while (ed::QueryDeletedLink(&linkId))
			{
				if (ed::AcceptDeletedItem())
					linksToDelete.push_back(static_cast<Engine::GraphLinkId>(linkId.Get()));
			}

			ed::NodeId nodeId;
			while (ed::QueryDeletedNode(&nodeId))
			{
				if (ed::AcceptDeletedItem())
					nodesToDelete.push_back(static_cast<Engine::GraphNodeId>(nodeId.Get()));
			}
			ed::EndDelete();

			bool changed = false;
			for (const auto id : linksToDelete)
				changed |= m_ShaderGraph.Data.Disconnect(id).Succeeded();
			for (const auto id : nodesToDelete)
				changed |= m_ShaderGraph.Data.RemoveNode(id).Succeeded();
			if (changed)
			{
				m_SelectedNode = 0;
				m_SelectedLink = 0;
				CommitShaderGraphEdit();
			}
		}
	}

	void MaterialEditor::ReadSelection()
	{
		std::vector<ed::NodeId> selectedNodes(ed::GetSelectedObjectCount());
		std::vector<ed::LinkId> selectedLinks(ed::GetSelectedObjectCount());
		const int nodeCount = ed::GetSelectedNodes(selectedNodes.data(), static_cast<int>(selectedNodes.size()));
		const int linkCount = ed::GetSelectedLinks(selectedLinks.data(), static_cast<int>(selectedLinks.size()));

		m_SelectedNodes.clear();
		for (int i = 0; i < nodeCount; ++i)
			m_SelectedNodes.push_back(static_cast<Engine::GraphNodeId>(selectedNodes[i].Get()));

		m_SelectedLinks.clear();
		for (int i = 0; i < linkCount; ++i)
			m_SelectedLinks.push_back(static_cast<Engine::GraphLinkId>(selectedLinks[i].Get()));

		m_SelectedNode = m_SelectedNodes.empty() ? 0 : m_SelectedNodes.front();
		m_SelectedLink = m_SelectedLinks.empty() ? 0 : m_SelectedLinks.front();
	}

	void MaterialEditor::ReserveEditorIds()
	{
		// 노드 위치 동기화
		for (const auto& node : m_ShaderGraph.Data.GetNodes())
		{
			const auto pos = ed::GetNodePosition(ed::NodeId(node.Id));
			if (pos.x != FLT_MAX && pos.y != FLT_MAX && (pos.x != 0.0f || pos.y != 0.0f))
			{
				(void)m_ShaderGraph.Data.SetNodePosition(node.Id, pos.x, pos.y);
			}
		}
	}

	int MaterialEditor::FindPinId(const Engine::GraphPinReference& reference) const
	{
		if (reference.Node == 0 || reference.Pin.empty()) return 0;
		const uint32_t pinHash = std::hash<std::string>{}(reference.Pin) & 0x3FF;
		return static_cast<int>((reference.Node << 10) | pinHash);
	}

	std::pair<int, int> MaterialEditor::GetConnectionPins(const Engine::GraphLink& link) const
	{
		return { FindPinId(link.From), FindPinId(link.To) };
	}

	ImU32 MaterialEditor::PinColor(Engine::EGraphPinType type) const
	{
		switch (type)
		{
		case Engine::EGraphPinType::Bool:   return IM_COL32(230, 70, 70, 255);
		case Engine::EGraphPinType::Int:    return IM_COL32(70, 200, 230, 255);
		case Engine::EGraphPinType::Float:  return IM_COL32(150, 230, 70, 255);
		case Engine::EGraphPinType::Float2: return IM_COL32(230, 200, 70, 255);
		case Engine::EGraphPinType::Float3: return IM_COL32(230, 140, 70, 255);
		case Engine::EGraphPinType::Float4: return IM_COL32(200, 100, 230, 255);
		case Engine::EGraphPinType::String: return IM_COL32(230, 120, 180, 255);
		case Engine::EGraphPinType::Object: return IM_COL32(70, 120, 230, 255);
		default: return IM_COL32(180, 180, 180, 255);
		}
	}

	const char* MaterialEditor::PinTypeName(Engine::EGraphPinType type) const
	{
		switch (type)
		{
		case Engine::EGraphPinType::Bool:   return "Bool";
		case Engine::EGraphPinType::Int:    return "Int";
		case Engine::EGraphPinType::Float:  return "Float";
		case Engine::EGraphPinType::Float2: return "Float2";
		case Engine::EGraphPinType::Float3: return "Float3";
		case Engine::EGraphPinType::Float4: return "Float4";
		case Engine::EGraphPinType::String: return "String";
		case Engine::EGraphPinType::Object: return "Object";
		default: return "Unknown";
		}
	}

	void MaterialEditor::DrawCommentNode(const Engine::GraphNode& node)
	{
		ImVec2 groupSize(400.0f, 250.0f);
		const auto foundSize = node.InputValues.find("Size");
		if (foundSize != node.InputValues.end() && std::holds_alternative<std::array<float, 2>>(foundSize->second))
		{
			const auto size = std::get<std::array<float, 2>>(foundSize->second);
			groupSize = ImVec2(size[0], size[1]);
		}

		ed::PushStyleColor(ed::StyleColor_NodeBg, ImColor(255, 255, 255, 14));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImColor(255, 255, 255, 40));
		ed::PushStyleColor(ed::StyleColor_HovNodeBorder, ImColor(255, 255, 255, 120));
		ed::PushStyleColor(ed::StyleColor_SelNodeBorder, ImColor(255, 220, 60, 220));
		ed::PushStyleVar(ed::StyleVar_NodeRounding, 6.0f);
		ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 1.2f);
		ed::PushStyleVar(ed::StyleVar_GroupRounding, 6.0f);
		ed::PushStyleVar(ed::StyleVar_GroupBorderWidth, 1.2f);

		ed::BeginNode(ed::NodeId(node.Id));
		ImGui::PushID(static_cast<int>(node.Id));
		ImGui::BeginGroup();
		ImGui::TextUnformatted(node.Comment.empty() ? LOCAL_CSTR("EditorCommon.Comment") : node.Comment.c_str());
		ed::Group(groupSize);
		ImGui::EndGroup();
		ImGui::PopID();
		ed::EndNode();
		ed::PopStyleVar(4);
		ed::PopStyleColor(4);

		ed::NodeState state{};
		if (ed::GetNodeState(ed::NodeId(node.Id), &state) && state.GroupSize.x > 0.0f && state.GroupSize.y > 0.0f &&
			(std::abs(state.GroupSize.x - groupSize.x) > 0.5f || std::abs(state.GroupSize.y - groupSize.y) > 0.5f))
		{
			const auto result = m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, "Size" },
				std::array<float, 2>{ state.GroupSize.x, state.GroupSize.y });
			if (result.Succeeded()) CommitShaderGraphEdit();
		}
	}

	void MaterialEditor::CreateCommentBoxFromSelection()
	{
		// 단축키는 캔버스 밖에서 호출된다. 노드 위치를 다루기 전에 이 문서의 편집기를 현재 편집기로 둔다.
		if (!m_GraphReady || !m_ShaderGraph.Context) return;
		const RestoreMaterialContext restore;
		ed::SetCurrentEditor(m_ShaderGraph.Context);

		auto diag = m_ShaderGraph.Data.AddNode(m_GraphRegistry, "System.Comment");
		if (diag.Succeeded() && !m_ShaderGraph.Data.GetNodes().empty())
		{
			auto nodeId = m_ShaderGraph.Data.GetNodes().back().Id;
			ed::SetNodePosition(ed::NodeId(nodeId), m_AddPosition);
			CommitShaderGraphEdit();
		}
	}

	void MaterialEditor::DrawRerouteNode(const Engine::GraphNode& node)
	{
		const auto* schema = m_GraphRegistry.Find(node.Type);
		if (!schema) return;

		const auto* inPin = m_GraphRegistry.FindPin(node.Type, "In");
		const auto* outPin = m_GraphRegistry.FindPin(node.Type, "Out");
		if (!inPin || !outPin) return;

		const ImU32 pinColor = PinColor(inPin->Type);
		const bool straight = m_WireStyle == EWireStyle::Straight;
		if (straight) ed::PushStyleVar(ed::StyleVar_LinkStrength, 0.0f);
		ed::PushStyleColor(ed::StyleColor_NodeBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ed::PushStyleColor(ed::StyleColor_NodeBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ed::PushStyleColor(ed::StyleColor_HovNodeBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ed::PushStyleColor(ed::StyleColor_SelNodeBorder, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, 0.0f);
		ed::PushStyleVar(ed::StyleVar_HoveredNodeBorderWidth, 0.0f);
		ed::PushStyleVar(ed::StyleVar_SelectedNodeBorderWidth, 0.0f);
		ed::PushStyleVar(ed::StyleVar_NodePadding, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

		ed::BeginNode(ed::NodeId(node.Id));
		ImGui::PushID(static_cast<int>(node.Id));
		const ImVec4 dotColor = ed::IsNodeSelected(ed::NodeId(node.Id))
			? ImVec4(1.0f, 0.85f, 0.3f, 1.0f)
			: ImGui::ColorConvertU32ToFloat4(pinColor);
		ImGui::TextColored(dotColor, ICON_FA_CIRCLE);
		const ImVec2 dotMin = ImGui::GetItemRectMin();
		const ImVec2 dotMax = ImGui::GetItemRectMax();
		const ImVec2 dotCenter((dotMin.x + dotMax.x) * 0.5f, (dotMin.y + dotMax.y) * 0.5f);

		ed::BeginPin(ed::PinId(FindPinId({ node.Id, "In" })), ed::PinKind::Input);
		ed::PinRect(dotMin, ImVec2(dotCenter.x, dotMax.y));
		ed::PinPivotRect(dotCenter, dotCenter);
		ed::EndPin();
		ed::BeginPin(ed::PinId(FindPinId({ node.Id, "Out" })), ed::PinKind::Output);
		ed::PinRect(ImVec2(dotCenter.x, dotMin.y), dotMax);
		ed::PinPivotRect(dotCenter, dotCenter);
		ed::EndPin();
		ImGui::PopID();
		ed::EndNode();
		ed::PopStyleVar(4);
		ed::PopStyleColor(4);
		if (straight) ed::PopStyleVar();
	}

	void MaterialEditor::InsertRerouteNodeOnLink(Engine::GraphLinkId linkId)
	{
	}

	void MaterialEditor::AlignSelectedNodes(int alignment)
	{
		if (m_SelectedNodes.size() < 2 || !m_ShaderGraph.Context) return;
		const RestoreMaterialContext restore;
		ed::SetCurrentEditor(m_ShaderGraph.Context);
		float targetX = 0.0f, targetY = 0.0f;
		for (auto id : m_SelectedNodes)
		{
			const auto pos = ed::GetNodePosition(ed::NodeId(id));
			targetX = pos.x;
			targetY = pos.y;
			break;
		}
		for (auto id : m_SelectedNodes)
		{
			auto pos = ed::GetNodePosition(ed::NodeId(id));
			if (alignment == 0) pos.x = targetX; // 좌측 정렬
			else if (alignment == 1) pos.y = targetY; // 상단 정렬
			ed::SetNodePosition(ed::NodeId(id), pos);
		}
		CommitShaderGraphEdit();
	}

	void MaterialEditor::DuplicateSelection()
	{
		if (!m_GraphReady || !m_ShaderGraph.Context) return;
		const RestoreMaterialContext restore;
		ed::SetCurrentEditor(m_ShaderGraph.Context);

		std::vector<Engine::GraphNodeId> newNodes;
		for (auto id : m_SelectedNodes)
		{
			const auto* node = m_ShaderGraph.Data.FindNode(id);
			if (!node) continue;
			auto diag = m_ShaderGraph.Data.AddNode(m_GraphRegistry, node->Type);
			if (diag.Succeeded() && !m_ShaderGraph.Data.GetNodes().empty())
			{
				auto newId = m_ShaderGraph.Data.GetNodes().back().Id;
				const auto pos = ed::GetNodePosition(ed::NodeId(id));
				ed::SetNodePosition(ed::NodeId(newId), ImVec2(pos.x + 30.0f, pos.y + 30.0f));
				newNodes.push_back(newId);
			}
		}
		if (!newNodes.empty())
		{
			CommitShaderGraphEdit();
		}
	}

	void MaterialEditor::BreakAllNodeLinks(Engine::GraphNodeId nodeId)
	{
		std::vector<Engine::GraphLinkId> linksToRemove;
		for (const auto& link : m_ShaderGraph.Data.GetLinks())
		{
			if (link.From.Node == nodeId || link.To.Node == nodeId)
				linksToRemove.push_back(link.Id);
		}
		for (auto lId : linksToRemove)
			(void)m_ShaderGraph.Data.Disconnect(lId);

		CommitShaderGraphEdit();
	}

	void MaterialEditor::PrepareNodeMenu(const Engine::GraphPinReference& sourcePin)
	{
		m_NodeCandidates = FindGraphNodeCandidates(m_ShaderGraph.Data, m_GraphRegistry, sourcePin);
		m_NodeSearch[0] = '\0';
	}

	void MaterialEditor::DrawNodeMenu()
	{
		if (ImGui::BeginPopup("##MaterialAddNodePopup"))
		{
			ImGui::SetNextItemWidth(200.0f);
			ImGui::InputTextWithHint("##SearchNode", LOCAL_CSTR("GraphPanel.SearchNodes"), m_NodeSearch, sizeof(m_NodeSearch));
			ImGui::Separator();

			const std::string filter = Engine::ToLowerStr(m_NodeSearch);

			for (const auto& candidate : m_NodeCandidates)
			{
				const auto* def = m_GraphRegistry.Find(candidate.Type);
				const std::string displayName = def && !def->DisplayName.empty() ? def->DisplayName : candidate.Type;

				if (!filter.empty() && Engine::ToLowerStr(displayName).find(filter) == std::string::npos)
					continue;

				if (ImGui::Selectable(displayName.c_str()))
				{
					auto diag = AddGraphNodeCandidate(m_ShaderGraph.Data, m_GraphRegistry, candidate, m_MenuSourcePin);
					if (diag.Succeeded())
					{
						ed::SetNodePosition(ed::NodeId(diag.Node), m_AddPosition);
						CommitShaderGraphEdit();
					}
					ImGui::CloseCurrentPopup();
				}
			}

			ImGui::EndPopup();
		}

		if (ImGui::BeginPopup("##MaterialNodePopup"))
		{
			if (ImGui::MenuItem(LOCAL_CSTR("GraphPanel.BreakAllPinLinks")))
			{
				BreakAllNodeLinks(m_SelectedNode);
			}
			if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Delete")))
			{
				DeleteSelectedElements();
			}
			ImGui::EndPopup();
		}
	}

	void MaterialEditor::DeleteSelectedElements()
	{
		for (auto linkId : m_SelectedLinks)
			(void)m_ShaderGraph.Data.Disconnect(linkId);
		for (auto nodeId : m_SelectedNodes)
			(void)m_ShaderGraph.Data.RemoveNode(nodeId);

		m_SelectedLinks.clear();
		m_SelectedNodes.clear();
		m_SelectedNode = 0;
		m_SelectedLink = 0;
		CommitShaderGraphEdit();
	}

	void MaterialEditor::OpenAddNodeMenu()
	{
		// 툴바·단축키는 캔버스(ed::Begin~End) 밖에서 호출된다. ed::Suspend()는 캔버스 안에서만 쓸 수 있고,
		// 팝업도 캔버스의 ID 스택에서 열어야 DrawNodeMenu()의 BeginPopup과 맞으므로 요청만 남기고 다음 캔버스 그리기에서 연다.
		if (!m_GraphReady) return;
		m_MenuSourcePin = {};
		PrepareNodeMenu(m_MenuSourcePin);
		m_AddNodeAtMouse = true;
		m_OpenAddNodePopup = true;
	}

	void MaterialEditor::ResetCanvasView()
	{
		if (!m_ShaderGraph.Context) return;
		const RestoreMaterialContext restore;
		ed::SetCurrentEditor(m_ShaderGraph.Context);
		ed::NavigateToContent(0.5f);
	}
}
