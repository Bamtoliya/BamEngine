#include "GizmoHelper.h"
#include "EditorSettingsService.h"
#include "InputManager.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

void GizmoHelper::BeginViewport(const CameraBuffer& camera, const ImVec2& screenOrigin, const ImVec2& size, bool isOrtho)
{
	s_Camera = camera;
	s_ScreenOrigin = screenOrigin;
	s_ViewportSize = size;
	s_IsOrtho = isOrtho;
}

GizmoResult GizmoHelper::Manipulate(const char* gizmoId, Engine::mat4& inOutWorldMatrix,
	const GizmoParams& params, bool& inOutWasUsingTracker)
{
	GizmoResult result{};
	if (s_ViewportSize.x <= 0.0f || s_ViewportSize.y <= 0.0f) return result;

	ImGuizmo::SetID(static_cast<int>(ImGui::GetID(gizmoId)));
	ImGuizmo::SetOrthographic(s_IsOrtho);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(s_ScreenOrigin.x, s_ScreenOrigin.y, s_ViewportSize.x, s_ViewportSize.y);

	// 환경설정 기반 스냅 수치 계산
	const auto& preferences = EditorSettingsService::Get().GetPreferences();
	float snapStep = params.CustomSnapStep;
	if (snapStep <= 0.0f)
	{
		if (params.Operation == ImGuizmo::ROTATE) snapStep = preferences.GizmoSnapRotation;
		else if (params.Operation == ImGuizmo::SCALE) snapStep = preferences.GizmoSnapScale;
		else snapStep = preferences.GizmoSnapTranslation;
	}

	auto& input = Engine::InputManager::Get();
	const bool useSnap = (params.UseSnap || input.IsCtrlPressed()) && std::isfinite(snapStep) && snapStep > 0.0f;
	const Engine::vec3 snapValues(snapStep);

	ImGuizmo::Enable(params.AllowInput);
	Engine::mat4 deltaMatrix(1.0f);
	const bool changed = ImGuizmo::Manipulate(
		glm::value_ptr(s_Camera.viewMatrix), glm::value_ptr(s_Camera.projMatrix),
		params.Operation, params.Mode, glm::value_ptr(inOutWorldMatrix),
		glm::value_ptr(deltaMatrix), useSnap ? glm::value_ptr(snapValues) : nullptr);

	result.IsUsing = params.AllowInput && ImGuizmo::IsUsing();
	result.IsOver = params.AllowInput && ImGuizmo::IsOver();
	ImGuizmo::Enable(true);

	// 드래그 상태 전이 판정
	if (result.IsUsing && !inOutWasUsingTracker) result.DragStarted = true;
	if (!result.IsUsing && inOutWasUsingTracker) result.DragEnded = true;
	inOutWasUsingTracker = result.IsUsing;

	if (changed)
	{
		result.Changed = true;
		const Engine::mat4 invParent = glm::inverse(params.ParentWorldMatrix);
		const Engine::mat4 localMatrix = invParent * inOutWorldMatrix;

		float translation[3]{}, rotation[3]{}, scale[3]{};
		ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), translation, rotation, scale);
		result.Position = Engine::vec3(translation[0], translation[1], translation[2]);
		result.Rotation = Engine::vec3(rotation[0], rotation[1], rotation[2]);
		result.Scale = Engine::vec3(scale[0], scale[1], scale[2]);
	}

	return result;
}

void GizmoHelper::DrawGizmoToolbar(IGizmoHost* host, const ImVec2& screenOrigin, const ImVec2& size, bool vertical)
{
	if (!host || size.x <= 0.0f || size.y <= 0.0f) return;

	const auto& sm = ShortcutManager::Get();
	const auto btnSize = EditorUI::GetIconButtonSize(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT);
	constexpr float pad = 4.0f;
	constexpr float spacing = 4.0f;
	constexpr float border = 1.0f;
	constexpr float separatorHeight = 6.0f;
	constexpr float gripHeight = 10.0f; // 최상단 드래그 핸들 높이

	const float toolbarWidth = vertical ?
		(btnSize.x + (pad + border) * 2.0f) :
		((btnSize.x + spacing) * 7.0f + separatorHeight + (pad + border) * 2.0f);

	const float toolbarHeight = vertical ?
		(gripHeight + spacing + btnSize.y * 7.0f + spacing * 6.0f + separatorHeight + (pad + border) * 2.0f) :
		(btnSize.y + (pad + border) * 2.0f);

	// 뷰포트 영역 밖으로 벗어나지 않도록 클램핑
	ImVec2 offset = host->GetGizmoToolbarOffset();
	offset.x = std::clamp(offset.x, 4.0f, std::max(4.0f, size.x - toolbarWidth - 4.0f));
	offset.y = std::clamp(offset.y, 4.0f, std::max(4.0f, size.y - toolbarHeight - 4.0f));
	host->SetGizmoToolbarOffset(offset);

	const ImVec2 toolbarPos = ImVec2(screenOrigin.x + offset.x, screenOrigin.y + offset.y);
	const ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;

	ImGui::SetCursorScreenPos(toolbarPos);
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 0.85f));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));

	if (ImGui::BeginChild("##GizmoOverlayToolbarChild", ImVec2(toolbarWidth, toolbarHeight), ImGuiChildFlags_Borders, childFlags))
	{
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			SetActiveHost(host);

		// --- 드래그 핸들 (Grip) ---
		if (vertical)
		{
			ImGui::InvisibleButton("##ToolbarDragGrip", ImVec2(btnSize.x, gripHeight));
			auto& input = Engine::InputManager::Get();

			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
				if (input.IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
					host->SetGizmoToolbarOffset(ImVec2(10.0f, 54.0f)); // 더블클릭 시 기본 위치로 복귀
			}

			if (ImGui::IsItemActive() && input.IsMouseButtonPressed(Engine::EMouseButton::Left))
			{
				const auto delta = input.GetMouseDelta();
				host->SetGizmoToolbarOffset(ImVec2(offset.x + delta.x, offset.y + delta.y));
			}

			// 그립 라인 시각화 (중앙 2줄 선)
			const ImVec2 pMin = ImGui::GetItemRectMin();
			const ImVec2 pMax = ImGui::GetItemRectMax();
			auto* drawList = ImGui::GetWindowDrawList();
			const ImU32 gripColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
			const float midY = (pMin.y + pMax.y) * 0.5f;
			const float midX = (pMin.x + pMax.x) * 0.5f;
			constexpr float halfW = 8.0f;
			drawList->AddLine(ImVec2(midX - halfW, midY - 2.0f), ImVec2(midX + halfW, midY - 2.0f), gripColor, 1.5f);
			drawList->AddLine(ImVec2(midX - halfW, midY + 2.0f), ImVec2(midX + halfW, midY + 2.0f), gripColor, 1.5f);
		}

		auto drawToolBtn = [&](const char* id, const char* icon, const char* tooltipKey, const char* shortcutId, bool active) -> bool
			{
				EditorUI::IconButtonOptions opt;
				opt.SameLine = !vertical;
				opt.Active = active;
				opt.NoWrap = true;
				opt.Size = btnSize;
				opt.ShortcutHint = sm.GetShortcutHint(shortcutId);
				return EditorUI::IconButton(id, icon, tooltipKey, opt);
			};

		const auto curOp = host->GetGizmoOperation();
		const bool isLocal = (host->GetGizmoMode() == ImGuizmo::LOCAL);
		const bool isPivot = (host->GetGizmoPivotMode() == GizmoPivotMode::Pivot);
		const bool useSnap = host->GetGizmoUseSnap();

		// 1. [선택 Ctrl+1]
		if (drawToolBtn("ToolSelect", ICON_FA_ARROW_POINTER, "Action.Viewport.GizmoSelect", "Viewport.GizmoSelect", curOp == static_cast<ImGuizmo::OPERATION>(0)))
			host->SetGizmoOperation(static_cast<ImGuizmo::OPERATION>(0));

		// 2. [이동 Ctrl+2]
		if (drawToolBtn("ToolTranslate", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT, "Action.Viewport.GizmoTranslate", "Viewport.GizmoTranslate", curOp == ImGuizmo::TRANSLATE))
			host->SetGizmoOperation(ImGuizmo::TRANSLATE);

		// 3. [회전 Ctrl+3]
		if (drawToolBtn("ToolRotate", ICON_FA_ARROWS_ROTATE, "Action.Viewport.GizmoRotate", "Viewport.GizmoRotate", curOp == ImGuizmo::ROTATE))
			host->SetGizmoOperation(ImGuizmo::ROTATE);

		// 4. [스케일 Ctrl+4]
		if (drawToolBtn("ToolScale", ICON_FA_UP_RIGHT_AND_DOWN_LEFT_FROM_CENTER, "Action.Viewport.GizmoScale", "Viewport.GizmoScale", curOp == ImGuizmo::SCALE))
			host->SetGizmoOperation(ImGuizmo::SCALE);

		// 구분선
		if (vertical) ImGui::Separator();
		else { ImGui::SameLine(); ImGui::TextDisabled("|"); }

		// 5. [좌표계: Local/World (`)]
		if (drawToolBtn("ToolCoord", isLocal ? ICON_FA_CUBE : ICON_FA_GLOBE, isLocal ? "Viewport.Local" : "Viewport.World", "Viewport.ToggleCoord", false))
			host->ToggleGizmoMode();

		// 6. [피벗 기준: Pivot/Center (Z)]
		if (drawToolBtn("ToolPivot", isPivot ? ICON_FA_LOCATION_CROSSHAIRS : ICON_FA_CIRCLE_DOT, isPivot ? "Viewport.Pivot" : "Viewport.Center", "Viewport.TogglePivot", false))
			host->ToggleGizmoPivot();

		// 7. [스냅: Magnet (Shift+S)]
		if (drawToolBtn("ToolSnap", ICON_FA_MAGNET, "Settings.GizmoSnapping", "Viewport.ToggleSnap", useSnap))
			host->ToggleGizmoSnap();
	}
	ImGui::EndChild();

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor();
}

bool GizmoHelper::WorldToScreen(const Engine::vec3& worldPos, const Engine::mat4& viewProj,
	const ImVec2& screenOrigin, const ImVec2& size, ImVec2& outScreenPos)
{
	const Engine::vec4 clip = viewProj * Engine::vec4(worldPos, 1.0f);
	if (clip.w <= 0.0001f) return false;
	const Engine::vec3 ndc = Engine::vec3(clip) / clip.w;
	if (ndc.z < -1.0f || ndc.z > 1.0f) return false;
	outScreenPos.x = screenOrigin.x + (ndc.x * 0.5f + 0.5f) * size.x;
	outScreenPos.y = screenOrigin.y + ((1.0f - ndc.y) * 0.5f) * size.y;
	return true;
}

void GizmoHelper::DrawWireCircle(ImDrawList* drawList, const Engine::vec3& center,
	const Engine::vec3& uAxis, const Engine::vec3& vAxis, float radius,
	const Engine::mat4& viewProj, const ImVec2& screenOrigin, const ImVec2& size,
	ImU32 color, int segments, float thickness)
{
	ImVec2 prevPos{};
	bool hasPrev = false;
	for (int i = 0; i <= segments; ++i)
	{
		const float angle = (static_cast<float>(i) / static_cast<float>(segments)) * 3.14159265f * 2.0f;
		const Engine::vec3 pt = center + (uAxis * std::cos(angle) + vAxis * std::sin(angle)) * radius;
		ImVec2 screenPos{};
		if (WorldToScreen(pt, viewProj, screenOrigin, size, screenPos))
		{
			if (hasPrev) drawList->AddLine(prevPos, screenPos, color, thickness);
			prevPos = screenPos;
			hasPrev = true;
		}
		else hasPrev = false;
	}
}

void GizmoHelper::DrawWireSphere(ImDrawList* drawList, const Engine::vec3& center, float radius,
	const Engine::mat4& viewProj, const ImVec2& screenOrigin, const ImVec2& size,
	ImU32 color, int segments, float thickness)
{
	DrawWireCircle(drawList, center, Engine::vec3(1, 0, 0), Engine::vec3(0, 0, 1), radius, viewProj, screenOrigin, size, color, segments, thickness);
	DrawWireCircle(drawList, center, Engine::vec3(1, 0, 0), Engine::vec3(0, 1, 0), radius, viewProj, screenOrigin, size, color, segments, thickness);
	DrawWireCircle(drawList, center, Engine::vec3(0, 1, 0), Engine::vec3(0, 0, 1), radius, viewProj, screenOrigin, size, color, segments, thickness);
}

void GizmoHelper::DrawWireBox(ImDrawList* drawList, const Engine::vec3& center, const Engine::vec3& halfExtent,
	const Engine::mat4& orientation, const Engine::mat4& viewProj,
	const ImVec2& screenOrigin, const ImVec2& size, ImU32 color, float thickness)
{
	const Engine::vec3 corners[8] = {
		center + Engine::vec3(orientation * Engine::vec4(-halfExtent.x, -halfExtent.y, -halfExtent.z, 0.0f)),
		center + Engine::vec3(orientation * Engine::vec4(halfExtent.x, -halfExtent.y, -halfExtent.z, 0.0f)),
		center + Engine::vec3(orientation * Engine::vec4(halfExtent.x, -halfExtent.y,  halfExtent.z, 0.0f)),
		center + Engine::vec3(orientation * Engine::vec4(-halfExtent.x, -halfExtent.y,  halfExtent.z, 0.0f)),
		center + Engine::vec3(orientation * Engine::vec4(-halfExtent.x,  halfExtent.y, -halfExtent.z, 0.0f)),
		center + Engine::vec3(orientation * Engine::vec4(halfExtent.x,  halfExtent.y, -halfExtent.z, 0.0f)),
		center + Engine::vec3(orientation * Engine::vec4(halfExtent.x,  halfExtent.y,  halfExtent.z, 0.0f)),
		center + Engine::vec3(orientation * Engine::vec4(-halfExtent.x,  halfExtent.y,  halfExtent.z, 0.0f))
	};

	ImVec2 sc[8]{};
	bool valid[8]{};
	for (int i = 0; i < 8; ++i) valid[i] = WorldToScreen(corners[i], viewProj, screenOrigin, size, sc[i]);

	const int edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7}
	};

	for (const auto& edge : edges)
	{
		if (valid[edge[0]] && valid[edge[1]])
			drawList->AddLine(sc[edge[0]], sc[edge[1]], color, thickness);
	}
}

void GizmoHelper::DrawWireCone(ImDrawList* drawList, const Engine::vec3& apex, const Engine::vec3& direction,
	float length, float angleDegrees, const Engine::mat4& viewProj,
	const ImVec2& screenOrigin, const ImVec2& size, ImU32 color,
	int segments, float thickness)
{
	if (length <= 0.001f) return;
	const Engine::vec3 dir = (glm::length(direction) > 0.0001f) ? glm::normalize(direction) : Engine::vec3(0, 1, 0);
	const float rad = glm::radians(std::clamp(angleDegrees, 0.1f, 89.0f));
	const float baseRadius = length * std::tan(rad);
	const Engine::vec3 baseCenter = apex + dir * length;

	Engine::vec3 u = (std::abs(dir.y) < 0.99f) ? glm::normalize(glm::cross(dir, Engine::vec3(0, 1, 0)))
		: glm::normalize(glm::cross(dir, Engine::vec3(1, 0, 0)));
	Engine::vec3 v = glm::normalize(glm::cross(dir, u));

	// 1. 밑면 원
	DrawWireCircle(drawList, baseCenter, u, v, baseRadius, viewProj, screenOrigin, size, color, segments, thickness);

	// 2. 꼭짓점에서 밑면 4방향으로 뻗는 모서리 능선 및 중심축
	ImVec2 scApex{};
	if (WorldToScreen(apex, viewProj, screenOrigin, size, scApex))
	{
		const Engine::vec3 pts[4] = {
			baseCenter + u * baseRadius,
			baseCenter - u * baseRadius,
			baseCenter + v * baseRadius,
			baseCenter - v * baseRadius
		};
		for (const auto& pt : pts)
		{
			ImVec2 scPt{};
			if (WorldToScreen(pt, viewProj, screenOrigin, size, scPt))
				drawList->AddLine(scApex, scPt, color, thickness);
		}

		ImVec2 scBaseCenter{};
		if (WorldToScreen(baseCenter, viewProj, screenOrigin, size, scBaseCenter))
			drawList->AddLine(scApex, scBaseCenter, (color & 0x00FFFFFF) | 0x55000000, 1.0f);
	}
}
