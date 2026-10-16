#include "TestSupport.h"
#include "../DiagnosticsTypes.h"
#include "Demo.gen.h"

#include <reflection/Reflection.h>

#include <iostream>
#include <string_view>
#include <array>
#include <map>
#include <vector>
#include <utility>

namespace reflection_tests
{
    namespace
    {
        bool CheckNestedContainerAccess()
        {
            using Error = reflection::PropertyAccessError;
            struct Owner
            {
                std::vector<std::vector<int>> Values{ { 1, 2 } };
                std::map<int, std::vector<int>> Mapped{ { 7, { 3 } } };
                const std::vector<std::vector<int>> ReadOnly{ { 4 } };
            };
            Owner source;
            const auto property = reflection::MakeProperty<&Owner::Values>("Values", "Values");
            const auto mapped = reflection::MakeProperty<&Owner::Mapped>("Mapped", "Mapped");
            const auto readOnly = reflection::MakeProperty<&Owner::ReadOnly>("ReadOnly", "ReadOnly");
            const auto* child = property.GetElementContainerProperty();
            const auto* mappedChild = mapped.GetMapValueContainerProperty();
            const auto object = reflection::ObjectView::From(source);
            if (child == nullptr || mappedChild == nullptr || child != mappedChild ||
                child->GetElementContainerProperty() != nullptr || child->GetMapValueContainerProperty() != nullptr)
            {
                return false;
            }
            const auto value = property.TryReadElement(object, 0);
            if (!value)
            {
                return false;
            }
            const auto constChild = child->AsObject(value.Value);
            const int inserted = 9;
            const auto insertedValue = reflection::ValueView::From(inserted);
            const auto mutableChild = property.TryEditElementObject(object, 0);
            if (!constChild.IsReadOnly() || child->TryGetSize(constChild).Size != 2 ||
                child->TryAppend(constChild, insertedValue) != Error::ReadOnlyObject ||
                !mutableChild || child->TryAppend(mutableChild.Object, insertedValue) != Error::None ||
                source.Values[0] != std::vector<int>{ 1, 2, 9 })
            {
                return false;
            }
            const auto mapValue = mapped.TryEditMapValueObject(
                object, reflection::ValueView::From(source.Mapped.begin()->first));
            const auto readOnlyValue = readOnly.TryReadElement(object, 0);
            return mapValue && mappedChild->TryClear(mapValue.Object) == Error::None && source.Mapped.at(7).empty() &&
                readOnlyValue && child->AsObject(readOnlyValue.Value).IsReadOnly() &&
                !readOnly.TryEditElementObject(object, 0) && !child->AsObject(insertedValue).IsValid() &&
                child->TryGetSize(object).Error == Error::OwnerTypeMismatch;
        }

        bool CheckFixedArrayReset()
        {
            struct Element
            {
                int Value = 7;
                Element() = default;
                Element(const Element&) = default;
                Element(Element&&) = delete;
                Element& operator=(const Element&) = default;
                Element& operator=(Element&&) = delete;
            };

            struct NoDefault
            {
                explicit NoDefault(int value) : Value(value) {}
                int Value;
            };

            struct Arrays
            {
                std::array<Element, 2> Values{};
                std::array<int, 2> Numbers{ 9, 11 };
                const std::array<Element, 1> ReadOnly{};
                std::array<NoDefault, 1> Constructed{ NoDefault{ 13 } };
                std::array<NoDefault, 0> Empty{};
                std::vector<int> Dynamic{ 1, 2 };
                int Scalar = 3;
            };

            using Error = reflection::PropertyAccessError;
            const auto values = reflection::MakeProperty<&Arrays::Values>("Values", "Values");
            const auto numbers = reflection::MakeProperty<&Arrays::Numbers>("Numbers", "Numbers");
            const auto readOnly = reflection::MakeProperty<&Arrays::ReadOnly>("ReadOnly", "ReadOnly");
            const auto constructed = reflection::MakeProperty<&Arrays::Constructed>("Constructed", "Constructed");
            const auto empty = reflection::MakeProperty<&Arrays::Empty>("Empty", "Empty");
            const auto dynamic = reflection::MakeProperty<&Arrays::Dynamic>("Dynamic", "Dynamic");
            const auto scalar = reflection::MakeProperty<&Arrays::Scalar>("Scalar", "Scalar");
            Arrays source;
            source.Values[0].Value = 99;
            const auto object = reflection::ObjectView::From(source);

            if (!values.CanResetElements() || values.TryResetElements(object) != Error::None ||
                source.Values[0].Value != 7 || source.Values[1].Value != 7 || source.Values.size() != 2 ||
                !numbers.CanResetElements() || numbers.TryResetElements(object) != Error::None ||
                source.Numbers != std::array<int, 2>{ 0, 0 } ||
                !empty.CanResetElements() || empty.TryResetElements(object) != Error::None)
            {
                return false;
            }

            const auto constObject = reflection::ObjectView::From(std::as_const(source));
            const auto otherObject = reflection::ObjectView::From(source.Values);
            return values.TryResetElements({}) == Error::InvalidObject &&
                values.TryResetElements(otherObject) == Error::OwnerTypeMismatch &&
                values.TryResetElements(constObject) == Error::ReadOnlyObject &&
                !readOnly.CanResetElements() && readOnly.TryResetElements(object) == Error::ReadOnlyProperty &&
                !constructed.CanResetElements() && constructed.TryResetElements(object) == Error::ResetUnavailable &&
                source.Constructed[0].Value == 13 &&
                !dynamic.CanResetElements() && dynamic.TryResetElements(object) == Error::ResetUnavailable &&
                source.Dynamic == std::vector<int>{ 1, 2 } &&
                scalar.TryResetElements(object) == Error::NotContainer &&
                reflection::ToString(Error::ResetUnavailable) == "ResetUnavailable";
        }
    }

    bool ValidatePropertyDiagnostics()
    {
        if (!CheckFixedArrayReset() || !CheckNestedContainerAccess())
        {
            return false;
        }

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
