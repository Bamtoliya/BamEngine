#include "TestSupport.h"

#include <archive/JsonArchive.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#if defined(ARCHIVE_EXAMPLE_GLM)

#include "Adapters/GlmArchiveAdapter.h"

namespace archive_reflection_tests
{
    namespace
    {
        struct MathSettings
        {
            bool Enabled = true;
            glm::vec3 Position{ 1.0f, 2.0f, 3.0f };
            glm::quat Rotation = glm::quat::wxyz(0.8f, 0.0f, 0.0f, 0.6f);
            glm::mat4 Matrix{ 1.0f };
        };
        bool SameMathSettings(const MathSettings& lhs, const MathSettings& rhs)
        {
            const bool samePosition = lhs.Position.x == rhs.Position.x &&
                lhs.Position.y == rhs.Position.y && lhs.Position.z == rhs.Position.z;

            const bool sameRotation = lhs.Rotation.x == rhs.Rotation.x && lhs.Rotation.y == rhs.Rotation.y &&
                lhs.Rotation.z == rhs.Rotation.z && lhs.Rotation.w == rhs.Rotation.w;

            if (lhs.Enabled != rhs.Enabled || !samePosition || !sameRotation)
            {
                return false;
            }

            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (lhs.Matrix[column][row] != rhs.Matrix[column][row])
                    {
                        return false;
                    }
                }
            }

            return true;
        }
    }

    int CheckReflectedMath(const std::filesystem::path& directory)
    {
        archive_reflection::PropertyCodecs codecs;

        if (!codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3) ||
            !codecs.Register<glm::quat>(archive_glm::WriteQuat, archive_glm::ReadQuat) ||
            !codecs.Register<glm::mat4>(archive_glm::WriteMat4, archive_glm::ReadMat4))
        {
            return Fail("Math codec registration failed.");
        }

        if (codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3))
        {
            return Fail("Duplicate math codec registration was accepted.");
        }

        auto type = reflection::TypeInfo::For<MathSettings>("example::MathSettings");
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Enabled>("Enabled", "bool"));
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Position>("Position", "glm::vec3"));
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Rotation>("Rotation", "glm::quat"));
        type.Properties.push_back(reflection::MakeProperty<&MathSettings::Matrix>("Matrix", "glm::mat4"));

        MathSettings source;

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                source.Matrix[column][row] = static_cast<float>(column * 4 + row + 1);
            }
        }

        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Math", type,
            reflection::ObjectView::From(source), nullptr, &codecs))
        {
            return Fail("Reflected math writing failed.");
        }

        const auto path = directory / "reflection-math.json";
        archive::JsonArchive reader;
        MathSettings restored;
        restored.Enabled = false;
        restored.Position = glm::vec3{ -10.0f };
        restored.Rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
        restored.Matrix = glm::mat4{ 0.0f };

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Math", type, restored, nullptr, &codecs) ||
            !SameMathSettings(restored, source))
        {
            return Fail("Reflected math round-trip failed.");
        }

        std::array<float, 4> serializedRotation{};
        std::array<float, 16> serializedMatrix{};

        if (!reader.BeginReadObject("Math") ||
            !reader.ReadArray("Rotation", serializedRotation) ||
            !reader.ReadArray("Matrix", serializedMatrix) || !reader.EndObject())
        {
            return Fail("Cannot inspect serialized math components.");
        }

        const std::array<float, 4> expectedRotation{
            source.Rotation.x, source.Rotation.y, source.Rotation.z, source.Rotation.w
        };

        if (serializedRotation != expectedRotation)
        {
            return Fail("Serialized quaternion XYZW order differs.");
        }

        for (int index = 0; index < 16; ++index)
        {
            if (serializedMatrix[index] != static_cast<float>(index + 1))
            {
                return Fail("Serialized matrix column order differs.");
            }
        }

        using Code = archive::ArchiveErrorCode;

        struct FailureCase
        {
            const char* Json;
            const char* Field;
            Code Expected;
        };

        const FailureCase failures[] = {
            {
                R"({"Math":{"Enabled":false,"Position":[9,"bad",7],"Rotation":[],"Matrix":[]}})",
                "Position", Code::InvalidValue
            },
            {
                R"({"Math":{"Enabled":false,"Position":[9,8,7],"Rotation":[0,0,1],"Matrix":[]}})",
                "Rotation", Code::InvalidValue
            },
            {
                R"({"Math":{"Enabled":false,"Position":[9,8,7],"Rotation":[0,0,0,1],"Matrix":[1,2,3]}})",
                "Matrix", Code::InvalidValue
            },
            {
                R"({"Math":{"Enabled":false,"Position":[9,8,7],"Rotation":[0,0,0,1]}})",
                "Matrix", Code::MissingField
            }
        };

        for (const auto& failure : failures)
        {
            archive::JsonArchive failureReader;

            if (!failureReader.Parse(failure.Json))
            {
                return Fail("Cannot prepare reflected math failure.");
            }

            MathSettings unchanged = source;
            const auto result = archive_reflection::ReadObject(
                failureReader, "Math", type, unchanged, nullptr, &codecs);

            if (result.Code != failure.Expected || result.Field != failure.Field ||
                !SameMathSettings(unchanged, source))
            {
                return Fail("Failed math codec reading changed the destination.");
            }
        }

        std::string beforeFailure;

        if (!writer.ToJson(beforeFailure))
        {
            return Fail("Cannot capture the math document.");
        }

        MathSettings invalid = source;
        invalid.Enabled = false;
        invalid.Position = glm::vec3{ 9.0f, 8.0f, 7.0f };
        invalid.Rotation = glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f);
        invalid.Matrix[3][3] = std::numeric_limits<float>::infinity();

        const auto result = archive_reflection::WriteObject(
            writer, "Math", type, reflection::ObjectView::From(invalid), nullptr, &codecs);

        std::string afterFailure;

        if (result.Code != Code::InvalidValue || result.Field != "Matrix" ||
            !writer.ToJson(afterFailure) || afterFailure != beforeFailure)
        {
            return Fail("Failed math codec writing changed the document.");
        }

        std::cout << beforeFailure << '\n';
        std::cout << "Reflection math codec checks passed.\n";
        std::cout << "Reflection math codec failure checks passed.\n";
        return 0;
    }
}

#endif