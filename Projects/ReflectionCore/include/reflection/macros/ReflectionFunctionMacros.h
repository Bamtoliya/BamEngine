#pragma once

#include "reflection/core/FunctionInfo.h"
#include "reflection/core/QualifiedName.h"

#include <type_traits>
#include <utility>

namespace reflection
{
    template<typename Ret, typename Class, typename... Args>
    void* InvokeMemberFunction(Ret(Class::* fn)(Args...), void* instance, void** args)
    {
        return[&]<std::size_t... Index>(std::index_sequence<Index...>) -> void*
        {
            if constexpr (std::is_void_v<Ret>)
            {
                (static_cast<Class*>(instance)->*fn)(
                    *static_cast<std::remove_cvref_t<Args>*>(args[Index])...
                    );
                return nullptr;
            }
            else
            {
                static thread_local std::remove_cvref_t<Ret> return_value{};
                return_value = (static_cast<Class*>(instance)->*fn)(
                    *static_cast<std::remove_cvref_t<Args>*>(args[Index])...
                    );
                return &return_value;
            }
        }(std::index_sequence_for<Args...>{});
    }

    template<typename Ret, typename Class, typename... Args>
    void* InvokeMemberFunction(Ret(Class::* fn)(Args...) const, void* instance, void** args)
    {
        return[&]<std::size_t... Index>(std::index_sequence<Index...>) -> void*
        {
            if constexpr (std::is_void_v<Ret>)
            {
                (static_cast<Class*>(instance)->*fn)(
                    *static_cast<std::remove_cvref_t<Args>*>(args[Index])...
                    );
                return nullptr;
            }
            else
            {
                static thread_local std::remove_cvref_t<Ret> return_value{};
                return_value = (static_cast<Class*>(instance)->*fn)(
                    *static_cast<std::remove_cvref_t<Args>*>(args[Index])...
                    );
                return &return_value;
            }
        }(std::index_sequence_for<Args...>{});
    }

    template<auto Fn>
    void* AutoThunk(void* instance, void** args)
    {
        return InvokeMemberFunction(Fn, instance, args);
    }
}

#define EMPTY_FUNCTIONS(TypeName) \
    static constexpr std::span<const reflection::FunctionInfo> TypeName##_Functions{};

#define DECLARE_FUNCTION_PARAMS(TypeName, FunctionName) \
    static constexpr reflection::VariableInfo TypeName##_##FunctionName##_Params[] = {

#define FUNCTION_PARAM(TypeNameLiteral, TypeEnumValue) \
    { TypeNameLiteral, TypeEnumValue, nullptr },

#define END_FUNCTION_PARAMS \
    };

#define BEGIN_FUNCTIONS(TypeName) \
    static constexpr reflection::FunctionInfo TypeName##_Functions[] = {

#define END_FUNCTIONS \
    };

#define REFLECT_FUNCTION_EX(TypeName, OwnerQualifiedNameLiteral, FunctionName, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    { \
        reflection::CompileTimeHash(OwnerQualifiedNameLiteral "::" #FunctionName), \
        #FunctionName, \
        #FunctionName, \
        OwnerQualifiedNameLiteral, \
        { ReturnTypeName, ReturnTypeEnum, nullptr }, \
        ParamsSpan, \
        &reflection::AutoThunk<&TypeName::FunctionName> \
    },

#define REFLECT_FUNCTION_OVERLOAD_EX(TypeName, OwnerQualifiedNameLiteral, DisplayName, SignatureText, FunctionPtr, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    { \
        reflection::CompileTimeHash(OwnerQualifiedNameLiteral "::" SignatureText), \
        DisplayName, \
        SignatureText, \
        OwnerQualifiedNameLiteral, \
        { ReturnTypeName, ReturnTypeEnum, nullptr }, \
        ParamsSpan, \
        &reflection::AutoThunk<FunctionPtr> \
    },

#define REFLECT_FUNCTION(TypeName, FunctionName, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    REFLECT_FUNCTION_EX(TypeName, #TypeName, FunctionName, ReturnTypeName, ReturnTypeEnum, ParamsSpan)

#define REFLECT_FUNCTION_OVERLOAD(TypeName, DisplayName, SignatureText, FunctionPtr, ReturnTypeName, ReturnTypeEnum, ParamsSpan) \
    REFLECT_FUNCTION_OVERLOAD_EX(TypeName, #TypeName, DisplayName, SignatureText, FunctionPtr, ReturnTypeName, ReturnTypeEnum, ParamsSpan)