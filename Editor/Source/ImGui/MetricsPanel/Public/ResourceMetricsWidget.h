#pragma once

#include "Editor_Includes.h"
#include "ResourceMetrics.h"

BEGIN(Editor)
class ResourceMetricsWidget
{
public:
    void Update(f32 dt);
    [[nodiscard]] bool DrawCard(float cardWidth, bool isSelected) const;
    void DrawDetails() const;

private:
    ResourceMetrics m_ResourceMetrics{};
    f32 m_ResourceRefreshElapsed = 0.5f;
};

END