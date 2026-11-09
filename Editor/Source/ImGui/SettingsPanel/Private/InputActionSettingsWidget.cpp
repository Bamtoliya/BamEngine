#include "InputActionSettingsWidget.h"
#include "EditorSettingsService.h"
#include "InputManager.h"
#include "LocalizationManager.h"
#include "EditorUI.h"
#include "NotificationManager.h"
#include "ShortcutManager.h"
#include <imgui_stdlib.h>

void InputActionSettingsWidget::ReloadDraft(EditorSettingsService& settings)
{
    CancelCapture();
    m_ExpandedAction.reset();
    m_NewInputActionIds.clear();
    m_NewCategoryName.clear();

    m_InputActionsLoaded = settings.LoadInputActionSettings(m_DraftInputActions);
    if (m_InputActionsLoaded)
        m_InputActionsDirty = false;
}

void InputActionSettingsWidget::Apply(EditorSettingsService& settings)
{
    if (!m_InputActionsLoaded || !m_InputActionsDirty) return;

    if (settings.SaveInputActionSettings(m_DraftInputActions) == EEditorSettingsSaveStatus::Saved)
    {
        m_InputActionsDirty = false;
        return;
    }

    NotificationManager::Get().AddNotification(
        LOCAL("InputActions.Title"), LOCAL("InputActions.SaveFailed"), ELogLevel::Error);
}

namespace
{
    bool DrawInputActionBindingSummary(const Engine::InputActionMap& map, const std::vector<std::size_t>& indices)
    {
        const auto& style = ImGui::GetStyle();
        const float spacing = style.ItemSpacing.x;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const auto chipWidth = [&](const char* label)
            {
                return ImGui::CalcTextSize(label).x + style.FramePadding.x * 2.0f;
            };

        float totalWidth = 0.0f;
        for (const auto index : indices)
            totalWidth += chipWidth(map.Bindings[index].Key.c_str()) + spacing;
        totalWidth -= spacing;

        const bool hasOverflow = totalWidth > availableWidth;
        const std::string overflowLabel = "+" + std::to_string(indices.size());
        const float keyAreaWidth = hasOverflow
            ? availableWidth - chipWidth(overflowLabel.c_str()) - spacing : availableWidth;

        bool clicked = false;
        float usedWidth = 0.0f;
        std::size_t visibleCount = 0;

        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
        for (const auto index : indices)
        {
            const auto& key = map.Bindings[index].Key;
            const float nextWidth = chipWidth(key.c_str()) + (visibleCount > 0 ? spacing : 0.0f);
            if (usedWidth + nextWidth > keyAreaWidth)
                break;

            if (visibleCount > 0)
                ImGui::SameLine();

            ImGui::PushID(static_cast<int>(index));
            clicked |= ImGui::SmallButton((key + "###BindingSummary").c_str());
            ImGui::PopID();

            usedWidth += nextWidth;
            ++visibleCount;
        }

        if (visibleCount < indices.size())
        {
            if (visibleCount > 0)
                ImGui::SameLine();

            const auto label = "+" + std::to_string(indices.size() - visibleCount) + "###MoreBindings";
            clicked |= ImGui::SmallButton(label.c_str());
        }
        ImGui::PopStyleColor();

        return clicked;
    }
}

void InputActionSettingsWidget::Draw()
{
    ImGui::TextUnformatted(LOCAL_CSTR("InputActions.Title"));
    ImGui::Separator();

    if (!m_InputActionsLoaded)
    {
        ImGui::TextWrapped("%s", LOCAL_CSTR("InputActions.LoadFailed"));
        return;
    }

    ImGui::TextWrapped("%s", LOCAL_CSTR("InputActions.NextPlay"));

    auto& shortcuts = ShortcutManager::Get();
    const auto startCapture = [&](const Engine::InputActionMap& map, const Engine::InputAction& action,
        std::optional<std::size_t> index)
        {
            m_InputBindingCapture = InputBindingCapture{ map.Id, action.Id, index };
            shortcuts.SetInputActionCapturing(true);
        };

    const auto& style = ImGui::GetStyle();

    // 1. 최상단: 새 카테고리(Action Map) 추가 영역
    ImGui::Spacing();
    ImGui::BeginDisabled(shortcuts.IsCapturing());
    const std::string addCatLabel = std::string(ICON_FA_PLUS " ") + LOCAL("InputActions.AddCategory");
    const float addCatBtnW = ImGui::CalcTextSize(addCatLabel.c_str()).x + style.FramePadding.x * 2.0f + 16.0f;
    ImGui::SetNextItemWidth(std::max(180.0f, ImGui::GetContentRegionAvail().x - addCatBtnW - style.ItemSpacing.x));
    ImGui::InputTextWithHint("###NewCategoryInput", LOCAL_CSTR("InputActions.NewCategoryHint"), &m_NewCategoryName);

    ImGui::SameLine();
    const bool emptyCategory = m_NewCategoryName.find_first_not_of(" \t\r\n") == std::string::npos;
    const bool duplicateCategory = std::ranges::any_of(m_DraftInputActions.Maps, [this](const auto& m) { return m.Id == m_NewCategoryName; });

    ImGui::BeginDisabled(emptyCategory || duplicateCategory);
    if (ImGui::Button(addCatLabel.c_str(), ImVec2(addCatBtnW, 0.0f)))
    {
        m_DraftInputActions.Maps.push_back({ m_NewCategoryName, {}, {} });
        m_NewCategoryName.clear();
        m_InputActionsDirty = true;
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (duplicateCategory)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("InputActions.DuplicateCategory"));
    }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
    constexpr ImGuiTreeNodeFlags actionFlags =
        ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;

    const std::string addBindingLabel = std::string(ICON_FA_PLUS " ") + LOCAL_LABEL("InputActions.AddBinding");
    const std::string changeBindingLabel = LOCAL_LABEL("InputActions.ChangeBinding");
    const float editButtonsWidth = ImGui::CalcTextSize(changeBindingLabel.c_str(), nullptr, true).x +
        ImGui::CalcTextSize(ICON_FA_XMARK).x + style.FramePadding.x * 4.0f + style.ItemSpacing.x;

    std::vector<std::size_t> bindingIndices;
    std::optional<std::size_t> removedMapIndex;

    for (std::size_t mapIdx = 0; mapIdx < m_DraftInputActions.Maps.size(); ++mapIdx)
    {
        auto& map = m_DraftInputActions.Maps[mapIdx];
        ImGui::PushID(static_cast<int>(mapIdx));

        const std::string headerTitle = std::string(ICON_FA_FOLDER_OPEN "  ") + map.Id +
            " (" + std::to_string(map.Actions.size()) + ")###CategoryHeader_" + map.Id;

        const bool isCategoryOpen = ImGui::CollapsingHeader(headerTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
        if (!isCategoryOpen)
        {
            if (m_InputBindingCapture && m_InputBindingCapture->MapId == map.Id)
                CancelCapture();
            ImGui::PopID();
            continue;
        }

        ImGui::Spacing();

        // 카테고리 내부 툴바: 새 액션 추가 및 카테고리 삭제
        auto& newActionId = m_NewInputActionIds[map.Id];
        const std::string addActionLabel = std::string(ICON_FA_PLUS " ") + LOCAL("InputActions.AddAction");
        const float addActionBtnW = ImGui::CalcTextSize(addActionLabel.c_str()).x + style.FramePadding.x * 2.0f + 12.0f;
        const std::string delCatLabel = std::string(ICON_FA_TRASH " ") + LOCAL("InputActions.DeleteCategory");
        const float delCatBtnW = ImGui::CalcTextSize(delCatLabel.c_str()).x + style.FramePadding.x * 2.0f + 12.0f;

        ImGui::BeginDisabled(shortcuts.IsCapturing());
        const float availW = ImGui::GetContentRegionAvail().x;
        const float inputW = std::max(120.0f, availW - addActionBtnW - delCatBtnW - style.ItemSpacing.x * 2.0f);
        ImGui::SetNextItemWidth(inputW);
        ImGui::InputTextWithHint("###NewActionInput", LOCAL_CSTR("InputActions.ActionId"), &newActionId);

        const bool emptyId = newActionId.find_first_not_of(" \t\r\n") == std::string::npos;
        const bool duplicateId =
            std::ranges::find(map.Actions, newActionId, &Engine::InputAction::Id) != map.Actions.end();

        ImGui::SameLine();
        ImGui::BeginDisabled(emptyId || duplicateId);
        if (ImGui::Button(addActionLabel.c_str(), ImVec2(addActionBtnW, 0.0f)))
        {
            map.Actions.push_back({ newActionId, "Button" });
            newActionId.clear();
            m_InputActionsDirty = true;
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(delCatLabel.c_str(), ImVec2(delCatBtnW, 0.0f)))
        {
            removedMapIndex = mapIdx;
        }
        ImGui::EndDisabled();

        if (duplicateId)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("InputActions.DuplicateBinding"));
        }

        ImGui::Spacing();

        std::optional<std::string> removedActionId;
        std::optional<std::size_t> removedBinding;

        if (EditorUI::BeginSettingsTable("InputActionTable", 2, tableFlags))
        {
            ImGui::TableSetupColumn(LOCAL_CSTR("InputActions.ActionId"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn(LOCAL_CSTR("InputActions.Bindings"), ImGuiTableColumnFlags_WidthStretch, 1.7f);
            ImGui::TableHeadersRow();

            for (std::size_t actionIndex = 0; actionIndex < map.Actions.size(); ++actionIndex)
            {
                const auto& action = map.Actions[actionIndex];
                const auto isExpanded = [&]
                    {
                        return m_ExpandedAction && m_ExpandedAction->MapId == map.Id &&
                            m_ExpandedAction->ActionId == action.Id;
                    };
                const auto setExpanded = [&](bool open)
                    {
                        if (open == isExpanded())
                            return;

                        CancelCapture();
                        if (open)
                            m_ExpandedAction = ExpandedAction{ map.Id, action.Id };
                        else
                            m_ExpandedAction.reset();
                    };

                bindingIndices.clear();
                for (std::size_t index = 0; index < map.Bindings.size(); ++index)
                {
                    if (map.Bindings[index].ActionId == action.Id)
                        bindingIndices.push_back(index);
                }

                ImGui::PushID(static_cast<int>(actionIndex));
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::SetNextItemOpen(isExpanded(), ImGuiCond_Always);
                const bool open = ImGui::TreeNodeEx("ActionRow", actionFlags, "%s", action.Id.c_str());
                if (ImGui::IsItemToggledOpen())
                    setExpanded(open);

                ImGui::TableNextColumn();
                if (!bindingIndices.empty())
                {
                    if (DrawInputActionBindingSummary(map, bindingIndices))
                        setExpanded(true);
                }
                else if (!isExpanded())
                {
                    ImGui::BeginDisabled(shortcuts.IsCapturing());
                    if (ImGui::SmallButton(addBindingLabel.c_str()))
                    {
                        setExpanded(true);
                        startCapture(map, action, std::nullopt);
                    }
                    ImGui::EndDisabled();
                }

                if (isExpanded())
                {
                    const auto drawCaptureHint = [&](std::optional<std::size_t> index)
                        {
                            if (!m_InputBindingCapture || m_InputBindingCapture->MapId != map.Id ||
                                m_InputBindingCapture->ActionId != action.Id || m_InputBindingCapture->BindingIndex != index)
                            {
                                return;
                            }

                            ImGui::TextWrapped("%s", LOCAL_CSTR("InputActions.PressChord"));
                            if (m_InputBindingCapture->Duplicate)
                                ImGui::TextWrapped("%s", LOCAL_CSTR("InputActions.DuplicateBinding"));

                            ImGui::Button(LOCAL_LABEL("InputActions.MouseCaptureArea").c_str(), ImVec2(-FLT_MIN, 0.0f));
                            if (ImGui::IsItemHovered())
                                m_InputBindingCapture->MouseCaptureHoverFrame = ImGui::GetFrameCount();

                            ImGui::SetItemTooltip("%s", LOCAL_CSTR("InputActions.MouseCaptureArea"));
                        };

                    for (const auto index : bindingIndices)
                    {
                        ImGui::PushID(static_cast<int>(index));
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Indent();
                        ImGui::TextWrapped("%s", map.Bindings[index].Key.c_str());
                        ImGui::Unindent();

                        ImGui::TableNextColumn();
                        const bool removeInline = ImGui::GetContentRegionAvail().x >= editButtonsWidth;
                        ImGui::BeginDisabled(shortcuts.IsCapturing());
                        if (ImGui::SmallButton(changeBindingLabel.c_str()))
                            startCapture(map, action, index);

                        if (removeInline)
                            ImGui::SameLine();

                        if (ImGui::SmallButton(ICON_FA_XMARK "###RemoveBinding"))
                            removedBinding = index;
                        ImGui::EndDisabled();
                        ImGui::SetItemTooltip("%s", LOCAL_CSTR("EditorCommon.Delete"));

                        drawCaptureHint(index);
                        ImGui::PopID();
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::Indent();
                    ImGui::BeginDisabled(shortcuts.IsCapturing());
                    if (ImGui::SmallButton(ICON_FA_ELLIPSIS "###ManageAction"))
                        ImGui::OpenPopup("ActionMenu");
                    ImGui::EndDisabled();
                    ImGui::SetItemTooltip("%s", LOCAL_CSTR("InputActions.Manage"));

                    if (ImGui::BeginPopup("ActionMenu"))
                    {
                        if (ImGui::MenuItem(LOCAL_LABEL("InputActions.DeleteAction").c_str()))
                            removedActionId = action.Id;
                        ImGui::EndPopup();
                    }
                    ImGui::Unindent();

                    ImGui::TableNextColumn();
                    ImGui::BeginDisabled(shortcuts.IsCapturing());
                    if (ImGui::SmallButton(addBindingLabel.c_str()))
                        startCapture(map, action, std::nullopt);
                    ImGui::EndDisabled();
                    drawCaptureHint(std::nullopt);
                }

                ImGui::PopID();
            }

            EditorUI::EndSettingsTable();
        }

        if (!m_InputBindingCapture && removedBinding)
        {
            map.Bindings.erase(map.Bindings.begin() + static_cast<std::ptrdiff_t>(*removedBinding));
            m_InputActionsDirty = true;
        }

        if (!m_InputBindingCapture && removedActionId)
        {
            if (m_ExpandedAction && m_ExpandedAction->MapId == map.Id &&
                m_ExpandedAction->ActionId == *removedActionId)
            {
                m_ExpandedAction.reset();
            }

            std::erase_if(map.Actions, [&](const Engine::InputAction& action)
                {
                    return action.Id == *removedActionId;
                });
            std::erase_if(map.Bindings, [&](const Engine::InputBinding& binding)
                {
                    return binding.ActionId == *removedActionId;
                });
            m_InputActionsDirty = true;
        }

        ImGui::Spacing();
        ImGui::PopID();
    }

    if (removedMapIndex)
    {
        const auto& removedMap = m_DraftInputActions.Maps[*removedMapIndex];
        if (m_InputBindingCapture && m_InputBindingCapture->MapId == removedMap.Id)
            CancelCapture();

        if (m_ExpandedAction && m_ExpandedAction->MapId == removedMap.Id)
            m_ExpandedAction.reset();

        m_NewInputActionIds.erase(removedMap.Id);
        m_DraftInputActions.Maps.erase(m_DraftInputActions.Maps.begin() + static_cast<std::ptrdiff_t>(*removedMapIndex));
        m_InputActionsDirty = true;
    }
}

void InputActionSettingsWidget::HandleCapture(bool active)
{
    if (!m_InputBindingCapture)
        return;

    auto& input = Engine::InputManager::Get();
    if (!active || !input.HasKeyboardFocus() ||
        !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
    {
        CancelCapture();
        return;
    }

    auto& target = *m_InputBindingCapture;
    if (target.SkipFirstFrame)
    {
        target.SkipFirstFrame = false;
        return;
    }

    const bool overMouseCapture = target.MouseCaptureHoverFrame == ImGui::GetFrameCount();
    if (input.IsKeyButtonDown(Engine::EKeyCode::Escape) ||
        (!overMouseCapture && input.IsMouseButtonDown(Engine::EMouseButton::Left)) ||
        ImGui::GetIO().WantTextInput)
    {
        CancelCapture();
        return;
    }

    std::string text;
    if (overMouseCapture)
    {
        using Engine::EMouseButton;
        constexpr std::pair<EMouseButton, const char*> mouseBindings[]{
            { EMouseButton::Left, "Mouse:Left" },
            { EMouseButton::Right, "Mouse:Right" },
            { EMouseButton::Middle, "Mouse:Middle" },
            { EMouseButton::X1, "Mouse:X1" },
            { EMouseButton::X2, "Mouse:X2" }
        };

        for (const auto& [button, name] : mouseBindings)
        {
            if (input.IsMouseButtonDown(button))
            {
                text = name;
                target.PendingModifier = Engine::EKeyCode::Unknown;
                break;
            }
        }
    }

    Engine::KeyChord chord;
    if (text.empty())
    {
        chord = Engine::KeyChord::Capture(input, target.PendingModifier);
        if (!chord.IsValid())
            return;

        text = chord.ToString();
    }

    auto& maps = m_DraftInputActions.Maps;
    const auto mapIt = std::ranges::find(maps, target.MapId, &Engine::InputActionMap::Id);
    if (mapIt == maps.end() ||
        std::ranges::find(mapIt->Actions, target.ActionId, &Engine::InputAction::Id) == mapIt->Actions.end())
    {
        CancelCapture();
        return;
    }

    auto& bindings = mapIt->Bindings;
    Engine::InputBinding* current = nullptr;
    if (target.BindingIndex)
    {
        const auto index = *target.BindingIndex;
        if (index >= bindings.size() || bindings[index].ActionId != target.ActionId)
        {
            CancelCapture();
            return;
        }
        current = &bindings[index];
    }

    target.Duplicate = std::ranges::any_of(bindings, [&](const Engine::InputBinding& binding)
        {
            if (&binding == current || binding.ActionId != target.ActionId)
                return false;

            return chord.IsValid() ? Engine::KeyChord::FromString(binding.Key) == chord : binding.Key == text;
        });
    if (target.Duplicate)
        return;

    if (!current)
    {
        bindings.push_back({ target.ActionId, text });
        m_InputActionsDirty = true;
    }
    else if (current->Key != text)
    {
        current->Key = text;
        m_InputActionsDirty = true;
    }

    CancelCapture();
}

void InputActionSettingsWidget::CancelCapture()
{
    if (!m_InputBindingCapture) return;

    m_InputBindingCapture.reset();
    auto& shortcuts = ShortcutManager::Get();
    shortcuts.SetInputActionCapturing(false);
    shortcuts.SuppressNextProcess();
}