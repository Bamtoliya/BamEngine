#include "glm-tests/TestSupport.h"

#include <filesystem>
#include <iostream>
#include <system_error>

int main()
{
    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return archive_glm_tests::Fail("Cannot create the GLM test directory.");
    }

    if (archive_glm_tests::CheckVectorQuaternion(directory) != 0 ||
        archive_glm_tests::CheckMatrix(directory) != 0 ||
        archive_glm_tests::CheckJsonFormatting() != 0 ||
        archive_glm_tests::CheckTransform(directory) != 0)
    {
        return 1;
    }

    std::cout << "All GLM archive checks passed.\n";
    return 0;
}
