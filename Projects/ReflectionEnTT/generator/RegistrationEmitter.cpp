#include "RegistrationEmitter.h"

#include <entt/core/hashed_string.hpp>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

namespace reflection_entt_codegen
{
    namespace
    {
        std::string Literal(std::string_view value)
        {
            std::ostringstream out;
            out << '"';
            for (const unsigned char character : value)
            {
                if (character == '"' || character == '\\')
                {
                    out << '\\' << static_cast<char>(character);
                }
                else if (character >= 32 && character <= 126)
                {
                    out << static_cast<char>(character);
                }
                else
                {
                    out << '\\' << std::oct << std::setw(3) << std::setfill('0')
                        << static_cast<unsigned>(character) << std::dec;
                }
            }
            out << '"';
            return out.str();
        }

        std::string Value(const reflection_entt::MetadataValue& value)
        {
            return std::visit([](const auto& item) -> std::string
            {
                using T = std::remove_cvref_t<decltype(item)>;
                if constexpr (std::is_same_v<T, std::string>)
                {
                    return "std::string{" + Literal(item) + ", " + std::to_string(item.size()) + "}";
                }
                else if constexpr (std::is_same_v<T, bool>)
                {
                    return item ? "true" : "false";
                }
                else if constexpr (std::is_same_v<T, std::int64_t>)
                {
                    return item == std::numeric_limits<T>::min() ? "std::numeric_limits<std::int64_t>::min()"
                        : "std::int64_t{" + std::to_string(item) + "LL}";
                }
                else if constexpr (std::is_same_v<T, std::uint64_t>)
                {
                    return "std::uint64_t{" + std::to_string(item) + "ULL}";
                }
                else
                {
                    std::ostringstream out;
                    out.imbue(std::locale::classic());
                    out << std::scientific << std::setprecision(std::numeric_limits<double>::max_digits10) << item;
                    return out.str();
                }
            }, value);
        }

        void Metadata(std::ostream& out, const std::vector<reflection_entt::MetadataEntry>& entries)
        {
            out << "        {\n            reflection_entt::Metadata metadata;\n";
            for (const auto& entry : entries)
            {
                out << "            (void)metadata.Add(std::string{" << Literal(entry.Key) << ", "
                    << entry.Key.size() << "}, " << Value(entry.Value) << ");\n";
            }
            out << "            factory.custom<reflection_entt::Metadata>(std::move(metadata));\n        }\n";
        }

        std::string GlobalType(std::string type)
        {
            for (const auto prefix : {"struct ", "class ", "enum "})
            {
                if (type.starts_with(prefix))
                {
                    type.erase(0, std::char_traits<char>::length(prefix));
                    break;
                }
            }
            return type.starts_with("::") ? type : "::" + type;
        }

        std::string FunctionPointer(const FunctionDetails& function)
        {
            std::string pointer = function.CanonicalReturnType + " (";
            pointer += function.OwnerType.empty() || function.IsStaticMember
                ? "*" : "::" + function.OwnerType + "::*";
            pointer += ")(";
            for (std::size_t index = 0; index < function.Parameters.size(); ++index)
            {
                if (index != 0) pointer += ", ";
                pointer += function.Parameters[index].CanonicalTypeName;
            }
            pointer += ")";
            if (!function.IsStaticMember && !function.OwnerType.empty() && function.IsConstMember) pointer += " const";
            if (function.IsNoexcept) pointer += " noexcept";
            return pointer;
        }

        void Function(std::ostream& out, const DeclarationModel& declaration, std::string_view name)
        {
            const auto& function = std::get<FunctionDetails>(declaration.Details);
            out << "        factory.func<static_cast<" << FunctionPointer(function) << ">(&::"
                << declaration.QualifiedName << "), entt::as_is_t>(" << Literal(name) << ");\n";
            Metadata(out, declaration.Metadata);
        }

        // Hash collisions fail generation instead of silently overwriting EnTT registrations.
        void CheckIdentifier(std::unordered_map<entt::id_type, std::string>& identifiers, const std::string& name)
        {
            const auto id = entt::hashed_string::value(name.c_str());
            const auto [position, inserted] = identifiers.emplace(id, name);
            if (!inserted && position->second != name)
            {
                throw std::runtime_error("Identifier hash collision: " + name + " and " + position->second);
            }
        }

        void ValidateFunction(const DeclarationModel& declaration, bool hasFriend)
        {
            const auto& details = std::get<FunctionDetails>(declaration.Details);
            if (declaration.QualifiedName.find("(anonymous namespace)") != std::string::npos)
            {
                throw std::runtime_error("Anonymous namespace functions are not supported: " + declaration.QualifiedName);
            }
            if (details.BindingStatus != BindingStatus::Available)
            {
                throw std::runtime_error(declaration.QualifiedName + ": " + std::string(ToString(details.BindingStatus)));
            }
            if ((declaration.Access == AccessKind::Private || declaration.Access == AccessKind::Protected) && !hasFriend)
            {
                throw std::runtime_error("Non-public function requires REFLECT_BODY(): " + declaration.QualifiedName);
            }
        }

        void Validate(const ReflectionUnit& unit, std::string_view module)
        {
            std::unordered_map<entt::id_type, std::string> types;
            CheckIdentifier(types, "reflection_entt.module." + std::string(module));
            if (!unit.FreeFunctions.empty())
            {
                CheckIdentifier(types, "reflection_entt.functions." + std::string(module));
            }
            std::unordered_set<std::string> names;
            for (const auto& type : unit.Types)
            {
                const auto& details = std::get<TypeDetails>(type.Declaration.Details);
                if (type.Declaration.Access == AccessKind::Private || type.Declaration.Access == AccessKind::Protected)
                {
                    throw std::runtime_error("Non-public nested types are not supported: " + type.Declaration.QualifiedName);
                }
                if (details.IsTemplate) throw std::runtime_error("Template types are not supported: " + type.Declaration.QualifiedName);
                if (details.HasNonPublicBase) throw std::runtime_error("Only public inheritance is supported: " + type.Declaration.QualifiedName);
                if (type.Declaration.QualifiedName.find("(anonymous namespace)") != std::string::npos)
                {
                    throw std::runtime_error("Anonymous namespace types are not supported: " + type.Declaration.QualifiedName);
                }
                CheckIdentifier(types, type.Declaration.QualifiedName);
                names.insert(type.Declaration.QualifiedName);
                std::unordered_map<entt::id_type, std::string> properties, functions;
                std::unordered_set<std::string> memberNames, signatures;
                for (const auto& property : type.Properties)
                {
                    const auto& field = std::get<PropertyDetails>(property.Details);
                    if (field.IsBitField || field.IsReference || field.IsVolatile)
                    {
                        throw std::runtime_error("Unsupported bit-field/reference/volatile property: " + property.QualifiedName);
                    }
                    if (!memberNames.insert(property.Name).second)
                    {
                        throw std::runtime_error("Duplicate property: " + property.QualifiedName);
                    }
                    CheckIdentifier(properties, property.Name);
                }
                for (const auto& function : type.Functions)
                {
                    ValidateFunction(function, details.HasRegistrationFriend);
                    CheckIdentifier(functions, function.Name);
                    const auto& callable = std::get<FunctionDetails>(function.Details);
                    if (!signatures.insert(function.Name + "\n" + callable.Signature).second)
                    {
                        throw std::runtime_error("Duplicate function: " + function.QualifiedName);
                    }
                }
            }
            for (const auto& enumeration : unit.Enums)
            {
                if (enumeration.Access == AccessKind::Private || enumeration.Access == AccessKind::Protected ||
                    enumeration.QualifiedName.find("(anonymous namespace)") != std::string::npos)
                {
                    throw std::runtime_error("Non-public/anonymous enum is not supported: " + enumeration.QualifiedName);
                }
                if (!names.insert(enumeration.QualifiedName).second)
                {
                    throw std::runtime_error("Duplicate type or enum: " + enumeration.QualifiedName);
                }
                CheckIdentifier(types, enumeration.QualifiedName);
                std::unordered_map<entt::id_type, std::string> entries;
                for (const auto& entry : std::get<EnumDetails>(enumeration.Details).Entries)
                {
                    CheckIdentifier(entries, entry.Name);
                }
            }
            std::unordered_map<entt::id_type, std::string> functions;
            std::unordered_set<std::string> signatures;
            for (const auto& function : unit.FreeFunctions)
            {
                ValidateFunction(function, false);
                CheckIdentifier(functions, function.QualifiedName);
                const auto& callable = std::get<FunctionDetails>(function.Details);
                if (!signatures.insert(function.QualifiedName + "\n" + callable.Signature).second)
                {
                    throw std::runtime_error("Duplicate free function: " + function.QualifiedName);
                }
            }
        }
    }

    std::string EmitRegistrationHeader(std::string_view moduleName)
    {
        std::ostringstream out;
        out << "// Generated by ReflectionEnTT. Do not edit.\n#pragma once\n\n"
            << "#include <reflection_entt/Registration.h>\n\nnamespace reflection_entt_generated\n{\n"
            << "    bool Register_" << moduleName << "(entt::meta_ctx& context);\n"
            << "    bool Register_" << moduleName
            << "(entt::meta_ctx& context, reflection_entt::RegistrationResult& result);\n"
            << "    entt::meta_type FreeFunctions_" << moduleName << "(const entt::meta_ctx& context);\n}\n";
        return out.str();
    }

    std::string EmitRegistrationCpp(const ReflectionUnit& unit, std::string_view moduleName,
        std::span<const std::string> headerPaths, std::string_view generatedHeaderName)
    {
        Validate(unit, moduleName);
        std::ostringstream out;
        out << "// Generated by ReflectionEnTT. Do not edit.\n#include " << Literal(generatedHeaderName)
            << "\n#include <reflection_entt/Reflection.h>\n#include <limits>\n#include <utility>\n";
        for (auto path : headerPaths)
        {
            std::replace(path.begin(), path.end(), '\\', '/');
            out << "#include " << Literal(path) << "\n";
        }
        out << "\nnamespace reflection_entt::detail\n{\n"
            << "struct Module_" << moduleName << " {};\nstruct Functions_" << moduleName << " {};\n";
        for (const auto& type : unit.Types)
        {
            const auto& declaration = type.Declaration;
            const auto& details = std::get<TypeDetails>(declaration.Details);
            out << "\ntemplate<>\nstruct TypeRegistration<::" << declaration.QualifiedName << ">\n{\n"
                << "    static void Register(entt::meta_ctx& context)\n    {\n"
                << "        auto factory = entt::meta_factory<::" << declaration.QualifiedName << ">{context};\n"
                << "        factory.type(" << Literal(declaration.QualifiedName) << ");\n";
            Metadata(out, declaration.Metadata);
            for (const auto& base : details.BaseTypes)
            {
                out << "        factory.base<" << GlobalType(base) << ">();\n";
            }
            for (const auto& property : type.Properties)
            {
                out << "        factory.data<&::" << property.QualifiedName << ", entt::as_ref_t>("
                    << Literal(property.Name) << ");\n";
                Metadata(out, property.Metadata);
            }
            for (const auto& function : type.Functions) Function(out, function, function.Name);
            out << "    }\n};\n";
        }
        out << "}\n\nnamespace reflection_entt_generated\n{\nbool Register_" << moduleName
            << "(entt::meta_ctx& context, reflection_entt::RegistrationResult& result)\n{\n    result = {};\n";
        const auto check = [&](const std::string& cppType, const std::string& name)
        {
            out << "    if (!reflection_entt::detail::CheckType<" << cppType << ">(context, "
                << Literal(name) << ", result)) return false;\n";
        };
        check("reflection_entt::detail::Module_" + std::string(moduleName), "reflection_entt.module." + std::string(moduleName));
        for (const auto& type : unit.Types) check("::" + type.Declaration.QualifiedName, type.Declaration.QualifiedName);
        for (const auto& enumeration : unit.Enums) check("::" + enumeration.QualifiedName, enumeration.QualifiedName);
        if (!unit.FreeFunctions.empty())
        {
            check("reflection_entt::detail::Functions_" + std::string(moduleName),
                "reflection_entt.functions." + std::string(moduleName));
        }
        for (const auto& type : unit.Types)
        {
            out << "    reflection_entt::detail::TypeRegistration<::" << type.Declaration.QualifiedName
                << ">::Register(context);\n";
        }
        for (const auto& enumeration : unit.Enums)
        {
            out << "    {\n        auto factory = entt::meta_factory<::" << enumeration.QualifiedName << ">{context};\n"
                << "        factory.type(" << Literal(enumeration.QualifiedName) << ");\n";
            Metadata(out, enumeration.Metadata);
            for (const auto& entry : std::get<EnumDetails>(enumeration.Details).Entries)
            {
                out << "        factory.data<::" << enumeration.QualifiedName << "::" << entry.Name
                    << ">(" << Literal(entry.Name) << ");\n";
            }
            out << "    }\n";
        }
        if (!unit.FreeFunctions.empty())
        {
            out << "    {\n        auto factory = entt::meta_factory<reflection_entt::detail::Functions_"
                << moduleName << ">{context};\n        factory.type("
                << Literal("reflection_entt.functions." + std::string(moduleName)) << ");\n";
            for (const auto& function : unit.FreeFunctions) Function(out, function, function.QualifiedName);
            out << "    }\n";
        }
        out << "    entt::meta_factory<reflection_entt::detail::Module_" << moduleName << ">{context}.type("
            << Literal("reflection_entt.module." + std::string(moduleName)) << ");\n    return true;\n}\n\n"
            << "bool Register_" << moduleName << "(entt::meta_ctx& context)\n{\n"
            << "    reflection_entt::RegistrationResult result;\n    return Register_" << moduleName
            << "(context, result);\n}\n\nentt::meta_type FreeFunctions_" << moduleName
            << "(const entt::meta_ctx& context)\n{\n";
        if (unit.FreeFunctions.empty()) out << "    (void)context;\n    return {};\n";
        else out << "    return entt::resolve(context, entt::type_id<reflection_entt::detail::Functions_" << moduleName << ">());\n";
        out << "}\n}\n";
        return out.str();
    }
}
