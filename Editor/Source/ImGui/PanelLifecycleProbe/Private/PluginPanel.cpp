#include "PluginPanel.h"
#include "AssetManager.h"
#include "AssetReferenceTimings.h"
#include "EditorSettings.h"
#include "InputManager.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include "RenderPassManager.h"
#include "SceneManager.h"
#include <algorithm>
#include <limits>
#include <utility>

BEGIN(Editor)
namespace
{
    constexpr std::uint64_t MaxPanelStateSize = 16 * 1024 * 1024;
    static_assert(AssetReferenceTimings::KindCount == PluginReferenceTimingKindCountV2);

    bool IsRightMouseButtonDown()
    {
        return Engine::InputManager::Get().IsMouseButtonDown(Engine::EMouseButton::Right);
    }

    void GetEngineTaskMetrics(PluginEngineTaskMetricsV2* output)
    {
        const auto& assets = AssetManager::Get();
        const auto taskMetrics = assets.GetTaskMetrics();
        const auto& referenceTimings = assets.GetAssetReferenceTimings();
        output->Size = sizeof(PluginEngineTaskMetricsV2);
        output->Outstanding = taskMetrics.Outstanding;
        output->Succeeded = taskMetrics.Succeeded;
        output->Failed = taskMetrics.Failed;
        output->ActiveSceneCount = Engine::SceneManager::Get().GetActiveScenes().size();
        output->RegisteredPassCount = Engine::RenderPassManager::Get().GetAllRenderPasses().size();
        output->LastFrame = referenceTimings.LastFrame;
        output->LastFrameMs = referenceTimings.LastFrameMs;
        output->PeakFrameMs = referenceTimings.PeakFrameMs;
        output->LastScan = {
            static_cast<std::uint32_t>(referenceTimings.LastScan.Kind),
            referenceTimings.LastScan.Frame,
            referenceTimings.LastScan.Directories,
            referenceTimings.LastScan.Documents,
            referenceTimings.LastScan.References,
            referenceTimings.LastScan.Issues,
            referenceTimings.LastScan.TraversalMs,
            referenceTimings.LastScan.ReadParseMs,
            referenceTimings.LastScan.ResolveMs,
            referenceTimings.LastScan.RefreshViewMs,
        };
        for (std::size_t i = 0; i < AssetReferenceTimings::KindCount; ++i)
        {
            const auto& timing = referenceTimings.ByKind[i];
            output->ByKind[i] = { timing.Samples, timing.LastMs, timing.TotalMs, timing.PeakMs };
        }
    }

    void ResetAssetReferenceTimings()
    {
        AssetManager::Get().ResetAssetReferenceTimings();
    }

    void SetWorkspaceFloat(void* context, const char* key, float value)
    {
        if (context && key)
            static_cast<EditorWorkspace*>(context)->SetSplitter(key, value);
    }

    float GetWorkspaceFloat(const void* context, const char* key, float defaultValue)
    {
        return context && key ? static_cast<const EditorWorkspace*>(context)->GetSplitter(key, defaultValue) :
            defaultValue;
    }
}

PluginPanelModule::PluginPanelModule(const wchar_t* dllName)
    : m_DllName(dllName)
{
}

PluginPanelModule::~PluginPanelModule()
{
    DestroyPanels(m_Panels);
}

const PluginPanelModule::Panel* PluginPanelModule::FindPanel(std::wstring_view id) const
{
    const auto found = std::find_if(m_Panels.begin(), m_Panels.end(),
        [id](const Panel& panel) { return panel.Id == id; });
    return found != m_Panels.end() ? &*found : nullptr;
}

void PluginPanelModule::DestroyPanels(std::vector<Panel>& panels) const
{
    for (auto& panel : panels)
    {
        if (panel.Instance && panel.ApiV2.Destroy)
            panel.ApiV2.Destroy(panel.Instance);
        panel.Instance = nullptr;
    }
}

bool PluginPanelModule::Reload()
{
    PluginModule candidate;
    return candidate.Open(m_DllName.c_str()) && Adopt(candidate);
}

bool PluginPanelModule::Adopt(PluginModule& candidate)
{
    const auto v2Symbol = candidate.FindSymbol(PluginEntryV2);
    const auto getApiV2 = reinterpret_cast<GetPluginApiV2>(v2Symbol);
    const auto* apiV2 = getApiV2 ? getApiV2() : nullptr;
    std::vector<Panel> nextPanels;

    if (apiV2)
    {
        if (apiV2->Size != sizeof(PluginApiV2) || apiV2->PanelSize != sizeof(PanelRegistrationV2) ||
            apiV2->DrawContextSize != sizeof(PanelDrawContextV2) ||
            apiV2->HostServicesSize != sizeof(PanelHostServicesV2) || apiV2->PanelCount == 0 || !apiV2->Panels)
        {
            EDITOR_LOG_ERROR("Editor panel plugin has an incompatible V2 API: {}", WStrToStr(m_DllName));
            return false;
        }

        nextPanels.reserve(apiV2->PanelCount);
        for (std::uint32_t i = 0; i < apiV2->PanelCount; ++i)
        {
            const auto& panel = apiV2->Panels[i];
            if (!panel.Id || !*panel.Id || !panel.TitleKey || !*panel.TitleKey || !panel.Create || !panel.Destroy ||
                !panel.GetStateSize || !panel.CaptureState || !panel.Update || !panel.Draw ||
                !panel.SaveWorkspace || !panel.RestoreWorkspace ||
                std::any_of(nextPanels.begin(), nextPanels.end(),
                    [&panel](const Panel& entry) { return entry.Id == panel.Id; }))
            {
                EDITOR_LOG_ERROR("Editor panel plugin has an invalid or duplicate V2 panel: {}",
                    WStrToStr(m_DllName));
                return false;
            }

            nextPanels.push_back({ panel.Id, panel.TitleKey, nullptr, panel, nullptr });
        }

        if (!m_Panels.empty() &&
            (m_Panels.size() != nextPanels.size() ||
                std::any_of(nextPanels.begin(), nextPanels.end(),
                    [this](const Panel& panel) { return FindPanel(panel.Id) == nullptr; })))
        {
            EDITOR_LOG_ERROR("Editor panel IDs changed; unload and load the plugin again: {}", WStrToStr(m_DllName));
            return false;
        }

        for (auto& panel : nextPanels)
        {
            const auto* previous = FindPanel(panel.Id);
            std::vector<std::byte> state;
            if (previous && previous->ApiV2.Create &&
                previous->ApiV2.StateVersion == panel.ApiV2.StateVersion)
            {
                const auto stateSize = previous->ApiV2.GetStateSize(previous->Instance);
                if (stateSize > MaxPanelStateSize || stateSize > std::numeric_limits<std::size_t>::max())
                {
                    EDITOR_LOG_ERROR("Editor panel plugin state is too large to reload: {}", WStrToStr(m_DllName));
                    DestroyPanels(nextPanels);
                    return false;
                }

                state.resize(static_cast<std::size_t>(stateSize));
                if (!previous->ApiV2.CaptureState(previous->Instance, state.data(), stateSize))
                {
                    EDITOR_LOG_ERROR("Editor panel plugin could not capture state for reload: {}",
                        WStrToStr(m_DllName));
                    DestroyPanels(nextPanels);
                    return false;
                }
            }

            panel.Instance = panel.ApiV2.Create(state.empty() ? nullptr : state.data(), state.size());
            if (!panel.Instance)
            {
                EDITOR_LOG_ERROR("Editor panel plugin could not create or restore a V2 panel: {}",
                    WStrToStr(m_DllName));
                DestroyPanels(nextPanels);
                return false;
            }
        }
    }
    else
    {
        const auto getApiV1 = reinterpret_cast<GetPluginApiV1>(candidate.FindSymbol(PluginEntryV1));
        const auto* apiV1 = getApiV1 ? getApiV1() : nullptr;
        if (!apiV1 || apiV1->Size != sizeof(PluginApiV1) || apiV1->PanelSize != sizeof(PanelRegistrationV1) ||
            apiV1->DrawContextSize != sizeof(PanelDrawContextV1) || apiV1->PanelCount == 0 || !apiV1->Panels)
        {
            EDITOR_LOG_ERROR("Editor panel plugin has an incompatible V1 or V2 API: {}", WStrToStr(m_DllName));
            return false;
        }

        nextPanels.reserve(apiV1->PanelCount);
        for (std::uint32_t i = 0; i < apiV1->PanelCount; ++i)
        {
            const auto& panel = apiV1->Panels[i];
            if (!panel.Id || !*panel.Id || !panel.TitleKey || !*panel.TitleKey || !panel.Draw ||
                std::any_of(nextPanels.begin(), nextPanels.end(),
                    [&panel](const Panel& entry) { return entry.Id == panel.Id; }))
            {
                EDITOR_LOG_ERROR("Editor panel plugin has an invalid or duplicate V1 panel: {}",
                    WStrToStr(m_DllName));
                return false;
            }

            nextPanels.push_back({ panel.Id, panel.TitleKey, panel.Draw, {}, nullptr });
        }

        if (!m_Panels.empty() &&
            (m_Panels.size() != nextPanels.size() ||
                std::any_of(nextPanels.begin(), nextPanels.end(),
                    [this](const Panel& panel) { return FindPanel(panel.Id) == nullptr; })))
        {
            EDITOR_LOG_ERROR("Editor panel IDs changed; unload and load the plugin again: {}", WStrToStr(m_DllName));
            return false;
        }
    }

    DestroyPanels(m_Panels);
    m_Module.Swap(candidate);
    m_Panels.swap(nextPanels);
    return true;
}

void PluginPanelModule::Update(std::wstring_view id, float dt)
{
    if (const auto* panel = FindPanel(id); panel && panel->ApiV2.Update)
        panel->ApiV2.Update(panel->Instance, dt);
}

void PluginPanelModule::Draw(std::wstring_view id, const PanelHostServicesV2& host)
{
    const auto* panel = FindPanel(id);
    if (!panel)
        return;

    if (panel->ApiV2.Draw)
    {
        ImGuiMemAllocFunc allocate = nullptr;
        ImGuiMemFreeFunc free = nullptr;
        void* allocatorData = nullptr;
        ImGui::GetAllocatorFunctions(&allocate, &free, &allocatorData);
        const PanelDrawContextV2 context{
            static_cast<std::uint32_t>(ImGui::GetFrameCount()), &host, ImGui::GetCurrentContext(),
            allocate, free, allocatorData
        };
        panel->ApiV2.Draw(panel->Instance, context);
        return;
    }

    const PanelDrawContextV1 context{
        static_cast<std::uint32_t>(ImGui::GetFrameCount()),
        [](const char* key, std::uint32_t value) {
            const auto text = LOCAL_FMT(key, value);
            ImGui::TextUnformatted(text.c_str());
        },
    };
    panel->DrawV1(context);
}

void PluginPanelModule::CaptureWorkspace(std::wstring_view id, EditorWorkspace& workspace) const
{
    const auto* panel = FindPanel(id);
    if (panel && panel->ApiV2.SaveWorkspace)
        panel->ApiV2.SaveWorkspace(panel->Instance, &workspace, SetWorkspaceFloat);
}

void PluginPanelModule::RestoreWorkspace(std::wstring_view id, const EditorWorkspace& workspace)
{
    const auto* panel = FindPanel(id);
    if (panel && panel->ApiV2.RestoreWorkspace)
        panel->ApiV2.RestoreWorkspace(panel->Instance, &workspace, GetWorkspaceFloat);
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

    if (panel->ApiV2.Id && panel->ApiV2.InitialWidth > 0.0f && panel->ApiV2.InitialHeight > 0.0f)
        ImGui::SetNextWindowSize(ImVec2(panel->ApiV2.InitialWidth, panel->ApiV2.InitialHeight), ImGuiCond_FirstUseEver);

    const auto title = LOCAL(panel->TitleKey) + "###" + m_WindowId;
    if (BeginPanelWindow(title.c_str(), m_Open, static_cast<ImGuiWindowFlags>(panel->ApiV2.WindowFlags)))
    {
        static const PanelHostServicesV2 host{
            &IsRightMouseButtonDown,
            &GetEngineTaskMetrics,
            &ResetAssetReferenceTimings,
        };
        m_Module->Draw(m_Name, host);
    }
    ImGui::End();
}

void PluginPanel::Update(f32 dt)
{
    m_Module->Update(m_Name, dt);
}

void PluginPanel::CaptureState(EditorWorkspace& workspace) const
{
    IImGui::CaptureState(workspace);
    m_Module->CaptureWorkspace(m_Name, workspace);
}

void PluginPanel::RestoreState(const EditorWorkspace& workspace)
{
    IImGui::RestoreState(workspace);
    m_Module->RestoreWorkspace(m_Name, workspace);
}

END
