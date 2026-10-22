#include "Scene.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "TransformPropertyEditor.h"
#include "SceneEditHistory.h"
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include <stdexcept>
#include <iostream>
#include <array>

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
    void WorldQueryCache()
    {
        entt::registry world;
        const auto parent = world.create();
        const auto child = world.create();
        world.emplace<Engine::TransformComponent>(parent);
        world.emplace<Engine::TransformComponent>(child);
        world.emplace<Engine::WorldTransformComponent>(parent);
        world.emplace<Engine::WorldTransformComponent>(child);
        world.emplace<Engine::HierarchyComponent>(child).parent = parent;

        auto& parentTransform = world.get<Engine::TransformComponent>(parent);
        auto& childTransform = world.get<Engine::TransformComponent>(child);
        parentTransform.position = { 10, 0, 0 };
        childTransform.position = { 1, 0, 0 };
        Check(System::UpdateHierarchy(world), "query cache setup");

        const auto parentBefore = world.get<Engine::WorldTransformComponent>(parent).worldMatrix;
        const auto childBefore = world.get<Engine::WorldTransformComponent>(child).worldMatrix;
        const auto rotationBefore = parentTransform.rotation;

        Check(System::UpdateHierarchy(world), "unchanged hierarchy update");

        world.emplace<Engine::HierarchyComponent>(parent).parent = child;
        Check(!System::UpdateHierarchy(world), "unchanged matrices hid hierarchy cycle");
        world.remove<Engine::HierarchyComponent>(parent);
        Check(System::UpdateHierarchy(world), "restored hierarchy update");

        world.get<Engine::WorldTransformComponent>(child).worldMatrix = glm::mat4(0.0f);
        Check(System::UpdateHierarchy(world) &&
            Near(world.get<Engine::WorldTransformComponent>(child).worldMatrix, childBefore),
            "unchanged update did not restore published world matrix");

        const auto verify = [&]()
            {
                glm::mat4 parentWorld, expected, actual;
                Check(System::TryComposeWorld(parentTransform, glm::mat4(1), parentWorld), "query parent reference");
                Check(System::TryComposeWorld(childTransform, parentWorld, expected), "query child reference");
                Check(System::TryGetWorldMatrix(world, child, actual) && Near(actual, expected), "stale world query");
            };

        verify();
        parentTransform.position.x += 5;
        verify();
        parentTransform.eulerRotation.z = 90;
        verify();
        childTransform.eulerRotation.z = 30;
        verify();
        childTransform.absolute = Flag::All;
        verify();

        Check(parentTransform.rotation == rotationBefore, "query modified source rotation");
        Check(Near(world.get<Engine::WorldTransformComponent>(parent).worldMatrix, parentBefore) &&
            Near(world.get<Engine::WorldTransformComponent>(child).worldMatrix, childBefore),
            "query modified published world matrices");

        world.emplace<Engine::HierarchyComponent>(parent).parent = child;
        Check(!System::UpdateHierarchy(world), "update accepted cycle");
        Check(parentTransform.rotation == rotationBefore &&
            Near(world.get<Engine::WorldTransformComponent>(parent).worldMatrix, parentBefore) &&
            Near(world.get<Engine::WorldTransformComponent>(child).worldMatrix, childBefore),
            "failed update committed pending transform");
        glm::mat4 output(7.0f);
        Check(!System::TryGetWorldMatrix(world, child, output) && Near(output, glm::mat4(7.0f)),
            "cached query accepted cycle or modified failure output");

        world.get<Engine::HierarchyComponent>(parent).parent = entt::null;
        Check(System::UpdateHierarchy(world), "update after rejected hierarchy");
        const auto expectedRotation = glm::quat(glm::radians(glm::vec3(0, 0, 90)));
        Check(std::abs(glm::dot(parentTransform.rotation, expectedRotation)) > 0.9999f,
            "successful update did not commit pending rotation");
        verify();
        world.remove<Engine::WorldTransformComponent>(parent);
        world.remove<Engine::WorldTransformComponent>(child);
        verify();
    }

    void BatchVectorEditing()
    {
        using Edit = Editor::TransformPropertyEditor;
        using Mode = Editor::TransformEditMode;
        using Result = Editor::TransformBatchResult;

        ScenePtr scene(Engine::Scene::Create());
        Check(bool(scene), "batch scene");
        auto& first = Add(*scene);
        auto& second = Add(*scene);
        auto& child = Add(*scene);
        Link(*scene, child, first);

        auto& world = scene->GetRegistry();
        const auto a = first.GetEntityHandle(), b = second.GetEntityHandle(), c = child.GetEntityHandle();
        const std::array targets{ a, b };
        auto& ta = world.get<Engine::TransformComponent>(a);
        auto& tb = world.get<Engine::TransformComponent>(b);

        ta.position = { 1, 2, 3 };
        tb.position = { 10, 20, 30 };
        ta.scale = { 1, 2, 3 };
        tb.scale = { 4, 5, 6 };
        Check(ta.SetEulerRotation({ 10, 20, 30 }) && tb.SetEulerRotation({ -5, 40, 70 }), "batch rotations");
        Check(System::UpdateHierarchy(world), "batch initial update");
        const auto originalA = ta, originalB = tb;

        Check(Edit::SetVectorAxis(world, targets, Mode::Translation, 0, 7) == Result::Changed, "batch position");
        Check(ta.position == glm::vec3(7, 2, 3) && tb.position == glm::vec3(7, 20, 30), "position axes");

        Check(Edit::SetVectorAxis(world, targets, Mode::Scale, 0, -2) == Result::Changed, "negative batch scale");
        Check(ta.scale == glm::vec3(-2, 2, 3) && tb.scale == glm::vec3(-2, 5, 6), "scale axes");
        Check(ta.position == glm::vec3(7, 2, 3) && tb.position == glm::vec3(7, 20, 30), "scale kept position");
        Check(ta.rotation == originalA.rotation && tb.rotation == originalB.rotation &&
            ta.eulerRotation == originalA.eulerRotation && tb.eulerRotation == originalB.eulerRotation,
            "scale kept rotation");
        Check(Edit::SetVectorAxis(world, targets, Mode::Scale, 0, -2) == Result::Unchanged, "unchanged scale");
        Check(Edit::SetVectorAxis(world, targets, Mode::Scale, 0, 0) == Result::Changed, "zero batch scale");

        auto* settings = Edit::GetSettings(world, b, Mode::Scale);
        Check(settings != nullptr, "batch scale settings");
        settings->LockedAxes[1] = true;
        Check(Edit::SetVectorAxis(world, targets, Mode::Scale, 1, 9) == Result::Rejected, "batch axis lock");
        Check(ta.scale == glm::vec3(0, 2, 3) && tb.scale == glm::vec3(0, 5, 6), "lock preserved both");
        settings->LockedAxes[1] = false;

        ta.scale = tb.scale = glm::vec3(1);
        world.get<Engine::TransformComponent>(c).absolute = Flag::Rotation;
        Check(System::UpdateHierarchy(world), "batch failure setup");
        const auto cacheA = world.get<Engine::WorldTransformComponent>(a).worldMatrix;
        const auto cacheB = world.get<Engine::WorldTransformComponent>(b).worldMatrix;
        const auto cacheC = world.get<Engine::WorldTransformComponent>(c).worldMatrix;

        Check(Edit::SetVectorAxis(world, targets, Mode::Scale, 0, 0) == Result::Rejected, "descendant rejection");
        Check(ta.scale == glm::vec3(1) && tb.scale == glm::vec3(1), "batch rollback");
        Check(world.get<Engine::WorldTransformComponent>(a).worldMatrix == cacheA &&
            world.get<Engine::WorldTransformComponent>(b).worldMatrix == cacheB &&
            world.get<Engine::WorldTransformComponent>(c).worldMatrix == cacheC, "rollback kept caches");

        Editor::SceneEditHistory history;
        history.Synchronize(scene.get(), true);
        auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
        Check(bool(before), "batch before snapshot");
        Check(Edit::SetVectorAxis(world, targets, Mode::Scale, 0, 2) == Result::Changed, "batch history edit");
        auto after = Editor::SceneEditSnapshot::Capture(*scene, targets);
        Check(bool(after), "batch after snapshot");
        history.Record(std::move(before), std::move(after), "UI_HISTORY_TRANSFORM");
        Check(history.Undo() && ta.scale == glm::vec3(1) && tb.scale == glm::vec3(1), "batch scale undo");
        Check(history.Redo() && ta.scale == glm::vec3(2, 1, 1) && tb.scale == glm::vec3(2, 1, 1), "batch scale redo");
    }

    void BatchRotationEditing()
    {
        using Edit = Editor::TransformPropertyEditor;
        using Mode = Editor::TransformEditMode;
        using Result = Editor::TransformBatchResult;

        ScenePtr scene(Engine::Scene::Create());
        Check(bool(scene), "rotation scene");
        const auto a = Add(*scene).GetEntityHandle();
        const auto b = Add(*scene).GetEntityHandle();
        const std::array targets{ a, b };
        auto& world = scene->GetRegistry();
        auto& ta = world.get<Engine::TransformComponent>(a);
        auto& tb = world.get<Engine::TransformComponent>(b);

        ta.position = { 1, 2, 3 };
        tb.position = { 10, 20, 30 };
        ta.scale = { 1, 2, 3 };
        tb.scale = { 4, 5, 6 };
        Check(ta.SetEulerRotation({ 10, 20, 350 }) && tb.SetEulerRotation({ -5, 40, -10 }), "rotation setup");
        Check(System::UpdateHierarchy(world), "rotation initial update");

        auto expectedA = ta, expectedB = tb;
        Check(expectedA.SetEulerRotation({ 10, 20, 450 }) &&
            expectedB.SetEulerRotation({ -5, 40, 450 }), "rotation expectations");

        Check(Edit::SetVectorAxis(world, targets, Mode::Rotation, 2, 450) == Result::Changed, "batch rotation");
        Check(ta.eulerRotation == expectedA.eulerRotation && tb.eulerRotation == expectedB.eulerRotation,
            "Euler axes and accumulated angle");
        Check(ta.rotation == expectedA.rotation && tb.rotation == expectedB.rotation, "batch quaternions");
        Check(ta.IsRotationSynchronized() && tb.IsRotationSynchronized(), "batch rotation synchronization");
        Check(ta.position == glm::vec3(1, 2, 3) && tb.position == glm::vec3(10, 20, 30) &&
            ta.scale == glm::vec3(1, 2, 3) && tb.scale == glm::vec3(4, 5, 6), "rotation kept position and scale");
        Check(Edit::SetVectorAxis(world, targets, Mode::Rotation, 2, 450) == Result::Unchanged, "same rotation");

        auto* sa = Edit::GetSettings(world, a, Mode::Rotation);
        auto* sb = Edit::GetSettings(world, b, Mode::Rotation);
        Check(sa && sb, "rotation settings");
        sb->LockedAxes[2] = true;
        Check(Edit::SetVectorAxis(world, targets, Mode::Rotation, 2, 460) == Result::Rejected, "rotation axis lock");
        Check(ta.rotation == expectedA.rotation && tb.rotation == expectedB.rotation, "lock kept quaternions");
        sb->LockedAxes[2] = false;

        sa->SnapEnabled = sb->SnapEnabled = true;
        sa->SnapStep = sb->SnapStep = 15;
        Check(Edit::SetVectorAxis(world, targets, Mode::Rotation, 2, 464) == Result::Changed, "rotation snap");
        Check(ta.eulerRotation.z == 465 && tb.eulerRotation.z == 465, "rotation snapped values");
        sa->SnapEnabled = sb->SnapEnabled = false;

        const auto savedA = ta, savedB = tb;
        Editor::SceneEditHistory history;
        history.Synchronize(scene.get(), true);
        auto before = Editor::SceneEditSnapshot::Capture(*scene, targets);
        Check(bool(before), "rotation before snapshot");
        Check(Edit::SetVectorAxis(world, targets, Mode::Rotation, 2, 720) == Result::Changed, "rotation history edit");
        auto after = Editor::SceneEditSnapshot::Capture(*scene, targets);
        Check(bool(after), "rotation after snapshot");
        history.Record(std::move(before), std::move(after), "UI_HISTORY_TRANSFORM");

        Check(history.Undo() && ta.eulerRotation == savedA.eulerRotation &&
            tb.eulerRotation == savedB.eulerRotation && ta.rotation == savedA.rotation &&
            tb.rotation == savedB.rotation, "batch rotation undo");
        Check(history.Redo() && ta.eulerRotation.z == 720 && tb.eulerRotation.z == 720 &&
            ta.IsRotationSynchronized() && tb.IsRotationSynchronized(), "batch rotation redo");
    }
    void BatchRotationRollback()
    {
        using Edit = Editor::TransformPropertyEditor;
        using Mode = Editor::TransformEditMode;
        using Result = Editor::TransformBatchResult;

        ScenePtr scene(Engine::Scene::Create());
        Check(bool(scene), "rotation rollback scene");
        auto& grandparent = Add(*scene);
        auto& first = Add(*scene);
        auto& second = Add(*scene);
        auto& child = Add(*scene);
        Link(*scene, first, grandparent);
        Link(*scene, child, first);

        auto& world = scene->GetRegistry();
        const auto a = first.GetEntityHandle(), b = second.GetEntityHandle(), c = child.GetEntityHandle();
        const std::array targets{ a, b };
        world.get<Engine::TransformComponent>(grandparent.GetEntityHandle()).scale = { 2, 1, 1 };
        world.get<Engine::TransformComponent>(c).absolute = Flag::Rotation;
        auto& ta = world.get<Engine::TransformComponent>(a);
        auto& tb = world.get<Engine::TransformComponent>(b);
        Check(tb.SetEulerRotation({ 0, 0, 10 }), "rollback rotation setup");
        Check(System::UpdateHierarchy(world), "rollback initial update");

        const auto savedA = ta, savedB = tb;
        const auto cacheA = world.get<Engine::WorldTransformComponent>(a).worldMatrix;
        const auto cacheB = world.get<Engine::WorldTransformComponent>(b).worldMatrix;
        const auto cacheC = world.get<Engine::WorldTransformComponent>(c).worldMatrix;

        Check(Edit::SetVectorAxis(world, targets, Mode::Rotation, 2, 45) == Result::Rejected,
            "rotation descendant rejection");
        Check(ta.eulerRotation == savedA.eulerRotation && tb.eulerRotation == savedB.eulerRotation &&
            ta.rotation == savedA.rotation && tb.rotation == savedB.rotation &&
            ta.IsRotationSynchronized() && tb.IsRotationSynchronized(), "rotation rollback and synchronization");
        Check(world.get<Engine::WorldTransformComponent>(a).worldMatrix == cacheA &&
            world.get<Engine::WorldTransformComponent>(b).worldMatrix == cacheB &&
            world.get<Engine::WorldTransformComponent>(c).worldMatrix == cacheC, "rotation rollback kept caches");
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
        WorldQueryCache();
        BatchVectorEditing();
        BatchRotationEditing();
        BatchRotationRollback();
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
