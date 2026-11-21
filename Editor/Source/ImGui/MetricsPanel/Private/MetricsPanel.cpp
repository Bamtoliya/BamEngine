#include "MetricsPanel.h"
#include "TimeManager.h"
#include "MetricsCardUI.h"
#include <cstring>

namespace
{
    struct MetricsPanelPluginState
    {
        std::uint8_t SelectedDomain;
        std::uint8_t Paused;
    };
}

std::vector<std::byte> MetricsPanel::CapturePluginState() const
{
    const MetricsPanelPluginState state{ static_cast<std::uint8_t>(m_SelectedDomain),
        static_cast<std::uint8_t>(m_Paused) };
    std::vector<std::byte> bytes(sizeof(state));
    std::memcpy(bytes.data(), &state, sizeof(state));
    return bytes;
}

bool MetricsPanel::RestorePluginState(const void* state, std::size_t stateSize)
{
    if (!state || stateSize != sizeof(MetricsPanelPluginState))
        return false;

    MetricsPanelPluginState snapshot{};
    std::memcpy(&snapshot, state, sizeof(snapshot));
    if (snapshot.SelectedDomain > static_cast<std::uint8_t>(EMetricsDomain::Network) || snapshot.Paused > 1)
        return false;

    m_SelectedDomain = static_cast<EMetricsDomain>(snapshot.SelectedDomain);
    m_Paused = snapshot.Paused != 0;
    return true;
}

void MetricsPanel::Update(float dt)
{
    if (m_Paused)
        return;

    m_ResourceMetrics.Update(dt);
    m_RenderMetrics.Update(dt);
    m_FrameMetrics.Update(dt, Engine::TimeManager::Get().GetTargetFPS());
}

void MetricsPanel::Draw(const PanelHostServicesV2& host)
{
    if (!m_Paused)
        m_EngineTaskMetrics.Update(host);
    DrawToolbar(host);
    ImGui::Separator();
    DrawDashboardCards();

    if (m_SelectedDomain != EMetricsDomain::None)
    {
        ImGui::Spacing();
        ImGui::Separator();
        DrawDetailedInspectionView();
    }
}

void MetricsPanel::DrawToolbar(const PanelHostServicesV2& host)
{
    const auto pause = LOCAL("Metrics.PauseCapture");
    const auto reset = LOCAL("Metrics.ResetHistory");
    const auto& style = ImGui::GetStyle();
    const float available = ImGui::GetContentRegionAvail().x;
    const float pauseWidth = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(pause.c_str()).x;
    const bool pauseFits = pauseWidth <= available;
    ImGui::Checkbox((pauseFits ? pause + "###Metrics.PauseCapture" : "###Metrics.PauseCapture").c_str(), &m_Paused);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOCAL_CSTR("Metrics.PauseHint"));

    if (!pauseFits)
        ImGui::TextWrapped("%s", pause.c_str());
    else if (pauseWidth + style.ItemSpacing.x + MetricsCardUI::ButtonWidth(reset.c_str()) <= available)
        ImGui::SameLine();
    if (MetricsCardUI::Button("Metrics.ResetHistory", reset.c_str()))
    {
        m_FrameMetrics.ResetHistory();
        m_RenderMetrics.ResetHistory();
        m_EngineTaskMetrics.ResetReferenceHistory(host);
    }
}

void MetricsPanel::DrawDashboardCards()
{
    const float availableWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);
    const auto& style = ImGui::GetStyle();
    const float spacing = style.ItemSpacing.x;
    const float actionWidth = std::max(MetricsCardUI::ButtonWidth(LOCAL_CSTR("Metrics.ViewDetails")),
        MetricsCardUI::ButtonWidth(LOCAL_CSTR("Metrics.CloseDetail")));
    const float minimumWidth = std::max(ImGui::GetFontSize() * 14.0f, actionWidth) + 2.0f * style.WindowPadding.x;
    const int columns = std::clamp(static_cast<int>((availableWidth + spacing) / (minimumWidth + spacing)), 1, 4);

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
            const auto title = ">> " + LOCAL(titleKey);
            const auto close = LOCAL("Metrics.CloseDetailX");
            const float start = ImGui::GetCursorPosX();
            const float available = ImGui::GetContentRegionAvail().x;
            const float closeWidth = MetricsCardUI::ButtonWidth(close.c_str());
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", title.c_str());
            ImGui::PopTextWrapPos();
            const float titleWidth = ImGui::CalcTextSize(title.c_str()).x;
            if (titleWidth > available)
                MetricsCardUI::ShowFullTextOnHover(title.c_str());
            if (titleWidth + ImGui::GetStyle().ItemSpacing.x + closeWidth <= available)
                ImGui::SameLine(start + available - closeWidth);
            if (MetricsCardUI::Button("Metrics.CloseDetailX", close.c_str()))
            {
                m_SelectedDomain = EMetricsDomain::None;
                return;
            }

            ImGui::Separator();
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0.0f);
            widget.DrawDetails();
            ImGui::PopTextWrapPos();
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
