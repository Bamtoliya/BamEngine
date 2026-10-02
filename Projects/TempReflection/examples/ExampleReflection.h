#pragma once

#include <reflection/Registry.h>

namespace reflection_generated
{
    bool Register_Demo(reflection::Registry& registry);
    bool Register_Demo(
        reflection::Registry& registry, reflection::RegistrationResult& failure);
    bool Register_Failure(reflection::Registry& registry);
    bool Register_Failure(
        reflection::Registry& registry, reflection::RegistrationResult& failure);
}