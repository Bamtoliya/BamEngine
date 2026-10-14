#include "TestSupport.h"
#include "GlmTestData.h"

#include <archive/JsonArchive.h>

#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>
#include <array>
#include <cstddef>

namespace archive_glm_tests
{
    int CheckMatrix(const std::filesystem::path& directory)
    {
        glm::mat4 sourceMatrix{ 1.0f };

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                sourceMatrix[column][row] = static_cast<float>(column * 4 + row + 1);
            }
        }

        archive::JsonArchive matrixWriter;

        if (!archive_glm::WriteMat4(matrixWriter, "WorldMatrix", sourceMatrix))
        {
            return Fail("Matrix writing failed.");
        }

        const auto matrixPath = directory / "glm-matrix.json";

        if (!matrixWriter.SaveToFile(matrixPath))
        {
            return Fail("Matrix file saving failed.");
        }

        archive::JsonArchive matrixReader;
        glm::mat4 restoredMatrix{ 1.0f };
        std::array<float, 16> serializedMatrix{};

        if (!matrixReader.LoadFromFile(matrixPath) ||
            !archive_glm::ReadMat4(matrixReader, "WorldMatrix", restoredMatrix) ||
            !matrixReader.ReadArray("WorldMatrix", serializedMatrix) ||
            !SameMat4(restoredMatrix, sourceMatrix))
        {
            return Fail("Matrix file round-trip failed.");
        }

        for (std::size_t index = 0; index < serializedMatrix.size(); ++index)
        {
            if (serializedMatrix[index] != static_cast<float>(index + 1))
            {
                return Fail("Serialized matrix column order differs.");
            }
        }

        archive::JsonArchive matrixFailureReader;
        const std::string matrixFailureJson =
            R"({"Short":[1,2,3],"Long":[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17],)"
            R"("Mixed":[1,2,3,4,5,6,7,"bad",9,10,11,12,13,14,15,16]})";

        if (!matrixFailureReader.Parse(matrixFailureJson))
        {
            return Fail("Cannot prepare matrix failure checks.");
        }

        glm::mat4 unchangedMatrix = sourceMatrix;

        for (const char* key : { "Short", "Long", "Mixed" })
        {
            const auto matrixResult = archive_glm::ReadMat4(matrixFailureReader, key, unchangedMatrix);

            if (matrixResult.Code != archive::ArchiveErrorCode::InvalidValue ||
                !SameMat4(unchangedMatrix, sourceMatrix))
            {
                return Fail("Failed matrix reading changed the destination.");
            }
        }

        std::string beforeMatrixFailure;

        if (!matrixWriter.ToJson(beforeMatrixFailure))
        {
            return Fail("Cannot capture the matrix document.");
        }

        glm::mat4 invalidMatrix = sourceMatrix;
        invalidMatrix[2][1] = std::numeric_limits<float>::infinity();
        const auto result = archive_glm::WriteMat4(matrixWriter, "WorldMatrix", invalidMatrix);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue)
        {
            return Fail("Non-finite matrix check failed.");
        }

        std::string afterMatrixFailure;

        if (!matrixWriter.ToJson(afterMatrixFailure) || afterMatrixFailure != beforeMatrixFailure)
        {
            return Fail("Failed matrix writing changed the document.");
        }

        const std::string expectedMatrixLayout =
            "\"WorldMatrix\": [\n"
            "        1, 2, 3, 4,\n"
            "        5, 6, 7, 8,\n"
            "        9, 10, 11, 12,\n"
            "        13, 14, 15, 16\n"
            "    ]";

        if (beforeMatrixFailure.find(expectedMatrixLayout) == std::string::npos)
        {
            return Fail("Matrix JSON layout differs.");
        }

        std::cout << beforeMatrixFailure << '\n';
        std::cout << "GLM matrix checks passed.\n";
        std::cout << "GLM matrix failure checks passed.\n";
        return 0;
    }
}
