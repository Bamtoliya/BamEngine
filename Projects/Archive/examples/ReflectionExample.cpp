#include "reflection-tests/TestSupport.h"
#include "reflection-tests/SettingsTestData.h"

#include <reflection/Registry.h>

#include <filesystem>
#include <iostream>
#include <system_error>

int main()
{
    reflection::Registry registry;

    if (!registry.Register(archive_reflection_tests::MakeSettingsType()))
    {
        return archive_reflection_tests::Fail("Settings registration failed.");
    }

    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return archive_reflection_tests::Fail("Cannot create the reflection archive directory.");
    }

    if (archive_reflection_tests::CheckSettingsArchive(registry, directory) != 0 ||
        archive_reflection_tests::CheckEnumArchive(registry, directory) != 0)
    {
        return 1;
    }

#if defined(ARCHIVE_EXAMPLE_GLM)
    if (archive_reflection_tests::CheckReflectedMath(directory) != 0)
    {
        return 1;
    }
#endif

    if (archive_reflection_tests::CheckNestedArchive(registry, directory) != 0 ||
        archive_reflection_tests::CheckContainerArchive(directory) != 0 ||
        archive_reflection_tests::CheckSerializationPolicy(directory) != 0 ||
        archive_reflection_tests::CheckOptionalFields(registry, directory) != 0 ||
        archive_reflection_tests::CheckSetContainers(directory) != 0 ||
        archive_reflection_tests::CheckSetMutation(directory) != 0 ||
        archive_reflection_tests::CheckMapContainers(directory) != 0 ||
        archive_reflection_tests::CheckMapKeyAccess() != 0 ||
        archive_reflection_tests::CheckMapMutation(directory) != 0 ||
        archive_reflection_tests::CheckObjectMapAccess() != 0 ||
        archive_reflection_tests::CheckObjectMapArchive(directory) != 0)
    {
        return 1;
    }

    std::cout << "All reflection archive checks passed.\n";
    return 0;
}