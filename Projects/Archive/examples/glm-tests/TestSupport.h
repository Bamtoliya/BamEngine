#pragma once

#include <filesystem>
#include <iostream>

namespace reflection
{
    class Registry;
}

namespace archive_glm_tests
{
    inline int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    int CheckVectorQuaternion(const std::filesystem::path& directory);
    int CheckMatrix(const std::filesystem::path& directory);
    int CheckJsonFormatting();
    int CheckTransform(const std::filesystem::path& directory);
}
