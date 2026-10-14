#include "engine-component-tests/TestSupport.h"
#include "EngineComponents.gen.h"

#include <reflection/Registry.h>

#include <filesystem>
#include <iostream>
#include <system_error>

int main()
{
    reflection::Registry registry;

    if (!reflection_generated::Register_EngineComponents(registry))
    {
        return archive_engine_component_tests::Fail("Engine component registration failed.");
    }

    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return archive_engine_component_tests::Fail("Cannot create the engine component archive directory.");
    }

    if (archive_engine_component_tests::CheckEngineNameComponent(registry, directory) != 0 ||
        archive_engine_component_tests::CheckEngineValueComponents(registry, directory) != 0 ||
        archive_engine_component_tests::CheckEngineTransformComponents(registry, directory) != 0 ||
        archive_engine_component_tests::CheckEngineComponentBindings(registry) != 0)
    {
        return 1;
    }

    std::cout << "All engine component archive checks passed.\n";
    return 0;
}
