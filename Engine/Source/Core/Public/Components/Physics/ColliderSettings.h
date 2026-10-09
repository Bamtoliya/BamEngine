#pragma once

#include "Types.h"
#include <reflection/Annotations.h>

namespace Engine
{
    STRUCT()
    struct ColliderSettings
    {
		REFLECT_BODY()

		PROPERTY(EDITABLE)
        bool Enabled = true;
        PROPERTY(EDITABLE)
        bool IsTrigger = false;

        PROPERTY(EDITABLE)
        f32 Friction = 0.5f;
        PROPERTY(EDITABLE)
        f32 Restitution = 0.f;

        PROPERTY(EDITABLE)
        uint32 CollisionLayer = 1u;
        PROPERTY(EDITABLE)
        uint32 CollisionMask = 0xFFFFFFFFu;
    };
}