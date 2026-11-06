#include "MetricsPanel.h"
#include "TimeManager.h"

MetricsPanel::MetricsPanel()
{
    m_Name = L"Metrics";
    m_DisplayNameKey = "Metrics.Title";
}

void MetricsPanel::Update(f32 dt)
{
    if (m_Paused)
        return;

    m_EngineTaskMetrics.Update();
    m_ResourceMetrics.Update(dt);
    m_RenderMetrics.Update(dt);
    m_FrameMetrics.Update(dt, Engine::TimeManager::Get().GetTargetFPS());
}

void MetricsPanel::Draw()
{
    if (!m_Open)
        return;

    const std::string windowTitle = LOCAL("Metrics.Title") + "###EngineMetricsPanel";
    if (BeginPanelWindow(windowTitle.c_str(), m_Open))
    {
        m_Focused = ImGui::IsWindowFocused();
        m_Hovered = ImGui::IsWindowHovered();

        DrawToolbar();
        ImGui::Separator();
        DrawDashboardCards();

        if (m_SelectedDomain != EMetricsDomain::None)
        {
            ImGui::Spacing();
            ImGui::Separator();
            DrawDetailedInspectionView();
        }
    }
    ImGui::End();
}

void MetricsPanel::DrawToolbar()
{
    ImGui::Checkbox(LOCAL_LABEL("Metrics.PauseCapture").c_str(), &m_Paused);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOCAL_CSTR("Metrics.PauseHint"));

    ImGui::SameLine();
    if (ImGui::Button(LOCAL_LABEL("Metrics.ResetHistory").c_str()))
    {
        m_FrameMetrics.ResetHistory();
        m_RenderMetrics.ResetHistory();
    }
}

void MetricsPanel::DrawDashboardCards()
{
    const float availableWidth = ImGui::GetContentRegionAvail().x;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;

    int columns = 4;
    if (availableWidth < 550.0f)
        columns = 1;
    else if (availableWidth < 850.0f)
        columns = 2;
    else if (availableWidth < 1100.0f)
        columns = 3;

    const float cardWidth = (availableWidth - spacing * (columns - 1)) / static_cast<float>(columns);

    int cardIndex = 0;
    const auto drawCard = [&](EMetricsDomain domain, const auto& widget)
        {
            if (cardIndex > 0 && cardIndex % columns != 0)
                ImGui::SameLine();
            ++cardIndex;

            const bool selected = m_SelectedDomain == domain;
            if (widget.DrawCard(cardWidth, selected))
                m_SelectedDomain = selected ? EMetricsDomain::None : domain;
        };

    drawCard(EMetricsDomain::Performance, m_FrameMetrics);
    drawCard(EMetricsDomain::Rendering, m_RenderMetrics);
    drawCard(EMetricsDomain::Memory, m_ResourceMetrics);
    drawCard(EMetricsDomain::EngineTasks, m_EngineTaskMetrics);
    drawCard(EMetricsDomain::Network, m_NetworkMetrics);
}

void MetricsPanel::DrawDetailedInspectionView()
{
    const auto drawDetails = [this](const char* titleKey, const auto& widget)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), ">> %s", LOCAL_CSTR(titleKey));
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 110.0f);
            if (ImGui::Button(LOCAL_LABEL("Metrics.CloseDetailX").c_str()))
            {
                m_SelectedDomain = EMetricsDomain::None;
                return;
            }

            ImGui::Separator();
            ImGui::Spacing();
            widget.DrawDetails();
        };

    switch (m_SelectedDomain)
    {
    case EMetricsDomain::Performance:
        drawDetails("Metrics.DetailPerformance", m_FrameMetrics);
        break;
    case EMetricsDomain::Rendering:
        drawDetails("Metrics.DetailRendering", m_RenderMetrics);
        break;
    case EMetricsDomain::Memory:
        drawDetails("Metrics.DetailMemory", m_ResourceMetrics);
        break;
    case EMetricsDomain::EngineTasks:
        drawDetails("Metrics.DetailTasks", m_EngineTaskMetrics);
        break;
    case EMetricsDomain::Network:
        drawDetails("Metrics.DetailNetwork", m_NetworkMetrics);
        break;
    case EMetricsDomain::None:
        break;
    }
}