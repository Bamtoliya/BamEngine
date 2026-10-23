#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Tools::ModelConversion
{
    struct VertexData
    {
        std::array<float, 3> Position{};
        std::array<float, 3> Normal{};
        std::array<float, 2> TexCoord{};
        std::array<float, 3> Tangent{};
        std::array<float, 3> Bitangent{};
        std::array<float, 4> Color{ 1.f, 1.f, 1.f, 1.f };
    };

    struct MeshData
    {
        std::string Name;
        std::uint32_t MaterialIndex = 0;
        std::vector<VertexData> Vertices;
        std::vector<std::uint32_t> Indices;
        std::array<float, 3> BoundsMin{};
        std::array<float, 3> BoundsMax{};
    };

    struct NodeData
    {
        std::string Name;
        std::int32_t ParentIndex = -1;

        // GLM 행렬 생성 시 사용할 수 있는 column-major 순서입니다.
        std::array<float, 16> LocalTransform{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };

        std::vector<std::uint32_t> MeshIndices;
    };

    struct ModelData
    {
        std::filesystem::path SourcePath;
        std::uint32_t MaterialCount = 0;
        std::vector<MeshData> Meshes;
        std::vector<NodeData> Nodes;
    };

    struct Options
    {
        bool FlipUVs = true;
        bool ConvertToLeftHanded = false;
    };

    enum class Error
    {
        None,
        InvalidPath,
        ReadFailed,
        InvalidData,
        UnsupportedFeature,
        LimitExceeded,
        InternalError
    };

    struct Result
    {
        Error Code = Error::None;
        std::string Message;
        std::optional<ModelData> Model;

        explicit operator bool() const noexcept
        {
            return Code == Error::None && Model.has_value();
        }
    };

    [[nodiscard]] Result ConvertStaticModel(const std::filesystem::path& path, const Options& options = {});
}