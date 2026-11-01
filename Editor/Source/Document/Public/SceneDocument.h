#pragma once

#include "EditorDocumentInterface.h"
#include "SceneEditHistory.h"

namespace Engine
{
    class Scene;
    class SceneManager;
}

namespace Editor
{
    // 메인 씬의 문서 어댑터. SceneManager와 편집 이력은 이 문서보다 오래 살아야 한다.
    class SceneDocument final : public IEditorDocument
    {
    public:
        SceneDocument(Engine::SceneManager& scenes, SceneEditHistory& history, std::filesystem::path path = {});
        ~SceneDocument() override;

        SceneDocument(const SceneDocument&) = delete;
        SceneDocument& operator=(const SceneDocument&) = delete;
        SceneDocument(SceneDocument&&) = delete;
        SceneDocument& operator=(SceneDocument&&) = delete;

        const std::filesystem::path& GetPath() const override { return m_Path; }
        bool HasUnsavedChanges() const override;
        bool CanSave() const override;
        Engine::EResult SaveTo(const std::filesystem::path& path) override;
        Engine::EResult DiscardChanges() override;

        Engine::Scene* GetScene() const { return m_Scene; }
        void SetReadOnly(bool readOnly) { m_ReadOnly = readOnly; }

        EditorDocumentSaveInfo GetSaveInfo() const override
        {
            return { "FileDialog.SceneFiles", L"*.bamscene.json;*.json", L"NewScene.bamscene.json", L"bamscene.json", EFileDialogContext::SceneSave };
        }

        Engine::EResult RebindRestoredScene();

    private:
        Engine::SceneManager& m_Scenes;
        SceneEditHistory& m_History;
        Engine::Scene* m_Scene = nullptr;
        std::filesystem::path m_Path;
        bool m_ReadOnly = true;
    };
}