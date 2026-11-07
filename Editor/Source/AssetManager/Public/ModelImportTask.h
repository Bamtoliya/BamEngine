#pragma once

#include "ModelImporter.h"
#include "ImportJob.h"
#include "ModelConverter.h"
#include "Vertex.h"

namespace Editor
{
    struct ModelFinalizeState;
    struct ModelImportRequest
    {
        filesystem::path SourcePath;
        filesystem::path DestinationDirectory;
        ModelImportDesc Settings;
    };

    struct PreparedModelMesh
    {
        std::string Name;
        std::uint32_t MaterialIndex = 0;

        std::vector<Engine::VertexPosition> Positions;
        std::vector<Engine::VertexMaterial> Attributes;
        std::vector<Engine::VertexSkinData> SkinWeights;
        std::vector<Engine::uint32> Indices;

        glm::vec3 BoundsMin{};
        glm::vec3 BoundsMax{};

        std::vector<PreparedModelMesh> LODs;
    };

    struct PreparedModelData
    {
        filesystem::path SourcePath;
        std::uint32_t MaterialCount = 0;
        std::vector<Tools::ModelConversion::MaterialData> Materials;
        std::vector<PreparedModelMesh> Meshes;
        std::vector<Tools::ModelConversion::NodeData> Nodes;
        std::vector<Tools::ModelConversion::BoneData> Bones;
        std::vector<Tools::ModelConversion::AnimationData> Animations;
    };

    class ModelImportTask final : public ImportJob
    {
    public:
        explicit ModelImportTask(ModelImportRequest request);

        EResult Prepare() override;

        // Prepare 실행 완료를 동기화한 뒤에만 접근합니다.
        const ModelImportRequest& GetRequest() const { return m_Request; }
        PreparedModelData* GetPreparedModel()
        {
            return m_PreparedModel ? &*m_PreparedModel : nullptr;
        }
        const PreparedModelData* GetPreparedModel() const
        {
            return m_PreparedModel ? &*m_PreparedModel : nullptr;
        }

        Tools::ModelConversion::Error GetErrorCode() const { return m_ErrorCode; }
        const std::string& GetErrorMessage() const { return m_ErrorMessage; }

    private:
        ModelImportRequest m_Request;
        std::optional<PreparedModelData> m_PreparedModel;
        Tools::ModelConversion::Error m_ErrorCode = Tools::ModelConversion::Error::None;
        std::string m_ErrorMessage;
        bool m_PrepareStarted = false;
        friend class ModelImporter;
        std::shared_ptr<ModelFinalizeState> m_FinalizeState;
    };
}