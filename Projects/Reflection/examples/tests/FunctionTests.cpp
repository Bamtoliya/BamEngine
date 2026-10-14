#include "TestSupport.h"
#include "../ExampleTypes.h"
#include "../DiagnosticsTypes.h"
#include "Demo.gen.h"

#include <reflection/Reflection.h>

#include <initializer_list>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace reflection_tests
{
    bool ValidateGeneratedFunctions()
    {
        reflection::Registry registry;

        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        const reflection::TypeInfo* player =
            registry.FindType("example::Player");

        if (player == nullptr || player->Functions.size() != 8)
        {
            return false;
        }

        const reflection::FunctionInfo* floatMove =
            player->FindFunction("Move", "void (float)");
        const reflection::FunctionInfo* intMove =
            player->FindFunction("Move", "void (int)");

        if (floatMove == nullptr ||
            intMove == nullptr ||
            floatMove == intMove)
        {
            return false;
        }

        if (floatMove->ReturnType != "void" ||
            floatMove->IsStaticMember ||
            floatMove->IsConstMember ||
            floatMove->Parameters.size() != 1 ||
            intMove->Parameters.size() != 1)
        {
            return false;
        }

        const reflection::ParameterInfo& parameter =
            floatMove->Parameters.front();

        if (parameter.Name != "distance" ||
            parameter.TypeName != "float" ||
            intMove->Parameters.front().TypeName != "int")
        {
            return false;
        }

        const reflection::MetadataValue* metadata =
            floatMove->Metadata.Find("Category");

        if (metadata == nullptr)
        {
            return false;
        }

        const auto* category = std::get_if<std::string>(metadata);

        if (category == nullptr || *category != "Movement")
        {
            return false;
        }

        if (player->FindFunction("Move", "void (double)") != nullptr ||
            player->FindFunction("Missing", "void (float)") != nullptr)
        {
            return false;
        }

        if (!floatMove->CanInvoke() || !intMove->CanInvoke())
        {
            return false;
        }

        const reflection::PropertyInfo* position =
            player->FindProperty("m_position");

        if (position == nullptr)
        {
            return false;
        }

        example::Player object;
        example::MovementSettings wrongOwner;

        // 함수 선택 이후에는 암시적 인자 변환을 하지 않습니다.
        if (floatMove->Invoke(object, 2) ||
            intMove->Invoke(object, 2.0f) ||
            floatMove->Invoke(object, 2.0) ||
            floatMove->Invoke(object) ||
            floatMove->Invoke(wrongOwner, 2.0f))
        {
            return false;
        }

        const float* unchanged = position->Read<float>(object);

        if (unchanged == nullptr || *unchanged != 0.0f)
        {
            return false;
        }

        if (!floatMove->Invoke(object, 2.5f) ||
            !intMove->Invoke(object, 3))
        {
            return false;
        }

        const float* updated = position->Read<float>(object);

        if (updated == nullptr || *updated != 5.5f)
        {
            return false;
        }

        // 바인딩 없는 정보는 호출할 수 없습니다.
        reflection::FunctionInfo descriptorOnly;

        if (descriptorOnly.CanInvoke() ||
            descriptorOnly.Invoke(object, 1.0f))
        {
            return false;
        }

        // FunctionInfo를 복사해도 함수 포인터 바인딩이 유지됩니다.
        reflection::FunctionInfo copied = *floatMove;

        if (!copied.Invoke(object, 1.0f))
        {
            return false;
        }

        const float* finalPosition = position->Read<float>(object);

        if (finalPosition == nullptr || *finalPosition != 6.5f)
        {
            return false;
        }

        std::cout
            << "Invoked reflected functions: position = "
            << *finalPosition << '\n';

        const reflection::FunctionInfo* copyPosition =
            player->FindFunction(
                "CopyPosition",
                "void (float *) const"
            );

        if (copyPosition == nullptr ||
            !copyPosition->IsConstMember ||
            !copyPosition->CanInvoke())
        {
            return false;
        }

        // 일반 객체에서도 const 멤버 함수를 호출할 수 있습니다.
        float copiedPosition = -1.0f;

        if (!copyPosition->Invoke(object, &copiedPosition) ||
            copiedPosition != 6.5f)
        {
            return false;
        }

        // 실제 const 객체에서도 호출할 수 있어야 합니다.
        const example::Player frozen = object;
        float frozenPosition = -1.0f;

        if (!copyPosition->Invoke(frozen, &frozenPosition) ||
            frozenPosition != 6.5f)
        {
            return false;
        }

        // const 객체에서 일반 멤버 함수 호출은 거부합니다.
        if (floatMove->Invoke(frozen, 1.0f) ||
            intMove->Invoke(frozen, 1))
        {
            return false;
        }

        // 인자 타입이 다르면 외부 값도 변경하지 않아야 합니다.
        double wrongOutput = -1.0;

        if (copyPosition->Invoke(frozen, &wrongOutput) ||
            wrongOutput != -1.0)
        {
            return false;
        }

        const float* frozenMember = position->Read<float>(frozen);

        if (frozenMember == nullptr || *frozenMember != 6.5f)
        {
            return false;
        }

        // const 멤버 함수의 반환값을 확인합니다.
        const reflection::FunctionInfo* getter =
            player->FindFunction("GetPosition", "float (void) const");

        if (getter == nullptr || !getter->CanInvoke())
        {
            return false;
        }

        const auto mutableResult = getter->InvokeValue<float>(object);
        const auto constResult = getter->InvokeValue<float>(frozen);

        if (!mutableResult || *mutableResult != 6.5f ||
            !constResult || *constResult != 6.5f)
        {
            return false;
        }

        // 반환형, 소유 타입, 인자 개수가 다르면 호출하지 않습니다.
        if (getter->InvokeValue<double>(frozen) ||
            getter->InvokeValue<float>(wrongOwner) ||
            getter->InvokeValue<float>(frozen, 1))
        {
            return false;
        }

        // void 호출 API와 값 반환 API는 서로 다른 함수 포인터를 요구합니다.
        if (getter->Invoke(frozen) ||
            floatMove->InvokeValue<float>(object, 1.0f))
        {
            return false;
        }

        // 0도 비어 있지 않은 정상 반환값이어야 합니다.
        const example::Player initialObject;
        const auto zero = getter->InvokeValue<float>(initialObject);

        if (!zero || *zero != 0.0f)
        {
            return false;
        }

        const reflection::FunctionInfo* advance =
            player->FindFunction("Advance", "float (float)");

        if (advance == nullptr || !advance->CanInvoke())
        {
            return false;
        }

        example::Player advancingObject;
        const example::Player& readOnlyAdvancing = advancingObject;

        if (advance->InvokeValue<float>(readOnlyAdvancing, 2.0f) ||
            advance->InvokeValue<double>(advancingObject, 2.0f) ||
            advance->InvokeValue<float>(advancingObject, 2))
        {
            return false;
        }

        const auto unchangedValue =
            getter->InvokeValue<float>(advancingObject);

        if (!unchangedValue || *unchangedValue != 0.0f)
        {
            return false;
        }

        const auto advanced =
            advance->InvokeValue<float>(advancingObject, 2.5f);
        const auto observed =
            getter->InvokeValue<float>(readOnlyAdvancing);

        if (!advanced || *advanced != 2.5f ||
            !observed || *observed != 2.5f)
        {
            return false;
        }

        if (descriptorOnly.InvokeValue<float>(object))
        {
            return false;
        }

        std::cout
            << "Reflected return value: " << *constResult << '\n'
            << "Reflected advance result: " << *advanced << '\n'
            << "Return value checks passed.\n";

        std::cout
            << "Const reflected call: position = "
            << frozenPosition << '\n'
            << "Const invocation checks passed.\n";

        // 같은 이름과 시그니처의 중복을 거부합니다.
        reflection::TypeInfo invalid;
        invalid.QualifiedName = "example::InvalidFunctions";
        invalid.Functions.push_back(*floatMove);
        invalid.Functions.push_back(*floatMove);

        if (registry.Register(std::move(invalid)) ||
            registry.FindType("example::InvalidFunctions") != nullptr)
        {
            return false;
        }

        std::cout
            << "Registered function: " << floatMove->Name
            << " " << floatMove->Signature << '\n'
            << "Registered overload: " << intMove->Name
            << " " << intMove->Signature << '\n'
            << "Generated function checks passed.\n";

        return true;
    }

    bool ValidateStaticFunctions()
    {
        reflection::Registry registry;
        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        const auto* player = registry.FindType("example::Player");
        if (player == nullptr)
        {
            return false;
        }

        const auto* floatScale = player->FindFunction("Scale", "float (float)");
        const auto* intScale = player->FindFunction("Scale", "int (int)");
        const auto* store = player->FindFunction("Store", "void (float *, float)");
        const auto* move = player->FindFunction("Move", "void (float)");

        if (floatScale == nullptr || intScale == nullptr ||
            store == nullptr || move == nullptr)
        {
            return false;
        }

        for (const auto* function : { floatScale, intScale, store })
        {
            if (!function->IsStaticMember || function->IsConstMember ||
                !function->CanInvoke())
            {
                return false;
            }
        }

        const auto floatResult = floatScale->InvokeValueWithoutObject<float>(2.5f);
        const auto intResult = intScale->InvokeValueWithoutObject<int>(4);
        const auto zero = floatScale->InvokeValueWithoutObject<float>(0.0f);

        if (!floatResult || *floatResult != 5.0f ||
            !intResult || *intResult != 12 || !zero || *zero != 0.0f)
        {
            return false;
        }

        // 반환형·인자 타입·개수가 다르면 호출하지 않습니다.
        if (floatScale->InvokeValueWithoutObject<double>(2.5f) ||
            floatScale->InvokeValueWithoutObject<float>(2) ||
            floatScale->InvokeValueWithoutObject<float>() ||
            intScale->InvokeValueWithoutObject<int>(2.0f))
        {
            return false;
        }

        float output = -1.0f;
        if (!store->InvokeWithoutObject(&output, 7.0f) || output != 7.0f)
        {
            return false;
        }

        if (store->InvokeWithoutObject(&output, 8) ||
            store->InvokeValueWithoutObject<float>(&output, 8.0f) ||
            output != 7.0f)
        {
            return false;
        }

        // 객체 기반 API와 객체 없는 API를 혼용하지 않습니다.
        example::Player object;
        if (floatScale->InvokeValue<float>(object, 2.5f) ||
            store->Invoke(object, &output, 9.0f) ||
            move->InvokeWithoutObject(1.0f) ||
            floatScale->InvokeWithoutObject(2.5f) || output != 7.0f)
        {
            return false;
        }

        const reflection::FunctionInfo copied = *floatScale;
        const auto copiedResult = copied.InvokeValueWithoutObject<float>(3.0f);
        if (!copiedResult || *copiedResult != 6.0f)
        {
            return false;
        }

        std::cout << "Static float result: " << *floatResult << '\n'
            << "Static int result: " << *intResult << '\n'
            << "Static invocation checks passed.\n";
        return true;
    }

    bool ValidateFreeFunctions()
    {
        reflection::Registry registry;
        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        const auto* floatAdd =
            registry.FindFreeFunction("example::Add", "float (float, float)");
        const auto* intAdd =
            registry.FindFreeFunction("example::Add", "int (int, int)");
        const auto* otherAdd =
            registry.FindFreeFunction("example::other::Add", "float (float, float)");
        const auto* write =
            registry.FindFreeFunction("example::WriteValue", "void (float *, float)");

        if (floatAdd == nullptr || intAdd == nullptr ||
            otherAdd == nullptr || write == nullptr)
        {
            return false;
        }

        if (floatAdd->IsStaticMember || floatAdd->IsConstMember ||
            !floatAdd->CanInvoke())
        {
            return false;
        }

        const auto sum = floatAdd->InvokeValueWithoutObject<float>(1.5f, 2.0f);
        const auto integerSum = intAdd->InvokeValueWithoutObject<int>(2, 3);
        const auto otherSum = otherAdd->InvokeValueWithoutObject<float>(1.5f, 2.0f);

        if (!sum || *sum != 3.5f || !integerSum || *integerSum != 5 ||
            !otherSum || *otherSum != 13.5f)
        {
            return false;
        }

        const auto* metadata = floatAdd->Metadata.Find("Category");
        const auto* category = metadata ? std::get_if<std::string>(metadata) : nullptr;
        if (category == nullptr || *category != "Math")
        {
            return false;
        }

        if (floatAdd->InvokeValueWithoutObject<double>(1.5f, 2.0f) ||
            floatAdd->InvokeValueWithoutObject<float>(1, 2) ||
            floatAdd->InvokeWithoutObject(1.5f, 2.0f))
        {
            return false;
        }

        float output = -1.0f;
        if (!write->InvokeWithoutObject(&output, 4.0f) || output != 4.0f)
        {
            return false;
        }

        if (write->InvokeWithoutObject(&output, 5) || output != 4.0f ||
            registry.FindFreeFunction("example::Add", "double (double, double)") ||
            registry.FindFreeFunction("missing::Add", "float (float, float)"))
        {
            return false;
        }

        if (registry.RegisterFreeFunction("example::Add", *floatAdd))
        {
            return false;
        }

        // 자유 함수가 충돌하면 타입 등록을 시작하지 않습니다.
        reflection::Registry conflicting;
        if (!conflicting.RegisterFreeFunction("example::Add", *floatAdd))
        {
            return false;
        }

        if (reflection_generated::Register_Demo(conflicting) ||
            conflicting.FindType("example::Player") != nullptr ||
            conflicting.FindEnum("example::MovementMode") != nullptr)
        {
            return false;
        }

        std::cout << "Free function result: " << *sum << '\n'
            << "Other namespace result: " << *otherSum << '\n'
            << "Free function checks passed.\n";
        return true;
    }

    bool ValidateInvocationDiagnostics()
    {
        using Status = reflection::InvocationBindingStatus;

        reflection::Registry registry;
        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        const auto* type = registry.FindType("diagnostic::Probe");
        if (type == nullptr || type->Functions.size() != 5)
        {
            return false;
        }

        // 이 예제에는 같은 이름의 오버로드가 없습니다.
        const auto findByName = [&](std::string_view name) -> const reflection::FunctionInfo*
            {
                for (const auto& function : type->Functions)
                {
                    if (function.Name == name)
                    {
                        return &function;
                    }
                }
                return nullptr;
            };

        const std::pair<const char*, Status> checks[] = {
            {"Reference", Status::ReferenceParameter},
            {"Safe", Status::ExceptionSpecification},
            {"ReferenceResult", Status::UnsupportedReturnType},
            {"Hidden", Status::InaccessibleMember}
        };

        diagnostic::Probe object;

        for (const auto& [name, expected] : checks)
        {
            const auto* function = findByName(name);
            if (function == nullptr || function->CanInvoke() ||
                function->GetBindingStatus() != expected)
            {
                return false;
            }

            if (function->Invoke(object))
            {
                return false;
            }

            std::cout << "Unavailable: " << function->Name
                << " [" << reflection::ToString(function->GetBindingStatus()) << "]\n";
        }

        const auto* ready = findByName("Ready");
        if (ready == nullptr || !ready->CanInvoke() ||
            ready->GetBindingStatus() != Status::Available || !ready->Invoke(object))
        {
            return false;
        }

        const reflection::FunctionInfo empty;
        if (empty.CanInvoke() || empty.GetBindingStatus() != Status::NotBound)
        {
            return false;
        }

        std::cout << "Invocation diagnostics checks passed.\n";
        return true;
    }
}