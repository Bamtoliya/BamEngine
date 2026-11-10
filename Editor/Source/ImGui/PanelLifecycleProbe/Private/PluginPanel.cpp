#include "PluginPanel.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <algorithm>
#include <utility>

BEGIN(Editor)

PluginPanelModule::PluginPanelModule(const wchar_t* dllName)
    : m_DllName(dllName)
{
}

const PluginPanelModule::Panel* PluginPanelModule::FindPanel(std::wstring_view id) const
{
    const auto found = std::find_if(m_Panels.begin(), m_Panels.end(),
        [id](const Panel& panel) { return panel.Id == id; });
    return found != m_Panels.end() ? &*found : nullptr;
}

bool PluginPanelModule::Reload()
{
    PluginModule candidate;
    return candidate.Open(m_DllName.c_str()) && Adopt(candidate);
}

bool PluginPanelModule::Adopt(PluginModule& candidate)
{
    const auto getApi = reinterpret_cast<GetPluginApiV1>(candidate.FindSymbol(PluginEntryV1));
    const auto* api = getApi ? getApi() : nullptr;
    if (!api || api->Size != sizeof(PluginApiV1) || api->PanelSize != sizeof(PanelRegistrationV1) ||
        api->DrawContextSize != sizeof(PanelDrawContextV1) || api->PanelCount == 0 || !api->Panels)
    {
        EDITOR_LOG_ERROR("Editor panel plugin has an incompatible V1 API: {}", WStrToStr(m_DllName));
        return false;
    }

    std::vector<Panel> nextPanels;
    nextPanels.reserve(api->PanelCount);
    for (std::uint32_t i = 0; i < api->PanelCount; ++i)
    {
        const auto& panel = api->Panels[i];
        if (!panel.Id || !*panel.Id || !panel.TitleKey || !*panel.TitleKey || !panel.Draw ||
            std::any_of(nextPanels.begin(), nextPanels.end(),
                [&panel](const Panel& entry) { return entry.Id == panel.Id; }))
        {
            EDITOR_LOG_ERROR("Editor panel plugin has an invalid or duplicate panel: {}", WStrToStr(m_DllName));
            return false;
        }

        nextPanels.push_back({ panel.Id, panel.TitleKey, panel.Draw });
    }

    if (!m_Panels.empty() &&
        (m_Panels.size() != nextPanels.size() ||
            std::any_of(nextPanels.begin(), nextPanels.end(),
                [this](const Panel& panel) { return FindPanel(panel.Id) == nullptr; })))
    {
        EDITOR_LOG_ERROR("Editor panel IDs changed; unload and load the plugin again: {}", WStrToStr(m_DllName));
        return false;
    }

    m_Module.Swap(candidate);
    m_Panels.swap(nextPanels);
    return true;
}

PluginPanel::PluginPanel(std::shared_ptr<PluginPanelModule> module, const std::wstring& id,
    std::string windowId, EPanelMenuCategory category)
    : m_Module(std::move(module)), m_WindowId(std::move(windowId)), m_Category(category)
{
    m_Name = id;
}

string PluginPanel::GetDisplayName() const
{
    const auto* panel = m_Module->FindPanel(m_Name);
    return panel ? LOCAL(panel->TitleKey) : IImGui::GetDisplayName();
}

void PluginPanel::Draw()
{
    if (!m_Open)
        return;

    const auto* panel = m_Module->FindPanel(m_Name);
    if (!panel)
        return;

    const auto title = LOCAL(panel->TitleKey) + "###" + m_WindowId;
    if (BeginPanelWindow(title.c_str(), m_Open))
    {
        const PanelDrawContextV1 context{
            static_cast<std::uint32_t>(ImGui::GetFrameCount()),
            [](const char* key, std::uint32_t value) {
                const auto text = LOCAL_FMT(key, value);
                ImGui::TextUnformatted(text.c_str());
            },
        };
        panel->Draw(context);
    }
    ImGui::End();
}

END
