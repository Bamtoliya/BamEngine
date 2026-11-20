#include "EngineTaskMetricsWidget.h"

void EngineTaskMetricsWidget::Update(const PanelHostServicesV2& host)
{
    m_Metrics.Size = sizeof(PluginEngineTaskMetricsV2);
    host.GetEngineTaskMetrics(&m_Metrics);
}

void EngineTaskMetricsWidget::ResetReferenceHistory(const PanelHostServicesV2& host)
{
    host.ResetAssetReferenceTimings();
    m_Metrics.LastFrame = -1;
    m_Metrics.LastFrameMs = 0.0;
    m_Metrics.PeakFrameMs = 0.0;
    m_Metrics.LastScan.Frame = -1;
    for (auto& timing : m_Metrics.ByKind)
        timing = {};
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
    if (m_Metrics.Outstanding > 0) ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), LOCAL_CSTR("Metrics.Busy"));
    else ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), LOCAL_CSTR("Metrics.Idle"));

    ImGui::Text(LOCAL_CSTR("Metrics.PassesScenes"), m_Metrics.RegisteredPassCount, m_Metrics.ActiveSceneCount);
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.TasksActiveDone"), m_Metrics.Outstanding,
        m_Metrics.Succeeded + m_Metrics.Failed);

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
    const auto text = LOCAL_FMT("Metrics.ActiveScenes", m_Metrics.ActiveSceneCount);
    ImGui::TextUnformatted(text.c_str());
    const auto text2 = LOCAL_FMT("Metrics.RegisteredPasses", m_Metrics.RegisteredPassCount);
    ImGui::TextUnformatted(text2.c_str());
}

void EngineTaskMetricsWidget::DrawAssetTaskMetrics() const
{
    const auto outstanding = LOCAL_FMT("Metrics.Outstanding", m_Metrics.Outstanding);
    const auto completed = LOCAL_FMT("Metrics.Completed", m_Metrics.Succeeded + m_Metrics.Failed);
    const auto succeeded = LOCAL_FMT("Metrics.Succeeded", m_Metrics.Succeeded);
    const auto failed = LOCAL_FMT("Metrics.Failed", m_Metrics.Failed);

    ImGui::TextUnformatted(outstanding.c_str());
    ImGui::TextUnformatted(completed.c_str());
    ImGui::TextUnformatted(succeeded.c_str());

    if (m_Metrics.Failed > 0)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));

    ImGui::TextUnformatted(failed.c_str());

    if (m_Metrics.Failed > 0)
        ImGui::PopStyleColor();

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.TaskHint"));
    ImGui::PopTextWrapPos();
}

void EngineTaskMetricsWidget::DrawReferenceTimings() const
{
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.ReferenceTimings"));
    if (m_Metrics.LastFrame < 0)
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.ReferenceNoSamples"));
        return;
    }

    const auto frame = LOCAL_FMT("Metrics.ReferenceFrameSum", m_Metrics.LastFrame,
        m_Metrics.LastFrameMs, m_Metrics.PeakFrameMs);
    ImGui::TextWrapped("%s", frame.c_str());

    constexpr const char* labels[] = {
        "Metrics.ReferenceInitial", "Metrics.ReferenceManual", "Metrics.ReferenceAutomatic",
        "Metrics.ReferenceAfterRepair", "Metrics.ReferenceDeleteImpact", "Metrics.ReferenceMovePreparation",
        "Metrics.ReferenceMoveRecheck", "Metrics.ReferenceIncremental"
    };
    static_assert(IM_ARRAYSIZE(labels) == PluginReferenceTimingKindCountV2);

    const auto& scan = m_Metrics.LastScan;
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

    for (std::size_t index = 0; index < PluginReferenceTimingKindCountV2; ++index)
    {
        const auto& timing = m_Metrics.ByKind[index];
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
        ImGui::Text("%.2f", timing.TotalMs / static_cast<double>(timing.Samples));
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.2f", timing.PeakMs);
    }
    ImGui::EndTable();
}
