#include "Editor_Includes.h"
#include "EditorUI.h"
#include "InputManager.h"
#include <imgui.h>
#include <imgui_internal.h>

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

    DialogLayout MeasureDialog(const EditorUI::DialogAction* actions, int count, float available)
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

float EditorUI::GetDialogButtonBarHeight(const DialogAction* actions, int count)
{
    return MeasureDialog(actions, count, std::max(1.0f, ImGui::GetContentRegionAvail().x)).Height;
}

int EditorUI::DialogButtonBar(const char* id, const DialogAction* actions, int count, const DialogButtonBarOptions& options)
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

    std::string BuildTextButtonDisplay(const char* text, const char* icon)
    {
        std::string display;
        if (icon && icon[0] != '\0')
        {
            display += icon;
            if (text && text[0] != '\0')
                display += ' ';
        }
        if (text)
            display += text;
        return display;
    }

    ImVec2 TextButtonPadding()
    {
        const float fontSize = ImGui::GetFontSize();
        const auto& padding = ImGui::GetStyle().FramePadding;
        return ImVec2(std::max(padding.x, fontSize * 0.45f), std::max(padding.y, fontSize * 0.2f));
    }
}

ImVec2 EditorUI::GetIconButtonSize(const char* icon, bool compact)
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

ImVec2 EditorUI::MeasureIconGrid(ImVec2 buttonSize, int count, float availableWidth)
{
    if (count <= 0) return ImVec2(0.0f, 0.0f);
    const auto spacing = ImGui::GetStyle().ItemSpacing;
    const int columns = std::clamp(static_cast<int>((std::max(0.0f, availableWidth) + spacing.x) /
        (buttonSize.x + spacing.x)), 1, count);
    const int rows = (count + columns - 1) / columns;
    return ImVec2(std::min(std::max(1.0f, availableWidth), columns * buttonSize.x + (columns - 1) * spacing.x),
        rows * buttonSize.y + (rows - 1) * spacing.y);
}

bool EditorUI::SmallIconButton(const char* id, const char* icon, const char* tooltipKey, bool enabled)
{
    IconButtonOptions options;
    options.Compact = true;
    options.Enabled = enabled;
    return IconButton(id, icon, tooltipKey, options);
}

void EditorUI::SameLineIfFits(float width)
{
    const float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    const float next = ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x;
    if (next + width <= right)
        ImGui::SameLine();
}

bool EditorUI::IconButton(const char* id, const char* icon, const char* tooltipKey, const IconButtonOptions& options)
{
    const auto minimum = GetIconButtonSize(icon, options.Compact);
    const ImVec2 size(std::max(minimum.x, options.Size.x), std::max(minimum.y, options.Size.y));
    if (options.SameLine)
        SameLineIfFits(size.x);
    if (!options.NoWrap && ImGui::GetContentRegionAvail().x < size.x)
        ImGui::NewLine();
    // 버튼 하나의 최소 크기도 확보할 수 없으면 잘린 버튼이나 가로 스크롤 영역을 만들지 않는다.
    if (!options.NoWrap && ImGui::GetContentRegionAvail().x < size.x)
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

ImVec2 EditorUI::GetTextButtonSize(const char* text, const TextButtonOptions& options)
{
    const std::string display = BuildTextButtonDisplay(text, options.Icon);
    const ImVec2 textSize = ImGui::CalcTextSize(display.c_str());
    const ImVec2 padding = TextButtonPadding();
    const float naturalWidth = textSize.x + padding.x * 2.0f;
    const float width = std::max(naturalWidth, options.MinWidth);
    const float baseHeight = std::max(ImGui::GetFrameHeight(), textSize.y + padding.y * 2.0f);
    const float height = options.Height > 0.0f ? std::max(baseHeight, options.Height) : baseHeight;
    return ImVec2(width, height);
}

bool EditorUI::TextButtonRaw(const char* id, const char* text, const TextButtonOptions& options)
{
    const std::string display = BuildTextButtonDisplay(text, options.Icon);
    const ImVec2 textSize = ImGui::CalcTextSize(display.c_str());
    const ImVec2 padding = TextButtonPadding();
    const float naturalWidth = textSize.x + padding.x * 2.0f;
    const float baseHeight = std::max(ImGui::GetFrameHeight(), textSize.y + padding.y * 2.0f);
    const float height = options.Height > 0.0f ? std::max(baseHeight, options.Height) : baseHeight;

    float width = std::max(naturalWidth, options.MinWidth);
    if (options.FullWidth)
        width = std::max(std::max(1.0f, ImGui::GetContentRegionAvail().x), options.MinWidth);
    else if (options.SameLine)
        SameLineIfFits(width);

    const std::string label = (id && id[0] != '\0') ? (display + "###" + id) : display;
    ImGui::BeginDisabled(!options.Enabled);

    int pushedColors = 0;
    if (options.Style == EButtonStyle::Danger)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.15f, 0.15f, 1.0f));
        pushedColors = 3;
    }
    else if (options.Style == EButtonStyle::Primary)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.48f, 0.82f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.06f, 0.53f, 0.98f, 1.0f));
        pushedColors = 3;
    }
    else if (options.Active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        pushedColors = 1;
    }

    const bool pressed = ImGui::Button(label.c_str(), ImVec2(width, height));

    if (pushedColors > 0)
        ImGui::PopStyleColor(pushedColors);

    const bool textClipped = (width - ImGui::GetStyle().FramePadding.x * 2.0f) < textSize.x - 0.5f;
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        std::string tooltip;
        if (options.TooltipText && options.TooltipText[0] != '\0')
            tooltip = options.TooltipText;
        else if (options.TooltipKey && options.TooltipKey[0] != '\0')
            tooltip = LOCAL(options.TooltipKey);
        else if (textClipped && !display.empty())
            tooltip = display;

        if (!tooltip.empty())
        {
            if (options.ShortcutHint && options.ShortcutHint[0] != '\0')
                ImGui::SetTooltip("%s (%s)", tooltip.c_str(), options.ShortcutHint);
            else
                ImGui::SetTooltip("%s", tooltip.c_str());
        }
        else if (options.ShortcutHint && options.ShortcutHint[0] != '\0')
        {
            ImGui::SetTooltip("%s", options.ShortcutHint);
        }
    }

    ImGui::EndDisabled();
    return pressed;
}

bool EditorUI::TextButton(const char* id, const char* labelKey, const TextButtonOptions& options)
{
    const std::string translated = (labelKey && labelKey[0] != '\0') ? LOCAL(labelKey) : std::string();
    return TextButtonRaw(id, translated.c_str(), options);
}

bool EditorUI::ToolbarLayout::Icon(const char* id, const char* icon, const char* tooltipKey, const IconButtonOptions& options)
{
    auto buttonOptions = options;
    buttonOptions.SameLine = m_HasItem;
    const bool pressed = EditorUI::IconButton(id, icon, tooltipKey, buttonOptions);
    m_HasItem = true;
    return pressed;
}

bool EditorUI::ToolbarLayout::Button(const char* id, const char* icon, const char* labelKey)
{
    TextButtonOptions options;
    options.Icon = icon;
    options.TooltipKey = labelKey;
    const std::string text = LOCAL(labelKey);
    const float width = EditorUI::GetTextButtonSize(text.c_str(), options).x;
    if (width > ImGui::GetContentRegionAvail().x)
        return Icon(id, icon, labelKey);
    options.SameLine = m_HasItem;
    m_HasItem = true;
    return EditorUI::TextButtonRaw(id, text.c_str(), options);
}

void EditorUI::ToolbarLayout::SetNextItemWidth(float preferredWidth)
{
    const float width = std::min(preferredWidth, std::max(1.0f, ImGui::GetContentRegionAvail().x));
    if (m_HasItem)
        EditorUI::SameLineIfFits(width);
    ImGui::SetNextItemWidth(width);
    m_HasItem = true;
}

void EditorUI::ToolbarLayout::Text(const char* text, bool disabled)
{
    const float width = ImGui::CalcTextSize(text).x;
    if (m_HasItem)
        EditorUI::SameLineIfFits(width);
    if (disabled)
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    if (disabled)
        ImGui::PopStyleColor();
    m_HasItem = true;
}

void EditorUI::EmptyState(const char* messageKey)
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

bool EditorUI::SearchBar(const char* id, char* buffer, size_t bufferSize, const char* hintKey)
{
    return EditorUI::DrawSearchBar(id, buffer, bufferSize, hintKey).Changed;
}

EditorUI::SearchBarResult EditorUI::DrawSearchBar(const char* id, char* buffer, size_t bufferSize,
    const char* hintKey, const SearchBarOptions& options)
{
    EditorUI::SearchBarResult result;
    if (!buffer || bufferSize == 0)
        return result;

    const std::string previousText(buffer);
    const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const float width = options.Width > 0.0f ? std::min(options.Width, availableWidth) : availableWidth;

    const float frameHeight = ImGui::GetFrameHeight();
    const float buttonSize = frameHeight;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float iconWidth = ImGui::CalcTextSize(ICON_FA_MAGNIFYING_GLASS).x;
    const bool showIcon = width >= buttonSize * 3.0f + iconWidth + spacing * 2.0f;
    const bool showClear = width >= buttonSize + spacing + ImGui::GetFontSize();

    // 4.0px 안전 마진을 두어 DPI 스케일링/픽셀 스냅 오차 발생 시에도 버튼 폭이 보장되도록 설정
    const float reservedForClear = showClear ? (buttonSize + spacing + 4.0f) : 0.0f;
    const float reservedForIcon = showIcon ? (iconWidth + spacing) : 0.0f;
    const float inputWidth = std::max(1.0f, width - reservedForClear - reservedForIcon);
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
    ImGui::SetNextItemWidth(inputWidth);
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
    if (showClear)
    {
        ImGui::SameLine(0.0f, spacing);
        IconButtonOptions clearOptions;
        clearOptions.Enabled = (buffer[0] != '\0');
        clearOptions.SameLine = false; // 수동으로 SameLine을 불렀으므로 내부 SameLineIfFits 비활성화
        clearOptions.NoWrap = true;   // [핵심] 검색바 내 인라인 요소이므로 강제 개행 원천 차단
        clearOptions.Compact = true;
        clearOptions.Size = ImVec2(buttonSize, buttonSize);
        if (IconButton("ClearSearch", ICON_FA_XMARK, "EditorCommon.Clear", clearOptions))
        {
            buffer[0] = '\0';
            result.Cleared = true;
        }
    }
    ImGui::EndGroup();
    ImGui::PopID();
    result.Changed = (previousText != buffer);
    return result;
}

bool EditorUI::BeginSettingsTable(const char* id, int columns, ImGuiTableFlags flags, const TableStyle& style)
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

void EditorUI::EndSettingsTable()
{
    ImGui::EndTable();
    ImGui::PopStyleVar();
}

bool EditorUI::BeginPropertyTable(const char* id, const PropertyTableStyle& style)
{
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_SizingStretchProp;
    if (!EditorUI::BeginSettingsTable(id, 2, flags, style))
        return false;

    ImGui::TableSetupColumn("##Label", ImGuiTableColumnFlags_WidthFixed, style.LabelWidth);
    ImGui::TableSetupColumn("##Widget", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void EditorUI::EndPropertyTable()
{
    EditorUI::EndSettingsTable();
}

namespace
{
    std::string TruncateTextWithEllipsis(const char* text, float maxWidth, bool& outClipped)
    {
        outClipped = false;
        if (!text || text[0] == '\0')
            return {};
        if (maxWidth <= 0.0f || ImGui::CalcTextSize(text).x <= maxWidth)
            return text;

        outClipped = true;
        constexpr const char* kEllipsis = "...";
        const float ellipsisWidth = ImGui::CalcTextSize(kEllipsis).x;
        if (maxWidth <= ellipsisWidth)
            return kEllipsis;

        const float targetWidth = maxWidth - ellipsisWidth;
        const size_t len = std::strlen(text);
        size_t bestEnd = 0;
        size_t pos = 0;
        while (pos < len)
        {
            const unsigned char c = static_cast<unsigned char>(text[pos]);
            size_t charLen = 1;
            if ((c & 0x80u) == 0u) charLen = 1;
            else if ((c & 0xE0u) == 0xC0u) charLen = 2;
            else if ((c & 0xF0u) == 0xE0u) charLen = 3;
            else if ((c & 0xF8u) == 0xF0u) charLen = 4;

            const size_t nextPos = std::min(pos + charLen, len);
            if (ImGui::CalcTextSize(text, text + nextPos).x > targetWidth)
                break;
            bestEnd = nextPos;
            pos = nextPos;
        }

        std::string result(text, bestEnd);
        result += kEllipsis;
        return result;
    }
}

void EditorUI::TextEllipsis(const char* text, float maxWidth, const char* tooltipDetail)
{
    if (!text) text = "";
    const float availWidth = (maxWidth > 0.0f) ? maxWidth : ImGui::GetContentRegionAvail().x;
    bool isClipped = false;
    const std::string display = TruncateTextWithEllipsis(text, availWidth, isClipped);
    const bool hasDetail = tooltipDetail != nullptr && tooltipDetail[0] != '\0';

    ImGui::TextUnformatted(display.c_str());
    if ((isClipped || hasDetail) && ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        if (isClipped) ImGui::TextUnformatted(text);
        if (isClipped && hasDetail) ImGui::Separator();
        if (hasDetail) ImGui::TextDisabled("%s", tooltipDetail);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void EditorUI::ReadOnlyTextField(const char* id, const char* text, float width)
{
    if (!text) text = "";
    const float availWidth = (width > 0.0f) ? width : ImGui::GetContentRegionAvail().x;
    const float framePaddingX = ImGui::GetStyle().FramePadding.x * 2.0f;
    const bool isClipped = ImGui::CalcTextSize(text).x > std::max(1.0f, availWidth - framePaddingX);

    std::string buffer(text);
    ImGui::SetNextItemWidth(width);
    ImGui::InputText(id, buffer.data(), buffer.size() + 1, ImGuiInputTextFlags_ReadOnly);

    if (isClipped && ImGui::IsItemHovered() && !ImGui::IsItemActive())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void EditorUI::PropertyLabel(const char* text, const char* detail, float reservedRightWidth)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);

    const float availWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - reservedRightWidth);
    ImGui::AlignTextToFramePadding();
    EditorUI::TextEllipsis(text, availWidth, detail);
}

void EditorUI::PropertyValue()
{
    ImGui::TableSetColumnIndex(1);
}

void EditorUI::PropertyRow(const char* labelKey)
{
    EditorUI::PropertyLabel(LOCAL_CSTR(labelKey));
    EditorUI::PropertyValue();
}

EditorUI::PropertyTableStyle EditorUI::InspectorPropertyTableStyle()
{
    EditorUI::PropertyTableStyle style;
    style.LabelWidth = ImGui::GetFontSize() * 9.0f;
    style.PaddingXInFontUnits = 0.3f;
    style.PaddingYInFontUnits = 0.15f;
    return style;
}