#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <reflection/TypeInfo.h>

namespace archive_reflection_tests
{
    struct Settings
    {
        bool Enabled = true;
        float Speed = 25.0f;
        std::int64_t Minimum = std::numeric_limits<std::int64_t>::min();
        std::uint64_t Maximum = std::numeric_limits<std::uint64_t>::max();
        std::string Name = "Movement";
        std::wstring Title = L"이동 설정 \U0001F600";
    };

    inline reflection::TypeInfo MakeSettingsType()
    {
        auto type = reflection::TypeInfo::For<Settings>("example::Settings");
        type.Properties.push_back(reflection::MakeProperty<&Settings::Enabled>("Enabled", "bool"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Speed>("Speed", "float"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Minimum>("Minimum", "std::int64_t"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Maximum>("Maximum", "std::uint64_t"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Name>("Name", "std::string"));
        type.Properties.push_back(reflection::MakeProperty<&Settings::Title>("Title", "std::wstring"));
        return type;
    }

    inline bool SameSettings(const Settings& lhs, const Settings& rhs)
    {
        return lhs.Enabled == rhs.Enabled && lhs.Speed == rhs.Speed &&
            lhs.Minimum == rhs.Minimum && lhs.Maximum == rhs.Maximum &&
            lhs.Name == rhs.Name && lhs.Title == rhs.Title;
    }
}