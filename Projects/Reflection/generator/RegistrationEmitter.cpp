#include "RegistrationEmitter.h"
#include <reflection/PropertyAccess.h>

#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <type_traits>
#include <algorithm>
#include <variant>

#include <llvm/Support/raw_ostream.h>

namespace reflection_codegen
{
    namespace
    {
        std::string EmitString(const std::string& value)
        {
            std::ostringstream output;
            output.imbue(std::locale::classic());

            output << "std::string{\"";

            for (const unsigned char character : value)
            {
                if (character == '"' || character == '\\')
                {
                    output << '\\' << static_cast<char>(character);
                }
                else if (character >= 32 && character <= 126)
                {
                    output << static_cast<char>(character);
                }
                else
                {
                    // 고정 3자리 8진수로 출력하여 뒤 문자와 합쳐지지 않게 합니다.
                    output
                        << '\\'
                        << std::oct
                        << std::setw(3)
                        << std::setfill('0')
                        << static_cast<unsigned>(character)
                        << std::dec;
                }
            }

            // 문자열 내부의 NUL도 보존하도록 길이를 명시합니다.
            output << "\", " << value.size() << "}";

            return output.str();
        }

        std::string EmitValue(const reflection::MetadataValue& value)
        {
            return std::visit(
                [](const auto& item) -> std::string
                {
                    using T = std::remove_cvref_t<decltype(item)>;

                    if constexpr (std::is_same_v<T, std::string>)
                    {
                        return EmitString(item);
                    }
                    else if constexpr (std::is_same_v<T, bool>)
                    {
                        return item ? "true" : "false";
                    }
                    else if constexpr (std::is_same_v<T, std::int64_t>)
                    {
                        if (item == std::numeric_limits<std::int64_t>::min())
                        {
                            return "std::numeric_limits<std::int64_t>::min()";
                        }

                        return "std::int64_t{" +
                            std::to_string(item) + "LL}";
                    }
                    else if constexpr (std::is_same_v<T, std::uint64_t>)
                    {
                        return "std::uint64_t{" +
                            std::to_string(item) + "ULL}";
                    }
                    else
                    {
                        std::ostringstream output;
                        output.imbue(std::locale::classic());

                        output
                            << std::scientific
                            << std::setprecision(
                                std::numeric_limits<double>::max_digits10
                            )
                            << item;

                        return output.str();
                    }
                },
                value
            );
        }

        std::string EmitEnumValue(
            const EnumEntry& entry,
            bool isUnsigned
        )
        {
            if (isUnsigned)
            {
                return "std::uint64_t{" +
                    entry.DecimalValue + "ULL}";
            }

            // 최솟값의 양수 부분은 signed long long 범위를 넘습니다.
            if (entry.DecimalValue == "-9223372036854775808")
            {
                return "std::numeric_limits<std::int64_t>::min()";
            }

            return "std::int64_t{" +
                entry.DecimalValue + "LL}";
        }

        void EmitMetadata(
            std::ostream& output, const char* destination,
            const std::vector<reflection::MetadataEntry>& entries,
            const std::string& owner)
        {
            for (const auto& entry : entries)
            {
                output << "        if (!" << destination << ".Add("
                    << EmitString(entry.Key) << ", " << EmitValue(entry.Value) << "))\n"
                    << "        {\n"
                    << "            failure = {reflection::RegistrationError::DuplicateMetadata, "
                    << EmitString(owner) << ", " << EmitString(entry.Key) << "};\n"
                    << "            return false;\n"
                    << "        }\n";
            }
        }

        void EmitFunction(
            std::ostream& output, const DeclarationModel& function,
            reflection::InvocationBindingStatus status,
            const char* registryName = "registry")
        {
            using Status = reflection::InvocationBindingStatus;
            const auto& details = std::get<FunctionDetails>(function.Details);
            const bool canBind = status == Status::Available;

            if (!canBind)
            {
                llvm::errs() << "Reflection descriptor only: " << function.QualifiedName
                    << " [" << reflection::ToString(status) << "]\n";
            }

            output << "        {\n";

            if (canBind)
            {
                output << "        auto function = reflection::MakeFunction<"
                    << "static_cast<" << details.CanonicalReturnType;

                if (details.OwnerType.empty() || details.IsStaticMember)
                {
                    output << " (*)(";
                }
                else
                {
                    output << " (::" << details.OwnerType << "::*)(";
                }

                for (std::size_t index = 0; index < details.Parameters.size(); ++index)
                {
                    if (index != 0)
                    {
                        output << ", ";
                    }

                    output << details.Parameters[index].CanonicalTypeName;
                }

                output << ")";
                if (!details.IsStaticMember && details.IsConstMember)
                {
                    output << " const";
                }

                output << ">(&::" << function.QualifiedName << ")>();\n";
            }
            else
            {
                output << "        auto function = reflection::FunctionInfo::DescribeOnly("
                    << "reflection::InvocationBindingStatus::"
                    << reflection::ToString(status) << ");\n";
            }

            output << "        function.Name = " << EmitString(function.Name) << ";\n"
                << "        function.ReturnType = " << EmitString(details.ReturnType) << ";\n"
                << "        function.Signature = " << EmitString(details.Signature) << ";\n"
                << "        function.IsStaticMember = "
                << (details.IsStaticMember ? "true" : "false") << ";\n"
                << "        function.IsConstMember = "
                << (details.IsConstMember ? "true" : "false") << ";\n";

            for (const ParameterDetails& parameter : details.Parameters)
            {
                output << "        function.Parameters.push_back(reflection::ParameterInfo{"
                    << EmitString(parameter.Name) << ", "
                    << EmitString(parameter.CanonicalTypeName) << "});\n";
            }

            EmitMetadata(output, "function.Metadata", function.Metadata, function.QualifiedName);

            if (details.OwnerType.empty())
            {
                output << "        failure = " << registryName
                    << ".TryRegisterFreeFunction("
                    << EmitString(function.QualifiedName) << ", std::move(function));\n"
                    << "        if (!failure) return false;\n";
            }
            else
            {
                output << "        type.Functions.push_back(std::move(function));\n";
            }

            output << "        }\n";
        }

    }

    std::string EmitRegistrationHeader(
        const ReflectionUnit& unit,
        std::string_view moduleName,
        std::span<const std::string> headerPaths,
        std::string_view typeListMetadata)
    {
        std::ostringstream output;

        output << "// Generated by Reflection. Do not edit.\n"
            << "#pragma once\n\n"
            << "#include <reflection/Registry.h>\n";

        if (!typeListMetadata.empty())
        {
            for (const auto& path : headerPaths)
            {
                std::string header = path;
                std::replace(header.begin(), header.end(), '\\', '/');
                output << "#include \"" << header << "\"\n";
            }
        }

        output << "\nnamespace reflection_generated\n"
            << "{\n"
            << "    bool Register_" << moduleName << "(reflection::Registry& registry);\n"
            << "    bool Register_" << moduleName
            << "(reflection::Registry& registry, reflection::RegistrationResult& failure);\n";

        if (!typeListMetadata.empty())
        {
            output << "\n    template<typename Visitor>\n"
                << "    [[nodiscard]] bool ForEach_" << moduleName << "_Type(Visitor&& visitor)\n"
                << "    {\n"
                << "        (void)visitor;\n";

            for (const auto& type : unit.Types)
            {
                const auto& entries = type.Declaration.Metadata;

                const auto metadata = std::find_if(entries.begin(), entries.end(),
                    [&](const reflection::MetadataEntry& entry)
                    {
                        return entry.Key == typeListMetadata;
                    });

                if (metadata == entries.end())
                {
                    continue;
                }

                const auto* enabled = std::get_if<bool>(&metadata->Value);

                if (enabled == nullptr || !*enabled)
                {
                    continue;
                }

                output << "        if (!visitor.template operator()<::"
                    << type.Declaration.QualifiedName << ">())\n"
                    << "        {\n"
                    << "            return false;\n"
                    << "        }\n";
            }

            output << "        return true;\n"
                << "    }\n";
        }

        output << "}\n";
        return output.str();
    }

    std::string EmitRegistrationCpp(
        const ReflectionUnit& unit,
        std::string_view moduleName,
        std::span<const std::string> headerPaths,
        std::string_view generatedHeaderName)
    {
        std::ostringstream output;
        output.imbue(std::locale::classic());

        output << "// Generated by Reflection. Do not edit.\n"
            << "#include \"" << generatedHeaderName << "\"\n"
            << "#include <reflection/Registry.h>\n"
            << "#include <reflection/Annotations.h>\n";

        for (const std::string& path : headerPaths)
        {
            std::string header = path;
            std::replace(header.begin(), header.end(), '\\', '/');
            output << "#include \"" << header << "\"\n";
        }

        output << "#include <cstdint>\n"
            << "#include <limits>\n"
            << "#include <string>\n"
            << "#include <utility>\n\n"
            << "namespace reflection::detail\n"
            << "{\n";

        for (const ReflectedType& type : unit.Types)
        {
            const auto& typeDetails =
                std::get<TypeDetails>(type.Declaration.Details);

            output
                << "\ntemplate<>\n"
                << "struct TypeRegistration<::"
                << type.Declaration.QualifiedName << ">\n"
                << "{\n"
                << "    static bool Register(reflection::Registry& registry, "
                << "reflection::RegistrationResult& failure)\n"
                << "    {\n"
                << "        auto type = reflection::TypeInfo::For<::"
                << type.Declaration.QualifiedName << ">("
                << EmitString(type.Declaration.QualifiedName) << ");\n";

            EmitMetadata(
                output,
                "type.Metadata",
                type.Declaration.Metadata, type.Declaration.QualifiedName
            );

            for (const DeclarationModel& property : type.Properties)
            {
                const auto& details =
                    std::get<PropertyDetails>(property.Details);

                const bool hasAccess =
                    property.Access == AccessKind::Public ||
                    (
                        typeDetails.HasRegistrationFriend &&
                        (
                            property.Access == AccessKind::Private ||
                            property.Access == AccessKind::Protected
                            )
                        );

                using Status = reflection::PropertyAccessStatus;
                Status status = Status::Available;

                if (details.IsBitField)
                {
                    status = Status::BitField;
                }
                else if (details.IsReference)
                {
                    status = Status::ReferenceMember;
                }
                else if (details.IsVolatile)
                {
                    status = Status::VolatileQualified;
                }
                else if (!hasAccess)
                {
                    status = Status::InaccessibleMember;
                }

                const bool canBind = status == Status::Available;

                output << "        {\n";

                if (canBind)
                {
                    output
                        << "        auto property = reflection::MakeProperty<&::"
                        << details.OwnerType << "::"
                        << property.Name << ">(\n"
                        << "            " << EmitString(property.Name) << ",\n"
                        << "            "
                        << EmitString(details.CanonicalTypeName)
                        << ");\n";
                }
                else
                {
                    output
                        << "        auto property = reflection::PropertyInfo::DescribeOnly("
                        << "reflection::PropertyAccessStatus::"
                        << reflection::ToString(status) << ");\n"
                        << "        property.Name = "
                        << EmitString(property.Name) << ";\n"
                        << "        property.TypeName = "
                        << EmitString(details.CanonicalTypeName) << ";\n";
                }

                EmitMetadata(
                    output,
                    "property.Metadata",
                    property.Metadata,
                    property.QualifiedName
                );

                output
                    << "        type.Properties.push_back("
                    << "std::move(property));\n"
                    << "        }\n";
            }

            for (const DeclarationModel& function : type.Functions)
            {
                const auto& details = std::get<FunctionDetails>(function.Details);
                const bool hasAccess =
                    function.Access == AccessKind::Public ||
                    (typeDetails.HasRegistrationFriend &&
                        (function.Access == AccessKind::Private ||
                            function.Access == AccessKind::Protected));

                auto status = details.BindingStatus;
                if (!hasAccess && status == reflection::InvocationBindingStatus::Available)
                {
                    status = reflection::InvocationBindingStatus::InaccessibleMember;
                }

                EmitFunction(output, function, status);
            }

            output
                << "        failure = registry.TryRegister(std::move(type));\n"
                << "        return failure.Succeeded();\n"
                << "    }\n"
                << "};\n";
        }

        output << "}\n\n"
            << "namespace reflection_generated\n"
            << "{\n"
            << "bool Register_" << moduleName
            << "(reflection::Registry& registry, reflection::RegistrationResult& failure)\n"
            << "{\n"
            << "    failure = {};\n"
            << "    reflection::Registry staged;\n";

        for (const ReflectedType& type : unit.Types)
        {
            output << "    if (registry.FindType("
                << EmitString(type.Declaration.QualifiedName) << ") != nullptr)\n"
                << "    {\n"
                << "        failure = {reflection::RegistrationError::DuplicateType, "
                << EmitString(type.Declaration.QualifiedName) << ", {}};\n"
                << "        return false;\n"
                << "    }\n";
        }

        for (const DeclarationModel& enumeration : unit.Enums)
        {
            output << "    if (registry.FindEnum("
                << EmitString(enumeration.QualifiedName) << ") != nullptr)\n"
                << "    {\n"
                << "        failure = {reflection::RegistrationError::DuplicateEnum, "
                << EmitString(enumeration.QualifiedName) << ", {}};\n"
                << "        return false;\n"
                << "    }\n";
        }

        for (const DeclarationModel& function : unit.FreeFunctions)
        {
            const auto& details = std::get<FunctionDetails>(function.Details);
            output << "    if (registry.FindFreeFunction("
                << EmitString(function.QualifiedName) << ", "
                << EmitString(details.Signature) << ") != nullptr)\n"
                << "    {\n"
                << "        failure = {reflection::RegistrationError::DuplicateFreeFunction, "
                << EmitString(function.QualifiedName) << ", "
                << EmitString(details.Signature) << "};\n"
                << "        return false;\n"
                << "    }\n";
        }

        for (const ReflectedType& type : unit.Types)
        {
            output << "    if (!reflection::detail::TypeRegistration<::"
                << type.Declaration.QualifiedName
                << ">::Register(staged, failure)) return false;\n";
        }

        for (const DeclarationModel& enumeration : unit.Enums)
        {
            const auto& details = std::get<EnumDetails>(enumeration.Details);

            output << "    {\n"
                << "        auto enumeration = reflection::EnumInfo::For<::"
                << enumeration.QualifiedName << ">("
                << EmitString(enumeration.QualifiedName) << ");\n"
                << "        enumeration.UnderlyingType = "
                << EmitString(details.UnderlyingType) << ";\n"
                << "        enumeration.IsScoped = "
                << (details.IsScoped ? "true" : "false") << ";\n";

            EmitMetadata(
                output, "enumeration.Metadata", enumeration.Metadata,
                enumeration.QualifiedName);

            for (const EnumEntry& entry : details.Entries)
            {
                output << "        enumeration.Entries.push_back(reflection::EnumEntry{"
                    << EmitString(entry.Name) << ", "
                    << EmitEnumValue(entry, details.IsUnsigned) << "});\n";
            }

            output << "        failure = staged.TryRegisterEnum(std::move(enumeration));\n"
                << "        if (!failure) return false;\n"
                << "    }\n";
        }

        for (const DeclarationModel& function : unit.FreeFunctions)
        {
            const auto& details = std::get<FunctionDetails>(function.Details);
            EmitFunction(output, function, details.BindingStatus, "staged");
        }

        output << "    failure = registry.TryMerge(std::move(staged));\n"
            << "    return failure.Succeeded();\n"
            << "}\n\n"
            << "bool Register_" << moduleName << "(reflection::Registry& registry)\n"
            << "{\n"
            << "    reflection::RegistrationResult failure;\n"
            << "    return Register_" << moduleName << "(registry, failure);\n"
            << "}\n"
            << "}\n";

        return output.str();
    }
}