#include "TestSupport.h"
#include "Demo.gen.h"

#include <reflection/Reflection.h>

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <variant>

namespace reflection_tests
{
    namespace
    {
        bool CheckEnumValueVisits()
        {
            enum class Small : std::int8_t { Zero = 0 };
            enum class Wide : std::uint64_t { Zero = 0 };
            enum class Binary : bool { Off = false, On = true };
            enum Legacy { Zero = 0 };
            const auto small = reflection::EnumInfo::For<Small>("tests::Small");
            const auto wide = reflection::EnumInfo::For<Wide>("tests::Wide");
            const auto binary = reflection::EnumInfo::For<Binary>("tests::Binary");
            const auto legacy = reflection::EnumInfo::For<Legacy>("tests::Legacy");
            int calls = 0;
            bool valid = false;

            if (!small.IsUnderlyingSigned() || wide.IsUnderlyingSigned() || !small.CanVisitValue() ||
                !small.TryVisitValue(reflection::EnumValue{ std::int64_t{ -128 } },
                    [&](const reflection::ValueView& value)
                    {
                        ++calls;
                        const auto* number = value.Get<Small>();
                        valid = number != nullptr && static_cast<std::int8_t>(*number) == -128;
                    }) || calls != 1 || !valid)
            {
                return false;
            }

            const auto count = [&](const reflection::ValueView&) { ++calls; };

            if (small.TryVisitValue(reflection::EnumValue{ std::int64_t{ -129 } }, count) ||
                small.TryVisitValue(reflection::EnumValue{ std::uint64_t{ 128 } }, count) ||
                wide.TryVisitValue(reflection::EnumValue{ std::int64_t{ -1 } }, count) ||
                binary.TryVisitValue(reflection::EnumValue{ std::uint64_t{ 2 } }, count) ||
                legacy.CanVisitValue() || legacy.TryVisitValue(reflection::EnumValue{ std::int64_t{ 0 } }, count) ||
                reflection::EnumInfo{}.TryVisitValue(reflection::EnumValue{ std::int64_t{ 0 } }, count) || calls != 1)
            {
                return false;
            }

            if (!wide.TryVisitValue(reflection::EnumValue{ (std::numeric_limits<std::uint64_t>::max)() },
                [&](const reflection::ValueView& value)
                {
                    const auto* number = value.Get<Wide>();
                    valid = number != nullptr &&
                        static_cast<std::uint64_t>(*number) == (std::numeric_limits<std::uint64_t>::max)();
                }) || !valid || !binary.TryVisitValue(reflection::EnumValue{ std::int64_t{ 1 } },
                [&](const reflection::ValueView& value)
                {
                    const auto* number = value.Get<Binary>();
                    valid = number != nullptr && *number == Binary::On;
                }) || !valid)
            {
                return false;
            }

            return true;
        }
    }

    bool ValidateEnumRegistry()
    {
        if (!CheckEnumValueVisits())
        {
            return false;
        }

        reflection::Registry registry;

        reflection::EnumInfo enumeration;
        enumeration.QualifiedName = "example::RuntimeEnum";
        enumeration.UnderlyingType = "long long";
        enumeration.IsScoped = true;

        enumeration.Entries.push_back({ "Negative", std::int64_t{-1} });
        enumeration.Entries.push_back({ "Zero", std::int64_t{0} });

        // 같은 값에 다른 이름을 붙이는 것은 허용합니다.
        enumeration.Entries.push_back({ "Default", std::int64_t{0} });

        if (!enumeration.Metadata.Add("DisplayName", std::string{ "Runtime Enum" }))
        {
            return false;
        }

        if (!registry.RegisterEnum(std::move(enumeration)))
        {
            return false;
        }

        const reflection::EnumInfo* info = registry.FindEnum("example::RuntimeEnum");

        if (info == nullptr ||
            !info->IsScoped ||
            info->UnderlyingType != "long long" ||
            info->Entries.size() != 3)
        {
            return false;
        }

        const reflection::EnumEntry* negative = info->FindEntry("Negative");
        const reflection::EnumEntry* alias = info->FindEntry("Default");

        if (negative == nullptr || alias == nullptr)
        {
            return false;
        }

        const auto* negativeValue = std::get_if<std::int64_t>(&negative->Value);
        const auto* aliasValue = std::get_if<std::int64_t>(&alias->Value);

        if (negativeValue == nullptr || *negativeValue != -1 ||
            aliasValue == nullptr || *aliasValue != 0)
        {
            return false;
        }

        const reflection::MetadataValue* displayName = info->Metadata.Find("DisplayName");

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
        wide.Entries.push_back({ "Maximum", std::numeric_limits<std::uint64_t>::max() });

        if (!registry.RegisterEnum(std::move(wide)))
        {
            return false;
        }

        const reflection::EnumInfo* wideInfo = registry.FindEnum("example::WideEnum");

        if (wideInfo == nullptr)
        {
            return false;
        }

        const reflection::EnumEntry* maximum = wideInfo->FindEntry("Maximum");

        if (maximum == nullptr)
        {
            return false;
        }

        const auto* maximumValue = std::get_if<std::uint64_t>(&maximum->Value);

        if (maximumValue == nullptr || *maximumValue != std::numeric_limits<std::uint64_t>::max())
        {
            return false;
        }

        if (info->FindEntry("Missing") != nullptr || registry.FindEnum("example::MissingEnum") != nullptr)
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

        const reflection::EnumInfo* movement = registry.FindEnum("example::MovementMode");

        if (movement == nullptr ||
            !movement->IsScoped ||
            movement->UnderlyingType != "int" ||
            movement->Entries.size() != 2)
        {
            return false;
        }

        const reflection::EnumEntry* walk = movement->FindEntry("Walk");
        const reflection::EnumEntry* run = movement->FindEntry("Run");

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

        const reflection::EnumInfo* signedInfo = registry.FindEnum("example::SignedBoundary");
        const reflection::EnumInfo* unsignedInfo = registry.FindEnum("example::UnsignedBoundary");

        if (signedInfo == nullptr || unsignedInfo == nullptr)
        {
            return false;
        }

        const reflection::EnumEntry* minimum = signedInfo->FindEntry("Minimum");
        const reflection::EnumEntry* negative = signedInfo->FindEntry("Negative");
        const reflection::EnumEntry* alias = signedInfo->FindEntry("Alias");
        const reflection::EnumEntry* maximum = unsignedInfo->FindEntry("Maximum");

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
}
