#include <reflection/Reflection.h>
#include "AppTypes.h"
#include "Consumer.gen.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <variant>
#include <utility>
#include <memory>
#include <limits>

int main()
{
    reflection::Registry registry;
    if (!reflection_generated::Register_Consumer(registry))
    {
        std::cerr << "Consumer registration failed.\n";
        return 1;
    }

    const auto* settingsType = registry.FindType("consumer::Settings");
    if (settingsType == nullptr)
    {
        return 2;
    }

    const auto* speed = settingsType->FindProperty("Speed");
    if (speed == nullptr)
    {
        return 3;
    }

    consumer::Settings settings;
    if (!speed->Write<float>(settings, 12.0f) || settings.Speed != 12.0f)
    {
        return 4;
    }

    const auto* unitMetadata = speed->Metadata.Find("Unit");
    const auto* unit = unitMetadata ? std::get_if<std::string>(unitMetadata) : nullptr;
    if (unit == nullptr || *unit != "m/s")
    {
        return 5;
    }

    const auto* meterType = registry.FindType("consumer::Meter");
    if (meterType == nullptr)
    {
        return 6;
    }

    const auto* value = meterType->FindProperty("m_value");
    const auto* add = meterType->FindFunction("Add", "void (float)");
    const auto* getter = meterType->FindFunction("GetValue", "float (void) const");
    if (value == nullptr || add == nullptr || getter == nullptr)
    {
        return 7;
    }

    consumer::Meter meter;
    if (!value->Write<float>(meter, 3.0f) || !add->Invoke(meter, 2.0f))
    {
        return 8;
    }

    const consumer::Meter& readOnly = meter;
    const auto measured = getter->InvokeValue<float>(readOnly);
    if (!measured || *measured != 5.0f || add->Invoke(readOnly, 1.0f))
    {
        return 9;
    }

    const auto* state = registry.FindEnum("consumer::State");
    if (state == nullptr)
    {
        return 10;
    }

    const auto* running = state->FindEntry("Running");
    if (running == nullptr ||
        running->Value != reflection::EnumValue{ std::int64_t{1} })
    {
        return 11;
    }

    const auto* doubleValue =
        registry.FindFreeFunction("consumer::DoubleValue", "float (float)");
    if (doubleValue == nullptr)
    {
        return 12;
    }

    const auto doubled = doubleValue->InvokeValueWithoutObject<float>(*measured);
    if (!doubled || *doubled != 10.0f ||
        doubleValue->InvokeValueWithoutObject<float>(5))
    {
        return 13;
    }

    if (reflection_generated::Register_Consumer(registry))
    {
        return 14;
    }

    const auto* title = settingsType->FindProperty("Title");
    if (title == nullptr || !title->CanRead() || !title->CanWrite())
    {
        return 15;
    }

    if (!title->Write<std::string>(settings, std::string{ "Reflection Ready" }))
    {
        return 16;
    }

    const auto* reflectedTitle = title->Read<std::string>(settings);
    if (reflectedTitle == nullptr || *reflectedTitle != "Reflection Ready" ||
        settings.Title != "Reflection Ready")
    {
        return 17;
    }

    using ValueKind = reflection::PropertyValueKind;

    if (speed->GetValueKind() != ValueKind::FloatingPoint ||
        !speed->IsValueType<float>() || speed->IsValueType<double>())
    {
        return 18;
    }

    if (title->GetValueKind() != ValueKind::String ||
        !title->IsValueType<std::string>() || title->IsValueType<std::wstring>())
    {
        return 19;
    }

    reflection::PropertyInfo unbound;
    if (unbound.GetValueKind() != ValueKind::Unknown ||
        unbound.IsValueType<float>())
    {
        return 20;
    }

    // 정확한 자료형을 모르는 순회 코드에서도 종류에 따라 분기합니다.
    std::size_t floatingProperties = 0;
    std::size_t stringProperties = 0;

    for (const auto& property : settingsType->Properties)
    {
        switch (property.GetValueKind())
        {
        case ValueKind::FloatingPoint:
            ++floatingProperties;
            break;

        case ValueKind::String:
            ++stringProperties;
            break;

        default:
            break;
        }
    }

    if (floatingProperties != 1 || stringProperties != 1)
    {
        return 21;
    }

    std::cout << "Property value kind checks passed.\n";

    const auto view = reflection::ObjectView::From(settings);

    if (!view.IsValid() || view.IsReadOnly() ||
        !view.Is<consumer::Settings>() || view.Is<consumer::Meter>())
    {
        return 22;
    }

    if (!speed->WriteTo<float>(view, 18.0f))
    {
        return 23;
    }

    const auto* viewedSpeed = speed->ReadFrom<float>(view);
    if (viewedSpeed == nullptr || *viewedSpeed != 18.0f ||
        settings.Speed != 18.0f)
    {
        return 24;
    }

    // 값의 자료형이 다른 호출은 거부합니다.
    if (speed->ReadFrom<int>(view) != nullptr ||
        speed->WriteTo<int>(view, 1) || settings.Speed != 18.0f)
    {
        return 25;
    }

    const consumer::Settings& readOnlySettings = settings;
    const auto readOnlyView = reflection::ObjectView::From(readOnlySettings);

    if (!readOnlyView.IsReadOnly() ||
        speed->WriteTo<float>(readOnlyView, 99.0f))
    {
        return 26;
    }

    const auto* readOnlySpeed = speed->ReadFrom<float>(readOnlyView);
    if (readOnlySpeed == nullptr || *readOnlySpeed != 18.0f)
    {
        return 27;
    }

    const auto wrongOwner = reflection::ObjectView::From(meter);
    if (speed->ReadFrom<float>(wrongOwner) != nullptr ||
        speed->WriteTo<float>(wrongOwner, 99.0f))
    {
        return 28;
    }

    const reflection::ObjectView empty;
    if (empty.IsValid() || speed->ReadFrom<float>(empty) != nullptr ||
        speed->WriteTo<float>(empty, 99.0f))
    {
        return 29;
    }

    // 기존 소비자 출력값을 유지합니다.
    if (!speed->WriteTo<float>(view, 12.0f))
    {
        return 30;
    }

    std::cout << "Object view checks passed.\n";

    const auto speedValue = speed->ReadValue(view);
    const auto* speedAddress = speedValue.Get<float>();

    if (!speedValue.IsValid() || !speedValue.Is<float>() ||
        speedValue.Is<int>() || speedValue.Get<int>() != nullptr ||
        speedAddress != &settings.Speed)
    {
        return 31;
    }

    consumer::Settings destination;
    const auto destinationView = reflection::ObjectView::From(destination);

    // 값 타입을 지정하지 않고 다른 객체의 같은 프로퍼티로 복사합니다.
    if (!speed->WriteValue(destinationView, speedValue) ||
        destination.Speed != settings.Speed)
    {
        return 32;
    }

    const auto titleValue = title->ReadValue(view);
    if (!title->WriteValue(destinationView, titleValue) ||
        destination.Title != settings.Title)
    {
        return 33;
    }

    // 문자열 값을 float 프로퍼티에 대입할 수 없습니다.
    const float previousSpeed = destination.Speed;
    if (speed->WriteValue(destinationView, titleValue) ||
        destination.Speed != previousSpeed)
    {
        return 34;
    }

    // const 원본은 읽을 수 있지만 const 대상에는 쓸 수 없습니다.
    const auto readOnlyValue = speed->ReadValue(readOnlyView);
    if (!readOnlyValue.IsValid() ||
        speed->WriteValue(readOnlyView, speedValue))
    {
        return 35;
    }

    if (speed->ReadValue(wrongOwner).IsValid() ||
        speed->WriteValue(wrongOwner, speedValue) ||
        speed->ReadValue(empty).IsValid() ||
        speed->WriteValue(empty, speedValue))
    {
        return 36;
    }

    const reflection::ValueView emptyValue;
    if (emptyValue.Get<float>() != nullptr ||
        speed->WriteValue(destinationView, emptyValue) ||
        unbound.ReadValue(view).IsValid() ||
        unbound.WriteValue(view, speedValue))
    {
        return 37;
    }

    // 외부에서 준비한 값도 정확한 타입이면 대입할 수 있습니다.
    const float replacement = 24.0f;
    const auto replacementValue = reflection::ValueView::From(replacement);

    if (!speed->WriteValue(destinationView, replacementValue) ||
        destination.Speed != replacement)
    {
        return 38;
    }

    // 읽은 뷰는 복사본이 아니라 원본 멤버를 계속 가리킵니다.
    const auto destinationValue = speed->ReadValue(destinationView);
    if (!speed->WriteValue(destinationView, speedValue))
    {
        return 39;
    }

    const auto* updatedValue = destinationValue.Get<float>();
    if (updatedValue == nullptr || *updatedValue != settings.Speed)
    {
        return 40;
    }

    std::cout << "Value view checks passed.\n";

    using AccessError = reflection::PropertyAccessError;

    const auto readResult = speed->TryReadValue(view);
    if (!readResult || readResult.Value.Get<float>() != &settings.Speed)
    {
        return 41;
    }

    const auto wrongRead = speed->TryReadValue(wrongOwner);
    if (wrongRead.Error != AccessError::OwnerTypeMismatch ||
        wrongRead.Value.IsValid())
    {
        return 42;
    }

    if (speed->TryReadValue(empty).Error != AccessError::InvalidObject ||
        unbound.TryReadValue(view).Error != AccessError::ReadUnavailable)
    {
        return 43;
    }

    if (speed->TryWriteValue(view, emptyValue) != AccessError::InvalidValue ||
        speed->TryWriteValue(empty, speedValue) != AccessError::InvalidObject ||
        speed->TryWriteValue(wrongOwner, speedValue) != AccessError::OwnerTypeMismatch)
    {
        return 44;
    }

    if (speed->TryWriteValue(readOnlyView, speedValue) != AccessError::ReadOnlyObject ||
        speed->TryWriteValue(view, titleValue) != AccessError::ValueTypeMismatch ||
        unbound.TryWriteValue(view, speedValue) != AccessError::WriteUnavailable)
    {
        return 45;
    }

    // const 멤버와 const 객체를 구분해서 진단합니다.
    struct ReadOnlySample
    {
        const int Value = 3;
    };

    auto fixedProperty = reflection::MakeProperty<&ReadOnlySample::Value>("Value", "int");
    ReadOnlySample fixedObject;
    const auto fixedView = reflection::ObjectView::From(fixedObject);

    const int newFixedValue = 4;
    const auto fixedSource = reflection::ValueView::From(newFixedValue);

    if (fixedProperty.TryWriteValue(fixedView, fixedSource) != AccessError::WriteUnavailable ||
        fixedProperty.GetWriteStatus() != reflection::PropertyAccessStatus::ConstQualified ||
        fixedObject.Value != 3)
    {
        return 46;
    }

    const auto fixedRead = fixedProperty.TryReadValue(fixedView);
    const auto* fixedAddress = fixedRead.Value.Get<int>();
    if (!fixedRead || fixedAddress == nullptr || *fixedAddress != 3)
    {
        return 47;
    }

    // 실패한 쓰기에서 원본 값이 변경되지 않았는지 확인합니다.
    if (settings.Speed != 12.0f ||
        speed->TryWriteValue(destinationView, speedValue) != AccessError::None ||
        destination.Speed != settings.Speed)
    {
        return 48;
    }

    std::cout << "Property access error checks passed.\n";

    using RegistrationError = reflection::RegistrationError;

    if (registry.FindType<consumer::Settings>() != settingsType ||
        registry.FindType<const consumer::Settings>() != settingsType ||
        registry.FindType<consumer::Meter>() != meterType ||
        registry.FindEnum<consumer::State>() != state ||
        registry.FindEnum<const consumer::State>() != state)
    {
        return 49;
    }

    struct UnregisteredType {};
    enum class UnregisteredEnum { Value };

    if (registry.FindType<UnregisteredType>() != nullptr ||
        registry.FindEnum<UnregisteredEnum>() != nullptr)
    {
        return 50;
    }

    // 이름만 달라도 같은 C++ 타입의 중복 등록은 거부합니다.
    auto duplicateType = *settingsType;
    duplicateType.QualifiedName = "consumer::SettingsAlias";

    if (registry.TryRegister(std::move(duplicateType)).Error !=
        RegistrationError::DuplicateCppType)
    {
        return 51;
    }

    auto duplicateEnum = *state;
    duplicateEnum.QualifiedName = "consumer::StateAlias";

    if (registry.TryRegisterEnum(std::move(duplicateEnum)).Error !=
        RegistrationError::DuplicateCppEnum)
    {
        return 52;
    }

    // 병합 충돌에서도 고유 항목이 일부 이동하면 안 됩니다.
    reflection::Registry conflictingRegistry;

    auto conflictingType = *settingsType;
    conflictingType.QualifiedName = "consumer::MergeAlias";

    reflection::TypeInfo uniqueType;
    uniqueType.QualifiedName = "consumer::Uncommitted";

    if (!conflictingRegistry.TryRegister(std::move(conflictingType)) ||
        !conflictingRegistry.TryRegister(std::move(uniqueType)))
    {
        return 53;
    }

    const auto mergeResult = registry.TryMerge(std::move(conflictingRegistry));
    if (mergeResult.Error != RegistrationError::DuplicateCppType ||
        registry.FindType("consumer::Uncommitted") != nullptr ||
        conflictingRegistry.FindType("consumer::Uncommitted") == nullptr ||
        conflictingRegistry.FindType<consumer::Settings>() == nullptr)
    {
        return 54;
    }

    // enum 연결도 병합 충돌을 검사합니다.
    reflection::Registry conflictingEnums;
    auto conflictingEnum = *state;
    conflictingEnum.QualifiedName = "consumer::MergeStateAlias";

    if (!conflictingEnums.TryRegisterEnum(std::move(conflictingEnum)) ||
        registry.TryMerge(std::move(conflictingEnums)).Error !=
        RegistrationError::DuplicateCppEnum ||
        conflictingEnums.FindEnum<consumer::State>() == nullptr)
    {
        return 55;
    }

    if (registry.FindType<consumer::Settings>() != settingsType ||
        registry.FindEnum<consumer::State>() != state)
    {
        return 56;
    }

    std::cout << "C++ type registry checks passed.\n";

    if (registry.FindType(view) != settingsType ||
        registry.FindType(readOnlyView) != settingsType ||
        registry.FindType(wrongOwner) != meterType)
    {
        return 57;
    }

    consumer::Snapshot snapshot;
    snapshot.Configuration.Speed = 7.0f;
    snapshot.Current = consumer::State::Running;
    snapshot.Target = &settings;

    const auto snapshotView = reflection::ObjectView::From(snapshot);
    const auto* snapshotType = registry.FindType(snapshotView);

    if (snapshotType == nullptr ||
        snapshotType->QualifiedName != "consumer::Snapshot")
    {
        return 58;
    }

    const auto* configurationProperty = snapshotType->FindProperty("Configuration");
    const auto* stateProperty = snapshotType->FindProperty("Current");
    const auto* targetProperty = snapshotType->FindProperty("Target");

    if (configurationProperty == nullptr || stateProperty == nullptr ||
        targetProperty == nullptr)
    {
        return 59;
    }

    const auto configurationValue = configurationProperty->ReadValue(snapshotView);
    const auto stateValue = stateProperty->ReadValue(snapshotView);
    const auto targetValue = targetProperty->ReadValue(snapshotView);

    if (!configurationValue.IsValid() || !stateValue.IsValid() ||
        !targetValue.IsValid())
    {
        return 60;
    }

    // 구체적인 타입을 지정하지 않고 프로퍼티 값에서 등록 정보를 찾습니다.
    if (registry.FindType(configurationValue) != settingsType ||
        registry.FindEnum(stateValue) != state)
    {
        return 61;
    }

    // enum과 클래스의 등록 정보를 혼동하지 않습니다.
    if (registry.FindEnum(configurationValue) != nullptr ||
        registry.FindType(stateValue) != nullptr)
    {
        return 62;
    }

    // 포인터 값에서 참조 대상의 타입을 자동 조회하지 않습니다.
    if (!targetValue.Is<consumer::Settings*>() ||
        registry.FindType(targetValue) != nullptr)
    {
        return 63;
    }

    if (registry.FindType(empty) != nullptr ||
        registry.FindType(emptyValue) != nullptr ||
        registry.FindEnum(emptyValue) != nullptr)
    {
        return 64;
    }

    // 유효한 값이어도 해당 타입이 미등록이면 nullptr입니다.
    const auto stringValue = title->ReadValue(view);
    if (!stringValue.IsValid() || registry.FindType(stringValue) != nullptr)
    {
        return 65;
    }

    std::cout << "View type lookup checks passed.\n";

    const auto* nestedType = registry.FindType(configurationValue);
    if (nestedType == nullptr)
    {
        return 66;
    }

    const auto nestedObject = nestedType->AsObject(configurationValue);
    if (!nestedObject.IsValid() || !nestedObject.IsReadOnly() ||
        registry.FindType(nestedObject) != settingsType)
    {
        return 67;
    }

    const auto* nestedSpeed = nestedType->FindProperty("Speed");
    const auto* nestedTitle = nestedType->FindProperty("Title");
    if (nestedSpeed == nullptr || nestedTitle == nullptr)
    {
        return 68;
    }

    const auto nestedSpeedResult = nestedSpeed->TryReadValue(nestedObject);
    const auto* nestedSpeedAddress = nestedSpeedResult.Value.Get<float>();

    if (!nestedSpeedResult ||
        nestedSpeedAddress != &snapshot.Configuration.Speed ||
        *nestedSpeedAddress != 7.0f)
    {
        return 69;
    }

    const auto nestedTitleValue = nestedTitle->ReadValue(nestedObject);
    const auto* nestedTitleAddress = nestedTitleValue.Get<std::string>();
    if (nestedTitleAddress == nullptr || *nestedTitleAddress != "Default")
    {
        return 70;
    }

    // 읽기 전용으로 연결한 중첩 객체에는 쓸 수 없습니다.
    if (nestedSpeed->TryWriteValue(nestedObject, replacementValue) !=
        reflection::PropertyAccessError::ReadOnlyObject ||
        snapshot.Configuration.Speed != 7.0f)
    {
        return 71;
    }

    // 다른 타입의 값을 해당 객체로 해석할 수 없습니다.
    if (settingsType->AsObject(stateValue).IsValid() ||
        settingsType->AsObject(targetValue).IsValid() ||
        settingsType->AsObject(emptyValue).IsValid() ||
        meterType->AsObject(configurationValue).IsValid())
    {
        return 72;
    }

    reflection::TypeInfo unlinkedType;
    if (unlinkedType.AsObject(configurationValue).IsValid())
    {
        return 73;
    }

    // 내부 프로퍼티도 소유 클래스 타입 없이 순회할 수 있습니다.
    std::size_t readableNestedProperties = 0;
    for (const auto& property : nestedType->Properties)
    {
        if (property.TryReadValue(nestedObject))
        {
            ++readableNestedProperties;
        }
    }

    if (readableNestedProperties != 2)
    {
        return 74;
    }

    // 뷰는 복사본이 아니라 원본 중첩 객체를 가리킵니다.
    snapshot.Configuration.Speed = 9.0f;
    if (*nestedSpeedAddress != 9.0f)
    {
        return 75;
    }

    snapshot.Configuration.Speed = 7.0f;
    std::cout << "Nested object read checks passed.\n";

    const auto editResult = configurationProperty->TryEditObject(snapshotView);
    if (!configurationProperty->CanEditObject() || !editResult ||
        editResult.Object.IsReadOnly() ||
        registry.FindType(editResult.Object) != settingsType)
    {
        return 76;
    }

    if (nestedSpeed->TryWriteValue(editResult.Object, replacementValue) !=
        reflection::PropertyAccessError::None ||
        snapshot.Configuration.Speed != replacement)
    {
        return 77;
    }

    const consumer::Snapshot& constSnapshot = snapshot;
    const auto constSnapshotView = reflection::ObjectView::From(constSnapshot);

    if (configurationProperty->TryEditObject(constSnapshotView).Error !=
        reflection::PropertyAccessError::ReadOnlyObject)
    {
        return 78;
    }

    const auto* frozenProperty = snapshotType->FindProperty("FrozenConfiguration");
    if (frozenProperty == nullptr || frozenProperty->CanEditObject() ||
        frozenProperty->TryEditObject(snapshotView).Error !=
        reflection::PropertyAccessError::ReadOnlyProperty)
    {
        return 79;
    }

    // const 멤버도 기존 읽기 경로로는 접근할 수 있습니다.
    const auto frozenValue = frozenProperty->ReadValue(snapshotView);
    const auto frozenObject = settingsType->AsObject(frozenValue);

    const auto frozenSpeed = nestedSpeed->TryReadValue(frozenObject);
    const auto* frozenSpeedAddress = frozenSpeed.Value.Get<float>();
    if (!frozenObject.IsReadOnly() || !frozenSpeed ||
        frozenSpeedAddress == nullptr || *frozenSpeedAddress != 1.0f)
    {
        return 80;
    }

    if (configurationProperty->TryEditObject(empty).Error !=
        reflection::PropertyAccessError::InvalidObject ||
        configurationProperty->TryEditObject(wrongOwner).Error !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        unbound.TryEditObject(snapshotView).Error !=
        reflection::PropertyAccessError::ReadUnavailable)
    {
        return 81;
    }

    if (targetProperty->TryEditObject(snapshotView).Error !=
        reflection::PropertyAccessError::ObjectUnavailable ||
        stateProperty->TryEditObject(snapshotView).Error !=
        reflection::PropertyAccessError::ObjectUnavailable ||
        nestedSpeed->TryEditObject(editResult.Object).Error !=
        reflection::PropertyAccessError::ObjectUnavailable)
    {
        return 82;
    }

    // 수정 경로를 추가해도 기존 읽기 전용 경로는 유지됩니다.
    if (!nestedObject.IsReadOnly() ||
        nestedSpeed->TryWriteValue(nestedObject, speedValue) !=
        reflection::PropertyAccessError::ReadOnlyObject)
    {
        return 83;
    }

    // 전체 대입이 불가능한 타입도 내부의 변경 가능한 멤버는 수정할 수 있습니다.
    struct NonAssignable
    {
        int Number = 1;
        NonAssignable& operator=(const NonAssignable&) = delete;
    };

    struct NonAssignableOwner
    {
        NonAssignable Child;
    };

    const auto childProperty =
        reflection::MakeProperty<&NonAssignableOwner::Child>("Child", "NonAssignable");
    const auto numberProperty =
        reflection::MakeProperty<&NonAssignable::Number>("Number", "int");

    NonAssignableOwner owner;
    const auto ownerView = reflection::ObjectView::From(owner);
    const auto childResult = childProperty.TryEditObject(ownerView);

    const int number = 5;
    const auto numberValue = reflection::ValueView::From(number);

    if (childProperty.CanWrite() || !childProperty.CanEditObject() || !childResult ||
        numberProperty.TryWriteValue(childResult.Object, numberValue) !=
        reflection::PropertyAccessError::None ||
        owner.Child.Number != 5)
    {
        return 84;
    }

    snapshot.Configuration.Speed = 7.0f;
    std::cout << "Nested object edit checks passed.\n";

    consumer::Collection collection;
    const auto collectionView = reflection::ObjectView::From(collection);
    const auto* collectionType = registry.FindType(collectionView);

    if (collectionType == nullptr)
    {
        return 85;
    }

    const auto* samplesProperty = collectionType->FindProperty("Samples");
    const auto* itemsProperty = collectionType->FindProperty("Items");
    const auto* frozenVectorProperty = collectionType->FindProperty("Frozen");
    const auto* emptyVectorProperty = collectionType->FindProperty("Empty");
    const auto* flagsProperty = collectionType->FindProperty("Flags");

    if (samplesProperty == nullptr || itemsProperty == nullptr ||
        frozenVectorProperty == nullptr || emptyVectorProperty == nullptr ||
        flagsProperty == nullptr)
    {
        return 86;
    }

    const auto samplesSize = samplesProperty->TryGetSize(collectionView);
    const auto sample = samplesProperty->TryReadElement(collectionView, 1);
    const auto* sampleAddress = sample.Value.Get<float>();

    if (samplesProperty->GetValueKind() != reflection::PropertyValueKind::Vector ||
        !samplesProperty->IsContainer() || !samplesProperty->CanReadElement() ||
        !samplesSize || samplesSize.Size != 2 || !sample ||
        sampleAddress != &collection.Samples[1] || *sampleAddress != 2.0f)
    {
        return 87;
    }

    const auto emptySize = emptyVectorProperty->TryGetSize(collectionView);
    if (!emptySize || emptySize.Size != 0 ||
        emptyVectorProperty->TryReadElement(collectionView, 0).Error !=
        reflection::PropertyAccessError::IndexOutOfRange ||
        samplesProperty->TryReadElement(collectionView, 2).Error !=
        reflection::PropertyAccessError::IndexOutOfRange)
    {
        return 88;
    }

    const consumer::Collection& constCollection = collection;
    const auto constCollectionView = reflection::ObjectView::From(constCollection);
    const auto frozenElement = frozenVectorProperty->TryReadElement(constCollectionView, 0);
    const auto* frozenElementAddress = frozenElement.Value.Get<int>();

    if (!frozenElement || frozenElementAddress == nullptr ||
        *frozenElementAddress != 3 ||
        !samplesProperty->TryReadElement(constCollectionView, 0))
    {
        return 89;
    }

    // 객체 요소의 등록 정보를 찾아 내부 프로퍼티를 읽습니다.
    const auto item = itemsProperty->TryReadElement(collectionView, 0);
    const auto* itemType = registry.FindType(item.Value);

    if (!item || itemType != settingsType)
    {
        return 90;
    }

    const auto itemObject = itemType->AsObject(item.Value);
    const auto itemSpeed = nestedSpeed->TryReadValue(itemObject);
    const auto* itemSpeedAddress = itemSpeed.Value.Get<float>();

    if (!itemObject.IsReadOnly() || !itemSpeed ||
        itemSpeedAddress != &collection.Items[0].Speed ||
        *itemSpeedAddress != 1.0f)
    {
        return 91;
    }

    const auto flagsSize = flagsProperty->TryGetSize(collectionView);
    if (!flagsProperty->IsContainer() || flagsProperty->CanReadElement() ||
        !flagsSize || flagsSize.Size != 1 ||
        flagsProperty->TryReadElement(collectionView, 0).Error !=
        reflection::PropertyAccessError::ElementUnavailable)
    {
        return 92;
    }

    if (samplesProperty->TryGetSize(empty).Error !=
        reflection::PropertyAccessError::InvalidObject ||
        samplesProperty->TryReadElement(wrongOwner, 0).Error !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        speed->TryGetSize(view).Error !=
        reflection::PropertyAccessError::NotContainer ||
        speed->TryReadElement(view, 0).Error !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryGetSize(collectionView).Error !=
        reflection::PropertyAccessError::ReadUnavailable)
    {
        return 93;
    }

    std::cout << "Vector read checks passed.\n";

    if (!samplesProperty->CanWriteElement() ||
        !samplesProperty->IsElementType<float>() ||
        samplesProperty->IsElementType<double>() ||
        frozenVectorProperty->CanWriteElement() ||
        flagsProperty->CanWriteElement())
    {
        return 94;
    }

    if (samplesProperty->TryWriteElement(collectionView, 1, replacementValue) !=
        reflection::PropertyAccessError::None ||
        collection.Samples[1] != replacement ||
        *sampleAddress != replacement)
    {
        return 95;
    }

    // 객체 요소도 실제 타입이 같으면 전체 대입할 수 있습니다.
    const auto settingsValue = reflection::ValueView::From(settings);
    if (itemsProperty->TryWriteElement(collectionView, 0, settingsValue) !=
        reflection::PropertyAccessError::None ||
        collection.Items[0].Speed != settings.Speed ||
        collection.Items[0].Title != settings.Title ||
        *itemSpeedAddress != settings.Speed)
    {
        return 96;
    }

    const float beforeFailure = collection.Samples[1];
    if (samplesProperty->TryWriteElement(collectionView, 1, titleValue) !=
        reflection::PropertyAccessError::ValueTypeMismatch ||
        collection.Samples[1] != beforeFailure)
    {
        return 97;
    }

    if (samplesProperty->TryWriteElement(constCollectionView, 1, replacementValue) !=
        reflection::PropertyAccessError::ReadOnlyObject ||
        frozenVectorProperty->TryWriteElement(collectionView, 0, numberValue) !=
        reflection::PropertyAccessError::ReadOnlyProperty ||
        collection.Samples[1] != beforeFailure || collection.Frozen[0] != 3)
    {
        return 98;
    }

    if (samplesProperty->TryWriteElement(collectionView, 2, replacementValue) !=
        reflection::PropertyAccessError::IndexOutOfRange ||
        emptyVectorProperty->TryWriteElement(collectionView, 0, numberValue) !=
        reflection::PropertyAccessError::IndexOutOfRange ||
        collection.Samples.size() != 2 || !collection.Empty.empty())
    {
        return 99;
    }

    const bool flag = false;
    const auto flagValue = reflection::ValueView::From(flag);

    if (flagsProperty->TryWriteElement(collectionView, 0, flagValue) !=
        reflection::PropertyAccessError::ElementWriteUnavailable ||
        !collection.Flags[0])
    {
        return 100;
    }

    if (samplesProperty->TryWriteElement(empty, 0, replacementValue) !=
        reflection::PropertyAccessError::InvalidObject ||
        samplesProperty->TryWriteElement(wrongOwner, 0, replacementValue) !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        samplesProperty->TryWriteElement(collectionView, 0, emptyValue) !=
        reflection::PropertyAccessError::InvalidValue ||
        speed->TryWriteElement(view, 0, replacementValue) !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryWriteElement(collectionView, 0, replacementValue) !=
        reflection::PropertyAccessError::ReadUnavailable)
    {
        return 101;
    }

    std::cout << "Vector element write checks passed.\n";

    const auto elementObject =
        itemsProperty->TryEditElementObject(collectionView, 0);

    if (!itemsProperty->CanEditElementObject() || !elementObject ||
        elementObject.Object.IsReadOnly() ||
        registry.FindType(elementObject.Object) != settingsType)
    {
        return 102;
    }

    const auto previousTitle = collection.Items[0].Title;
    if (nestedSpeed->TryWriteValue(elementObject.Object, replacementValue) !=
        reflection::PropertyAccessError::None ||
        collection.Items[0].Speed != replacement ||
        collection.Items[0].Title != previousTitle ||
        *itemSpeedAddress != replacement)
    {
        return 103;
    }

    const auto* frozenItemsProperty = collectionType->FindProperty("FrozenItems");
    if (frozenItemsProperty == nullptr ||
        frozenItemsProperty->CanEditElementObject() ||
        frozenItemsProperty->TryEditElementObject(collectionView, 0).Error !=
        reflection::PropertyAccessError::ReadOnlyProperty ||
        itemsProperty->TryEditElementObject(constCollectionView, 0).Error !=
        reflection::PropertyAccessError::ReadOnlyObject)
    {
        return 104;
    }

    if (itemsProperty->TryEditElementObject(collectionView, 1).Error !=
        reflection::PropertyAccessError::IndexOutOfRange ||
        itemsProperty->TryEditElementObject(empty, 0).Error !=
        reflection::PropertyAccessError::InvalidObject ||
        itemsProperty->TryEditElementObject(wrongOwner, 0).Error !=
        reflection::PropertyAccessError::OwnerTypeMismatch)
    {
        return 105;
    }

    if (samplesProperty->TryEditElementObject(collectionView, 0).Error !=
        reflection::PropertyAccessError::ObjectUnavailable ||
        flagsProperty->TryEditElementObject(collectionView, 0).Error !=
        reflection::PropertyAccessError::ObjectUnavailable ||
        speed->TryEditElementObject(view, 0).Error !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryEditElementObject(collectionView, 0).Error !=
        reflection::PropertyAccessError::ReadUnavailable)
    {
        return 106;
    }

    // 앞에서 선언한 NonAssignable 타입을 요소로 사용합니다.
    struct NonAssignableCollection
    {
        std::vector<NonAssignable> Items = std::vector<NonAssignable>(1);
    };

    const auto nonAssignableItems =
        reflection::MakeProperty<&NonAssignableCollection::Items>(
            "Items", "std::vector<NonAssignable>");

    NonAssignableCollection nonAssignableCollection;
    const auto nonAssignableView =
        reflection::ObjectView::From(nonAssignableCollection);
    const auto nonAssignableElement =
        nonAssignableItems.TryEditElementObject(nonAssignableView, 0);

    if (nonAssignableItems.CanWrite() ||
        nonAssignableItems.CanWriteElement() ||
        !nonAssignableItems.CanEditElementObject() || !nonAssignableElement)
    {
        return 107;
    }

    if (numberProperty.TryWriteValue(nonAssignableElement.Object, numberValue) !=
        reflection::PropertyAccessError::None ||
        nonAssignableCollection.Items[0].Number != number)
    {
        return 108;
    }

    // 기존 읽기 경로로 얻은 객체 뷰는 여전히 읽기 전용입니다.
    if (!itemObject.IsReadOnly() ||
        nestedSpeed->TryWriteValue(itemObject, replacementValue) !=
        reflection::PropertyAccessError::ReadOnlyObject)
    {
        return 109;
    }

    std::cout << "Vector element object edit checks passed.\n";

    consumer::Collection resizeCollection;
    const auto resizeView = reflection::ObjectView::From(resizeCollection);

    if (!samplesProperty->CanResize() || !itemsProperty->CanResize() ||
        !flagsProperty->CanResize() || frozenVectorProperty->CanResize())
    {
        return 110;
    }

    if (samplesProperty->TryResize(resizeView, 4) !=
        reflection::PropertyAccessError::None ||
        resizeCollection.Samples.size() != 4 ||
        resizeCollection.Samples[0] != 1.0f ||
        resizeCollection.Samples[1] != 2.0f ||
        resizeCollection.Samples[2] != 0.0f ||
        resizeCollection.Samples[3] != 0.0f)
    {
        return 111;
    }

    if (itemsProperty->TryResize(resizeView, 2) !=
        reflection::PropertyAccessError::None ||
        resizeCollection.Items.size() != 2 ||
        resizeCollection.Items[1].Speed != 1.0f ||
        resizeCollection.Items[1].Title != "Default")
    {
        return 112;
    }

    // 크기 변경 후 요소 뷰를 새로 얻습니다.
    const auto resizedItem = itemsProperty->TryEditElementObject(resizeView, 1);
    if (!resizedItem ||
        nestedSpeed->TryWriteValue(resizedItem.Object, replacementValue) !=
        reflection::PropertyAccessError::None ||
        resizeCollection.Items[1].Speed != replacement)
    {
        return 113;
    }

    if (samplesProperty->TryResize(resizeView, 1) !=
        reflection::PropertyAccessError::None ||
        resizeCollection.Samples.size() != 1 ||
        resizeCollection.Samples[0] != 1.0f ||
        samplesProperty->TryReadElement(resizeView, 1).Error !=
        reflection::PropertyAccessError::IndexOutOfRange)
    {
        return 114;
    }

    if (samplesProperty->TryResize(resizeView, 0) !=
        reflection::PropertyAccessError::None ||
        !resizeCollection.Samples.empty() ||
        flagsProperty->TryResize(resizeView, 3) !=
        reflection::PropertyAccessError::None ||
        resizeCollection.Flags.size() != 3 ||
        !resizeCollection.Flags[0] ||
        resizeCollection.Flags[1] || resizeCollection.Flags[2])
    {
        return 115;
    }

    const auto readOnlyResizeView =
        reflection::ObjectView::From(
            static_cast<const consumer::Collection&>(resizeCollection));

    if (samplesProperty->TryResize(readOnlyResizeView, 2) !=
        reflection::PropertyAccessError::ReadOnlyObject ||
        frozenVectorProperty->TryResize(resizeView, 2) !=
        reflection::PropertyAccessError::ReadOnlyProperty ||
        samplesProperty->TryResize(empty, 2) !=
        reflection::PropertyAccessError::InvalidObject ||
        samplesProperty->TryResize(wrongOwner, 2) !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        speed->TryResize(view, 2) !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryResize(resizeView, 2) !=
        reflection::PropertyAccessError::ReadUnavailable ||
        !resizeCollection.Samples.empty())
    {
        return 116;
    }

    struct NoDefaultElement
    {
        explicit NoDefaultElement(int value) : Number(value) {}
        int Number;
    };

    struct NoDefaultCollection
    {
        std::vector<NoDefaultElement> Items;
    };

    const auto noDefaultProperty =
        reflection::MakeProperty<&NoDefaultCollection::Items>(
            "Items", "std::vector<NoDefaultElement>");

    NoDefaultCollection noDefaultCollection;
    const auto noDefaultView = reflection::ObjectView::From(noDefaultCollection);

    if (noDefaultProperty.CanResize() ||
        noDefaultProperty.TryResize(noDefaultView, 1) !=
        reflection::PropertyAccessError::ResizeUnavailable ||
        !noDefaultCollection.Items.empty())
    {
        return 117;
    }

    const auto maximumSize = resizeCollection.Samples.max_size();
    if (maximumSize < static_cast<std::size_t>(-1))
    {
        if (samplesProperty->TryResize(resizeView, maximumSize + 1) !=
            reflection::PropertyAccessError::SizeOutOfRange ||
            !resizeCollection.Samples.empty())
        {
            return 118;
        }
    }

    std::cout << "Vector resize checks passed.\n";

    consumer::Collection clearCollection;
    const auto clearView = reflection::ObjectView::From(clearCollection);

    if (!samplesProperty->CanClear() || !itemsProperty->CanClear() ||
        !flagsProperty->CanClear() || frozenVectorProperty->CanClear())
    {
        return 119;
    }

    if (samplesProperty->TryClear(clearView) !=
        reflection::PropertyAccessError::None ||
        itemsProperty->TryClear(clearView) !=
        reflection::PropertyAccessError::None ||
        flagsProperty->TryClear(clearView) !=
        reflection::PropertyAccessError::None ||
        !clearCollection.Samples.empty() ||
        !clearCollection.Items.empty() ||
        !clearCollection.Flags.empty())
    {
        return 120;
    }

    // 빈 벡터를 다시 비워도 성공합니다.
    if (samplesProperty->TryClear(clearView) !=
        reflection::PropertyAccessError::None ||
        samplesProperty->TryReadElement(clearView, 0).Error !=
        reflection::PropertyAccessError::IndexOutOfRange)
    {
        return 121;
    }

    // 앞 단계의 기본 생성 불가능한 요소도 제거할 수 있습니다.
    noDefaultCollection.Items.emplace_back(7);

    if (!noDefaultProperty.CanClear() ||
        noDefaultProperty.TryClear(noDefaultView) !=
        reflection::PropertyAccessError::None ||
        !noDefaultCollection.Items.empty())
    {
        return 122;
    }

    consumer::Collection protectedCollection;
    const auto protectedView = reflection::ObjectView::From(protectedCollection);
    const auto protectedReadOnlyView = reflection::ObjectView::From(
        static_cast<const consumer::Collection&>(protectedCollection));

    if (samplesProperty->TryClear(protectedReadOnlyView) !=
        reflection::PropertyAccessError::ReadOnlyObject ||
        frozenVectorProperty->TryClear(protectedView) !=
        reflection::PropertyAccessError::ReadOnlyProperty ||
        samplesProperty->TryClear(empty) !=
        reflection::PropertyAccessError::InvalidObject ||
        samplesProperty->TryClear(wrongOwner) !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        speed->TryClear(view) !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryClear(protectedView) !=
        reflection::PropertyAccessError::ReadUnavailable ||
        protectedCollection.Samples != std::vector<float>{ 1.0f, 2.0f } ||
        protectedCollection.Frozen != std::vector<int>{ 3, 4 })
    {
        return 123;
    }

    std::cout << "Vector clear checks passed.\n";

    consumer::Collection appendCollection;
    const auto appendView = reflection::ObjectView::From(appendCollection);

    if (!samplesProperty->CanAppend() || !itemsProperty->CanAppend() ||
        !flagsProperty->CanAppend() || frozenVectorProperty->CanAppend())
    {
        return 124;
    }

    if (samplesProperty->TryAppend(appendView, replacementValue) !=
        reflection::PropertyAccessError::None ||
        appendCollection.Samples.size() != 3 ||
        appendCollection.Samples.back() != replacement ||
        appendCollection.Samples[0] != 1.0f ||
        appendCollection.Samples[1] != 2.0f)
    {
        return 125;
    }

    if (itemsProperty->TryAppend(appendView, settingsValue) !=
        reflection::PropertyAccessError::None ||
        appendCollection.Items.size() != 2 ||
        appendCollection.Items.back().Speed != settings.Speed ||
        appendCollection.Items.back().Title != settings.Title ||
        flagsProperty->TryAppend(appendView, flagValue) !=
        reflection::PropertyAccessError::None ||
        appendCollection.Flags.size() != 2 ||
        appendCollection.Flags.back() != flag)
    {
        return 126;
    }

    // 기본 생성이 불가능한 요소도 값이 있으면 추가할 수 있습니다.
    const NoDefaultElement suppliedElement{ 9 };
    const auto suppliedValue = reflection::ValueView::From(suppliedElement);

    if (!noDefaultProperty.CanAppend() ||
        noDefaultProperty.TryAppend(noDefaultView, suppliedValue) !=
        reflection::PropertyAccessError::None ||
        noDefaultCollection.Items.size() != 1 ||
        noDefaultCollection.Items[0].Number != 9)
    {
        return 127;
    }

    const auto beforeAppendFailure = appendCollection.Samples;
    const auto readOnlyAppendView = reflection::ObjectView::From(
        static_cast<const consumer::Collection&>(appendCollection));

    if (samplesProperty->TryAppend(appendView, titleValue) !=
        reflection::PropertyAccessError::ValueTypeMismatch ||
        samplesProperty->TryAppend(appendView, {}) !=
        reflection::PropertyAccessError::InvalidValue ||
        samplesProperty->TryAppend(readOnlyAppendView, replacementValue) !=
        reflection::PropertyAccessError::ReadOnlyObject ||
        frozenVectorProperty->TryAppend(appendView, numberValue) !=
        reflection::PropertyAccessError::ReadOnlyProperty ||
        samplesProperty->TryAppend(empty, replacementValue) !=
        reflection::PropertyAccessError::InvalidObject ||
        samplesProperty->TryAppend(wrongOwner, replacementValue) !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        speed->TryAppend(view, replacementValue) !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryAppend(appendView, replacementValue) !=
        reflection::PropertyAccessError::ReadUnavailable ||
        appendCollection.Samples != beforeAppendFailure)
    {
        return 128;
    }

    // 읽기 전용 ValueView에서 이동 전용 값을 소비하지 않습니다.
    struct MoveOnlyCollection
    {
        std::vector<std::unique_ptr<int>> Items;
    };

    const auto moveOnlyProperty =
        reflection::MakeProperty<&MoveOnlyCollection::Items>(
            "Items", "std::vector<std::unique_ptr<int>>");

    MoveOnlyCollection moveOnlyCollection;
    const auto moveOnlyView = reflection::ObjectView::From(moveOnlyCollection);
    const auto ownedNumber = std::make_unique<int>(11);
    const auto ownedNumberValue = reflection::ValueView::From(ownedNumber);

    if (moveOnlyProperty.CanAppend() ||
        moveOnlyProperty.TryAppend(moveOnlyView, ownedNumberValue) !=
        reflection::PropertyAccessError::AppendUnavailable ||
        !moveOnlyCollection.Items.empty() || !ownedNumber ||
        *ownedNumber != 11)
    {
        return 129;
    }

    // 중첩 벡터의 복사 불가능한 내부 요소도 검사합니다.
    struct NestedMoveOnlyCollection
    {
        std::vector<std::vector<std::unique_ptr<int>>> Items;
    };

    const auto nestedMoveOnlyProperty =
        reflection::MakeProperty<&NestedMoveOnlyCollection::Items>(
            "Items", "std::vector<std::vector<std::unique_ptr<int>>>");

    if (nestedMoveOnlyProperty.CanWrite() ||
        nestedMoveOnlyProperty.CanWriteElement() ||
        nestedMoveOnlyProperty.CanAppend())
    {
        return 130;
    }

    std::cout << "Vector append checks passed.\n";

    consumer::Collection insertCollection;
    const auto insertView = reflection::ObjectView::From(insertCollection);

    if (!samplesProperty->CanInsert() || !itemsProperty->CanInsert() ||
        !flagsProperty->CanInsert() || frozenVectorProperty->CanInsert())
    {
        return 131;
    }

    // 앞, 중간, 끝에 삽입합니다.
    if (samplesProperty->TryInsert(insertView, 0, replacementValue) !=
        reflection::PropertyAccessError::None ||
        samplesProperty->TryInsert(insertView, 2, replacementValue) !=
        reflection::PropertyAccessError::None ||
        samplesProperty->TryInsert(insertView, 4, replacementValue) !=
        reflection::PropertyAccessError::None ||
        insertCollection.Samples !=
        std::vector<float>{ replacement, 1.0f, replacement, 2.0f, replacement })
    {
        return 132;
    }

    if (emptyVectorProperty->TryInsert(insertView, 0, numberValue) !=
        reflection::PropertyAccessError::None ||
        insertCollection.Empty != std::vector<int>{ number } ||
        flagsProperty->TryInsert(insertView, 0, flagValue) !=
        reflection::PropertyAccessError::None ||
        insertCollection.Flags.size() != 2 ||
        insertCollection.Flags[0] != flag ||
        !insertCollection.Flags[1])
    {
        return 133;
    }

    // 같은 벡터 안의 객체를 입력으로 사용합니다.
    insertCollection.Items[0].Speed = 42.0f;
    insertCollection.Items[0].Title = "Internal source";

    const auto internalSource = itemsProperty->TryReadElement(insertView, 0);
    if (!internalSource ||
        itemsProperty->TryInsert(insertView, 0, internalSource.Value) !=
        reflection::PropertyAccessError::None ||
        insertCollection.Items.size() != 2 ||
        insertCollection.Items[0].Speed != 42.0f ||
        insertCollection.Items[1].Speed != 42.0f ||
        insertCollection.Items[0].Title != "Internal source" ||
        insertCollection.Items[1].Title != "Internal source")
    {
        return 134;
    }

    // 삽입 후 internalSource는 다시 사용하지 않습니다.
    if (!noDefaultProperty.CanInsert() ||
        noDefaultProperty.TryInsert(noDefaultView, 0, suppliedValue) !=
        reflection::PropertyAccessError::None ||
        noDefaultCollection.Items.size() != 2 ||
        noDefaultCollection.Items[0].Number != 9 ||
        noDefaultCollection.Items[1].Number != 9)
    {
        return 135;
    }

    const auto beforeInsertFailure = insertCollection.Samples;
    const auto readOnlyInsertView = reflection::ObjectView::From(
        static_cast<const consumer::Collection&>(insertCollection));

    if (samplesProperty->TryInsert(
        insertView, insertCollection.Samples.size() + 1, replacementValue) !=
        reflection::PropertyAccessError::IndexOutOfRange ||
        samplesProperty->TryInsert(insertView, 0, titleValue) !=
        reflection::PropertyAccessError::ValueTypeMismatch ||
        samplesProperty->TryInsert(insertView, 0, {}) !=
        reflection::PropertyAccessError::InvalidValue ||
        samplesProperty->TryInsert(readOnlyInsertView, 0, replacementValue) !=
        reflection::PropertyAccessError::ReadOnlyObject ||
        frozenVectorProperty->TryInsert(insertView, 0, numberValue) !=
        reflection::PropertyAccessError::ReadOnlyProperty ||
        insertCollection.Samples != beforeInsertFailure)
    {
        return 136;
    }

    if (samplesProperty->TryInsert(empty, 0, replacementValue) !=
        reflection::PropertyAccessError::InvalidObject ||
        samplesProperty->TryInsert(wrongOwner, 0, replacementValue) !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        speed->TryInsert(view, 0, replacementValue) !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryInsert(insertView, 0, replacementValue) !=
        reflection::PropertyAccessError::ReadUnavailable)
    {
        return 137;
    }

    // 복사는 가능해도, 대입이 불가능한 요소는 삽입 대상에 연결하지 않습니다.
    const NonAssignable nonAssignableInput{};
    const auto nonAssignableInputValue =
        reflection::ValueView::From(nonAssignableInput);

    if (nonAssignableItems.CanInsert() ||
        nonAssignableItems.TryInsert(
            nonAssignableView, 0, nonAssignableInputValue) !=
        reflection::PropertyAccessError::InsertUnavailable ||
        nonAssignableCollection.Items.size() != 1 ||
        moveOnlyProperty.CanInsert() ||
        moveOnlyProperty.TryInsert(moveOnlyView, 0, ownedNumberValue) !=
        reflection::PropertyAccessError::InsertUnavailable ||
        nestedMoveOnlyProperty.CanInsert())
    {
        return 138;
    }

    std::cout << "Vector insert checks passed.\n";

    consumer::Collection eraseCollection;
    eraseCollection.Samples = { 1.0f, 2.0f, 3.0f, 4.0f };
    const auto eraseView = reflection::ObjectView::From(eraseCollection);

    if (!samplesProperty->CanErase() || !itemsProperty->CanErase() ||
        !flagsProperty->CanErase() || frozenVectorProperty->CanErase())
    {
        return 139;
    }

    // 중간, 앞, 끝 순서로 삭제하며 나머지 요소의 순서를 확인합니다.
    if (samplesProperty->TryErase(eraseView, 1) !=
        reflection::PropertyAccessError::None ||
        eraseCollection.Samples != std::vector<float>{ 1.0f, 3.0f, 4.0f } ||
        samplesProperty->TryErase(eraseView, 0) !=
        reflection::PropertyAccessError::None ||
        eraseCollection.Samples != std::vector<float>{ 3.0f, 4.0f } ||
        samplesProperty->TryErase(eraseView, 1) !=
        reflection::PropertyAccessError::None ||
        eraseCollection.Samples != std::vector<float>{ 3.0f })
    {
        return 140;
    }

    if (samplesProperty->TryErase(eraseView, 0) !=
        reflection::PropertyAccessError::None ||
        !eraseCollection.Samples.empty() ||
        samplesProperty->TryErase(eraseView, 0) !=
        reflection::PropertyAccessError::IndexOutOfRange ||
        flagsProperty->TryErase(eraseView, 0) !=
        reflection::PropertyAccessError::None ||
        !eraseCollection.Flags.empty())
    {
        return 141;
    }

    eraseCollection.Items.resize(2);
    eraseCollection.Items[0].Speed = 10.0f;
    eraseCollection.Items[1].Speed = 20.0f;
    eraseCollection.Items[1].Title = "Remaining";

    if (itemsProperty->TryErase(eraseView, 0) !=
        reflection::PropertyAccessError::None ||
        eraseCollection.Items.size() != 1 ||
        eraseCollection.Items[0].Speed != 20.0f ||
        eraseCollection.Items[0].Title != "Remaining" ||
        noDefaultProperty.TryErase(noDefaultView, 0) !=
        reflection::PropertyAccessError::None ||
        noDefaultCollection.Items.size() != 1 ||
        noDefaultCollection.Items[0].Number != 9)
    {
        return 142;
    }

    // 이동 전용 요소를 삭제하고, 남은 객체의 소유권을 확인합니다.
    moveOnlyCollection.Items.push_back(std::make_unique<int>(21));
    moveOnlyCollection.Items.push_back(std::make_unique<int>(22));
    const auto* remainingNumber = moveOnlyCollection.Items[1].get();

    if (!moveOnlyProperty.CanErase() ||
        moveOnlyProperty.TryErase(moveOnlyView, 0) !=
        reflection::PropertyAccessError::None ||
        moveOnlyCollection.Items.size() != 1 ||
        moveOnlyCollection.Items[0].get() != remainingNumber ||
        *moveOnlyCollection.Items[0] != 22)
    {
        return 143;
    }

    const auto beforeEraseFailure = protectedCollection.Samples;

    if (samplesProperty->TryErase(
        protectedView, protectedCollection.Samples.size()) !=
        reflection::PropertyAccessError::IndexOutOfRange ||
        samplesProperty->TryErase(protectedReadOnlyView, 0) !=
        reflection::PropertyAccessError::ReadOnlyObject ||
        frozenVectorProperty->TryErase(protectedView, 0) !=
        reflection::PropertyAccessError::ReadOnlyProperty ||
        samplesProperty->TryErase(empty, 0) !=
        reflection::PropertyAccessError::InvalidObject ||
        samplesProperty->TryErase(wrongOwner, 0) !=
        reflection::PropertyAccessError::OwnerTypeMismatch ||
        speed->TryErase(view, 0) !=
        reflection::PropertyAccessError::NotContainer ||
        unbound.TryErase(protectedView, 0) !=
        reflection::PropertyAccessError::ReadUnavailable ||
        protectedCollection.Samples != beforeEraseFailure ||
        protectedCollection.Frozen != std::vector<int>{ 3, 4 })
    {
        return 144;
    }

    if (nonAssignableItems.CanErase() ||
        nonAssignableItems.TryErase(nonAssignableView, 0) !=
        reflection::PropertyAccessError::EraseUnavailable ||
        nonAssignableCollection.Items.size() != 1 ||
        nonAssignableCollection.Items[0].Number != number)
    {
        return 145;
    }

    std::cout << "Vector erase checks passed.\n";

    const auto* privateCollectionType = registry.FindType<consumer::PrivateCollection>();
    const auto* privateValues = privateCollectionType
        ? privateCollectionType->FindProperty("m_values") : nullptr;
    const auto* privateItems = privateCollectionType
        ? privateCollectionType->FindProperty("m_items") : nullptr;

    if (privateValues == nullptr || privateItems == nullptr)
    {
        return 146;
    }

    if (!privateValues->CanReadElement() || !privateValues->CanWriteElement() ||
        !privateValues->CanResize() || !privateValues->CanClear() ||
        !privateValues->CanAppend() || !privateValues->CanInsert() || !privateValues->CanErase())
    {
        return 147;
    }

    consumer::PrivateCollection privateCollection;
    const auto privateView = reflection::ObjectView::From(privateCollection);
    const auto privateSize = privateValues->TryGetSize(privateView);
    const auto privateRead = privateValues->TryReadElement(privateView, 0);

    if (!privateSize || privateSize.Size != 2 || !privateRead)
    {
        return 148;
    }

    const auto* privateFirstValue = privateRead.Value.Get<float>();
    if (privateFirstValue == nullptr || *privateFirstValue != 1.0f ||
        privateValues->TryWriteElement(privateView, 1, replacementValue) != AccessError::None ||
        privateCollection.GetValues() != std::vector<float>{ 1.0f, replacement })
    {
        return 149;
    }

    // 이후 구조 변경으로 기존 요소 주소가 무효화될 수 있으므로 다시 사용하지 않습니다.
    if (privateValues->TryResize(privateView, 3) != AccessError::None ||
        privateCollection.GetValues() != std::vector<float>{ 1.0f, replacement, 0.0f } ||
        privateValues->TryAppend(privateView, replacementValue) != AccessError::None ||
        privateValues->TryInsert(privateView, 1, replacementValue) != AccessError::None ||
        privateCollection.GetValues() != std::vector<float>{ 1.0f, replacement, replacement, 0.0f, replacement })
    {
        return 150;
    }

    if (privateValues->TryErase(privateView, 2) != AccessError::None ||
        privateCollection.GetValues() != std::vector<float>{ 1.0f, replacement, 0.0f, replacement })
    {
        return 151;
    }

    // 비공개 벡터 안의 객체도 수정 가능한 뷰로 연결되는지 확인합니다.
    const auto privateItem = privateItems->TryEditElementObject(privateView, 0);
    if (!privateItems->CanEditElementObject() || !privateItem ||
        registry.FindType(privateItem.Object) != settingsType ||
        nestedSpeed->TryWriteValue(privateItem.Object, replacementValue) != AccessError::None ||
        privateCollection.GetItems()[0].Speed != replacement ||
        privateCollection.GetItems()[0].Title != "Default")
    {
        return 152;
    }

    const auto privateReadOnlyView = reflection::ObjectView::From(
        static_cast<const consumer::PrivateCollection&>(privateCollection));
    const auto privateBeforeFailure = privateCollection.GetValues();

    if (privateValues->TryWriteElement(privateReadOnlyView, 0, replacementValue) != AccessError::ReadOnlyObject ||
        privateValues->TryResize(privateReadOnlyView, 0) != AccessError::ReadOnlyObject ||
        privateValues->TryClear(privateReadOnlyView) != AccessError::ReadOnlyObject ||
        privateValues->TryAppend(privateReadOnlyView, replacementValue) != AccessError::ReadOnlyObject ||
        privateValues->TryInsert(privateReadOnlyView, 0, replacementValue) != AccessError::ReadOnlyObject ||
        privateValues->TryErase(privateReadOnlyView, 0) != AccessError::ReadOnlyObject ||
        privateItems->TryEditElementObject(privateReadOnlyView, 0).Error != AccessError::ReadOnlyObject ||
        privateCollection.GetValues() != privateBeforeFailure)
    {
        return 153;
    }

    if (privateValues->TryClear(privateView) != AccessError::None ||
        !privateCollection.GetValues().empty() ||
        privateValues->TryReadElement(privateView, 0).Error != AccessError::IndexOutOfRange)
    {
        return 154;
    }

    std::cout << "Private container checks passed.\n";

    // 프로퍼티의 실제 enum 타입을 지정하지 않고 값을 읽습니다.
    const auto* valueEnum = registry.FindEnum(stateValue);
    const auto currentEnumNumber = valueEnum ? valueEnum->ReadValue(stateValue) : std::nullopt;

    if (valueEnum != state || !state->CanReadValue() || !currentEnumNumber ||
        *currentEnumNumber != reflection::EnumValue{ std::int64_t{ 1 } })
    {
        return 155;
    }

    // ValueView는 원본을 가리키지만 ReadValue의 결과는 복사본입니다.
    snapshot.Current = consumer::State::Stopped;
    const auto changedEnumNumber = state->ReadValue(stateValue);

    if (!changedEnumNumber || *changedEnumNumber != reflection::EnumValue{ std::int64_t{ 0 } } ||
        *currentEnumNumber != reflection::EnumValue{ std::int64_t{ 1 } })
    {
        return 156;
    }

    snapshot.Current = consumer::State::Running;

    enum class SignedCode : std::int64_t
    {
        Negative = -7
    };

    enum class UnsignedCode : std::uint64_t
    {
        Maximum = std::numeric_limits<std::uint64_t>::max()
    };

    const auto signedInfo = reflection::EnumInfo::For<SignedCode>("SignedCode");
    const auto unsignedInfo = reflection::EnumInfo::For<UnsignedCode>("UnsignedCode");
    const SignedCode signedCode = SignedCode::Negative;
    const UnsignedCode unsignedCode = UnsignedCode::Maximum;
    const auto signedCodeView = reflection::ValueView::From(signedCode);
    const auto unsignedCodeView = reflection::ValueView::From(unsignedCode);
    const auto signedNumber = signedInfo.ReadValue(signedCodeView);
    const auto unsignedNumber = unsignedInfo.ReadValue(unsignedCodeView);

    if (!signedNumber || *signedNumber != reflection::EnumValue{ std::int64_t{ -7 } } ||
        !unsignedNumber ||
        *unsignedNumber != reflection::EnumValue{ std::numeric_limits<std::uint64_t>::max() })
    {
        return 157;
    }

    enum class OtherState : int
    {
        Running = 1
    };

    const OtherState otherState = OtherState::Running;
    const auto otherStateView = reflection::ValueView::From(otherState);

    if (state->ReadValue(emptyValue) || state->ReadValue(stringValue) ||
        state->ReadValue(otherStateView) || signedInfo.ReadValue(unsignedCodeView))
    {
        return 158;
    }

    reflection::EnumInfo descriptionOnly;
    descriptionOnly.QualifiedName = "DescriptionOnly";

    if (descriptionOnly.CanReadValue() || descriptionOnly.ReadValue(stateValue))
    {
        return 159;
    }

    // 선언된 열거자에 없는 값도 실제 숫자를 읽습니다.
    const auto unnamedState = static_cast<consumer::State>(99);
    const auto unnamedStateView = reflection::ValueView::From(unnamedState);
    const auto unnamedNumber = state->ReadValue(unnamedStateView);

    if (!unnamedNumber || *unnamedNumber != reflection::EnumValue{ std::int64_t{ 99 } })
    {
        return 160;
    }

    std::cout << "Enum value read checks passed.\n";

    if (!stateProperty->CanWriteEnumValue() ||
        stateProperty->TryWriteEnumValue(snapshotView, reflection::EnumValue{ std::int64_t{ 0 } }) != AccessError::None ||
        snapshot.Current != consumer::State::Stopped)
    {
        return 161;
    }

    // 이름 없는 값도 기반 타입의 표현 범위 안이면 쓸 수 있습니다.
    if (stateProperty->TryWriteEnumValue(snapshotView, reflection::EnumValue{ std::uint64_t{ 99 } }) != AccessError::None ||
        snapshot.Current != static_cast<consumer::State>(99))
    {
        return 162;
    }

    const auto readOnlySnapshot = reflection::ObjectView::From(
        static_cast<const consumer::Snapshot&>(snapshot));

    if (stateProperty->TryWriteEnumValue(readOnlySnapshot, reflection::EnumValue{ std::int64_t{ 0 } }) !=
        AccessError::ReadOnlyObject ||
        snapshot.Current != static_cast<consumer::State>(99))
    {
        return 163;
    }

    enum class SmallFlags : std::uint8_t
    {
        None = 0,
        Read = 1,
        Write = 2
    };

    struct EnumWriteOwner
    {
        SmallFlags Flags = SmallFlags::None;
        SignedCode Signed = SignedCode::Negative;
        UnsignedCode Unsigned = UnsignedCode::Maximum;
        const SmallFlags Frozen = SmallFlags::Read;
    };

    EnumWriteOwner enumWriteOwner;
    const auto enumWriteView = reflection::ObjectView::From(enumWriteOwner);
    const auto flagsAccess = reflection::MakeProperty<&EnumWriteOwner::Flags>("Flags", "SmallFlags");
    const auto signedAccess = reflection::MakeProperty<&EnumWriteOwner::Signed>("Signed", "SignedCode");
    const auto unsignedAccess = reflection::MakeProperty<&EnumWriteOwner::Unsigned>("Unsigned", "UnsignedCode");
    const auto frozenAccess = reflection::MakeProperty<&EnumWriteOwner::Frozen>("Frozen", "SmallFlags");

    // Read | Write에 해당하는 조합 값도 허용합니다.
    if (flagsAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ std::uint64_t{ 3 } }) != AccessError::None ||
        static_cast<std::uint8_t>(enumWriteOwner.Flags) != 3)
    {
        return 164;
    }

    if (flagsAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ std::int64_t{ -1 } }) !=
        AccessError::ValueOutOfRange ||
        flagsAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ std::uint64_t{ 256 } }) !=
        AccessError::ValueOutOfRange ||
        static_cast<std::uint8_t>(enumWriteOwner.Flags) != 3)
    {
        return 165;
    }

    const auto unsignedMaximum = std::numeric_limits<std::uint64_t>::max();

    if (signedAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ std::int64_t{ -7 } }) != AccessError::None ||
        signedAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ unsignedMaximum }) !=
        AccessError::ValueOutOfRange ||
        enumWriteOwner.Signed != SignedCode::Negative ||
        unsignedAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ unsignedMaximum }) != AccessError::None ||
        unsignedAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ std::int64_t{ -1 } }) !=
        AccessError::ValueOutOfRange ||
        enumWriteOwner.Unsigned != UnsignedCode::Maximum)
    {
        return 166;
    }

    if (frozenAccess.CanWriteEnumValue() ||
        frozenAccess.TryWriteEnumValue(enumWriteView, reflection::EnumValue{ std::uint64_t{ 0 } }) !=
        AccessError::WriteUnavailable ||
        enumWriteOwner.Frozen != SmallFlags::Read ||
        speed->CanWriteEnumValue() ||
        speed->TryWriteEnumValue(view, reflection::EnumValue{ std::int64_t{ 1 } }) !=
        AccessError::EnumWriteUnavailable)
    {
        return 167;
    }

    if (stateProperty->TryWriteEnumValue(empty, reflection::EnumValue{ std::int64_t{ 0 } }) !=
        AccessError::InvalidObject ||
        stateProperty->TryWriteEnumValue(wrongOwner, reflection::EnumValue{ std::int64_t{ 0 } }) !=
        AccessError::OwnerTypeMismatch)
    {
        return 168;
    }

    snapshot.Current = consumer::State::Running;
    std::cout << "Enum value write checks passed.\n";

    const auto* runningBySigned = state->FindEntryByValue(reflection::EnumValue{ std::int64_t{ 1 } });
    const auto* runningByUnsigned = state->FindEntryByValue(reflection::EnumValue{ std::uint64_t{ 1 } });

    if (runningBySigned != running || runningByUnsigned != running)
    {
        return 169;
    }

    const auto* activeAlias = state->FindEntry("Active");
    if (activeAlias == nullptr || activeAlias == running ||
        activeAlias->Value != running->Value ||
        state->FindEntryByValue(activeAlias->Value) != running)
    {
        return 170;
    }

    // 실제 프로퍼티 값에서 열거자 이름을 역조회합니다.
    const auto currentNumberForLookup = state->ReadValue(stateValue);
    const auto* currentEntry = currentNumberForLookup
        ? state->FindEntryByValue(*currentNumberForLookup) : nullptr;

    if (currentEntry == nullptr || currentEntry->Name != "Running")
    {
        return 171;
    }

    auto signedLookup = signedInfo;
    signedLookup.Entries = {
        { "Negative", reflection::EnumValue{ std::int64_t{ -7 } } },
        { "Positive", reflection::EnumValue{ std::int64_t{ 7 } } }
    };

    const auto* negativeEntry = signedLookup.FindEntryByValue(reflection::EnumValue{ std::int64_t{ -7 } });
    const auto* positiveEntry = signedLookup.FindEntryByValue(reflection::EnumValue{ std::uint64_t{ 7 } });

    if (negativeEntry == nullptr || negativeEntry->Name != "Negative" ||
        positiveEntry == nullptr || positiveEntry->Name != "Positive" ||
        signedLookup.FindEntryByValue(reflection::EnumValue{ unsignedMaximum }) != nullptr)
    {
        return 172;
    }

    auto unsignedLookup = unsignedInfo;
    unsignedLookup.Entries = {
        { "Maximum", reflection::EnumValue{ unsignedMaximum } }
    };

    const auto* maximumEntry = unsignedLookup.FindEntryByValue(reflection::EnumValue{ unsignedMaximum });
    if (maximumEntry == nullptr || maximumEntry->Name != "Maximum" ||
        unsignedLookup.FindEntryByValue(reflection::EnumValue{ std::int64_t{ -1 } }) != nullptr)
    {
        return 173;
    }

    auto flagsLookup = reflection::EnumInfo::For<SmallFlags>("SmallFlags");
    flagsLookup.Entries = {
        { "None", reflection::EnumValue{ std::uint64_t{ 0 } } },
        { "Read", reflection::EnumValue{ std::uint64_t{ 1 } } },
        { "Write", reflection::EnumValue{ std::uint64_t{ 2 } } }
    };

    if (flagsLookup.FindEntryByValue(reflection::EnumValue{ std::uint64_t{ 3 } }) != nullptr ||
        state->FindEntryByValue(reflection::EnumValue{ std::int64_t{ 99 } }) != nullptr ||
        descriptionOnly.FindEntryByValue(reflection::EnumValue{ std::int64_t{ 0 } }) != nullptr)
    {
        return 174;
    }

    std::cout << "Enum value lookup checks passed.\n";

    const auto* statesProperty = collectionType->FindProperty("States");
    const auto* frozenStatesProperty = collectionType->FindProperty("FrozenStates");

    if (statesProperty == nullptr || frozenStatesProperty == nullptr ||
        !statesProperty->CanWriteEnumElement() || frozenStatesProperty->CanWriteEnumElement())
    {
        return 175;
    }

    consumer::Collection enumCollection;
    const auto enumCollectionView = reflection::ObjectView::From(enumCollection);

    if (statesProperty->TryWriteEnumElement(
        enumCollectionView, 0, reflection::EnumValue{ std::uint64_t{ 1 } }) != AccessError::None ||
        enumCollection.States[0] != consumer::State::Running)
    {
        return 176;
    }

    const auto writtenElement = statesProperty->TryReadElement(enumCollectionView, 0);
    const auto* writtenEnum = writtenElement ? registry.FindEnum(writtenElement.Value) : nullptr;
    const auto writtenNumber = writtenEnum ? writtenEnum->ReadValue(writtenElement.Value) : std::nullopt;
    const auto* writtenEntry = writtenNumber ? writtenEnum->FindEntryByValue(*writtenNumber) : nullptr;

    if (writtenEntry == nullptr || writtenEntry->Name != "Running")
    {
        return 177;
    }

    struct FlagsVectorOwner
    {
        std::vector<SmallFlags> Values{ SmallFlags::None };
    };

    FlagsVectorOwner flagsVectorOwner;
    const auto flagsVectorView = reflection::ObjectView::From(flagsVectorOwner);
    const auto flagsVectorAccess =
        reflection::MakeProperty<&FlagsVectorOwner::Values>("Values", "std::vector<SmallFlags>");

    if (!flagsVectorAccess.CanWriteEnumElement() ||
        flagsVectorAccess.TryWriteEnumElement(
            flagsVectorView, 0, reflection::EnumValue{ std::uint64_t{ 3 } }) != AccessError::None ||
        static_cast<std::uint8_t>(flagsVectorOwner.Values[0]) != 3)
    {
        return 178;
    }

    if (flagsVectorAccess.TryWriteEnumElement(
        flagsVectorView, 0, reflection::EnumValue{ std::int64_t{ -1 } }) != AccessError::ValueOutOfRange ||
        flagsVectorAccess.TryWriteEnumElement(
            flagsVectorView, 0, reflection::EnumValue{ std::uint64_t{ 256 } }) != AccessError::ValueOutOfRange ||
        static_cast<std::uint8_t>(flagsVectorOwner.Values[0]) != 3)
    {
        return 179;
    }

    const auto readOnlyEnumCollection = reflection::ObjectView::From(
        static_cast<const consumer::Collection&>(enumCollection));
    const reflection::EnumValue stoppedNumber{ std::int64_t{ 0 } };

    if (statesProperty->TryWriteEnumElement(readOnlyEnumCollection, 0, stoppedNumber) != AccessError::ReadOnlyObject ||
        frozenStatesProperty->TryWriteEnumElement(enumCollectionView, 0, stoppedNumber) !=
        AccessError::ReadOnlyProperty ||
        statesProperty->TryWriteEnumElement(enumCollectionView, 2, stoppedNumber) != AccessError::IndexOutOfRange ||
        enumCollection.States[0] != consumer::State::Running ||
        enumCollection.FrozenStates[0] != consumer::State::Stopped)
    {
        return 180;
    }

    if (statesProperty->TryWriteEnumElement(empty, 0, stoppedNumber) != AccessError::InvalidObject ||
        statesProperty->TryWriteEnumElement(wrongOwner, 0, stoppedNumber) != AccessError::OwnerTypeMismatch ||
        samplesProperty->CanWriteEnumElement() ||
        samplesProperty->TryWriteEnumElement(enumCollectionView, 0, stoppedNumber) != AccessError::EnumWriteUnavailable ||
        speed->TryWriteEnumElement(view, 0, stoppedNumber) != AccessError::NotContainer ||
        unbound.TryWriteEnumElement(enumCollectionView, 0, stoppedNumber) != AccessError::ReadUnavailable)
    {
        return 181;
    }

    std::cout << "Enum element write checks passed.\n";

    if (!statesProperty->CanAppendEnumElement() || !statesProperty->CanInsertEnumElement() ||
        frozenStatesProperty->CanAppendEnumElement() || frozenStatesProperty->CanInsertEnumElement())
    {
        return 182;
    }

    consumer::Collection enumEditCollection;
    const auto enumEditView = reflection::ObjectView::From(enumEditCollection);
    const reflection::EnumValue runningNumber{ std::uint64_t{ 1 } };
    const reflection::EnumValue unknownNumber{ std::int64_t{ 99 } };

    if (statesProperty->TryAppendEnumElement(enumEditView, stoppedNumber) != AccessError::None ||
        statesProperty->TryInsertEnumElement(enumEditView, 1, runningNumber) != AccessError::None ||
        statesProperty->TryInsertEnumElement(enumEditView, 4, unknownNumber) != AccessError::None ||
        enumEditCollection.States != std::vector<consumer::State>{
        consumer::State::Stopped, consumer::State::Running, consumer::State::Running,
            consumer::State::Stopped, static_cast<consumer::State>(99) })
    {
        return 183;
    }

    if (flagsVectorAccess.TryAppendEnumElement(
        flagsVectorView, reflection::EnumValue{ std::uint64_t{ 255 } }) != AccessError::None ||
        flagsVectorAccess.TryInsertEnumElement(
            flagsVectorView, 0, reflection::EnumValue{ std::int64_t{ 1 } }) != AccessError::None ||
        flagsVectorOwner.Values.size() != 3 ||
        static_cast<std::uint8_t>(flagsVectorOwner.Values[0]) != 1 ||
        static_cast<std::uint8_t>(flagsVectorOwner.Values[1]) != 3 ||
        static_cast<std::uint8_t>(flagsVectorOwner.Values[2]) != 255)
    {
        return 184;
    }

    const auto flagsBeforeFailure = flagsVectorOwner.Values;

    if (flagsVectorAccess.TryAppendEnumElement(
        flagsVectorView, reflection::EnumValue{ std::int64_t{ -1 } }) != AccessError::ValueOutOfRange ||
        flagsVectorAccess.TryInsertEnumElement(
            flagsVectorView, 0, reflection::EnumValue{ std::uint64_t{ 256 } }) != AccessError::ValueOutOfRange ||
        flagsVectorOwner.Values != flagsBeforeFailure)
    {
        return 185;
    }

    const auto readOnlyEnumEditView = reflection::ObjectView::From(
        static_cast<const consumer::Collection&>(enumEditCollection));
    const auto statesBeforeFailure = enumEditCollection.States;

    if (statesProperty->TryAppendEnumElement(readOnlyEnumEditView, runningNumber) != AccessError::ReadOnlyObject ||
        statesProperty->TryInsertEnumElement(readOnlyEnumEditView, 0, runningNumber) != AccessError::ReadOnlyObject ||
        frozenStatesProperty->TryAppendEnumElement(enumEditView, runningNumber) != AccessError::ReadOnlyProperty ||
        frozenStatesProperty->TryInsertEnumElement(enumEditView, 0, runningNumber) != AccessError::ReadOnlyProperty ||
        statesProperty->TryInsertEnumElement(enumEditView, 6, runningNumber) != AccessError::IndexOutOfRange ||
        enumEditCollection.States != statesBeforeFailure ||
        enumEditCollection.FrozenStates != std::vector<consumer::State>{ consumer::State::Stopped })
    {
        return 186;
    }

    if (statesProperty->TryAppendEnumElement(empty, runningNumber) != AccessError::InvalidObject ||
        statesProperty->TryInsertEnumElement(wrongOwner, 0, runningNumber) != AccessError::OwnerTypeMismatch ||
        samplesProperty->TryAppendEnumElement(enumEditView, runningNumber) != AccessError::EnumWriteUnavailable ||
        samplesProperty->TryInsertEnumElement(enumEditView, 0, runningNumber) != AccessError::EnumWriteUnavailable ||
        speed->TryAppendEnumElement(view, runningNumber) != AccessError::NotContainer ||
        speed->TryInsertEnumElement(view, 0, runningNumber) != AccessError::NotContainer ||
        unbound.TryAppendEnumElement(enumEditView, runningNumber) != AccessError::ReadUnavailable ||
        unbound.TryInsertEnumElement(enumEditView, 0, runningNumber) != AccessError::ReadUnavailable)
    {
        return 187;
    }

    // 빈 벡터의 위치 0에도 삽입할 수 있습니다.
    consumer::Collection emptyEnumCollection;
    emptyEnumCollection.States.clear();
    const auto emptyEnumView = reflection::ObjectView::From(emptyEnumCollection);

    if (statesProperty->TryInsertEnumElement(emptyEnumView, 0, runningNumber) != AccessError::None ||
        emptyEnumCollection.States != std::vector<consumer::State>{ consumer::State::Running })
    {
        return 188;
    }

    std::cout << "Enum element append/insert checks passed.\n";

    enum ManualPlain
    {
        ManualPlainValue = 0
    };

    enum ManualFixed : std::uint8_t
    {
        ManualFixedValue = 0
    };

    const auto manualScoped = reflection::EnumInfo::For<consumer::State>("ManualScoped");
    const auto manualPlain = reflection::EnumInfo::For<ManualPlain>("ManualPlain");
    const auto manualFixed = reflection::EnumInfo::For<ManualFixed>("ManualFixed");

    if (!manualScoped.IsScoped || manualPlain.IsScoped || manualFixed.IsScoped)
    {
        std::cerr << "Manual enum scope checks failed.\n";
        return 189;
    }

    std::cout << "Manual enum scope checks passed.\n";

    std::cout << "Consumer Speed: " << settings.Speed << '\n'
        << "Consumer Unit: " << *unit << '\n'
        << "Consumer Meter: " << *measured << '\n'
        << "Consumer free function: " << *doubled << '\n'
        << "Consumer Title: " << *reflectedTitle << '\n'
        << "Standalone consumer checks passed.\n";

    return 0;
}