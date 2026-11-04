#pragma once
#include "Types.h"
#include "Macro.h"

STRUCT(Component, Category = "Gameplay", DisplayName = "플레이어 스탯")
struct PlayerStatsComponent
{
    PROPERTY()
        float MoveSpeed = 5.0f;

    PROPERTY()
        int32 Level = 1;
};