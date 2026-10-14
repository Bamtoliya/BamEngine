#include "TestSupport.h"
#include "../ExampleTypes.h"
#include "Demo.gen.h"
#include "Failure.gen.h"

#include <reflection/Reflection.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <variant>

namespace reflection_tests
{
    bool ValidateRegistry()
    {
        reflection::Registry registry;

        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        const reflection::TypeInfo* type =
            registry.FindType("example::MovementSettings");

        if (type == nullptr)
        {
            return false;
        }

        const reflection::PropertyInfo* property =
            type->FindProperty("Speed");

        if (property == nullptr || property->TypeName != "float")
        {
            return false;
        }

        example::MovementSettings settings;

        const float* initial = property->Read<float>(settings);

        if (initial == nullptr || *initial != 10.0f)
        {
            return false;
        }

        if (!property->Write<float>(settings, 25.0f) ||
            settings.Speed != 25.0f)
        {
            return false;
        }

        const example::MovementSettings& readOnly = settings;

        const float* updated = property->Read<float>(readOnly);

        if (updated == nullptr || *updated != 25.0f)
        {
            return false;
        }

        // 값 타입이 다르면 접근하지 않습니다.
        if (property->Read<int>(settings) != nullptr ||
            property->Write<int>(settings, 3))
        {
            return false;
        }

        // 소유 타입이 다르면 접근하지 않습니다.
        example::Player wrongOwner;

        if (property->Read<float>(wrongOwner) != nullptr ||
            property->Write<float>(wrongOwner, 3.0f))
        {
            return false;
        }

        if (settings.Speed != 25.0f)
        {
            return false;
        }

        std::cout << "Reflected Speed: " << settings.Speed << '\n';

        const reflection::TypeInfo* playerType =
            registry.FindType("example::Player");

        if (playerType == nullptr)
        {
            return false;
        }

        const reflection::PropertyInfo* position =
            playerType->FindProperty("m_position");

        if (position == nullptr)
        {
            return false;
        }

        example::Player player;

        const float* initialPosition = position->Read<float>(player);

        if (initialPosition == nullptr || *initialPosition != 0.0f)
        {
            return false;
        }

        if (!position->Write<float>(player, 4.0f))
        {
            return false;
        }

        // 일반 멤버 함수와 리플렉션이 같은 값을 다루는지 확인합니다.
        player.Move(2.0f);

        const example::Player& constPlayer = player;
        const float* updatedPosition =
            position->Read<float>(constPlayer);

        if (updatedPosition == nullptr || *updatedPosition != 6.0f)
        {
            return false;
        }

        if (position->Read<int>(player) != nullptr ||
            position->Write<int>(player, 10))
        {
            return false;
        }

        std::cout
            << "Reflected private position: "
            << *updatedPosition << '\n';

        const reflection::MetadataValue* editable =
            property->Metadata.Find("Editable");

        if (editable == nullptr)
        {
            return false;
        }

        const bool* editableValue = std::get_if<bool>(editable);

        if (editableValue == nullptr || !*editableValue)
        {
            return false;
        }

        // 같은 타입 이름은 다시 등록할 수 없습니다.
        reflection::TypeInfo duplicate;
        duplicate.QualifiedName = "example::MovementSettings";

        if (registry.Register(std::move(duplicate)))
        {
            return false;
        }

        // 같은 이름의 프로퍼티가 있으면 타입 전체를 거부합니다.
        reflection::TypeInfo invalid;
        invalid.QualifiedName = "example::Invalid";

        reflection::PropertyInfo repeated;
        repeated.Name = "Value";
        repeated.TypeName = "int";

        invalid.Properties.push_back(repeated);
        invalid.Properties.push_back(repeated);

        if (registry.Register(std::move(invalid)))
        {
            return false;
        }

        if (registry.FindType("example::Invalid") != nullptr ||
            registry.FindType("example::Missing") != nullptr ||
            type->FindProperty("Missing") != nullptr)
        {
            return false;
        }

        std::cout
            << "Registered type: " << type->QualifiedName << '\n'
            << "Property: " << property->Name
            << " (" << property->TypeName << ")\n";

        return true;
    }

    bool ValidateRegistrationDiagnostics()
    {
        using Error = reflection::RegistrationError;

        reflection::Registry registry;
        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        reflection::TypeInfo duplicate;
        duplicate.QualifiedName = "example::Player";
        const auto duplicateResult = registry.TryRegister(std::move(duplicate));

        if (duplicateResult || duplicateResult.Error != Error::DuplicateType ||
            duplicateResult.Owner != "example::Player")
        {
            return false;
        }

        reflection::TypeInfo invalid;
        invalid.QualifiedName = "diagnostic::InvalidProperties";

        reflection::PropertyInfo property;
        property.Name = "Value";
        property.TypeName = "int";
        invalid.Properties.push_back(property);
        invalid.Properties.push_back(property);

        const auto propertyResult = registry.TryRegister(std::move(invalid));
        if (propertyResult || propertyResult.Error != Error::DuplicateProperty ||
            propertyResult.Member != "Value" ||
            registry.FindType("diagnostic::InvalidProperties") != nullptr)
        {
            return false;
        }

        reflection::TypeInfo invalidFunction;
        invalidFunction.QualifiedName = "diagnostic::InvalidParameters";

        reflection::FunctionInfo function;
        function.Name = "Run";
        function.ReturnType = "void";
        function.Signature = "void (int)";
        function.Parameters.push_back({ "value", "" });
        invalidFunction.Functions.push_back(function);

        const auto parameterResult = registry.TryRegister(std::move(invalidFunction));
        if (parameterResult || parameterResult.Error != Error::InvalidParameter ||
            parameterResult.Member != "Run parameter #0" ||
            registry.FindType("diagnostic::InvalidParameters") != nullptr)
        {
            return false;
        }

        reflection::EnumInfo invalidEnum;
        invalidEnum.QualifiedName = "diagnostic::InvalidEnum";
        invalidEnum.UnderlyingType = "int";
        invalidEnum.Entries.push_back({ "Same", std::int64_t{0} });
        invalidEnum.Entries.push_back({ "Same", std::int64_t{1} });

        const auto enumResult = registry.TryRegisterEnum(std::move(invalidEnum));
        if (enumResult || enumResult.Error != Error::DuplicateEnumEntry ||
            enumResult.Member != "Same" ||
            registry.FindEnum("diagnostic::InvalidEnum") != nullptr)
        {
            return false;
        }

        const auto* existing =
            registry.FindFreeFunction("example::Add", "float (float, float)");
        if (existing == nullptr)
        {
            return false;
        }

        const auto freeResult = registry.TryRegisterFreeFunction("example::Add", *existing);
        if (freeResult || freeResult.Error != Error::DuplicateFreeFunction)
        {
            return false;
        }

        // 중복 등록 실패 후에도 원래 함수가 유지되어야 합니다.
        const auto sum = existing->InvokeValueWithoutObject<float>(1.0f, 2.0f);
        if (!sum || *sum != 3.0f)
        {
            return false;
        }

        reflection::TypeInfo valid;
        valid.QualifiedName = "diagnostic::Valid";
        const auto success = registry.TryRegister(std::move(valid));
        if (!success || success.Error != Error::None ||
            registry.FindType("diagnostic::Valid") == nullptr)
        {
            return false;
        }

        std::cout << "Registration error: " << reflection::ToString(duplicateResult.Error) << '\n'
            << "Registration error: " << reflection::ToString(propertyResult.Error) << '\n'
            << "Registration error: " << reflection::ToString(parameterResult.Error) << '\n'
            << "Registration error: " << reflection::ToString(enumResult.Error) << '\n'
            << "Registration error: " << reflection::ToString(freeResult.Error) << '\n'
            << "Registration diagnostics checks passed.\n";
        return true;
    }

    bool ValidateAtomicRegistration()
    {
        using Error = reflection::RegistrationError;

        reflection::Registry registry;
        reflection::TypeInfo seed;
        seed.QualifiedName = "atomic::Seed";

        if (!registry.TryRegister(std::move(seed)))
        {
            return false;
        }

        const auto* seedBefore = registry.FindType("atomic::Seed");
        reflection::RegistrationResult failure;

        // 타입 등록 후 자유 함수에서 실패해도 대상에 타입이 남으면 안 됩니다.
        if (reflection_generated::Register_Failure(registry, failure))
        {
            return false;
        }

        if (failure.Error != Error::DuplicateFreeFunction ||
            failure.Owner != "batch_failure::Duplicate")
        {
            return false;
        }

        if (registry.FindType("batch_failure::ShouldNotAppear") != nullptr ||
            registry.FindFreeFunction("batch_failure::Duplicate", "void (void)") != nullptr)
        {
            return false;
        }

        // 정상 모듈 병합 후에도 기존 타입 포인터가 유지되어야 합니다.
        if (!reflection_generated::Register_Demo(registry, failure))
        {
            return false;
        }

        if (registry.FindType("atomic::Seed") != seedBefore)
        {
            return false;
        }

        // 같은 함수 이름의 다른 오버로드를 병합합니다.
        reflection::Registry overloads;
        const auto* floatAdd = registry.FindFreeFunction("example::Add", "float (float, float)");
        const auto* intAdd = registry.FindFreeFunction("example::Add", "int (int, int)");

        if (floatAdd == nullptr || intAdd == nullptr)
        {
            return false;
        }

        reflection::Registry destination;
        if (!destination.TryRegisterFreeFunction("merge::Add", *floatAdd) ||
            !overloads.TryRegisterFreeFunction("merge::Add", *intAdd))
        {
            return false;
        }

        const auto* functionBefore =
            destination.FindFreeFunction("merge::Add", floatAdd->Signature);

        if (!destination.TryMerge(std::move(overloads)))
        {
            return false;
        }

        if (destination.FindFreeFunction("merge::Add", floatAdd->Signature) != functionBefore ||
            destination.FindFreeFunction("merge::Add", intAdd->Signature) == nullptr ||
            overloads.FindFreeFunction("merge::Add", intAdd->Signature) != nullptr)
        {
            return false;
        }

        // 병합 충돌 시 고유 항목도 대상에 옮겨지면 안 됩니다.
        reflection::Registry conflicting;
        reflection::TypeInfo duplicate;
        duplicate.QualifiedName = "atomic::Seed";

        reflection::TypeInfo unique;
        unique.QualifiedName = "atomic::Uncommitted";

        if (!conflicting.TryRegister(std::move(duplicate)) ||
            !conflicting.TryRegister(std::move(unique)))
        {
            return false;
        }

        const auto mergeFailure = registry.TryMerge(std::move(conflicting));
        if (mergeFailure.Error != Error::DuplicateType ||
            registry.FindType("atomic::Uncommitted") != nullptr ||
            conflicting.FindType("atomic::Seed") == nullptr ||
            conflicting.FindType("atomic::Uncommitted") == nullptr)
        {
            return false;
        }

        if (registry.TryMerge(std::move(registry)).Error != Error::InvalidMergeSource)
        {
            return false;
        }

        std::cout << "Atomic registration checks passed.\n";
        return true;
    }
}