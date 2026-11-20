#pragma once

#include "Editor_Includes.h"
#include "MetricsWidgets.h"

#include <cstddef>
#include <cstdint>
#include <vector>

BEGIN(Editor)

enum class EMetricsDomain : std::uint8_t
{
    None = 0,
    Performance = 1,
    Rendering = 2,
    Memory = 3,
    EngineTasks = 4,
    Network = 5
};

class MetricsPanel
{
public:
    MetricsPanel() = default;

    std::vector<std::byte> CapturePluginState() const;
    bool RestorePluginState(const void* state, std::size_t stateSize);
    void Draw(const PanelHostServicesV2& host);
    void Update(float dt);
private:
    void DrawToolbar(const PanelHostServicesV2& host);
    void DrawDashboardCards();
    void DrawDetailedInspectionView();

    EMetricsDomain m_SelectedDomain = EMetricsDomain::None;
    bool m_Paused = false;

    FrameMetricsWidget m_FrameMetrics;
    RenderMetricsWidget m_RenderMetrics;
    ResourceMetricsWidget m_ResourceMetrics;
    EngineTaskMetricsWidget m_EngineTaskMetrics;
    NetworkMetricsWidget m_NetworkMetrics;
};

END