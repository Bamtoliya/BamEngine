#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)

class NetworkMetricsWidget
{
public:
    [[nodiscard]] bool DrawCard(f32 cardWidth, bool isSelected) const;
    void DrawDetails() const;
};

END