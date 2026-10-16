#pragma once

#include <archive/ArchiveInterface.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <string_view>

namespace archive_glm
{
    [[nodiscard]] inline archive::ArchiveResult WriteVec2(archive::IArchive& ar, std::string_view key, const glm::vec2& value)
    {
        return ar.WriteArray(key, std::array<float, 2>{ value.x, value.y });
    }
    [[nodiscard]] inline archive::ArchiveResult ReadVec2(const archive::IArchive& ar, std::string_view key, glm::vec2& value)
    {
        std::array<float, 2> components{};
        const auto result = ar.ReadArray(key, components);
        if (result) value = glm::vec2(components[0], components[1]);
        return result;
    }
    [[nodiscard]] inline archive::ArchiveResult WriteVec4(archive::IArchive& ar, std::string_view key, const glm::vec4& value)
    {
        return ar.WriteArray(key, std::array<float, 4>{ value.x, value.y, value.z, value.w });
    }
    [[nodiscard]] inline archive::ArchiveResult ReadVec4(const archive::IArchive& ar, std::string_view key, glm::vec4& value)
    {
        std::array<float, 4> components{};
        const auto result = ar.ReadArray(key, components);
        if (result) value = glm::vec4(components[0], components[1], components[2], components[3]);
        return result;
    }
    [[nodiscard]] inline archive::ArchiveResult WriteVec3(archive::IArchive& ar, std::string_view key, const glm::vec3& value)
    {
        return ar.WriteArray(key, std::array<float, 3>{ value.x, value.y, value.z });
    }

    [[nodiscard]] inline archive::ArchiveResult ReadVec3(const archive::IArchive& ar, std::string_view key, glm::vec3& value)
    {
        std::array<float, 3> components{};
        const auto result = ar.ReadArray(key, components);

        if (!result)
        {
            return result;
        }

        value = glm::vec3{ components[0], components[1], components[2] };
        return {};
    }

    [[nodiscard]] inline archive::ArchiveResult WriteQuat(archive::IArchive& ar, std::string_view key, const glm::quat& value)
    {
        return ar.WriteArray(key, std::array<float, 4>{ value.x, value.y, value.z, value.w });
    }

    [[nodiscard]] inline archive::ArchiveResult ReadQuat(const archive::IArchive& ar, std::string_view key, glm::quat& value)
    {
        std::array<float, 4> components{};
        const auto result = ar.ReadArray(key, components);

        if (!result)
        {
            return result;
        }

        glm::quat candidate{};
        candidate.x = components[0];
        candidate.y = components[1];
        candidate.z = components[2];
        candidate.w = components[3];

        value = candidate;
        return {};
    }

    [[nodiscard]] inline archive::ArchiveResult WriteMat4(archive::IArchive& ar, std::string_view key, const glm::mat4& value)
    {
        std::array<float, 16> components{};

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                components[column * 4 + row] = value[column][row];
            }
        }

        return ar.WriteArray(key, components);
    }

    [[nodiscard]] inline archive::ArchiveResult ReadMat4(const archive::IArchive& ar, std::string_view key, glm::mat4& value)
    {
        std::array<float, 16> components{};
        const auto result = ar.ReadArray(key, components);

        if (!result)
        {
            return result;
        }

        glm::mat4 candidate{ 1.0f };

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                candidate[column][row] = components[column * 4 + row];
            }
        }

        value = candidate;
        return {};
    }
}
