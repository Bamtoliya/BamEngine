#pragma once

namespace Editor
{
    // Last drawn menu-bar geometry, in SDL client-window coordinates.
    struct MainMenuBarLayout
    {
        float Top = 0.0f;
        float Bottom = 0.0f;
        float MenusEndX = 0.0f;
        float WindowButtonsBeginX = 0.0f;

        bool IsDragArea(float x, float y) const
        {
            return y >= Top && y < Bottom && x >= MenusEndX && x < WindowButtonsBeginX;
        }
    };
}
