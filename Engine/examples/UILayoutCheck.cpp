#include "UILayout.h"
#include "UIClipLayout.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <iostream>
#include <limits>
#include <memory>

namespace
{
    bool Near(const glm::vec2& left, const glm::vec2& right) { return glm::length(left - right) < 0.001f; }
    int Fail(const char* message) { std::cerr << message << '\n'; return 1; }
    struct SceneRelease { void operator()(Engine::Scene* scene) const { if (scene) scene->Release(); } };
}

int main()
{
    using namespace Engine;
    RectTransformComponent rect;
    UIElementLayout layout;
    if (!UILayout::Resolve(rect, { 1000.0f, 500.0f }, glm::vec2(0.0f), glm::mat4(1.0f), layout) ||
        !Near(glm::vec2(layout.worldTransform[3]), { 500.0f, 250.0f }) ||
        !Near(glm::vec2(layout.renderMatrix * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f)), { 450.0f, 200.0f }) ||
        !UILayout::Contains(layout, { 500.0f, 250.0f }) || UILayout::Contains(layout, { 700.0f, 250.0f }))
        return Fail("Fixed anchors, pivot, or transformed hit testing failed.");
    rect.anchorMin = { 0.0f, 0.0f };
    rect.anchorMax = { 1.0f, 1.0f };
    rect.sizeDelta = { -20.0f, -40.0f };
    if (!UILayout::Resolve(rect, { 1000.0f, 500.0f }, glm::vec2(0.0f), glm::mat4(1.0f), layout) ||
        !Near(layout.size, { 980.0f, 460.0f })) return Fail("Stretched anchors did not preserve margins.");
    const glm::mat4 parent = glm::translate(glm::mat4(1.0f), { 100.0f, 50.0f, 0.0f }) *
        glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), { 0.0f, 0.0f, 1.0f }) *
        glm::scale(glm::mat4(1.0f), { 2.0f, 3.0f, 1.0f });
    rect = {};
    rect.anchoredPosition = { 10.0f, 20.0f };
    if (!UILayout::Resolve(rect, { 200.0f, 100.0f }, glm::vec2(0.5f), parent, layout) ||
        !Near(glm::vec2(layout.worldTransform[3]), { 40.0f, 70.0f }) || !UILayout::Contains(layout, { 40.0f, 70.0f }))
        return Fail("Parent rotation, scale or parent-pivot mapping failed.");
    rect.scale.x = 0.0f;
    if (!UILayout::Resolve(rect, { 200.0f, 100.0f }, glm::vec2(0.5f), parent, layout) ||
        UILayout::Contains(layout, { 40.0f, 70.0f })) return Fail("Singular geometry should not be selectable.");
    rect = {};
    rect.sizeDelta = { -1.0f, 10.0f };
    if (UILayout::Resolve(rect, { 100.0f, 100.0f }, glm::vec2(0.0f), glm::mat4(1.0f), layout))
        return Fail("Negative resolved size was accepted.");
    rect = {};
    rect.anchorMin.x = 0.9f;
    rect.anchorMax.x = 0.1f;
    if (UILayout::IsValid(rect)) return Fail("Reversed anchors were accepted.");
    CanvasComponent invalid;
    invalid.referenceResolution.x = std::numeric_limits<float>::quiet_NaN();
    if (UILayout::IsValid(invalid)) return Fail("Nonfinite canvas resolution was accepted.");

    if (!ReflectionRegistry::Create()) return Fail("Reflection initialization failed.");
    struct RegistryRelease { ~RegistryRelease() { ReflectionRegistry::Destroy(); } } registryRelease;
    {
        std::unique_ptr<Scene, SceneRelease> source(Scene::Create());
        auto& canvas = source->CreateEntity();
        const auto handle = canvas.GetEntityHandle();
        source->GetRegistry().emplace<CanvasComponent>(handle);
        source->GetRegistry().emplace<HierarchyComponent>(handle);
        archive::JsonArchive prefab;
        if (!source->WritePrefabArchive(prefab, canvas))
            return Fail("Empty Canvas prefab could not be saved.");

        std::unique_ptr<Scene, SceneRelease> destination(Scene::Create());
        Entity* loadedCanvas = nullptr;
        std::vector<UIElementLayout> emptyLayouts;
        if (!destination->InstantiatePrefabArchive(prefab, loadedCanvas, nullptr) || !loadedCanvas ||
            !UILayout::Build(destination->GetRegistry(), loadedCanvas->GetEntityHandle(), emptyLayouts) || !emptyLayouts.empty())
            return Fail("Empty Canvas prefab did not restore as a valid layout root.");
    }
    {
        std::unique_ptr<Scene, SceneRelease> scene(Scene::Create());
        auto& world = scene->GetRegistry();
        const auto canvas = scene->CreateEntity().GetEntityHandle();
        world.emplace<CanvasComponent>(canvas);
        world.emplace<HierarchyComponent>(canvas);
        const auto add = [&](entt::entity parent, glm::vec2 origin, glm::vec2 size)
        {
            const auto handle = scene->CreateEntity().GetEntityHandle();
            world.emplace<HierarchyComponent>(handle).parent = parent;
            world.get<HierarchyComponent>(parent).children.push_back(handle);
            auto& rect = world.emplace<RectTransformComponent>(handle);
            rect.pivot = rect.anchorMin = rect.anchorMax = glm::vec2(0.0f);
            rect.anchoredPosition = origin;
            rect.sizeDelta = size;
            return handle;
        };
        const auto viewport = add(canvas, { 0.0f, 0.0f }, { 100.0f, 100.0f });
        const auto content = add(viewport, { 0.0f, 0.0f }, { 100.0f, 200.0f });
        const auto mask = add(content, { 20.0f, 20.0f }, { 60.0f, 150.0f });
        const auto item = add(mask, { 0.0f, 0.0f }, { 100.0f, 150.0f });
        world.emplace<UIScrollComponent>(viewport);
        world.emplace<UIClipComponent>(mask);

        std::vector<UIElementLayout> layouts;
        if (!UILayout::Build(world, canvas, layouts) || layouts.size() != 4 || layouts[3].entity != item ||
            !UILayout::Contains(layouts[3], { 25.0f, 99.0f }) || UILayout::Contains(layouts[3], { 25.0f, 101.0f }) ||
            UILayout::Contains(layouts[3], { 90.0f, 25.0f }))
            return Fail("Nested clipping did not limit hit testing to both ancestors.");
        if (!UIClipLayout::ScrollAt(world, layouts, { 25.0f, 25.0f }, -1.0f) ||
            !UILayout::Build(world, canvas, layouts) ||
            !Near(glm::vec2(layouts[1].renderMatrix[3]), { 0.0f, -40.0f }) ||
            !Near(world.get<RectTransformComponent>(content).anchoredPosition, glm::vec2(0.0f)))
            return Fail("Wheel scrolling did not move computed geometry independently of authored values.");
    }
    {
        std::unique_ptr<Scene, SceneRelease> editing(Scene::Create());
        auto& registry = editing->GetRegistry();
        auto& canvas = editing->CreateEntity();
        const auto canvasId = canvas.GetEntityHandle();
        registry.emplace<CanvasComponent>(canvasId);
        registry.emplace<FlagComponent>(canvasId);
        registry.emplace<HierarchyComponent>(canvasId);
        const auto addRect = [&](Entity& parent) -> Entity&
        {
            auto& child = editing->CreateEntity();
            const auto id = child.GetEntityHandle();
            registry.emplace<RectTransformComponent>(id);
            registry.emplace<FlagComponent>(id);
            auto& relation = registry.emplace<HierarchyComponent>(id);
            relation.parent = parent.GetEntityHandle();
            registry.get<HierarchyComponent>(relation.parent).children.push_back(id);
            return child;
        };

        auto& source = addRect(canvas);
        auto& destination = addRect(canvas);
        auto& child = addRect(source);
        const auto childId = child.GetEntityHandle();
        const auto destinationId = destination.GetEntityHandle();
        auto& destinationRect = registry.get<RectTransformComponent>(destinationId);
        destinationRect.rotation = 30.0f;
        destinationRect.scale = { 1.5f, 1.5f };

        std::vector<UIElementLayout> before;
        std::vector<UIElementLayout> after;
        if (!UILayout::Build(registry, canvasId, before) ||
            editing->ReparentEntity(child, &destination) != EntityReparentResult::Success ||
            registry.get<HierarchyComponent>(childId).parent != destinationId ||
            !UILayout::Build(registry, canvasId, after))
            return Fail("A UI child could not move between free-layout parents.");
        const auto sameQuad = [&]()
        {
            const auto old = std::ranges::find(before, childId, &UIElementLayout::entity);
            const auto current = std::ranges::find(after, childId, &UIElementLayout::entity);
            if (old == before.end() || current == after.end()) return false;
            for (int axis : { 0, 1, 3 })
                if (!Near(glm::vec2(old->renderMatrix[axis]), glm::vec2(current->renderMatrix[axis]))) return false;
            return true;
        };
        if (!sameQuad() || editing->ReparentEntity(child, &canvas) != EntityReparentResult::Success ||
            registry.get<HierarchyComponent>(childId).parent != canvasId ||
            !UILayout::Build(registry, canvasId, after) || !sameQuad())
            return Fail("Reparenting a UI child failed to preserve its canvas geometry.");

        const auto authoredRect = registry.get<RectTransformComponent>(childId);
        registry.emplace<UILayoutGroupComponent>(destinationId).mode = EUILayoutGroupMode::Grid;
        if (editing->ReparentEntity(child, &destination) != EntityReparentResult::Success ||
            !UILayout::Build(registry, canvasId, after))
            return Fail("A UI child could not join a layout group.");
        const auto managed = std::ranges::find(after, childId, &UIElementLayout::entity);
        if (managed == after.end() || !managed->managedByGroup ||
            !Near(registry.get<RectTransformComponent>(childId).anchoredPosition, authoredRect.anchoredPosition))
            return Fail("Layout group reparenting must preserve authored RectTransform values.");
        if (editing->ReparentEntity(child, nullptr) != EntityReparentResult::InvalidTransform ||
            registry.get<HierarchyComponent>(childId).parent != destinationId)
            return Fail("UI detachment outside a canvas should fail without changing the hierarchy.");
    }
    std::unique_ptr<Scene, SceneRelease> scene(Scene::Create());
    auto& rootWrapper = scene->CreateEntity();
    auto& childWrapper = scene->CreateEntity();
    auto& otherWrapper = scene->CreateEntity();
    const auto root = rootWrapper.GetEntityHandle();
    const auto child = childWrapper.GetEntityHandle();
    const auto other = otherWrapper.GetEntityHandle();
    auto& world = scene->GetRegistry();
    world.emplace<CanvasComponent>(root);
    world.emplace<FlagComponent>(root);
    world.emplace<HierarchyComponent>(root).children = { child, other };
    for (auto entity : { child, other })
    {
        world.emplace<RectTransformComponent>(entity);
        world.emplace<FlagComponent>(entity);
        world.emplace<HierarchyComponent>(entity).parent = root;
        world.emplace<UIImageComponent>(entity);
    }
    std::vector<UIElementLayout> layouts;
    if (!UILayout::Build(world, root, layouts) || layouts.size() != 2 || layouts[0].entity != child ||
        layouts[1].entity != other || UILayout::FindCanvas(world, child) != root)
        return Fail("Canvas traversal/sibling draw order failed.");
    const auto originalChildren = world.get<HierarchyComponent>(root).children;
    world.emplace<UIRenderOrderComponent>(child).zIndex = 10;
    if (!UILayout::Build(world, root, layouts) || layouts[0].entity != other || layouts[1].entity != child ||
        world.get<HierarchyComponent>(root).children != originalChildren)
        return Fail("Z-index did not reorder drawing independently of the authored hierarchy.");
    auto& group = world.emplace<UILayoutGroupComponent>(root);
    group.padding = { 10.0f, 20.0f, 10.0f, 20.0f };
    group.spacing = { 6.0f, 4.0f };
    const auto at = [](const UIElementLayout& item, const glm::vec2& origin)
    {
        return item.managedByGroup && Near(glm::vec2(item.renderMatrix[3]), origin);
    };
    const auto authoredRect = world.get<RectTransformComponent>(child);
    if (!UILayout::Build(world, root, layouts) || layouts.size() != 2 || !at(layouts[0], { 116.0f, 20.0f }) ||
        !at(layouts[1], { 10.0f, 20.0f }))
        return Fail("Row placement did not use authored order independently from Z-index.");
    group.mode = EUILayoutGroupMode::Column;
    if (!UILayout::Build(world, root, layouts) || layouts.size() != 2 || !at(layouts[0], { 10.0f, 124.0f }) ||
        !at(layouts[1], { 10.0f, 20.0f }))
        return Fail("Column placement did not apply padding and spacing.");
    group.mode = EUILayoutGroupMode::Grid;
    group.columns = 1;
    group.cellSize = { 48.0f, 36.0f };
    if (!UILayout::Build(world, root, layouts) || layouts.size() != 2 || !at(layouts[0], { 10.0f, 60.0f }) ||
        !at(layouts[1], { 10.0f, 20.0f }) || !Near(layouts[1].size, group.cellSize) ||
        !Near(world.get<RectTransformComponent>(child).sizeDelta, authoredRect.sizeDelta) ||
        !Near(world.get<RectTransformComponent>(child).anchoredPosition, authoredRect.anchoredPosition))
        return Fail("Grid placement did not compute fixed cells independently of authored RectTransform.");
    const auto grandchild = scene->CreateEntity().GetEntityHandle();
    world.emplace<RectTransformComponent>(grandchild);
    world.emplace<FlagComponent>(grandchild);
    world.emplace<HierarchyComponent>(grandchild).parent = child;
    world.get<HierarchyComponent>(child).children.push_back(grandchild);
    world.emplace<UIImageComponent>(grandchild);
    world.emplace<UIRenderOrderComponent>(grandchild).zIndex = (std::numeric_limits<std::int32_t>::max)();
    world.emplace<UIRenderOrderComponent>(other).zIndex = 20;
    if (!UILayout::Build(world, root, layouts) || layouts.size() != 3 || layouts[0].entity != child ||
        layouts[1].entity != grandchild || layouts[2].entity != other)
        return Fail("A descendant escaped its parent's stacking group.");
    world.get<UIRenderOrderComponent>(other).zIndex = 10;
    if (!UILayout::Build(world, root, layouts) || layouts[0].entity != child || layouts[1].entity != grandchild ||
        layouts[2].entity != other) return Fail("Equal Z-index lost stable sibling order.");
    world.get<UIRenderOrderComponent>(child).zIndex = (std::numeric_limits<std::int32_t>::max)();
    world.get<UIRenderOrderComponent>(other).zIndex = (std::numeric_limits<std::int32_t>::min)();
    if (!UILayout::Build(world, root, layouts) || layouts[0].entity != other || layouts[1].entity != child ||
        layouts[2].entity != grandchild) return Fail("Extreme Z-index values broke ordering.");
    world.emplace<UIRenderOrderComponent>(root).zIndex = -3;
    world.get<FlagComponent>(root).flags = EEntityFlag::None;
    if (!UILayout::Build(world, root, layouts) ||
        std::ranges::any_of(layouts, [](const auto& item) { return item.visible; }))
        return Fail("Canvas visibility did not suppress its descendants.");
    world.get<FlagComponent>(root).flags = EEntityFlag::Default;
    archive::JsonArchive document;
    if (!scene->WriteArchive(document)) return Fail("UI-only hierarchy serialization failed.");
    Scene* output = nullptr;
    if (!Scene::CreateFromArchive(document, output)) return Fail("UI-only hierarchy restoration failed.");
    std::unique_ptr<Scene, SceneRelease> restored(output);
    auto* restoredRoot = restored->FindEntityById(world.get<IDComponent>(root).id);
    if (!restoredRoot || !UILayout::Build(restored->GetRegistry(), restoredRoot->GetEntityHandle(), layouts) ||
        layouts.size() != 3) return Fail("Restored canvas layout failed.");
    const auto& restoredWorld = restored->GetRegistry();
    if (UILayout::GetZIndex(restoredWorld, restoredRoot->GetEntityHandle()) != -3 ||
        restoredWorld.get<IDComponent>(layouts[0].entity).id != world.get<IDComponent>(other).id ||
        restoredWorld.get<IDComponent>(layouts[1].entity).id != world.get<IDComponent>(child).id ||
        restoredWorld.get<IDComponent>(layouts[2].entity).id != world.get<IDComponent>(grandchild).id)
        return Fail("Z-index or ordered subtree restoration failed.");
    const auto* restoredGroup = restoredWorld.try_get<UILayoutGroupComponent>(restoredRoot->GetEntityHandle());
    if (!restoredGroup || restoredGroup->mode != EUILayoutGroupMode::Grid || restoredGroup->columns != 1 ||
        !Near(restoredGroup->cellSize, group.cellSize) || !at(layouts[0], { 10.0f, 60.0f }))
        return Fail("Grid layout was not preserved by scene restoration.");
    world.get<HierarchyComponent>(child).children.push_back(root);
    if (UILayout::Build(world, root, layouts)) return Fail("Cyclic canvas hierarchy was accepted.");
    std::cout << "UI layout and scene archive checks passed.\n";
    return 0;
}
