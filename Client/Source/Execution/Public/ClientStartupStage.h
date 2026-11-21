#pragma once

namespace Client
{
    enum class ClientStartupStage
    {
        Starting,
        Graphics,
        Resources,
        Scene,
        FirstFrame
    };
}
