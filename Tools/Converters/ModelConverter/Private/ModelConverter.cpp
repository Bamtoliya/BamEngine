#include "ModelConverter.h"

#include <meshoptimizer.h>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace Tools::ModelConversion
{
    namespace
    {
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
    }

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

    Result ConvertStaticModel(const std::filesystem::path& path, const Options& options)
    {
        if (!std::isfinite(options.UniformScale) || options.UniformScale <= 0.0f)
            return Fail(Error::InvalidData, "Import scale must be finite and positive.");

        std::error_code ec;
        const auto sourcePath = std::filesystem::absolute(path, ec).lexically_normal();

        if (ec || path.empty() || !std::filesystem::is_regular_file(sourcePath, ec))
            return Fail(Error::InvalidPath, "Model file is missing or is not a regular file.");

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
            aiMatrix4x4 localMat = orientation * source.mTransformation * orientation;
            if (current.ParentIndex == -1)
            {
                aiMatrix4x4 rotX, rotY, rotZ;
                const float radX = options.RotationOffset[0] * (3.14159265359f / 180.0f);
                const float radY = options.RotationOffset[1] * (3.14159265359f / 180.0f);
                const float radZ = options.RotationOffset[2] * (3.14159265359f / 180.0f);
                aiMatrix4x4::RotationX(radX, rotX);
                aiMatrix4x4::RotationY(radY, rotY);
                aiMatrix4x4::RotationZ(radZ, rotZ);
                localMat = rotZ * rotY * rotX * localMat;
            }
            // Uniform import scale changes asset units, so scale translations and mesh positions together.
            node.LocalTransform = CopyMatrixColumnMajor(localMat, options.UniformScale);
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

                if (!IsFinite(vertex.Position) || !IsFinite(vertex.Normal) || !IsFinite(vertex.TexCoord) ||
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

        return { Error::None, {}, std::move(model) };
    }
}
