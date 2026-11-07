#include "ReflectionPropertyDrawer.h"
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
#include "PhysicsComponent.h"
#include <imgui_stdlib.h>
#include <archive/Unicode.h>
#include "ResourcePicker.h"
#include "SceneManager.h"
#include "Scene.h"
#include "CoreComponents.h"
#pragma push_macro("new")
#undef new
#include <imgui_internal.h>
#pragma pop_macro("new")


namespace
{
	void DrawResponsivePropertyLabel(const std::string& displayName, const std::string& typeName,
		float reservedRightWidth = 0.0f)
	{
		const float availWidth = ImGui::GetContentRegionAvail().x - reservedRightWidth;
		const ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());
		const bool isClipped = textSize.x > availWidth;
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(displayName.c_str());
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			if (isClipped)
			{
				ImGui::TextUnformatted(displayName.c_str());
				if (!typeName.empty()) ImGui::Separator();
			}
			if (!typeName.empty())
			{
				ImGui::TextDisabled("%s", typeName.c_str());
			}
			ImGui::EndTooltip();
		}
	}

    bool DrawInspectorText(std::string& value)
    {
        return ImGui::InputText("##Value", &value);
    }

    bool DrawInspectorText(std::wstring& value)
    {
        std::string text;
        if (!archive::detail::WideToUtf8(value, text)) return false;
        if (!ImGui::InputText("##Value", &text)) return false;
        return archive::detail::Utf8ToWide(text, value);
    }

    template<typename Matrix>
    bool DrawInspectorMatrix(Matrix& value)
    {
        bool changed = false;
        for (int column = 0; column < value.length(); ++column)
        {
            ImGui::PushID(column);
            changed |= ImGui::DragScalarN("##Column", ImGuiDataType_Float, glm::value_ptr(value[column]),
                value[column].length(), 0.01f);
            ImGui::PopID();
            for (int row = 0; row < value[column].length(); ++row)
                if (!std::isfinite(value[column][row])) return false;
        }
        return changed;
    }

    bool DrawInspectorEnum(const reflection::PropertyInfo& property, const reflection::ObjectView& object,
        const reflection::EnumInfo& enumeration, const reflection::ValueView& current, bool canEdit)
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
                if (!label.empty()) label += " | ";
                label += enumText(item.Name);
                covered |= mask;
            }
            if (covered != bits || label.empty())
            {
                if (!label.empty()) label += " | ";
                label += std::to_string(bits & ~covered);
            }
        }
        bool changed = false;
        ImGui::BeginDisabled(!canEdit);
        if (ImGui::BeginCombo("##Value", label.c_str()))
        {
            for (const auto& item : enumeration.Entries)
            {
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
        return property.TryWriteEnumValue(object, edited) == reflection::PropertyAccessError::None;
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

	bool DrawInspectorEntityPicker(entt::entity& value, bool canEdit)
	{
		bool changed = false;
		Engine::Scene* scene = Engine::SceneManager::Get().GetCurrentScene();
		if (!scene) return false;
		auto& world = scene->GetRegistry();
		// 1. 현재 할당된 엔티티 이름 결정
		std::string displayName = "None (Entity)";
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

		const bool showClearBtn = (value != entt::null && canEdit);
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
			if (ImGui::IsWindowAppearing())
			{
				searchBuffer[0] = '\0';
				ImGui::SetKeyboardFocusHere(0);
			}

			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##Search", LOCAL_CSTR("Inspector.SearchEntity"), searchBuffer, sizeof(searchBuffer));
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
	std::string_view TrimCondition(std::string_view str)
	{
		const auto start = str.find_first_not_of(" \t\r\n");
		if (start == std::string_view::npos) return {};
		const auto end = str.find_last_not_of(" \t\r\n");
		return str.substr(start, end - start + 1);
	}

	bool EvaluateEditCondition(const reflection::PropertyInfo& property, const reflection::TypeInfo& type,
		const reflection::ObjectView& object, const reflection::Registry* registry)
	{
		const auto* meta = property.Metadata.Find("EditCondition");
		const auto* condStr = meta ? std::get_if<std::string>(meta) : nullptr;
		if (!condStr || condStr->empty()) return true;

		std::string_view cond = *condStr;
		std::string_view left, right;
		bool isEquals = true;

		if (const auto pos = cond.find("=="); pos != std::string_view::npos)
		{
			left = TrimCondition(cond.substr(0, pos));
			right = TrimCondition(cond.substr(pos + 2));
			isEquals = true;
		}
		else if (const auto pos = cond.find("!="); pos != std::string_view::npos)
		{
			left = TrimCondition(cond.substr(0, pos));
			right = TrimCondition(cond.substr(pos + 2));
			isEquals = false;
		}
		else
		{
			// "IsEnabled" 또는 "!IsEnabled" 단일 bool 조건식
			bool negate = false;
			if (cond.starts_with('!')) { negate = true; cond = cond.substr(1); }
			const auto* targetProp = type.FindProperty(TrimCondition(cond));
			if (!targetProp) return true;
			const auto read = targetProp->TryReadValue(object);
			if (!read) return true;
			const auto* bVal = read.Value.Get<bool>();
			return bVal ? (*bVal != negate) : true;
		}

		const auto* targetProp = type.FindProperty(left);
		if (!targetProp) return true;
		const auto read = targetProp->TryReadValue(object);
		if (!read) return true;

		// ELightType::Spot 같은 형태에서 식별자("Spot")만 추출
		if (const auto colon = right.rfind("::"); colon != std::string_view::npos) right = right.substr(colon + 2);

		bool matches = false;
		if (const auto* enumeration = registry ? registry->FindEnum(read.Value) : nullptr)
		{
			const auto currentVal = enumeration->ReadValue(read.Value);
			const auto* entry = enumeration->FindEntry(right);
			if (currentVal && entry)
			{
				matches = std::visit([](auto a, auto b) { return std::cmp_equal(a, b); }, entry->Value, *currentVal);
			}
		}
		else if (const auto* bVal = read.Value.Get<bool>())
		{
			matches = (*bVal == (right == "true" || right == "1"));
		}

		return isEquals ? matches : !matches;
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
					return EvaluateEditCondition(property, type, object, registry);
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
		std::function<void()> onHeaderContextMenu)
	{
		if (!IsVisible(type)) return false;

		ImGui::PushID(type.QualifiedName.c_str());

		const std::string displayName = GetReflectionDrawerDisplayName(type.Metadata, type.QualifiedName);
		bool changed = false;

		const std::string headerLabel = displayName + "###ObjectHeader";
		const bool open = ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

		// [중요] CollapsingHeader 바로 직후에 호출해야 헤더바에만 정확히 우클릭 메뉴가 바인딩됩니다.
		if (onHeaderContextMenu && ImGui::BeginPopupContextItem("##HeaderContextMenu"))
		{
			onHeaderContextMenu();
			ImGui::EndPopup();
		}

		if (open)
		{
			changed = DrawPropertyTable(type, object, registry, editSettings);
		}

		ImGui::PopID();
		return changed;
	}

	bool ReflectionPropertyDrawer::DrawPropertyTable(const reflection::TypeInfo& type,
		const reflection::ObjectView& object, const reflection::Registry* registry,
		ReflectionObjectEditSettings* editSettings)
	{
		if (!IsVisible(type)) return false;

		ImGui::PushID(type.QualifiedName.c_str());

		const auto flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

		bool changed = false;

		std::vector<std::string> categories;
		for (const auto& property : type.Properties)
		{
			if (!GetReflectionDrawerBoolMetadata(property.Metadata, "Visible", true)) continue;
			if (!EvaluateEditCondition(property, type, object, registry)) continue; // 추가: 조건 거짓이면 카테고리 집계 제외
			const auto category = InspectorMetadataText(property.Metadata, "Category", "EditorCommon.General");
			if (std::ranges::find(categories, category) == categories.end()) categories.push_back(category);
		}
		for (const auto& category : categories)
		{
			ImGui::PushID(category.c_str());
			const auto categoryLabel = LocalizeInspectorText(category) + "###PropertyCategory";
			const auto categoryFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (!ImGui::TreeNodeEx(categoryLabel.c_str(), categoryFlags))
			{
				ImGui::PopID();
				continue;
			}
			if (ImGui::BeginTable("Properties", 2, flags))
			{
				// [반응형 컬럼 정책]: 라벨은 기본 120px를 확보하고, 값 컨트롤이 나머지 공간을 모두 차지
				ImGui::TableSetupColumn("##Property", ImGuiTableColumnFlags_WidthFixed, 120.0f);
				ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch);
				for (const auto& property : type.Properties)
				{
					if (InspectorMetadataText(property.Metadata, "Category", "EditorCommon.General") != category) continue;
					if (!GetReflectionDrawerBoolMetadata(property.Metadata, "Visible", true)) continue;
					if (!EvaluateEditCondition(property, type, object, registry)) continue;
					const std::string displayName = GetReflectionDrawerDisplayName(property.Metadata, property.Name);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					// 1. 자물쇠 버튼 유무 확인 및 우측 예약 공간 계산
					const bool hasLock = (editSettings != nullptr &&
						(editSettings->Vectors.contains(property.Name) || type.QualifiedName == "Engine::TransformComponent"));
					const float btnWidth = ImGui::GetFrameHeight() * 0.85f;
					const float rightPadding = 4.0f;
					const float reservedRightWidth = hasLock ? (btnWidth + rightPadding + ImGui::GetStyle().ItemSpacing.x) : 0.0f;
					// 2. 스마트 반응형 라벨 출력 (잘림 감지 및 통합 툴팁)
					DrawResponsivePropertyLabel(displayName, property.TypeName, reservedRightWidth);
					// 3. Transform 벡터 속성 자물쇠 아이콘 배치
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
					ImGui::TableSetColumnIndex(1);
					changed |= DrawValue(property, object, registry, editSettings);
				}

				ImGui::EndTable();
			}
			ImGui::TreePop();
			ImGui::PopID();
		}

		ImGui::PopID();
		return changed;
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
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[](std::string& candidate) { return DrawInspectorText(candidate); });
		}
		else if (const auto* current = read.Value.Get<std::wstring>())
		{
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[](std::wstring& candidate) { return DrawInspectorText(candidate); });
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
#undef INSPECTOR_RESOURCE
		else if (const auto* current = read.Value.Get<entt::entity>())
		{
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&](entt::entity& candidate) { return DrawInspectorEntityPicker(candidate, canEdit); });
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
		else if (const auto* tags = read.Value.Get<std::unordered_set<std::wstring>>())
		{
			std::vector<std::wstring> ordered(tags->begin(), tags->end());
			std::sort(ordered.begin(), ordered.end());
			ImGui::BeginDisabled(!canEdit);
			std::optional<std::wstring> removed;
			for (const auto& tag : ordered)
			{
				ImGui::PushID(Engine::WStrToStr(tag).c_str());
				ImGui::TextUnformatted(Engine::WStrToStr(tag).c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("X")) removed = tag;
				ImGui::PopID();
			}
			if (ImGui::Button(LocalizeInspectorText("Inspector.AddValue").c_str())) ImGui::OpenPopup("AddTag");
			if (ImGui::BeginPopup("AddTag"))
			{
				// Normal Inspector calls store input per entity and component in world.ctx().
				std::string temporary;
				auto& text = editSettings ? editSettings->TextBuffers[property.Name] : temporary;
				if (ImGui::InputText("##NewTag", &text, ImGuiInputTextFlags_EnterReturnsTrue) && canEdit)
				{
					std::wstring tag;
					if (!text.empty() && archive::detail::Utf8ToWide(text, tag))
						changed = property.TryInsertSetElement(object, reflection::ValueView::From(tag)) ==
							reflection::PropertyAccessError::None;
					if (changed) { text.clear(); ImGui::CloseCurrentPopup(); }
				}
				ImGui::EndPopup();
			}
			if (canEdit && removed) changed |= property.TryEraseSetElement(object,
				reflection::ValueView::From(*removed)) == reflection::PropertyAccessError::None;
			ImGui::EndDisabled();
		}
		else if (const auto* nested = registry ? registry->FindType(read.Value) : nullptr)
		{
			const auto view = canEdit ? property.TryEditObject(object).Object : nested->AsObject(read.Value);
			changed = view.IsValid() && DrawPropertyTable(*nested, view, registry, nullptr);
		}
		else if (property.IsContainer())
		{
			const auto sizeResult = property.TryGetSize(object);
			const std::size_t count = sizeResult ? sizeResult.Size : 0;
			const auto preview = sizeResult ? LOCAL_FMT("Inspector.ElementCount", count) : LOCAL("Inspector.SizeUnavailable");

			const auto* elementType = registry ? registry->FindType(property.GetElementCppType()) : nullptr;
			const bool isObjectSequence = (elementType != nullptr) && property.CanEditElementObject();

			// 컨테이너 트리 노드 헤더 (예: "요소 0개###ContainerTree_layers")
			const std::string treeLabel = fmt::format("{}###ContainerTree_{}", preview, property.Name);
			const bool nodeOpen = ImGui::TreeNode(treeLabel.c_str());

			// [+] 요소 추가 버튼: property.TryResize(count + 1) 호출
			if (property.CanResize() && canEdit)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton((ICON_FA_PLUS "##Add_" + property.Name).c_str()))
				{
					if (property.TryResize(object, count + 1) == reflection::PropertyAccessError::None)
					{
						changed = true;
					}
				}
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("Inspector.AddValue"));
			}

			if (nodeOpen)
			{
				std::size_t toRemoveIndex = std::numeric_limits<std::size_t>::max();

				for (std::size_t i = 0; i < count; ++i)
				{
					ImGui::PushID(static_cast<int>(i));

					if (isObjectSequence)
					{
						// 리플렉션 구조체 요소인 경우 서브 트리 노드 및 하위 프로퍼티 테이블 렌더링
						auto elementResult = property.TryEditElementObject(object, i);
						if (elementResult && elementResult.Object.IsValid())
						{
							std::string typeName = elementType->QualifiedName;
							if (const auto pos = typeName.rfind("::"); pos != std::string::npos) typeName = typeName.substr(pos + 2);
							const std::string elementLabel = fmt::format("[{}] {}###Element_{}", i, typeName, i);
							if (ImGui::TreeNode(elementLabel.c_str()))
							{
								if (property.CanErase() && canEdit)
								{
									ImGui::SameLine(ImGui::GetContentRegionAvail().x - 28.0f);
									if (ImGui::SmallButton(ICON_FA_TRASH "##DelElem")) toRemoveIndex = i;
								}

								changed |= DrawPropertyTable(*elementType, elementResult.Object, registry, editSettings);
								ImGui::TreePop();
							}
						}
					}
					ImGui::PopID();
				}

				// [-] 요소 삭제: property.TryErase(toRemoveIndex) 호출
				if (toRemoveIndex < count && property.CanErase() && canEdit)
				{
					if (property.TryErase(object, toRemoveIndex) == reflection::PropertyAccessError::None)
					{
						changed = true;
					}
				}

				ImGui::TreePop();
			}
		}
		else
		{
			const std::string preview = FormatReflectionDrawerValue(read.Value, registry);
			ImGui::TextUnformatted(preview.data(), preview.data() + preview.size());
		}

		ImGui::PopID();
		return changed;
	}
}
