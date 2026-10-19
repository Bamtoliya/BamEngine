#include "Engine_Includes.h"
#include "EntityHierarchyView.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "LocalizationManager.h"
#include "InputManager.h"
#include <archive/JsonArchive.h>
#include <imgui.h>
#include <iostream>
#include <memory>

namespace
{
    using Result = Engine::EntityReparentResult;
    struct SceneRelease { void operator()(Engine::Scene* value) const { if (value) value->Release(); } };
    using ScenePtr = std::unique_ptr<Engine::Scene, SceneRelease>;
    bool Fail(const char* message) { std::cerr << message << '\n'; return false; }
    Engine::Entity& Add(Engine::Scene& scene, glm::vec3 position = {}, const wchar_t* name = L"Entity")
    {
        auto& entity = scene.CreateEntity();
        const auto handle = entity.GetEntityHandle();
        auto& world = scene.GetRegistry();
        world.emplace<Engine::NameComponent>(handle, name);
        world.emplace<Engine::TransformComponent>(handle).position = position;
        world.emplace<Engine::WorldTransformComponent>(handle);
        world.emplace<Engine::HierarchyComponent>(handle);
        return entity;
    }
    bool Near(const glm::mat4& a, const glm::mat4& b)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(a[c][r]) || !std::isfinite(b[c][r]) ||
                    std::abs(a[c][r] - b[c][r]) > 0.001f) return false;
        return true;
    }
    bool CheckMoves()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& parent = Add(*scene, { 10.0f, 2.0f, 0.0f });
        auto& other = Add(*scene, { -4.0f, 0.0f, 0.0f });
        auto& child = Add(*scene, { 3.0f, 0.0f, 0.0f });
        auto& grandchild = Add(*scene, { 5.0f, 0.0f, 0.0f });
        auto& world = scene->GetRegistry();
        const auto p = parent.GetEntityHandle(), c = child.GetEntityHandle(), g = grandchild.GetEntityHandle();
        if (scene->ReparentEntity(grandchild, &child) != Result::Success) return Fail("Initial parenting failed.");
        Engine::TransformSystem::UpdateHierarchy(world);
        const auto original = world.get<Engine::WorldTransformComponent>(c).worldMatrix;
        const auto descendant = world.get<Engine::WorldTransformComponent>(g).worldMatrix;
        if (scene->ReparentEntity(child, &parent) != Result::Success ||
            world.get<Engine::HierarchyComponent>(c).parent != p ||
            world.get<Engine::HierarchyComponent>(p).children != std::vector{ c } ||
            !Near(original, world.get<Engine::WorldTransformComponent>(c).worldMatrix) ||
            !Near(descendant, world.get<Engine::WorldTransformComponent>(g).worldMatrix))
            return Fail("Parenting changed world pose or lost reciprocal links.");
        if (scene->ReparentEntity(parent, &grandchild) != Result::Cycle ||
            scene->ReparentEntity(child, &child) != Result::Cycle ||
            world.get<Engine::HierarchyComponent>(p).parent != entt::null)
            return Fail("A self or descendant cycle was accepted.");
        if (scene->ReparentEntity(child, &parent) != Result::Unchanged ||
            world.get<Engine::HierarchyComponent>(p).children.size() != 1)
            return Fail("Repeated parenting duplicated a child.");
        if (scene->ReparentEntity(child, &other) != Result::Success ||
            !world.get<Engine::HierarchyComponent>(p).children.empty() ||
            !Near(original, world.get<Engine::WorldTransformComponent>(c).worldMatrix))
            return Fail("Moving between parents failed.");
        archive::JsonArchive writer;
        Engine::Scene* loaded = nullptr;
        if (!scene->WriteArchive(writer) || !Engine::Scene::CreateFromArchive(writer, loaded))
            return Fail("Edited hierarchy did not archive and restore.");
        ScenePtr restored(loaded);
        const auto childId = world.get<Engine::IDComponent>(c).id;
        const auto parentId = world.get<Engine::IDComponent>(other.GetEntityHandle()).id;
        const auto restoredChild = restored->FindEntityById(childId)->GetEntityHandle();
        if (restored->GetRegistry().get<Engine::HierarchyComponent>(restoredChild).parent !=
            restored->FindEntityById(parentId)->GetEntityHandle()) return Fail("Restored parent differs.");
        if (scene->ReparentEntity(child) != Result::Success ||
            world.get<Engine::HierarchyComponent>(c).parent != entt::null ||
            !Near(original, world.get<Engine::WorldTransformComponent>(c).worldMatrix))
            return Fail("Moving to root changed the world pose.");
        ScenePtr second(Engine::Scene::Create());
        auto& foreign = Add(*second);
        if (scene->ReparentEntity(child, &foreign) != Result::InvalidEntity)
            return Fail("Cross-scene parenting accepted an aliased handle.");
        world.get<Engine::TransformComponent>(p).scale = glm::vec3(0.0f);
        const auto local = world.get<Engine::TransformComponent>(c).GetLocalMatrix();
        if (scene->ReparentEntity(child, &parent) != Result::InvalidTransform ||
            !Near(local, world.get<Engine::TransformComponent>(c).GetLocalMatrix()) ||
            world.get<Engine::HierarchyComponent>(c).parent != entt::null)
            return Fail("A singular parent changed the original.");
        world.get<Engine::TransformComponent>(p).scale = { 2.0f, 1.0f, 1.0f };
        if (!world.get<Engine::TransformComponent>(c).SetEulerRotation({ 0.0f, 0.0f, 45.0f })) return false;
        if (scene->ReparentEntity(child, &parent) != Result::InvalidTransform)
            return Fail("An unrepresentable shear was silently approximated.");
        world.get<Engine::TransformComponent>(p).scale = { -2.0f, 2.0f, 2.0f };
        if (scene->ReparentEntity(child, &parent) != Result::Success)
            return Fail("A representable mirrored parent was rejected.");
        return true;
    }
    bool CheckInvalidAndDepth()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& first = Add(*scene);
        auto& second = Add(*scene);
        auto& world = scene->GetRegistry();
        auto& relation = world.get<Engine::HierarchyComponent>(first.GetEntityHandle());
        relation.parent = second.GetEntityHandle();
        if (scene->ReparentEntity(first) != Result::InvalidHierarchy)
            return Fail("Inconsistent links were silently repaired by a move.");
        relation.parent = entt::null;
        auto* parent = &first;
        for (int i = 1; i < 256; ++i)
        {
            auto& child = Add(*scene);
            world.get<Engine::HierarchyComponent>(child.GetEntityHandle()).parent = parent->GetEntityHandle();
            world.get<Engine::HierarchyComponent>(parent->GetEntityHandle()).children.push_back(
                child.GetEntityHandle());
            parent = &child;
        }
        if (scene->ReparentEntity(second, parent) != Result::DepthLimit)
            return Fail("A move deeper than the scene archive limit was accepted.");
        return true;
    }
    bool CheckTreeInput()
    {
        ImGui::CreateContext();
        struct UiScope { ~UiScope() { ImGui::DestroyContext(); } } uiScope;
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.DisplaySize = ImVec2(800.0f, 600.0f);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        ScenePtr scene(Engine::Scene::Create());
        auto& first = Add(*scene);
        auto& second = Add(*scene);
        const auto a = first.GetEntityHandle(), b = second.GetEntityHandle();
        Editor::EntityHierarchyView view;
        float rowHeight = 0.0f, textHeight = 0.0f, renderedHeight = 0.0f;
        const auto frame = [&](Engine::Scene& drawn, entt::entity reveal = entt::null)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(ImVec2(700.0f, 500.0f));
            ImGui::Begin("HierarchyInputCheck");
            ImGui::SetCursorScreenPos(ImVec2(20.0f, 40.0f));
            rowHeight = ImGui::GetTextLineHeightWithSpacing();
            textHeight = ImGui::GetTextLineHeight();
            const auto result = view.Draw(drawn, {}, reveal);
            renderedHeight = ImGui::GetCursorScreenPos().y - 40.0f;
            ImGui::End();
            ImGui::Render();
            return result;
        };
        frame(*scene);
        io.AddMousePosEvent(75.0f, 40.0f + textHeight * 0.5f);
        frame(*scene);
        io.AddMouseButtonEvent(0, true);
        if (frame(*scene).Clicked != a) return Fail("Tree click did not select the named row.");
        io.AddMousePosEvent(75.0f, 40.0f + rowHeight + textHeight * 0.5f);
        frame(*scene);
        frame(*scene);
        io.AddMouseButtonEvent(0, false);
        const auto dropped = frame(*scene);
        if (!dropped.Move || dropped.Move->Child != a || dropped.Move->Parent != b ||
            scene->ReparentEntity(first, &second) != Result::Success)
            return Fail("Native ImGui entity drag/drop did not produce the correct move.");
        frame(*scene, a);
        const float expanded = renderedHeight;
        io.AddMousePosEvent(25.0f, 40.0f + textHeight * 0.5f);
        frame(*scene);
        io.AddMouseButtonEvent(0, true);
        const auto arrow = frame(*scene);
        io.AddMouseButtonEvent(0, false);
        frame(*scene);
        frame(*scene);
        if (arrow.Clicked != entt::null || renderedHeight >= expanded)
            return Fail("Arrow toggling selected an entity or failed to keep the branch collapsed.");
        frame(*scene, a);
        if (renderedHeight < expanded) return Fail("Revealing a selection did not expand its ancestors.");
        io.AddMousePosEvent(75.0f, 40.0f + rowHeight + textHeight * 0.5f);
        frame(*scene);
        io.AddMouseButtonEvent(0, true);
        frame(*scene);
        io.AddMousePosEvent(75.0f, 40.0f + 2.0f * rowHeight + textHeight * 0.5f);
        frame(*scene);
        frame(*scene);
        io.AddMouseButtonEvent(0, false);
        const auto rootDrop = frame(*scene);
        if (!rootDrop.Move || rootDrop.Move->Child != a || rootDrop.Move->Parent != entt::null ||
            scene->ReparentEntity(first) != Result::Success)
            return Fail("The root drop area failed to detach the dragged entity.");
        frame(*scene);
        view.ResetDrag();

        ScenePtr source(Engine::Scene::Create());
        const auto from = Add(*source).GetEntityHandle();
        Add(*source);
        ScenePtr destination(Engine::Scene::Create());
        Add(*destination);
        Add(*destination);
        frame(*source);
        io.AddMousePosEvent(75.0f, 40.0f + textHeight * 0.5f);
        frame(*source);
        io.AddMouseButtonEvent(0, true);
        frame(*source);
        io.AddMousePosEvent(75.0f, 40.0f + rowHeight + textHeight * 0.5f);
        frame(*source);
        frame(*source);
        auto* heldSource = source.release();
        if (heldSource->Release() != 1) return Fail("A drag did not retain its source scene.");
        frame(*destination);
        frame(*destination);
        io.AddMouseButtonEvent(0, false);
        const auto foreign = frame(*destination);
        if (!foreign.RejectedDrop || foreign.Move ||
            heldSource->GetRegistry().get<Engine::HierarchyComponent>(from).parent != entt::null)
            return Fail("A cross-scene drop was accepted or modified its source.");
        view.ResetDrag();
        return true;
    }

    bool CheckTree()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& parent = Add(*scene);
        auto& first = Add(*scene);
        auto& second = Add(*scene);
        auto& world = scene->GetRegistry();
        const auto p = parent.GetEntityHandle(), a = first.GetEntityHandle(), b = second.GetEntityHandle();
        world.remove<Engine::NameComponent>(a);
        world.get<Engine::HierarchyComponent>(p).children = { b, a };
        world.get<Engine::HierarchyComponent>(a).parent = p;
        world.get<Engine::HierarchyComponent>(b).parent = p;
        const auto raw = world.create();
        world.emplace<Engine::NameComponent>(raw, L"Unwrapped");
        auto tree = Editor::BuildEntityHierarchyTree(*scene);
        if (tree.Roots != std::vector{ p } || tree.Children.at(p) != std::vector{ b, a } ||
            tree.Parents.contains(raw) || !tree.Parents.contains(a))
            return Fail("Tree lost child order, nameless entities, or scene ownership.");
        world.get<Engine::HierarchyComponent>(p).parent = b;
        world.get<Engine::HierarchyComponent>(b).children = { p };
        tree = Editor::BuildEntityHierarchyTree(*scene);
        std::unordered_set<entt::entity> visited;
        const auto visit = [&](auto&& self, entt::entity entity) -> bool
        {
            if (!visited.insert(entity).second) return false;
            for (const auto child : tree.Children.at(entity)) if (!self(self, child)) return false;
            return true;
        };
        for (const auto root : tree.Roots) if (!visit(visit, root)) return Fail("View contains a cycle.");
        if (visited.size() != 3 || tree.Invalid.empty() || world.get<Engine::HierarchyComponent>(p).parent != b)
            return Fail("Invalid relations hid entities or mutated the source.");
        return true;
    }
}

int main()
{
    if (!Engine::LocalizationManager::Create() || !Engine::ReflectionRegistry::Create() ||
        !Engine::InputManager::Create()) return 1;
    const bool passed = CheckMoves() && CheckInvalidAndDepth() && CheckTree() && CheckTreeInput();
    Engine::ReflectionRegistry::Destroy();
    Engine::LocalizationManager::Destroy();
    Engine::InputManager::Destroy();
    return passed ? 0 : 1;
}
