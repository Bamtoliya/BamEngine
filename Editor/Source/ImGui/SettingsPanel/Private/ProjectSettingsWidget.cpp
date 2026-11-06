#include "ProjectSettingsWidget.h"
#include "EditorSettingsService.h"
#include "FileDialogs.h"
#include "Functions.h"
#include "LocalizationManager.h"

void ProjectSettingsWidget::ReloadDraft(const EditorSettingsService& settings)
{
    const auto& project = settings.GetProjectSettings();
    m_DraftProjectName = project.ProjectName;
    m_DraftStartupScenePath = project.StartupScenePath;
    m_DraftAssetDirectory = project.AssetDirectory;
    m_DraftShaderSourceDirectory = project.ShaderSourceDirectory;
    m_Dirty = false;
}

void ProjectSettingsWidget::Apply(EditorSettingsService& settings)
{
    if (!m_Dirty) return;

    auto project = settings.GetProjectSettings();
    project.ProjectName = m_DraftProjectName;
    project.StartupScenePath = m_DraftStartupScenePath;
    project.AssetDirectory = m_DraftAssetDirectory;
    project.ShaderSourceDirectory = m_DraftShaderSourceDirectory;

    settings.SaveProjectSettings(project);
    m_Dirty = false;
}

void ProjectSettingsWidget::Draw(const EditorSettingsService& settings)
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryProjectSettings"));
    ImGui::Separator();
    ImGui::Spacing();

    if (!ImGui::CollapsingHeader(LOCAL_CSTR("Settings.ProjectMetadata"), ImGuiTreeNodeFlags_DefaultOpen))
        return;

    const auto& projectRoot = settings.GetProjectRoot();

    ImGui::Spacing();
    ImGui::TextDisabled("%s: %s", LOCAL_CSTR("Settings.ProjectRoot"), projectRoot.string().c_str());
    ImGui::Spacing();

    const auto drawText = [this](const std::string& label, std::string& draft, auto& buffer)
        {
            strncpy_s(buffer, draft.c_str(), sizeof(buffer));
            ImGui::SetNextItemWidth(300.0f);
            if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer)))
            {
                draft = buffer;
                m_Dirty = true;
            }
        };

    const auto assignSelectedPath = [&](std::string& draft, const std::wstring& selectedPath)
        {
            std::error_code ec;
            const auto relativePath = std::filesystem::relative(selectedPath, projectRoot, ec);
            draft = ec ? Engine::WStrToStr(selectedPath) : relativePath.generic_string();
            m_Dirty = true;
        };

    char nameBuffer[256];
    drawText(LOCAL_LABEL("Settings.ProjectName"), m_DraftProjectName, nameBuffer);

    char sceneBuffer[512];
    drawText(LOCAL_LABEL("Settings.StartupScene"), m_DraftStartupScenePath, sceneBuffer);

    ImGui::SameLine();
    if (ImGui::Button((LOCAL("Settings.BrowseScene") + "###BrowseStartupScene").c_str()))
    {
        std::wstring scenePath;
        const auto scenesDir = projectRoot / "Resources" / "Scenes";
        if (FileDialogs::OpenFileDialog(scenePath,
            { { Engine::StrToWStr(LOCAL("FileDialog.EditorSceneFiles")), L"*.bamscene.json;*.json" } },
            scenesDir.wstring()))
        {
            assignSelectedPath(m_DraftStartupScenePath, scenePath);
        }
    }

    const auto drawFolderPicker = [&](const std::string& label, const char* buttonId, std::string& draft)
        {
            char buffer[256];
            drawText(label, draft, buffer);

            ImGui::SameLine();
            if (ImGui::Button((LOCAL("Settings.BrowseFolder") + buttonId).c_str()))
            {
                std::wstring folderPath;
                if (FileDialogs::OpenFolderDialog(folderPath, projectRoot.wstring()))
                    assignSelectedPath(draft, folderPath);
            }
        };

    drawFolderPicker(LOCAL_LABEL("Settings.AssetDirectory"), "###BrowseAssetDir", m_DraftAssetDirectory);
    drawFolderPicker(LOCAL_LABEL("Settings.ShaderDirectory"), "###BrowseShaderDir", m_DraftShaderSourceDirectory);
    ImGui::Spacing();
}