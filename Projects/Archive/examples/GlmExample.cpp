#include "Adapters/GlmArchiveAdapter.h"
#include <archive/JsonArchive.h>

#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <array>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    bool SameVec3(const glm::vec3& lhs, const glm::vec3& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
    }

    bool SameQuat(const glm::quat& lhs, const glm::quat& rhs)
    {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
    }

    bool SameMat4(const glm::mat4& lhs, const glm::mat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (lhs[column][row] != rhs[column][row])
                {
                    return false;
                }
            }
        }

        return true;
    }
    struct TransformData
    {
        glm::vec3 Position{ 0.0f };
        glm::quat Rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 Scale{ 1.0f };
    };

    bool SameTransform(const TransformData& lhs, const TransformData& rhs)
    {
        return SameVec3(lhs.Position, rhs.Position) &&
            SameQuat(lhs.Rotation, rhs.Rotation) && SameVec3(lhs.Scale, rhs.Scale);
    }

    archive::ArchiveResult WriteTransformFields(archive::GlazeArchiveBase& ar, const TransformData& value)
    {
        auto result = archive_glm::WriteVec3(ar, "Position", value.Position);

        if (!result)
        {
            return result;
        }

        result = archive_glm::WriteQuat(ar, "Rotation", value.Rotation);

        if (!result)
        {
            return result;
        }

        return archive_glm::WriteVec3(ar, "Scale", value.Scale);
    }

    archive::ArchiveResult ReadTransformFields(archive::GlazeArchiveBase& ar, TransformData& value)
    {
        auto result = archive_glm::ReadVec3(ar, "Position", value.Position);

        if (!result)
        {
            return result;
        }

        result = archive_glm::ReadQuat(ar, "Rotation", value.Rotation);

        if (!result)
        {
            return result;
        }

        return archive_glm::ReadVec3(ar, "Scale", value.Scale);
    }
}

int main()
{
#pragma region vec, quat

    const glm::vec3 sourcePosition{ 1.0f, 2.0f, 3.0f };

    glm::quat sourceRotation{};
    sourceRotation.x = 0.0f;
    sourceRotation.y = 0.0f;
    sourceRotation.z = 0.6f;
    sourceRotation.w = 0.8f;

    archive::JsonArchive writer;

    if (!archive_glm::WriteVec3(writer, "Position", sourcePosition) ||
        !archive_glm::WriteQuat(writer, "Rotation", sourceRotation))
    {
        return Fail("GLM writing failed.");
    }

    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return Fail("Cannot create the GLM test directory.");
    }

    const auto path = directory / "glm.json";

    if (!writer.SaveToFile(path))
    {
        return Fail("GLM file saving failed.");
    }

    archive::JsonArchive reader;
    glm::vec3 restoredPosition{};
    glm::quat restoredRotation{};

    if (!reader.LoadFromFile(path) ||
        !archive_glm::ReadVec3(reader, "Position", restoredPosition) ||
        !archive_glm::ReadQuat(reader, "Rotation", restoredRotation))
    {
        return Fail("GLM file reading failed.");
    }

    if (!SameVec3(restoredPosition, sourcePosition) || !SameQuat(restoredRotation, sourceRotation))
    {
        return Fail("Restored GLM values differ.");
    }

    archive::JsonArchive failureReader;

    if (!failureReader.Parse(
        R"({"ShortVec":[1,2],"BadVec":[1,"bad",3],"ShortQuat":[0,0,1],"BadQuat":[0,0,"bad",1]})"))
    {
        return Fail("Cannot prepare GLM failure checks.");
    }

    glm::vec3 unchangedPosition = sourcePosition;
    glm::quat unchangedRotation = sourceRotation;

    for (const char* key : { "ShortVec", "BadVec" })
    {
        const auto result = archive_glm::ReadVec3(failureReader, key, unchangedPosition);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
            !SameVec3(unchangedPosition, sourcePosition))
        {
            return Fail("Failed vector reading changed the destination.");
        }
    }

    for (const char* key : { "ShortQuat", "BadQuat" })
    {
        const auto result = archive_glm::ReadQuat(failureReader, key, unchangedRotation);

        if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
            !SameQuat(unchangedRotation, sourceRotation))
        {
            return Fail("Failed quaternion reading changed the destination.");
        }
    }

    std::string beforeFailure;

    if (!writer.ToJson(beforeFailure))
    {
        return Fail("Cannot capture the GLM document.");
    }

    const glm::vec3 invalidPosition{ 1.0f, std::numeric_limits<float>::infinity(), 3.0f };
    auto result = archive_glm::WriteVec3(writer, "Position", invalidPosition);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue)
    {
        return Fail("Non-finite vector check failed.");
    }

    glm::quat invalidRotation = sourceRotation;
    invalidRotation.w = std::numeric_limits<float>::quiet_NaN();
    result = archive_glm::WriteQuat(writer, "Rotation", invalidRotation);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue)
    {
        return Fail("Non-finite quaternion check failed.");
    }

    std::string afterFailure;

    if (!writer.ToJson(afterFailure) || afterFailure != beforeFailure)
    {
        return Fail("Failed GLM writing changed the document.");
    }
#pragma endregion

#pragma region matrix
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
    result = archive_glm::WriteMat4(matrixWriter, "WorldMatrix", invalidMatrix);

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
#pragma endregion

#pragma region Format
    const std::string formatInput =
        R"({"Four":[1,2,3,4],"Five":[1,2,3,4,5],"Mixed":[1,2,3,4,"tail"],)"
        R"("Label":"문자열 [1,2,3,4,5], \"quoted\" \\ path","Embedded":"A\u0000한글",)"
        R"("Nested":{"Five":[-1,-2,-3,-4,-5]},"Items":[{"Name":"첫 번째"},{"Name":"두 번째"}]})";

    archive::JsonArchive formatWriter;
    std::string formattedJson;

    if (!formatWriter.Parse(formatInput) || !formatWriter.ToJson(formattedJson))
    {
        return Fail("JSON formatting failed.");
    }

    const std::string expectedFiveLayout =
        "\"Five\": [\n"
        "        1, 2, 3, 4,\n"
        "        5\n"
        "    ]";

    if (formattedJson.find("\"Four\": [1, 2, 3, 4]") == std::string::npos ||
        formattedJson.find(expectedFiveLayout) == std::string::npos ||
        formattedJson.find("\"Mixed\": [1, 2, 3, 4, \"tail\"]") == std::string::npos)
    {
        return Fail("Numeric array layout checks failed.");
    }

    if (formattedJson.find('\0') != std::string::npos ||
        formattedJson.find(R"(A\u0000한글)") == std::string::npos)
    {
        return Fail("Formatting damaged control-character escaping.");
    }

    archive::JsonArchive formatReader;
    std::string reformattedJson;
    std::string originalLabel;
    std::string restoredLabel;
    std::string originalEmbedded;
    std::string restoredEmbedded;

    if (!formatReader.Parse(formattedJson) ||
        !formatReader.ToJson(reformattedJson) ||
        !formatWriter.Read("Label", originalLabel) ||
        !formatReader.Read("Label", restoredLabel) ||
        !formatWriter.Read("Embedded", originalEmbedded) ||
        !formatReader.Read("Embedded", restoredEmbedded))
    {
        return Fail("Formatted JSON reading failed.");
    }

    if (reformattedJson != formattedJson ||
        restoredLabel != originalLabel ||
        restoredEmbedded != originalEmbedded)
    {
        return Fail("Formatting changed the JSON values.");
    }

    std::cout << "JSON layout checks passed.\n";
#pragma endregion

#pragma region Transform

    TransformData sourceTransform;
    sourceTransform.Position = sourcePosition;
    sourceTransform.Rotation = sourceRotation;
    sourceTransform.Scale = glm::vec3{ 2.0f, 3.0f, 4.0f };

    archive::JsonArchive transformWriter;

    if (!transformWriter.WriteObject("Transform", sourceTransform, WriteTransformFields))
    {
        return Fail("Transform writing failed.");
    }

    const auto transformPath = directory / "transform.json";

    if (!transformWriter.SaveToFile(transformPath))
    {
        return Fail("Transform file saving failed.");
    }

    archive::JsonArchive transformReader;
    TransformData restoredTransform;

    if (!transformReader.LoadFromFile(transformPath) ||
        !transformReader.ReadObject("Transform", restoredTransform, ReadTransformFields) ||
        !SameTransform(restoredTransform, sourceTransform))
    {
        return Fail("Transform file round-trip failed.");
    }

    std::string transformJson;

    if (!transformWriter.ToJson(transformJson))
    {
        return Fail("Cannot capture the Transform document.");
    }

    TransformData invalidTransform = sourceTransform;
    invalidTransform.Position = glm::vec3{ 9.0f, 8.0f, 7.0f };
    invalidTransform.Rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
    invalidTransform.Scale.z = std::numeric_limits<float>::infinity();

    const auto writeResult = transformWriter.WriteObject("Transform", invalidTransform, WriteTransformFields);

    if (writeResult.Code != archive::ArchiveErrorCode::InvalidValue || writeResult.Field != "Scale")
    {
        return Fail("Invalid Transform writing check failed.");
    }

    std::string afterTransformFailure;

    if (!transformWriter.ToJson(afterTransformFailure) || afterTransformFailure != transformJson)
    {
        return Fail("Failed Transform writing changed the document.");
    }

    archive::JsonArchive transformFailureReader;
    const std::string invalidTransformJson =
        R"({"Transform":{"Position":[9,8,7],"Rotation":[0,0,0,1],"Scale":[1,"bad",1]}})";

    if (!transformFailureReader.Parse(invalidTransformJson))
    {
        return Fail("Cannot prepare the Transform reading failure.");
    }

    TransformData unchangedTransform = sourceTransform;
    const auto readResult =
        transformFailureReader.ReadObject("Transform", unchangedTransform, ReadTransformFields);

    if (readResult.Code != archive::ArchiveErrorCode::InvalidValue ||
        readResult.Field != "Scale" || !SameTransform(unchangedTransform, sourceTransform))
    {
        return Fail("Failed Transform reading changed the destination.");
    }

    std::cout << transformJson << '\n';
    std::cout << "Transform archive checks passed.\n";
    std::cout << "Transform failure checks passed.\n";

#pragma endregion

    std::cout << beforeFailure << '\n';
    std::cout << "GLM archive checks passed.\n";
    std::cout << "GLM archive failure checks passed.\n";
    return 0;
}