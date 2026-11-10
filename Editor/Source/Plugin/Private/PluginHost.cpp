#include "PluginHost.h"
#include "Editor_Macros.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "Logger.h"
#include "PluginPanel.h"
#include "HotReloadManager.h"
#include "ModuleBuildManager.h"

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

            return Engine::WStrToStr(panelId);
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
            entry.Catalog = std::move(item);
            m_Entries.push_back(std::move(entry));
        }
    }

    void PluginHost::Clear()
    {
        ResolveApproval(false);
        m_Entries.clear();
        m_ExecutableDirectory.clear();
        m_ProjectRoot.clear();
        m_UserDataDirectory.clear();
    }

    PluginHostStatus PluginHost::GetStatus(std::size_t index) const
    {
        const auto& entry = m_Entries.at(index);
        return { entry.Module != nullptr, entry.LastActionFailed };
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
            return false;

        catalog.Status = ResolveDevelopmentPluginBinary(m_ExecutableDirectory, *catalog.Development, catalog.BinaryPath);
        if (catalog.Status != EPluginCatalogStatus::Ready)
            return false;

        auto module = std::make_shared<PluginPanelModule>(catalog.BinaryPath.c_str());
        if (!module->Reload())
            return false;

        return RegisterPanels(entry, std::move(module), gui);
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
        if (!catalog.Development)
            return false;

        catalog.Status = ResolveDevelopmentPluginBinary(m_ExecutableDirectory, *catalog.Development, catalog.BinaryPath);
        if (catalog.Status != EPluginCatalogStatus::Ready)
            return false;

        PluginModule candidate;
        return candidate.Open(catalog.BinaryPath.c_str()) && entry.Module->Adopt(candidate);
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

        if (custom && confirmed && !SavePluginApproval(approvalsPath, approval))
            EDITOR_LOG_WARN("Plugin {} loaded, but its approval could not be saved", catalog.Id);

        return true;
    }

    void PluginHost::Unload(Entry& entry, ImGuiManager& gui)
    {
        if (!entry.Module)
            return;

        for (const auto& definition : entry.Module->GetPanels())
        {
            if (auto* panel = gui.FindRegisteredPanel(definition.Id))
                QueuePanelRemoval(gui, panel);
        }

        entry.Module.reset();
    }
}
