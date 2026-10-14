#include "TestSupport.h"
#include "../ExtraTypes.h"
#include "Demo.gen.h"

#include <reflection/Reflection.h>

#include <iostream>

namespace reflection_tests
{
    bool ValidateMultipleHeaders()
    {
        reflection::Registry registry;
        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        // 첫 번째 헤더와 두 번째 헤더의 타입이 함께 등록되어야 합니다.
        const auto* original = registry.FindType("example::MovementSettings");
        const auto* counter = registry.FindType("example::extra::Counter");
        if (original == nullptr || counter == nullptr)
        {
            return false;
        }

        const auto* value = counter->FindProperty("Value");
        if (value == nullptr || value->TypeName != "int")
        {
            return false;
        }

        example::extra::Counter object;
        const int* initial = value->Read<int>(object);
        if (initial == nullptr || *initial != 3)
        {
            return false;
        }

        if (!value->Write<int>(object, 5) || object.Value != 5)
        {
            return false;
        }

        const auto* twice =
            registry.FindFreeFunction("example::extra::Twice", "int (int)");
        if (twice == nullptr || !twice->CanInvoke())
        {
            return false;
        }

        const auto result = twice->InvokeValueWithoutObject<int>(object.Value);
        if (!result || *result != 10)
        {
            return false;
        }

        std::cout << "Additional type: " << counter->QualifiedName << '\n'
            << "Additional function result: " << *result << '\n'
            << "Multiple header checks passed.\n";
        return true;
    }

    bool ValidateModuleDiagnostics()
    {
        using Error = reflection::RegistrationError;

        reflection::Registry registry;
        reflection::RegistrationResult failure;

        // 이전 오류가 남아 있어도 성공하면 초기화해야 합니다.
        failure = { Error::InvalidProperty, "Previous", "Value" };
        if (!reflection_generated::Register_Demo(registry, failure) ||
            !failure || !failure.Owner.empty() || !failure.Member.empty())
        {
            return false;
        }

        if (reflection_generated::Register_Demo(registry, failure) ||
            failure.Error != Error::DuplicateType ||
            failure.Owner != "example::MovementSettings")
        {
            return false;
        }

        std::cout << "Module error: " << reflection::ToString(failure.Error)
            << " [" << failure.Owner << "]\n";

        const auto* enumeration = registry.FindEnum("example::MovementMode");
        const auto* function =
            registry.FindFreeFunction("example::Add", "float (float, float)");
        if (enumeration == nullptr || function == nullptr)
        {
            return false;
        }

        reflection::Registry enumConflict;
        if (!enumConflict.RegisterEnum(*enumeration))
        {
            return false;
        }

        if (reflection_generated::Register_Demo(enumConflict, failure) ||
            failure.Error != Error::DuplicateEnum ||
            failure.Owner != "example::MovementMode" ||
            enumConflict.FindType("example::MovementSettings") != nullptr)
        {
            return false;
        }

        std::cout << "Module error: " << reflection::ToString(failure.Error)
            << " [" << failure.Owner << "]\n";

        reflection::Registry functionConflict;
        if (!functionConflict.RegisterFreeFunction("example::Add", *function))
        {
            return false;
        }

        if (reflection_generated::Register_Demo(functionConflict, failure) ||
            failure.Error != Error::DuplicateFreeFunction ||
            failure.Owner != "example::Add" ||
            failure.Member != "float (float, float)" ||
            functionConflict.FindType("example::Player") != nullptr ||
            functionConflict.FindEnum("example::MovementMode") != nullptr)
        {
            return false;
        }

        std::cout << "Module error: " << reflection::ToString(failure.Error)
            << " [" << failure.Owner << "]\n"
            << "Module diagnostics checks passed.\n";
        return true;
    }
}