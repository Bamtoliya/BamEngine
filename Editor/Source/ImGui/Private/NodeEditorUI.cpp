#include "NodeEditorUI.h"
#include <algorithm>

namespace Editor::NodeEditorUI
{
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

			if (pin.Connected)
			{
				drawList->AddCircleFilled(center, radius, pin.Color);
			}
			else
			{
				ImVec4 c = ImGui::ColorConvertU32ToFloat4(pin.Color);
				c.w *= 0.35f;
				drawList->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(c));
			}
			// 1.8px 두께의 선명한 어두운 외곽선
			drawList->AddCircle(center, radius, IM_COL32(20, 20, 24, 255), 0, 1.8f);
		}
	}

	void DrawCirclePin(ed::PinId pinId, ed::PinKind kind, const ImVec2& center, float radius, ImU32 pinColor, bool connected)
	{
		const float hitExtent = radius + 3.0f;
		ed::BeginPin(pinId, kind);
		ed::PinRect(ImVec2(center.x - hitExtent, center.y - hitExtent), ImVec2(center.x + hitExtent, center.y + hitExtent));
		ed::PinPivotRect(center, center);
		ed::EndPin();

		auto* drawList = ImGui::GetWindowDrawList();
		if (connected)
		{
			drawList->AddCircleFilled(center, radius, pinColor);
		}
		else
		{
			ImVec4 c = ImGui::ColorConvertU32ToFloat4(pinColor);
			c.w *= 0.35f;
			drawList->AddCircleFilled(center, radius, ImGui::ColorConvertFloat4ToU32(c));
		}
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
