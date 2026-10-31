#include "SceneDocument.h"
#include "Scene.h"
#include "SceneManager.h"
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
            const auto result = Engine::Scene::CreateFromArchive(reader, restored);
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