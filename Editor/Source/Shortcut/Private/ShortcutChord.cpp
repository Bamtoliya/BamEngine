#include "ShortcutChord.h"

bool ShortcutChord::Evaluate(Engine::InputManager& input) const
{
    return Evaluate(input, false);
}

bool ShortcutChord::Evaluate(Engine::InputManager& input, bool allowRepeat) const
{
    if (!IsValid())
        return false;

    if (input.IsCtrlPressed() != HasCtrl()) return false;
    if (input.IsShiftPressed() != HasShift()) return false;
    if (input.IsAltPressed() != HasAlt()) return false;

    return input.IsKeyButtonDown(Key) || (allowRepeat && input.IsKeyButtonRepeated(Key));
}

std::string ShortcutChord::ToString() const
{
    if (!IsValid())
        return "None";

    std::string result;
    if (HasCtrl())  result += "Ctrl+";
    if (HasShift()) result += "Shift+";
    if (HasAlt())   result += "Alt+";

    result += Engine::EngineKeyCodeToString(Key);
    return result;
}

ShortcutChord ShortcutChord::FromString(const std::string& str)
{
    if (str.empty() || str == "None")
        return ShortcutChord();

    uint8 modifiers = 0;
    std::string keyText = str;

    while (true)
    {
        const auto separator = keyText.find('+');
        if (separator == std::string::npos)
            break;

        std::string token = keyText.substr(0, separator);
        std::transform(token.begin(), token.end(), token.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        uint8 modifier = 0;

        if (token == "ctrl")
            modifier = static_cast<uint8>(EModifierKey::Ctrl);
        else if (token == "shift")
            modifier = static_cast<uint8>(EModifierKey::Shift);
        else if (token == "alt")
            modifier = static_cast<uint8>(EModifierKey::Alt);
        else
            break;

        if ((modifiers & modifier) != 0)
            return ShortcutChord();

        modifiers |= modifier;
        keyText.erase(0, separator + 1);
    }

    const auto key = Engine::StringToEngineKeyCode(keyText);
    if (key == Engine::EKeyCode::Unknown)
        return ShortcutChord();

    return ShortcutChord(key, modifiers);
}