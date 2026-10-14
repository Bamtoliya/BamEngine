#include "generated-tests/TestSupport.h"
#include "ArchiveDemo.gen.h"

#include <reflection/Registry.h>

#include <filesystem>
#include <iostream>
#include <system_error>

int main()
{
    reflection::Registry registry;

    if (!reflection_generated::Register_ArchiveDemo(registry))
    {
        return archive_generated_tests::Fail("Generated archive registration failed.");
    }

    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return archive_generated_tests::Fail("Cannot create the generated archive directory.");
    }

    if (archive_generated_tests::CheckGeneratedProfile(registry, directory) != 0 ||
        archive_generated_tests::CheckGeneratedContainers(registry, directory) != 0 ||
        archive_generated_tests::CheckGeneratedObjectArray(registry, directory) != 0 ||
        archive_generated_tests::CheckGeneratedFixedArray(registry, directory) != 0 ||
        archive_generated_tests::CheckGeneratedContainerTraversal(registry) != 0)
    {
        return 1;
    }

    std::cout << "All generated reflection archive checks passed.\n";
    return 0;
}
