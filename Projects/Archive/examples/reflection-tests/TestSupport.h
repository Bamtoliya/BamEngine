#pragma once

#include <filesystem>
#include <iostream>

namespace reflection
{
    class Registry;
}

namespace archive_reflection_tests
{
    inline int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    // Map
    int CheckMapKeyAccess();
    int CheckMapContainers(const std::filesystem::path& directory);
    int CheckMapMutation(const std::filesystem::path& directory);

    // Object map
    int CheckObjectMapAccess();
    int CheckObjectMapArchive(const std::filesystem::path& directory);

    // Set
    int CheckSetContainers(const std::filesystem::path& directory);
    int CheckSetMutation(const std::filesystem::path& directory);

    int CheckSettingsArchive(reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckNestedArchive(reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckContainerArchive(const std::filesystem::path& directory);
    int CheckSerializationPolicy(const std::filesystem::path& directory);
    int CheckOptionalFields(reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckEnumArchive(reflection::Registry& registry, const std::filesystem::path& directory);

#if defined(ARCHIVE_EXAMPLE_GLM)
    int CheckReflectedMath(const std::filesystem::path& directory);
#endif

}