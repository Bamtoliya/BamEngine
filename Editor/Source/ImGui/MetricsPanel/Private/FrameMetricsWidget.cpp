#include "FrameMetricsWidget.h"
#include "LocalizationManager.h"

void FrameMetricsWidget::Update(f32 dt, uint32 targetFPS)
{
    m_TargetFPS = targetFPS;

    if (!std::isfinite(dt) || dt <= 0.0f)
        return;

    const f32 frameMs = dt * 1000.0f;
    if (!std::isfinite(frameMs))
        return;

    m_CurrentMs = frameMs;
    m_FrameTimes[m_WriteIndex] = frameMs;
    m_WriteIndex = (m_WriteIndex + 1) % HistoryCapacity;

    if (m_SampleCount < HistoryCapacity)
        ++m_SampleCount;

    RecalculateStatistics();
}

void FrameMetricsWidget::ResetHistory()
{
    m_FrameTimes.fill(0.0f);
    m_WriteIndex = 0;
    m_SampleCount = 0;
    m_CurrentMs = 0.0f;
    m_AverageMs = 0.0f;
    m_MinMs = 0.0f;
    m_MaxMs = 0.0f;
}

void FrameMetricsWidget::RecalculateStatistics()
{
    f64 totalMs = 0.0;
    m_MinMs = m_FrameTimes[0];
    m_MaxMs = m_FrameTimes[0];

    for (std::size_t index = 0; index < m_SampleCount; ++index)
    {
        const f32 frameMs = m_FrameTimes[index];
        totalMs += frameMs;
        if (frameMs < m_MinMs) m_MinMs = frameMs;
        if (frameMs > m_MaxMs) m_MaxMs = frameMs;
    }

    m_AverageMs = static_cast<f32>(totalMs / static_cast<f64>(m_SampleCount));
}

bool FrameMetricsWidget::DrawCard(float cardWidth, bool selected) const
{
    const ImVec4 background = selected ? ImVec4(0.15f, 0.25f, 0.35f, 0.8f) : ImVec4(0.14f, 0.14f, 0.16f, 0.7f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, background);
    ImGui::BeginChild("Card_Performance", ImVec2(cardWidth, 125.0f),
        ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

    ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("Metrics.Performance"));
    ImGui::SameLine(cardWidth - 75.0f);

    const f32 fps = m_AverageMs > 0.0f ? 1000.0f / m_AverageMs : 0.0f;
    if (fps >= 55.0f)
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "%s", LOCAL_CSTR("Metrics.Good"));
    else if (fps >= 30.0f)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", LOCAL_CSTR("Metrics.Warn"));
    else
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", LOCAL_CSTR("Metrics.Poor"));

    ImGui::Text(LOCAL_CSTR("Metrics.Fps"), fps);
    ImGui::TextDisabled(LOCAL_CSTR("Metrics.FrameMsAvg"), m_CurrentMs, m_AverageMs);

    ImGui::Spacing();
    const auto detailLabel = LOCAL(selected ? "Metrics.CloseDetail" : "Metrics.ViewDetails") + "###Details";
    const bool clicked = ImGui::SmallButton(detailLabel.c_str());

    ImGui::EndChild();
    ImGui::PopStyleColor();
    return clicked;
}

void FrameMetricsWidget::DrawDetails() const
{
    DrawSummary();
    ImGui::Separator();
    DrawGraph();
}

void FrameMetricsWidget::DrawSummary() const
{
    if (m_SampleCount == 0)
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("Metrics.NoSamples"));
        return;
    }

    const f32 averageFPS = m_AverageMs > 0.0f ? 1000.0f / m_AverageMs : 0.0f;
    ImGui::TextUnformatted(LOCAL_FMT("Metrics.AverageFPS", averageFPS).c_str());

    if (ImGui::BeginTable("FrameStatistics", 2, ImGuiTableFlags_SizingStretchSame))
    {
        const auto drawRow = [](const char* key, f32 frameMs)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(LOCAL_CSTR(key));
                ImGui::TableNextColumn();
                ImGui::Text("%.3f ms", frameMs);
            };

        drawRow("Metrics.CurrentFrame", m_CurrentMs);
        drawRow("Metrics.Average", m_AverageMs);
        drawRow("Metrics.Minimum", m_MinMs);
        drawRow("Metrics.Maximum", m_MaxMs);
        ImGui::EndTable();
    }

    if (m_TargetFPS > 0)
    {
        const f64 targetMs = 1000.0 / static_cast<f64>(m_TargetFPS);
        ImGui::TextUnformatted(LOCAL_FMT("Metrics.FPSCap", m_TargetFPS, targetMs).c_str());
    }
    else
    {
        ImGui::TextUnformatted(LOCAL_CSTR("Metrics.FPSUnlimited"));
    }
}

void FrameMetricsWidget::DrawGraph() const
{
    if (m_SampleCount == 0)
        return;

    const int32 offset = m_SampleCount == HistoryCapacity ? static_cast<int32>(m_WriteIndex) : 0;
    const f32 scaleMax = m_MaxMs > 30.0f ? m_MaxMs * 1.1f : 33.333f;

    ImGui::PlotLines("##FrameTimeHistory", m_FrameTimes.data(), static_cast<int32>(m_SampleCount), offset,
        LOCAL_CSTR("Metrics.FrameInterval"), 0.0f, scaleMax, ImVec2(ImGui::GetContentRegionAvail().x, 130.0f));

    ImGui::TextUnformatted(LOCAL_FMT("Metrics.RecentFrames", static_cast<int32>(m_SampleCount)).c_str());
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(LOCAL_CSTR("Metrics.FrameTimeHint"));
    ImGui::PopTextWrapPos();
}