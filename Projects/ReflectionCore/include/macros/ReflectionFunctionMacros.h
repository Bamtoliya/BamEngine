#pragma once

#include "ReflectionRegistry.h"

#define EMPTY_FUNCTIONS(ClassName) \
    static constexpr std::span<const Engine::FunctionInfo> ClassName##_Functions{};

#define DECLARE_FUNCTION_PARAMS(ClassName, FuncName) \
    static constexpr Engine::VariableInfo ClassName##_##FuncName##_Params[] = {

#define FUNCTION_PARAM(TypeName, TypeEnum) \
    {TypeName, TypeEnum},

#define END_FUNCTION_PARAMS };

#define BEGIN_FUNCTIONS(ClassName) \
    static constexpr Engine::FunctionInfo ClassName##_Functions[] = {

#define END_FUNCTIONS };

#define REFLECT_FUNCTION_EX(Class, OwnerQualifiedNameLiteral, FuncName, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    { Engine::CompileTimeHash(OwnerQualifiedNameLiteral "::" #FuncName), #FuncName, #FuncName, OwnerQualifiedNameLiteral, \
      {ReturnTypeName, ReturnTypeEnum}, ParamsSpan, &Engine::AutoThunk<&Class::FuncName> },

#define REFLECT_FUNCTION_OVERLOAD_EX(Class, OwnerQualifiedNameLiteral, DisplayName, SignatureText, FunctionPtr, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    { Engine::CompileTimeHash(OwnerQualifiedNameLiteral "::" SignatureText), DisplayName, SignatureText, OwnerQualifiedNameLiteral, \
      {ReturnTypeName, ReturnTypeEnum}, ParamsSpan, &Engine::AutoThunk<FunctionPtr> },

#define REFLECT_FUNCTION(Class, FuncName, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    REFLECT_FUNCTION_EX(Class, #Class, FuncName, ReturnTypeName, ReturnTypeEnum, ParamsSpan)

#define REFLECT_FUNCTION_OVERLOAD(Class, DisplayName, SignatureText, FunctionPtr, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    REFLECT_FUNCTION_OVERLOAD_EX(Class, #Class, DisplayName, SignatureText, FunctionPtr, ReturnTypeName, ReturnTypeEnum, ParamsSpan)