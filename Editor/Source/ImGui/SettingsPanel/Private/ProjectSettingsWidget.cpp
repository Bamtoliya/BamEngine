#include "ProjectSettingsWidget.h"
#include "EditorSettingsService.h"
#include "FileDialogs.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "LocalizationManager.h"
#include "EditorUI.h"
#include "RaytracedShadowModeUI.h"
#include "AssetHotReloadManager.h"
#include "HotReloadManager.h"
#include "PlatformPath.h"

namespace
{
    void DrawProjectLocalization()
    {
        if (!ImGui::CollapsingHeader(LOCAL_LABEL("Settings.ProjectLocale").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            return;

        const auto uri = Engine::WStrToStr(Engine::LocalizationManager::ProjectCatalogDirectory);
        ImGui::TextWrapped("%s", LOCAL_FMT("Settings.ProjectLocaleHelp", uri).c_str());
        if (!Engine::LocalizationManager::IsCreated() || !Editor::HotReloadManager::IsCreated())
            return;

        auto& locale = Engine::LocalizationManager::Get();
        const auto& files = locale.GetProjectCatalogFiles();
        if (files.empty())
            ImGui::TextWrapped("%s", LOCAL_CSTR("Settings.ProjectLocaleEmpty"));
        else
        {
            for (const auto& file : files)
            {
                ImGui::Bullet();
                ImGui::TextWrapped("%s", Engine::WStrToStr(file.filename().wstring()).c_str());
            }
        }

        auto& reloader = Editor::HotReloadManager::Get().GetAssetReloader();
        EditorUI::ToolbarLayout actions;
        if (actions.Button("ImportProjectLocale", ICON_FA_FILE_IMPORT, "Settings.ProjectLocaleImport"))
        {
            std::wstring path;
            const auto catalogDirectory = Engine::PlatformPath::ResolveUri(Engine::LocalizationManager::ProjectCatalogDirectory);
            const vector<FileDialogFilter> filters = {
                { Engine::StrToWStr(LOCAL("Settings.ProjectLocaleFiles")), L"*.csv;*.json" }
            };
            if (FileDialogs::OpenFileDialog(path, filters, catalogDirectory.wstring(), "Settings.ProjectLocaleImport"))
                reloader.RequestProjectLocalizationImport(std::filesystem::path(path));
        }
        if (actions.Button("ReloadProjectLocale", ICON_FA_ROTATE, "Settings.ProjectLocaleReload"))
            reloader.RequestProjectLocalizationReload();
    }
}

void ProjectSettingsWidget::ReloadDraft(const EditorSettingsService& settings)
{
    const auto& project = settings.GetProjectSettings();
    m_DraftProjectName = project.ProjectName;
    m_DraftStartupScenePath = project.StartupScenePath;
    m_DraftAssetDirectory = project.AssetDirectory;
    m_DraftShaderSourceDirectory = project.ShaderSourceDirectory;
    m_DraftRaytracedShadow = project.RaytracedShadowMode;
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
    project.RaytracedShadowMode = m_DraftRaytracedShadow;

    const auto result = settings.SaveProjectSettings(project);
    if (result == EEditorSettingsSaveStatus::Saved)
        m_Dirty = false;
    ImGuiManager::Get().NotifySettingsChange({ result == EEditorSettingsSaveStatus::Saved, result });
}

void ProjectSettingsWidget::Draw(const EditorSettingsService& settings)
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryProjectSettings"));
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader(LOCAL_LABEL("Settings.ProjectMetadata").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        DrawMetadata(settings);

    DrawRendering();
    DrawProjectLocalization();
}

void ProjectSettingsWidget::DrawRendering()
{
    if (!ImGui::CollapsingHeader(LOCAL_LABEL("GraphicsSettings.SectionRaytracing").c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        return;

    if (EditorUI::BeginPropertyTable("##ProjectRaytracingTable"))
    {
        EditorUI::PropertyRow("GraphicsSettings.RaytracedShadow");
        m_Dirty |= DrawRaytracedShadowModeCombo("###ProjectRaytracedShadow", m_DraftRaytracedShadow);
        EditorUI::EndPropertyTable();
    }
    ImGui::Spacing();
}

void ProjectSettingsWidget::DrawMetadata(const EditorSettingsService& settings)
{
    const auto& projectRoot = settings.GetProjectRoot();

    if (EditorUI::BeginPropertyTable("##ProjectSettingsTable"))
    {
        const auto assignSelectedPath = [&](std::string& draft, const std::wstring& selectedPath)
            {
                std::error_code ec;
                const auto relativePath = std::filesystem::relative(selectedPath, projectRoot, ec);
                draft = ec ? Engine::WStrToStr(selectedPath) : relativePath.generic_string();
                m_Dirty = true;
            };

        // 1. Project Root
        EditorUI::PropertyRow("Settings.ProjectRoot");
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", projectRoot.string().c_str());

        // 2. Project Name
        EditorUI::PropertyRow("Settings.ProjectName");
        char nameBuffer[256];
        strncpy_s(nameBuffer, m_DraftProjectName.c_str(), sizeof(nameBuffer));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("###ProjectName", nameBuffer, sizeof(nameBuffer)))
        {
            m_DraftProjectName = nameBuffer;
            m_Dirty = true;
        }

        // 3. Startup Scene
        EditorUI::PropertyRow("Settings.StartupScene");
        const std::string browseSceneBtnText = LOCAL("Settings.BrowseScene");
        const float browseSceneBtnW = ImGui::CalcTextSize(browseSceneBtnText.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
        char sceneBuffer[512];
        strncpy_s(sceneBuffer, m_DraftStartupScenePath.c_str(), sizeof(sceneBuffer));
        ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - browseSceneBtnW - ImGui::GetStyle().ItemSpacing.x));
        if (ImGui::InputText("###StartupScene", sceneBuffer, sizeof(sceneBuffer)))
        {
            m_DraftStartupScenePath = sceneBuffer;
            m_Dirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button((browseSceneBtnText + "###BrowseStartupScene").c_str(), ImVec2(browseSceneBtnW, 0.0f)))
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

        const auto drawFolderRow = [&](const char* labelKey, const char* inputId, const char* buttonId, std::string& draft)
            {
                EditorUI::PropertyRow(labelKey);
                const std::string browseFolderBtnText = LOCAL("Settings.BrowseFolder");
                const float browseFolderBtnW = ImGui::CalcTextSize(browseFolderBtnText.c_str()).x + ImGui::GetStyle().FramePadding.x * 2.0f + 8.0f;
                char buffer[256];
                strncpy_s(buffer, draft.c_str(), sizeof(buffer));
                ImGui::SetNextItemWidth(std::max(100.0f, ImGui::GetContentRegionAvail().x - browseFolderBtnW - ImGui::GetStyle().ItemSpacing.x));
                if (ImGui::InputText(inputId, buffer, sizeof(buffer)))
                {
                    draft = buffer;
                    m_Dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button((browseFolderBtnText + buttonId).c_str(), ImVec2(browseFolderBtnW, 0.0f)))
                {
                    std::wstring folderPath;
                    if (FileDialogs::OpenFolderDialog(folderPath, projectRoot.wstring()))
                        assignSelectedPath(draft, folderPath);
                }
            };

        // 4. Asset Directory
        drawFolderRow("Settings.AssetDirectory", "###AssetDirectory", "###BrowseAssetDir", m_DraftAssetDirectory);

        // 5. Shader Directory
        drawFolderRow("Settings.ShaderDirectory", "###ShaderDirectory", "###BrowseShaderDir", m_DraftShaderSourceDirectory);

        EditorUI::EndPropertyTable();
    }
    ImGui::Spacing();
}