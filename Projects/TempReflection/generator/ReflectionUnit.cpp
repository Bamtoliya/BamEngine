#include "ReflectionUnit.h"

#include <unordered_map>
#include <utility>

namespace reflection_codegen
{
    UnitBuildResult BuildReflectionUnit(
        std::span<const DeclarationModel> declarations
    )
    {
        UnitBuildResult result;

        std::unordered_map<std::string, std::size_t> typeIndices;

        const auto fail = [&](
            std::size_t index,
            std::string message
            )
            {
                result.Error = std::move(message);
                result.DeclarationIndex = index;
                result.Unit = ReflectionUnit{};
            };

        // 먼저 타입을 모두 수집합니다.
        for (std::size_t index = 0;
            index < declarations.size();
            ++index)
        {
            const DeclarationModel& declaration = declarations[index];

            if (!std::holds_alternative<TypeDetails>(
                declaration.Details))
            {
                continue;
            }

            const std::size_t typeIndex = result.Unit.Types.size();

            if (!typeIndices.emplace(
                declaration.QualifiedName,
                typeIndex
            ).second)
            {
                fail(
                    index,
                    "Duplicate reflected type: " +
                    declaration.QualifiedName
                );

                return result;
            }

            result.Unit.Types.push_back(
                ReflectedType{ declaration, {}, {} }
            );
        }

        // 그다음 소유 타입을 기준으로 멤버를 연결합니다.
        for (std::size_t index = 0;
            index < declarations.size();
            ++index)
        {
            const DeclarationModel& declaration = declarations[index];

            if (const auto* property =
                std::get_if<PropertyDetails>(&declaration.Details))
            {
                const auto owner = typeIndices.find(property->OwnerType);

                if (owner == typeIndices.end())
                {
                    fail(
                        index,
                        "Property owner has no CLASS/STRUCT annotation: " +
                        property->OwnerType
                    );

                    return result;
                }

                const auto& ownerDetails =
                    std::get<TypeDetails>(
                        result.Unit.Types[owner->second]
                        .Declaration.Details
                    );

                const bool needsFriend =
                    declaration.Access == AccessKind::Private ||
                    declaration.Access == AccessKind::Protected;

                const bool supportsValueAccess =
                    !property->IsBitField &&
                    !property->IsReference &&
                    !property->IsVolatile;

                if (needsFriend &&
                    supportsValueAccess &&
                    !ownerDetails.HasRegistrationFriend)
                {
                    fail(
                        index,
                        "Non-public property requires REFLECT_BODY(): " +
                        property->OwnerType
                    );

                    return result;
                }

                result.Unit.Types[owner->second].Properties.push_back(
                    declaration
                );
            }
            else if (const auto* function =
                std::get_if<FunctionDetails>(&declaration.Details))
            {
                if (function->OwnerType.empty())
                {
                    result.Unit.FreeFunctions.push_back(declaration);
                    continue;
                }

                const auto owner = typeIndices.find(function->OwnerType);

                if (owner == typeIndices.end())
                {
                    fail(
                        index,
                        "Function owner has no CLASS/STRUCT annotation: " +
                        function->OwnerType
                    );

                    return result;
                }

                result.Unit.Types[owner->second].Functions.push_back(
                    declaration
                );
            }
            else if (std::holds_alternative<EnumDetails>(
                declaration.Details))
            {
                result.Unit.Enums.push_back(declaration);
            }
        }

        return result;
    }
}