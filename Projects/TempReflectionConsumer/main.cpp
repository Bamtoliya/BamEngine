#include <reflection/Reflection.h>
#include "AppTypes.h"

#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

namespace reflection_generated
{
    bool Register_Consumer(reflection::Registry& registry);
}

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

    std::cout << "Consumer Speed: " << settings.Speed << '\n'
        << "Consumer Unit: " << *unit << '\n'
        << "Consumer Meter: " << *measured << '\n'
        << "Consumer free function: " << *doubled << '\n'
        << "Consumer Title: " << *reflectedTitle << '\n'
        << "Standalone consumer checks passed.\n";

    return 0;
}