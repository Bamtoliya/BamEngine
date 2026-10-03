#pragma once

#include <string_view>

namespace reflection
{
    enum class InvocationBindingStatus
    {
        Available,
        NotBound,
        UnsupportedPrototype,
        SpecialMemberFunction,
        DeletedFunction,
        UnsupportedReturnType,
        UnsupportedQualifiers,
        RefQualifiedMember,
        VariadicFunction,
        ExceptionSpecification,
        ReferenceParameter,
        InaccessibleMember
    };

    constexpr std::string_view ToString(InvocationBindingStatus status)
    {
        switch (status)
        {
        case InvocationBindingStatus::Available: return "Available";
        case InvocationBindingStatus::NotBound: return "NotBound";
        case InvocationBindingStatus::UnsupportedPrototype: return "UnsupportedPrototype";
        case InvocationBindingStatus::SpecialMemberFunction: return "SpecialMemberFunction";
        case InvocationBindingStatus::DeletedFunction: return "DeletedFunction";
        case InvocationBindingStatus::UnsupportedReturnType: return "UnsupportedReturnType";
        case InvocationBindingStatus::UnsupportedQualifiers: return "UnsupportedQualifiers";
        case InvocationBindingStatus::RefQualifiedMember: return "RefQualifiedMember";
        case InvocationBindingStatus::VariadicFunction: return "VariadicFunction";
        case InvocationBindingStatus::ExceptionSpecification: return "ExceptionSpecification";
        case InvocationBindingStatus::ReferenceParameter: return "ReferenceParameter";
        case InvocationBindingStatus::InaccessibleMember: return "InaccessibleMember";
        }

        return "Unknown";
    }
}