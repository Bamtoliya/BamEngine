#include "PanelLifecycleProbe.h"
#include "Logger.h"
#include <utility>

BEGIN(Editor)

EResult PanelLifecycleProbe::OnInitialize(void*)
{
	return ReloadModule();
}

void PanelLifecycleProbe::Update(f32)
{
	if (std::exchange(m_ReloadPending, false))
		ReloadModule();
}

EResult PanelLifecycleProbe::ReloadModule()
{
    EditorPluginModule candidate;
    if (!candidate.Open(L"EditorPanelLifecycleProbe.dll"))
        return EResult::Fail;

    const auto getApi = reinterpret_cast<GetEditorPluginApiV1>(candidate.FindSymbol(EditorPluginEntryV1));
    const auto* api = getApi ? getApi() : nullptr;
    if (!api || api->Size != sizeof(EditorPluginApiV1) || api->PanelSize != sizeof(PanelRegistrationV1) ||
        api->DrawContextSize != sizeof(PanelDrawContextV1) || api->PanelCount != 1 || !api->Panels)
    {
        EDITOR_LOG_ERROR("Panel probe has an incompatible V1 plugin API");
        return EResult::Fail;
    }

    const auto& panel = api->Panels[0];
    if (!panel.Id || m_Name != panel.Id || !panel.TitleKey || !*panel.TitleKey || !panel.Draw)
    {
        EDITOR_LOG_ERROR("Panel probe registration is invalid");
        return EResult::Fail;
    }

    m_DisplayNameKey = panel.TitleKey;
    m_Module.Swap(candidate);
    m_Draw = panel.Draw;
    return EResult::Success;
}

void PanelLifecycleProbe::Draw()
{
#if defined(_WIN32)
	if (!m_Open || !m_Draw)
		return;

    const auto title = GetDisplayName() + "###PanelLifecycleProbe.Title";
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
#endif
}

void PanelLifecycleProbe::Free()
{
	m_Draw = nullptr;
	m_Module.Close();
}

END