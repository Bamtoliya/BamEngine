#include "ReflectionPropertyDrawer.h"
#include "Functions.h"

#include "IconsFontAwesome7.h"
#include <reflection/Registry.h>
#include <imgui.h>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <string>
#include <variant>
#include <algorithm>
#include <cmath>
#include "Editor_Macros.h"
#include "LocalizationManager.h"
#include "ReflectionInspectorSupport.h"
#include "PhysicsComponent.h"
#include <imgui_stdlib.h>
#include <archive/Unicode.h>
#include <limits>

namespace
{
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

    template<typename T>
    bool DrawResourcePicker(Engine::ResourceHandle<T>& value, bool canEdit)
    {
        auto& manager = Engine::ResourceManager::Get();
        auto* current = manager.GetResource(value.GetRawHandle());
        const auto label = current != nullptr ? Engine::WStrToStr(current->GetKey()) :
            Editor::LocalizeInspectorText("UI_RESOURCE_NONE");
        bool changed = false;
        bool failed = ImGui::GetStateStorage()->GetBool(ImGui::GetID("DropFailed"), false);
        ImGui::BeginDisabled(!canEdit);
        const auto button = label + "###ResourcePicker";
        if (ImGui::Button(button.c_str(), ImVec2(-1.0f, 0.0f))) ImGui::OpenPopup("ResourceChoices");
        if (canEdit && ImGui::BeginDragDropTarget())
        {
            if (const auto* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
            {
                const auto path = Editor::DecodeInspectorResourcePath(payload->Data, payload->DataSize);
                const auto handle = path ? manager.LoadFile(*path) : Engine::Handle{};
                changed = Editor::TryAssignInspectorResource(value, handle);
                failed = !changed;
            }
            ImGui::EndDragDropTarget();
        }
        if (ImGui::BeginPopup("ResourceChoices"))
        {
            if (ImGui::Selectable(Editor::LocalizeInspectorText("UI_RESOURCE_CLEAR").c_str()))
            {
                value.Reset();
                changed = true;
                failed = false;
            }
            for (const auto& entry : manager.GetDebugSnapshot())
            {
                const auto handle = manager.FindHandleByKey(entry.Key);
                if (dynamic_cast<T*>(manager.GetResource(handle)) == nullptr) continue;
                const auto text = Engine::WStrToStr(entry.Key);
                ImGui::PushID(text.c_str());
                if (ImGui::Selectable(text.c_str(), handle == value.GetRawHandle()))
                {
                    changed = Editor::TryAssignInspectorResource(value, handle);
                    failed = !changed;
                }
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
        ImGui::GetStateStorage()->SetBool(ImGui::GetID("DropFailed"), failed);
        if (failed) ImGui::TextDisabled("%s", Editor::LocalizeInspectorText("UI_RESOURCE_REJECTED").c_str());
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
            return localized == key ? Editor::LocalizeInspectorText(name) : localized;
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

		return "Value preview unavailable";
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

		if (value == nullptr)
		{
			return fallback;
		}

		double result = fallback;

		if (const auto* number = std::get_if<double>(value))
		{
			result = *number;
		}
		else if (const auto* number = std::get_if<std::int64_t>(value))
		{
			result = static_cast<double>(*number);
		}
		else if (const auto* number = std::get_if<std::uint64_t>(value))
		{
			result = static_cast<double>(*number);
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
		return DrawReflectionPropertyEditor(property, object, current, canEdit,
			[dataType](T& candidate)
			{
				return ImGui::InputScalar("##Value", dataType, &candidate);
			});
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
			const ImGuiIO& io = ImGui::GetIO();

			if (!locked && hovered)
			{
				// Window의 스크롤 대신 이 입력칸이 휠을 사용합니다.
				ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

				if (io.MouseWheel != 0.0f)
				{
					const float wheelStep = snapEnabled ? snapStep : dragSpeed;
					const float multiplier = io.KeyShift ? 10.0f : 1.0f;

					rawValue += io.MouseWheel * wheelStep * multiplier;
					requested = true;
				}

				if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
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

	bool ReflectionPropertyDrawer::IsVisible(const reflection::TypeInfo& type)
	{
		return GetReflectionDrawerBoolMetadata(type.Metadata, "Visible", true);
	}

	bool ReflectionPropertyDrawer::DrawObject(const reflection::TypeInfo& type, const reflection::ObjectView& object,
		const reflection::Registry* registry, ReflectionObjectEditSettings* editSettings)
	{
		if (!IsVisible(type)) return false;

		ImGui::PushID(type.QualifiedName.c_str());

		const std::string displayName = GetReflectionDrawerDisplayName(type.Metadata, type.QualifiedName);
		bool changed = false;

		const std::string headerLabel = displayName + "###ObjectHeader";

		if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
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
			const auto category = InspectorMetadataText(property.Metadata, "Category", "UI_GENERAL");
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
				ImGui::TableSetupColumn("##Property", ImGuiTableColumnFlags_WidthStretch, 0.25f);
				ImGui::TableSetupColumn("##Value", ImGuiTableColumnFlags_WidthStretch, 0.75f);

				for (const auto& property : type.Properties)
				{
					if (InspectorMetadataText(property.Metadata, "Category", "UI_GENERAL") != category) continue;
					if (!GetReflectionDrawerBoolMetadata(property.Metadata, "Visible", true))
					{
						continue;
					}

					const std::string displayName = GetReflectionDrawerDisplayName(property.Metadata, property.Name);

					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(displayName.data(), displayName.data() + displayName.size());

					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("%s", property.TypeName.c_str());
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
			ImGui::TextDisabled("%s", LOCAL("UI_VALUE_UNAVAILABLE").c_str());
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
				[&](auto& candidate) { return DrawResourcePicker(candidate, canEdit); }); \
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
#undef INSPECTOR_RESOURCE
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
			if (ImGui::Button(LocalizeInspectorText("UI_ADD_VALUE").c_str())) ImGui::OpenPopup("AddTag");
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
			const auto size = property.TryGetSize(object);
			const auto preview = size ? LOCAL_FMT("UI_ELEMENT_COUNT", size.Size) : LOCAL("UI_SIZE_UNAVAILABLE");
			ImGui::TextDisabled("%s", preview.c_str());
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
