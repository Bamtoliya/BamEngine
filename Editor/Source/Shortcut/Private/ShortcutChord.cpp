#include "ShortcutChord.h"
#include <sstream>
#include <algorithm>

bool ShortcutChord::Evaluate(Engine::InputManager& input) const
{
    if (!IsValid())
        return false;

    // 보조키 상태가 기대치와 정확히 일치해야 함 (초과 눌림 방지)
    if (input.IsCtrlPressed() != HasCtrl()) return false;
    if (input.IsShiftPressed() != HasShift()) return false;
    if (input.IsAltPressed() != HasAlt()) return false;

    // 주 키는 이번 프레임에 눌린(Down) 순간만 참
    return input.IsKeyButtonDown(Key);
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
    std::string token;
    std::istringstream stream(str);
    std::vector<std::string> tokens;

    while (std::getline(stream, token, '+'))
    {
        if (!token.empty())
            tokens.push_back(token);
    }

    if (tokens.empty())
        return ShortcutChord();

    // 마지막 토큰 직전까지는 보조키
    for (size_t i = 0; i < tokens.size() - 1; ++i)
    {
        std::string mod = tokens[i];
        std::transform(mod.begin(), mod.end(), mod.begin(), [](unsigned char c) { return std::tolower(c); });

        if (mod == "ctrl")  modifiers |= static_cast<uint8>(EModifierKey::Ctrl);
        else if (mod == "shift") modifiers |= static_cast<uint8>(EModifierKey::Shift);
        else if (mod == "alt")   modifiers |= static_cast<uint8>(EModifierKey::Alt);
    }

    // 마지막 토큰은 주 키
    Engine::EKeyCode key = Engine::StringToEngineKeyCode(tokens.back());
    return ShortcutChord(key, modifiers);
}