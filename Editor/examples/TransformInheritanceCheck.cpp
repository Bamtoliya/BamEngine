#include "Scene.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "TransformPropertyEditor.h"
#include "SceneEditHistory.h"
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include <stdexcept>
#include <iostream>

namespace
{
    using Flag = Engine::ETransformAbsolute;
    using System = Engine::TransformSystem;
    struct Release { void operator()(Engine::Scene* scene) const { if (scene) scene->Release(); } };
    using ScenePtr = std::unique_ptr<Engine::Scene, Release>;
    void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    bool Near(const glm::mat4& a, const glm::mat4& b)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(a[c][r]) || !std::isfinite(b[c][r]) || std::abs(a[c][r] - b[c][r]) > 0.002f)
                    return false;
        return true;
    }
    Engine::Entity& Add(Engine::Scene& scene)
    {
        auto& value = scene.CreateEntity();
        auto& world = scene.GetRegistry();
        const auto entity = value.GetEntityHandle();
        world.emplace<Engine::TransformComponent>(entity);
        world.emplace<Engine::WorldTransformComponent>(entity);
        world.emplace<Engine::HierarchyComponent>(entity);
        return value;
    }
    void Link(Engine::Scene& scene, Engine::Entity& child, Engine::Entity& parent)
    {
        auto& world = scene.GetRegistry();
        world.get<Engine::HierarchyComponent>(child.GetEntityHandle()).parent = parent.GetEntityHandle();
        world.get<Engine::HierarchyComponent>(parent.GetEntityHandle()).children.push_back(child.GetEntityHandle());
    }
    void Combinations()
    {
        Engine::TransformComponent parent, child;
        parent.position = { 10, 20, 30 };
        parent.scale = { 2, 3, 4 };
        Check(parent.SetEulerRotation({ 0, 0, 90 }), "parent rotation");
        child.position = { 1, 2, 3 };
        child.scale = { 3, 4, 5 };
        Check(child.SetEulerRotation({ 0, 0, 30 }), "child rotation");
        const auto parentMatrix = parent.GetLocalMatrix();
        for (unsigned bits = 0; bits != 8; ++bits)
        {
            child.absolute = static_cast<Flag>(bits);
            glm::mat4 actual;
            Check(System::TryComposeWorld(child, parentMatrix, actual), "flag compose");
            const auto p = bits & 1 ? glm::vec4(child.position, 1) : parentMatrix * glm::vec4(child.position, 1);
            glm::mat4 expected;
            if (!(bits & 6)) expected = parentMatrix * child.GetLocalMatrix();
            else
            {
                const auto rotation = bits & 2 ? child.rotation : parent.rotation * child.rotation;
                const auto scale = bits & 4 ? child.scale : parent.scale * child.scale;
                expected = glm::mat4_cast(rotation) * glm::scale(glm::mat4(1), scale);
            }
            expected[3] = p;
            Check(Near(actual, expected), "flag channel semantics");
            glm::dmat4 local;
            auto recovered = child;
            Check(System::TryGetLocalMatrix(parentMatrix, child.absolute, actual, local) &&
                System::TryAssignLocalMatrix(recovered, local), "flag inverse");
            glm::mat4 rebuilt;
            Check(System::TryComposeWorld(recovered, parentMatrix, rebuilt) && Near(actual, rebuilt), "flag round trip");
        }
        child.absolute = Flag::All;
        glm::mat4 result;
        Check(System::TryComposeWorld(child, glm::scale(glm::mat4(1), glm::vec3(0)), result) &&
            Near(result, child.GetLocalMatrix()), "absolute under zero scale");
        parent.scale = { -2, -3, -4 };
        for (unsigned bits = 0; bits != 8; ++bits)
        {
            child.absolute = static_cast<Flag>(bits);
            glm::dmat4 local;
            Check(System::TryComposeWorld(child, parent.GetLocalMatrix(), result) &&
                System::TryGetLocalMatrix(parent.GetLocalMatrix(), child.absolute, result, local), "mirrored parent");
            auto recovered = child;
            Check(System::TryAssignLocalMatrix(recovered, local), "mirrored local");
            glm::mat4 rebuilt;
            Check(System::TryComposeWorld(recovered, parent.GetLocalMatrix(), rebuilt) &&
                Near(rebuilt, result), "mirrored compose");
        }
    }
    template<typename Archive>
    void SceneRoundTrip(Engine::Scene& scene, uint64 id, Flag flags, const glm::mat4& expected)
    {
        Archive ar;
        Check(bool(scene.WriteArchive(ar)), "scene write flags");
        Engine::Scene* raw = nullptr;
        Check(bool(Engine::Scene::CreateFromArchive(ar, raw)), "scene read flags");
        ScenePtr loaded(raw);
        const auto entity = loaded->FindEntityById(id)->GetEntityHandle();
        Check(loaded->GetRegistry().get<Engine::TransformComponent>(entity).absolute == flags &&
            Near(loaded->GetRegistry().get<Engine::WorldTransformComponent>(entity).worldMatrix, expected),
            "scene flag/world round trip");
    }
    void HierarchyAndHistory()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& parent = Add(*scene);
        auto& child = Add(*scene);
        auto& grandchild = Add(*scene);
        auto& other = Add(*scene);
        Link(*scene, child, parent);
        Link(*scene, grandchild, child);
        auto& world = scene->GetRegistry();
        const auto p = parent.GetEntityHandle(), c = child.GetEntityHandle(), g = grandchild.GetEntityHandle();
        auto& pt = world.get<Engine::TransformComponent>(p);
        auto& ct = world.get<Engine::TransformComponent>(c);
        pt.position = { 8, 12, 16 };
        pt.scale = glm::vec3(2);
        Check(pt.SetEulerRotation({ 0, 0, 35 }), "parent rotation");
        ct.position = { 1, 2, 3 };
        Check(ct.SetEulerRotation({ 0, 0, 20 }), "child rotation");
        world.get<Engine::TransformComponent>(g).position = { 5, 0, 0 };
        world.get<Engine::TransformComponent>(other.GetEntityHandle()).position = { -8, 3, 0 };
        Check(System::UpdateHierarchy(world), "initial update");
        const auto childWorld = world.get<Engine::WorldTransformComponent>(c).worldMatrix;
        const auto descendantWorld = world.get<Engine::WorldTransformComponent>(g).worldMatrix;
        Editor::SceneEditHistory history;
        history.Synchronize(scene.get(), true);
        for (unsigned bits = 0; bits != 8; ++bits)
        {
            const std::array targets{ c };
            const auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
            Check(System::TrySetAbsolute(world, c, static_cast<Flag>(bits)), "toggle absolute");
            Check(Near(world.get<Engine::WorldTransformComponent>(c).worldMatrix, childWorld) &&
                Near(world.get<Engine::WorldTransformComponent>(g).worldMatrix, descendantWorld), "toggle moved pose");
            history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, targets), "absolute");
            Check(history.Undo() && history.Redo(), "absolute undo redo");
            const auto id = world.get<Engine::IDComponent>(c).id;
            SceneRoundTrip<archive::JsonArchive>(*scene, id, static_cast<Flag>(bits), childWorld);
            SceneRoundTrip<archive::BeveArchive>(*scene, id, static_cast<Flag>(bits), childWorld);
        }
        Check(System::TrySetAbsolute(world, c, Flag::All), "absolute all");
        pt.position += glm::vec3(100);
        Check(System::UpdateHierarchy(world) &&
            Near(world.get<Engine::WorldTransformComponent>(c).worldMatrix, childWorld), "independent position");
        Check(scene->ReparentEntity(child, &other) == Engine::EntityReparentResult::Success &&
            Near(world.get<Engine::WorldTransformComponent>(c).worldMatrix, childWorld), "absolute reparent");
        Check(scene->ReparentEntity(child) == Engine::EntityReparentResult::Success &&
            Near(world.get<Engine::WorldTransformComponent>(g).worldMatrix, descendantWorld), "absolute detach");
        auto* lock = Editor::TransformPropertyEditor::GetSettings(world, c, Editor::TransformEditMode::Translation);
        lock->Locked = true;
        auto changed = ct;
        changed.position += glm::vec3(5);
        Check(Editor::TransformPropertyEditor::ApplyCandidate(world, c, changed) && ct.position != changed.position,
            "full position lock");
        const std::array targets{ c };
        const auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
        changed = ct;
        changed.scale *= 2;
        Check(Editor::TransformPropertyEditor::ApplyCandidate(world, c, changed), "unlocked scale");
        history.Record(before, Editor::SceneEditSnapshot::Capture(*scene, targets), "scale");
        Check(history.Undo() && Editor::TransformPropertyEditor::IsLocked(world, c,
            Editor::TransformEditMode::Translation), "undo erased editor lock");
        changed = ct;
        changed.absolute = Flag::None;
        Check(!Editor::TransformPropertyEditor::ApplyCandidate(world, c, changed), "locked flag change");
        lock->Locked = false;
        changed = ct;
        changed.position += glm::vec3(7);
        glm::mat4 desired;
        Check(System::TryComposeWorld(changed, glm::mat4(1), desired) &&
            Editor::TransformPropertyEditor::ApplyWorldMatrix(world, c, desired, Editor::TransformEditMode::Translation),
            "absolute gizmo edit");
    }
    void FailureAndCompatibility()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& root = Add(*scene);
        auto& parent = Add(*scene);
        auto& child = Add(*scene);
        Link(*scene, parent, root);
        Link(*scene, child, parent);
        auto& world = scene->GetRegistry();
        const auto r = root.GetEntityHandle(), p = parent.GetEntityHandle(), c = child.GetEntityHandle();
        world.get<Engine::TransformComponent>(r).scale = { 2, 3, 4 };
        auto& pt = world.get<Engine::TransformComponent>(p);
        Check(pt.SetEulerRotation({ 0, 0, 45 }) && System::UpdateHierarchy(world), "sheared baseline");
        const auto previous = world.get<Engine::TransformComponent>(c);
        const auto oldWorld = world.get<Engine::WorldTransformComponent>(c).worldMatrix;
        Check(!System::TrySetAbsolute(world, c, Flag::Rotation) &&
            world.get<Engine::TransformComponent>(c).absolute == previous.absolute &&
            Near(world.get<Engine::WorldTransformComponent>(c).worldMatrix, oldWorld), "shear toggle mutated source");
        auto invalid = previous;
        invalid.absolute = static_cast<Flag>(128);
        Check(!System::TryCommitTransform(world, c, invalid), "unknown flags accepted");
        invalid = previous;
        invalid.absolute = Flag::All;
        invalid.position += glm::vec3(1);
        Check(!System::TryCommitTransform(world, c, invalid), "mixed flag/value edit was silently overwritten");
        archive::JsonArchive ar;
        Check(bool(Engine::ReflectionRegistry::Get().WriteComponent<Engine::TransformComponent>(
            ar, "Transform", world, c)), "component write");
        std::string json;
        Check(bool(ar.ToJson(json)), "json output");
        const auto flagValue = json.find(':', json.find("\"absolute\"")) + 1;
        auto badJson = json;
        const auto valueEnd = badJson.find_first_of(",\n}", flagValue);
        badJson.replace(flagValue, valueEnd - flagValue, " 2");
        archive::JsonArchive unsupported;
        Check(bool(unsupported.Parse(badJson)) &&
            !Engine::ReflectionRegistry::Get().ReadComponent<Engine::TransformComponent>(
                unsupported, "Transform", world, c) &&
            world.get<Engine::TransformComponent>(c).absolute == Flag::None &&
            Near(world.get<Engine::WorldTransformComponent>(c).worldMatrix, oldWorld), "read failure preservation");
        const auto key = json.find("\"absolute\"");
        Check(key != std::string::npos, "absolute field missing");
        const auto lineStart = json.rfind('\n', key) + 1;
        const auto lineEnd = json.find('\n', key);
        json.erase(lineStart, lineEnd - lineStart + 1);
        // absolute is the last field; remove the preceding trailing comma.
        const auto comma = json.rfind(',', lineStart);
        if (comma != std::string::npos) json.erase(comma, 1);
        archive::JsonArchive legacy;
        Check(bool(legacy.Parse(json)), "legacy JSON fixture");
        auto& fresh = Add(*scene);
        Check(bool(Engine::ReflectionRegistry::Get().ReadComponent<Engine::TransformComponent>(
            legacy, "Transform", world, fresh.GetEntityHandle())) &&
            world.get<Engine::TransformComponent>(fresh.GetEntityHandle()).absolute == Flag::None, "legacy default");
    }
    void DescendantFailureAndGizmos()
    {
        ScenePtr scene(Engine::Scene::Create());
        auto& root = Add(*scene);
        auto& parent = Add(*scene);
        auto& child = Add(*scene);
        Link(*scene, parent, root);
        Link(*scene, child, parent);
        auto& world = scene->GetRegistry();
        const auto r = root.GetEntityHandle(), p = parent.GetEntityHandle(), c = child.GetEntityHandle();
        world.get<Engine::TransformComponent>(r).scale = { 2, 3, 4 };
        auto& pt = world.get<Engine::TransformComponent>(p);
        auto& ct = world.get<Engine::TransformComponent>(c);
        Check(System::UpdateHierarchy(world) && System::TrySetAbsolute(world, c, Flag::Rotation), "partial setup");
        const auto before = pt;
        const auto cached = world.get<Engine::WorldTransformComponent>(c).worldMatrix;
        auto changed = pt;
        Check(changed.SetEulerRotation({ 0, 0, 45 }) && !System::TryCommitTransform(world, p, changed) &&
            pt.rotation == before.rotation && Near(cached, world.get<Engine::WorldTransformComponent>(c).worldMatrix),
            "ancestor edit broke partial descendant");
        auto* positionLock = Editor::TransformPropertyEditor::GetSettings(world, c,
            Editor::TransformEditMode::Translation);
        positionLock->Locked = true;
        changed = pt;
        changed.position.x += 10;
        Check(System::TryCommitTransform(world, p, changed) &&
            !Near(cached, world.get<Engine::WorldTransformComponent>(c).worldMatrix), "editor lock blocked inheritance");
        positionLock->Locked = false;
        world.get<Engine::TransformComponent>(r).scale = glm::vec3(1);
        pt.scale = glm::vec3(2);
        Check(pt.SetEulerRotation({ 0, 0, 25 }), "gizmo parent rotation");
        const auto base = ct;
        for (unsigned bits = 0; bits != 8; ++bits)
            for (auto mode : { Editor::TransformEditMode::Translation, Editor::TransformEditMode::Rotation,
                Editor::TransformEditMode::Scale })
            {
                ct = base;
                ct.absolute = static_cast<Flag>(bits);
                Check(System::UpdateHierarchy(world), "gizmo setup");
                auto candidate = ct;
                if (mode == Editor::TransformEditMode::Translation) candidate.position += glm::vec3(2, 3, 4);
                if (mode == Editor::TransformEditMode::Rotation)
                    Check(candidate.SetEulerRotation({ 10, 20, 30 }), "gizmo candidate rotation");
                if (mode == Editor::TransformEditMode::Scale) candidate.scale *= 1.5f;
                glm::mat4 parentWorld, desired;
                Check(System::TryGetWorldMatrix(world, p, parentWorld) &&
                    System::TryComposeWorld(candidate, parentWorld, desired) &&
                    Editor::TransformPropertyEditor::ApplyWorldMatrix(world, c, desired, mode) &&
                    Near(desired, world.get<Engine::WorldTransformComponent>(c).worldMatrix), "flag gizmo conversion");
            }
        ct = base;
        Check(System::UpdateHierarchy(world), "restore local mode");
        for (auto mode : { Editor::TransformEditMode::Rotation, Editor::TransformEditMode::Scale })
        {
            Editor::TransformPropertyEditor::GetSettings(world, c, mode)->Locked = true;
            auto candidate = ct;
            candidate.scale *= 2;
            Check(candidate.SetEulerRotation({ 50, 60, 70 }), "locked candidate");
            Check(Editor::TransformPropertyEditor::ApplyCandidate(world, c, candidate), "locked edit acceptance");
            if (mode == Editor::TransformEditMode::Rotation) Check(ct.rotation == base.rotation, "rotation lock");
            else Check(ct.scale == candidate.scale / 2.0f, "scale lock");
            Editor::TransformPropertyEditor::GetSettings(world, c, mode)->Locked = false;
            ct = base;
        }
    }
}
int main()
{
    try
    {
        Check(Engine::ReflectionRegistry::Create() != nullptr, "reflection init");
        Combinations();
        HierarchyAndHistory();
        FailureAndCompatibility();
        DescendantFailureAndGizmos();
        Engine::ReflectionRegistry::Destroy();
        std::cout << "Transform inheritance and editor lock checks passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        Engine::ReflectionRegistry::Destroy();
        return 1;
    }
}
