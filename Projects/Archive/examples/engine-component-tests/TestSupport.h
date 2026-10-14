#pragma once

#include <filesystem>
#include <iostream>

namespace reflection
{
    class Registry;
}

namespace archive_engine_component_tests
{
    inline int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    int CheckEngineNameComponent(const reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckEngineValueComponents(const reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckEngineTransformComponents(const reflection::Registry& registry, const std::filesystem::path& directory);
    int CheckEngineComponentBindings(const reflection::Registry& registry);
}
