#include "WorkspaceSettingsWidget.h"
#include "BaseViewportPanel.h"
#include "EditorSettingsService.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"

void WorkspaceSettingsWidget::ReloadDraft(const EditorSettingsService& settings)
{
    m_DraftThumbnailSize = settings.GetWorkspace().ContentBrowserThumbnailSize;
    m_WorkspaceDirty = false;
    m_ViewportDrafts.clear();
}

void WorkspaceSettingsWidget::SaveWorkspace(EditorSettingsService& settings)
{
    if (!m_WorkspaceDirty) return;

    settings.SaveWorkspace();
    m_WorkspaceDirty = false;
}

void WorkspaceSettingsWidget::ApplyViewportDrafts(EditorSettingsService& settings)
{
    for (const auto& [name, draft] : m_ViewportDrafts)
        (void)settings.RequestViewportResolutionChange(name, draft);
}

void WorkspaceSettingsWidget::Update(EditorSettingsService& settings)
{
    if (const auto vpResult = settings.TakeViewportResolutionResult())
    {
        if (vpResult->Change.Applied)
            m_ViewportDrafts.erase(vpResult->Name);
        if (!vpResult->RenderTargetsValid)
        {
            NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.ViewportRestoreFailed"), ELogLevel::Error);
        }
        else if (vpResult->Change.Applied && vpResult->Change.SaveStatus == EEditorSettingsSaveStatus::Saved)
        {
            NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.ViewportSaved"), ELogLevel::Info);
        }
        else
        {
            ImGuiManager::Get().NotifySettingsChange(vpResult->Change);
        }
    }
}

void WorkspaceSettingsWidget::Draw(EditorSettingsService& settings)
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryWorkspace"));
    ImGui::Separator();
    ImGui::Spacing();

    // 1. 뷰포트 해상도 설정
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionViewportResolution"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        auto states = settings.GetViewportResolutionStates();

        if (states.empty())
        {
            m_SelectedViewportName.clear();
            ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.NoViewport"));
        }
        else
        {
            auto selected = std::find_if(states.begin(), states.end(), [this](const auto& snapshot) {
                return snapshot.Name == m_SelectedViewportName;
                });
            if (selected == states.end()) selected = states.begin();
            m_SelectedViewportName = selected->Name;

            const bool blocked = settings.IsWindowChangePending() || settings.IsVSyncChangePending() || settings.IsViewportResolutionChangePending();
            ImGui::BeginDisabled(blocked);

            // 타깃 뷰포트 선택 콤보
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.TargetViewport").c_str(), selected->DisplayName.c_str()))
            {
                for (auto current = states.begin(); current != states.end(); ++current)
                {
                    const bool isSelected = (current->Name == m_SelectedViewportName);
                    const auto label = current->DisplayName + "###ResolutionTarget." + WStrToStr(current->Name);
                    if (ImGui::Selectable(label.c_str(), isSelected))
                    {
                        m_SelectedViewportName = current->Name;
                        selected = current;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const auto& actual = selected->State;
            const auto found = m_ViewportDrafts.find(selected->Name);
            auto draft = found != m_ViewportDrafts.end() ? found->second : actual;
            bool changed = false;

            static constexpr const char* modeKeys[] = { "Viewport.FreePanelSize", "GraphicsSettings.ViewportPreset", "GraphicsSettings.ViewportCustom" };
            const std::array<std::string, 7> aspects = { LOCAL("Viewport.FreeAspect"), "16:9", "16:10", "4:3", "21:9", "1:1", "9:16" };

            // 해상도 모드 콤보
            const auto draftMode = glm::clamp(draft.ResolutionMode, 0, 2);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.ViewportMode").c_str(), LOCAL_CSTR(modeKeys[draftMode])))
            {
                for (int32 index = 0; index < 3; ++index)
                {
                    if (ImGui::Selectable(LOCAL_LABEL(modeKeys[index]).c_str(), draft.ResolutionMode == index))
                    {
                        draft.ResolutionMode = index;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            // 프리셋 선택
            if (draft.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Preset))
            {
                std::string preview = LOCAL("GraphicsSettings.ViewportPreset");
                for (const auto& preset : g_ResolutionPresets)
                {
                    if (draft.PresetWidth == static_cast<int32>(preset.Width) && draft.PresetHeight == static_cast<int32>(preset.Height))
                    {
                        preview = LOCAL(preset.NameKey);
                        break;
                    }
                }

                ImGui::SetNextItemWidth(220.0f);
                if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.ViewportPreset").c_str(), preview.c_str()))
                {
                    for (const auto& preset : g_ResolutionPresets)
                    {
                        const bool isSelected = (draft.PresetWidth == static_cast<int32>(preset.Width) && draft.PresetHeight == static_cast<int32>(preset.Height));
                        if (ImGui::Selectable(LOCAL_LABEL(preset.NameKey).c_str(), isSelected))
                        {
                            draft.PresetWidth = static_cast<int32>(preset.Width);
                            draft.PresetHeight = static_cast<int32>(preset.Height);
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else if (draft.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Custom))
            {
                ImGui::SetNextItemWidth(120.0f);
                bool customChanged = ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), &draft.CustomWidth, 50, 100);
                ImGui::SetNextItemWidth(120.0f);
                customChanged |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), &draft.CustomHeight, 50, 100);

                if (customChanged)
                {
                    draft.CustomWidth = glm::clamp(draft.CustomWidth, BaseViewportPanel::MinResolutionDimension, BaseViewportPanel::MaxResolutionWidth);
                    draft.CustomHeight = glm::clamp(draft.CustomHeight, BaseViewportPanel::MinResolutionDimension, BaseViewportPanel::MaxResolutionHeight);
                    changed = true;
                }
            }

            // 종횡비 콤보
            const auto draftAspect = glm::clamp(draft.AspectRatioMode, 0, 6);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo(LOCAL_LABEL("Viewport.AspectRatio").c_str(), aspects[draftAspect].c_str()))
            {
                for (int32 index = 0; index < static_cast<int32>(aspects.size()); ++index)
                {
                    const auto label = aspects[index] + "###ViewportAspect." + std::to_string(index);
                    if (ImGui::Selectable(label.c_str(), draft.AspectRatioMode == index))
                    {
                        draft.AspectRatioMode = index;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            if (changed)
                m_ViewportDrafts.insert_or_assign(selected->Name, draft);

            ImGui::Spacing();
            if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ApplyViewport").c_str()))
            {
                if (settings.RequestViewportResolutionChange(selected->Name, draft) != EResult::Success)
                    NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.ViewportRequestRejected"), ELogLevel::Warn);
            }
            ImGui::SameLine();
            if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ReloadViewport").c_str()))
                m_ViewportDrafts.erase(selected->Name);

            ImGui::EndDisabled();

            if (settings.IsViewportResolutionChangePending())
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", LOCAL_CSTR("GraphicsSettings.ViewportPending"));
        }
        ImGui::Spacing();
    }

    // 2. 콘텐츠 브라우저 환경설정
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionContentBrowser"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("Settings.ThumbnailSize").c_str(), &m_DraftThumbnailSize, 32.0f, 128.0f, "%.0f px"))
            m_WorkspaceDirty = true;
        ImGui::Spacing();
    }

    // 3. 도킹 및 레이아웃
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionDockingLayout"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::TextDisabled(LOCAL_CSTR("Settings.DockLayoutHelp"));
        ImGui::Spacing();
    }
}