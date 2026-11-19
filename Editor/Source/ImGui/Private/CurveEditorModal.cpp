#include "CurveEditorModal.h"
#include "EditorUI.h"
#include "LocalizationManager.h"
#include "InputManager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <limits>

namespace Editor
{
	void CurveEditorModal::Open(const std::string& title, Engine::FloatCurve* targetCurve, std::function<void()> onApplied)
	{
		if (!targetCurve) return;
		m_Title = title;
		m_TargetCurve = targetCurve;
		m_WorkingCurve = *targetCurve;
		m_OnApplied = std::move(onApplied);
		m_SelectedKeyIndex = m_WorkingCurve.IsEmpty() ? -1 : 0;
		m_IsOpen = true;
		m_JustOpened = true;
		FitView(m_WorkingCurve);
	}

	void CurveEditorModal::FitView(const Engine::FloatCurve& curve)
	{
		if (curve.IsEmpty())
		{
			m_ViewTimeMin = 0.0f; m_ViewTimeMax = 1.0f;
			m_ViewValueMin = 0.0f; m_ViewValueMax = 1.0f;
			return;
		}

		float tMin = curve.keys.front().time, tMax = curve.keys.back().time;
		float vMin = curve.keys.front().value, vMax = curve.keys.front().value;
		for (const auto& k : curve.keys)
		{
			tMin = std::min(tMin, k.time); tMax = std::max(tMax, k.time);
			vMin = std::min(vMin, k.value); vMax = std::max(vMax, k.value);
		}
		const float tSpan = std::max(tMax - tMin, 0.2f);
		const float vSpan = std::max(vMax - vMin, 0.2f);
		m_ViewTimeMin = std::min(0.0f, tMin - tSpan * 0.05f);
		m_ViewTimeMax = std::max(1.0f, tMax + tSpan * 0.05f);
		m_ViewValueMin = vMin - vSpan * 0.15f;
		m_ViewValueMax = vMax + vSpan * 0.15f;
	}

	bool CurveEditorModal::Draw()
	{
		if (!m_IsOpen) return false;

		const std::string popupId = "##CurveEditorModalWindow";
		if (m_JustOpened)
		{
			ImGui::OpenPopup(popupId.c_str());
			m_JustOpened = false;
		}

		ImGui::SetNextWindowSize(ImVec2(780.0f, 600.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSizeConstraints(ImVec2(680.0f, 520.0f), ImVec2(1600.0f, 1200.0f));
		bool applied = false;

		if (ImGui::BeginPopupModal(popupId.c_str(), nullptr, ImGuiWindowFlags_NoCollapse))
		{
			// 1. 상단 타이틀 (### ID 분리 처리) 및 힌트 텍스트
			std::string cleanTitle = m_Title;
			if (const size_t pos = cleanTitle.find("###"); pos != std::string::npos) cleanTitle = cleanTitle.substr(0, pos);
			ImGui::TextUnformatted(cleanTitle.c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("(%s)", LOCAL_CSTR("VFXEditor.CurveHintDouble"));

			ImGui::Separator();
			ImGui::Spacing();

			// 프리셋 빠른 적용 버튼들
			if (ImGui::SmallButton(LOCAL_CSTR("VFXEditor.CurvePresetLinear"))) { m_WorkingCurve = Engine::FloatCurve::CreateLinear(0.0f, 1.0f); FitView(m_WorkingCurve); }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("VFXEditor.CurvePresetEaseIn"))) { m_WorkingCurve = Engine::FloatCurve::CreateEaseIn(0.0f, 1.0f); FitView(m_WorkingCurve); }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("VFXEditor.CurvePresetEaseOut"))) { m_WorkingCurve = Engine::FloatCurve::CreateEaseOut(0.0f, 1.0f); FitView(m_WorkingCurve); }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("VFXEditor.CurvePresetEaseInOut"))) { m_WorkingCurve = Engine::FloatCurve::CreateEaseInOut(0.0f, 1.0f); FitView(m_WorkingCurve); }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("VFXEditor.CurvePresetSpike"))) { m_WorkingCurve = Engine::FloatCurve::CreateSpike(1.0f, 0.2f); FitView(m_WorkingCurve); }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("VFXEditor.FitView"))) { FitView(m_WorkingCurve); }

			ImGui::Spacing();

			// 2. 중앙 2D 베지어 캔버스 (세로 230px로 최적화)
			const float canvasW = ImGui::GetContentRegionAvail().x;
			const float canvasH = 230.0f;
			DrawGraphCanvas(m_WorkingCurve, canvasW, canvasH);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// 3. 하단 선택된 키프레임 정밀 편집 속성
			DrawKeyDetail(m_WorkingCurve);

			ImGui::Spacing();

			// 4. 하단 [취소] [확인] 액션 버튼 바
			constexpr EditorUI::DialogAction actions[] = {
				{ "EditorCommon.Cancel", true, false },
				{ "EditorCommon.OK", true, false }
			};
			const int actionIdx = EditorUI::DialogButtonBar("CurveModalButtons", actions, 2);
			if (actionIdx == 0) // 취소
			{
				m_IsOpen = false;
				ImGui::CloseCurrentPopup();
			}
			else if (actionIdx == 1) // 확인
			{
				applied = m_TargetCurve && !std::ranges::equal(m_TargetCurve->keys, m_WorkingCurve.keys,
					[](const Engine::CurveKey& lhs, const Engine::CurveKey& rhs) {
						return lhs.time == rhs.time && lhs.value == rhs.value && lhs.inTangent == rhs.inTangent &&
							lhs.outTangent == rhs.outTangent && lhs.interpMode == rhs.interpMode;
					});
				if (applied)
				{
					*m_TargetCurve = m_WorkingCurve;
					if (m_OnApplied) m_OnApplied();
				}
				m_IsOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
		else
		{
			m_IsOpen = false;
		}

		return applied;
	}

	bool CurveEditorModal::DrawGraphCanvas(Engine::FloatCurve& curve, float width, float height)
	{
		const float pad = 24.0f;
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 corner(origin.x + width, origin.y + height);
		const float innerW = std::max(width - pad * 2.0f, 10.0f);
		const float innerH = std::max(height - pad * 2.0f, 10.0f);

		const float tSpan = std::max(m_ViewTimeMax - m_ViewTimeMin, 1e-4f);
		const float vSpan = std::max(m_ViewValueMax - m_ViewValueMin, 1e-4f);

		const auto toScreen = [&](float t, float v)
			{
				return ImVec2(origin.x + pad + ((t - m_ViewTimeMin) / tSpan) * innerW,
					corner.y - pad - ((v - m_ViewValueMin) / vSpan) * innerH);
			};
		const auto toTime = [&](float x) { return m_ViewTimeMin + std::clamp((x - origin.x - pad) / innerW, 0.0f, 1.0f) * tSpan; };
		const auto toValue = [&](float y) { return m_ViewValueMin + std::clamp((corner.y - pad - y) / innerH, 0.0f, 1.0f) * vSpan; };

		ImGui::InvisibleButton("##CurveCanvas", ImVec2(width, height), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered();
		const bool active = ImGui::IsItemActive();

		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(origin, corner, IM_COL32(28, 30, 36, 255), 4.0f);
		drawList->AddRect(origin, corner, IM_COL32(60, 64, 75, 255), 4.0f);
		drawList->PushClipRect(origin, corner, true);

		// 격자선 및 기준선 그리기
		for (const float vGuide : { 0.0f, 0.5f, 1.0f })
		{
			const float y = toScreen(m_ViewTimeMin, vGuide).y;
			drawList->AddLine(ImVec2(origin.x, y), ImVec2(corner.x, y), vGuide == 0.0f ? IM_COL32(90, 95, 110, 255) : IM_COL32(45, 48, 58, 255));
			char buf[32]; snprintf(buf, sizeof(buf), "%.1f", vGuide);
			drawList->AddText(ImVec2(origin.x + 4.0f, y - 12.0f), IM_COL32(140, 145, 160, 200), buf);
		}
		for (const float tGuide : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
		{
			const float x = toScreen(tGuide, m_ViewValueMin).x;
			drawList->AddLine(ImVec2(x, origin.y), ImVec2(x, corner.y), tGuide == 0.0f || tGuide == 1.0f ? IM_COL32(80, 85, 100, 255) : IM_COL32(45, 48, 58, 255));
			char buf[32]; snprintf(buf, sizeof(buf), "%.2f", tGuide);
			drawList->AddText(ImVec2(x - 12.0f, corner.y - 18.0f), IM_COL32(140, 145, 160, 200), buf);
		}

		// 곡선 평가 및 렌더링
		constexpr int kSamples = 128;
		ImVec2 curvePoints[kSamples + 1];
		for (int i = 0; i <= kSamples; ++i)
		{
			const float t = m_ViewTimeMin + (tSpan * static_cast<float>(i) / kSamples);
			curvePoints[i] = toScreen(t, curve.Evaluate(t, 0.0f));
		}
		// 곡선 하단 반투명 실루엣
		const float zeroY = toScreen(m_ViewTimeMin, 0.0f).y;
		for (int i = 0; i < kSamples; ++i)
		{
			drawList->AddQuadFilled(ImVec2(curvePoints[i].x, zeroY), curvePoints[i],
				curvePoints[i + 1], ImVec2(curvePoints[i + 1].x, zeroY), IM_COL32(230, 160, 40, 35));
		}
		drawList->AddPolyline(curvePoints, kSamples + 1, IM_COL32(255, 195, 65, 255), 0, 2.2f);

		// 마우스 입력 수집
		auto& input = Engine::InputManager::Get();
		const auto mouse = input.GetScreenMousePosition();
		const float hitRadius = 8.0f;
		int hoveredKey = -1;
		for (int i = 0; hovered && i < static_cast<int>(curve.keys.size()); ++i)
		{
			const ImVec2 p = toScreen(curve.keys[i].time, curve.keys[i].value);
			if ((p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y) <= hitRadius * hitRadius)
			{
				hoveredKey = i;
				break;
			}
		}

		bool changed = false;
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) m_SelectedKeyIndex = hoveredKey;

		// 키프레임 드래그 (이웃 키 사이 시간 클램핑)
		if (active && m_SelectedKeyIndex >= 0 && input.IsMouseButtonDragging(Engine::EMouseButton::Left, 0.0f))
		{
			auto& key = curve.keys[m_SelectedKeyIndex];
			const float minT = (m_SelectedKeyIndex > 0) ? curve.keys[m_SelectedKeyIndex - 1].time + 0.001f : m_ViewTimeMin;
			const float maxT = (m_SelectedKeyIndex + 1 < static_cast<int>(curve.keys.size())) ? curve.keys[m_SelectedKeyIndex + 1].time - 0.001f : m_ViewTimeMax;
			const float newT = std::clamp(toTime(mouse.x), minT, std::max(minT, maxT));
			const float newV = toValue(mouse.y);
			if (newT != key.time || newV != key.value)
			{
				key.time = newT; key.value = newV; changed = true;
			}
		}

		// 빈 공간 더블클릭: 새 키프레임 생성
		if (hovered && hoveredKey < 0 && input.IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
		{
			m_SelectedKeyIndex = curve.AddKey(toTime(mouse.x), toValue(mouse.y));
			changed = true;
		}

		// 키프레임 우클릭 컨텍스트 메뉴
		if (hoveredKey >= 0 && ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			m_SelectedKeyIndex = hoveredKey;
			ImGui::OpenPopup("##KeyContextMenu");
		}
		if (ImGui::BeginPopup("##KeyContextMenu"))
		{
			if (m_SelectedKeyIndex >= 0 && m_SelectedKeyIndex < static_cast<int>(curve.keys.size()))
			{
				auto& key = curve.keys[m_SelectedKeyIndex];
				if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Delete")))
				{
					curve.RemoveKey(static_cast<std::size_t>(m_SelectedKeyIndex));
					m_SelectedKeyIndex = -1; changed = true;
				}
				ImGui::Separator();
				if (ImGui::MenuItem(LOCAL_CSTR("Engine::ECurveInterpMode.CubicBezier"), nullptr, key.interpMode == Engine::ECurveInterpMode::CubicBezier)) { key.interpMode = Engine::ECurveInterpMode::CubicBezier; changed = true; }
				if (ImGui::MenuItem(LOCAL_CSTR("Engine::ECurveInterpMode.Linear"), nullptr, key.interpMode == Engine::ECurveInterpMode::Linear)) { key.interpMode = Engine::ECurveInterpMode::Linear; changed = true; }
				if (ImGui::MenuItem(LOCAL_CSTR("Engine::ECurveInterpMode.Constant"), nullptr, key.interpMode == Engine::ECurveInterpMode::Constant)) { key.interpMode = Engine::ECurveInterpMode::Constant; changed = true; }
				ImGui::Separator();
				if (ImGui::MenuItem(LOCAL_CSTR("VFXEditor.FlattenTangents"))) { key.inTangent = 0.0f; key.outTangent = 0.0f; changed = true; }
			}
			ImGui::EndPopup();
		}

		// 키프레임 핸들 렌더링
		for (int i = 0; i < static_cast<int>(curve.keys.size()); ++i)
		{
			const ImVec2 p = toScreen(curve.keys[i].time, curve.keys[i].value);
			const bool isSelected = (i == m_SelectedKeyIndex);
			const bool isHovered = (i == hoveredKey);

			// 선택된 키 외곽 발광 링
			if (isSelected) drawList->AddCircle(p, 7.5f, IM_COL32(255, 255, 255, 255), 16, 2.0f);
			drawList->AddCircleFilled(p, isSelected ? 5.5f : (isHovered ? 5.0f : 4.0f),
				isSelected ? IM_COL32(255, 230, 80, 255) : (isHovered ? IM_COL32(255, 255, 255, 230) : IM_COL32(200, 205, 220, 220)));
		}

		drawList->PopClipRect();
		return changed;
	}

	bool CurveEditorModal::DrawKeyDetail(Engine::FloatCurve& curve)
	{
		if (m_SelectedKeyIndex < 0 || m_SelectedKeyIndex >= static_cast<int>(curve.keys.size()))
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.CurveEditHint"));
			return false;
		}

		auto& key = curve.keys[m_SelectedKeyIndex];
		const float minT = (m_SelectedKeyIndex > 0) ? curve.keys[m_SelectedKeyIndex - 1].time + 0.001f : m_ViewTimeMin;
		const float maxT = (m_SelectedKeyIndex + 1 < static_cast<int>(curve.keys.size())) ? curve.keys[m_SelectedKeyIndex + 1].time - 0.001f : m_ViewTimeMax;

		// 상단 헤더: 선택된 키 번호 및 우측 삭제 아이콘 버튼
		ImGui::Text(ICON_FA_KEY " %s #%d", LOCAL_CSTR("STRUCT_CURVE_KEY"), m_SelectedKeyIndex + 1);
		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
		if (EditorUI::SmallIconButton("DeleteKey", ICON_FA_TRASH, "EditorCommon.Delete") && curve.RemoveKey(static_cast<std::size_t>(m_SelectedKeyIndex)))
		{
			m_SelectedKeyIndex = -1;
			return true;
		}

		bool changed = false;
		if (EditorUI::BeginPropertyTable("KeyDetailTable", EditorUI::InspectorPropertyTableStyle()))
		{
			EditorUI::PropertyRow("PROP_TIME");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Time", &key.time, 0.01f, minT, std::max(minT, maxT), "%.3f")) changed = true;

			EditorUI::PropertyRow("PROP_VALUE");
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Value", &key.value, 0.01f, 0.0f, 0.0f, "%.3f")) changed = true;

			if (key.interpMode == Engine::ECurveInterpMode::CubicBezier)
			{
				EditorUI::PropertyRow("PROP_IN_TANGENT");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat("##InTan", &key.inTangent, 0.05f, 0.0f, 0.0f, "%.2f")) changed = true;

				EditorUI::PropertyRow("PROP_OUT_TANGENT");
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (ImGui::DragFloat("##OutTan", &key.outTangent, 0.05f, 0.0f, 0.0f, "%.2f")) changed = true;
			}

			EditorUI::PropertyRow("PROP_INTERP_MODE");
			ImGui::SetNextItemWidth(-FLT_MIN);
			const struct { Engine::ECurveInterpMode mode; const char* key; } modes[] = {
				{ Engine::ECurveInterpMode::Constant, "Engine::ECurveInterpMode.Constant" },
				{ Engine::ECurveInterpMode::Linear, "Engine::ECurveInterpMode.Linear" },
				{ Engine::ECurveInterpMode::CubicBezier, "Engine::ECurveInterpMode.CubicBezier" }
			};
			const char* currentLabel = "Unknown";
			for (const auto& m : modes) if (m.mode == key.interpMode) { currentLabel = LOCAL_CSTR(m.key); break; }
			if (ImGui::BeginCombo("##InterpMode", currentLabel))
			{
				for (const auto& m : modes)
				{
					const bool isSelected = (m.mode == key.interpMode);
					if (ImGui::Selectable(LOCAL_CSTR(m.key), isSelected))
					{
						key.interpMode = m.mode;
						changed = true;
					}
					if (isSelected) ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			EditorUI::EndPropertyTable();
		}

		return changed;
	}
}