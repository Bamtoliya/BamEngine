#pragma once

#include <string_view>

namespace reflection
{
    enum class PropertyAccessStatus
    {
        Available,
        NotBound,
        ConstQualified,
        NotAssignable,
        BitField,
        ReferenceMember,
        VolatileQualified,
        InaccessibleMember
    };

    constexpr std::string_view ToString(PropertyAccessStatus status)
    {
        switch (status)
        {
        case PropertyAccessStatus::Available: return "Available";
        case PropertyAccessStatus::NotBound: return "NotBound";
        case PropertyAccessStatus::ConstQualified: return "ConstQualified";
        case PropertyAccessStatus::NotAssignable: return "NotAssignable";
        case PropertyAccessStatus::BitField: return "BitField";
        case PropertyAccessStatus::ReferenceMember: return "ReferenceMember";
        case PropertyAccessStatus::VolatileQualified: return "VolatileQualified";
        case PropertyAccessStatus::InaccessibleMember: return "InaccessibleMember";
        }

        return "Unknown";
    }
}