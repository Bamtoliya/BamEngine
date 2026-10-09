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

		return LOCAL(*key);
	}

	std::string FormatReflectionDrawerValue(const reflection::ValueView& value, const reflection::Registry* registry)
	{
		if (const auto* text = value.Get<std::string>()) return *text;
		if (const auto* text = value.Get<std::wstring>()) return Engine::WStrToStr(*text);
		if (const auto* number = value.Get<bool>()) return *number ? "true" : "false";
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

		const auto error = property.TryWriteValue(object, reflection::ValueView::From(candidate));

		if (error != reflection::PropertyAccessError::None)
		{
			const auto message = reflection::ToString(error);
			ImGui::TextDisabled("Write failed: %.*s", static_cast<int>(message.size()), message.data());
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

		ImGui::PushID(componentCount == 3 ? "Vector3" : "Vector4");

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
				ImGui::SetTooltip("Reset %s to %.3f", axisNames[axis], resetValue);
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
				ImGui::SetTooltip(locked ? "Unlock %s" : "Lock %s", axisNames[axis]);
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
			ImGui::TextUnformatted("Edit settings");
			ImGui::Separator();

			ImGui::Checkbox("Snap", &snapEnabled);

			float editedStep = snapStep;
			if (ImGui::InputFloat("Snap step", &editedStep) &&
				std::isfinite(editedStep) && editedStep > 0.0f)
			{
				snapStep = editedStep;
			}

			ImGui::Separator();
			ImGui::TextDisabled("Wheel: change value");
			ImGui::TextDisabled("Shift + wheel: x10");
			ImGui::TextDisabled("Axis button / middle click: reset");

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

	bool ReflectionPropertyDrawer::DrawObject(const reflection::TypeInfo& type, const reflection::ObjectView& object,
		const reflection::Registry* registry, ReflectionObjectEditSettings* editSettings)
	{
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
		ImGui::PushID(type.QualifiedName.c_str());

		const auto flags = ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;

		bool changed = false;

		if (ImGui::BeginTable("Properties", 2, flags))
		{
			ImGui::TableSetupColumn(LOCAL("UI_PROPERTY").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.25f);
			ImGui::TableSetupColumn(LOCAL("UI_VALUE").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.75f);
			ImGui::TableHeadersRow();

			for (const auto& property : type.Properties)
			{
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

		if (property.IsContainer())
		{
			const auto size = property.TryGetSize(object);
			const std::string preview = size ? LOCAL_FMT("UI_ELEMENT_COUNT", size.Size) : LOCAL("UI_SIZE_UNAVAILABLE");

			ImGui::TextUnformatted(preview.data(), preview.data() + preview.size());
			ImGui::PopID();
			return false;
		}

		const bool editable = GetReflectionDrawerBoolMetadata(property.Metadata, "Editable", false);
		const bool readOnly = GetReflectionDrawerBoolMetadata(property.Metadata, "ReadOnly", false);
		const bool canEdit = editable && !readOnly && property.CanWrite() && !object.IsReadOnly();

		bool changed = false;

		if (const auto* current = read.Value.Get<bool>())
		{
			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[](bool& candidate)
				{
					return ImGui::Checkbox("##Value", &candidate);
				});
		}
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
		else if (const auto* current = read.Value.Get<glm::vec3>())
		{
			auto* vectorSettings = editSettings != nullptr ? &editSettings->Vectors[property.Name] : nullptr;

			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&property, vectorSettings](glm::vec3& candidate)
				{
					return DrawVector3(property.Metadata, candidate, vectorSettings);
				});
		}
		else if (const auto* current = read.Value.Get<glm::vec4>())
		{
			auto* vectorSettings = editSettings != nullptr ? &editSettings->Vectors[property.Name] : nullptr;

			changed = DrawReflectionPropertyEditor(property, object, *current, canEdit,
				[&property, vectorSettings](glm::vec4& candidate)
				{
					return DrawVector4(property.Metadata, candidate, vectorSettings);
				});
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