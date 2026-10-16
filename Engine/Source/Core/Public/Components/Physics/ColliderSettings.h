#pragma once

#include "Types.h"
#include <reflection/Annotations.h>

namespace Engine
{
    STRUCT(DISPLAY_NAME("STRUCT_COLLIDER_SETTINGS"))
    struct ColliderSettings
    {
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
        bool Enabled = true;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_TRIGGER"))
        bool IsTrigger = false;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FRICTION"))
        f32 Friction = 0.5f;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RESTITUTION"))
        f32 Restitution = 0.f;

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLLISION_LAYER"))
        uint32 CollisionLayer = 1u;
        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_COLLISION_MASK"))
        uint32 CollisionMask = 0xFFFFFFFFu;
    };
}