#include "Editor_Includes.h"
#include "EditorUI.h"
#include "InputManager.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace EditorUI
{
    namespace
    {
        struct DialogLayout
        {
            std::vector<std::string> Labels;
            std::vector<float> Widths;
            std::vector<float> Heights;
            float TotalWidth = 0.0f;
            float Height = 0.0f;
            bool OneRow = true;
        };

        DialogLayout MeasureDialog(const DialogAction* actions, int count, float available)
        {
            DialogLayout layout;
            if (!actions || count <= 0) return layout;
            const auto& style = ImGui::GetStyle();
            layout.TotalWidth = style.ItemSpacing.x * (count - 1);
            for (int i = 0; i < count; ++i)
            {
                layout.Labels.push_back(LOCAL(actions[i].LabelKey));
                const float width = std::max(ImGui::GetFontSize() * 5.0f,
                    ImGui::CalcTextSize(layout.Labels.back().c_str()).x + style.FramePadding.x * 2.0f);
                layout.Widths.push_back(std::min(width, available));
                layout.TotalWidth += width;
                const float inset = std::min(style.FramePadding.x, layout.Widths.back() * 0.2f);
                const float wrapWidth = std::max(1.0f, layout.Widths.back() - inset * 2.0f);
                const auto textSize = ImGui::CalcTextSize(layout.Labels.back().c_str(), nullptr, false, wrapWidth);
                layout.Heights.push_back(std::max(ImGui::GetFrameHeight(), textSize.y + style.FramePadding.y * 2.0f));
            }
            layout.OneRow = layout.TotalWidth <= available;
            for (const float height : layout.Heights)
                layout.Height = layout.OneRow ? std::max(layout.Height, height) : layout.Height + height;
            if (!layout.OneRow) layout.Height += style.ItemSpacing.y * (count - 1);
            return layout;
        }
    }

    float GetDialogButtonBarHeight(const DialogAction* actions, int count)
    {
        return MeasureDialog(actions, count, std::max(1.0f, ImGui::GetContentRegionAvail().x)).Height;
    }

    int DialogButtonBar(const char* id, const DialogAction* actions, int count, const DialogButtonBarOptions& options)
    {
        if (!actions || count <= 0)
            return -1;

        const float available = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float left = ImGui::GetCursorPosX();
        const auto padding = ImGui::GetStyle().FramePadding;
        const auto layout = MeasureDialog(actions, count, available);
        const auto& labels = layout.Labels;
        const auto& widths = layout.Widths;
        const float totalWidth = layout.TotalWidth;
        const bool oneRow = layout.OneRow;
        const bool keyboardAllowed = ImGui::IsWindowFocused() && !ImGui::IsWindowAppearing() &&
            !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive();
        int selected = -1;
        int focused = -1;
        ImGui::PushID(id);
        for (int i = 0; i < count; ++i)
        {
            const float width = std::min(widths[i], available);
            if (oneRow && i > 0)
                ImGui::SameLine();
            else
                ImGui::SetCursorPosX(left + available - (oneRow ? totalWidth : width));

            const float inset = std::min(padding.x, width * 0.2f);
            const float wrapWidth = std::max(1.0f, width - inset * 2.0f);
            const auto textSize = ImGui::CalcTextSize(labels[i].c_str(), nullptr, false, wrapWidth);
            const float height = layout.Heights[i];

            ImGui::PushID(i);
            ImGui::BeginDisabled(!actions[i].Enabled);
            if (actions[i].Destructive)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
            }
            if (ImGui::Button("##Action", ImVec2(width, height)))
                selected = i;
            if (ImGui::IsItemFocused())
                focused = i;
            const auto start = ImGui::GetItemRectMin();
            const ImVec2 textPos(start.x + std::max(inset, (width - textSize.x) * 0.5f),
                start.y + (height - textSize.y) * 0.5f);
            ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos,
                ImGui::GetColorU32(ImGuiCol_Text), labels[i].c_str(), nullptr, wrapWidth);
            if (actions[i].Destructive)
                ImGui::PopStyleColor(3);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        ImGui::PopID();
        if (selected < 0 && keyboardAllowed)
        {
            auto& input = Engine::InputManager::Get();
            if (input.IsKeyButtonDown(Engine::EKeyCode::Escape) && options.CancelIndex >= 0 &&
                options.CancelIndex < count && actions[options.CancelIndex].Enabled)
                selected = options.CancelIndex;
            else if (input.IsKeyButtonDown(Engine::EKeyCode::Enter) ||
                input.IsKeyButtonDown(Engine::EKeyCode::NumpadEnter))
            {
                const int target = focused >= 0 ? focused : count - 1;
                if (actions[target].Enabled)
                    selected = target;
            }
        }
        return selected;
    }

    namespace
    {
        ImVec2 IconPadding()
        {
            const float minimum = ImGui::GetFontSize() * 0.2f;
            const auto& padding = ImGui::GetStyle().FramePadding;
            return ImVec2(std::max(padding.x, minimum), std::max(padding.y, minimum));
        }
    }

    ImVec2 GetIconButtonSize(const char* icon, bool compact)
    {
        if (compact)
        {
            const auto text = ImGui::CalcTextSize(icon);
            const float padding = ImGui::GetFontSize() * 0.15f;
            return ImVec2(std::max(text.x, text.y) + padding * 2.0f, text.y + padding * 2.0f);
        }
        const auto text = ImGui::CalcTextSize(icon);
        const auto padding = IconPadding();
        const float side = std::max(ImGui::GetFrameHeight(),
            std::max(text.x + padding.x * 2.0f, text.y + padding.y * 2.0f));
        return ImVec2(side, side);
    }

    ImVec2 MeasureIconGrid(ImVec2 buttonSize, int count, float availableWidth)
    {
        if (count <= 0) return ImVec2(0.0f, 0.0f);
        const auto spacing = ImGui::GetStyle().ItemSpacing;
        const int columns = std::clamp(static_cast<int>((std::max(0.0f, availableWidth) + spacing.x) /
            (buttonSize.x + spacing.x)), 1, count);
        const int rows = (count + columns - 1) / columns;
        return ImVec2(std::min(std::max(1.0f, availableWidth), columns * buttonSize.x + (columns - 1) * spacing.x),
            rows * buttonSize.y + (rows - 1) * spacing.y);
    }

    bool SmallIconButton(const char* id, const char* icon, const char* tooltipKey, bool enabled)
    {
        IconButtonOptions options;
        options.Compact = true;
        options.Enabled = enabled;
        return IconButton(id, icon, tooltipKey, options);
    }

    void SameLineIfFits(float width)
    {
        const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
        const float next = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;
        if (next + width <= right)
            ImGui::SameLine();
    }

    bool IconButton(const char* id, const char* icon, const char* tooltipKey, const IconButtonOptions& options)
    {
        const auto minimum = GetIconButtonSize(icon, options.Compact);
        const ImVec2 size(std::max(minimum.x, options.Size.x), std::max(minimum.y, options.Size.y));
        if (options.SameLine)
            SameLineIfFits(size.x);
        if (ImGui::GetContentRegionAvail().x < size.x)
            ImGui::NewLine();
        // 버튼 하나의 최소 크기도 확보할 수 없으면 잘린 버튼이나 가로 스크롤 영역을 만들지 않는다.
        if (ImGui::GetContentRegionAvail().x < size.x)
            return false;

        const std::string label = std::string(icon) + "###" + id;
        const float compactPadding = ImGui::GetFontSize() * 0.15f;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, options.Compact ? ImVec2(compactPadding, compactPadding) : IconPadding());
        ImGui::BeginDisabled(!options.Enabled);
        if (options.Active)
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        const bool pressed = ImGui::Button(label.c_str(), size);
        if (options.Active)
            ImGui::PopStyleColor();
        if (tooltipKey && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            const auto tooltip = LOCAL(tooltipKey);
            if (options.ShortcutHint && options.ShortcutHint[0] != '\0')
                ImGui::SetTooltip("%s (%s)", tooltip.c_str(), options.ShortcutHint);
            else
                ImGui::SetTooltip("%s", tooltip.c_str());
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar();
        return pressed;
    }

    bool ToolbarLayout::Icon(const char* id, const char* icon, const char* tooltipKey, const IconButtonOptions& options)
    {
        auto buttonOptions = options;
        buttonOptions.SameLine = m_HasItem;
        const bool pressed = IconButton(id, icon, tooltipKey, buttonOptions);
        m_HasItem = true;
        return pressed;
    }

    bool ToolbarLayout::Button(const char* id, const char* icon, const char* labelKey)
    {
        const std::string text = std::string(icon) + " " + LOCAL(labelKey);
        const float width = ImGui::CalcTextSize(text.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (width > ImGui::GetContentRegionAvail().x)
            return Icon(id, icon, labelKey);
        if (m_HasItem)
            SameLineIfFits(width);
        m_HasItem = true;
        const bool pressed = ImGui::Button((text + "###" + id).c_str());
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", LOCAL_CSTR(labelKey));
        return pressed;
    }

    void ToolbarLayout::SetNextItemWidth(float preferredWidth)
    {
        const float width = std::min(preferredWidth, std::max(1.0f, ImGui::GetContentRegionAvail().x));
        if (m_HasItem)
            SameLineIfFits(width);
        ImGui::SetNextItemWidth(width);
        m_HasItem = true;
    }

    void ToolbarLayout::Text(const char* text, bool disabled)
    {
        const float width = ImGui::CalcTextSize(text).x;
        if (m_HasItem)
            SameLineIfFits(width);
        if (disabled)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        if (disabled)
            ImGui::PopStyleColor();
        m_HasItem = true;
    }

    void EmptyState(const char* messageKey)
    {
        const auto message = LOCAL(messageKey);
        const auto available = ImGui::GetContentRegionAvail();
        const float wrapWidth = std::max(1.0f, std::min(available.x, ImGui::GetFontSize() * 32.0f));
        const auto size = ImGui::CalcTextSize(message.c_str(), nullptr, false, wrapWidth);
        auto cursor = ImGui::GetCursorPos();
        cursor.x += std::max(0.0f, (available.x - size.x) * 0.5f);
        cursor.y += std::max(0.0f, (available.y - size.y) * 0.5f);
        ImGui::SetCursorPos(cursor);
        ImGui::PushTextWrapPos(cursor.x + wrapWidth);
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(message.c_str());
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
    }

    bool SearchBar(const char* id, char* buffer, size_t bufferSize, const char* hintKey)
    {
        return DrawSearchBar(id, buffer, bufferSize, hintKey).Changed;
    }

    SearchBarResult DrawSearchBar(const char* id, char* buffer, size_t bufferSize,
        const char* hintKey, const SearchBarOptions& options)
    {
        SearchBarResult result;
        if (!buffer || bufferSize == 0)
            return result;

        const std::string previousText(buffer);
        const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        const float width = options.Width > 0.0f ? std::min(options.Width, availableWidth) : availableWidth;
        const float buttonSize = GetIconButtonSize(ICON_FA_XMARK).x;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float iconWidth = ImGui::CalcTextSize(ICON_FA_MAGNIFYING_GLASS).x;
        const bool showIcon = width >= buttonSize * 3.0f + iconWidth + spacing * 2.0f;
        const bool showClear = width >= buttonSize + spacing + ImGui::GetFontSize();
        const float inputWidth = width - (showClear ? buttonSize + spacing : 0.0f) -
            (showIcon ? iconWidth + spacing : 0.0f);

        ImGui::PushID(id);
        ImGui::BeginGroup();
        if (showIcon)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(ICON_FA_MAGNIFYING_GLASS);
            ImGui::SameLine();
        }

        const ImGuiID inputId = ImGui::GetID("##SearchInput");
        const bool clearOnEscape = ImGui::GetActiveID() == inputId &&
            ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
            Engine::InputManager::Get().IsKeyButtonDown(Engine::EKeyCode::Escape);

        if (options.FocusOnDraw)
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(std::max(1.0f, inputWidth));
        result.Submitted = ImGui::InputTextWithHint("##SearchInput", LOCAL_CSTR(hintKey), buffer, bufferSize,
            ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_EnterReturnsTrue);

        if (clearOnEscape)
        {
            buffer[0] = '\0';
            result.Cleared = true;
            result.Submitted = false;
            if (ImGui::GetActiveID() == inputId)
                ImGui::ClearActiveID();
        }

        IconButtonOptions clearOptions;
        clearOptions.Enabled = buffer[0] != '\0';
        clearOptions.SameLine = true;
        if (showClear && IconButton("ClearSearch", ICON_FA_XMARK, "EditorCommon.Clear", clearOptions))
        {
            buffer[0] = '\0';
            result.Cleared = true;
        }
        ImGui::EndGroup();
        ImGui::PopID();
        result.Changed = previousText != buffer;
        return result;
    }

    bool BeginSettingsTable(const char* id, int columns, ImGuiTableFlags flags, const TableStyle& style)
    {
        if (style.Resizable)
            flags |= ImGuiTableFlags_Resizable;

        const float fontSize = ImGui::GetFontSize();
        const ImVec2 padding(fontSize * style.PaddingXInFontUnits, fontSize * style.PaddingYInFontUnits);
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, padding);

        if (!ImGui::BeginTable(id, columns, flags))
        {
            ImGui::PopStyleVar();
            return false;
        }

        return true;
    }

    void EndSettingsTable()
    {
        ImGui::EndTable();
        ImGui::PopStyleVar();
    }

    bool BeginPropertyTable(const char* id, const PropertyTableStyle& style)
    {
        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_SizingStretchProp;
        if (!BeginSettingsTable(id, 2, flags, style))
            return false;

        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, style.LabelWidth);
        ImGui::TableSetupColumn("Widget", ImGuiTableColumnFlags_WidthStretch);
        return true;
    }

    void EndPropertyTable()
    {
        EndSettingsTable();
    }

    void PropertyRow(const char* labelKey)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(LOCAL_CSTR(labelKey));
        ImGui::TableSetColumnIndex(1);
    }
}
