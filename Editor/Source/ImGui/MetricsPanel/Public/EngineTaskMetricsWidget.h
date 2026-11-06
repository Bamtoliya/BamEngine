#pragma once

#include "Editor_Includes.h"
#include "AssetTaskMetrics.h"

BEGIN(Editor)

class EngineTaskMetricsWidget
{
public:
    void Update();
    [[nodiscard]] bool DrawCard(f32 cardWidth, bool isSelected) const;
    void DrawDetails() const;

private:
    void DrawEngineStatus() const;
    void DrawAssetTaskMetrics() const;

    size_t m_ActiveSceneCount = 0;
    size_t m_RegisteredPassCount = 0;
    AssetTaskMetrics m_AssetTaskMetrics{};
};

END