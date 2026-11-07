#pragma once

#include "ImGuiInterface.h"
#include "MetricsWidgets.h"

#include <cstdint>

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

class MetricsPanel : public IImGui
{
public:
    MetricsPanel();
    ~MetricsPanel() override = default;

    void Draw() override;
    void Update(f32 dt) override;
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Diagnostics; }
private:
    void DrawToolbar();
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