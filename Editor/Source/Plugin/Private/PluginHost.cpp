#include "PluginHost.h"
#include "Editor_Macros.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "Logger.h"
#include "PluginPanel.h"
#include "HotReloadManager.h"
#include "ModuleBuildManager.h"
#include "PlatformPath.h"

#include <algorithm>
#include <system_error>
#include <utility>

namespace Editor
{
    namespace
    {
        void QueuePanelRemoval(ImGuiManager& gui, IImGui* panel)
        {
            panel->Close();
            gui.RemoveImGuiPanel(panel);
        }

        std::string ResolvePluginPanelWindowId(const std::wstring& panelId)
        {
            // 첫 번째 Probe 창이 저장한 기존 도킹 키를 유지한다.
            if (panelId == L"Editor.BuiltIn.PanelLifecycleProbe")
                return "PanelLifecycleProbe.Title";
            if (panelId == L"Console")
                return "ConsolePanel";
            if (panelId == L"Metrics")
                return "EngineMetricsPanel";
            if (panelId == L"Network Profiler")
                return "UI_NETWORK_PROFILER_PANEL";

            return Engine::WStrToStr(panelId);
        }

        std::filesystem::path ResolvePluginContentDirectory(const PluginCatalogEntry& catalog)
        {
            if (catalog.PackageDirectory.empty())
                return {};

            std::error_code ec;
            for (const char* manifestName : { "plugin.json", ".bamplugin.json" })
            {
                const auto manifestPath = catalog.PackageDirectory / manifestName;
                if (std::filesystem::exists(manifestPath, ec))
                {
                    PluginManifest manifest;
                    if (ReadPluginManifest(manifestPath, manifest) == EPluginManifestResult::Success && !manifest.contentRoot.empty())
                    {
                        const auto candidate = catalog.PackageDirectory / manifest.contentRoot;
                        if (std::filesystem::is_directory(candidate, ec))
                            return candidate;
                    }
                    break;
                }
            }

            const auto defaultContent = catalog.PackageDirectory / "Content";
            if (std::filesystem::is_directory(defaultContent, ec))
                return defaultContent;

            return {};
        }

        bool TryMountPluginContent(const PluginCatalogEntry& catalog)
        {
            const auto contentDir = ResolvePluginContentDirectory(catalog);
            if (!contentDir.empty())
            {
                Engine::PlatformPath::RegisterPluginMount(catalog.Id, contentDir);
                return true;
            }
            return false;
        }

        void TryUnmountPluginContent(const PluginCatalogEntry& catalog)
        {
            Engine::PlatformPath::UnregisterPluginMount(catalog.Id);
        }
    }

    void PluginHost::Initialize(const std::filesystem::path& executableDirectory, const std::filesystem::path& projectRoot,
        const std::filesystem::path& preferencesPath)
    {
        m_ExecutableDirectory = executableDirectory;
        std::error_code error;
        m_ProjectRoot = projectRoot.empty() ? std::filesystem::path{} : std::filesystem::canonical(projectRoot, error);
        m_UserDataDirectory = preferencesPath.is_absolute() ? preferencesPath.parent_path() : std::filesystem::path{};
        const auto packages = DiscoverPluginPackages(executableDirectory, m_ProjectRoot);
        auto catalog = BuildPluginCatalog(executableDirectory, packages, PluginSdkCompatibility);
        m_Entries.reserve(catalog.size());

        for (auto& item : catalog)
        {
            Entry entry;
            if (item.Development && item.Development->AutoLoad)
                entry.PendingAction = EPluginHostAction::Load;
            entry.Catalog = std::move(item);
            m_Entries.push_back(std::move(entry));
        }
    }

    void PluginHost::Clear()
    {
        m_SourceBuilder.Free();
        m_PendingBuildPluginId.clear();
        m_PluginBuildLogs.clear();
        m_PluginBuildErrors.clear();
        for (auto& entry : m_Entries)
        {
            if (entry.IsContentMounted)
            {
                TryUnmountPluginContent(entry.Catalog);
                entry.IsContentMounted = false;
            }
        }
        Engine::PlatformPath::ClearPluginMounts();
        ResolveApproval(false);
        m_Entries.clear();
        m_ExecutableDirectory.clear();
        m_ProjectRoot.clear();
        m_UserDataDirectory.clear();
    }

    PluginHostStatus PluginHost::GetStatus(std::size_t index) const
    {
        const auto& entry = m_Entries.at(index);
        return { entry.Module != nullptr || entry.IsContentMounted, entry.LastActionFailed, entry.IsContentMounted };
    }

    bool PluginHost::IsPluginContentMounted(std::size_t index) const
    {
        if (index >= m_Entries.size()) return false;
        return m_Entries[index].IsContentMounted;
    }

    std::vector<PluginHostPanel> PluginHost::GetPanels(std::size_t index) const
    {
        const auto& module = m_Entries.at(index).Module;
        std::vector<PluginHostPanel> panels;
        if (!module)
            return panels;

        panels.reserve(module->GetPanels().size());
        for (const auto& panel : module->GetPanels())
            panels.push_back({ panel.Id, panel.TitleKey });

        return panels;
    }

    std::filesystem::path PluginHost::GetProjectPluginDirectory() const
    {
        return m_ProjectRoot.empty() ? std::filesystem::path{} : m_ProjectRoot / "Plugins";
    }

    void PluginHost::RefreshInstalledPackages()
    {
        if (m_ApprovalPrompt)
            return;

        const auto packages = DiscoverPluginPackages(m_ExecutableDirectory, m_ProjectRoot);
        auto catalog = BuildPluginCatalog(m_ExecutableDirectory, packages, PluginSdkCompatibility);
        for (auto& entry : m_Entries)
        {
            if (entry.Catalog.Origin == EPluginOrigin::Development)
                continue;

            entry.Catalog.Status = EPluginCatalogStatus::MissingPackage;
            if (entry.PendingAction && *entry.PendingAction != EPluginHostAction::Unload)
                entry.PendingAction.reset();
        }

        for (auto& item : catalog)
        {
            if (item.Origin == EPluginOrigin::Development)
                continue;

            const auto existing = std::find_if(m_Entries.begin(), m_Entries.end(), [&item](const Entry& entry)
            {
                return entry.Catalog.Origin == item.Origin && entry.Catalog.PackageDirectory == item.PackageDirectory;
            });
            if (existing != m_Entries.end())
            {
                existing->Catalog = std::move(item);
                existing->LastActionFailed = false;
            }
            else
            {
                Entry entry;
                entry.Catalog = std::move(item);
                m_Entries.push_back(std::move(entry));
            }
        }
    }

    void PluginHost::Request(std::size_t index, EPluginHostAction action)
    {
        if (index >= m_Entries.size())
            return;

        auto& pending = m_Entries[index].PendingAction;
        if (!pending || action == EPluginHostAction::Unload)
            pending = action;
    }

    void PluginHost::FocusPanel(std::size_t index, const std::wstring& panelId, ImGuiManager& gui) const
    {
        const auto& module = m_Entries.at(index).Module;
        if (!module || !module->FindPanel(panelId))
            return;

        if (auto* panel = gui.FindRegisteredPanel(panelId))
            panel->Focus();
    }

    const PluginApprovalPrompt* PluginHost::GetApprovalPrompt() const
    {
        return m_ApprovalPrompt ? &*m_ApprovalPrompt : nullptr;
    }

    void PluginHost::ResolveApproval(bool approved)
    {
        m_ApprovalConfirmed = approved && m_ApprovalPrompt.has_value();
        if (!approved)
            m_ApprovalPrompt.reset();
    }

    void PluginHost::Update(ImGuiManager& gui)
    {
        // Poll source builder for completed async builds
        if (m_SourceBuilder.HasNewResult())
        {
            m_SourceBuilder.ClearNewResult();
            const auto result = m_SourceBuilder.GetLastResult();
            m_PluginBuildLogs[result.PluginId] = result.Logs;
            if (result.Success)
            {
                m_PluginBuildErrors.erase(result.PluginId);
                EDITOR_LOG_INFO("[PluginHost] Plugin '{}' source build succeeded. Refreshing and loading/reloading...", result.PluginId);
                RefreshInstalledPackages();
                const auto it = std::find_if(m_Entries.begin(), m_Entries.end(), [&result](const Entry& e) {
                    return e.Catalog.Id == result.PluginId;
                });
                if (it != m_Entries.end())
                {
                    const std::size_t foundIndex = std::distance(m_Entries.begin(), it);
                    if (it->Module || it->IsContentMounted)
                        Request(foundIndex, EPluginHostAction::Reload);
                    else
                        Request(foundIndex, EPluginHostAction::Load);
                }
            }
            else
            {
                m_PluginBuildErrors[result.PluginId] = result.ErrorMessage;
                EDITOR_LOG_ERROR("[PluginHost] Plugin '{}' source build failed: {}", result.PluginId, result.ErrorMessage);
                const auto it = std::find_if(m_Entries.begin(), m_Entries.end(), [&result](const Entry& e) {
                    return e.Catalog.Id == result.PluginId;
                });
                if (it != m_Entries.end())
                    it->LastActionFailed = true;
            }
            m_PendingBuildPluginId.clear();
        }

        for (std::size_t index = 0; index < m_Entries.size(); ++index)
        {
            auto& entry = m_Entries[index];
            const auto action = std::exchange(entry.PendingAction, std::nullopt);
            if (!action)
                continue;

            if (*action != EPluginHostAction::Unload && entry.Catalog.Development && HotReloadManager::IsCreated())
            {
                const auto* builder = HotReloadManager::Get().GetPluginBuilder(entry.Catalog.Development->Target);
                if (builder && builder->IsCompiling())
                    continue;
            }

            switch (*action)
            {
            case EPluginHostAction::Load:
                if (!entry.Module)
                    entry.LastActionFailed = entry.Catalog.Development ? !Load(entry, gui) : !ActivateInstalled(index, gui);
                break;
            case EPluginHostAction::Reload:
                if (entry.Module)
                    entry.LastActionFailed = entry.Catalog.Development ? !Reload(entry) : !ActivateInstalled(index, gui);
                break;
            case EPluginHostAction::Unload:
                if (m_ApprovalPrompt && m_ApprovalPrompt->Index == index)
                    ResolveApproval(false);
                Unload(entry, gui);
                entry.LastActionFailed = false;
                break;
            }
        }

        if (m_ApprovalConfirmed && m_ApprovalPrompt)
        {
            auto prompt = std::move(*m_ApprovalPrompt);
            ResolveApproval(false);
            m_Entries[prompt.Index].LastActionFailed = !ActivateInstalled(prompt.Index, gui, &prompt.Approval);
        }
    }

    bool PluginHost::Load(Entry& entry, ImGuiManager& gui)
    {
        auto& catalog = entry.Catalog;
        if (!catalog.Development)
        {
            const auto contentDir = ResolvePluginContentDirectory(catalog);
            if (catalog.BinaryPath.empty() && !contentDir.empty())
            {
                if (TryMountPluginContent(catalog))
                    entry.IsContentMounted = true;
                return entry.IsContentMounted;
            }
            return false;
        }

        catalog.Status = ResolveDevelopmentPluginBinary(m_ExecutableDirectory, *catalog.Development, catalog.BinaryPath);
        if (catalog.Status != EPluginCatalogStatus::Ready)
            return false;

        auto module = std::make_shared<PluginPanelModule>(catalog.BinaryPath.c_str());
        if (!module->Reload())
            return false;

        if (!RegisterPanels(entry, std::move(module), gui))
            return false;

        if (TryMountPluginContent(catalog))
            entry.IsContentMounted = true;

        return true;
    }

    bool PluginHost::RegisterPanels(Entry& entry, std::shared_ptr<PluginPanelModule> module, ImGuiManager& gui)
    {
        const auto& catalog = entry.Catalog;
        for (const auto& panel : module->GetPanels())
        {
            if (gui.FindRegisteredPanel(panel.Id))
            {
                EDITOR_LOG_ERROR("Plugin {} conflicts with panel {}", catalog.Id, Engine::WStrToStr(panel.Id));
                return false;
            }
        }

        std::vector<IImGui*> registered;
        registered.reserve(module->GetPanels().size());
        for (const auto& definition : module->GetPanels())
        {
            auto* panel = new PluginPanel(module, definition.Id, ResolvePluginPanelWindowId(definition.Id),
                EPanelMenuCategory::Diagnostics);
            if (panel->Initialize() != EResult::Success)
            {
                Safe_Release(panel);
                for (auto* previous : registered)
                    QueuePanelRemoval(gui, previous);

                EDITOR_LOG_ERROR("Plugin {} failed to register panel {}", catalog.Id, Engine::WStrToStr(definition.Id));
                return false;
            }

            registered.push_back(panel);
        }

        entry.Module = std::move(module);
        return true;
    }

    bool PluginHost::Reload(Entry& entry)
    {
        auto& catalog = entry.Catalog;
        if (entry.IsContentMounted && (!catalog.Development && catalog.BinaryPath.empty()))
        {
            if (TryMountPluginContent(catalog))
            {
                entry.IsContentMounted = true;
                return true;
            }
            return false;
        }

        if (!catalog.Development)
            return false;

        catalog.Status = ResolveDevelopmentPluginBinary(m_ExecutableDirectory, *catalog.Development, catalog.BinaryPath);
        if (catalog.Status != EPluginCatalogStatus::Ready)
            return false;

        PluginModule candidate;
        const bool adopted = candidate.Open(catalog.BinaryPath.c_str()) && entry.Module->Adopt(candidate);
        if (adopted && TryMountPluginContent(catalog))
            entry.IsContentMounted = true;

        return adopted;
    }

    bool PluginHost::ActivateInstalled(std::size_t index, ImGuiManager& gui, const PluginApproval* confirmed)
    {
        if (m_UserDataDirectory.empty() || m_ApprovalPrompt)
            return false;

        auto& entry = m_Entries[index];
        auto& catalog = entry.Catalog;
        const auto packages = DiscoverPluginPackages(m_ExecutableDirectory, m_ProjectRoot);
        const auto current = BuildPluginCatalog(m_ExecutableDirectory, packages, PluginSdkCompatibility);
        const auto found = std::find_if(current.begin(), current.end(), [&catalog](const PluginCatalogEntry& item)
        {
            return item.Origin == catalog.Origin && item.Id == catalog.Id && item.Version == catalog.Version &&
                item.PackageDirectory == catalog.PackageDirectory;
        });
        if (found == current.end())
            return false;

        catalog.Status = found->Status;
        catalog.BinaryPath = found->BinaryPath;

        // Content-only plugin check: If no DLL binary but has Content directory, activate as content mount
        const auto contentDir = ResolvePluginContentDirectory(catalog);
        const bool isContentOnly = catalog.BinaryPath.empty() && !contentDir.empty();
        if (isContentOnly)
        {
            if (TryMountPluginContent(catalog))
            {
                entry.IsContentMounted = true;
                return true;
            }
            return false;
        }

        if (catalog.Status != EPluginCatalogStatus::Ready && catalog.Status != EPluginCatalogStatus::NeedsApproval)
            return false;

        const bool custom = catalog.Origin == EPluginOrigin::Custom;
        if (custom && m_ProjectRoot.empty())
            return false;

        PluginModule candidate;
        if (!candidate.PrepareInstalled(catalog.BinaryPath, m_UserDataDirectory / "PluginShadow"))
            return false;

        PluginApproval approval{
            Engine::WStrToStr(m_ProjectRoot.wstring()), catalog.Id, catalog.Version,
            Engine::WStrToStr(catalog.PackageDirectory.wstring()), Engine::WStrToStr(catalog.BinaryPath.wstring()),
            candidate.GetPreparedFingerprint()
        };
        if (approval.sha256.empty() || (confirmed && *confirmed != approval))
        {
            EDITOR_LOG_WARN("Plugin {} fingerprint is unavailable or changed before approval", catalog.Id);
            return false;
        }

        const auto approvalsPath = m_UserDataDirectory / "PluginApprovals.json";
        if (custom && !confirmed && !HasPluginApproval(approvalsPath, approval))
        {
            m_ApprovalPrompt = PluginApprovalPrompt{index, found->DisplayName, std::move(approval)};
            return true;
        }

        if (!candidate.OpenApproved(approval.sha256))
            return false;

        if (entry.Module)
        {
            if (!entry.Module->Adopt(candidate))
                return false;
        }
        else
        {
            auto module = std::make_shared<PluginPanelModule>(catalog.BinaryPath.c_str());
            if (!module->Adopt(candidate) || !RegisterPanels(entry, std::move(module), gui))
                return false;
        }

        if (TryMountPluginContent(catalog))
            entry.IsContentMounted = true;

        if (custom && confirmed && !SavePluginApproval(approvalsPath, approval))
            EDITOR_LOG_WARN("Plugin {} loaded, but its approval could not be saved", catalog.Id);

        return true;
    }

    void PluginHost::Unload(Entry& entry, ImGuiManager& gui)
    {
        if (entry.IsContentMounted)
        {
            TryUnmountPluginContent(entry.Catalog);
            entry.IsContentMounted = false;
        }

        if (!entry.Module)
            return;

        for (const auto& definition : entry.Module->GetPanels())
        {
            if (auto* panel = gui.FindRegisteredPanel(definition.Id))
                QueuePanelRemoval(gui, panel);
        }

        entry.Module.reset();
    }

    bool PluginHost::BuildAndReloadPluginSource(const std::string& pluginId)
    {
        if (m_SourceBuilder.IsBuilding())
            return false;

        const auto it = std::find_if(m_Entries.begin(), m_Entries.end(), [&pluginId](const Entry& e) {
            return e.Catalog.Id == pluginId;
        });

        if (it == m_Entries.end())
            return false;

        auto& entry = *it;
        if (entry.Catalog.PackageDirectory.empty())
            return false;

        const auto sourceDir = entry.Catalog.PackageDirectory;
        std::error_code ec;
        if (!std::filesystem::exists(sourceDir / "CMakeLists.txt", ec))
            return false;

        std::filesystem::path outputBinDir;
        std::filesystem::path expectedDllName;
        if (!entry.Catalog.BinaryPath.empty())
        {
            outputBinDir = entry.Catalog.BinaryPath.parent_path();
            expectedDllName = entry.Catalog.BinaryPath.filename();
        }
        else
        {
            for (const char* manifestName : { "plugin.json", ".bamplugin.json" })
            {
                const auto manifestPath = sourceDir / manifestName;
                if (!std::filesystem::exists(manifestPath, ec))
                    continue;
                PluginManifest manifest;
                if (ReadPluginManifest(manifestPath, manifest) == EPluginManifestResult::Success)
                {
                    for (const auto& mod : manifest.modules)
                    {
                        if (mod.environment == "Editor" && !mod.binary.empty())
                        {
                            const auto rel = Engine::Utf8ToPath(mod.binary);
                            if (!rel.parent_path().empty())
                                outputBinDir = sourceDir / rel.parent_path();
                            expectedDllName = rel.filename();
                            break;
                        }
                    }
                }
                break;
            }
            if (outputBinDir.empty())
                outputBinDir = sourceDir / "Binaries" / "Win64";
        }

        m_PluginBuildErrors.erase(pluginId);
        m_PluginBuildLogs.erase(pluginId);
        m_PendingBuildPluginId = pluginId;
        EDITOR_LOG_INFO("[PluginHost] Starting source build for plugin '{}' in '{}'", pluginId, sourceDir.string());
        return m_SourceBuilder.StartBuild(pluginId, sourceDir, outputBinDir, m_ExecutableDirectory, expectedDllName);
    }

    bool PluginHost::IsPluginSourceBuilding() const
    {
        return m_SourceBuilder.IsBuilding();
    }

    std::string PluginHost::GetPluginBuildLogs() const
    {
        return m_SourceBuilder.GetLogs();
    }

    std::string PluginHost::GetPluginBuildLogs(const std::string& pluginId) const
    {
        if (m_SourceBuilder.IsBuilding() && m_PendingBuildPluginId == pluginId)
            return m_SourceBuilder.GetLogs();
        const auto it = m_PluginBuildLogs.find(pluginId);
        return it != m_PluginBuildLogs.end() ? it->second : std::string{};
    }

    std::string PluginHost::GetPluginBuildError(const std::string& pluginId) const
    {
        const auto it = m_PluginBuildErrors.find(pluginId);
        return it != m_PluginBuildErrors.end() ? it->second : std::string{};
    }
}
