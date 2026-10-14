#pragma once

#include "SettingsTestData.h"

#include <cstdint>
#include <string>

namespace archive_reflection_tests
{
    struct ProfileData
    {
        std::string Name = "Player";
        Settings Options;
        std::int32_t Level = 3;
    };

    inline bool SameProfile(const ProfileData& lhs, const ProfileData& rhs)
    {
        return lhs.Name == rhs.Name && SameSettings(lhs.Options, rhs.Options) && lhs.Level == rhs.Level;
    }
}