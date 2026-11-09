#include "EditorEnvironmentSettingsWidget.h"
#include "EditorSettingsService.h"
#include "Editor_Includes.h"
#include "LocalizationManager.h"
#include "SceneViewportPanel.h"

void EditorEnvironmentSettingsWidget::ReloadDraft(const EditorPreferences& preferences)
{
    if (SceneViewportPanel::GetActiveSceneViewport())
    {
        m_DraftGizmoSnapTranslation = preferences.GizmoSnapTranslation;
        m_DraftGizmoSnapRotation = preferences.GizmoSnapRotation;
        m_DraftGizmoSnapScale = preferences.GizmoSnapScale;
    }

    m_DraftCameraSpeed = preferences.CameraSpeed;
    m_DraftCameraFOV = preferences.CameraFOV;
    m_DraftPreviewLightIntensity = preferences.PreviewLightIntensity;
    m_EditorDirty = false;
    m_PreviewLightingDirty = false;
}

void EditorEnvironmentSettingsWidget::ResetToDefaults(const EditorPreferences& defaults)
{
    m_DraftGizmoSnapTranslation = defaults.GizmoSnapTranslation;
    m_DraftGizmoSnapRotation = defaults.GizmoSnapRotation;
    m_DraftGizmoSnapScale = defaults.GizmoSnapScale;
    m_DraftCameraSpeed = defaults.CameraSpeed;
    m_DraftCameraFOV = defaults.CameraFOV;
    m_DraftPreviewLightIntensity = defaults.PreviewLightIntensity;
    m_EditorDirty = true;
    m_PreviewLightingDirty = true;
}

void EditorEnvironmentSettingsWidget::WriteDraft(EditorPreferences& candidate) const
{
    candidate.CameraSpeed = m_DraftCameraSpeed;
    candidate.CameraFOV = m_DraftCameraFOV;
    candidate.GizmoSnapTranslation = m_DraftGizmoSnapTranslation;
    candidate.GizmoSnapRotation = m_DraftGizmoSnapRotation;
    candidate.GizmoSnapScale = m_DraftGizmoSnapScale;

    if (m_PreviewLightingDirty)
        candidate.PreviewLightIntensity = m_DraftPreviewLightIntensity;
}

void EditorEnvironmentSettingsWidget::FinishApply(bool applied)
{
    if (!applied)
        return;

    m_PreviewLightingDirty = false;
    m_EditorDirty = false;
}

void EditorEnvironmentSettingsWidget::Draw()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryEditor"));
    ImGui::Separator();
    ImGui::Spacing();

    // 1. 기즈모 스냅 단위
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.GizmoSnapping"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##GizmoSnapTable"))
        {
            // Translation Snap
            EditorUI::PropertyRow("Viewport.SnapTranslation");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat("###SnapTranslation", &m_DraftGizmoSnapTranslation, 0.1f, 0.01f, 100.0f, "%.2f"))
                m_EditorDirty = true;

            // Rotation Snap
            EditorUI::PropertyRow("Viewport.SnapRotation");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat("###SnapRotation", &m_DraftGizmoSnapRotation, 1.0f, 1.0f, 180.0f, "%.1f deg"))
                m_EditorDirty = true;

            // Scale Snap
            EditorUI::PropertyRow("Viewport.SnapScale");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragFloat("###SnapScale", &m_DraftGizmoSnapScale, 0.05f, 0.01f, 10.0f, "%.2f"))
                m_EditorDirty = true;

            EditorUI::EndPropertyTable();
        }
        ImGui::Spacing();
    }

    // 2. 최소 콘솔 로그 레벨
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.Logging"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##LoggingTable"))
        {
            EditorUI::PropertyRow("Settings.MinimumLogLevel");
            const std::string levelLabels[] = { LOCAL("LogLevel.Trace"), LOCAL("LogLevel.Info"), LOCAL("LogLevel.Warn"), LOCAL("LogLevel.Error"), LOCAL("LogLevel.Fatal") };
            const char* logLevels[] = { levelLabels[0].c_str(), levelLabels[1].c_str(), levelLabels[2].c_str(), levelLabels[3].c_str(), levelLabels[4].c_str() };
            int currentLevelIdx = static_cast<int>(m_DraftLogLevel);
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::Combo("###MinimumLogLevel", &currentLevelIdx, logLevels, IM_ARRAYSIZE(logLevels)))
            {
                m_DraftLogLevel = static_cast<Engine::ELogLevel>(currentLevelIdx);
                m_EditorDirty = true;
            }

            EditorUI::EndPropertyTable();
        }
        ImGui::Spacing();
    }

    // 3. 카메라 이동 속도 및 화각
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.CameraMovement"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##CameraMovementTable"))
        {
            // Camera Speed
            EditorUI::PropertyRow("Viewport.CameraSpeed");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::SliderFloat("###CameraSpeed", &m_DraftCameraSpeed, 1.0f, 500.0f, "%.1f"))
                m_EditorDirty = true;

            // Camera FOV
            EditorUI::PropertyRow("Settings.CameraFOV");
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::SliderFloat("###CameraFOV", &m_DraftCameraFOV, 30.0f, 120.0f, "%.0f deg"))
                m_EditorDirty = true;

            EditorUI::EndPropertyTable();
        }
        ImGui::Spacing();
    }

    // 4. 뷰포트 미리보기 조명 강도
    if (ImGui::CollapsingHeader(LOCAL_LABEL("Settings.PreviewLighting").c_str()))
    {
        constexpr const char* keys[] = {
            "Settings.PreviewKeyLight", "Settings.PreviewFillLight", "Settings.PreviewRimLight"
        };

        if (!m_PreviewLightingDirty)
            m_DraftPreviewLightIntensity = EditorSettingsService::Get().GetPreferences().PreviewLightIntensity;

        if (EditorUI::BeginPropertyTable("##PreviewLightingTable"))
        {
            for (std::size_t i = 0; i < m_DraftPreviewLightIntensity.size(); ++i)
            {
                ImGui::PushID(static_cast<int>(i));
                EditorUI::PropertyRow(keys[i]);
                ImGui::SetNextItemWidth(-FLT_MIN);

                m_PreviewLightingDirty |= ImGui::SliderFloat("###LightIntensity", &m_DraftPreviewLightIntensity[i],
                    0.0f, EditorPreferences::MaxPreviewLightIntensity, "%.2f");

                ImGui::PopID();
            }

            EditorUI::EndPropertyTable();
        }
        ImGui::Spacing();
    }
}