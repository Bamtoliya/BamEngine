#include "ModelConverter.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <algorithm>
#include <cmath>
#include <limits>
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

        std::array<float, 3> CopyVector(const aiVector3D& value)
        {
            return {
                static_cast<float>(value.x),
                static_cast<float>(value.y),
                static_cast<float>(value.z)
            };
        }

        template<std::size_t N>
        bool IsFinite(const std::array<float, N>& values)
        {
            return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
        }
    }

    Result ConvertStaticModel(const std::filesystem::path& path, const Options& options)
    {
        std::error_code ec;
        const auto sourcePath = std::filesystem::absolute(path, ec).lexically_normal();

        if (ec || path.empty() || !std::filesystem::is_regular_file(sourcePath, ec))
            return Fail(Error::InvalidPath, "Model file is missing or is not a regular file.");

        unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace;

        if (options.FlipUVs)
            flags |= aiProcess_FlipUVs;

        if (options.ConvertToLeftHanded)
            flags |= aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder;

        const auto utf8Path = sourcePath.u8string();
        const std::string filename(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filename, flags);

        if (!scene)
            return Fail(Error::ReadFailed, importer.GetErrorString());

        if ((scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
            return Fail(Error::InvalidData, "Assimp returned an incomplete scene.");

        if (scene->mNumAnimations > 0)
            return Fail(Error::UnsupportedFeature, "Animation conversion is not implemented.");

        if (scene->mNumMeshes == 0 || !scene->mMeshes)
            return Fail(Error::InvalidData, "The model contains no meshes.");

        ModelData model;
        model.SourcePath = sourcePath;
        model.MaterialCount = scene->mNumMaterials;
        model.Meshes.reserve(scene->mNumMeshes);

        for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
        {
            const aiMesh* source = scene->mMeshes[meshIndex];

            if (!source || !source->HasPositions() || !source->HasFaces() || !source->HasNormals())
                return Fail(Error::InvalidData, "A mesh is missing positions, faces, or normals.");

            if (source->HasBones() || source->mNumAnimMeshes > 0)
                return Fail(Error::UnsupportedFeature, "Skinning and morph conversion are not implemented.");

            if (source->mMaterialIndex >= scene->mNumMaterials)
                return Fail(Error::InvalidData, "A mesh references an invalid material index.");

            if (source->mNumFaces > std::numeric_limits<std::uint32_t>::max() / 3)
                return Fail(Error::LimitExceeded, "A mesh contains too many triangle indices.");

            MeshData mesh;
            mesh.Name = source->mName.C_Str();
            mesh.MaterialIndex = source->mMaterialIndex;
            mesh.Vertices.resize(source->mNumVertices);
            mesh.Indices.reserve(static_cast<std::size_t>(source->mNumFaces) * 3);

            for (unsigned int vertexIndex = 0; vertexIndex < source->mNumVertices; ++vertexIndex)
            {
                auto& vertex = mesh.Vertices[vertexIndex];
                vertex.Position = CopyVector(source->mVertices[vertexIndex]);
                vertex.Normal = CopyVector(source->mNormals[vertexIndex]);

                if (source->HasTextureCoords(0))
                {
                    const auto& uv = source->mTextureCoords[0][vertexIndex];
                    vertex.TexCoord = { static_cast<float>(uv.x), static_cast<float>(uv.y) };
                }

                if (source->HasTangentsAndBitangents())
                {
                    vertex.Tangent = CopyVector(source->mTangents[vertexIndex]);
                    vertex.Bitangent = CopyVector(source->mBitangents[vertexIndex]);
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

            model.Meshes.push_back(std::move(mesh));
        }

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

            const auto& matrix = source.mTransformation;
            const std::array<ai_real, 16> values{
                matrix.a1, matrix.b1, matrix.c1, matrix.d1,
                matrix.a2, matrix.b2, matrix.c2, matrix.d2,
                matrix.a3, matrix.b3, matrix.c3, matrix.d3,
                matrix.a4, matrix.b4, matrix.c4, matrix.d4
            };

            for (std::size_t i = 0; i < values.size(); ++i)
                node.LocalTransform[i] = static_cast<float>(values[i]);

            if (!IsFinite(node.LocalTransform))
                return Fail(Error::InvalidData, "A node transform contains a non-finite value.");

            node.MeshIndices.reserve(source.mNumMeshes);

            for (unsigned int i = 0; i < source.mNumMeshes; ++i)
            {
                if (source.mMeshes[i] >= model.Meshes.size())
                    return Fail(Error::InvalidData, "A node references an invalid mesh index.");

                node.MeshIndices.push_back(source.mMeshes[i]);
            }

            const auto nodeIndex = static_cast<std::int32_t>(model.Nodes.size());
            model.Nodes.push_back(std::move(node));

            for (unsigned int i = source.mNumChildren; i > 0; --i)
                pending.push_back({ source.mChildren[i - 1], nodeIndex });
        }

        return { Error::None, {}, std::move(model) };
    }
}