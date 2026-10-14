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
    namespace
    {
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

        archive::ArchiveResult WriteTransformFields(archive::IArchive& ar, const TransformData& value)
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

        archive::ArchiveResult ReadTransformFields(archive::IArchive& ar, TransformData& value)
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

    int CheckTransform(const std::filesystem::path& directory)
    {
        const glm::vec3 sourcePosition{ 1.0f, 2.0f, 3.0f };

        glm::quat sourceRotation{};
        sourceRotation.x = 0.0f;
        sourceRotation.y = 0.0f;
        sourceRotation.z = 0.6f;
        sourceRotation.w = 0.8f;

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
        return 0;
    }
}
