#include "TestSupport.h"
#include "EngineComponents.gen.h"

#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "Components/TransformComponent.h"
#include "Components/WorldTransformComponent.h"
#include "Adapters/GlmArchiveAdapter.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

namespace archive_engine_component_tests
{
    namespace
    {
        bool SameTransform(const Engine::TransformComponent& lhs, const Engine::TransformComponent& rhs)
        {
            const auto sameVector = [](const glm::vec3& a, const glm::vec3& b)
                {
                    return a.x == b.x && a.y == b.y && a.z == b.z;
                };

            return sameVector(lhs.position, rhs.position) && sameVector(lhs.scale, rhs.scale) &&
                lhs.rotation.x == rhs.rotation.x && lhs.rotation.y == rhs.rotation.y &&
                lhs.rotation.z == rhs.rotation.z && lhs.rotation.w == rhs.rotation.w;
        }

        bool SameMatrix(const glm::mat4& lhs, const glm::mat4& rhs)
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
    }

    int CheckEngineTransformComponents(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        const auto* type = registry.FindType<Engine::TransformComponent>();
        const auto* worldType = registry.FindType<Engine::WorldTransformComponent>();

        if (type == nullptr || worldType == nullptr)
        {
            return Fail("Engine transform type lookup failed.");
        }

        const auto* position = type->FindProperty("position");
        const auto* rotation = type->FindProperty("rotation");
        const auto* worldMatrix = worldType->FindProperty("worldMatrix");

        if (position == nullptr || rotation == nullptr || worldMatrix == nullptr)
        {
            return Fail("Engine transform property lookup failed.");
        }

        const auto* metadata = worldMatrix->Metadata.Find("ReadOnly");
        const auto* readOnly = metadata ? std::get_if<bool>(metadata) : nullptr;

        if (readOnly == nullptr || !*readOnly)
        {
            return Fail("Engine world transform metadata lookup failed.");
        }

        Engine::TransformComponent source;

        if (source.rotation.x != 0.0f || source.rotation.y != 0.0f ||
            source.rotation.z != 0.0f || source.rotation.w != 1.0f)
        {
            return Fail("Engine default rotation is not identity.");
        }

        const auto object = reflection::ObjectView::From(source);
        const glm::vec3 desiredPosition{ 1.0f, 2.0f, 3.0f };
        const auto desiredRotation = glm::quat::wxyz(0.8f, 0.0f, 0.0f, 0.6f);

        if (position->TryWriteValue(object, reflection::ValueView::From(desiredPosition)) !=
            reflection::PropertyAccessError::None ||
            rotation->TryWriteValue(object, reflection::ValueView::From(desiredRotation)) !=
            reflection::PropertyAccessError::None)
        {
            return Fail("Generated engine transform property writing failed.");
        }

        source.scale = glm::vec3{ 2.0f, 3.0f, 4.0f };
        Engine::WorldTransformComponent sourceWorld;

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                sourceWorld.worldMatrix[column][row] = static_cast<float>(column * 4 + row + 1);
            }
        }

        archive_reflection::PropertyCodecs codecs;

        if (!codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3) ||
            !codecs.Register<glm::quat>(archive_glm::WriteQuat, archive_glm::ReadQuat) ||
            !codecs.Register<glm::mat4>(archive_glm::WriteMat4, archive_glm::ReadMat4))
        {
            return Fail("Engine transform codec registration failed.");
        }

        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Transform", *type, object, &registry, &codecs) ||
            !archive_reflection::WriteObject(
                writer, "WorldTransform", *worldType, reflection::ObjectView::From(sourceWorld), &registry, &codecs))
        {
            return Fail("Engine transform writing failed.");
        }

        const auto path = directory / "engine-transform-components.json";
        archive::JsonArchive reader;
        Engine::TransformComponent restored;
        Engine::WorldTransformComponent restoredWorld;

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Transform", *type, restored, &registry, &codecs) ||
            !archive_reflection::ReadObject(
                reader, "WorldTransform", *worldType, restoredWorld, &registry, &codecs) ||
            !SameTransform(restored, source) || !SameMatrix(restoredWorld.worldMatrix, sourceWorld.worldMatrix))
        {
            return Fail("Engine transform round-trip failed.");
        }

        if (!reader.BeginReadObject("WorldTransform"))
        {
            return Fail("Cannot inspect engine matrix layout.");
        }

        std::array<float, 16> matrixElements{};
        const auto matrixRead = reader.ReadArray("worldMatrix", matrixElements);
        const auto matrixEnd = reader.EndObject();

        if (!matrixRead || !matrixEnd ||
            matrixElements[1] != sourceWorld.worldMatrix[0][1] ||
            matrixElements[4] != sourceWorld.worldMatrix[1][0])
        {
            return Fail("Engine matrix column order changed.");
        }

        archive::JsonArchive failureReader;
        const std::string failureJson =
            R"({"Transform":{"position":[9,8,7],"rotation":[0,0,1]},)"
            R"("WorldTransform":{"worldMatrix":[1,2]}})";

        if (!failureReader.Parse(failureJson))
        {
            return Fail("Cannot prepare engine transform reading failure.");
        }

        const auto beforeTransform = restored;
        const auto beforeWorld = restoredWorld;
        const auto transformFailure = archive_reflection::ReadObject(
            failureReader, "Transform", *type, restored, &registry, &codecs);
        const auto worldFailure = archive_reflection::ReadObject(
            failureReader, "WorldTransform", *worldType, restoredWorld, &registry, &codecs);

        if (transformFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
            transformFailure.Field != "rotation" || !SameTransform(restored, beforeTransform) ||
            worldFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
            worldFailure.Field != "worldMatrix" ||
            !SameMatrix(restoredWorld.worldMatrix, beforeWorld.worldMatrix))
        {
            return Fail("Failed engine transform reading changed the destination.");
        }

        std::string beforeWriteFailure;

        if (!writer.ToJson(beforeWriteFailure))
        {
            return Fail("Cannot capture the engine transform document.");
        }

        auto invalid = source;
        invalid.position.x = std::numeric_limits<float>::infinity();
        const auto writeFailure = archive_reflection::WriteObject(
            writer, "Transform", *type, reflection::ObjectView::From(invalid), &registry, &codecs);
        std::string afterWriteFailure;

        if (writeFailure.Code != archive::ArchiveErrorCode::InvalidValue || writeFailure.Field != "position" ||
            !writer.ToJson(afterWriteFailure) || afterWriteFailure != beforeWriteFailure)
        {
            return Fail("Failed engine transform writing changed the document.");
        }

        std::cout << "Engine transform generation checks passed.\n";
        std::cout << "Engine transform archive checks passed.\n";
        std::cout << "Engine transform failure checks passed.\n";
        return 0;
    }
}
