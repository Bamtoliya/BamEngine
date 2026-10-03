#pragma once

#include "ReflectionModel.h"

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace reflection_codegen
{
    struct ReflectedType
    {
        DeclarationModel Declaration;
        std::vector<DeclarationModel> Properties;
        std::vector<DeclarationModel> Functions;
    };

    struct ReflectionUnit
    {
        std::vector<ReflectedType> Types;
        std::vector<DeclarationModel> FreeFunctions;
        std::vector<DeclarationModel> Enums;
    };

    struct UnitBuildResult
    {
        ReflectionUnit Unit;
        std::string Error;
        std::size_t DeclarationIndex = 0;

        bool Succeeded() const
        {
            return Error.empty();
        }
    };

    UnitBuildResult BuildReflectionUnit(
        std::span<const DeclarationModel> declarations
    );
}