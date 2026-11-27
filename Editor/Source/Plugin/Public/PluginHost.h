#pragma once

#include "PluginApproval.h"
#include "PluginCatalog.h"
#include "PluginSourceBuilder.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <unordered_map>

namespace Editor
{
    class ImGuiManager;
    class PluginPanelModule;

    enum class EPluginHostAction
    {
        Load,
        Reload,
        Unload
    };

    struct PluginHostStatus
    {
        bool IsLoaded = false;
        bool LastActionFailed = false;
        bool IsContentMounted = false;
    };

    struct PluginHostPanel
    {
        std::wstring Id;
        std::string TitleKey;
    };

    struct PluginApprovalPrompt
    {
        std::size_t Index = 0;
        std::string DisplayName;
        PluginApproval Approval;
    };

    class PluginHost final
    {
    public:
        void Initialize(const std::filesystem::path& executableDirectory, const std::filesystem::path& projectRoot,
            const std::filesystem::path& preferencesPath);
        void Clear();
        void Update(ImGuiManager& gui);

        std::size_t GetCount() const { return m_Entries.size(); }
        const PluginCatalogEntry& GetEntry(std::size_t index) const { return m_Entries.at(index).Catalog; }
        PluginHostStatus GetStatus(std::size_t index) const;
        std::vector<PluginHostPanel> GetPanels(std::size_t index) const;
        std::filesystem::path GetProjectPluginDirectory() const;
        void RefreshInstalledPackages();

        void Request(std::size_t index, EPluginHostAction action);
        void FocusPanel(std::size_t index, const std::wstring& panelId, ImGuiManager& gui) const;
        const PluginApprovalPrompt* GetApprovalPrompt() const;
        void ResolveApproval(bool approved);

        // One-click plugin source build, safe staging & reload
        bool BuildAndReloadPluginSource(const std::string& pluginId);
        bool IsPluginSourceBuilding() const;
        std::string GetPluginBuildLogs() const;
        std::string GetPluginBuildLogs(const std::string& pluginId) const;
        std::string GetPluginBuildError(const std::string& pluginId) const;
        const std::string& GetBuildingPluginId() const { return m_PendingBuildPluginId; }
        bool IsPluginContentMounted(std::size_t index) const;

    private:
        struct Entry
        {
            PluginCatalogEntry Catalog;
            std::shared_ptr<PluginPanelModule> Module;
            std::optional<EPluginHostAction> PendingAction;
            bool LastActionFailed = false;
            bool IsContentMounted = false;
        };

        bool Load(Entry& entry, ImGuiManager& gui);
        bool Reload(Entry& entry);
        void Unload(Entry& entry, ImGuiManager& gui);
        bool RegisterPanels(Entry& entry, std::shared_ptr<PluginPanelModule> module, ImGuiManager& gui);
        bool ActivateInstalled(std::size_t index, ImGuiManager& gui, const PluginApproval* confirmed = nullptr);

        std::filesystem::path m_ExecutableDirectory;
        std::filesystem::path m_ProjectRoot;
        std::filesystem::path m_UserDataDirectory;
        std::vector<Entry> m_Entries;
        std::optional<PluginApprovalPrompt> m_ApprovalPrompt;
        bool m_ApprovalConfirmed = false;
        PluginSourceBuilder m_SourceBuilder;
        std::string m_PendingBuildPluginId;
        std::unordered_map<std::string, std::string> m_PluginBuildLogs;
        std::unordered_map<std::string, std::string> m_PluginBuildErrors;
    };
}
