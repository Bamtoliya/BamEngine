#include "ReflectionPropertyDrawer.h"
#include "EditorUI.h"
#include "InputManager.h"
#include "Functions.h"

#include "IconsFontAwesome7.h"
#include <reflection/Registry.h>
#include <imgui.h>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Editor_Macros.h"
#include "LocalizationManager.h"
#include "ReflectionRegistry.h"
#include "ReflectionInspectorSupport.h"
#include "EditCondition.h"
#include "PhysicsComponent.h"
#include <imgui_stdlib.h>
#include <archive/Unicode.h>
#include "ResourcePicker.h"
#include "SceneManager.h"
#include "Scene.h"
#include "SceneEntityReference.h"
#include "WorldComponents.h"
#include "FloatCurve.h"
#include "UITextComponent.h"
#include <algorithm>
#include <filesystem>
#include <unordered_set>
#include <vector>
#pragma push_macro("new")
#undef new
#include <imgui_internal.h>
#pragma pop_macro("new")


namespace
{
#if defined(_WIN32)
    int CALLBACK CollectInstalledFont(const LOGFONTW* font, const TEXTMETRICW*, DWORD, LPARAM context)
    {
        if (font->lfFaceName[0] == L'@') return 1;
        auto& fonts = *reinterpret_cast<std::vector<std::string>*>(context);
        std::string name;
        if (archive::detail::WideToUtf8(font->lfFaceName, name) && !name.empty() &&
            name.size() <= Engine::UITextComponent::MaxFontFamilyBytes)
            fonts.push_back(std::move(name));
        return 1;
    }
#endif

    std::vector<std::string> EnumerateInstalledFonts()
    {
        std::vector<std::string> fonts;
#if defined(_WIN32)
        if (HDC dc = CreateCompatibleDC(nullptr))
        {
            LOGFONTW query{};
            query.lfCharSet = DEFAULT_CHARSET;
            EnumFontFamiliesExW(dc, &query, CollectInstalledFont, reinterpret_cast<LPARAM>(&fonts), 0);
            DeleteDC(dc);
            std::ranges::sort(fonts);
            fonts.erase(std::unique(fonts.begin(), fonts.end()), fonts.end());
        }
#endif
        return fonts;
    }

    bool DrawInspectorFontFamily(std::string& value)
    {
        const auto preview = value.empty() ? LOCAL("Inspector.FontSystemDefault") : value;
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (!ImGui::BeginCombo("##Value", preview.c_str(), ImGuiComboFlags_HeightLarge))
        {
            ImGui::SetItemTooltip("%s", LOCAL_CSTR("Inspector.FontCanvasPreview"));
            return false;
        }

        static char search[128]{};
        static std::vector<std::string> fonts;
        EditorUI::SearchBarOptions options;
        options.FocusOnDraw = ImGui::IsWindowAppearing();
        if (options.FocusOnDraw)
        {
            search[0] = '\0';
            fonts = EnumerateInstalledFonts();
        }
        EditorUI::DrawSearchBar("FontSearch", search, sizeof(search), "Inspector.SearchFont", options);
        ImGui::Separator();

        bool changed = false;
        if (ImGui::BeginChild("##FontFamilies", ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 12.0f)))
        {
            if (ImGui::Selectable(LOCAL_LABEL("Inspector.FontSystemDefault").c_str(), value.empty()))
            {
                changed = !value.empty();
                value.clear();
                ImGui::CloseCurrentPopup();
            }

            const ImGuiTextFilter filter(search);
            for (const auto& name : fonts)
            {
                if (!filter.PassFilter(name.c_str())) continue;
                ImGui::PushID(name.c_str());
                if (ImGui::Selectable((name + "###Family").c_str(), name == value))
                {
                    changed = name != value;
                    value = name;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::EndCombo();
        return changed;
    }

    void DrawClippedStringTooltip(std::string_view text)
    {
        if (text.empty() || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;

        const float innerWidth = ImGui::GetItemRectSize().x - ImGui::GetStyle().FramePadding.x * 2.0f;
        if (ImGui::CalcTextSize(text.data(), text.data() + text.size()).x <= innerWidth) return;

        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
        ImGui::TextUnformatted(text.data(), text.data() + text.size());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    bool DrawInspectorText(std::string& value, bool readOnly = false)
    {
        if (readOnly) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        const ImGuiInputTextFlags flags = readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
        const bool edited = ImGui::InputText("##Value", &value, flags);
        if (readOnly) ImGui::PopStyleColor();
        DrawClippedStringTooltip(value);
        return !readOnly && edited;
    }

    bool DrawInspectorText(std::wstring& value, bool readOnly = false)
    {
        std::string text;
        if (!archive::detail::WideToUtf8(value, text)) return false;
        if (!DrawInspectorText(text, readOnly)) return false;
        return archive::detail::Utf8ToWide(text, value);
    }

    bool DrawInspectorPath(std::filesystem::path& value, bool readOnly = false)
    {
        const auto u8 = value.u8string();
        std::string text(u8.begin(), u8.end());
        if (!DrawInspectorText(text, readOnly)) return false;
        value = Engine::Utf8ToPath(text);
        return true;
    }

    template<typename Matrix>
    bool DrawInspectorMatrix(Matrix& value)
    {
        const int columns = value.length();
        const int rows = columns > 0 ? value[0].length() : 0;
        if (columns <= 0 || rows <= 0) return false;

        bool changed = false;
        bool valid = true;
        if (ImGui::BeginTable("##MatrixTable", columns, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame))
        {
            for (int row = 0; row < rows; ++row)
            {
                ImGui::TableNextRow();
                for (int column = 0; column < columns; ++column)
                {
                    ImGui::TableSetColumnIndex(column);
                    ImGui::PushID(row * columns + column);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    changed |= ImGui::DragFloat("##Cell", &value[column][row], 0.01f);
                    ImGui::PopID();
                    if (!std::isfinite(value[column][row])) valid = false;
                }
            }
            ImGui::EndTable();
        }
        return valid && changed;
    }

    bool DrawInspectorEnumValue(const reflection::Metadata& propMetadata, const reflection::EnumInfo& enumeration,
        const reflection::ValueView& current, bool canEdit,
        const std::function<bool(const reflection::EnumValue&)>& onCommit)
    {
        const auto value = enumeration.ReadValue(current);
        if (!value) return false;
        const auto bits = std::visit([](auto number) { return static_cast<std::uint64_t>(number); }, *value);
        auto candidate = bits;
        const auto* entry = enumeration.FindEntryByValue(*value);
		const auto enumText = [&](const std::string& name)
		{
			const auto key = enumeration.QualifiedName + "." + name;
			const auto localized = Editor::LocalizeInspectorText(key);
			// QualifiedName.Name 키가 등록되어 있으면 해당 번역을 쓰고, 없으면 C++ 식별자 이름(name)을 그대로 반환
			return localized == key ? name : localized;
		};
        // HiddenEntries에 '|'로 나열한 항목은 값은 유지하고 표시만 하지 않는다.
        const auto hiddenEntries = "|" + Editor::InspectorMetadataText(propMetadata, "HiddenEntries") + "|";
        const auto isHiddenEntry = [&](const std::string& name) { return hiddenEntries.find("|" + name + "|") != std::string::npos; };
        std::string label = entry ? enumText(entry->Name) : std::to_string(bits);
        const auto* metadata = enumeration.Metadata.Find("Bitmask");
        const auto* flag = metadata ? std::get_if<bool>(metadata) : nullptr;
        if (!entry && flag && *flag)
        {
            label.clear();
            std::uint64_t covered = 0;
            for (const auto& item : enumeration.Entries)
            {
                const auto mask = std::visit([](auto number) { return static_cast<std::uint64_t>(number); }, item.Value);
                if (!mask || (mask & (mask - 1)) || (bits & mask) != mask) continue;
                covered |= mask;
                if (isHiddenEntry(item.Name)) continue;
                if (!label.empty()) label += " | ";
                label += enumText(item.Name);
            }
            if (covered != bits || label.empty())
            {
                if (!label.empty()) label += " | ";
                label += std::to_string(bits & ~covered);
            }
        }
        bool changed = false;
        ImGui::BeginDisabled(!canEdit);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::BeginCombo("##Value", label.c_str()))
        {
            for (const auto& item : enumeration.Entries)
            {
                if (isHiddenEntry(item.Name)) continue;
                const auto mask = std::visit([](auto number) { return static_cast<std::uint64_t>(number); },
                    item.Value);
                bool selected = flag && *flag ? (mask == 0 ? candidate == 0 : (candidate & mask) == mask) :
                    candidate == mask;
                const auto text = enumText(item.Name);
                ImGui::PushID(item.Name.c_str());
                if (flag && *flag)
                {
                    if (ImGui::Checkbox(text.c_str(), &selected))
                    {
                        candidate = mask == 0 ? 0 : selected ? candidate | mask : candidate & ~mask;
                        changed = true;
                    }
                }
                else if (ImGui::Selectable(text.c_str(), selected)) { candidate = mask; changed = true; }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        if (!canEdit || !changed || bits == candidate) return false;
        const reflection::EnumValue edited = std::holds_alternative<std::int64_t>(*value) ?
            reflection::EnumValue(static_cast<std::int64_t>(candidate)) : reflection::EnumValue(candidate);
        return onCommit(edited);
    }

    bool DrawInspectorEnum(const reflection::PropertyInfo& property, const reflection::ObjectView& object,
        const reflection::EnumInfo& enumeration, const reflection::ValueView& current, bool canEdit)
    {
        return DrawInspectorEnumValue(property.Metadata, enumeration, current, canEdit,
            [&](const reflection::EnumValue& edited)
            {
                return property.TryWriteEnumValue(object, edited) == reflection::PropertyAccessError::None;
            });
    }

    template<typename Variant, std::size_t Index = 0>
    void SelectInspectorVariant(Variant& value, std::size_t index)
    {
        if constexpr (Index < std::variant_size_v<Variant>)
        {
            if (index == Index) value.template emplace<Index>();
            else SelectInspectorVariant<Variant, Index + 1>(value, index);
        }
    }

	bool DrawCurveInterpCombo(Engine::ECurveInterpMode& mode)
	{
		constexpr std::pair<Engine::ECurveInterpMode, const char*> modes[] = { { Engine::ECurveInterpMode::Constant, "Constant" }, { Engine::ECurveInterpMode::Linear, "Linear" }, { Engine::ECurveInterpMode::CubicBezier, "CubicBezier" } };
		const auto modeText = [](const char* name)
			{
				const std::string key = std::string("Engine::ECurveInterpMode.") + name;
				const auto localized = Editor::LocalizeInspectorText(key);
				return localized == key ? std::string(name) : localized;
			};

		const char* currentName = "?";
		for (const auto& [value, name] : modes) if (value == mode) currentName = name;

		bool changed = false;
		if (ImGui::BeginCombo("##Mode", modeText(currentName).c_str()))
		{
			for (const auto& [value, name] : modes)
			{
				if (ImGui::Selectable(modeText(name).c_str(), value == mode) && value != mode) { mode = value; changed = true; }
			}
			ImGui::EndCombo();
		}
		return changed;
	}

	// 커브 그래프의 축 범위. 최소 0~1이며 키와 곡선이 벗어나면 넓힌다.
	struct CurveAxisRange
	{
		float TimeMin = 0.0f, TimeMax = 1.0f, ValueMin = 0.0f, ValueMax = 1.0f;
	};

	CurveAxisRange ComputeCurveAxisRange(const Engine::FloatCurve& curve)
	{
		CurveAxisRange range;
		for (const auto& key : curve.keys)
		{
			range.TimeMin = std::min(range.TimeMin, key.time); range.TimeMax = std::max(range.TimeMax, key.time);
			range.ValueMin = std::min(range.ValueMin, key.value); range.ValueMax = std::max(range.ValueMax, key.value);
		}
		constexpr int probeCount = 32;
		for (int i = 0; i < probeCount; ++i)
		{
			const float value = curve.Evaluate(range.TimeMin + (range.TimeMax - range.TimeMin) * static_cast<float>(i) / (probeCount - 1), 0.0f);
			range.ValueMin = std::min(range.ValueMin, value); range.ValueMax = std::max(range.ValueMax, value);
		}
		return range;
	}

	// 그래프에서 키를 선택·드래그·추가·삭제한다. selected는 선택 키 인덱스(-1 = 없음). 바뀌면 true.
	bool DrawFloatCurveGraph(Engine::FloatCurve& curve, int& selected)
	{
		const float pad = ImGui::GetFontSize() * 0.5f;
		const float width = std::max(ImGui::GetContentRegionAvail().x, pad * 2.0f + 1.0f);
		const float height = ImGui::GetFrameHeight() * 5.0f;
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 corner(origin.x + width, origin.y + height);

		// 드래그 중에는 시작 시점의 축 범위를 유지한다.
		ImGuiStorage* storage = ImGui::GetStateStorage();
		const ImGuiID graphId = ImGui::GetID("##CurveGraph");
		const ImGuiID rangeIds[4] = { ImGui::GetID("##RangeT0"), ImGui::GetID("##RangeT1"), ImGui::GetID("##RangeV0"), ImGui::GetID("##RangeV1") };
		CurveAxisRange range = ComputeCurveAxisRange(curve);
		if (ImGui::GetActiveID() == graphId)
			range = { storage->GetFloat(rangeIds[0], range.TimeMin), storage->GetFloat(rangeIds[1], range.TimeMax), storage->GetFloat(rangeIds[2], range.ValueMin), storage->GetFloat(rangeIds[3], range.ValueMax) };

		const float innerW = width - pad * 2.0f, innerH = height - pad * 2.0f;
		const auto toScreen = [&](float t, float v)
			{ return ImVec2(origin.x + pad + (t - range.TimeMin) / (range.TimeMax - range.TimeMin) * innerW, corner.y - pad - (v - range.ValueMin) / (range.ValueMax - range.ValueMin) * innerH); };
		const auto toTime = [&](float x) { return range.TimeMin + std::clamp((x - origin.x - pad) / innerW, 0.0f, 1.0f) * (range.TimeMax - range.TimeMin); };
		const auto toValue = [&](float y) { return range.ValueMin + std::clamp((corner.y - pad - y) / innerH, 0.0f, 1.0f) * (range.ValueMax - range.ValueMin); };

		ImGui::InvisibleButton("##CurveGraph", ImVec2(width, height), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered();
		const bool dragging = ImGui::IsItemActive();
		if (!dragging)
		{
			storage->SetFloat(rangeIds[0], range.TimeMin); storage->SetFloat(rangeIds[1], range.TimeMax);
			storage->SetFloat(rangeIds[2], range.ValueMin); storage->SetFloat(rangeIds[3], range.ValueMax);
		}

		// 커서 아래 키 찾기
		auto& input = Engine::InputManager::Get();
		const auto mouse = input.GetScreenMousePosition();
		const float hitRadius = ImGui::GetFontSize() * 0.5f;
		int hoveredKey = -1;
		for (int i = 0; hovered&& i < static_cast<int>(curve.keys.size()); ++i)
		{
			const ImVec2 p = toScreen(curve.keys[i].time, curve.keys[i].value);
			if ((p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y) <= hitRadius * hitRadius) { hoveredKey = i; break; }
		}

		bool changed = false;
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) selected = hoveredKey;

		// 드래그: 시간은 이웃 키 사이로 제한해 키 순서를 유지한다.
		if (dragging && selected >= 0 && input.IsMouseButtonDragging(Engine::EMouseButton::Left, 0.0f))
		{
			auto& key = curve.keys[selected];
			const float minTime = selected > 0 ? curve.keys[selected - 1].time + 0.001f : range.TimeMin;
			const float maxTime = selected + 1 < static_cast<int>(curve.keys.size()) ? curve.keys[selected + 1].time - 0.001f : range.TimeMax;
			const float newTime = std::clamp(toTime(mouse.x), minTime, std::max(minTime, maxTime));
			const float newValue = toValue(mouse.y);
			if (newTime != key.time || newValue != key.value) { key.time = newTime; key.value = newValue; changed = true; }
		}

		// 빈 곳 더블클릭: 키 추가
		if (hovered && hoveredKey < 0 && input.IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
		{
			selected = curve.AddKey(toTime(mouse.x), toValue(mouse.y));
			changed = true;
		}

		// 키 우클릭: 컨텍스트 메뉴
		if (hoveredKey >= 0 && ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			selected = hoveredKey;
			ImGui::OpenPopup("##CurveKeyMenu");
		}
		if (ImGui::BeginPopup("##CurveKeyMenu"))
		{
			if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Delete")) && selected >= 0 && curve.RemoveKey(static_cast<std::size_t>(selected)))
			{
				selected = -1;
				changed = true;
			}
			ImGui::EndPopup();
		}

		if (hoveredKey >= 0 && !dragging)
			ImGui::SetTooltip("%.3f, %.3f", curve.keys[hoveredKey].time, curve.keys[hoveredKey].value);

		// 그리기
		auto* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(origin, corner, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding);
		drawList->PushClipRect(origin, corner, true);
		for (const float guide : { 0.0f, 1.0f })
		{
			const float y = toScreen(range.TimeMin, guide).y;
			drawList->AddLine(ImVec2(origin.x, y), ImVec2(corner.x, y), ImGui::GetColorU32(ImGuiCol_Border));
		}
		constexpr int sampleCount = 64;
		std::array<ImVec2, sampleCount> points{};
		for (int i = 0; i < sampleCount; ++i)
		{
			const float t = range.TimeMin + (range.TimeMax - range.TimeMin) * static_cast<float>(i) / (sampleCount - 1);
			points[i] = toScreen(t, curve.Evaluate(t, 0.0f));
		}
		drawList->AddPolyline(points.data(), sampleCount, ImGui::GetColorU32(ImGuiCol_PlotLines), ImDrawFlags_None, 1.5f);
		for (int i = 0; i < static_cast<int>(curve.keys.size()); ++i)
		{
			const bool emphasized = (i == selected || i == hoveredKey);
			drawList->AddCircleFilled(toScreen(curve.keys[i].time, curve.keys[i].value), emphasized ? 5.0f : 3.5f,
				ImGui::GetColorU32(emphasized ? ImGuiCol_PlotLinesHovered : ImGuiCol_Text));
		}
		drawList->PopClipRect();
		return changed;
	}

	// 선택한 키의 세부 값. 시간은 이웃 키 사이로 제한한다.
	bool DrawSelectedCurveKey(Engine::FloatCurve& curve, int& selected)
	{
		if (selected < 0)
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.CurveEditHint"));
			ImGui::PopTextWrapPos();
			return false;
		}

		auto& key = curve.keys[selected];
		const float minTime = selected > 0 ? curve.keys[selected - 1].time + 0.001f : std::numeric_limits<float>::lowest();
		const float maxTime = selected + 1 < static_cast<int>(curve.keys.size()) ? curve.keys[selected + 1].time - 0.001f : (std::numeric_limits<float>::max)();
		const auto row = [](const char* labelKey)
			{
				EditorUI::PropertyLabel(Editor::LocalizeInspectorText(labelKey).c_str());
				EditorUI::PropertyValue();
				ImGui::SetNextItemWidth(-FLT_MIN);
			};

		bool changed = false;
		if (EditorUI::BeginPropertyTable("##CurveKeyDetail", EditorUI::InspectorPropertyTableStyle()))
		{
			row("PROP_TIME"); changed |= ImGui::DragFloat("##Time", &key.time, 0.01f, minTime, std::max(minTime, maxTime), "%.3f", ImGuiSliderFlags_AlwaysClamp);
			row("PROP_VALUE"); changed |= ImGui::DragFloat("##Value", &key.value, 0.01f, 0.0f, 0.0f, "%.3f");
			row("PROP_IN_TANGENT"); changed |= ImGui::DragFloat("##In", &key.inTangent, 0.05f, 0.0f, 0.0f, "%.2f");
			row("PROP_OUT_TANGENT"); changed |= ImGui::DragFloat("##Out", &key.outTangent, 0.05f, 0.0f, 0.0f, "%.2f");
			row("PROP_INTERP_MODE"); changed |= DrawCurveInterpCombo(key.interpMode);
			EditorUI::EndPropertyTable();
		}

		if (EditorUI::SmallIconButton("RemoveKey", ICON_FA_TRASH, "EditorCommon.Delete") && curve.RemoveKey(static_cast<std::size_t>(selected)))
		{
			selected = -1;
			changed = true;
		}
		return changed;
	}

	// FloatCurve 전용 편집기. 선택 상태는 위젯 ID별 ImGuiStorage에 보관한다.
	bool DrawFloatCurveField(Engine::FloatCurve& curve)
	{
		ImGuiStorage* storage = ImGui::GetStateStorage();
		const ImGuiID selectionId = ImGui::GetID("##CurveSelection");
		int selected = storage->GetInt(selectionId, -1);
		if (selected >= static_cast<int>(curve.keys.size())) selected = -1;

		bool changed = DrawFloatCurveGraph(curve, selected);
		changed |= DrawSelectedCurveKey(curve, selected);
		storage->SetInt(selectionId, selected);
		return changed;
	}

    template<typename Variant>
    bool DrawColliderShape(Variant& value, const reflection::Registry* registry)
    {
        if (registry == nullptr || value.valueless_by_exception()) return false;
        const auto name = std::visit([&](const auto& shape)
        {
            const auto* type = registry->FindType<std::decay_t<decltype(shape)>>();
            return type ? Editor::InspectorMetadataText(type->Metadata, "DisplayName", type->QualifiedName) : "?";
        }, value);
        bool changed = false;
        if (ImGui::BeginCombo("##Shape", Editor::LocalizeInspectorText(name).c_str()))
        {
            [&]<std::size_t... I>(std::index_sequence<I...>)
            {
                ([&]
                {
                    const auto* type = registry->FindType<std::variant_alternative_t<I, Variant>>();
                    const auto key = type ? Editor::InspectorMetadataText(type->Metadata, "DisplayName",
                        type->QualifiedName) : std::to_string(I);
                    const auto label = Editor::LocalizeInspectorText(key);
                    if (ImGui::Selectable(label.c_str(), value.index() == I))
                    {
                        SelectInspectorVariant(value, I);
                        changed = true;
                    }
                }(), ...);
            }(std::make_index_sequence<std::variant_size_v<Variant>>{});
            ImGui::EndCombo();
        }
        changed |= std::visit([&](auto& shape)
        {
            const auto* type = registry->FindType<std::decay_t<decltype(shape)>>();
            return type && Editor::ReflectionPropertyDrawer::DrawPropertyTable(
                *type, reflection::ObjectView::From(shape), registry);
        }, value);
        return changed;
    }

	bool DrawInspectorEntityPicker(entt::entity& value, bool canEdit, bool hasMissingReference = false)
	{
		bool changed = false;
		Engine::Scene* scene = Engine::SceneManager::Get().GetCurrentScene();
		if (!scene) return false;
		auto& world = scene->GetRegistry();
		// 1. 현재 할당된 엔티티 이름 결정
		std::string displayName = hasMissingReference ? LOCAL("Inspector.EntityUnavailable") : LOCAL("EditorCommon.None");
		if (value != entt::null && world.valid(value))
		{
			if (const auto* nameComp = world.try_get<Engine::NameComponent>(value))
				displayName = fmt::format("[Entity] {}", Engine::WStrToStr(nameComp->name));
			else
				displayName = fmt::format("[Entity] ID: {}", entt::to_integral(value));
		}
		// 우측 테두리 잘림 방지를 위한 여백 및 버튼 크기 계산
		const float itemSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float clearBtnSize = ImGui::GetFrameHeight(); // 프레임 높이에 맞춘 정사각형 버튼
		const float rightPadding = 4.0f;                    // 테이블 셀 우측 경계선과의 안전 여백
		const float availWidth = ImGui::GetContentRegionAvail().x;

		const bool showClearBtn = (value != entt::null || hasMissingReference) && canEdit;
		const float buttonWidth = showClearBtn
			? std::max(20.0f, availWidth - clearBtnSize - itemSpacing - rightPadding)
			: std::max(20.0f, availWidth - rightPadding);

		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if (ImGui::Button(displayName.c_str(), ImVec2(buttonWidth, 0.0f)))
		{
			if (canEdit) ImGui::OpenPopup("SelectEntityPopup");
		}
		ImGui::PopStyleVar();

		// 2. HierarchyPanel 드래그 앤 드롭 수신 ("BAM_HIERARCHY_ENTITY")
		if (canEdit && ImGui::BeginDragDropTarget())
		{
			if (const auto* payload = ImGui::AcceptDragDropPayload("BAM_HIERARCHY_ENTITY"))
			{
				struct EntityDragPayload
				{
					Engine::Scene* Scene = nullptr;
					entt::entity Entity = entt::null;
					std::uint64_t PersistentId = 0;
				};
				if (payload->DataSize == sizeof(EntityDragPayload))
				{
					EntityDragPayload source{};
					std::memcpy(&source, payload->Data, sizeof(source));
					if (source.Scene == scene && world.valid(source.Entity))
					{
						value = source.Entity;
						changed = true;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// 3. 할당 해제 (Clear 'X') 버튼
		if (showClearBtn)
		{
			ImGui::SameLine();
			if (ImGui::Button("X##ClearEntity", ImVec2(clearBtnSize, clearBtnSize)))
			{
				value = entt::null;
				changed = true;
			}
		}
		// 4. 클릭 시 엔티티 검색 및 선택 팝업 (크기 제한 및 내부 스크롤 영역 확보)
		ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 160.0f), ImVec2(400.0f, 360.0f));
		ImGui::SetNextWindowSize(ImVec2(280.0f, 260.0f), ImGuiCond_Appearing);
		if (ImGui::BeginPopup("SelectEntityPopup"))
		{
			static char searchBuffer[128] = "";
			EditorUI::SearchBarOptions searchOptions;
			searchOptions.FocusOnDraw = ImGui::IsWindowAppearing();
			if (searchOptions.FocusOnDraw)
				searchBuffer[0] = '\0';
			EditorUI::DrawSearchBar("EntitySearch", searchBuffer, sizeof(searchBuffer), "Inspector.SearchEntity", searchOptions);
			ImGui::Separator();

			// 상단 검색창은 고정하고, 하단 목록만 스크롤되도록 Child 윈도우 구성
			if (ImGui::BeginChild("##EntityScrollRegion", ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_None))
			{
				if (ImGui::Selectable(LOCAL_LABEL("EditorCommon.None").c_str(), value == entt::null))
				{
					value = entt::null;
					changed = true;
					searchBuffer[0] = '\0';
					ImGui::CloseCurrentPopup();
				}

				auto view = world.view<entt::entity>();
				for (auto entity : view)
				{
					std::string name = fmt::format("Entity [{}]", entt::to_integral(entity));
					if (const auto* nameComp = world.try_get<Engine::NameComponent>(entity))
						name = Engine::WStrToStr(nameComp->name);

					if (searchBuffer[0] != '\0')
					{
						std::string lowerName = name;
						std::string lowerSearch = searchBuffer;
						std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
						std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
						if (lowerName.find(lowerSearch) == std::string::npos)
							continue;
					}

					if (ImGui::Selectable(name.c_str(), value == entity))
					{
						value = entity;
						changed = true;
						searchBuffer[0] = '\0';
						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::EndChild();
			}
			ImGui::EndPopup();
		}
		return changed;
	}
}

namespace
{
	std::string GetReflectionDrawerDisplayName(const reflection::Metadata& metadata, const std::string& fallback)
	{
		const auto* value = metadata.Find("DisplayName");
		const auto* key = value != nullptr ? std::get_if<std::string>(value) : nullptr;

		if (key == nullptr || key->empty())
		{
			return fallback;
		}

		return Editor::LocalizeInspectorText(*key);
	}

	std::string FormatReflectionDrawerValue(const reflection::ValueView& value, const reflection::Registry* registry)
	{
		if (const auto* text = value.Get<std::string>()) return *text;
		if (const auto* text = value.Get<std::wstring>()) return Engine::WStrToStr(*text);
		if (const auto* path = value.Get<std::filesystem::path>())
		{
			const auto u8 = path->u8string();
			return std::string(u8.begin(), u8.end());
		}
		if (const auto* number = value.Get<bool>()) return *number ? "true" : "false";
		if (const auto* entity = value.Get<entt::entity>())
			return *entity == entt::null ? "null" : std::to_string(entt::to_integral(*entity));
		if (const auto* number = value.Get<std::int64_t>()) return std::to_string(*number);
		if (const auto* number = value.Get<std::uint64_t>()) return std::to_string(*number);
		if (const auto* number = value.Get<float>()) return fmt::format("{}", *number);
		if (const auto* number = value.Get<double>()) return fmt::format("{}", *number);

		if (const auto* vector = value.Get<glm::vec3>())
		{
			return fmt::format("({}, {}, {})", vector->x, vector->y, vector->z);
		}
		if (const auto* vector = value.Get<glm::vec4>())
		{
			return fmt::format("({}, {}, {}, {})", vector->x, vector->y, vector->z, vector->w);
		}

		if (const auto* rotation = value.Get<glm::quat>())
		{
			return fmt::format("(w: {}, x: {}, y: {}, z: {})",
				rotation->w, rotation->x, rotation->y, rotation->z);
		}

		const auto* enumeration = registry != nullptr ? registry->FindEnum(value) : nullptr;

		if (enumeration != nullptr)
		{
			const auto number = enumeration->ReadValue(value);

			if (number)
			{
				if (const auto* entry = enumeration->FindEntryByValue(*number))
				{
					return entry->Name;
				}

				return std::visit([](auto stored) { return std::to_string(stored); }, *number);
			}
		}

		return Editor::LocalizeInspectorText("Inspector.ValuePreviewUnavailable");
	}

	bool GetReflectionDrawerBoolMetadata(const reflection::Metadata& metadata, const char* key, bool fallback)
	{
		const auto* value = metadata.Find(key);
		const auto* flag = value != nullptr ? std::get_if<bool>(value) : nullptr;

		return flag != nullptr ? *flag : fallback;
	}

	double GetReflectionDrawerNumberMetadata(const reflection::Metadata& metadata, const char* key, double fallback)
	{
		const auto* value = metadata.Find(key);
		if (value == nullptr) return fallback;

		double result = fallback;
		if (const auto* number = std::get_if<double>(value)) result = *number;
		else if (const auto* number = std::get_if<std::int64_t>(value)) result = static_cast<double>(*number);
		else if (const auto* number = std::get_if<std::uint64_t>(value)) result = static_cast<double>(*number);
		else if (const auto* text = std::get_if<std::string>(value))
		{
			try { result = std::stod(*text); }
			catch (...) { result = fallback; }
		}

		return std::isfinite(result) ? result : fallback;
	}

	template<typename T, typename DrawWidget>
	bool DrawReflectionPropertyEditor(const reflection::PropertyInfo& property, const reflection::ObjectView& object,
		const T& current, bool canEdit, DrawWidget drawWidget)
	{
		T candidate = current;

		ImGui::BeginDisabled(!canEdit);
		ImGui::SetNextItemWidth(-1.0f);
		const bool requested = drawWidget(candidate);
		ImGui::EndDisabled();

		if (!canEdit || !requested || candidate == current)
		{
			return false;
		}
		if constexpr (std::is_floating_point_v<T>)
		{
			if (!std::isfinite(candidate)) return false;
			const auto minimum = GetReflectionDrawerNumberMetadata(property.Metadata, "RangeMin",
				-static_cast<double>((std::numeric_limits<T>::max)()));
			const auto maximum = GetReflectionDrawerNumberMetadata(property.Metadata, "RangeMax",
				static_cast<double>((std::numeric_limits<T>::max)()));
			if (minimum <= maximum) candidate = static_cast<T>(std::clamp(static_cast<double>(candidate), minimum, maximum));
		}

		const auto error = property.TryWriteValue(object, reflection::ValueView::From(candidate));

		if (error != reflection::PropertyAccessError::None)
		{
			const auto message = reflection::ToString(error);
			ImGui::TextDisabled("%s", LOCAL_FMT("Inspector.WriteFailed", message).c_str());
			return false;
		}

		return true;
	}

	template<typename T>
	bool DrawReflectionScalarProperty(const reflection::PropertyInfo& property, const reflection::ObjectView& object,
		const T& current, bool canEdit, ImGuiDataType dataType)
	{
		const bool isFloating = std::is_floating_point_v<T>;
		const double defaultStep = isFloating ? 0.05 : 1.0;
		const float dragSpeed = static_cast<float>(GetReflectionDrawerNumberMetadata(property.Metadata, "Step", defaultStep));

		const bool hasMin = property.Metadata.Find("RangeMin") != nullptr;
		const bool hasMax = property.Metadata.Find("RangeMax") != nullptr;
		const bool isSlider = GetReflectionDrawerBoolMetadata(property.Metadata, "Slider", false);

		T minVal = (std::numeric_limits<T>::lowest)();
		T maxVal = (std::numeric_limits<T>::max)();

		if (hasMin) minVal = static_cast<T>(GetReflectionDrawerNumberMetadata(property.Metadata, "RangeMin", static_cast<double>(minVal)));
		if (hasMax) maxVal = static_cast<T>(GetReflectionDrawerNumberMetadata(property.Metadata, "RangeMax", static_cast<double>(maxVal)));

		const void* pMin = hasMin ? &minVal : nullptr;
		const void* pMax = hasMax ? &maxVal : nullptr;
		const char* format = isFloating ? (dragSpeed < 0.001f ? "%.4f" : "%.3f") : nullptr;

		// [리셋 값 조회]: ResetValue 우선, 없으면 Default 확인, 둘 다 없으면 0.0
		T resetValue = T{};
		bool hasReset = false;
		// 1. 컴포넌트의 CDO에서 C++ 코드 기본값(= 1.2f 등)을 먼저 읽어옴
				// 1. 컴포넌트의 CDO에서 C++ 코드 기본값(= 1.2f 등)을 먼저 읽어옴
		auto& reg = Engine::ReflectionRegistry::Get();
		if (const auto* type = reg.GetRegistry().FindType(object))
		{
			const auto defaultObject = reg.GetDefaultComponentObject(*type);
			if (defaultObject.IsValid())
			{
				const auto readDefault = property.TryReadValue(defaultObject);
				if (readDefault.Succeeded() && readDefault.Value.Is<T>())
				{
					resetValue = *readDefault.Value.Get<T>();
					hasReset = true;
				}
			}
		}
		// 2. 만약 CDO 조회가 안 되었고 RESET_VALUE/DEFAULT 매크로가 있다면 메타데이터 사용
		if (!hasReset && (property.Metadata.Find("ResetValue") || property.Metadata.Find("Default")))
		{
			resetValue = static_cast<T>(GetReflectionDrawerNumberMetadata(property.Metadata, "ResetValue",
				GetReflectionDrawerNumberMetadata(property.Metadata, "Default", 0.0)));
			hasReset = true;
		}
		return DrawReflectionPropertyEditor(property, object, current, canEdit,
			[=, &current](T& candidate)
			{
				bool changed = false;
				if (isSlider && pMin != nullptr && pMax != nullptr)
					changed = ImGui::SliderScalar("##Value", dataType, &candidate, &minVal, &maxVal, format);
				else
					changed = ImGui::DragScalar("##Value", dataType, &candidate, dragSpeed, pMin, pMax, format);
				// 우클릭 시 "Reset to Default" 메뉴 제공
				if (hasReset && ImGui::BeginPopupContextItem("ScalarSettings", ImGuiPopupFlags_MouseButtonRight))
				{
					if (ImGui::MenuItem(LOCAL_CSTR("Inspector.ResetToDefault"), nullptr, false, candidate != resetValue))
					{
						candidate = resetValue;
						changed = true;
					}
					ImGui::EndPopup();
				}
				return changed;
			});
	}
}

namespace
{
	bool IsEditConditionMet(const reflection::PropertyInfo& property, const reflection::TypeInfo& type,
		const reflection::ObjectView& object, const reflection::Registry* registry)
	{
		const auto result = Editor::EvaluateEditCondition(property, type, object, registry);
		if (!result.Error.empty())
		{
			static std::unordered_set<std::string> s_Reported;
			if (s_Reported.insert(type.QualifiedName + "." + property.Name).second)
				EDITOR_LOG_WARN("Invalid EDITCONDITION on {}.{}: {}", type.QualifiedName, property.Name, result.Error);
		}
		return result.Met;
	}
}

namespace
{
	template<typename T>
	bool DrawCommonPrimitiveProperties(const reflection::TypeInfo& type,
		std::span<const reflection::ObjectView> objects, const reflection::Registry* registry,
		std::span<const Editor::ReflectionObjectEditSettings* const> editSettings)
	{
		if (objects.empty() || !Editor::ReflectionPropertyDrawer::IsVisible(type)) return false;
		if (!editSettings.empty() && editSettings.size() != objects.size()) return false;

		bool changed = false;
		ImGui::PushID(type.QualifiedName.c_str());

		for (const auto& property : type.Properties)
		{
			if (!property.IsValueType<T>()) continue;
			if (!GetReflectionDrawerBoolMetadata(property.Metadata, "Visible", true)) continue;

			const bool applicable = std::all_of(objects.begin(), objects.end(), [&](const auto& object)
				{
					return IsEditConditionMet(property, type, object, registry);
				});
			if (!applicable) continue;

			ImGui::PushID(property.Name.c_str());
			const auto label = GetReflectionDrawerDisplayName(property.Metadata, property.Name);
			bool canEdit = property.CanWrite() &&
				GetReflectionDrawerBoolMetadata(property.Metadata, "Editable", false) &&
				!GetReflectionDrawerBoolMetadata(property.Metadata, "ReadOnly", false);

			std::vector<T> previous;
			previous.reserve(objects.size());
			bool readable = true;

			for (std::size_t i = 0; i < objects.size(); ++i)
			{
				const auto read = property.TryReadValue(objects[i]);
				const auto* value = read.Value.Get<T>();
				if (!read || !value)
				{
					readable = false;
					break;
				}

				previous.push_back(*value);
				canEdit &= !objects[i].IsReadOnly();

				const auto* settings = editSettings.empty() ? nullptr : editSettings[i];
				if (settings)
				{
					const auto vector = settings->Vectors.find(property.Name);
					canEdit &= !settings->ReadOnlyProperties.contains(property.Name) &&
						(vector == settings->Vectors.end() || !vector->second.Locked);
				}
			}

			if (!readable)
			{
				ImGui::TextUnformatted(label.c_str());
				ImGui::SameLine();
				ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.ValueUnavailable"));
				ImGui::PopID();
				continue;
			}

			const T first = previous.front();
			const bool mixed = std::any_of(previous.begin(), previous.end(), [&](T value)
				{
					return value != first;
				});

			T candidate = first;
			bool requested = false;
			bool validInput = true;
			const auto widgetLabel = label + "###Value";

			ImGui::BeginDisabled(!canEdit);
			if constexpr (std::is_same_v<T, bool>)
			{
				if (mixed) candidate = false;

				ImGui::PushItemFlag(ImGuiItemFlags_MixedValue, mixed);
				requested = ImGui::Checkbox(widgetLabel.c_str(), &candidate);
				ImGui::PopItemFlag();

				if (mixed && ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", LOCAL_CSTR("Inspector.MixedFlagsHint"));
			}
			else
			{
				char buffer[64]{};
				if (!mixed)
				{
					const auto text = fmt::format("{:.9g}", first);
					std::copy(text.begin(), text.end(), buffer);
				}

				const auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll |
					ImGuiInputTextFlags_CharsScientific;
				requested = ImGui::InputTextWithHint(widgetLabel.c_str(), LOCAL_CSTR("Inspector.MixedValues"),
					buffer, sizeof(buffer), flags);

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", LOCAL_CSTR("Inspector.MultiFloatHint"));

				if (requested && canEdit)
				{
					const char* begin = buffer;
					const char* end = buffer + std::strlen(buffer);
					if (*begin == '+') ++begin;

					validInput = begin != end && !(buffer[0] == '+' && *begin == '-');
					if (validInput)
					{
						const auto parsed = std::from_chars(begin, end, candidate, std::chars_format::general);
						validInput = parsed.ec == std::errc{} && parsed.ptr == end && std::isfinite(candidate);
					}

					const double limit = static_cast<double>((std::numeric_limits<float>::max)());
					const double minimum = std::max(-limit,
						GetReflectionDrawerNumberMetadata(property.Metadata, "RangeMin", -limit));
					const double maximum = std::min(limit,
						GetReflectionDrawerNumberMetadata(property.Metadata, "RangeMax", limit));

					validInput &= minimum <= maximum;
					if (validInput)
						candidate = static_cast<float>(std::clamp(static_cast<double>(candidate), minimum, maximum));
				}
			}
			ImGui::EndDisabled();

			auto* storage = ImGui::GetStateStorage();
			const ImGuiID errorId = ImGui::GetID("CommonError");

			if (canEdit && requested)
			{
				if (!validInput)
				{
					storage->SetInt(errorId, 1);
				}
				else
				{
					const auto result = Editor::TryWriteCommonInspectorValue(property, objects, candidate);
					storage->SetInt(errorId, result.Failed ? 2 : 0);
					changed |= result.Changed;
				}
			}

			const int error = storage->GetInt(errorId, 0);
			if (error == 1)
				ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.InvalidValue"));
			else if (error == 2)
				ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.MultiPropertyWriteFailed"));

			ImGui::PopID();
		}

		ImGui::PopID();
		return changed;
	}
}

namespace Editor
{
	bool ReflectionPropertyDrawer::DrawVectorComponents(const reflection::Metadata& metadata, std::span<float> value, ReflectionVectorEditSettings* editSettings)
	{
		const int componentCount = static_cast<int>(value.size());
		static const char* axisNames[] = { "X", "Y", "Z", "W" };

		static const ImVec4 axisColors[] =
		{
			ImVec4(0.8f, 0.1f, 0.15f, 1.0f),
			ImVec4(0.2f, 0.7f, 0.2f, 1.0f),
			ImVec4(0.1f, 0.3f, 0.8f, 1.0f),
			ImVec4(0.5f, 0.5f, 0.5f, 1.0f)
		};

		ImGui::PushID(componentCount);

		ImGuiStorage* storage = ImGui::GetStateStorage();
		const ImGuiID snapEnabledId = ImGui::GetID("SnapEnabled");
		const ImGuiID snapStepId = ImGui::GetID("SnapStep");

		bool snapEnabled = editSettings != nullptr ?
			editSettings->SnapEnabled : storage->GetBool(snapEnabledId, false);
		float snapStep = static_cast<float>(GetReflectionDrawerNumberMetadata(metadata, "SnapStep", 1.0));
		float dragSpeed = static_cast<float>(GetReflectionDrawerNumberMetadata(metadata, "Step", 0.1));
		float resetValue = static_cast<float>(GetReflectionDrawerNumberMetadata(metadata, "ResetValue", 0.0));

		if (!std::isfinite(snapStep) || snapStep <= 0.0f) snapStep = 1.0f;
		if (!std::isfinite(dragSpeed) || dragSpeed <= 0.0f) dragSpeed = 0.1f;
		if (!std::isfinite(resetValue)) resetValue = 0.0f;

		if (editSettings != nullptr)
		{
			if (editSettings->SnapStep > 0.0f)
			{
				snapStep = editSettings->SnapStep;
			}
		}
		else
		{
			snapStep = storage->GetFloat(snapStepId, snapStep);
		}

		if (!std::isfinite(snapStep) || snapStep <= 0.0f)
		{
			snapStep = 1.0f;
		}

		const float height = ImGui::GetFrameHeight();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float innerSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
		const float availableWidth = ImGui::GetContentRegionAvail().x;
		const float horizontalWidth = (availableWidth - spacing * static_cast<float>(componentCount - 1)) / static_cast<float>(componentCount);
		const bool horizontal = horizontalWidth >= height * 2.0f + innerSpacing * 2.0f + 48.0f;
		const float axisWidth = horizontal ? horizontalWidth : availableWidth;
		const float inputWidth = std::max(1.0f, axisWidth - height * 2.0f - innerSpacing * 2.0f);

		bool changed = false;

		ImGui::BeginGroup();

		for (int axis = 0; axis < componentCount; ++axis)
		{
			if (axis != 0 && horizontal)
			{
				ImGui::SameLine(0.0f, spacing);
			}

			ImGui::PushID(axis);
			ImGui::BeginGroup();

			const ImGuiID lockId = ImGui::GetID("Locked");
			const ImGuiID rawId = ImGui::GetID("RawValue");
			const ImGuiID activeId = ImGui::GetID("WasActive");
			const ImGuiID outputId = ImGui::GetID("LastOutput");

			bool locked = editSettings != nullptr ?
				editSettings->LockedAxes[axis] : storage->GetBool(lockId, false);
			const float current = value[axis];
			float candidate = current;
			bool resetRequested = false;

			const bool continueDrag = storage->GetBool(activeId, false) &&
				storage->GetFloat(outputId, current) == current;

			float rawValue = continueDrag ? storage->GetFloat(rawId, current) : current;

			ImGui::BeginDisabled(locked);

			const ImVec4 color = axisColors[axis];
			const ImVec4 hoverColor(std::min(color.x + 0.1f, 1.0f),
				std::min(color.y + 0.1f, 1.0f), std::min(color.z + 0.1f, 1.0f), 1.0f);

			ImGui::PushStyleColor(ImGuiCol_Button, color);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);

			resetRequested = ImGui::Button(axisNames[axis], ImVec2(height, height));

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", LOCAL_FMT("Inspector.AxisReset", axisNames[axis], resetValue).c_str());
			}

			ImGui::PopStyleColor(3);
			ImGui::SameLine(0.0f, innerSpacing);
			ImGui::SetNextItemWidth(inputWidth);

			bool requested = ImGui::DragFloat("##Value", &rawValue, dragSpeed, 0.0f, 0.0f,
				"%.3f", ImGuiSliderFlags_NoRoundToFormat);

			const bool active = ImGui::IsItemActive();
			const bool hovered = ImGui::IsItemHovered();

			if (!locked && hovered)
			{
				// Window의 스크롤 대신 이 입력칸이 휠을 사용합니다.
				ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

				if (InputManager::Get().GetMouseScrollDeltaY() != 0.0f)
				{
					const float wheelStep = snapEnabled ? snapStep : dragSpeed;
					const float multiplier = InputManager::Get().IsShiftPressed() ? 10.0f : 1.0f;

					rawValue += InputManager::Get().GetMouseScrollDeltaY() * wheelStep * multiplier;
					requested = true;
				}

				if (InputManager::Get().IsMouseButtonDown(EMouseButton::Middle))
				{
					resetRequested = true;
				}
			}

			if (!locked && requested)
			{
				double result = static_cast<double>(rawValue);

				if (snapEnabled)
				{
					const double step = static_cast<double>(snapStep);
					result = std::round(result / step) * step;
				}

				const float converted = static_cast<float>(result);

				if (std::isfinite(converted))
				{
					candidate = converted;
				}
			}

			if (!locked && resetRequested)
			{
				// 초기화 값은 스냅 간격과 관계없이 그대로 적용합니다.
				candidate = resetValue;
				rawValue = resetValue;
			}

			storage->SetFloat(rawId, std::isfinite(rawValue) ? rawValue : current);
			storage->SetFloat(outputId, candidate);
			storage->SetBool(activeId, !locked && active);

			ImGui::EndDisabled();
			ImGui::SameLine(0.0f, innerSpacing);

			
			const char* lockLabel = locked ? ICON_FA_LOCK "###AxisLock" : ICON_FA_LOCK_OPEN "###AxisLock";

			if (ImGui::Button(lockLabel, ImVec2(height, height)))
			{
				locked = !locked;

				if (editSettings != nullptr)
				{
					editSettings->LockedAxes[axis] = locked;
				}
				else
				{
					storage->SetBool(lockId, locked);
				}
			}

			if (ImGui::IsItemHovered())
			{
				const char* tooltipKey = locked ? "Inspector.UnlockAxis" : "Inspector.LockAxis";
				ImGui::SetTooltip("%s", LOCAL_FMT(tooltipKey, axisNames[axis]).c_str());
			}

			if (candidate != current)
			{
				value[axis] = candidate;
				changed = true;
			}

			ImGui::EndGroup();
			ImGui::PopID();
		}

		ImGui::EndGroup();

		if (ImGui::BeginPopupContextItem("VectorSettings", ImGuiPopupFlags_MouseButtonRight))
		{
			ImGui::TextUnformatted(LOCAL_CSTR("Inspector.EditSettings"));
			ImGui::Separator();

			ImGui::Checkbox(LOCAL_LABEL("Inspector.Snap").c_str(), &snapEnabled);

			float editedStep = snapStep;
			if (ImGui::InputFloat(LOCAL_LABEL("Inspector.SnapStep").c_str(), &editedStep) &&
				std::isfinite(editedStep) && editedStep > 0.0f)
			{
				snapStep = editedStep;
			}

			ImGui::Separator();
			ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.WheelHint"));
			ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.FastWheelHint"));
			ImGui::TextDisabled("%s", LOCAL_CSTR("Inspector.AxisResetHint"));

			ImGui::EndPopup();
		}

		if (editSettings != nullptr)
		{
			editSettings->SnapEnabled = snapEnabled;
			editSettings->SnapStep = snapStep;
		}
		else
		{
			storage->SetBool(snapEnabledId, snapEnabled);
			storage->SetFloat(snapStepId, snapStep);
		}

		ImGui::PopID();
		return changed;
	}

	bool ReflectionPropertyDrawer::DrawVector3(const reflection::Metadata& metadata, glm::vec3& value,
		ReflectionVectorEditSettings* editSettings)
	{
		return DrawVectorComponents(metadata, std::span<float>(glm::value_ptr(value), 3), editSettings);
	}

	bool ReflectionPropertyDrawer::DrawVector4(const reflection::Metadata& metadata, glm::vec4& value,
		ReflectionVectorEditSettings* editSettings)
	{
		return DrawVectorComponents(metadata, std::span<float>(glm::value_ptr(value), 4), editSettings);
	}

	bool ReflectionPropertyDrawer::DrawCommonBoolProperties(const reflection::TypeInfo& type,
		std::span<const reflection::ObjectView> objects, const reflection::Registry* registry,
		std::span<const ReflectionObjectEditSettings* const> editSettings)
	{
		return DrawCommonPrimitiveProperties<bool>(type, objects, registry, editSettings);
	}

	bool ReflectionPropertyDrawer::DrawCommonFloatProperties(const reflection::TypeInfo& type,
		std::span<const reflection::ObjectView> objects, const reflection::Registry* registry,
		std::span<const ReflectionObjectEditSettings* const> editSettings)
	{
		return DrawCommonPrimitiveProperties<float>(type, objects, registry, editSettings);
	}

	bool ReflectionPropertyDrawer::IsVisible(const reflection::TypeInfo& type)
	{
		return GetReflectionDrawerBoolMetadata(type.Metadata, "Visible", true);
	}

	bool ReflectionPropertyDrawer::DrawObject(const reflection::TypeInfo& type, const reflection::ObjectView& object,
		const reflection::Registry* registry, ReflectionObjectEditSettings* editSettings,
		const ComponentHeaderActions* actions, bool* outIsOpen)
	{
		if (!IsVisible(type))
		{
			if (outIsOpen) *outIsOpen = false;
			return false;
		}

		ImGui::PushID(type.QualifiedName.c_str());

		const std::string displayName = GetReflectionDrawerDisplayName(type.Metadata, type.QualifiedName);
		const std::string headerLabel = displayName + "###ObjectHeader";
		bool open = false;
		if (actions)
		{
			const auto padding = ImGui::GetStyle().FramePadding;
			const float inset = std::min(padding.x, padding.y) * 0.5f;
			const ImVec2 controlPadding(padding.x, padding.y - inset);
			const float controlHeight = ImGui::GetFontSize() + 2.0f * controlPadding.y;
			const float menuWidth = std::max(controlHeight, ImGui::CalcTextSize(ICON_FA_ELLIPSIS_VERTICAL).x + 2.0f * padding.x);
			const bool hasMenu = static_cast<bool>(actions->DrawMenu);
			const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;
			const float actionsWidth = padding.x * 2.0f + controlHeight + (hasMenu ? spacing + menuWidth : 0.0f);
			constexpr ImVec4 transparent(0.f, 0.f, 0.f, 0.f);
			constexpr auto tableFlags = ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoPadInnerX;
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.f, inset));
			const bool tableOpen = ImGui::BeginTable("ComponentHeader", 2, tableFlags);
			ImGui::PopStyleVar();
			if (tableOpen)
			{
				ImGui::TableSetupColumn("Title", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, actionsWidth);
				ImGui::TableNextRow();
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, controlPadding);
				ImGui::TableSetColumnIndex(0);
				ImGui::PushStyleColor(ImGuiCol_Header, transparent);
				ImGui::PushStyleColor(ImGuiCol_HeaderHovered, transparent);
				ImGui::PushStyleColor(ImGuiCol_HeaderActive, transparent);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
				open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
				const auto background = ImGui::IsItemActive() ? ImGuiCol_HeaderActive :
					ImGui::IsItemHovered() ? ImGuiCol_HeaderHovered : ImGuiCol_Header;
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(background));
				if (hasMenu) ImGui::OpenPopupOnItemClick("##HeaderContextMenu", ImGuiPopupFlags_MouseButtonRight);

				ImGui::TableSetColumnIndex(1);
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + padding.x);
				const bool canToggle = static_cast<bool>(actions->SetEnabled);
				bool enabled = actions->Enabled;
				ImGui::BeginDisabled(!canToggle);
				if (ImGui::Checkbox("##ComponentEnabled", &enabled) && canToggle) actions->SetEnabled(enabled);
				ImGui::EndDisabled();
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
					ImGui::SetTooltip("%s", LOCAL_CSTR(actions->EnableTooltipKey));
				if (hasMenu)
				{
					ImGui::SameLine(0.f, spacing);
					ImGui::PushStyleColor(ImGuiCol_Button, transparent);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
					const bool pressed = ImGui::Button(ICON_FA_ELLIPSIS_VERTICAL "###ComponentMenu",
						ImVec2(menuWidth, controlHeight));
					ImGui::PopStyleVar();
					ImGui::PopStyleColor();
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						ImGui::SetTooltip("%s", LOCAL_CSTR("Inspector.ComponentMenu"));
					if (pressed) ImGui::OpenPopup("##HeaderContextMenu");
				}
				ImGui::PopStyleVar();
				if (hasMenu && ImGui::BeginPopup("##HeaderContextMenu"))
				{
					actions->DrawMenu();
					ImGui::EndPopup();
				}
				ImGui::EndTable();
			}
		}
		else
		{
			open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
		}

		if (outIsOpen) *outIsOpen = open;
		const bool changed = open && DrawPropertyTable(type, object, registry, editSettings,
			actions ? actions->EnableProperty : nullptr);
		ImGui::PopID();
		return changed;
	}

	namespace
	{
		bool DrawContainerPropertySection(const reflection::PropertyInfo& property,
			const reflection::ObjectView& object, const reflection::Registry* registry,
			ReflectionObjectEditSettings* editSettings, const char* fullWidthDisplayName);
	}

	bool ReflectionPropertyDrawer::DrawPropertyTable(const reflection::TypeInfo& type,
		const reflection::ObjectView& object, const reflection::Registry* registry,
		ReflectionObjectEditSettings* editSettings, const reflection::PropertyInfo* headerProperty)
	{
		if (!IsVisible(type)) return false;

		ImGui::PushID(type.QualifiedName.c_str());

		bool changed = false;
		constexpr std::string_view generalCategory = "EditorCommon.General";

		const auto isPropertyShown = [&](const reflection::PropertyInfo& property)
			{
				if (&property == headerProperty) return false;
				if (editSettings && editSettings->HiddenProperties.contains(property.Name)) return false;
				if (!GetReflectionDrawerBoolMetadata(property.Metadata, "Visible", true)) return false;
				return IsEditConditionMet(property, type, object, registry);
			};
		const auto categoryOf = [](const reflection::PropertyInfo& property) { return InspectorMetadataText(property.Metadata, "Category", "EditorCommon.General"); };

		// 한 카테고리의 속성 행을 표로 그리며, 컨테이너는 전체 폭 블록으로 분리한다.
		const auto drawCategoryRows = [&](const std::string& category)
			{
				bool tableOpen = false;
				int tableIndex = 0;
				const auto ensureTableOpen = [&]()
					{
						if (tableOpen) return true;
						auto tableStyle = EditorUI::InspectorPropertyTableStyle();
						const float availWidth = ImGui::GetContentRegionAvail().x;
						if (availWidth > 0.0f)
							tableStyle.LabelWidth = std::min(tableStyle.LabelWidth, std::max(80.0f, availWidth * 0.38f));
						const std::string tableId = tableIndex == 0 ? std::string("Properties") : fmt::format("Properties_{}", tableIndex);
						++tableIndex;
						tableOpen = EditorUI::BeginPropertyTable(tableId.c_str(), tableStyle);
						return tableOpen;
					};
				const auto closeTable = [&]()
					{
						if (!tableOpen) return;
						EditorUI::EndPropertyTable();
						tableOpen = false;
					};

				for (const auto& property : type.Properties)
				{
					if (!isPropertyShown(property) || categoryOf(property) != category) continue;
					const std::string displayName = GetReflectionDrawerDisplayName(property.Metadata, property.Name);

					if (property.IsContainer())
					{
						closeTable();
						ImGui::PushID(property.Name.c_str());
						changed |= DrawContainerPropertySection(property, object, registry, editSettings, displayName.c_str());
						ImGui::PopID();
						continue;
					}

					if (!ensureTableOpen()) continue;

					const bool hasLock = (editSettings != nullptr && (editSettings->Vectors.contains(property.Name) || type.QualifiedName == "Engine::TransformComponent"));
					const float btnWidth = ImGui::GetFrameHeight() * 0.85f;
					const float rightPadding = 4.0f;
					const float reservedRightWidth = hasLock ? (btnWidth + rightPadding + ImGui::GetStyle().ItemSpacing.x) : 0.0f;

					EditorUI::PropertyLabel(displayName.c_str(), property.TypeName.c_str(), reservedRightWidth);

					if (hasLock)
					{
						auto& vectorSettings = editSettings->Vectors[property.Name];
						const char* lockIcon = vectorSettings.Locked ? ICON_FA_LOCK : ICON_FA_LOCK_OPEN;
						ImGui::SameLine();
						const float avail = ImGui::GetContentRegionAvail().x;
						if (avail > (btnWidth + rightPadding))
							ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - btnWidth - rightPadding);
						ImGui::PushID(property.Name.c_str());
						const bool wasLocked = vectorSettings.Locked;
						if (wasLocked) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.1f, 0.8f));
						if (ImGui::SmallButton(lockIcon))
						{
							vectorSettings.Locked = !vectorSettings.Locked;
							if (vectorSettings.Locked) editSettings->ReadOnlyProperties.insert(property.Name);
							else editSettings->ReadOnlyProperties.erase(property.Name);
						}
						if (wasLocked) ImGui::PopStyleColor();
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("%s", vectorSettings.Locked ? LOCAL_CSTR("Inspector.UnlockProperty") : LOCAL_CSTR("Inspector.LockProperty"));
						ImGui::PopID();
					}

					EditorUI::PropertyValue();
					changed |= DrawValue(property, object, registry, editSettings);
				}
				closeTable();
			};

		bool hasGeneral = false;
		std::vector<std::string> categories;
		for (const auto& property : type.Properties)
		{
			if (!isPropertyShown(property)) continue;
			auto category = categoryOf(property);
			if (category == generalCategory) { hasGeneral = true; continue; }
			if (std::ranges::find(categories, category) == categories.end()) categories.push_back(std::move(category));
		}

		// 1. 카테고리가 없는 속성은 머리글 없이 맨 위에 펼쳐 그린다.
		//    ID 경로(타입/General/Properties)를 기존과 같게 유지해 저장된 열 너비를 보존한다.
		if (hasGeneral)
		{
			ImGui::PushID(generalCategory.data());
			drawCategoryRows(std::string(generalCategory));
			ImGui::PopID();
		}

		// 2. 명시적 카테고리만 접을 수 있는 노드로 묶는다.
		for (const auto& category : categories)
		{
			ImGui::PushID(category.c_str());
			const auto categoryLabel = LocalizeInspectorText(category) + "###PropertyCategory";
			if (ImGui::TreeNodeEx(categoryLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth))
			{
				drawCategoryRows(category);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		ImGui::PopID();
		return changed;
	}

	namespace
	{
		template<typename T, typename DrawWidget, typename CommitFn>
		bool DrawValueEditorWithCommit(const T& current, bool canEdit, bool disableWhenReadOnly,
			DrawWidget drawWidget, CommitFn commitValue)
		{
			T candidate = current;
			if (disableWhenReadOnly) ImGui::BeginDisabled(!canEdit);
			ImGui::SetNextItemWidth(-1.0f);
			const bool requested = drawWidget(candidate);
			if (disableWhenReadOnly) ImGui::EndDisabled();

			if (!canEdit || !requested || candidate == current) return false;
			return commitValue(candidate);
		}

		template<typename T, typename CommitFn>
		bool DrawInlineScalar(const T& current, bool canEdit, ImGuiDataType dataType, CommitFn commitValue)
		{
			const bool isFloating = std::is_floating_point_v<T>;
			const float dragSpeed = isFloating ? 0.05f : 1.0f;
			const char* format = isFloating ? "%.3f" : nullptr;
			return DrawValueEditorWithCommit(current, canEdit, true,
				[&](T& candidate)
				{
					return ImGui::DragScalar("##Value", dataType, &candidate, dragSpeed, nullptr, nullptr, format);
				},
				[&](const T& candidate)
				{
					if constexpr (std::is_floating_point_v<T>)
					{
						if (!std::isfinite(candidate)) return false;
					}
					return commitValue(reflection::ValueView::From(candidate));
				});
		}

		void DrawReadOnlyCellValue(const reflection::ValueView& value, const reflection::Registry* registry)
		{
			if (const auto* text = value.Get<std::string>())
			{
				std::string copy = *text;
				ImGui::SetNextItemWidth(-1.0f);
				DrawInspectorText(copy, true);
				return;
			}
			if (const auto* text = value.Get<std::wstring>())
			{
				std::wstring copy = *text;
				ImGui::SetNextItemWidth(-1.0f);
				DrawInspectorText(copy, true);
				return;
			}
			if (const auto* path = value.Get<std::filesystem::path>())
			{
				std::filesystem::path copy = *path;
				ImGui::SetNextItemWidth(-1.0f);
				DrawInspectorPath(copy, true);
				return;
			}

			const std::string preview = FormatReflectionDrawerValue(value, registry);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(preview.data(), preview.data() + preview.size());
			DrawClippedStringTooltip(preview);
		}

		bool DrawInlineValueView(const reflection::ValueView& value, const reflection::Registry* registry, bool canEdit,
			const std::function<bool(const reflection::ValueView&)>& commitValue,
			const std::function<bool(const reflection::EnumValue&)>& commitEnum)
		{
			if (const auto* enumeration = registry ? registry->FindEnum(value) : nullptr)
			{
				static const reflection::Metadata emptyMetadata;
				return DrawInspectorEnumValue(emptyMetadata, *enumeration, value, canEdit, commitEnum);
			}
			if (const auto* current = value.Get<std::string>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, false,
					[canEdit](std::string& candidate) { return DrawInspectorText(candidate, !canEdit); },
					[&](const std::string& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<std::wstring>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, false,
					[canEdit](std::wstring& candidate) { return DrawInspectorText(candidate, !canEdit); },
					[&](const std::wstring& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<std::filesystem::path>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, false,
					[canEdit](std::filesystem::path& candidate) { return DrawInspectorPath(candidate, !canEdit); },
					[&](const std::filesystem::path& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<bool>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, true,
					[](bool& candidate) { return ImGui::Checkbox("##Value", &candidate); },
					[&](const bool& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<std::int8_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_S8, commitValue);
			if (const auto* current = value.Get<std::uint8_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_U8, commitValue);
			if (const auto* current = value.Get<std::int16_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_S16, commitValue);
			if (const auto* current = value.Get<std::uint16_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_U16, commitValue);
			if (const auto* current = value.Get<std::int32_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_S32, commitValue);
			if (const auto* current = value.Get<std::uint32_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_U32, commitValue);
			if (const auto* current = value.Get<std::int64_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_S64, commitValue);
			if (const auto* current = value.Get<std::uint64_t>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_U64, commitValue);
			if (const auto* current = value.Get<float>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_Float, commitValue);
			if (const auto* current = value.Get<double>()) return DrawInlineScalar(*current, canEdit, ImGuiDataType_Double, commitValue);
			if (const auto* current = value.Get<glm::vec2>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, true,
					[](glm::vec2& candidate) { return ImGui::DragFloat2("##Value", glm::value_ptr(candidate), 0.05f); },
					[&](const glm::vec2& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<glm::vec3>())
			{
				static const reflection::Metadata emptyMetadata;
				return DrawValueEditorWithCommit(*current, canEdit, true,
					[](glm::vec3& candidate) { return Editor::ReflectionPropertyDrawer::DrawVector3(emptyMetadata, candidate); },
					[&](const glm::vec3& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<glm::vec4>())
			{
				static const reflection::Metadata emptyMetadata;
				return DrawValueEditorWithCommit(*current, canEdit, true,
					[](glm::vec4& candidate) { return Editor::ReflectionPropertyDrawer::DrawVector4(emptyMetadata, candidate); },
					[&](const glm::vec4& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<glm::quat>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, true,
					[](glm::quat& candidate)
					{
						if (!ImGui::DragFloat4("##Value", glm::value_ptr(candidate), 0.01f)) return false;
						const float length = glm::length(candidate);
						if (!std::isfinite(length) || length < 1.0e-6f) return false;
						candidate /= length;
						return true;
					},
					[&](const glm::quat& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
#define INLINE_RESOURCE(Type) \
			if (const auto* current = value.Get<Engine::ResourceHandle<Engine::Type>>()) \
			{ \
				return DrawValueEditorWithCommit(*current, canEdit, true, \
					[canEdit](auto& candidate) { return Editor::DrawInspectorResourcePicker(candidate, canEdit); }, \
					[&](const auto& candidate) { return commitValue(reflection::ValueView::From(candidate)); }); \
			}
			INLINE_RESOURCE(Texture)
			INLINE_RESOURCE(Sprite)
			INLINE_RESOURCE(Mesh)
			INLINE_RESOURCE(Material)
			INLINE_RESOURCE(MaterialInstance)
			INLINE_RESOURCE(MaterialInterface)
			INLINE_RESOURCE(Skeleton)
			INLINE_RESOURCE(Shader)
			INLINE_RESOURCE(Animation)
			INLINE_RESOURCE(AudioClip)
			INLINE_RESOURCE(SpriteAnimation)
			INLINE_RESOURCE(ParticleEffect)
			INLINE_RESOURCE(CompositeEffect)
			INLINE_RESOURCE(TerrainLayerAsset)
#undef INLINE_RESOURCE
			if (const auto* current = value.Get<entt::entity>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, true,
					[canEdit](entt::entity& candidate) { return DrawInspectorEntityPicker(candidate, canEdit); },
					[&](const entt::entity& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}
			if (const auto* current = value.Get<Engine::SceneEntityReference>())
			{
				return DrawValueEditorWithCommit(*current, canEdit, true,
					[canEdit](Engine::SceneEntityReference& candidate)
					{
						const auto* scene = Engine::SceneManager::Get().GetCurrentScene();
						if (!scene) return false;
						const auto* entity = scene->FindEntityById(candidate.PersistentId);
						entt::entity handle = entity ? entity->GetEntityHandle() : entt::null;
						if (!DrawInspectorEntityPicker(handle, canEdit, candidate.PersistentId != 0 && !entity)) return false;
						const auto* id = scene->GetRegistry().try_get<Engine::IDComponent>(handle);
						candidate.PersistentId = id ? id->id : 0;
						return true;
					},
					[&](const Engine::SceneEntityReference& candidate) { return commitValue(reflection::ValueView::From(candidate)); });
			}

			DrawReadOnlyCellValue(value, registry);
			return false;
		}

		const char* GetContainerKindLabel(const reflection::PropertyInfo& property)
		{
			switch (property.GetValueKind())
			{
			case reflection::PropertyValueKind::Vector: return "Vector";
			case reflection::PropertyValueKind::Array: return "Array";
			case reflection::PropertyValueKind::Set:
			case reflection::PropertyValueKind::UnorderedSet: return "Set";
			case reflection::PropertyValueKind::Map:
			case reflection::PropertyValueKind::UnorderedMap: return "Map";
			default: return "Container";
			}
		}

		bool SupportsTextParsedKey(std::type_index type)
		{
			return type == typeid(std::string) || type == typeid(std::wstring) ||
				type == typeid(std::filesystem::path) ||
				type == typeid(std::int32_t) || type == typeid(std::uint32_t) ||
				type == typeid(std::int64_t) || type == typeid(std::uint64_t) ||
				type == typeid(float) || type == typeid(double);
		}

		template<typename Fn>
		bool WithParsedValue(std::type_index type, const std::string& text, Fn&& fn)
		{
			if (type == typeid(std::string)) return fn(reflection::ValueView::From(text));
			if (type == typeid(std::wstring))
			{
				std::wstring wide;
				if (text.empty() || !archive::detail::Utf8ToWide(text, wide)) return false;
				return fn(reflection::ValueView::From(wide));
			}
			if (type == typeid(std::filesystem::path))
			{
				const std::filesystem::path path = Engine::Utf8ToPath(text);
				return fn(reflection::ValueView::From(path));
			}
			try
			{
				if (type == typeid(std::int32_t))
				{
					const auto v = static_cast<std::int32_t>(std::stoi(text));
					return fn(reflection::ValueView::From(v));
				}
				if (type == typeid(std::uint32_t))
				{
					const auto v = static_cast<std::uint32_t>(std::stoul(text));
					return fn(reflection::ValueView::From(v));
				}
				if (type == typeid(std::int64_t))
				{
					const auto v = static_cast<std::int64_t>(std::stoll(text));
					return fn(reflection::ValueView::From(v));
				}
				if (type == typeid(std::uint64_t))
				{
					const auto v = static_cast<std::uint64_t>(std::stoull(text));
					return fn(reflection::ValueView::From(v));
				}
				if (type == typeid(float))
				{
					const float v = std::stof(text);
					return std::isfinite(v) && fn(reflection::ValueView::From(v));
				}
				if (type == typeid(double))
				{
					const double v = std::stod(text);
					return std::isfinite(v) && fn(reflection::ValueView::From(v));
				}
			}
			catch (...)
			{
				return false;
			}
			return false;
		}

		bool TryInsertDefaultMapEntryWithKey(const reflection::PropertyInfo& property,
			const reflection::ObjectView& object, const reflection::ValueView& key)
		{
			if (property.CanEmplaceMapValueObject())
				return property.TryEmplaceMapValueObject(object, key).Succeeded();
			if (!property.CanInsertMapEntry()) return false;

			const auto insertDefault = [&]<typename T>(const T& defaultValue)
			{
				return property.TryInsertMapEntry(object, key, reflection::ValueView::From(defaultValue)) ==
					reflection::PropertyAccessError::None;
			};

			const std::type_index mappedType = property.GetMapValueType();
			if (mappedType == typeid(std::string)) return insertDefault(std::string{});
			if (mappedType == typeid(std::wstring)) return insertDefault(std::wstring{});
			if (mappedType == typeid(std::filesystem::path)) return insertDefault(std::filesystem::path{});
			if (mappedType == typeid(bool)) return insertDefault(false);
			if (mappedType == typeid(std::int32_t)) return insertDefault(std::int32_t{ 0 });
			if (mappedType == typeid(std::uint32_t)) return insertDefault(std::uint32_t{ 0 });
			if (mappedType == typeid(std::int64_t)) return insertDefault(std::int64_t{ 0 });
			if (mappedType == typeid(std::uint64_t)) return insertDefault(std::uint64_t{ 0 });
			if (mappedType == typeid(float)) return insertDefault(0.0f);
			if (mappedType == typeid(double)) return insertDefault(0.0);
			if (mappedType == typeid(glm::vec2)) return insertDefault(glm::vec2(0.0f));
			if (mappedType == typeid(glm::vec3)) return insertDefault(glm::vec3(0.0f));
			if (mappedType == typeid(glm::vec4)) return insertDefault(glm::vec4(0.0f));
			return false;
		}
	}

	bool ReflectionPropertyDrawer::DrawValue(const reflection::PropertyInfo& property,
		const reflection::ObjectView& object, const reflection::Registry* registry,
		ReflectionObjectEditSettings* editSettings)
	{
		ImGui::PushID(property.Name.c_str());

		const auto read = property.TryReadValue(object);

		if (!read)
		{
			ImGui::TextDisabled("%s", LOCAL("Inspector.ValueUnavailable").c_str());
			ImGui::PopID();
			return false;
		}

		const bool editable = GetReflectionDrawerBoolMetadata(property.Metadata, "Editable", false);
		bool readOnly = GetReflectionDrawerBoolMetadata(property.Metadata, "ReadOnly", false);
		if (editSettings)
		{
			const auto vector = editSettings->Vectors.find(property.Name);
			readOnly |= editSettings->ReadOnlyProperties.contains(property.Name) ||
				(vector != editSettings->Vectors.end() && vector->second.Locked);
		}
		const bool canEdit = editable && !readOnly && property.CanWrite() && !object.IsReadOnly();

		bool changed = false;

		if (const auto* enumeration = registry ? registry->FindEnum(read.Value) : nullptr)
		{
			changed = DrawInspectorEnum(property, object, *enumeration, read.Value, canEdit);
		}
		else if (const auto* current = read.Value.Get<std::string>())
		{
			const bool isUIFontFamily = object.Is<Engine::UITextComponent>() && property.Name == "fontFamily";
			if (isUIFontFamily)
			{
				changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
					[](std::string& candidate) { return DrawInspectorFontFamily(candidate); });
			}
			else
			{
				changed = DrawValueEditorWithCommit(*current, canEdit, false,
					[canEdit](std::string& candidate) { return DrawInspectorText(candidate, !canEdit); },
					[&](const std::string& candidate)
					{
						const auto error = property.TryWriteValue(object, reflection::ValueView::From(candidate));
						if (error != reflection::PropertyAccessError::None)
						{
							ImGui::TextDisabled("%s", LOCAL_FMT("Inspector.WriteFailed", reflection::ToString(error)).c_str());
							return false;
						}
						return true;
					});
			}
		}
		else if (const auto* current = read.Value.Get<std::wstring>())
		{
			changed = DrawValueEditorWithCommit(*current, canEdit, false,
				[canEdit](std::wstring& candidate) { return DrawInspectorText(candidate, !canEdit); },
				[&](const std::wstring& candidate)
				{
					const auto error = property.TryWriteValue(object, reflection::ValueView::From(candidate));
					if (error != reflection::PropertyAccessError::None)
					{
						ImGui::TextDisabled("%s", LOCAL_FMT("Inspector.WriteFailed", reflection::ToString(error)).c_str());
						return false;
					}
					return true;
				});
		}
		else if (const auto* current = read.Value.Get<std::filesystem::path>())
		{
			changed = DrawValueEditorWithCommit(*current, canEdit, false,
				[canEdit](std::filesystem::path& candidate) { return DrawInspectorPath(candidate, !canEdit); },
				[&](const std::filesystem::path& candidate)
				{
					const auto error = property.TryWriteValue(object, reflection::ValueView::From(candidate));
					if (error != reflection::PropertyAccessError::None)
					{
						ImGui::TextDisabled("%s", LOCAL_FMT("Inspector.WriteFailed", reflection::ToString(error)).c_str());
						return false;
					}
					return true;
				});
		}
		else if (const auto* current = read.Value.Get<bool>())
		{
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[](bool& candidate)
				{
					return ImGui::Checkbox("##Value", &candidate);
				});
		}
		else if (const auto* current = read.Value.Get<std::int8_t>())
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_S8);
		else if (const auto* current = read.Value.Get<std::uint8_t>())
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_U8);
		else if (const auto* current = read.Value.Get<std::int16_t>())
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_S16);
		else if (const auto* current = read.Value.Get<std::uint16_t>())
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_U16);
		else if (const auto* current = read.Value.Get<std::int32_t>())
		{
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_S32);
		}
		else if (const auto* current = read.Value.Get<std::uint32_t>())
		{
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_U32);
		}
		else if (const auto* current = read.Value.Get<std::int64_t>())
		{
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_S64);
		}
		else if (const auto* current = read.Value.Get<std::uint64_t>())
		{
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_U64);
		}
		else if (const auto* current = read.Value.Get<float>())
		{
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_Float);
		}
		else if (const auto* current = read.Value.Get<double>())
		{
			changed = DrawReflectionScalarProperty(property, object, *current, canEdit, ImGuiDataType_Double);
		}
		else if (const auto* current = read.Value.Get<glm::vec2>())
		{
			auto* settings = editSettings ? &editSettings->Vectors[property.Name] : nullptr;
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&](glm::vec2& candidate)
				{ return DrawVectorComponents(property.Metadata, { glm::value_ptr(candidate), 2 }, settings); });
		}
		else if (const auto* current = read.Value.Get<glm::vec3>())
		{
			auto* vectorSettings = editSettings != nullptr ? &editSettings->Vectors[property.Name] : nullptr;

			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&property, vectorSettings](glm::vec3& candidate)
				{
					return property.Metadata.Find("Color") ? ImGui::ColorEdit3("##Value", glm::value_ptr(candidate)) :
						DrawVector3(property.Metadata, candidate, vectorSettings);
				});
		}
		else if (const auto* current = read.Value.Get<glm::vec4>())
		{
			auto* vectorSettings = editSettings != nullptr ? &editSettings->Vectors[property.Name] : nullptr;

			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&property, vectorSettings](glm::vec4& candidate)
				{
					return property.Metadata.Find("Color") ? ImGui::ColorEdit4("##Value", glm::value_ptr(candidate)) :
						DrawVector4(property.Metadata, candidate, vectorSettings);
				});
		}
		else if (const auto* current = read.Value.Get<glm::mat3>())
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[](glm::mat3& candidate) { return DrawInspectorMatrix(candidate); });
		else if (const auto* current = read.Value.Get<glm::mat4>())
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[](glm::mat4& candidate) { return DrawInspectorMatrix(candidate); });
		else if (const auto* current = read.Value.Get<glm::quat>())
		{
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit, [](glm::quat& candidate)
			{
				if (!ImGui::DragFloat4("##Value", glm::value_ptr(candidate), 0.01f)) return false;
				const float length = glm::length(candidate);
				if (!std::isfinite(length) || length < 1.0e-6f) return false;
				candidate /= length;
				return true;
			});
		}
#define INSPECTOR_RESOURCE(Type) \
		else if (const auto* current = read.Value.Get<Engine::ResourceHandle<Engine::Type>>()) \
		{ \
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit, \
				[&](auto& candidate) { return Editor::DrawInspectorResourcePicker(candidate, canEdit); }); \
		}
		INSPECTOR_RESOURCE(Texture)
		INSPECTOR_RESOURCE(Sprite)
		INSPECTOR_RESOURCE(Mesh)
		INSPECTOR_RESOURCE(Material)
		INSPECTOR_RESOURCE(MaterialInstance)
		INSPECTOR_RESOURCE(MaterialInterface)
		INSPECTOR_RESOURCE(Skeleton)
		INSPECTOR_RESOURCE(Shader)
		INSPECTOR_RESOURCE(Animation)
		INSPECTOR_RESOURCE(AudioClip)
		INSPECTOR_RESOURCE(SpriteAnimation)
		INSPECTOR_RESOURCE(ParticleEffect)
		INSPECTOR_RESOURCE(CompositeEffect)
		INSPECTOR_RESOURCE(TerrainLayerAsset)
#undef INSPECTOR_RESOURCE
		else if (const auto* current = read.Value.Get<entt::entity>())
		{
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&](entt::entity& candidate) { return DrawInspectorEntityPicker(candidate, canEdit); });
		}
		else if (const auto* current = read.Value.Get<Engine::SceneEntityReference>())
		{
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&](Engine::SceneEntityReference& candidate)
				{
					const auto* scene = Engine::SceneManager::Get().GetCurrentScene();
					if (!scene) return false;
					const auto* entity = scene->FindEntityById(candidate.PersistentId);
					entt::entity handle = entt::null;
					if (entity) handle = entity->GetEntityHandle();
					if (!DrawInspectorEntityPicker(handle, canEdit, candidate.PersistentId != 0 && !entity)) return false;
					const auto* id = scene->GetRegistry().try_get<Engine::IDComponent>(handle);
					candidate.PersistentId = id ? id->id : 0;
					return true;
				});
		}
		else if (read.Value.Is<Engine::ColliderShape>() || read.Value.Is<Engine::Collider2DShape>())
		{
			ImGui::BeginDisabled(!canEdit);
			const auto edit = [&]<typename T>()
			{
				auto candidate = *read.Value.Get<T>();
				if (!DrawColliderShape(candidate, registry) || !canEdit) return false;
				return property.TryWriteValue(object, reflection::ValueView::From(candidate)) ==
					reflection::PropertyAccessError::None;
			};
			changed = read.Value.Is<Engine::ColliderShape>() ? edit.operator()<Engine::ColliderShape>() :
				edit.operator()<Engine::Collider2DShape>();
			ImGui::EndDisabled();
		}
		else if (const auto* curveValue = read.Value.Get<Engine::FloatCurve>())
		{
			ImGui::BeginDisabled(!canEdit);
			auto candidate = *curveValue;
			if (DrawFloatCurveField(candidate) && canEdit)
				changed = property.TryWriteValue(object, reflection::ValueView::From(candidate)) == reflection::PropertyAccessError::None;
			ImGui::EndDisabled();
		}
		else if (const auto* nested = registry ? registry->FindType(read.Value) : nullptr)
		{
			const auto view = canEdit ? property.TryEditObject(object).Object : nested->AsObject(read.Value);
			changed = view.IsValid() && DrawPropertyTable(*nested, view, registry, nullptr);
		}
		else if (property.IsContainer())
		{
			changed = DrawContainerPropertySection(property, object, registry, editSettings, nullptr);
		}
		else
		{
			const std::string preview = FormatReflectionDrawerValue(read.Value, registry);
			ImGui::TextUnformatted(preview.data(), preview.data() + preview.size());
			DrawClippedStringTooltip(preview);
		}

		ImGui::PopID();
		return changed;
	}

	namespace
	{
		const reflection::PropertyInfo* GetSingleInlinableProperty(const reflection::TypeInfo& elementType,
			const ReflectionObjectEditSettings* editSettings)
		{
			const reflection::PropertyInfo* candidate = nullptr;
			for (const auto& prop : elementType.Properties)
			{
				if (editSettings && editSettings->HiddenProperties.contains(prop.Name)) continue;
				if (!GetReflectionDrawerBoolMetadata(prop.Metadata, "Visible", true)) continue;
				if (prop.IsContainer() || prop.Metadata.Find("EditCondition") != nullptr) return nullptr;
				if (candidate != nullptr) return nullptr;
				candidate = &prop;
			}
			return candidate;
		}

		std::string BuildObjectSummary(const reflection::TypeInfo& type, const reflection::ObjectView& object,
			const reflection::Registry* registry)
		{
			std::string summary;
			int count = 0;
			const std::string unavailable = LocalizeInspectorText("Inspector.ValuePreviewUnavailable");
			for (const auto& prop : type.Properties)
			{
				const auto read = prop.TryReadValue(object);
				if (!read) continue;
				const std::string formatted = FormatReflectionDrawerValue(read.Value, registry);
				if (formatted.empty() || formatted == unavailable) continue;
				if (!summary.empty()) summary += ", ";
				summary += formatted;
				if (++count >= 2) break;
			}
			return summary;
		}

		std::string ShortTypeName(const std::string& qualifiedName)
		{
			if (const auto pos = qualifiedName.rfind("::"); pos != std::string::npos)
				return qualifiedName.substr(pos + 2);
			return qualifiedName;
		}

		bool DrawContainerPropertySection(const reflection::PropertyInfo& property,
			const reflection::ObjectView& object, const reflection::Registry* registry,
			ReflectionObjectEditSettings* editSettings, const char* fullWidthDisplayName)
		{
			const bool editable = GetReflectionDrawerBoolMetadata(property.Metadata, "Editable", false);
			bool readOnly = GetReflectionDrawerBoolMetadata(property.Metadata, "ReadOnly", false);
			if (editSettings)
			{
				const auto vector = editSettings->Vectors.find(property.Name);
				readOnly |= editSettings->ReadOnlyProperties.contains(property.Name) ||
					(vector != editSettings->Vectors.end() && vector->second.Locked);
			}
			const bool canEdit = editable && !readOnly && property.CanWrite() && !object.IsReadOnly();

			const auto sizeResult = property.TryGetSize(object);
			const std::size_t count = sizeResult ? sizeResult.Size : 0;
			const auto preview = sizeResult ? LOCAL_FMT("Inspector.ElementCount", count) : LOCAL("Inspector.SizeUnavailable");
			const char* kindLabel = GetContainerKindLabel(property);

			const bool isSet = property.IsSet();
			const bool isMap = property.IsMap();
			const bool canAddSequence = !isSet && !isMap && property.CanResize();
			const bool canAddSet = isSet && property.CanInsertSetElement() && SupportsTextParsedKey(property.GetElementCppType());
			const bool canAddMap = isMap && (property.CanInsertMapEntry() || property.CanEmplaceMapValueObject()) &&
				SupportsTextParsedKey(property.GetMapKeyType());
			const bool showAdd = canEdit && (canAddSequence || canAddSet || canAddMap);

			const std::string treeLabel = fullWidthDisplayName != nullptr
				? fmt::format("{}  [{}] {}###ContainerTree_{}", fullWidthDisplayName, kindLabel, preview, property.Name)
				: fmt::format("[{}] {}###ContainerTree_{}", kindLabel, preview, property.Name);

			const float rowStartX = ImGui::GetCursorPosX();
			const float rowAvailW = ImGui::GetContentRegionAvail().x;
			const float addBtnW = EditorUI::GetIconButtonSize(ICON_FA_PLUS, true).x;

			const bool nodeOpen = ImGui::TreeNodeEx(treeLabel.c_str(),
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap);
			if (fullWidthDisplayName != nullptr && ImGui::IsItemHovered() && !property.TypeName.empty())
				ImGui::SetTooltip("%s", property.TypeName.c_str());

			bool changed = false;
			if (showAdd)
			{
				if (rowAvailW > addBtnW + 8.0f)
					ImGui::SameLine(rowStartX + rowAvailW - addBtnW);
				else
					ImGui::SameLine();

				if (EditorUI::IconButton(("Add_" + property.Name).c_str(), ICON_FA_PLUS, "EditorCommon.Add",
					{ .Compact = true, .NoWrap = true }))
				{
					if (canAddSequence)
					{
						if (property.TryResize(object, count + 1) == reflection::PropertyAccessError::None)
							changed = true;
					}
					else
					{
						ImGui::OpenPopup("AddContainerEntry");
					}
				}
			}

			if (ImGui::BeginPopup("AddContainerEntry"))
			{
				static std::string s_FallbackInput;
				auto& text = editSettings ? editSettings->TextBuffers[property.Name] : s_FallbackInput;
				const bool submitted = ImGui::InputText("##NewEntry", &text, ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::SameLine();
				if ((submitted || ImGui::Button(LOCAL_CSTR("EditorCommon.Add"))) && canEdit)
				{
					bool inserted = false;
					if (isSet)
					{
						inserted = WithParsedValue(property.GetElementCppType(), text, [&](const reflection::ValueView& val)
						{
							return property.TryInsertSetElement(object, val) == reflection::PropertyAccessError::None;
						});
					}
					else if (isMap)
					{
						inserted = WithParsedValue(property.GetMapKeyType(), text, [&](const reflection::ValueView& key)
						{
							return TryInsertDefaultMapEntryWithKey(property, object, key);
						});
					}
					if (inserted)
					{
						changed = true;
						text.clear();
						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::EndPopup();
			}

			if (nodeOpen)
			{
				const float fontSize = ImGui::GetFontSize();
				const float indentW = fullWidthDisplayName != nullptr ? fontSize * 0.75f : 0.0f;
				if (indentW > 0.0f) ImGui::Indent(indentW);

				if (count == 0)
				{
					ImGui::TextDisabled("%s", LOCAL_CSTR("EditorCommon.None"));
				}
				else
				{
					constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
						ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
					const float deleteWidth = EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x + fontSize * 0.4f;
					const std::string tableId = fmt::format("##ContainerTable_{}", property.Name);

					if (isSet)
					{
						const bool canErase = canEdit && property.CanEraseSetElement();
						const int columns = canErase ? 3 : 2;
						ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(fontSize * 0.3f, fontSize * 0.15f));
						if (ImGui::BeginTable(tableId.c_str(), columns, tableFlags))
						{
							ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 2.4f);
							ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Value"), ImGuiTableColumnFlags_WidthStretch);
							if (canErase)
								ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, deleteWidth);
							ImGui::TableHeadersRow();

							std::size_t toRemoveIndex = std::numeric_limits<std::size_t>::max();
							(void)property.TryForEachElement(object, [&](std::size_t index, const reflection::ValueView& elem) -> bool
							{
								ImGui::TableNextRow();
								ImGui::PushID(static_cast<int>(index));

								ImGui::TableSetColumnIndex(0);
								ImGui::AlignTextToFramePadding();
								ImGui::Text("[%zu]", index);

								ImGui::TableSetColumnIndex(1);
								DrawReadOnlyCellValue(elem, registry);

								if (canErase)
								{
									ImGui::TableSetColumnIndex(2);
									if (EditorUI::SmallIconButton("DelElem", ICON_FA_TRASH, "EditorCommon.Delete"))
										toRemoveIndex = index;
								}

								ImGui::PopID();
								return true;
							});

							ImGui::EndTable();

							if (canErase && toRemoveIndex < count)
							{
								(void)property.TryForEachElement(object, [&](std::size_t index, const reflection::ValueView& elem) -> bool
								{
									if (index != toRemoveIndex) return true;
									if (property.TryEraseSetElement(object, elem) == reflection::PropertyAccessError::None)
										changed = true;
									return false;
								});
							}
						}
						ImGui::PopStyleVar();
					}
					else if (isMap)
					{
						const bool canErase = canEdit && property.CanEraseMapEntry();
						const auto* valueType = registry ? registry->FindType(property.GetMapValueType()) : nullptr;
						const reflection::PropertyInfo* singleValueProp = valueType != nullptr
							? GetSingleInlinableProperty(*valueType, editSettings) : nullptr;
						const bool isMultiFieldObjectMap = (valueType != nullptr) && (singleValueProp == nullptr);

						if (isMultiFieldObjectMap)
						{
							const std::string shortName = ShortTypeName(valueType->QualifiedName);
							const float delBtnW = EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x;
							std::size_t toRemoveIndex = std::numeric_limits<std::size_t>::max();

							for (std::size_t i = 0; i < count; ++i)
							{
								const auto keyRead = property.TryReadMapKey(object, i);
								if (!keyRead) continue;

								ImGui::PushID(static_cast<int>(i));
								auto valueObj = (canEdit && property.CanEditMapValueObject())
									? property.TryEditMapValueObject(object, keyRead.Value).Object
									: [&]()
									{
										const auto valRead = property.TryReadMapValue(object, i);
										return valRead ? valueType->AsObject(valRead.Value) : reflection::ObjectView{};
									}();

								const std::string keyText = FormatReflectionDrawerValue(keyRead.Value, registry);
								const std::string summary = valueObj.IsValid() ? BuildObjectSummary(*valueType, valueObj, registry) : std::string{};
								const std::string entryLabel = summary.empty()
									? fmt::format("[{}] {}###MapObj_{}", keyText, shortName, i)
									: fmt::format("[{}] {} — {}###MapObj_{}", keyText, shortName, summary, i);

								const float entryX = ImGui::GetCursorPosX();
								const float entryAvailW = ImGui::GetContentRegionAvail().x;
								const bool entryOpen = ImGui::TreeNodeEx(entryLabel.c_str(),
									ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap);

								if (canErase)
								{
									if (entryAvailW > delBtnW + 8.0f)
										ImGui::SameLine(entryX + entryAvailW - delBtnW);
									else
										ImGui::SameLine();
									if (EditorUI::IconButton("DelElem", ICON_FA_TRASH, "EditorCommon.Delete",
										{ .Compact = true, .NoWrap = true }))
									{
										toRemoveIndex = i;
									}
								}

								if (entryOpen)
								{
									if (valueObj.IsValid())
										changed |= ReflectionPropertyDrawer::DrawPropertyTable(*valueType, valueObj, registry, editSettings);
									ImGui::TreePop();
								}
								ImGui::PopID();
							}

							if (canErase && toRemoveIndex < count)
							{
								if (const auto keyRead = property.TryReadMapKey(object, toRemoveIndex))
								{
									if (property.TryEraseMapEntry(object, keyRead.Value) == reflection::PropertyAccessError::None)
										changed = true;
								}
							}
						}
						else
						{
							const int columns = canErase ? 3 : 2;
							const std::string valueHeader = singleValueProp != nullptr
								? GetReflectionDrawerDisplayName(singleValueProp->Metadata, singleValueProp->Name)
								: LOCAL("EditorCommon.Value");
							ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(fontSize * 0.3f, fontSize * 0.15f));
							if (ImGui::BeginTable(tableId.c_str(), columns, tableFlags))
							{
								ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.35f);
								ImGui::TableSetupColumn(valueHeader.c_str(), ImGuiTableColumnFlags_WidthStretch, 0.65f);
								if (canErase)
									ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, deleteWidth);
								ImGui::TableHeadersRow();

								std::size_t toRemoveIndex = std::numeric_limits<std::size_t>::max();
								for (std::size_t i = 0; i < count; ++i)
								{
									const auto keyRead = property.TryReadMapKey(object, i);
									if (!keyRead) continue;

									ImGui::TableNextRow();
									ImGui::PushID(static_cast<int>(i));

									ImGui::TableSetColumnIndex(0);
									DrawReadOnlyCellValue(keyRead.Value, registry);

									ImGui::TableSetColumnIndex(1);
									if (valueType != nullptr && singleValueProp != nullptr)
									{
										auto valueObj = (canEdit && property.CanEditMapValueObject())
											? property.TryEditMapValueObject(object, keyRead.Value).Object
											: [&]()
											{
												const auto valRead = property.TryReadMapValue(object, i);
												return valRead ? valueType->AsObject(valRead.Value) : reflection::ObjectView{};
											}();
										if (valueObj.IsValid())
											changed |= ReflectionPropertyDrawer::DrawValue(*singleValueProp, valueObj, registry, editSettings);
									}
									else if (const auto valRead = property.TryReadMapValue(object, i))
									{
										const bool canEditVal = canEdit && property.CanWriteMapValue();
										changed |= DrawInlineValueView(valRead.Value, registry, canEditVal,
											[&](const reflection::ValueView& newVal)
											{
												return property.TryWriteMapValue(object, keyRead.Value, newVal) ==
													reflection::PropertyAccessError::None;
											},
											[](const reflection::EnumValue&) { return false; });
									}

									if (canErase)
									{
										ImGui::TableSetColumnIndex(2);
										if (EditorUI::SmallIconButton("DelElem", ICON_FA_TRASH, "EditorCommon.Delete"))
											toRemoveIndex = i;
									}

									ImGui::PopID();
								}

								ImGui::EndTable();

								if (canErase && toRemoveIndex < count)
								{
									if (const auto keyRead = property.TryReadMapKey(object, toRemoveIndex))
									{
										if (property.TryEraseMapEntry(object, keyRead.Value) == reflection::PropertyAccessError::None)
											changed = true;
									}
								}
							}
							ImGui::PopStyleVar();
						}
					}
					else
					{
						const auto* elementType = registry ? registry->FindType(property.GetElementCppType()) : nullptr;
						const bool isObjectSequence = (elementType != nullptr) && property.CanEditElementObject();
						const reflection::PropertyInfo* singleElemProp = elementType != nullptr
							? GetSingleInlinableProperty(*elementType, editSettings) : nullptr;
						const bool isMultiFieldObjectSeq = (elementType != nullptr) && (singleElemProp == nullptr);
						const bool canErase = canEdit && property.CanErase();

						if (isMultiFieldObjectSeq)
						{
							const std::string shortName = ShortTypeName(elementType->QualifiedName);
							const float delBtnW = EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x;
							std::size_t toRemoveIndex = std::numeric_limits<std::size_t>::max();

							for (std::size_t i = 0; i < count; ++i)
							{
								ImGui::PushID(static_cast<int>(i));
								auto elemObj = (canEdit && isObjectSequence)
									? property.TryEditElementObject(object, i).Object
									: [&]()
									{
										const auto elemRead = property.TryReadElement(object, i);
										return elemRead ? elementType->AsObject(elemRead.Value) : reflection::ObjectView{};
									}();

								const std::string summary = elemObj.IsValid() ? BuildObjectSummary(*elementType, elemObj, registry) : std::string{};
								const std::string elementLabel = summary.empty()
									? fmt::format("[{}] {}###Element_{}", i, shortName, i)
									: fmt::format("[{}] {} — {}###Element_{}", i, shortName, summary, i);

								const float elemX = ImGui::GetCursorPosX();
								const float elemAvailW = ImGui::GetContentRegionAvail().x;
								const bool elemOpen = ImGui::TreeNodeEx(elementLabel.c_str(),
									ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap);

								if (canErase)
								{
									if (elemAvailW > delBtnW + 8.0f)
										ImGui::SameLine(elemX + elemAvailW - delBtnW);
									else
										ImGui::SameLine();
									if (EditorUI::IconButton("DelElem", ICON_FA_TRASH, "EditorCommon.Delete",
										{ .Compact = true, .NoWrap = true }))
									{
										toRemoveIndex = i;
									}
								}

								if (elemOpen)
								{
									if (elemObj.IsValid())
										changed |= ReflectionPropertyDrawer::DrawPropertyTable(*elementType, elemObj, registry, editSettings);
									ImGui::TreePop();
								}
								ImGui::PopID();
							}

							if (canErase && toRemoveIndex < count)
							{
								if (property.TryErase(object, toRemoveIndex) == reflection::PropertyAccessError::None)
									changed = true;
							}
						}
						else
						{
							const int columns = canErase ? 3 : 2;
							const std::string valueHeader = singleElemProp != nullptr
								? GetReflectionDrawerDisplayName(singleElemProp->Metadata, singleElemProp->Name)
								: LOCAL("EditorCommon.Value");
							ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(fontSize * 0.3f, fontSize * 0.15f));
							if (ImGui::BeginTable(tableId.c_str(), columns, tableFlags))
							{
								ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 2.4f);
								ImGui::TableSetupColumn(valueHeader.c_str(), ImGuiTableColumnFlags_WidthStretch);
								if (canErase)
									ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, deleteWidth);
								ImGui::TableHeadersRow();

								std::size_t toRemoveIndex = std::numeric_limits<std::size_t>::max();
								for (std::size_t i = 0; i < count; ++i)
								{
									ImGui::TableNextRow();
									ImGui::PushID(static_cast<int>(i));

									ImGui::TableSetColumnIndex(0);
									ImGui::AlignTextToFramePadding();
									ImGui::Text("[%zu]", i);

									ImGui::TableSetColumnIndex(1);
									if (elementType != nullptr && singleElemProp != nullptr)
									{
										auto elemObj = (canEdit && isObjectSequence)
											? property.TryEditElementObject(object, i).Object
											: [&]()
											{
												const auto elemRead = property.TryReadElement(object, i);
												return elemRead ? elementType->AsObject(elemRead.Value) : reflection::ObjectView{};
											}();
										if (elemObj.IsValid())
											changed |= ReflectionPropertyDrawer::DrawValue(*singleElemProp, elemObj, registry, editSettings);
									}
									else if (const auto elemRead = property.TryReadElement(object, i))
									{
										const bool canEditElem = canEdit && (property.CanWriteElement() ||
											(registry && registry->FindEnum(elemRead.Value) != nullptr));
										changed |= DrawInlineValueView(elemRead.Value, registry, canEditElem,
											[&](const reflection::ValueView& newVal)
											{
												return property.TryWriteElement(object, i, newVal) ==
													reflection::PropertyAccessError::None;
											},
											[&](const reflection::EnumValue& newEnum)
											{
												return property.TryWriteEnumElement(object, i, newEnum) ==
													reflection::PropertyAccessError::None;
											});
									}

									if (canErase)
									{
										ImGui::TableSetColumnIndex(2);
										if (EditorUI::SmallIconButton("DelElem", ICON_FA_TRASH, "EditorCommon.Delete"))
											toRemoveIndex = i;
									}

									ImGui::PopID();
								}

								ImGui::EndTable();

								if (canErase && toRemoveIndex < count)
								{
									if (property.TryErase(object, toRemoveIndex) == reflection::PropertyAccessError::None)
										changed = true;
								}
							}
							ImGui::PopStyleVar();
						}
					}
				}

				if (indentW > 0.0f) ImGui::Unindent(indentW);
				ImGui::TreePop();
			}

			return changed;
		}
	}
}
