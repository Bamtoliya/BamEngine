#include "SceneEditHistory.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include "UIComponents.h"
#include "UILayout.h"
#include <iostream>
#include <stdexcept>

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
}
int main()
{
    try
    {
        Check(Engine::ReflectionRegistry::Create() != nullptr, "reflection init");
        HistoryOwnership();
        ComponentAndGesture();
        StructureAndIdentity();
        FailureAndLifetime();
        ReferencesAndUI();
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
