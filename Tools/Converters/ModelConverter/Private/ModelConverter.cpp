#include "ModelConverter.h"

#include <meshoptimizer.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "../../Common/DdcCommon.h"

namespace Tools::ModelConversion
{
    namespace
    {
        constexpr uint32_t MODEL_DDC_MAGIC = 0x43444D42; // "BMDC"
        constexpr uint32_t MODEL_DDC_VERSION = 4;

        struct LegacyVertexData
        {
            std::array<float, 3> Position;
            std::array<float, 3> Normal;
            std::array<float, 2> TexCoord;
            std::array<float, 3> Tangent;
            std::array<float, 3> Bitangent;
            std::array<float, 4> Color;
            std::array<std::uint32_t, 4> BoneIndices;
            std::array<float, 4> BoneWeights;
        };

        void WriteString(std::ostream& os, const std::string& str)
        {
            const uint32_t len = static_cast<uint32_t>(str.size());
            os.write(reinterpret_cast<const char*>(&len), sizeof(len));
            if (len > 0) os.write(str.data(), len);
        }

        bool ReadString(std::istream& is, std::string& str)
        {
            uint32_t len = 0;
            is.read(reinterpret_cast<char*>(&len), sizeof(len));
            if (!is || len > 64 * 1024) return false;
            str.resize(len);
            if (len > 0) is.read(str.data(), len);
            return is.good();
        }

        template<typename T>
        void WritePodVector(std::ostream& os, const std::vector<T>& vec)
        {
            const uint32_t count = static_cast<uint32_t>(vec.size());
            os.write(reinterpret_cast<const char*>(&count), sizeof(count));
            if (count > 0) os.write(reinterpret_cast<const char*>(vec.data()), sizeof(T) * count);
        }

        template<typename T>
        bool ReadPodVector(std::istream& is, std::vector<T>& vec)
        {
            uint32_t count = 0;
            is.read(reinterpret_cast<char*>(&count), sizeof(count));
            if (!is || count > 50'000'000) return false;
            vec.resize(count);
            if (count > 0) is.read(reinterpret_cast<char*>(vec.data()), sizeof(T) * count);
            return is.good();
        }

        void WriteMesh(std::ostream& os, const MeshData& mesh)
        {
            WriteString(os, mesh.Name);
            os.write(reinterpret_cast<const char*>(&mesh.MaterialIndex), sizeof(mesh.MaterialIndex));
            os.write(reinterpret_cast<const char*>(&mesh.HasSkinning), sizeof(mesh.HasSkinning));
            os.write(reinterpret_cast<const char*>(mesh.BoundsMin.data()), sizeof(float) * 3);
            os.write(reinterpret_cast<const char*>(mesh.BoundsMax.data()), sizeof(float) * 3);
            WritePodVector(os, mesh.Vertices);
            WritePodVector(os, mesh.Indices);

            const uint32_t lodCount = static_cast<uint32_t>(mesh.LODs.size());
            os.write(reinterpret_cast<const char*>(&lodCount), sizeof(lodCount));
            for (const auto& lod : mesh.LODs) WriteMesh(os, lod);
        }

        bool ReadMesh(std::istream& is, MeshData& mesh, uint32_t version, bool allowLODs = true)
        {
            if (!ReadString(is, mesh.Name)) return false;
            is.read(reinterpret_cast<char*>(&mesh.MaterialIndex), sizeof(mesh.MaterialIndex));
            is.read(reinterpret_cast<char*>(&mesh.HasSkinning), sizeof(mesh.HasSkinning));
            is.read(reinterpret_cast<char*>(mesh.BoundsMin.data()), sizeof(float) * 3);
            is.read(reinterpret_cast<char*>(mesh.BoundsMax.data()), sizeof(float) * 3);
            if (version == 3)
            {
                std::vector<LegacyVertexData> legacy;
                if (!ReadPodVector(is, legacy)) return false;
                mesh.Vertices.resize(legacy.size());
                for (std::size_t i = 0; i < legacy.size(); ++i)
                {
                    const auto& src = legacy[i];
                    auto& dst = mesh.Vertices[i];
                    dst.Position = src.Position;
                    dst.Normal = src.Normal;
                    dst.TexCoord = src.TexCoord;
                    dst.Tangent = src.Tangent;
                    dst.Bitangent = src.Bitangent;
                    dst.Color = src.Color;
                    dst.BoneIndices = src.BoneIndices;
                    dst.BoneWeights = src.BoneWeights;
                    dst.TexCoord1 = src.TexCoord;
                }
            }
            else if (!ReadPodVector(is, mesh.Vertices)) return false;
            if (!ReadPodVector(is, mesh.Indices)) return false;

            uint32_t lodCount = 0;
            is.read(reinterpret_cast<char*>(&lodCount), sizeof(lodCount));
            if (!is || lodCount > (allowLODs ? 16u : 0u)) return false;
            mesh.LODs.resize(lodCount);
            for (uint32_t i = 0; i < lodCount; ++i)
            {
                if (!ReadMesh(is, mesh.LODs[i], version, false)) return false;
            }
            return is.good();
        }

        std::filesystem::path GetModelDdcPath(const std::filesystem::path& sourcePath, const Options& options)
        {
            std::error_code ec;
            if (!std::filesystem::exists(sourcePath, ec)) return {};

            Tools::Ddc::DdcHasher hasher;
            const auto u8Path = sourcePath.u8string();
            hasher.Combine(u8Path.data(), u8Path.size());
            hasher.CombineFileMetadata(sourcePath);
            hasher.CombinePod(options.FlipUVs);
            hasher.CombinePod(options.ConvertToLeftHanded);
            hasher.CombinePod(options.UniformScale);
            hasher.CombinePod(options.FlipWindingOrder);
            hasher.CombinePod(options.FlipPitch);
            hasher.CombinePod(options.FlipHeading);
            hasher.CombinePod(options.GenerateLODs);
            hasher.CombinePod(options.LODCount);
            hasher.CombinePod(options.TargetError);
            for (float rate : options.LODReductionRates) hasher.CombinePod(rate);
            hasher.CombineString("model_ddc_v4");

            return hasher.ResolvePath(Tools::Ddc::FindDdcDirectory("models"));
        }

        bool ReadModelData(const std::filesystem::path& path, ModelData& model)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) return false;

            uint32_t magic = 0, version = 0;
            file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            file.read(reinterpret_cast<char*>(&version), sizeof(version));
            if (magic != MODEL_DDC_MAGIC || (version != 3 && version != MODEL_DDC_VERSION)) return false;

            std::string sourcePathStr;
            if (!ReadString(file, sourcePathStr)) return false;
            model.SourcePath = std::filesystem::path(std::u8string(sourcePathStr.begin(), sourcePathStr.end()));
            file.read(reinterpret_cast<char*>(&model.MaterialCount), sizeof(model.MaterialCount));

            // 1. Materials
            uint32_t matCount = 0;
            file.read(reinterpret_cast<char*>(&matCount), sizeof(matCount));
            model.Materials.resize(matCount);
            for (auto& mat : model.Materials)
            {
                if (!ReadString(file, mat.Name)) return false;
                file.read(reinterpret_cast<char*>(mat.BaseColorFactor.data()), sizeof(float) * 4);
                file.read(reinterpret_cast<char*>(&mat.MetallicFactor), sizeof(float));
                file.read(reinterpret_cast<char*>(&mat.RoughnessFactor), sizeof(float));
                auto readTexInfo = [&file](TextureBindingInfo& info) -> bool
                    {
                        if (!ReadString(file, info.Path)) return false;
                        file.read(reinterpret_cast<char*>(&info.IsEmbedded), sizeof(bool));
                        return ReadPodVector(file, info.EmbeddedData);
                    };
                if (!readTexInfo(mat.BaseColorTexture) || !readTexInfo(mat.NormalTexture) ||
                    !readTexInfo(mat.MetallicRoughnessTexture) || !readTexInfo(mat.EmissiveTexture))
                    return false;
            }

            // 2. Meshes
            uint32_t meshCount = 0;
            file.read(reinterpret_cast<char*>(&meshCount), sizeof(meshCount));
            model.Meshes.resize(meshCount);
            for (auto& mesh : model.Meshes)
            {
                if (!ReadMesh(file, mesh, version)) return false;
            }

            // 3. Nodes
            uint32_t nodeCount = 0;
            file.read(reinterpret_cast<char*>(&nodeCount), sizeof(nodeCount));
            model.Nodes.resize(nodeCount);
            for (auto& node : model.Nodes)
            {
                if (!ReadString(file, node.Name)) return false;
                file.read(reinterpret_cast<char*>(&node.ParentIndex), sizeof(node.ParentIndex));
                file.read(reinterpret_cast<char*>(node.LocalTransform.data()), sizeof(float) * 16);
                if (!ReadPodVector(file, node.MeshIndices)) return false;
            }

            // 4. Bones
            uint32_t boneCount = 0;
            file.read(reinterpret_cast<char*>(&boneCount), sizeof(boneCount));
            model.Bones.resize(boneCount);
            for (auto& bone : model.Bones)
            {
                if (!ReadString(file, bone.Name)) return false;
                file.read(reinterpret_cast<char*>(&bone.ParentIndex), sizeof(bone.ParentIndex));
                file.read(reinterpret_cast<char*>(bone.OffsetMatrix.data()), sizeof(float) * 16);
                file.read(reinterpret_cast<char*>(bone.LocalTransform.data()), sizeof(float) * 16);
            }

            // 5. Animations
            uint32_t animCount = 0;
            file.read(reinterpret_cast<char*>(&animCount), sizeof(animCount));
            model.Animations.resize(animCount);
            for (auto& anim : model.Animations)
            {
                if (!ReadString(file, anim.Name)) return false;
                file.read(reinterpret_cast<char*>(&anim.Duration), sizeof(anim.Duration));
                file.read(reinterpret_cast<char*>(&anim.TicksPerSecond), sizeof(anim.TicksPerSecond));
                uint32_t trackCount = 0;
                file.read(reinterpret_cast<char*>(&trackCount), sizeof(trackCount));
                anim.Tracks.resize(trackCount);
                for (auto& track : anim.Tracks)
                {
                    if (!ReadString(file, track.BoneName)) return false;
                    if (!ReadPodVector(file, track.PositionKeys) || !ReadPodVector(file, track.RotationKeys) ||
                        !ReadPodVector(file, track.ScaleKeys))
                        return false;
                }
            }

            return file.good();
        }

        bool WriteModelData(const std::filesystem::path& path, const ModelData& model)
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            if (!file.is_open()) return false;

            file.write(reinterpret_cast<const char*>(&MODEL_DDC_MAGIC), sizeof(MODEL_DDC_MAGIC));
            file.write(reinterpret_cast<const char*>(&MODEL_DDC_VERSION), sizeof(MODEL_DDC_VERSION));
            const auto utf8Path = model.SourcePath.u8string();
            WriteString(file, std::string(utf8Path.begin(), utf8Path.end()));
            file.write(reinterpret_cast<const char*>(&model.MaterialCount), sizeof(model.MaterialCount));

            // 1. Materials
            const uint32_t matCount = static_cast<uint32_t>(model.Materials.size());
            file.write(reinterpret_cast<const char*>(&matCount), sizeof(matCount));
            for (const auto& mat : model.Materials)
            {
                WriteString(file, mat.Name);
                file.write(reinterpret_cast<const char*>(mat.BaseColorFactor.data()), sizeof(float) * 4);
                file.write(reinterpret_cast<const char*>(&mat.MetallicFactor), sizeof(float));
                file.write(reinterpret_cast<const char*>(&mat.RoughnessFactor), sizeof(float));
                auto writeTexInfo = [&file](const TextureBindingInfo& info)
                    {
                        WriteString(file, info.Path);
                        file.write(reinterpret_cast<const char*>(&info.IsEmbedded), sizeof(bool));
                        WritePodVector(file, info.EmbeddedData);
                    };
                writeTexInfo(mat.BaseColorTexture);
                writeTexInfo(mat.NormalTexture);
                writeTexInfo(mat.MetallicRoughnessTexture);
                writeTexInfo(mat.EmissiveTexture);
            }

            // 2. Meshes
            const uint32_t meshCount = static_cast<uint32_t>(model.Meshes.size());
            file.write(reinterpret_cast<const char*>(&meshCount), sizeof(meshCount));
            for (const auto& mesh : model.Meshes) WriteMesh(file, mesh);

            // 3. Nodes
            const uint32_t nodeCount = static_cast<uint32_t>(model.Nodes.size());
            file.write(reinterpret_cast<const char*>(&nodeCount), sizeof(nodeCount));
            for (const auto& node : model.Nodes)
            {
                WriteString(file, node.Name);
                file.write(reinterpret_cast<const char*>(&node.ParentIndex), sizeof(node.ParentIndex));
                file.write(reinterpret_cast<const char*>(node.LocalTransform.data()), sizeof(float) * 16);
                WritePodVector(file, node.MeshIndices);
            }

            // 4. Bones
            const uint32_t boneCount = static_cast<uint32_t>(model.Bones.size());
            file.write(reinterpret_cast<const char*>(&boneCount), sizeof(boneCount));
            for (const auto& bone : model.Bones)
            {
                WriteString(file, bone.Name);
                file.write(reinterpret_cast<const char*>(&bone.ParentIndex), sizeof(bone.ParentIndex));
                file.write(reinterpret_cast<const char*>(bone.OffsetMatrix.data()), sizeof(float) * 16);
                file.write(reinterpret_cast<const char*>(bone.LocalTransform.data()), sizeof(float) * 16);
            }

            // 5. Animations
            const uint32_t animCount = static_cast<uint32_t>(model.Animations.size());
            file.write(reinterpret_cast<const char*>(&animCount), sizeof(animCount));
            for (const auto& anim : model.Animations)
            {
                WriteString(file, anim.Name);
                file.write(reinterpret_cast<const char*>(&anim.Duration), sizeof(anim.Duration));
                file.write(reinterpret_cast<const char*>(&anim.TicksPerSecond), sizeof(anim.TicksPerSecond));
                const uint32_t trackCount = static_cast<uint32_t>(anim.Tracks.size());
                file.write(reinterpret_cast<const char*>(&trackCount), sizeof(trackCount));
                for (const auto& track : anim.Tracks)
                {
                    WriteString(file, track.BoneName);
                    WritePodVector(file, track.PositionKeys);
                    WritePodVector(file, track.RotationKeys);
                    WritePodVector(file, track.ScaleKeys);
                }
            }

            file.close();
            return file.good();
        }

        void SaveModelToDdc(const std::filesystem::path& ddcPath, const ModelData& model)
        {
            auto tmpPath = ddcPath;
            tmpPath += ".tmp";
            if (WriteModelData(tmpPath, model))
            {
                std::error_code ec;
                std::filesystem::rename(tmpPath, ddcPath, ec);
            }
        }

        Result Fail(Error code, std::string message)
        {
            return { code, std::move(message), std::nullopt };
        }

        std::array<float, 3> CopyVector(const aiVector3D& value,
            const aiVector3D& axisSign = aiVector3D(1.0f, 1.0f, 1.0f), float scale = 1.0f)
        {
            return {
                static_cast<float>(value.x * axisSign.x) * scale,
                static_cast<float>(value.y * axisSign.y) * scale,
                static_cast<float>(value.z * axisSign.z) * scale
            };
        }

        std::array<float, 16> CopyMatrixColumnMajor(const aiMatrix4x4& matrix, float translationScale = 1.0f)
        {
            return {
                static_cast<float>(matrix.a1), static_cast<float>(matrix.b1), static_cast<float>(matrix.c1), static_cast<float>(matrix.d1),
                static_cast<float>(matrix.a2), static_cast<float>(matrix.b2), static_cast<float>(matrix.c2), static_cast<float>(matrix.d2),
                static_cast<float>(matrix.a3), static_cast<float>(matrix.b3), static_cast<float>(matrix.c3), static_cast<float>(matrix.d3),
                static_cast<float>(matrix.a4) * translationScale, static_cast<float>(matrix.b4) * translationScale,
                static_cast<float>(matrix.c4) * translationScale, static_cast<float>(matrix.d4)
            };
        }

        template<std::size_t N>
        bool IsFinite(const std::array<float, N>& values)
        {
            return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
        }
    } // namespace

    void GenerateMeshLODs(MeshData& mesh, const Options& options)
        {
            if (!options.GenerateLODs || options.LODCount <= 1 || mesh.Indices.empty() || mesh.Vertices.empty())
                return;

            const size_t sourceIndexCount = mesh.Indices.size();
            const size_t sourceVertexCount = mesh.Vertices.size();
            const size_t lodLevelsToGenerate = std::min<size_t>(
                static_cast<size_t>(options.LODCount - 1),
                options.LODReductionRates.size());

            mesh.LODs.reserve(lodLevelsToGenerate);

            for (size_t lodIdx = 0; lodIdx < lodLevelsToGenerate; ++lodIdx)
            {
                const float targetRatio = options.LODReductionRates[lodIdx];
                if (targetRatio <= 0.0f || targetRatio >= 1.0f)
                    continue;

                size_t targetIndexCount = static_cast<size_t>(static_cast<float>(sourceIndexCount) * targetRatio);
                targetIndexCount = (targetIndexCount / 3) * 3;
                if (targetIndexCount < 3)
                    targetIndexCount = 3;

                // 원거리 LOD일수록 화면 크기가 작아지므로 허용 기하 오차를 점진적으로 확대 (LOD1: ~3~5%, LOD2: ~8~15%)
                const float stepMultiplier = lodIdx == 0 ? 2.5f : 6.0f;
                const float lodTargetError = std::clamp(options.TargetError * stepMultiplier, 0.01f, 0.30f);

                std::vector<unsigned int> destinationIndices(sourceIndexCount);
                float resultError = 0.0f;
                size_t simplifiedIndexCount = meshopt_simplify(
                    destinationIndices.data(),
                    mesh.Indices.data(),
                    sourceIndexCount,
                    &mesh.Vertices[0].Position[0],
                    sourceVertexCount,
                    sizeof(VertexData),
                    targetIndexCount,
                    lodTargetError,
                    0,
                    &resultError
                );

                // UV Seam이나 비다양체 에지로 인해 목표치에 충분히 도달하지 못한 경우 meshopt_simplifySloppy로 추가 축소
                const size_t acceptableThreshold = static_cast<size_t>(static_cast<double>(targetIndexCount) * 1.15);
                if (simplifiedIndexCount > acceptableThreshold)
                {
                    std::vector<unsigned int> sloppyIndices(sourceIndexCount);
                    float sloppyError = 0.0f;
                    const size_t sloppyCount = meshopt_simplifySloppy(
                        sloppyIndices.data(),
                        mesh.Indices.data(),
                        sourceIndexCount,
                        &mesh.Vertices[0].Position[0],
                        sourceVertexCount,
                        sizeof(VertexData),
                        targetIndexCount,
                        lodTargetError * 1.5f,
                        &sloppyError
                    );

                    if (sloppyCount > 0 && sloppyCount < simplifiedIndexCount)
                    {
                        simplifiedIndexCount = sloppyCount;
                        destinationIndices = std::move(sloppyIndices);
                    }
                }

                if (simplifiedIndexCount > 0 && simplifiedIndexCount < sourceIndexCount)
                {
                    destinationIndices.resize(simplifiedIndexCount);

                    std::vector<unsigned int> remap(sourceVertexCount);
                    const size_t uniqueVertexCount = meshopt_generateVertexRemap(
                        remap.data(),
                        destinationIndices.data(),
                        simplifiedIndexCount,
                        mesh.Vertices.data(),
                        sourceVertexCount,
                        sizeof(VertexData)
                    );

                    MeshData lodMesh;
                    lodMesh.Name = mesh.Name + "_LOD" + std::to_string(lodIdx + 1);
                    lodMesh.MaterialIndex = mesh.MaterialIndex;
                    lodMesh.HasSkinning = mesh.HasSkinning;
                    lodMesh.Vertices.resize(uniqueVertexCount);
                    lodMesh.Indices.resize(simplifiedIndexCount);

                    meshopt_remapVertexBuffer(
                        lodMesh.Vertices.data(),
                        mesh.Vertices.data(),
                        sourceVertexCount,
                        sizeof(VertexData),
                        remap.data()
                    );

                    meshopt_remapIndexBuffer(
                        lodMesh.Indices.data(),
                        destinationIndices.data(),
                        simplifiedIndexCount,
                        remap.data()
                    );

                    if (!lodMesh.Vertices.empty())
                    {
                        lodMesh.BoundsMin = lodMesh.BoundsMax = lodMesh.Vertices[0].Position;
                        for (const auto& v : lodMesh.Vertices)
                        {
                            for (size_t a = 0; a < 3; ++a)
                            {
                                lodMesh.BoundsMin[a] = std::min(lodMesh.BoundsMin[a], v.Position[a]);
                                lodMesh.BoundsMax[a] = std::max(lodMesh.BoundsMax[a], v.Position[a]);
                            }
                        }
                    }

                    mesh.LODs.push_back(std::move(lodMesh));
                }
            }
        }

    static Result ConvertModel(const std::filesystem::path& path, const Options& options, bool writeCache)
    {
        if (!std::isfinite(options.UniformScale) || options.UniformScale <= 0.0f)
            return Fail(Error::InvalidData, "Import scale must be finite and positive.");
        std::error_code ec;
        const auto sourcePath = std::filesystem::absolute(path, ec).lexically_normal();
        if (ec || path.empty() || !std::filesystem::is_regular_file(sourcePath, ec))
            return Fail(Error::InvalidPath, "Model file is missing or is not a regular file.");
        // ── 0. 모델 DDC 캐시 조회 (적중 시 Assimp 및 LOD 계산 100% 스킵) ──
        const auto ddcPath = GetModelDdcPath(sourcePath, options);
        if (!ddcPath.empty())
        {
            ModelData cachedModel;
            if (ReadModelData(ddcPath, cachedModel))
                return { Error::None, {}, std::move(cachedModel) };
        }
        unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals |
            aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights;

        if (options.FlipUVs)
            flags |= aiProcess_FlipUVs;

        if (options.ConvertToLeftHanded)
            flags |= aiProcess_MakeLeftHanded;

        if (options.FlipWindingOrder)
            flags |= aiProcess_FlipWindingOrder;

        const auto utf8Path = sourcePath.u8string();
        const std::string filename(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filename, flags);

        if (!scene)
            return Fail(Error::ReadFailed, importer.GetErrorString());

        if ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
            return Fail(Error::InvalidData, "Assimp returned an incomplete scene.");

        if (scene->mNumMeshes == 0 || !scene->mMeshes)
            return Fail(Error::InvalidData, "The model contains no meshes.");

        std::string extension = sourcePath.extension().string();
        for (char& character : extension)
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

        // Assimp mirrors Z for left-handed conversion. Turning glTF around Y preserves its +Z front.
        const bool gltfHeadingCorrection = options.ConvertToLeftHanded && (extension == ".gltf" || extension == ".glb");
        const bool flipHeading = options.FlipHeading != gltfHeadingCorrection;
        const aiVector3D axisSign(flipHeading ? -1.0f : 1.0f, options.FlipPitch ? -1.0f : 1.0f,
            flipHeading != options.FlipPitch ? -1.0f : 1.0f);
        aiMatrix4x4 orientation;
        aiMatrix4x4::Scaling(axisSign, orientation);

        ModelData model;
        model.SourcePath = sourcePath;
        model.MaterialCount = scene->mNumMaterials;
        model.Meshes.reserve(scene->mNumMeshes);

        // 1. 노드 계층 순회 및 구축 (부모 노드가 항상 자식 노드보다 앞 인덱스를 가짐)
        struct PendingNode
        {
            const aiNode* Source;
            std::int32_t ParentIndex;
        };

        std::vector<PendingNode> pending{ { scene->mRootNode, -1 } };
        std::unordered_set<const aiNode*> visited;

        while (!pending.empty())
        {
            const auto current = pending.back();
            pending.pop_back();

            if (!current.Source || !visited.insert(current.Source).second)
                return Fail(Error::InvalidData, "The node hierarchy contains an invalid or repeated node.");

            if (model.Nodes.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
                return Fail(Error::LimitExceeded, "The model contains too many nodes.");

            const aiNode& source = *current.Source;

            if ((source.mNumMeshes > 0 && !source.mMeshes) ||
                (source.mNumChildren > 0 && !source.mChildren))
            {
                return Fail(Error::InvalidData, "A node contains an invalid reference array.");
            }

            NodeData node;
            node.Name = source.mName.C_Str();
            node.ParentIndex = current.ParentIndex;
            node.LocalTransform = CopyMatrixColumnMajor(orientation * source.mTransformation * orientation, options.UniformScale);
            if (!IsFinite(node.LocalTransform))
                return Fail(Error::InvalidData, "A node transform contains a non-finite value.");

            node.MeshIndices.reserve(source.mNumMeshes);

            for (unsigned int i = 0; i < source.mNumMeshes; ++i)
            {
                if (source.mMeshes[i] >= scene->mNumMeshes)
                    return Fail(Error::InvalidData, "A node references an invalid mesh index.");

                node.MeshIndices.push_back(source.mMeshes[i]);
            }

            const auto nodeIndex = static_cast<std::int32_t>(model.Nodes.size());
            model.Nodes.push_back(std::move(node));

            for (unsigned int i = source.mNumChildren; i > 0; --i)
                pending.push_back({ source.mChildren[i - 1], nodeIndex });
        }

        // 2. 스켈레톤(BoneData) 구축: 본이 하나라도 있거나 애니메이션이 있으면 노드 계층을 기반으로 본 테이블 생성
        std::unordered_map<std::string, std::array<float, 16>> boneOffsetMap;
        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* source = scene->mMeshes[meshIndex];
            if (!source || !source->HasBones())
                continue;

            for (unsigned int boneIdx = 0; boneIdx < source->mNumBones; ++boneIdx)
            {
                const aiBone* bone = source->mBones[boneIdx];
                if (!bone)
                    continue;

                const std::string boneName = bone->mName.C_Str();
                if (!boneName.empty() && boneOffsetMap.find(boneName) == boneOffsetMap.end())
                {
                    const auto offsetMat = CopyMatrixColumnMajor(orientation * bone->mOffsetMatrix * orientation, options.UniformScale);
                    if (!IsFinite(offsetMat))
                        return Fail(Error::InvalidData, "A bone offset matrix contains a non-finite value.");
                    boneOffsetMap.emplace(boneName, offsetMat);
                }
            }
        }

        std::unordered_map<std::string, std::uint32_t> boneNameToIndex;
        if (!boneOffsetMap.empty() || scene->mNumAnimations > 0)
        {
            model.Bones.reserve(model.Nodes.size());
            for (std::size_t nodeIdx = 0; nodeIdx < model.Nodes.size(); ++nodeIdx)
            {
                const auto& node = model.Nodes[nodeIdx];
                BoneData bone;
                bone.Name = node.Name.empty() ? ("Bone_" + std::to_string(nodeIdx)) : node.Name;
                bone.ParentIndex = node.ParentIndex;
                // Keep the manual placement rotation out of the skeleton's bind pose.
                bone.LocalTransform = node.ParentIndex < 0
                    ? CopyMatrixColumnMajor(orientation * scene->mRootNode->mTransformation * orientation, options.UniformScale)
                    : node.LocalTransform;

                const auto offsetIt = boneOffsetMap.find(bone.Name);
                if (offsetIt != boneOffsetMap.end())
                    bone.OffsetMatrix = offsetIt->second;

                const auto boneIndex = static_cast<std::uint32_t>(model.Bones.size());
                boneNameToIndex.emplace(bone.Name, boneIndex);
                model.Bones.push_back(std::move(bone));
            }

            // 노드 계층에 없는 별도 본 이름이 메시에 존재할 경우 보강
            for (const auto& [boneName, offsetMat] : boneOffsetMap)
            {
                if (boneNameToIndex.find(boneName) == boneNameToIndex.end())
                {
                    BoneData bone;
                    bone.Name = boneName;
                    bone.ParentIndex = -1;
                    bone.OffsetMatrix = offsetMat;
                    const auto boneIndex = static_cast<std::uint32_t>(model.Bones.size());
                    boneNameToIndex.emplace(bone.Name, boneIndex);
                    model.Bones.push_back(std::move(bone));
                }
            }
        }

        // 3. 메시 및 정점 스키닝 가중치 추출
        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* source = scene->mMeshes[meshIndex];

            if (!source || !source->HasPositions() || !source->HasFaces() || !source->HasNormals())
                return Fail(Error::InvalidData, "A mesh is missing positions, faces, or normals.");

            if (source->mNumAnimMeshes > 0)
                return Fail(Error::UnsupportedFeature, "Morph target conversion is not implemented.");

            if (source->mMaterialIndex >= scene->mNumMaterials)
                return Fail(Error::InvalidData, "A mesh references an invalid material index.");

            if (source->mNumFaces > std::numeric_limits<std::uint32_t>::max() / 3)
                return Fail(Error::LimitExceeded, "A mesh contains too many triangle indices.");

            MeshData mesh;
            mesh.Name = source->mName.C_Str();
            mesh.MaterialIndex = source->mMaterialIndex;
            mesh.HasSkinning = source->HasBones();
            mesh.Vertices.resize(source->mNumVertices);
            mesh.Indices.reserve(static_cast<std::size_t>(source->mNumFaces) * 3);

            for (unsigned int vertexIndex = 0; vertexIndex < source->mNumVertices; ++vertexIndex)
            {
                auto& vertex = mesh.Vertices[vertexIndex];
                vertex.Position = CopyVector(source->mVertices[vertexIndex], axisSign, options.UniformScale);
                vertex.Normal = CopyVector(source->mNormals[vertexIndex], axisSign);

                if (source->HasTextureCoords(0))
                {
                    const auto& uv = source->mTextureCoords[0][vertexIndex];
                    vertex.TexCoord = { static_cast<float>(uv.x), static_cast<float>(uv.y) };
                }
                vertex.TexCoord1 = vertex.TexCoord;
                if (source->HasTextureCoords(1))
                {
                    const auto& uv = source->mTextureCoords[1][vertexIndex];
                    vertex.TexCoord1 = { static_cast<float>(uv.x), static_cast<float>(uv.y) };
                }

                if (source->HasTangentsAndBitangents())
                {
                    vertex.Tangent = CopyVector(source->mTangents[vertexIndex], axisSign);
                    vertex.Bitangent = CopyVector(source->mBitangents[vertexIndex], axisSign);
                }

                if (source->HasVertexColors(0))
                {
                    const auto& color = source->mColors[0][vertexIndex];
                    vertex.Color = {
                        static_cast<float>(color.r), static_cast<float>(color.g),
                        static_cast<float>(color.b), static_cast<float>(color.a)
                    };
                }

                if (!IsFinite(vertex.Position) || !IsFinite(vertex.Normal) || !IsFinite(vertex.TexCoord) || !IsFinite(vertex.TexCoord1) ||
                    !IsFinite(vertex.Tangent) || !IsFinite(vertex.Bitangent) || !IsFinite(vertex.Color))
                {
                    return Fail(Error::InvalidData, "A vertex contains a non-finite value.");
                }

                if (vertexIndex == 0)
                    mesh.BoundsMin = mesh.BoundsMax = vertex.Position;

                for (std::size_t axis = 0; axis < 3; ++axis)
                {
                    mesh.BoundsMin[axis] = std::min(mesh.BoundsMin[axis], vertex.Position[axis]);
                    mesh.BoundsMax[axis] = std::max(mesh.BoundsMax[axis], vertex.Position[axis]);
                }
            }

            if (source->HasBones())
            {
                std::vector<std::uint32_t> influenceCounts(source->mNumVertices, 0);

                for (unsigned int boneIdx = 0; boneIdx < source->mNumBones; ++boneIdx)
                {
                    const aiBone* bone = source->mBones[boneIdx];
                    if (!bone)
                        continue;

                    const auto foundBone = boneNameToIndex.find(bone->mName.C_Str());
                    if (foundBone == boneNameToIndex.end())
                        continue;

                    const std::uint32_t targetBoneIndex = foundBone->second;
                    for (unsigned int weightIdx = 0; weightIdx < bone->mNumWeights; ++weightIdx)
                    {
                        const aiVertexWeight& vw = bone->mWeights[weightIdx];
                        if (vw.mVertexId >= source->mNumVertices || vw.mWeight <= 0.f)
                            continue;

                        auto& vertex = mesh.Vertices[vw.mVertexId];
                        auto& count = influenceCounts[vw.mVertexId];
                        if (count < 4)
                        {
                            vertex.BoneIndices[count] = targetBoneIndex;
                            vertex.BoneWeights[count] = static_cast<float>(vw.mWeight);
                            ++count;
                        }
                        else
                        {
                            // 이미 4개가 찬 경우 가장 작은 가중치보다 크면 교체
                            std::size_t minSlot = 0;
                            for (std::size_t slot = 1; slot < 4; ++slot)
                            {
                                if (vertex.BoneWeights[slot] < vertex.BoneWeights[minSlot])
                                    minSlot = slot;
                            }
                            if (static_cast<float>(vw.mWeight) > vertex.BoneWeights[minSlot])
                            {
                                vertex.BoneIndices[minSlot] = targetBoneIndex;
                                vertex.BoneWeights[minSlot] = static_cast<float>(vw.mWeight);
                            }
                        }
                    }
                }

                for (auto& vertex : mesh.Vertices)
                {
                    const float weightSum = vertex.BoneWeights[0] + vertex.BoneWeights[1] + vertex.BoneWeights[2] + vertex.BoneWeights[3];
                    if (weightSum > 0.f)
                    {
                        for (float& weight : vertex.BoneWeights)
                            weight /= weightSum;
                    }
                    else
                    {
                        vertex.BoneIndices = { 0, 0, 0, 0 };
                        vertex.BoneWeights = { 1.f, 0.f, 0.f, 0.f };
                    }
                }
            }

            for (unsigned int faceIndex = 0; faceIndex < source->mNumFaces; ++faceIndex)
            {
                const auto& face = source->mFaces[faceIndex];

                if (face.mNumIndices != 3 || !face.mIndices)
                    return Fail(Error::UnsupportedFeature, "Only triangle meshes are supported.");

                for (unsigned int corner = 0; corner < 3; ++corner)
                {
                    if (face.mIndices[corner] >= source->mNumVertices)
                        return Fail(Error::InvalidData, "A face references an invalid vertex index.");

                    mesh.Indices.push_back(face.mIndices[corner]);
                }
            }

            GenerateMeshLODs(mesh, options);
            model.Meshes.push_back(std::move(mesh));
        }

        // 3-1. 머티리얼 및 텍스처(임베디드 포함) 정보 추출
        model.Materials.reserve(scene->mNumMaterials);
        for (unsigned int matIdx = 0; matIdx < scene->mNumMaterials; ++matIdx)
        {
            const aiMaterial* srcMat = scene->mMaterials[matIdx];
            if (!srcMat) continue;
            MaterialData mat;
            aiString name;
            if (srcMat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
                mat.Name = name.C_Str();
            if (mat.Name.empty())
                mat.Name = "Material_" + std::to_string(matIdx);
            aiColor4D baseColor(1.f, 1.f, 1.f, 1.f);
            if (srcMat->Get(AI_MATKEY_BASE_COLOR, baseColor) == AI_SUCCESS ||
                srcMat->Get(AI_MATKEY_COLOR_DIFFUSE, baseColor) == AI_SUCCESS)
            {
                mat.BaseColorFactor = { baseColor.r, baseColor.g, baseColor.b, baseColor.a };
            }
            float metallic = 0.f;
            if (srcMat->Get(AI_MATKEY_METALLIC_FACTOR, metallic) == AI_SUCCESS)
                mat.MetallicFactor = metallic;
            float roughness = 0.5f;
            if (srcMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS)
                mat.RoughnessFactor = roughness;
            auto extractTexture = [&](aiTextureType type, TextureBindingInfo& outInfo)
                {
                    if (srcMat->GetTextureCount(type) == 0) return;
                    aiString texPath;
                    if (srcMat->GetTexture(type, 0, &texPath) != AI_SUCCESS) return;
                    outInfo.Path = texPath.C_Str();
                    const aiTexture* embedded = scene->GetEmbeddedTexture(texPath.C_Str());
                    if (embedded)
                    {
                        outInfo.IsEmbedded = true;
                        // mHeight == 0 이면 PNG, JPG 등의 압축 포맷 바이너리입니다.
                        if (embedded->mHeight == 0 && embedded->mWidth > 0)
                        {
                            const auto* bytes = reinterpret_cast<const uint8_t*>(embedded->pcData);
                            outInfo.EmbeddedData.assign(bytes, bytes + embedded->mWidth);
                        }
                    }
                };
            // 슬롯별 텍스처 추출 (glTF 및 일반 모델 호환)
            extractTexture(aiTextureType_BASE_COLOR, mat.BaseColorTexture);
            if (mat.BaseColorTexture.Path.empty())
                extractTexture(aiTextureType_DIFFUSE, mat.BaseColorTexture);
            extractTexture(aiTextureType_NORMALS, mat.NormalTexture);
            if (mat.NormalTexture.Path.empty())
                extractTexture(aiTextureType_HEIGHT, mat.NormalTexture);
            extractTexture(aiTextureType_UNKNOWN, mat.MetallicRoughnessTexture);
            if (mat.MetallicRoughnessTexture.Path.empty())
                extractTexture(aiTextureType_METALNESS, mat.MetallicRoughnessTexture);
            extractTexture(aiTextureType_EMISSIVE, mat.EmissiveTexture);
            model.Materials.push_back(std::move(mat));
        }

        // 4. 애니메이션 클립 및 채널(TrackData) 추출
        model.Animations.reserve(scene->mNumAnimations);
        for (unsigned int animIdx = 0; animIdx < scene->mNumAnimations; ++animIdx)
        {
            const aiAnimation* srcAnim = scene->mAnimations[animIdx];
            if (!srcAnim)
                continue;

            AnimationData anim;
            anim.Name = srcAnim->mName.C_Str();
            if (anim.Name.empty())
                anim.Name = "Animation_" + std::to_string(animIdx);

            anim.Duration = static_cast<float>(srcAnim->mDuration);
            anim.TicksPerSecond = srcAnim->mTicksPerSecond > 0.0 ? static_cast<float>(srcAnim->mTicksPerSecond) : 25.f;
            anim.Tracks.reserve(srcAnim->mNumChannels);

            for (unsigned int chanIdx = 0; chanIdx < srcAnim->mNumChannels; ++chanIdx)
            {
                const aiNodeAnim* channel = srcAnim->mChannels[chanIdx];
                if (!channel)
                    continue;

                TrackData track;
                track.BoneName = channel->mNodeName.C_Str();
                if (track.BoneName.empty())
                    continue;

                track.PositionKeys.reserve(channel->mNumPositionKeys);
                for (unsigned int k = 0; k < channel->mNumPositionKeys; ++k)
                {
                    const auto& key = channel->mPositionKeys[k];
                    VectorKeyData vk{ static_cast<float>(key.mTime), CopyVector(key.mValue) };
                    if (!std::isfinite(vk.Time) || !IsFinite(vk.Value))
                        continue;

                    vk.Value = CopyVector(key.mValue, axisSign, options.UniformScale);
                    if (!IsFinite(vk.Value))
                        return Fail(Error::InvalidData, "A scaled animation position contains a non-finite value.");

                    track.PositionKeys.push_back(vk);
                }

                track.RotationKeys.reserve(channel->mNumRotationKeys);
                for (unsigned int k = 0; k < channel->mNumRotationKeys; ++k)
                {
                    const auto& key = channel->mRotationKeys[k];
                    QuatKeyData qk;
                    qk.Time = static_cast<float>(key.mTime);
                    qk.Value = {
                        static_cast<float>(key.mValue.x * axisSign.x), static_cast<float>(key.mValue.y * axisSign.y),
                        static_cast<float>(key.mValue.z * axisSign.z), static_cast<float>(key.mValue.w)
                    };
                    if (std::isfinite(qk.Time) && IsFinite(qk.Value))
                        track.RotationKeys.push_back(qk);
                }

                track.ScaleKeys.reserve(channel->mNumScalingKeys);
                for (unsigned int k = 0; k < channel->mNumScalingKeys; ++k)
                {
                    const auto& key = channel->mScalingKeys[k];
                    VectorKeyData sk{ static_cast<float>(key.mTime), CopyVector(key.mValue) };
                    if (std::isfinite(sk.Time) && IsFinite(sk.Value))
                        track.ScaleKeys.push_back(sk);
                }

                anim.Tracks.push_back(std::move(track));
            }

            model.Animations.push_back(std::move(anim));
        }

        // ── DDC 캐시 저장 (원자적 쓰기) ──
        if (writeCache && !ddcPath.empty())
        {
            SaveModelToDdc(ddcPath, model);
        }
        return { Error::None, {}, std::move(model) };
    }

    Result ConvertStaticModel(const std::filesystem::path& path, const Options& options)
    {
        return ConvertModel(path, options, true);
    }

    Result ConvertModelToSnapshot(const std::filesystem::path& sourcePath,
        const std::filesystem::path& snapshotPath, const Options& options)
    {
        auto result = ConvertModel(sourcePath, options, false);
        if (result && !WriteModelData(snapshotPath, *result.Model))
            return Fail(Error::InternalError, "Could not write model worker snapshot.");
        return result;
    }

    Result ReadModelSnapshot(const std::filesystem::path& snapshotPath)
    {
        ModelData model;
        if (!ReadModelData(snapshotPath, model) || model.Meshes.empty() || model.Nodes.empty())
            return Fail(Error::InvalidData, "Could not read model worker snapshot.");
        return { Error::None, {}, std::move(model) };
    }
}
