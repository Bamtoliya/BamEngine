#include "UILayout.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "Reflection/ReflectionRegistry.h"
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
    world.get<HierarchyComponent>(child).children.push_back(root);
    if (UILayout::Build(world, root, layouts)) return Fail("Cyclic canvas hierarchy was accepted.");
    std::cout << "UI layout and scene archive checks passed.\n";
    return 0;
}
