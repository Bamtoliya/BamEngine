#include "EngineTaskMetricsWidget.h"
#include "AssetManager.h"
#include "RenderPassManager.h"
#include "SceneManager.h"

void EngineTaskMetricsWidget::Update()
{
    auto& assets = AssetManager::Get();
    m_AssetTaskMetrics = assets.GetTaskMetrics();
    m_ReferenceTimings = assets.GetAssetReferenceTimings();
    m_ActiveSceneCount = Engine::SceneManager::Get().GetActiveScenes().size();
    m_RegisteredPassCount = Engine::RenderPassManager::Get().GetAllRenderPasses().size();
}

void EngineTaskMetricsWidget::ResetReferenceHistory()
{
    AssetManager::Get().ResetAssetReferenceTimings();
    m_ReferenceTimings = {};
}

void EngineTaskMetricsWidget::DrawDetails() const
{
    DrawEngineStatus();
    ImGui::Separator();
    DrawAssetTaskMetrics();
    ImGui::Separator();
    DrawReferenceTimings();
}

bool EngineTaskMetricsWidget::DrawCard(f32 cardWidth, bool isSelected) const
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, isSelected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f));
    ImGui::BeginChild("Card_EngineTasks", ImVec2(cardWidth, 125.0f), true, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.TasksEngine"));
    ImGui::SameLine(cardWidth - 75.0f);
    if (m_AssetTaskMetrics.Outstanding > 0) ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.Busy"));
    else ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), LOCAL_CSTR("Metrics.Idle"));

    ImGui::Text(LOCAL_CSTR("Metrics.PassesScenes"), m_RegisteredPassCount, m_ActiveSceneCount);
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.TasksActiveDone"), m_AssetTaskMetrics.Outstanding, m_AssetTaskMetrics.GetCompleted());

    ImGui::Spacing();
    const auto detailLabel = LOCAL(isSelected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details";
    const bool clicked = ImGui::SmallButton(detailLabel.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    return clicked;
}

void EngineTaskMetricsWidget::DrawEngineStatus() const
{
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.EngineState"));
    const auto text = LOCAL_FMT("Metrics.ActiveScenes", m_ActiveSceneCount);
    ImGui::TextUnformatted(text.c_str());
    const auto text2 = LOCAL_FMT("Metrics.RegisteredPasses", m_RegisteredPassCount);
    ImGui::TextUnformatted(text2.c_str());
}

void EngineTaskMetricsWidget::DrawAssetTaskMetrics() const
{
    const auto outstanding = LOCAL_FMT("Metrics.Outstanding", m_AssetTaskMetrics.Outstanding);
    const auto completed = LOCAL_FMT("Metrics.Completed", m_AssetTaskMetrics.GetCompleted());
    const auto succeeded = LOCAL_FMT("Metrics.Succeeded", m_AssetTaskMetrics.Succeeded);
    const auto failed = LOCAL_FMT("Metrics.Failed", m_AssetTaskMetrics.Failed);

    ImGui::TextUnformatted(outstanding.c_str());
    ImGui::TextUnformatted(completed.c_str());
    ImGui::TextUnformatted(succeeded.c_str());

    if (m_AssetTaskMetrics.Failed > 0)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

    ImGui::TextUnformatted(failed.c_str());

    if (m_AssetTaskMetrics.Failed > 0)
        ImGui::PopStyleColor();

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.TaskHint"));
    ImGui::PopTextWrapPos();
}

void EngineTaskMetricsWidget::DrawReferenceTimings() const
{
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.ReferenceTimings"));
    if (m_ReferenceTimings.LastFrame < 0)
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.ReferenceNoSamples"));
        return;
    }

    const auto frame = LOCAL_FMT("Metrics.ReferenceFrameSum", m_ReferenceTimings.LastFrame,
        m_ReferenceTimings.LastFrameMs, m_ReferenceTimings.PeakFrameMs);
    ImGui::TextWrapped("%s", frame.c_str());

    constexpr const char* labels[] = {
        "Metrics.ReferenceInitial", "Metrics.ReferenceManual", "Metrics.ReferenceAutomatic",
        "Metrics.ReferenceAfterRepair", "Metrics.ReferenceDeleteImpact", "Metrics.ReferenceMovePreparation",
        "Metrics.ReferenceMoveRecheck", "Metrics.ReferenceIncremental"
    };
    static_assert(IM_ARRAYSIZE(labels) == AssetReferenceTimings::KindCount);

    const auto& scan = m_ReferenceTimings.LastScan;
    if (scan.Frame >= 0)
    {
        const auto latest = LOCAL_FMT("Metrics.ReferenceScanLatest", LOCAL(labels[static_cast<std::size_t>(scan.Kind)]),
            scan.Frame, scan.Directories, scan.Documents, scan.References, scan.Issues);
        const auto phases = LOCAL_FMT("Metrics.ReferenceScanPhases", scan.TraversalMs, scan.ReadParseMs,
            scan.ResolveMs, scan.RefreshViewMs);
        ImGui::TextWrapped("%s", latest.c_str());
        ImGui::TextWrapped("%s", phases.c_str());
    }

    const auto flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
    if (!ImGui::BeginTable("AssetReferenceTimings", 5, flags))
        return;

    ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.Type").c_str(), ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.Count").c_str());
    ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.ReferenceLast").c_str());
    ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.Average").c_str());
    ImGui::TableSetupColumn(LOCAL_LABEL("Metrics.Maximum").c_str());
    ImGui::TableHeadersRow();

    for (std::size_t index = 0; index < m_ReferenceTimings.ByKind.size(); ++index)
    {
        const auto& timing = m_ReferenceTimings.ByKind[index];
        if (timing.Samples == 0)
            continue;

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextWrapped("%s", LOCAL_CSTR(labels[index]));
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%llu", static_cast<unsigned long long>(timing.Samples));
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.2f", timing.LastMs);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.2f", timing.AverageMs());
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.2f", timing.PeakMs);
    }
    ImGui::EndTable();
}
