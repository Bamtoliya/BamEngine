#pragma once

#include "Editor_Includes.h"
#include "InputManager.h"
#include "Interface/EnumBit.h"

BEGIN(Editor)

enum class EModifierKey : uint8
{
    None = 0,
    Ctrl = 1 << 0,
    Shift = 1 << 1,
    Alt = 1 << 2
};

ENABLE_BITMASK_OPERATORS(EModifierKey)

struct ShortcutChord
{
    Engine::EKeyCode Key = Engine::EKeyCode::Unknown;
    uint8 Modifiers = static_cast<uint8>(EModifierKey::None);

    ShortcutChord() = default;
    ShortcutChord(Engine::EKeyCode key, uint8 modifiers = 0)
        : Key(key), Modifiers(modifiers) {
    }
    ShortcutChord(Engine::EKeyCode key, EModifierKey modifier)
        : Key(key), Modifiers(static_cast<uint8>(modifier)) {
    }

    bool IsValid() const { return Key != Engine::EKeyCode::Unknown; }

    bool HasCtrl() const { return (Modifiers & static_cast<uint8>(EModifierKey::Ctrl)) != 0; }
    bool HasShift() const { return (Modifiers & static_cast<uint8>(EModifierKey::Shift)) != 0; }
    bool HasAlt() const { return (Modifiers & static_cast<uint8>(EModifierKey::Alt)) != 0; }

    bool operator==(const ShortcutChord& other) const
    {
        return Key == other.Key && Modifiers == other.Modifiers;
    }

    bool operator!=(const ShortcutChord& other) const { return !(*this == other); }

    bool Evaluate(Engine::InputManager& input) const;
    std::string ToString() const;
    static ShortcutChord FromString(const std::string& str);
};

END