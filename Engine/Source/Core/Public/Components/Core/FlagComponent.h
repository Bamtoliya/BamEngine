#pragma once

#include "Reflection/ComponentAnnotations.h"

#include "Interface/EnumBit.h"

#include <cstdint>

namespace Engine
{
    ENUM(BITMASK, DISPLAY_NAME("ENUM_ENTITY_FLAG"))
    enum class EEntityFlag : std::uint8_t
    {
        None = 0,
        Active = 1 << 0,
        Visible = 1 << 1,
        Paused = 1 << 2,
        Dead = 1 << 3,
        Default = Active | Visible
    };

    ENABLE_BITMASK_OPERATORS(EEntityFlag)

    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_FLAG"), CATEGORY("CATEGORY_CORE"))
    struct FlagComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_FLAG"))
        EEntityFlag flags = EEntityFlag::Default;

        PROPERTY(READONLY, TRANSIENT, DISPLAY_NAME("PROP_EFFECTIVE_FLAG"))
        EEntityFlag effectiveFlags = EEntityFlag::Default;
        bool IsActive() const { return HasFlag(flags, EEntityFlag::Active); }
        bool IsVisible() const { return HasFlag(flags, EEntityFlag::Visible); }
        bool IsActiveInHierarchy() const { return HasFlag(effectiveFlags, EEntityFlag::Active); }
        bool IsVisibleInHierarchy() const { return HasFlag(effectiveFlags, EEntityFlag::Visible); }
        void SetActive(bool active) { if (active) flags |= EEntityFlag::Active; else flags &= ~EEntityFlag::Active; }
        void SetVisible(bool visible) { if (visible) flags |= EEntityFlag::Visible; else flags &= ~EEntityFlag::Visible; }
        void ToggleVisible() { flags ^= EEntityFlag::Visible; }
    };
}