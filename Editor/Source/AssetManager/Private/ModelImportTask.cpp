#include "ModelImportTask.h"

namespace Editor
{
    ModelImportTask::ModelImportTask(ModelImportRequest request)
        : m_Request(std::move(request))
    {
    }

    EResult ModelImportTask::Prepare()
    {
        if (m_PrepareStarted)
            return EResult::AlreadyInitialized;

        m_PrepareStarted = true;

        using ConversionError = Tools::ModelConversion::Error;

        const auto fail = [this](ConversionError code, std::string message)
            {
                m_ErrorCode = code;
                m_ErrorMessage = std::move(message);
                return EResult::Fail;
            };

        const auto options = ModelImporter::MakeConversionOptions(m_Request.Settings);

        try
        {
            auto converted = Tools::ModelConversion::ConvertStaticModel(m_Request.SourcePath, options);
            if (!converted)
                return fail(converted.Code, std::move(converted.Message));

            auto& source = *converted.Model;
            const bool importSkeleton = HasFlag(m_Request.Settings.ImportOptions, EModelImportOption::Skeleton);
            const bool importAnimations = HasFlag(m_Request.Settings.ImportOptions, EModelImportOption::Animations);

            PreparedModelData prepared;
            prepared.SourcePath = std::move(source.SourcePath);
            prepared.MaterialCount = source.MaterialCount;
            if (HasFlag(m_Request.Settings.ImportOptions, EModelImportOption::Materials))
                prepared.Materials = std::move(source.Materials);
            prepared.Nodes = std::move(source.Nodes);
            if (importSkeleton)
                prepared.Bones = std::move(source.Bones);
            if (importAnimations)
                prepared.Animations = std::move(source.Animations);
            prepared.Meshes.reserve(source.Meshes.size());

            constexpr auto maxBufferBytes = (std::numeric_limits<Engine::uint32>::max)();

            for (auto& sourceMesh : source.Meshes)
            {
                const auto vertexCount = sourceMesh.Vertices.size();
                const auto indexCount = sourceMesh.Indices.size();
                const bool hasSkinning = importSkeleton && sourceMesh.HasSkinning && !prepared.Bones.empty();

                // 현재 Mesh/RHI의 버퍼 크기 인자는 uint32입니다.
                if (vertexCount > maxBufferBytes / sizeof(Engine::VertexPosition) ||
                    vertexCount > maxBufferBytes / sizeof(Engine::VertexMaterial) ||
                    (hasSkinning && vertexCount > maxBufferBytes / sizeof(Engine::VertexSkinData)) ||
                    indexCount > maxBufferBytes / sizeof(Engine::uint32))
                {
                    return fail(ConversionError::LimitExceeded, "A mesh exceeds the RHI buffer size limit.");
                }

                PreparedModelMesh mesh;
                mesh.Name = std::move(sourceMesh.Name);
                mesh.MaterialIndex = sourceMesh.MaterialIndex;
                mesh.BoundsMin = {
                    sourceMesh.BoundsMin[0], sourceMesh.BoundsMin[1], sourceMesh.BoundsMin[2]
                };
                mesh.BoundsMax = {
                    sourceMesh.BoundsMax[0], sourceMesh.BoundsMax[1], sourceMesh.BoundsMax[2]
                };

                mesh.Positions.resize(vertexCount);
                mesh.Attributes.resize(vertexCount);
                if (hasSkinning)
                    mesh.SkinWeights.resize(vertexCount);

                for (std::size_t i = 0; i < vertexCount; ++i)
                {
                    const auto& vertex = sourceMesh.Vertices[i];

                    mesh.Positions[i].position = {
                        vertex.Position[0], vertex.Position[1], vertex.Position[2]
                    };

                    auto& attributes = mesh.Attributes[i];
                    attributes.normal = { vertex.Normal[0], vertex.Normal[1], vertex.Normal[2] };
                    attributes.texCoord = { vertex.TexCoord[0], vertex.TexCoord[1] };
                    attributes.tangent = { vertex.Tangent[0], vertex.Tangent[1], vertex.Tangent[2] };
                    attributes.binormal = { vertex.Bitangent[0], vertex.Bitangent[1], vertex.Bitangent[2] };
                    attributes.color = { vertex.Color[0], vertex.Color[1], vertex.Color[2], vertex.Color[3] };

                    if (hasSkinning)
                    {
                        auto& skin = mesh.SkinWeights[i];
                        skin.boneIDs = {
                            vertex.BoneIndices[0], vertex.BoneIndices[1],
                            vertex.BoneIndices[2], vertex.BoneIndices[3]
                        };
                        skin.weights = {
                            vertex.BoneWeights[0], vertex.BoneWeights[1],
                            vertex.BoneWeights[2], vertex.BoneWeights[3]
                        };
                    }
                }

                mesh.Indices = std::move(sourceMesh.Indices);

                // 현재 메시의 원본 정점 메모리를 작업 스레드에서 해제합니다.
                std::vector<Tools::ModelConversion::VertexData>{}.swap(sourceMesh.Vertices);

                prepared.Meshes.push_back(std::move(mesh));
            }

            // 모든 메시 준비가 성공했을 때만 결과를 공개합니다.
            m_PreparedModel.emplace(std::move(prepared));
            return EResult::Success;
        }
        catch (const std::exception& exception)
        {
            return fail(ConversionError::InternalError, exception.what());
        }
        catch (...)
        {
            return fail(ConversionError::InternalError, "Unknown model preparation exception.");
        }
    }
}