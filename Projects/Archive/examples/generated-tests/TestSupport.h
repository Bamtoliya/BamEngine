#pragma once

#include <filesystem>
#include <iostream>

namespace reflection
{
    class Registry;
}

namespace archive_generated_tests
{
    inline int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    int CheckGeneratedProfile(const reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckGeneratedContainers(const reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckAutomaticMaps(const reflection::Registry& registry);
    int CheckEnumContainers(const reflection::Registry& registry);
    int CheckGeneratedObjectArray(const reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckGeneratedFixedArray(const reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckFixedObjectArrays(const reflection::Registry& registry);
    int CheckNestedContainers(const reflection::Registry& registry);
    int CheckInheritedObjects(const reflection::Registry& registry);
    int CheckGeneratedContainerTraversal(const reflection::Registry& registry);
}
