#include "NodeEditorUI.h"
#include "Editor_Macros.h"
#include "GraphNodePalette.h"
#include "LocalizationManager.h"
#include <fmt/format.h>
#include <map>
#include <algorithm>

namespace Editor::NodeEditorUI
{
	PinMenuAction DrawPinMenu(const char* name, const char* type, bool canAdd, bool canBreak, bool canReset, bool canPromote)
	{
		using Engine::LocalizationManager;
		ImGui::TextDisabled("%s", LOCAL_FMT("GraphPanel.PinSummary", name, type).c_str());
		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.AddConnectedNode").c_str(), nullptr, false, canAdd))
			return PinMenuAction::AddConnectedNode;
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.DisconnectPin").c_str(), nullptr, false, canBreak))
			return PinMenuAction::BreakLinks;
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.ResetValue").c_str(), nullptr, false, canReset))
			return PinMenuAction::ResetValue;
		if (canPromote && ImGui::MenuItem(LOCAL_LABEL("GraphPanel.PromoteVariable").c_str()))
			return PinMenuAction::PromoteVariable;
		return PinMenuAction::None;
	}

	void DrawNodePalette(const Engine::GraphRegistry& registry, const std::vector<GraphNodeCandidate>& candidates,
		const char* search, const std::function<bool(const GraphNodeCandidate&)>& onSelect)
	{
		using Engine::LocalizationManager;
		struct NodeEntry
		{
			std::string Type;
			std::string DisplayName;
			std::string Tooltip;
			std::vector<std::string> CompatiblePins;
		};

		struct CategoryNode
		{
			std::string DisplayName;
			std::map<std::string, CategoryNode> SubCategories;
			std::vector<NodeEntry> Nodes;
		};

		const bool hasSearch = search[0] != '\0';
		CategoryNode root;

		for (const auto& candidate : candidates)
		{
			const auto* definition = registry.Find(candidate.Type);
			if (!definition) continue;
			std::string name = definition->DisplayName.empty() ? candidate.Type : LOCAL(definition->DisplayName);
			std::string categoryRaw = definition->Category.empty() ? "EditorCommon.General" : definition->Category;
			std::string label = LOCAL(categoryRaw) + " / " + name;
			if (!candidate.Pin.empty())
			{
				const auto* pin = registry.FindPin(candidate.Type, candidate.Pin);
				if (pin) label += " / " + (pin->DisplayName.empty() ? pin->Key : LOCAL(pin->DisplayName));
			}
			const std::string searchText = label + " " + LOCAL(definition->Tooltip);
			if (!MatchesGraphNodeSearch(*definition, candidate, search, searchText)) continue;

			if (candidate.Type.starts_with("Core.Reroute"))
			{
				categoryRaw = "Organization/Reroute";
				const auto separator = candidate.Type.rfind('.');
				name = LOCAL(candidate.Type == "Core.Reroute" ? "Execution" : candidate.Type.substr(separator + 1));
			}

			CategoryNode* current = &root;
			std::string_view remaining = categoryRaw;
			while (!remaining.empty())
			{
				const auto slash = remaining.find('/');
				const auto part = slash == std::string_view::npos ? remaining : remaining.substr(0, slash);
				std::string partStr(part == "CATEGORY_PHYSICS" ? "Physics" : part);
				auto& child = current->SubCategories[partStr];
				if (child.DisplayName.empty()) child.DisplayName = LOCAL(partStr);
				current = &child;
				if (slash == std::string_view::npos) break;
				remaining.remove_prefix(slash + 1);
			}

			auto nodeIt = std::find_if(current->Nodes.begin(), current->Nodes.end(),
				[&](const NodeEntry& n) { return n.Type == candidate.Type; });
			if (nodeIt == current->Nodes.end())
			{
				current->Nodes.push_back({ candidate.Type, name, definition->Tooltip, {} });
				nodeIt = current->Nodes.end() - 1;
			}
			if (!candidate.Pin.empty()) nodeIt->CompatiblePins.push_back(candidate.Pin);
		}

		ImGui::BeginChild("##NodeResults", ImVec2(ImGui::GetFontSize() * 28.0f, ImGui::GetTextLineHeightWithSpacing() * 18));

		auto DrawCategory = [&](auto& self, const CategoryNode& cat) -> void
			{
				for (const auto& [rawName, subCat] : cat.SubCategories)
				{
					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
					if (hasSearch) ImGui::SetNextItemOpen(true);

					if (ImGui::TreeNodeEx(rawName.c_str(), flags, "%s", subCat.DisplayName.c_str()))
					{
						self(self, subCat);
						ImGui::TreePop();
					}
				}

				for (const auto& nodeItem : cat.Nodes)
				{
					if (nodeItem.CompatiblePins.size() > 1)
					{
						ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
						if (hasSearch) ImGui::SetNextItemOpen(true);

						if (ImGui::TreeNodeEx((nodeItem.DisplayName + "###" + nodeItem.Type).c_str(), nodeFlags))
						{
							for (const auto& pinKey : nodeItem.CompatiblePins)
							{
								const auto* pin = registry.FindPin(nodeItem.Type, pinKey);
								const std::string pinLabel = pin && !pin->DisplayName.empty() ? LOCAL(pin->DisplayName) : pinKey;

								ImGui::PushID(nodeItem.Type.c_str());
								ImGui::PushID(pinKey.c_str());
								if (ImGui::Selectable(pinLabel.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups))
								{
									if (onSelect({ nodeItem.Type, pinKey })) ImGui::CloseCurrentPopup();
								}
								ImGui::PopID();
								ImGui::PopID();
							}
							ImGui::TreePop();
						}
					}
					else
					{
						const std::string pinKey = nodeItem.CompatiblePins.empty() ? "" : nodeItem.CompatiblePins.front();
						std::string displayLabel = nodeItem.DisplayName;
						if (!pinKey.empty())
						{
							const auto* pin = registry.FindPin(nodeItem.Type, pinKey);
							displayLabel += " (" + (pin && !pin->DisplayName.empty() ? LOCAL(pin->DisplayName) : pinKey) + ")";
						}

						ImGui::PushID(nodeItem.Type.c_str());
						ImGui::PushID(pinKey.c_str());
						if (ImGui::Selectable((displayLabel + "###Item").c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups))
						{
							if (onSelect({ nodeItem.Type, pinKey })) ImGui::CloseCurrentPopup();
						}
						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::TextUnformatted(nodeItem.Type.c_str());
							if (!nodeItem.Tooltip.empty()) ImGui::TextUnformatted(LOCAL_CSTR(nodeItem.Tooltip));
							ImGui::EndTooltip();
						}
						ImGui::PopID();
						ImGui::PopID();
					}
				}
			};

		const bool any = !root.SubCategories.empty() || !root.Nodes.empty();
		if (any) DrawCategory(DrawCategory, root);
		else ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.NoMatchingNodes"));

		ImGui::EndChild();
	}

	void ApplyDefaultTheme(ed::Style& style)
	{
		style.NodeRounding = 6.0f;
		style.NodeBorderWidth = 1.2f;
		style.NodePadding = ImVec4(10.0f, 6.0f, 10.0f, 8.0f);
		style.Colors[ed::StyleColor_NodeBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.95f);
		style.Colors[ed::StyleColor_NodeBorder] = ImVec4(0.24f, 0.25f, 0.28f, 1.0f);
		style.Colors[ed::StyleColor_HovNodeBorder] = ImVec4(0.95f, 0.70f, 0.20f, 1.0f);
		style.Colors[ed::StyleColor_SelNodeBorder] = ImVec4(1.0f, 0.85f, 0.30f, 1.0f);
		style.Colors[ed::StyleColor_Flow] = ImVec4(0.0f, 1.0f, 0.8f, 1.0f);
		style.Colors[ed::StyleColor_FlowMarker] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
		style.FlowSpeed = 160.0f;
		style.FlowMarkerDistance = 24.0f;
	}

	ed::EditorContext* CreateDefaultEditor(const ed::Config* userConfig)
	{
		ed::Config config;
		if (userConfig)
		{
			config = *userConfig;
		}
		else
		{
			config.SettingsName = nullptr;  // ini 파일 오염 방지
			config.NavigateButtonIndex = 2; // 휠 클릭 드래그로 화면 이동, 우클릭은 컨텍스트 메뉴 전용
		}

		auto* context = ed::CreateEditor(&config);
		auto* previous = ed::GetCurrentEditor();
		ed::SetCurrentEditor(context);
		ApplyDefaultTheme(ed::GetStyle());
		ed::SetCurrentEditor(previous);
		return context;
	}

	float BeginNodeHeader(const char* title, float minWidth, const char* badge, const ImVec4& badgeColor)
	{
		ImGui::BeginGroup();
		ImGui::TextUnformatted(title ? title : "");
		if (badge && badge[0] != '\0')
		{
			ImGui::SameLine(0.0f, 6.0f);
			ImGui::TextColored(badgeColor, "%s", badge);
		}
		const float titleW = ImGui::GetItemRectSize().x;
		const float finalW = std::max(minWidth, titleW + 24.0f);
		ImGui::Dummy(ImVec2(finalW, 2.0f));
		ImGui::EndGroup();
		return ImGui::GetItemRectMax().y + 4.0f;
	}

	void DrawNodeHeader(ed::NodeId nodeId, const ImVec2& nodeMin, const ImVec2& nodeMax, float headerBottom, ImU32 headerColor)
	{
		if (!ImGui::IsItemVisible()) return;

		const auto& style = ed::GetStyle();
		const float halfBorder = style.NodeBorderWidth * 0.5f;

		auto* drawList = ed::GetNodeBackgroundDrawList(nodeId);
		drawList->AddRectFilled(
			ImVec2(nodeMin.x + halfBorder, nodeMin.y + halfBorder),
			ImVec2(nodeMax.x - halfBorder, headerBottom),
			headerColor, style.NodeRounding, ImDrawFlags_RoundCornersTop);

		drawList->AddLine(
			ImVec2(nodeMin.x + halfBorder, headerBottom),
			ImVec2(nodeMax.x - halfBorder, headerBottom),
			IM_COL32(255, 255, 255, 35), 1.0f);
	}

	void DrawNodeBorderPins(const ImVec2& nodeMin, const ImVec2& nodeMax, const std::vector<BorderPin>& pins)
	{
		if (pins.empty()) return;

		const auto& style = ed::GetStyle();
		const float radius = ImGui::GetFontSize() * 0.36f;
		const float hitExtent = radius + 4.0f;

		// 핀의 중심이 노드 테두리 경계선에 정확히 놓이도록 하여 핀이 테두리에 반만 걸치게 배치
		const float leftX = nodeMin.x - style.NodePadding.x;
		const float rightX = nodeMax.x + style.NodePadding.z;

		auto* drawList = ImGui::GetWindowDrawList();

		for (const auto& pin : pins)
		{
			const bool isLeft = (pin.Side == BorderSide::Left);
			const float posX = isLeft ? leftX : rightX;
			const ImVec2 center(posX, pin.CenterY);

			// ★ 와이어가 노드 몸통을 가로지르지 않고 항상 노드 바깥쪽으로 둥글게 돌아가도록 핀 방향 벡터 강제!
			// 좌측 핀: In/Out 불문하고 항상 노드 바깥쪽인 왼쪽(-1, 0)을 향함
			// 우측 핀: In/Out 불문하고 항상 노드 바깥쪽인 오른쪽(+1, 0)을 향함
			const ImVec2 pinDir = isLeft ? ImVec2(-1.0f, 0.0f) : ImVec2(1.0f, 0.0f);
			if (pin.Kind == ed::PinKind::Output)
				ed::PushStyleVar(ed::StyleVar_SourceDirection, pinDir);
			else
				ed::PushStyleVar(ed::StyleVar_TargetDirection, pinDir);

			ed::BeginPin(pin.PinId, pin.Kind);
			ed::PinRect(ImVec2(center.x - hitExtent, center.y - hitExtent), ImVec2(center.x + hitExtent, center.y + hitExtent));
			ed::PinPivotRect(center, center);
			ed::EndPin();

			ed::PopStyleVar(1);

			drawList->AddCircleFilled(center, radius, pin.Color);
			// 1.8px 두께의 선명한 어두운 외곽선
			drawList->AddCircle(center, radius, IM_COL32(20, 20, 24, 255), 0, 1.8f);
		}
	}

	void DrawCirclePin(ed::PinId pinId, ed::PinKind kind, const ImVec2& center, float radius, ImU32 pinColor)
	{
		const float hitExtent = radius + 3.0f;
		ed::BeginPin(pinId, kind);
		ed::PinRect(ImVec2(center.x - hitExtent, center.y - hitExtent), ImVec2(center.x + hitExtent, center.y + hitExtent));
		ed::PinPivotRect(center, center);
		ed::EndPin();

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddCircleFilled(center, radius, pinColor);
		drawList->AddCircle(center, radius, IM_COL32(20, 20, 24, 255), 0, 1.5f);
	}

	void DrawIconPin(ed::PinId pinId, ed::PinKind kind, const char* icon, const ImVec4& color, const ImVec2& pivot)
	{
		ed::BeginPin(pinId, kind);
		ed::PinPivotAlignment(pivot);
		ImGui::TextColored(color, "%s", icon);
		ed::EndPin();
	}

	void DrawLink(ed::LinkId linkId, ed::PinId startPin, ed::PinId endPin, const ImVec4& color, float thickness, bool isFlowing)
	{
		ed::Link(linkId, startPin, endPin, color, thickness);
		if (isFlowing)
		{
			ed::Flow(linkId);
		}
	}
}
