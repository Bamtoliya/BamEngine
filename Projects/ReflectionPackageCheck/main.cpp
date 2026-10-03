#include <reflection/Reflection.h>
#include "PackageTypes.h"
#include "PackageCheck.gen.h"

#include <iostream>
#include <string>
#include <variant>

int main()
{
    reflection::Registry registry;
    if (!reflection_generated::Register_PackageCheck(registry))
    {
        return 1;
    }

    const auto* type = registry.FindType<PackageSettings>();
    const auto* speed = type ? type->FindProperty("Speed") : nullptr;
    if (speed == nullptr)
    {
        return 2;
    }

    const auto* metadata = speed->Metadata.Find("DisplayName");
    const auto* displayName = metadata ? std::get_if<std::string>(metadata) : nullptr;
    if (displayName == nullptr || *displayName != "Speed")
    {
        return 3;
    }

    PackageSettings settings;
    if (!speed->Write<float>(settings, 12.0f))
    {
        return 4;
    }

    const auto* value = speed->Read<float>(settings);
    if (value == nullptr || *value != 12.0f || settings.Speed != 12.0f)
    {
        return 5;
    }

    std::cout << "Installed package generation checks passed.\n";
    return 0;
}