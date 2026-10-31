#include "SceneEditHistory.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include "UIComponents.h"
#include "UILayout.h"
#include <iostream>
#include <stdexcept>
#include "EditorDocumentManager.h"
#include "SceneDocument.h"
#include "SceneManager.h"
#include "EditorDocumentController.h"

namespace
{
    void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
    struct SceneDeleter { void operator()(Engine::Scene* scene) const { if (scene) scene->Release(); } };
    using ScenePtr = std::unique_ptr<Engine::Scene, SceneDeleter>;
    struct TestCommand final : Editor::EditorCommand
    {
        int& Value;
        int& Alive;
        bool& Fail;
        TestCommand(int& value, int& alive, bool& fail) : EditorCommand("test"), Value(value), Alive(alive), Fail(fail)
        { ++Alive; }
        ~TestCommand() override { --Alive; }
        bool Execute() override { if (Fail) return false; ++Value; return true; }
        bool Undo() override { if (Fail) return false; --Value; return true; }
    };
    void HistoryOwnership()
    {
        int value = 0, alive = 0;
        bool fail = false;
        {
            Editor::CommandHistory history(2);
            Check(history.Execute(std::make_unique<TestCommand>(value, alive, fail)), "execute");
            fail = true;
            Check(!history.Undo() && value == 1 && history.GetUndoCount() == 1, "failed undo cursor");
            fail = false;
            Check(history.Undo() && value == 0 && history.CanRedo(), "undo");
            fail = true;
            Check(!history.Redo() && value == 0 && history.GetRedoCount() == 1, "failed redo cursor");
            Check(!history.Execute(std::make_unique<TestCommand>(value, alive, fail)) && alive == 1, "failed ownership");
            fail = false;
            Check(history.Execute(std::make_unique<TestCommand>(value, alive, fail)) &&
                !history.CanRedo() && alive == 1, "redo truncation");
            history.Execute(std::make_unique<TestCommand>(value, alive, fail));
            history.Execute(std::make_unique<TestCommand>(value, alive, fail));
            Check(alive == 2 && history.GetUndoCount() == 2, "history limit");
            history.Clear();
            Check(alive == 0 && !history.CanUndo(), "clear ownership");
            history.Execute(std::make_unique<TestCommand>(value, alive, fail));
        }
        Check(alive == 0, "destructor ownership");
    }
    void HistoryStateIdentity()
    {
        int value = 0, alive = 0;
        bool fail = false;
        Editor::CommandHistory history(2);

        const auto execute = [&]()
            {
                return history.Execute(std::make_unique<TestCommand>(value, alive, fail));
            };

        const auto initial = history.GetCurrentStateId();
        Check(execute(), "first state");
        const auto saved = history.GetCurrentStateId();

        Check(execute(), "second state");
        const auto latest = history.GetCurrentStateId();

        fail = true;
        Check(!history.Undo() && history.GetCurrentStateId() == latest, "failed undo changed state");
        fail = false;

        Check(history.Undo() && history.GetCurrentStateId() == saved, "undo state");
        Check(history.Redo() && history.GetCurrentStateId() == latest, "redo state");
        Check(history.Undo() && history.Undo(), "return to initial");
        Check(history.GetCurrentStateId() == initial, "initial state lost");

        fail = true;
        Check(!history.Redo() && history.GetCurrentStateId() == initial, "failed redo changed state");
        Check(!execute() && history.GetCurrentStateId() == initial, "failed execute changed state");
        fail = false;

        Check(execute() && history.GetCurrentStateId() != saved, "branch reused saved state");
        const auto branch = history.GetCurrentStateId();

        Check(execute() && execute(), "history limit setup");
        Check(history.Undo() && history.Undo(), "history limit undo");
        Check(history.GetCurrentStateId() == branch && !history.CanUndo(), "history limit changed state");

        history.Clear();
        Check(history.GetCurrentStateId() == branch, "clear changed document state");

        ++value;
        Check(history.Record(std::make_unique<TestCommand>(value, alive, fail)), "record state");
        const auto recorded = history.GetCurrentStateId();
        Check(recorded != branch && recorded != saved, "record reused state");
        Check(history.Undo() && history.GetCurrentStateId() == branch, "record undo state");
        Check(history.Redo() && history.GetCurrentStateId() == recorded, "record redo state");
    }
    Engine::Entity& Add(Engine::Scene& scene, const wchar_t* name)
    {
        auto& entity = scene.CreateEntity();
        auto& world = scene.GetRegistry();
        const auto handle = entity.GetEntityHandle();
        world.emplace<Engine::NameComponent>(handle, name);
        world.emplace<Engine::TransformComponent>(handle);
        world.emplace<Engine::WorldTransformComponent>(handle);
        world.emplace<Engine::HierarchyComponent>(handle);
        return entity;
    }
    void SceneDocumentState()
    {
        ScenePtr scene(Engine::Scene::Create());
        Check(bool(scene), "document scene");

        auto& entity = Add(*scene, L"before");
        auto& world = scene->GetRegistry();
        const auto handle = entity.GetEntityHandle();
        const std::array targets{ handle };

        Editor::SceneEditHistory history;
        history.ResetDocument(scene.get(), true);
        Check(!history.HasUnsavedChanges(), "initial document dirty");

        const auto changeName = [&](const wchar_t* name)
            {
                auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
                world.get<Engine::NameComponent>(handle).name = name;
                history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, targets), "name", true);
            };

        changeName(L"first");
        Check(history.HasUnsavedChanges() && history.Commands().GetUndoCount() == 0, "pending edit ignored");

        const auto revision = history.Revision();
        Check(history.MarkSaved() && !history.HasUnsavedChanges(), "save baseline");
        Check(history.Revision() != revision, "save gesture boundary");

        changeName(L"second");
        history.Flush();
        Check(history.HasUnsavedChanges(), "second edit ignored");
        Check(history.Undo() && !history.HasUnsavedChanges(), "undo to saved state");
        Check(world.get<Engine::NameComponent>(handle).name == L"first", "saved value not restored");
        Check(history.Redo() && history.HasUnsavedChanges(), "redo dirty state");

        history.Clear();
        Check(history.HasUnsavedChanges() && !history.CanUndo(), "clear marked document saved");
        Check(history.MarkSaved(), "save after clear");

        changeName(L"third");
        history.Clear();
        Check(history.HasUnsavedChanges(), "discarded pending edit lost dirty state");
        Check(history.MarkSaved() && !history.HasUnsavedChanges(), "save pending result");

        world.get<Engine::NameComponent>(handle).name = L"external";
        history.NotifyUntrackedChange();
        history.Synchronize(scene.get(), false);
        Check(history.HasUnsavedChanges() && !history.MarkSaved(), "play transition lost dirty state");

        ScenePtr restored(Engine::Scene::Create());
        Check(bool(restored), "restored scene");
        history.Synchronize(restored.get(), true);
        Check(history.HasUnsavedChanges(), "scene pointer change marked document saved");

        history.ResetDocument(restored.get(), true);
        Check(!history.HasUnsavedChanges(), "new document baseline");
        history.Record({}, {}, "invalid");
        Check(history.HasUnsavedChanges(), "capture failure lost dirty state");

        history.ResetDocument(restored.get(), true);
        bool rejectedSave = false;
        try
        {
            Editor::SceneEditScope edit(history, *restored, "exception");
            rejectedSave = !history.MarkSaved();
            throw std::runtime_error("expected");
        }
        catch (const std::runtime_error&)
        {
        }

        Check(rejectedSave && history.HasUnsavedChanges(), "failed scope lost dirty state");
    }
    void ComponentAndGesture()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& entity = Add(*scene, L"before");
        auto& world = scene->GetRegistry();
        const auto handle = entity.GetEntityHandle();
        const std::array targets{ handle };
        Editor::SceneEditHistory history;
        history.Synchronize(scene.get(), true);
        for (int i = 1; i <= 3; ++i)
        {
            auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
            world.get<Engine::NameComponent>(handle).name = L"after";
            Check(world.get<Engine::TransformComponent>(handle).SetLocalPosition(glm::vec3(float(i))), "position");
            history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, targets), "properties", true);
        }
        history.Flush();
        Check(history.Commands().GetUndoCount() == 1, "gesture split");
        Check(history.Undo() && world.get<Engine::NameComponent>(handle).name == L"before" &&
            world.get<Engine::TransformComponent>(handle).position == glm::vec3(0), "component undo");
        Check(history.Redo() && world.get<Engine::TransformComponent>(handle).position == glm::vec3(3), "redo");
        auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
        world.emplace<Engine::CameraComponent>(handle);
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, targets), "add");
        Check(history.Undo() && !world.all_of<Engine::CameraComponent>(handle), "add undo");
        Check(history.Redo() && world.all_of<Engine::CameraComponent>(handle), "add redo");
        history.Synchronize(scene.get(), false);
        Check(!history.CanUndo() && !history.CanRedo(), "play state");
        history.Synchronize(scene.get(), true);
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, targets), "edit");
        ScenePtr other(Engine::Scene::Create());
        history.Synchronize(other.get(), true);
        Check(!history.CanUndo(), "scene switch");
    }
    void StructureAndIdentity()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& parent = Add(*scene, L"parent");
        auto& child = Add(*scene, L"child");
        auto& sibling = Add(*scene, L"sibling");
        auto& world = scene->GetRegistry();
        const auto parentId = world.get<Engine::IDComponent>(parent.GetEntityHandle()).id;
        const auto childId = world.get<Engine::IDComponent>(child.GetEntityHandle()).id;
        const auto siblingId = world.get<Engine::IDComponent>(sibling.GetEntityHandle()).id;
        Check(scene->ReparentEntity(child, &parent) == Engine::EntityReparentResult::Success, "parent");
        Check(scene->ReparentEntity(sibling, &parent) == Engine::EntityReparentResult::Success, "sibling");
        Editor::SceneEditHistory history;
        history.Synchronize(scene.get(), true);
        auto before = Editor::SceneEditSnapshot::Capture(*scene);
        Check(scene->ReparentEntity(child) == Engine::EntityReparentResult::Success, "detach");
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene), "parent");
        Check(history.Undo(), "parent undo");
        auto p = scene->FindEntityById(parentId)->GetEntityHandle();
        auto c = scene->FindEntityById(childId)->GetEntityHandle();
        auto s = scene->FindEntityById(siblingId)->GetEntityHandle();
        Check(world.get<Engine::HierarchyComponent>(p).children == std::vector{ c, s }, "sibling order");
        Check(history.Redo() && world.get<Engine::HierarchyComponent>(c).parent == entt::null, "detach redo");
        before = Editor::SceneEditSnapshot::Capture(*scene);
        const auto oldHandle = s;
        Check(scene->DestroyEntity(*scene->FindEntityById(siblingId)), "delete");
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene), "delete");
        Check(history.Undo(), "delete undo");
        s = scene->FindEntityById(siblingId)->GetEntityHandle();
        Check(s != oldHandle && world.get<Engine::HierarchyComponent>(p).children == std::vector{ s }, "ID remap");
        Check(history.Redo() && !scene->FindEntityById(siblingId), "delete redo");
        Check(history.Undo() && history.Undo(), "undo after recreation");
        p = scene->FindEntityById(parentId)->GetEntityHandle();
        c = scene->FindEntityById(childId)->GetEntityHandle();
        s = scene->FindEntityById(siblingId)->GetEntityHandle();
        Check(world.get<Engine::HierarchyComponent>(p).children == std::vector{ c, s }, "restored order");
        before = Editor::SceneEditSnapshot::Capture(*scene);
        auto& created = Add(*scene, L"created");
        const auto createdId = world.get<Engine::IDComponent>(created.GetEntityHandle()).id;
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene), "create");
        Check(!history.CanRedo() && history.Undo() && !scene->FindEntityById(createdId), "create undo");
        Check(history.Redo() && scene->FindEntityById(createdId), "create redo");
        before = Editor::SceneEditSnapshot::Capture(*scene);
        Check(scene->DestroyEntity(*scene->FindEntityById(parentId)), "subtree delete");
        Check(!scene->FindEntityById(childId) && !scene->FindEntityById(siblingId), "descendants");
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene), "delete subtree");
        Check(history.Undo() && scene->FindEntityById(parentId) && scene->FindEntityById(childId), "subtree undo");
        const auto previousName = scene->GetName();
        {
            Editor::SceneEditScope edit(history, *scene, "rename");
            scene->SetName(L"renamed");
        }
        Check(history.Undo() && scene->GetName() == previousName, "scene rename undo");
        Check(history.Redo() && scene->GetName() == L"renamed", "scene rename redo");
    }
    void FailureAndLifetime()
    {
        struct Unknown { int Value = 7; };
        ScenePtr scene(Engine::Scene::Create());
        auto& entity = Add(*scene, L"target");
        auto& world = scene->GetRegistry();
        const auto handle = entity.GetEntityHandle();
        const std::array targets{ handle };
        auto snapshot = Editor::SceneEditSnapshot::Capture(*scene, targets);
        world.emplace<Unknown>(handle);
        Check(!Editor::SceneEditSnapshot::Capture(*scene), "unknown component dropped");
        Check(snapshot.Restore() && world.get<Unknown>(handle).Value == 7, "unknown component overwritten");
        world.remove<Unknown>(handle);
        Editor::SceneEditHistory history;
        history.Synchronize(scene.get(), true);
        const auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
        world.get<Engine::NameComponent>(handle).name = L"new";
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, targets), "edit");
        Check(scene->DestroyEntity(entity), "external delete");
        auto& replacement = Add(*scene, L"replacement");
        Check(!history.Undo() && history.Commands().GetUndoCount() == 1 &&
            world.get<Engine::NameComponent>(replacement.GetEntityHandle()).name == L"replacement", "stale target");
        history.Clear();
        const auto wholeBefore = Editor::SceneEditSnapshot::Capture(*scene);
        world.get<Engine::NameComponent>(replacement.GetEntityHandle()).name = L"edited";
        history.Record(wholeBefore, Editor::SceneEditSnapshot::Capture(*scene), "whole");
        const auto outsideId = world.get<Engine::IDComponent>(Add(*scene, L"outside").GetEntityHandle()).id;
        Check(!history.Undo() && scene->FindEntityById(outsideId) && history.Commands().GetUndoCount() == 1,
            "unrecorded entity was deleted by whole restore");
        history.Clear();
        auto retained = Editor::SceneEditSnapshot::Capture(*scene);
        auto* raw = scene.release();
        Check(raw->Release() > 0 && retained.Restore(), "scene lifetime");
    }
    void ReferencesAndUI()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& owner = Add(*scene, L"joint");
        auto& target = Add(*scene, L"connected");
        auto& world = scene->GetRegistry();
        const auto ownerHandle = owner.GetEntityHandle();
        const auto targetId = world.get<Engine::IDComponent>(target.GetEntityHandle()).id;
        world.emplace<Engine::JointComponent>(ownerHandle).ConnectedEntity = target.GetEntityHandle();
        world.emplace<Engine::Joint2DComponent>(ownerHandle).ConnectedEntity = target.GetEntityHandle();
        Editor::SceneEditHistory history;
        history.Synchronize(scene.get(), true);
        auto before = Editor::SceneEditSnapshot::Capture(*scene);
        Check(bool(before) && scene->DestroyEntity(target), "joint delete");
        Check(world.get<Engine::JointComponent>(ownerHandle).ConnectedEntity == entt::null, "dangling joint");
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene), "delete");
        Check(history.Undo(), "joint undo");
        const auto recreated = scene->FindEntityById(targetId)->GetEntityHandle();
        Check(world.get<Engine::JointComponent>(ownerHandle).ConnectedEntity == recreated &&
            world.get<Engine::Joint2DComponent>(ownerHandle).ConnectedEntity == recreated, "joint ID remap");
        auto& canvas = scene->CreateEntity();
        const auto canvasHandle = canvas.GetEntityHandle();
        world.emplace<Engine::CanvasComponent>(canvasHandle);
        world.emplace<Engine::HierarchyComponent>(canvasHandle);
        auto& image = scene->CreateEntity();
        const auto imageHandle = image.GetEntityHandle();
        world.emplace<Engine::RectTransformComponent>(imageHandle);
        world.emplace<Engine::UIImageComponent>(imageHandle);
        world.emplace<Engine::HierarchyComponent>(imageHandle).parent = canvasHandle;
        world.get<Engine::HierarchyComponent>(canvasHandle).children.push_back(imageHandle);
        const std::array uiTargets{ canvasHandle, imageHandle };
        before = Editor::SceneEditSnapshot::Capture(*scene, uiTargets);
        world.get<Engine::RectTransformComponent>(imageHandle).anchoredPosition = glm::vec2(55, 80);
        world.get<Engine::CanvasComponent>(canvasHandle).referenceResolution = glm::vec2(800, 600);
        const auto after = Editor::SceneEditSnapshot::Capture(*scene, uiTargets);
        history.Record(before, after, "UI");
        Check(history.Undo() &&
            world.get<Engine::RectTransformComponent>(imageHandle).anchoredPosition == glm::vec2(0), "UI undo");
        Check(history.Redo() &&
            world.get<Engine::CanvasComponent>(canvasHandle).referenceResolution == glm::vec2(800, 600), "UI redo");
        std::vector<Engine::UIElementLayout> layouts;
        Check(Engine::UILayout::Build(world, canvasHandle, layouts), "UI layout after redo");
        const std::array orderTargets{ imageHandle };
        before = Editor::SceneEditSnapshot::Capture(*scene, orderTargets);
        world.emplace<Engine::UIRenderOrderComponent>(imageHandle).zIndex = 42;
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, orderTargets), "order");
        Check(history.Undo() && !world.all_of<Engine::UIRenderOrderComponent>(imageHandle) &&
            Engine::UILayout::GetZIndex(world, imageHandle) == 0, "order addition undo");
        Check(history.Redo() && Engine::UILayout::GetZIndex(world, imageHandle) == 42, "order addition redo");
        before = Editor::SceneEditSnapshot::Capture(*scene, orderTargets);
        world.get<Engine::UIRenderOrderComponent>(imageHandle).zIndex = -15;
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, orderTargets), "order");
        Check(history.Undo() && Engine::UILayout::GetZIndex(world, imageHandle) == 42, "order value undo");
        Check(history.Redo() && Engine::UILayout::GetZIndex(world, imageHandle) == -15, "order value redo");
        auto& bare = scene->CreateEntity();
        const auto bareHandle = bare.GetEntityHandle();
        const std::array bareTargets{ bareHandle };
        before = Editor::SceneEditSnapshot::Capture(*scene, bareTargets);
        const auto* entry = Engine::ReflectionRegistry::Get().FindComponent(typeid(Engine::CameraComponent));
        Check(entry && entry->TryAdd(world, bareHandle), "camera dependencies");
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, bareTargets), "add");
        Check(history.Undo() && !world.any_of<Engine::CameraComponent, Engine::TransformComponent,
            Engine::WorldTransformComponent, Engine::FlagComponent>(bareHandle), "added dependencies survived undo");
        history.Clear();
        uint64 buttonId = 0;
        {
            Editor::SceneEditScope buttonEdit(history, *scene, "button");
            auto& button = scene->CreateEntity();
            const auto buttonHandle = button.GetEntityHandle();
            buttonId = world.get<Engine::IDComponent>(buttonHandle).id;
            {
                Editor::SceneEditScope imageEdit(history, *scene, "image");
                world.emplace<Engine::RectTransformComponent>(buttonHandle);
                world.emplace<Engine::UIImageComponent>(buttonHandle);
                world.emplace<Engine::HierarchyComponent>(buttonHandle).parent = canvasHandle;
                world.get<Engine::HierarchyComponent>(canvasHandle).children.push_back(buttonHandle);
            }
            world.emplace<Engine::UIButtonComponent>(buttonHandle);
        }
        Check(history.Commands().GetUndoCount() == 1, "nested factory split history");
        Check(history.Undo() && !scene->FindEntityById(buttonId), "button undo");
        Check(history.Redo() && world.all_of<Engine::UIButtonComponent>(
            scene->FindEntityById(buttonId)->GetEntityHandle()), "button redo");
    }
    void DuplicateHierarchyRejection()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& parent = Add(*scene, L"parent");
        auto& child = Add(*scene, L"child");
        auto& other = Add(*scene, L"other");
        auto& world = scene->GetRegistry();
        const auto p = parent.GetEntityHandle(), c = child.GetEntityHandle(), o = other.GetEntityHandle();

        Check(scene->ReparentEntity(child, &parent) == Engine::EntityReparentResult::Success, "parent setup");

        const std::array targets{ o };
        const auto snapshot = Editor::SceneEditSnapshot::Capture(*scene, targets);
        Check(bool(snapshot), "snapshot setup");

        world.get<Engine::NameComponent>(o).name = L"edited";
        world.get<Engine::HierarchyComponent>(p).children.push_back(c);

        Check(!Editor::SceneEditSnapshot::Capture(*scene), "duplicate child accepted during capture");
        Check(!snapshot.Restore() && world.get<Engine::NameComponent>(o).name == L"edited" &&
            world.get<Engine::HierarchyComponent>(p).children == std::vector{ c, c },
            "rejected restore changed the scene");

        world.get<Engine::HierarchyComponent>(p).children.pop_back();
        Check(snapshot.Restore() && world.get<Engine::NameComponent>(o).name == L"other",
            "valid hierarchy restore failed");
    }

    struct TestDocument final : Editor::IEditorDocument
    {
        int& Alive;
        std::filesystem::path Path;
        bool Dirty = false;
        bool Writable = false;

        explicit TestDocument(int& alive) : Alive(alive) { ++Alive; }
        ~TestDocument() override { --Alive; }

        const std::filesystem::path& GetPath() const override { return Path; }
        bool HasUnsavedChanges() const override { return Dirty; }
        bool CanSave() const override { return Writable; }

        Engine::EResult SaveTo(const std::filesystem::path&) override
        {
            return Engine::EResult::NotImplemented;
        }

        Engine::EResult DiscardChanges() override
        {
            return Engine::EResult::NotImplemented;
        }
        Editor::EditorDocumentSaveInfo GetSaveInfo() const override { return {}; }
    };

    void DocumentOwnership()
    {
        using namespace Editor;

        int alive = 0;
        {
            EditorDocumentManager documents;
            Check(documents.Add(nullptr) == InvalidEditorDocumentId, "null document accepted");

            const auto first = documents.Add(std::make_unique<TestDocument>(alive));
            auto modified = std::make_unique<TestDocument>(alive);
            modified->Dirty = true;
            const auto second = documents.Add(std::move(modified));

            Check(first != InvalidEditorDocumentId && second != first && alive == 2, "document registration");
            Check(documents.GetActiveId() == first, "initial active document");
            Check(documents.GetDocumentIds() == std::vector<EditorDocumentId>{ first, second }, "document order");
            Check(documents.GetModifiedDocumentIds() == std::vector<EditorDocumentId>{ second },
                "modified document collection");

            Check(documents.SetActive(second), "activate document");
            Check(!documents.SetActive(second + 1) && documents.GetActiveId() == second,
                "invalid activation changed active document");

            Check(documents.Remove(second) && alive == 1, "removed document not destroyed");
            Check(!documents.Find(second) && documents.GetActiveId() == InvalidEditorDocumentId,
                "removed document still accessible");
            Check(!documents.Remove(second) && alive == 1, "repeated removal changed ownership");

            documents.Clear();
            Check(alive == 0 && !documents.Find(first), "clear retained documents");

            const auto third = documents.Add(std::make_unique<TestDocument>(alive));
            Check(third > second && !documents.Find(first) && !documents.Find(second), "stale document ID reused");
        }

        Check(alive == 0, "manager destruction retained documents");
    }
    void SceneDocumentPersistence()
    {
        using Engine::EResult;

        struct ManagerScope
        {
            ManagerScope()
            {
                Check(!Engine::SceneManager::IsCreated(), "scene manager already exists");
                Check(Engine::SceneManager::Create() != nullptr, "scene manager creation");
            }

            ~ManagerScope()
            {
                Engine::SceneManager::Destroy();
            }
        } managerScope;

        auto& scenes = Engine::SceneManager::Get();
        Check(scenes.NewScene() == EResult::Success, "document scene creation");

        Editor::SceneEditHistory history;
        history.ResetDocument(scenes.GetCurrentScene(), true);

        Editor::SceneDocument document(scenes, history);
        Check(!document.CanSave(), "document unexpectedly writable");
        document.SetReadOnly(false);

        const auto directory = std::filesystem::current_path() / "scene-document-check";
        std::filesystem::create_directories(directory);
        const auto path = directory / "document.scene.json";

        {
            Editor::SceneEditScope edit(history, *document.GetScene(), "saved name");
            document.GetScene()->SetName(L"saved");
        }

        Check(document.SaveTo(path) == EResult::Success, "document save");
        const auto savedPath = document.GetPath();
        Check(savedPath.is_absolute() && !document.HasUnsavedChanges(), "saved document state");

        {
            Editor::SceneEditScope edit(history, *document.GetScene(), "draft name");
            document.GetScene()->SetName(L"draft");
        }

        Check(document.HasUnsavedChanges(), "document edit not tracked");
        Check(history.Undo() && !document.HasUnsavedChanges(), "undo did not return to saved state");
        Check(history.Redo() && document.HasUnsavedChanges(), "redo did not restore dirty state");

        Check(document.SaveTo(directory) != EResult::Success, "directory accepted as save file");
        Check(document.GetPath() == savedPath && document.HasUnsavedChanges(), "failed save changed document state");
        Check(document.GetScene()->GetName() == L"draft", "failed save changed draft");

        Check(document.DiscardChanges() == EResult::Success, "document discard");
        Check(document.GetScene() == scenes.GetCurrentScene(), "document and manager scene mismatch");
        Check(document.GetScene()->GetName() == L"saved" && !document.HasUnsavedChanges(),
            "discard did not restore saved content");

        {
            Editor::SceneEditScope edit(history, *document.GetScene(), "retained draft");
            document.GetScene()->SetName(L"retained");
        }

        auto* retainedScene = document.GetScene();
        const auto retainedRevision = history.Revision();

        {
            std::ofstream damaged(savedPath, std::ios::binary | std::ios::trunc);
            damaged << "{";
            damaged.close();
            Check(bool(damaged), "test file corruption failed");
        }

        Check(document.DiscardChanges() != EResult::Success, "corrupt document restored");
        Check(document.GetScene() == retainedScene && scenes.GetCurrentScene() == retainedScene,
            "failed discard replaced scene");
        Check(document.GetScene()->GetName() == L"retained" && document.HasUnsavedChanges(),
            "failed discard lost draft");
        Check(history.Revision() == retainedRevision, "failed discard changed history");

        document.SetReadOnly(true);
        Check(document.SaveTo(path) == EResult::AccessDenied, "read-only document saved");
        Check(document.DiscardChanges() == EResult::AccessDenied, "read-only document discarded");
        document.SetReadOnly(false);

        {
            Editor::SceneEditScope edit(history, *document.GetScene(), "active edit");
            Check(document.SaveTo(path) == EResult::AccessDenied, "save accepted during active edit");
        }

        Check(scenes.NewScene() == EResult::Success, "external scene replacement");
        Check(!document.CanSave(), "stale scene document writable");
        Check(document.SaveTo(path) == EResult::AccessDenied, "stale scene document saved");

        const auto dirtyBeforeRebind = document.HasUnsavedChanges();
        const auto pathBeforeRebind = document.GetPath();

        Check(document.RebindRestoredScene() == EResult::Success, "restored scene rebind");
        Check(document.GetScene() == scenes.GetCurrentScene() && document.CanSave(), "rebound document unavailable");
        Check(document.GetPath() == pathBeforeRebind &&
            document.HasUnsavedChanges() == dirtyBeforeRebind, "rebind changed document save state");
    }
    void DocumentActionProtection()
    {
        using namespace Editor;
        using Engine::EResult;

        int alive = 0;
        EditorDocumentManager documents;
        EditorDocumentController controller(documents);

        auto owned = std::make_unique<TestDocument>(alive);
        auto* document = owned.get();
        document->Dirty = true;
        document->Writable = true;
        const auto id = documents.Add(std::move(owned));

        int executions = 0;
        const auto successAction = [&](const std::vector<EditorDocumentId>& discarded) -> EResult
            {
                Check(discarded.empty(), "unexpected discard approval");
                ++executions;
                return EResult::Success;
            };

        Check(controller.RequestAction({ id }, successAction), "request guarded action");
        Check(!controller.ProcessPendingAction() && controller.GetConfirmationDocumentId() == id,
            "dirty document did not require confirmation");
        Check(!controller.RequestSave(id), "independent save accepted during confirmation");
        Check(controller.ResolveConfirmation(EDocumentActionChoice::Cancel), "cancel confirmation");
        Check(!controller.HasPendingAction() && executions == 0 && document->Dirty, "cancel changed document");

        Check(controller.RequestAction({ id }, [&](const std::vector<EditorDocumentId>& discarded) -> EResult
            {
                Check(discarded == std::vector<EditorDocumentId>{ id }, "discard approval missing");
                Check(document->Dirty, "draft discarded before replacement");
                ++executions;
                return EResult::Fail;
            }), "request failing replacement");

        controller.ProcessPendingAction();
        Check(controller.ResolveConfirmation(EDocumentActionChoice::Discard), "approve discard");
        const auto failed = controller.ProcessPendingAction();
        Check(failed && *failed == EResult::Fail && document->Dirty && documents.Find(id),
            "failed replacement lost document");

        for (const auto status : { EDocumentSaveStatus::NotSelected,
            EDocumentSaveStatus::Failed, EDocumentSaveStatus::Unavailable })
        {
            Check(controller.RequestAction({ id }, successAction), "request action before failed save");
            controller.ProcessPendingAction();
            Check(controller.ResolveConfirmation(EDocumentActionChoice::Save), "choose save");
            controller.NotifySaveResult({ id, status });
            Check(!controller.HasPendingAction() && executions == 1 && document->Dirty,
                "incomplete save allowed replacement");
        }

        Check(controller.RequestAction({ id }, successAction), "request action before successful save");
        controller.ProcessPendingAction();
        Check(controller.ResolveConfirmation(EDocumentActionChoice::Save), "choose successful save");

        // 실제 파일 저장은 SceneDocumentPersistence에서 검사한다.
        document->Dirty = false;
        controller.NotifySaveResult({ id, EDocumentSaveStatus::Saved });

        const auto saved = controller.ProcessPendingAction();
        Check(saved && *saved == EResult::Success && executions == 2, "saved document did not allow action");

        document->Dirty = true;
        Check(controller.RequestAction({ id }, successAction), "request action before removal");
        controller.ProcessPendingAction();
        documents.Remove(id);

        const auto missing = controller.ProcessPendingAction();
        Check(missing && *missing == EResult::Fail && executions == 2, "missing document action executed");
    }
}
int main()
{
    try
    {
        Check(Engine::ReflectionRegistry::Create() != nullptr, "reflection init");
        HistoryOwnership();
        DocumentOwnership();
        HistoryStateIdentity();
        SceneDocumentState();
        ComponentAndGesture();
        StructureAndIdentity();
        DuplicateHierarchyRejection();
        FailureAndLifetime();
        ReferencesAndUI();
        SceneDocumentPersistence();
        DocumentActionProtection();
        Engine::ReflectionRegistry::Destroy();
        std::cout << "Editor history checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        Engine::ReflectionRegistry::Destroy();
        return 1;
    }
}
