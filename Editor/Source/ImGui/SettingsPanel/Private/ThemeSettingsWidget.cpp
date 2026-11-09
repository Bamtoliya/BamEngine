#include "ThemeSettingsWidget.h"
#include "EditorSettings.h"
#include "Editor_Includes.h"
#include "ImGuiManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"

void ThemeSettingsWidget::ReloadDraft(const EditorPreferences& preferences)
{
    m_AppliedTheme = preferences.Theme;
    m_DraftTheme = preferences.Theme;
    m_OriginalStyle = ImGuiManager::Get().GetBaseStyle();
    m_DraftStyle = m_OriginalStyle;
    m_StyleDirty = false;
    m_EditorDirty = false;
}

void ThemeSettingsWidget::ResetToDefaults(const EditorPreferences& defaults)
{
    m_DraftTheme = defaults.Theme;
    m_DraftStyle = m_OriginalStyle;
    m_StyleDirty = false;
    m_EditorDirty = m_DraftTheme != m_AppliedTheme;
}

bool ThemeSettingsWidget::SaveDraftStyle() const
{
    if (!m_StyleDirty || m_DraftTheme != "Custom")
        return true;

    auto& imgui = ImGuiManager::Get();
    if (imgui.SaveCustomStyle(imgui.GetCustomStylePath(), m_DraftStyle, 1.0f))
        return true;

    NotificationManager::Get().AddNotification(
        LOCAL("Settings.CategoryTheme"), LOCAL("Settings.StyleSaveFailed"), ELogLevel::Error);
    return false;
}

void ThemeSettingsWidget::WriteDraft(EditorPreferences& candidate) const
{
    candidate.Theme = m_DraftTheme;
}

void ThemeSettingsWidget::FinishApply(bool applied)
{
    if (!applied)
        return;

    m_AppliedTheme = m_DraftTheme;
    m_OriginalStyle = ImGuiManager::Get().GetBaseStyle();
    m_DraftStyle = m_OriginalStyle;
    m_StyleDirty = false;
    m_EditorDirty = false;
}

void ThemeSettingsWidget::Draw()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryTheme"));
    ImGui::Separator();
    ImGui::Spacing();

    if (EditorUI::BeginPropertyTable("##ThemeSettingsTable"))
    {
        EditorUI::PropertyRow("Settings.Theme");

        const std::string labels[] = {
            LOCAL("Settings.ThemeBamEngine"), LOCAL("Settings.ThemeLight"),
            LOCAL("Settings.ThemeClassic"), LOCAL("Settings.ThemeCustom")
        };
        const char* names[] = { "Dark", "Light", "Classic", "Custom" };
        const char* items[] = { labels[0].c_str(), labels[1].c_str(), labels[2].c_str(), labels[3].c_str() };

        int selected = 0;
        for (int i = 0; i < IM_ARRAYSIZE(names); ++i)
        {
            if (m_DraftTheme == names[i])
                selected = i;
        }

        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("###Theme", &selected, items, IM_ARRAYSIZE(items)))
        {
            m_DraftTheme = names[selected];
            m_DraftStyle = m_OriginalStyle;
            m_StyleDirty = false;
            m_EditorDirty = m_DraftTheme != m_AppliedTheme;
        }

        EditorUI::EndPropertyTable();
    }

    ImGui::Spacing();

    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.StyleEditor"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextWrapped("%s", LOCAL_CSTR("Settings.StyleHelp"));

        const bool presetPending = !m_StyleDirty && m_DraftTheme != m_AppliedTheme;
        if (presetPending)
        {
            ImGui::TextWrapped("%s", LOCAL_CSTR("Settings.StyleApplyPresetFirst"));
        }
        else
        {
            DrawStyleDraft();
        }
    }
}

void ThemeSettingsWidget::DrawStyleDraft()
{
    struct ScalarField
    {
        const char* Key;
        float ImGuiStyle::* Member;
    };

    const ScalarField scalarFields[] = {
        { "Settings.StyleWindowRounding", &ImGuiStyle::WindowRounding },
        { "Settings.StyleChildRounding", &ImGuiStyle::ChildRounding },
        { "Settings.StyleFrameRounding", &ImGuiStyle::FrameRounding },
        { "Settings.StylePopupRounding", &ImGuiStyle::PopupRounding },
        { "Settings.StyleScrollbarRounding", &ImGuiStyle::ScrollbarRounding },
        { "Settings.StyleGrabRounding", &ImGuiStyle::GrabRounding },
        { "Settings.StyleTabRounding", &ImGuiStyle::TabRounding },
        { "Settings.StyleWindowBorder", &ImGuiStyle::WindowBorderSize },
        { "Settings.StyleFrameBorder", &ImGuiStyle::FrameBorderSize },
        { "Settings.StylePopupBorder", &ImGuiStyle::PopupBorderSize },
        { "Settings.StyleScrollbarSize", &ImGuiStyle::ScrollbarSize },
        { "Settings.StyleGrabMinSize", &ImGuiStyle::GrabMinSize }
    };

    struct VectorField
    {
        const char* Key;
        ImVec2 ImGuiStyle::* Member;
    };

    const VectorField vectorFields[] = {
        { "Settings.StyleWindowPadding", &ImGuiStyle::WindowPadding },
        { "Settings.StyleFramePadding", &ImGuiStyle::FramePadding },
        { "Settings.StyleItemSpacing", &ImGuiStyle::ItemSpacing }
    };

    bool edited = false;

    if (EditorUI::BeginPropertyTable("##ThemeStyleValues"))
    {
        for (const auto& field : scalarFields)
        {
            ImGui::PushID(field.Key);
            EditorUI::PropertyRow(field.Key);
            ImGui::SetNextItemWidth(-FLT_MIN);
            edited |= ImGui::DragFloat("##Value", &(m_DraftStyle.*field.Member),
                0.1f, 0.0f, 100.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::PopID();
        }

        for (const auto& field : vectorFields)
        {
            ImGui::PushID(field.Key);
            EditorUI::PropertyRow(field.Key);
            ImGui::SetNextItemWidth(-FLT_MIN);

            auto& value = m_DraftStyle.*field.Member;
            float components[] = { value.x, value.y };
            if (ImGui::DragFloat2("##Value", components, 0.1f, 0.0f, 100.0f,
                "%.1f", ImGuiSliderFlags_AlwaysClamp))
            {
                value = ImVec2(components[0], components[1]);
                edited = true;
            }

            ImGui::PopID();
        }

        EditorUI::EndPropertyTable();
    }

    if (ImGui::TreeNode(LOCAL_LABEL("Settings.StyleColors").c_str()))
    {
        if (EditorUI::BeginPropertyTable("##ThemeStyleColors"))
        {
            for (int i = 0; i < ImGuiCol_COUNT; ++i)
            {
                ImGui::PushID(i);
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(ImGui::GetStyleColorName(static_cast<ImGuiCol>(i)));
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);

                auto& color = m_DraftStyle.Colors[i];
                float rgba[] = { color.x, color.y, color.z, color.w };
                if (ImGui::ColorEdit4("##Color", rgba, ImGuiColorEditFlags_AlphaBar))
                {
                    color = ImVec4(rgba[0], rgba[1], rgba[2], rgba[3]);
                    edited = true;
                }

                ImGui::PopID();
            }

            EditorUI::EndPropertyTable();
        }
        ImGui::TreePop();
    }

    if (!edited)
        return;

    m_StyleDirty = false;

    for (const auto& field : scalarFields)
        m_StyleDirty |= m_DraftStyle.*field.Member != m_OriginalStyle.*field.Member;

    for (const auto& field : vectorFields)
    {
        const auto& draft = m_DraftStyle.*field.Member;
        const auto& original = m_OriginalStyle.*field.Member;
        m_StyleDirty |= draft.x != original.x || draft.y != original.y;
    }

    for (int i = 0; i < ImGuiCol_COUNT; ++i)
    {
        const auto& draft = m_DraftStyle.Colors[i];
        const auto& original = m_OriginalStyle.Colors[i];
        m_StyleDirty |= draft.x != original.x || draft.y != original.y;
        m_StyleDirty |= draft.z != original.z || draft.w != original.w;
    }

    m_DraftTheme = m_StyleDirty ? "Custom" : m_AppliedTheme;
    m_EditorDirty = m_StyleDirty || m_DraftTheme != m_AppliedTheme;
}