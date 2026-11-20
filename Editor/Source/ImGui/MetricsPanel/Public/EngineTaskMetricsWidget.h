#pragma once

#include "Editor_Includes.h"
#include "EditorPanelPluginApi.h"

BEGIN(Editor)

class EngineTaskMetricsWidget
{
public:
    void Update(const PanelHostServicesV2& host);
    void ResetReferenceHistory(const PanelHostServicesV2& host);
    [[nodiscard]] bool DrawCard(f32 cardWidth, bool isSelected) const;
    void DrawDetails() const;

private:
    void DrawEngineStatus() const;
    void DrawAssetTaskMetrics() const;
    void DrawReferenceTimings() const;

    PluginEngineTaskMetricsV2 m_Metrics{};
};

END