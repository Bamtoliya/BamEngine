#include "TestSupport.h"
#include "../DiagnosticsTypes.h"
#include "Demo.gen.h"

#include <reflection/Reflection.h>

#include <iostream>
#include <string_view>

namespace reflection_tests
{
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