#pragma once

#include "reflection/core/ContainerInfo.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace reflection
{
    using InvokeFunctionPtr = void* (*)(void* instance, void** args);

    struct FunctionInfo
    {
        std::uint64_t ID = 0;
        std::string_view Name{};
        std::string_view Signature{};
        std::string_view OwnerQualifiedName{};
        VariableInfo ReturnType{};
        std::span<const VariableInfo> Parameters{};
        InvokeFunctionPtr InvokeFunc = nullptr;

        void* Invoke(void* instance, void** args = nullptr) const
        {
            if (!InvokeFunc)
            {
                return nullptr;
            }
            return InvokeFunc(instance, args);
        }
    };
}