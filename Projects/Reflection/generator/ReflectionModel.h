#pragma once

#include <reflection/Metadata.h>
#include <reflection/Invocation.h>

#include <string>
#include <variant>
#include <vector>

namespace reflection_codegen
{
    enum class AccessKind
    {
        None,
        Public,
        Protected,
        Private
    };

    struct TypeDetails
    {
        bool IsStruct = false;
        bool IsAbstract = false;
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
        reflection::InvocationBindingStatus BindingStatus = reflection::InvocationBindingStatus::NotBound;

        std::vector<ParameterDetails> Parameters;
    };

    struct EnumEntry
    {
        std::string Name;

        // signed/unsigned 값의 손실 없이 생성 코드에 전달합니다.
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

        std::vector<reflection::MetadataEntry> Metadata;

        std::variant<
            TypeDetails,
            PropertyDetails,
            FunctionDetails,
            EnumDetails
        > Details;
    };
}