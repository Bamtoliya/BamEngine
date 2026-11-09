#include "PluginPanel.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <utility>

BEGIN(Editor)

PluginPanel::PluginPanel(const wchar_t* id, const wchar_t* dllName, const char* windowId, EPanelMenuCategory category)
    : m_DllName(dllName), m_WindowId(windowId), m_Category(category)
{
    m_Name = id;
}

EResult PluginPanel::OnInitialize(void*)
{
    return ReloadModule();
}

void PluginPanel::Update(f32)
{
    if (std::exchange(m_ReloadPending, false))
        ReloadModule();
}

EResult PluginPanel::ReloadModule()
{
    EditorPluginModule candidate;
    if (!candidate.Open(m_DllName.c_str()))
        return EResult::Fail;

    const auto getApi = reinterpret_cast<GetEditorPluginApiV1>(candidate.FindSymbol(EditorPluginEntryV1));
    const auto* api = getApi ? getApi() : nullptr;
    if (!api || api->Size != sizeof(EditorPluginApiV1) || api->PanelSize != sizeof(PanelRegistrationV1) ||
        api->DrawContextSize != sizeof(PanelDrawContextV1) || api->PanelCount != 1 || !api->Panels)
    {
        EDITOR_LOG_ERROR("Editor panel plugin has an incompatible V1 API: {}", WStrToStr(m_DllName));
        return EResult::Fail;
    }

    const auto& panel = api->Panels[0];
    if (!panel.Id || m_Name != panel.Id || !panel.TitleKey || !*panel.TitleKey || !panel.Draw)
    {
        EDITOR_LOG_ERROR("Editor panel plugin registration is invalid: {}", WStrToStr(m_DllName));
        return EResult::Fail;
    }

    m_DisplayNameKey = panel.TitleKey;
    m_Module.Swap(candidate);
    m_Draw = panel.Draw;
    return EResult::Success;
}

void PluginPanel::Draw()
{
    if (!m_Open || !m_Draw)
        return;

    const auto title = GetDisplayName() + "###" + m_WindowId;
    if (BeginPanelWindow(title.c_str(), m_Open))
    {
        const PanelDrawContextV1 context{
            static_cast<std::uint32_t>(ImGui::GetFrameCount()),
            [](const char* key, std::uint32_t value) {
                const auto text = LOCAL_FMT(key, value);
                ImGui::TextUnformatted(text.c_str());
            },
        };
        m_Draw(context);
    }
    ImGui::End();
}

void PluginPanel::Free()
{
    m_Draw = nullptr;
    m_Module.Close();
}

END