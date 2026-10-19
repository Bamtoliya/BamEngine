#pragma once

#include "Engine_Includes.h"
#include "LocalizationManager.h"
#include <string_view>

namespace Editor
{
    // Keep Engine diagnostics and native exception details intact; translate at the UI boundary.
    inline std::string LocalizeGraphMessage(std::string_view message)
    {
        auto& localization = Engine::LocalizationManager::Get();
        auto text = localization.GetText(std::string(message));
        if (text != message) return text;
        const auto separator = message.find(": ");
        if (separator == std::string_view::npos) return text;
        const std::string prefix(message.substr(0, separator + 1));
        auto translatedPrefix = localization.GetText(prefix);
        if (translatedPrefix == prefix) return text;
        return translatedPrefix + " " + std::string(message.substr(separator + 2));
    }
}
