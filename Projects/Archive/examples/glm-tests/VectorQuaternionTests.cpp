#include "TestSupport.h"
#include "GlmTestData.h"

#include <archive/JsonArchive.h>

#include <filesystem>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string>

namespace archive_glm_tests
{
    int CheckVectorQuaternion(const std::filesystem::path& directory)
    {
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

        std::cout << beforeFailure << '\n';
        std::cout << "GLM archive checks passed.\n";
        std::cout << "GLM archive failure checks passed.\n";
        return 0;
    }
}
