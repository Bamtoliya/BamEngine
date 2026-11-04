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
        std::array<std::uint32_t, 4> BoneIndices{ 0, 0, 0, 0 };
        std::array<float, 4> BoneWeights{ 0.f, 0.f, 0.f, 0.f };
    };

    struct TextureBindingInfo
    {
        std::string Path;          // 파일 경로 또는 임베디드 인덱스 ("*0")
        bool IsEmbedded = false;   // GLB/FBX 내부에 포함된 텍스처인지 여부
        std::vector<uint8_t> EmbeddedData; // 임베디드 이미지 파일 바이너리 (PNG, JPG 바이트)
    };
    struct MaterialData
    {
        std::string Name;
        std::array<float, 4> BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        float MetallicFactor = 0.f;
        float RoughnessFactor = 0.5f;
        TextureBindingInfo BaseColorTexture;          // Slot 0 (Albedo / Base Color)
        TextureBindingInfo NormalTexture;             // Slot 1 (Normal)
        TextureBindingInfo MetallicRoughnessTexture;  // Slot 2 (Metallic-Roughness / ORM)
        TextureBindingInfo EmissiveTexture;           // Slot 3 (Emissive)
    };

    struct MeshData
    {
        std::string Name;
        std::uint32_t MaterialIndex = 0;
        bool HasSkinning = false;
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

    struct BoneData
    {
        std::string Name;
        std::int32_t ParentIndex = -1;

        std::array<float, 16> OffsetMatrix{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };

        std::array<float, 16> LocalTransform{
            1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f
        };
    };

    struct VectorKeyData
    {
        float Time = 0.f;
        std::array<float, 3> Value{};
    };

    struct QuatKeyData
    {
        float Time = 0.f;
        // GLM quat 생성자(w, x, y, z)나 필드에 맞추기 위해 (x, y, z, w) 순서로 저장합니다.
        std::array<float, 4> Value{ 0.f, 0.f, 0.f, 1.f };
    };

    struct TrackData
    {
        std::string BoneName;
        std::vector<VectorKeyData> PositionKeys;
        std::vector<QuatKeyData> RotationKeys;
        std::vector<VectorKeyData> ScaleKeys;
    };

    struct AnimationData
    {
        std::string Name;
        float Duration = 0.f;
        float TicksPerSecond = 25.f;
        std::vector<TrackData> Tracks;
    };

    struct ModelData
    {
        std::filesystem::path SourcePath;
        std::uint32_t MaterialCount = 0;
        std::vector<MaterialData> Materials;
        std::vector<MeshData> Meshes;
        std::vector<NodeData> Nodes;
        std::vector<BoneData> Bones;
        std::vector<AnimationData> Animations;
    };

    struct Options
    {
        bool FlipUVs = true;
        bool ConvertToLeftHanded = false;
        float UniformScale = 1.0f;
        std::array<float, 3> RotationOffset{ 0.0f, 0.0f, 0.0f };
        bool FlipWindingOrder = false;
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