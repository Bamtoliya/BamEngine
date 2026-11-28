#include "SceneDocument.h"
#include "Scene.h"
#include "SceneManager.h"
#include "AssetPathRelocation.h"
#include "TerrainDataFile.h"
#include "NavigationSystem.h"
#include "PlatformPath.h"
#include "Functions.h"
#include "ResourceReference.h"
#include <archive/JsonArchive.h>

namespace
{
    struct DocumentSceneDeleter
    {
        void operator()(Engine::Scene* scene) const
        {
            if (scene)
                scene->Release();
        }
    };

    using DocumentScenePtr = std::unique_ptr<Engine::Scene, DocumentSceneDeleter>;
}

namespace Editor
{
    SceneDocument::SceneDocument(Engine::SceneManager& scenes, SceneEditHistory& history, std::filesystem::path path)
        : m_Scenes(scenes), m_History(history)
    {
        if (!path.empty())
        {
            std::error_code error;
            m_Path = std::filesystem::absolute(path, error);
            if (error)
                throw std::filesystem::filesystem_error("Cannot resolve scene document path", path, error);

            m_Path = m_Path.lexically_normal();
        }

        m_Scene = m_Scenes.GetCurrentScene();
        if (!m_Scene)
            throw std::invalid_argument("SceneDocument requires a current scene");

        m_Scene->AddRef();
    }

    SceneDocument::~SceneDocument()
    {
        m_Scene->Release();
    }

    bool SceneDocument::HasUnsavedChanges() const
    {
        return m_History.HasUnsavedChanges();
    }

    bool SceneDocument::CanSave() const
    {
        return !m_ReadOnly && m_Scenes.GetCurrentScene() == m_Scene &&
            m_History.CanFinalizeEditsFor(m_Scene);
    }

    bool SceneDocument::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
    {
        if (HasUnsavedChanges() || !CanSave())
            return false;
        if (!m_History.PrepareAssetPathRelocation(relocation) ||
            !relocation.PrepareScenePaths(m_Scene->GetRegistry()))
            return false;
        relocation.Add(m_Path);
        return true;
    }

    Engine::EResult SceneDocument::SaveTo(const std::filesystem::path& path)
    {
        if (!CanSave())
            return Engine::EResult::AccessDenied;

        if (path.empty())
            return Engine::EResult::InvalidArgument;

        std::error_code error;
        auto target = std::filesystem::absolute(path, error);
        if (error)
            return Engine::EResult::Fail;

        target = target.lexically_normal();

        m_History.Flush();
        if (!m_History.CanMarkSaved())
            return Engine::EResult::Fail;

        // 지형 데이터 파일을 먼저 쓴다. 새 지형은 여기서 DataPath가 정해지므로 씬 JSON보다 앞서야 한다.
        if (!Engine::TerrainDataFile::SaveSceneTerrains(m_Scene->GetRegistry(), target))
            return Engine::EResult::Fail;

        if (auto navGrid = Engine::NavigationSystem::GetSceneNavGrid(m_Scene))
        {
            std::string navUri = m_Scene->GetNavGridPath();
            if (navUri.empty())
                navUri = Engine::Unicode::WideToUtf8(Engine::PlatformPath::ToVirtualUri(Engine::NavGrid::MakeDefaultPath(target)));
            if (!navGrid->SaveToFile(Engine::NavGrid::ResolveDataPath(navUri)))
                return Engine::EResult::Fail;
            m_Scene->SetNavGridPath(navUri);
        }

        if (auto navMesh = Engine::NavigationSystem::GetSceneNavMesh(m_Scene))
        {
            std::string navMeshUri = m_Scene->GetNavMeshPath();
            if (navMeshUri.empty())
                navMeshUri = Engine::Unicode::WideToUtf8(Engine::PlatformPath::ToVirtualUri(Engine::NavMesh::MakeDefaultPath(target)));
            if (!navMesh->SaveToFile(Engine::NavMesh::ResolveDataPath(navMeshUri)))
                return Engine::EResult::Fail;
            m_Scene->SetNavMeshPath(navMeshUri);
        }

        archive::JsonArchive writer;
        auto result = m_Scene->WriteArchive(writer);
        if (result)
            result = writer.SaveToFile(target);

        if (!result)
            return Engine::EResult::Fail;

        if (!m_History.MarkSaved())
            return Engine::EResult::Fail;

        m_Path.swap(target);
        return Engine::EResult::Success;
    }

    Engine::EResult SceneDocument::DiscardChanges()
    {
        if (!CanSave())
            return Engine::EResult::AccessDenied;

        if (!HasUnsavedChanges())
            return Engine::EResult::Success;

        DocumentScenePtr candidate;
        Engine::MissingResourceRecoveryContext recoveryContext;

        if (m_Path.empty())
        {
            candidate.reset(Engine::Scene::Create());
            if (!candidate)
                return Engine::EResult::Fail;
        }
        else
        {
            archive::JsonArchive reader;
            if (!reader.LoadFromFile(m_Path))
                return Engine::EResult::Fail;

            Engine::Scene* restored = nullptr;
            archive::ArchiveResult result;
            {
                Engine::MissingResourceRecoveryScope recoveryScope(&recoveryContext);
                result = Engine::Scene::CreateFromArchive(reader, restored);
            }
            candidate.reset(restored);

            if (!result || !candidate)
                return Engine::EResult::Fail;
        }

        const auto result = m_Scenes.OpenScene(candidate.get());
        if (result != Engine::EResult::Success)
            return result;

        DocumentScenePtr previous(m_Scene);
        m_Scene = candidate.release();
        m_History.ResetDocument(m_Scene, true);
        if (!recoveryContext.Entries.empty())
            m_History.NotifyUntrackedChange();

        return Engine::EResult::Success;
    }

    Engine::EResult SceneDocument::RebindRestoredScene()
    {
        auto* restored = m_Scenes.GetCurrentScene();
        if (!restored)
            return Engine::EResult::Fail;

        restored->AddRef();
        DocumentScenePtr previous(m_Scene);
        m_Scene = restored;

        m_History.Synchronize(m_Scene, true);
        return Engine::EResult::Success;
    }
}