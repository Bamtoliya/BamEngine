#include <reflection/Reflection.h>

#include "ExampleTypes.h"
#include "ExampleReflection.h"
#include "ExtraTypes.h"
#include "DiagnosticsTypes.h"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <cstdint>
#include <limits>

namespace
{
    constexpr std::string_view ExpandedMetadata =
        REFLECTION_DETAIL_STRINGIZE(
            NAME("Speed"),
            EDITABLE,
            RANGE(0.0, 100.0)
        );

    static_assert(
        ExpandedMetadata.find("DisplayName") != std::string_view::npos
        );

    static_assert(
        ExpandedMetadata.find("Editable") != std::string_view::npos
        );

    static_assert(
        ExpandedMetadata.find("RangeMin") != std::string_view::npos
        );

    static_assert(
        ExpandedMetadata.find("RangeMax") != std::string_view::npos
        );

    static_assert(
        ExpandedMetadata.find("NAME(") == std::string_view::npos
        );

    static_assert(
        ExpandedMetadata.find("RANGE(") == std::string_view::npos
        );

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

    bool ValidateEnumRegistry()
    {
        reflection::Registry registry;

        reflection::EnumInfo enumeration;
        enumeration.QualifiedName = "example::RuntimeEnum";
        enumeration.UnderlyingType = "long long";
        enumeration.IsScoped = true;

        enumeration.Entries.push_back(
            { "Negative", std::int64_t{-1} }
        );
        enumeration.Entries.push_back(
            { "Zero", std::int64_t{0} }
        );

        // 같은 값에 다른 이름을 붙이는 것은 허용합니다.
        enumeration.Entries.push_back(
            { "Default", std::int64_t{0} }
        );

        if (!enumeration.Metadata.Add(
            "DisplayName",
            std::string{ "Runtime Enum" }))
        {
            return false;
        }

        if (!registry.RegisterEnum(std::move(enumeration)))
        {
            return false;
        }

        const reflection::EnumInfo* info =
            registry.FindEnum("example::RuntimeEnum");

        if (info == nullptr ||
            !info->IsScoped ||
            info->UnderlyingType != "long long" ||
            info->Entries.size() != 3)
        {
            return false;
        }

        const reflection::EnumEntry* negative =
            info->FindEntry("Negative");
        const reflection::EnumEntry* alias =
            info->FindEntry("Default");

        if (negative == nullptr || alias == nullptr)
        {
            return false;
        }

        const auto* negativeValue =
            std::get_if<std::int64_t>(&negative->Value);
        const auto* aliasValue =
            std::get_if<std::int64_t>(&alias->Value);

        if (negativeValue == nullptr || *negativeValue != -1 ||
            aliasValue == nullptr || *aliasValue != 0)
        {
            return false;
        }

        const reflection::MetadataValue* displayName =
            info->Metadata.Find("DisplayName");

        if (displayName == nullptr)
        {
            return false;
        }

        const auto* text = std::get_if<std::string>(displayName);

        if (text == nullptr || *text != "Runtime Enum")
        {
            return false;
        }

        // 동일한 Enum 이름의 재등록을 거부합니다.
        if (registry.RegisterEnum(*info))
        {
            return false;
        }

        // 열거자 이름 중복을 거부합니다.
        reflection::EnumInfo invalid;
        invalid.QualifiedName = "example::InvalidEnum";
        invalid.UnderlyingType = "int";
        invalid.Entries.push_back({ "Value", std::int64_t{0} });
        invalid.Entries.push_back({ "Value", std::int64_t{1} });

        if (registry.RegisterEnum(std::move(invalid)) ||
            registry.FindEnum("example::InvalidEnum") != nullptr)
        {
            return false;
        }

        // signed 범위를 넘는 unsigned 값도 그대로 보관합니다.
        reflection::EnumInfo wide;
        wide.QualifiedName = "example::WideEnum";
        wide.UnderlyingType = "unsigned long long";
        wide.Entries.push_back(
            { "Maximum", std::numeric_limits<std::uint64_t>::max() }
        );

        if (!registry.RegisterEnum(std::move(wide)))
        {
            return false;
        }

        const reflection::EnumInfo* wideInfo =
            registry.FindEnum("example::WideEnum");

        if (wideInfo == nullptr)
        {
            return false;
        }

        const reflection::EnumEntry* maximum =
            wideInfo->FindEntry("Maximum");

        if (maximum == nullptr)
        {
            return false;
        }

        const auto* maximumValue =
            std::get_if<std::uint64_t>(&maximum->Value);

        if (maximumValue == nullptr ||
            *maximumValue != std::numeric_limits<std::uint64_t>::max())
        {
            return false;
        }

        if (info->FindEntry("Missing") != nullptr ||
            registry.FindEnum("example::MissingEnum") != nullptr)
        {
            return false;
        }

        std::cout << "Enum registry checks passed.\n";
        return true;
    }

    bool ValidateGeneratedEnums()
    {
        reflection::Registry registry;

        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        const reflection::EnumInfo* movement =
            registry.FindEnum("example::MovementMode");

        if (movement == nullptr ||
            !movement->IsScoped ||
            movement->UnderlyingType != "int" ||
            movement->Entries.size() != 2)
        {
            return false;
        }

        const reflection::EnumEntry* walk =
            movement->FindEntry("Walk");
        const reflection::EnumEntry* run =
            movement->FindEntry("Run");

        if (walk == nullptr || run == nullptr ||
            walk->Value != reflection::EnumValue{ std::int64_t{0} } ||
            run->Value != reflection::EnumValue{ std::int64_t{1} })
        {
            return false;
        }

        const reflection::MetadataValue* metadata =
            movement->Metadata.Find("DisplayName");

        if (metadata == nullptr)
        {
            return false;
        }

        const auto* displayName = std::get_if<std::string>(metadata);

        if (displayName == nullptr || *displayName != "Movement Mode")
        {
            return false;
        }

        const reflection::EnumInfo* signedInfo =
            registry.FindEnum("example::SignedBoundary");
        const reflection::EnumInfo* unsignedInfo =
            registry.FindEnum("example::UnsignedBoundary");

        if (signedInfo == nullptr || unsignedInfo == nullptr)
        {
            return false;
        }

        const reflection::EnumEntry* minimum =
            signedInfo->FindEntry("Minimum");
        const reflection::EnumEntry* negative =
            signedInfo->FindEntry("Negative");
        const reflection::EnumEntry* alias =
            signedInfo->FindEntry("Alias");
        const reflection::EnumEntry* maximum =
            unsignedInfo->FindEntry("Maximum");

        if (minimum == nullptr || negative == nullptr ||
            alias == nullptr || maximum == nullptr)
        {
            return false;
        }

        if (minimum->Value != reflection::EnumValue{
                std::numeric_limits<std::int64_t>::min() } ||
                negative->Value != reflection::EnumValue{ std::int64_t{-1} } ||
            alias->Value != negative->Value ||
            maximum->Value != reflection::EnumValue{
                std::numeric_limits<std::uint64_t>::max() })
        {
            return false;
        }

        // Enum 이름이 충돌하면 타입 등록을 시작하지 않아야 합니다.
        reflection::Registry conflicting;

        if (!conflicting.RegisterEnum(*movement))
        {
            return false;
        }

        if (reflection_generated::Register_Demo(conflicting) ||
            conflicting.FindType("example::MovementSettings") != nullptr ||
            conflicting.FindType("example::Player") != nullptr)
        {
            return false;
        }

        std::cout
            << "Registered enum: " << movement->QualifiedName << '\n'
            << "Enumerator: Walk = 0, Run = 1\n"
            << "Generated enum checks passed.\n";

        return true;
    }

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

    bool ValidatePropertyDiagnostics()
    {
        using Status = reflection::PropertyAccessStatus;

        reflection::Registry registry;
        if (!reflection_generated::Register_Demo(registry))
        {
            return false;
        }

        const auto* type = registry.FindType("diagnostic::PropertyProbe");
        if (type == nullptr)
        {
            return false;
        }

        const auto check = [&](std::string_view name, Status read, Status write)
            {
                const auto* property = type->FindProperty(name);
                return property != nullptr &&
                    property->GetReadStatus() == read &&
                    property->GetWriteStatus() == write &&
                    property->CanRead() == (read == Status::Available) &&
                    property->CanWrite() == (write == Status::Available);
            };

        if (!check("Value", Status::Available, Status::Available) ||
            !check("Fixed", Status::Available, Status::ConstQualified) ||
            !check("Bits", Status::BitField, Status::BitField) ||
            !check("Alias", Status::ReferenceMember, Status::ReferenceMember) ||
            !check("Signal", Status::VolatileQualified, Status::VolatileQualified) ||
            !check("Samples", Status::Available, Status::NotAssignable))
        {
            return false;
        }

        diagnostic::PropertyProbe object;
        const auto* value = type->FindProperty("Value");
        const auto* fixed = type->FindProperty("Fixed");
        const auto* samples = type->FindProperty("Samples");

        if (!value->Write<float>(object, 8.0f))
        {
            return false;
        }

        const auto* readValue = value->Read<float>(object);
        const auto* readFixed = fixed->Read<int>(object);
        const auto* readSamples = samples->Read<int[2]>(object);

        if (readValue == nullptr || *readValue != 8.0f ||
            readFixed == nullptr || *readFixed != 7 ||
            readSamples == nullptr || (*readSamples)[1] != 4)
        {
            return false;
        }

        if (fixed->Write<int>(object, 9) || object.Fixed != 7)
        {
            return false;
        }

        // 접근을 지원해도 값의 자료형이 다른 호출은 거부합니다.
        if (value->Read<int>(object) != nullptr ||
            value->Write<int>(object, 9) || object.Value != 8.0f)
        {
            return false;
        }

        const auto* bits = type->FindProperty("Bits");
        if (bits->Read<unsigned int>(object) != nullptr ||
            bits->Write<unsigned int>(object, 1u))
        {
            return false;
        }

        reflection::PropertyInfo unbound;
        if (unbound.CanRead() || unbound.CanWrite() ||
            unbound.GetReadStatus() != Status::NotBound ||
            unbound.GetWriteStatus() != Status::NotBound)
        {
            return false;
        }

        std::cout << "Property access diagnostics checks passed.\n";
        return true;
    }
}

int main()
{
    reflection::Metadata metadata;

    if (!metadata.Add("DisplayName", std::string{ "Speed" }))
    {
        return 1;
    }

    if (!metadata.Add("Editable", true))
    {
        return 2;
    }

    if (!metadata.Add("RangeMin", 0.0))
    {
        return 3;
    }

    // 중복 키는 거부되어야 합니다.
    if (metadata.Add("DisplayName", std::string{ "Other" }))
    {
        return 4;
    }

    const reflection::MetadataValue* value =
        metadata.Find("DisplayName");

    if (value == nullptr)
    {
        return 5;
    }

    const std::string* displayName =
        std::get_if<std::string>(value);

    if (displayName == nullptr || *displayName != "Speed")
    {
        return 6;
    }

    // 존재하는 키라도 요청한 값의 자료형이 다르면 실패해야 합니다.
    if (std::get_if<bool>(value) != nullptr)
    {
        return 7;
    }

    if (metadata.Find("Missing") != nullptr)
    {
        return 8;
    }

    std::cout << "DisplayName: " << *displayName << '\n';
    std::cout << "Metadata checks passed.\n";

    std::cout << "Expanded metadata: "
        << ExpandedMetadata << '\n';

    if (!ValidateRegistry())
    {
        std::cerr << "Registry checks failed.\n";
        return 9;
    }

    if (!ValidateEnumRegistry())
    {
        std::cerr << "Enum registry checks failed.\n";
        return 10;
    }

    if (!ValidateGeneratedEnums())
    {
        std::cerr << "Generated enum checks failed.\n";
        return 11;
    }

    if (!ValidateGeneratedFunctions())
    {
        std::cerr << "Generated function checks failed.\n";
        return 12;
    }

    if (!ValidateStaticFunctions())
    {
        std::cerr << "Static invocation checks failed.\n";
        return 13;
    }

    if (!ValidateFreeFunctions())
    {
        std::cerr << "Free function checks failed.\n";
        return 14;
    }

    if (!ValidateMultipleHeaders())
    {
        std::cerr << "Multiple header checks failed.\n";
        return 15;
    }

    if (!ValidateInvocationDiagnostics())
    {
        std::cerr << "Invocation diagnostics checks failed.\n";
        return 16;
    }

    if (!ValidateRegistrationDiagnostics())
    {
        std::cerr << "Registration diagnostics checks failed.\n";
        return 17;
    }

    if (!ValidateModuleDiagnostics())
    {
        std::cerr << "Module diagnostics checks failed.\n";
        return 18;
    }

    if (!ValidateAtomicRegistration())
    {
        std::cerr << "Atomic registration checks failed.\n";
        return 19;
    }

    if (!ValidatePropertyDiagnostics())
    {
        std::cerr << "Property access diagnostics checks failed.\n";
        return 20;
    }

    std::cout << "Registry checks passed.\n";

    return 0;
}