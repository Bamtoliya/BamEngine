#include "EditorEnvironmentSettingsWidget.h"
#include "EditorSettingsService.h"
#include "Editor_Includes.h"
#include "LocalizationManager.h"
#include "SceneViewportPanel.h"
#include "NotificationManager.h"
#include "AssetCache.h"

namespace
{
    struct CacheFolderInfo
    {
        uintmax_t TotalBytes = 0;
        uint32_t FileCount = 0;
    };
    CacheFolderInfo InspectCacheFolder(const std::filesystem::path& folderPath)
    {
        CacheFolderInfo info{};
        std::error_code ec;
        if (!std::filesystem::exists(folderPath, ec)) return info;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath, ec))
        {
            if (entry.is_regular_file(ec))
            {
                info.TotalBytes += entry.file_size(ec);
                ++info.FileCount;
            }
        }
        return info;
    }
    std::string FormatBytes(uintmax_t bytes)
    {
        if (bytes < 1024) return fmt::format("{} B", bytes);
        if (bytes < 1024 * 1024) return fmt::format("{:.1f} KB", static_cast<double>(bytes) / 1024.0);
        if (bytes < 1024 * 1024 * 1024) return fmt::format("{:.2f} MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
        return fmt::format("{:.2f} GB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    uintmax_t ClearCacheSubdirectory(const std::filesystem::path& folderPath)
    {
        uintmax_t deletedBytes = 0;
        std::error_code ec;
        if (!std::filesystem::exists(folderPath, ec)) return 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath, ec))
        {
            if (entry.is_regular_file(ec))
            {
                deletedBytes += entry.file_size(ec);
                std::filesystem::remove(entry.path(), ec);
            }
        }
        return deletedBytes;
    }
}

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

    // 5. 디스크 캐시 관리 (Disk Cache)
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.DiskCache"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        const auto rootCache = PlatformPath::GetProjectRoot() / ".bam" / "cache";
        const auto thumbInfo = InspectCacheFolder(rootCache / "thumbnails");
        const auto shaderInfo = InspectCacheFolder(rootCache / "shaders");
        const auto ddcInfo = InspectCacheFolder(rootCache / "ddc");
        const auto terrainInfo = InspectCacheFolder(rootCache / "terrain");
        const uintmax_t totalBytes = thumbInfo.TotalBytes + shaderInfo.TotalBytes + ddcInfo.TotalBytes + terrainInfo.TotalBytes;
        const uint32_t totalFiles = thumbInfo.FileCount + shaderInfo.FileCount + ddcInfo.FileCount + terrainInfo.FileCount;
        if (EditorUI::BeginPropertyTable("##DiskCacheTable"))
        {
            // 썸네일 캐시
            EditorUI::PropertyRow("Settings.DiskCacheThumbnails");
            ImGui::Text("%s (%u files)", FormatBytes(thumbInfo.TotalBytes).c_str(), thumbInfo.FileCount);
            // 셰이더/PSO 캐시
            EditorUI::PropertyRow("Settings.DiskCacheShaders");
            ImGui::Text("%s (%u files)", FormatBytes(shaderInfo.TotalBytes).c_str(), shaderInfo.FileCount);
            // 에셋 DDC 캐시
            EditorUI::PropertyRow("Settings.DiskCacheDDC");
            ImGui::Text("%s (%u files)", FormatBytes(ddcInfo.TotalBytes).c_str(), ddcInfo.FileCount);
            // 지형 그리드 캐시
            EditorUI::PropertyRow("Settings.DiskCacheTerrain");
            ImGui::Text("%s (%u files)", FormatBytes(terrainInfo.TotalBytes).c_str(), terrainInfo.FileCount);
            // 총합
            EditorUI::PropertyRow("Settings.DiskCacheTotal");
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s (%u files)", FormatBytes(totalBytes).c_str(), totalFiles);
            EditorUI::EndPropertyTable();
        }
        ImGui::Spacing();
        if (ImGui::Button(LOCAL_CSTR("Settings.ClearAllDiskCaches"), ImVec2(-FLT_MIN, 28.0f)))
        {
            const uintmax_t cleared = ClearCacheSubdirectory(rootCache);
            if (Editor::AssetCache::IsCreated())
                Editor::AssetCache::Get().ClearAll();
            NotificationManager::Get().AddNotification(
                LOCAL("Settings.DiskCache"),
                fmt::format("{} ({})", LOCAL("Settings.ClearDiskCacheSuccess"), FormatBytes(cleared)),
                ELogLevel::Info
            );
        }
        ImGui::Spacing();
    }
}