#pragma once

#include <reflection_entt/Metadata.h>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace reflection_entt_codegen
{
    enum class AccessKind { None, Public, Protected, Private };

    // Generator diagnostics, independent from either runtime's invocation API.
    enum class BindingStatus
    {
        Available, NotBound, UnsupportedPrototype, SpecialMemberFunction, DeletedFunction,
        UnsupportedQualifiers, RefQualifiedMember, VariadicFunction, InaccessibleMember
    };

    constexpr std::string_view ToString(BindingStatus status)
    {
        switch (status)
        {
        case BindingStatus::Available: return "Available";
        case BindingStatus::NotBound: return "NotBound";
        case BindingStatus::UnsupportedPrototype: return "UnsupportedPrototype";
        case BindingStatus::SpecialMemberFunction: return "SpecialMemberFunction";
        case BindingStatus::DeletedFunction: return "DeletedFunction";
        case BindingStatus::UnsupportedQualifiers: return "UnsupportedQualifiers";
        case BindingStatus::RefQualifiedMember: return "RefQualifiedMember";
        case BindingStatus::VariadicFunction: return "VariadicFunction";
        case BindingStatus::InaccessibleMember: return "InaccessibleMember";
        }
        return "Unknown";
    }

    struct TypeDetails
    {
        bool IsStruct = false;
        bool IsAbstract = false;
        bool IsTemplate = false;
        bool HasNonPublicBase = false;
        std::vector<std::string> BaseTypes;
        bool HasRegistrationFriend = false;
    };

    struct PropertyDetails
    {
        std::string OwnerType;
        std::string TypeName;
        std::string CanonicalTypeName;
        bool IsBitField = false;
        bool IsReference = false;
        bool IsVolatile = false;
    };

    struct ParameterDetails
    {
        std::string Name;
        std::string TypeName;
        std::string CanonicalTypeName;
    };

    struct FunctionDetails
    {
        std::string OwnerType;
        std::string ReturnType;
        std::string CanonicalReturnType;
        std::string Signature;
        bool IsStaticMember = false;
        bool IsConstMember = false;
        bool IsNoexcept = false;
        reflection_entt_codegen::BindingStatus BindingStatus = reflection_entt_codegen::BindingStatus::NotBound;
        std::vector<ParameterDetails> Parameters;
    };

    struct EnumEntry
    {
        std::string Name;
        std::string DecimalValue;
    };

    struct EnumDetails
    {
        bool IsScoped = false;
        bool IsUnsigned = false;
        std::string UnderlyingType;
        std::vector<EnumEntry> Entries;
    };

    struct DeclarationModel
    {
        std::string Name;
        std::string QualifiedName;
        AccessKind Access = AccessKind::None;
        std::vector<reflection_entt::MetadataEntry> Metadata;
        std::variant<TypeDetails, PropertyDetails, FunctionDetails, EnumDetails> Details;
    };
}
