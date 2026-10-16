#include "EntityHierarchyView.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "LocalizationManager.h"
#include "Functions.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace
{
    constexpr const char* EntityPayload = "BAM_HIERARCHY_ENTITY";
    struct EntityDragPayload
    {
        Engine::Scene* Scene = nullptr;
        entt::entity Entity = entt::null;
        std::uint64_t PersistentId = 0;
    };

    std::string Text(const char* key, const char* fallback)
    {
        auto text = Engine::LocalizationManager::Get().GetText(std::string(key));
        return text.empty() || text == key ? fallback : text;
    }

    std::string EntityName(Engine::Scene& scene, entt::entity entity)
    {
        const auto& world = scene.GetRegistry();
        const auto* name = world.try_get<Engine::NameComponent>(entity);
        if (name && !name->name.empty()) return Engine::WStrToStr(name->name);
        const auto* id = world.try_get<Engine::IDComponent>(entity);
        return "Entity " + std::to_string(id ? id->id : entt::to_integral(entity));
    }
}

Editor::EntityHierarchyTree Editor::BuildEntityHierarchyTree(Engine::Scene& scene)
{
    EntityHierarchyTree tree;
    const auto& world = scene.GetRegistry();
    std::vector<entt::entity> entities;
    if (const auto* storage = world.storage<entt::entity>())
        for (auto [entity] : storage->each())
            if (scene.FindEntity(entity)) entities.push_back(entity);
    const auto identity = [&](entt::entity entity)
    {
        const auto* id = world.try_get<Engine::IDComponent>(entity);
        return id ? id->id : static_cast<std::uint64_t>(entt::to_integral(entity));
    };
    std::sort(entities.begin(), entities.end(), [&](auto left, auto right)
    {
        if (identity(left) != identity(right)) return identity(left) < identity(right);
        return entt::to_integral(left) < entt::to_integral(right);
    });
    for (const auto entity : entities) tree.Parents.emplace(entity, entt::null);
    for (const auto entity : entities)
    {
        const auto* relation = world.try_get<Engine::HierarchyComponent>(entity);
        if (!relation) continue;
        if (relation->parent != entt::null)
        {
            const auto parent = relation->parent;
            const auto* link = tree.Parents.contains(parent) ?
                world.try_get<Engine::HierarchyComponent>(parent) : nullptr;
            if (parent != entity && link && std::count(link->children.begin(), link->children.end(), entity) == 1)
                tree.Parents[entity] = parent;
            else tree.Invalid.insert(entity);
        }
        std::unordered_set<entt::entity> unique;
        for (const auto child : relation->children)
        {
            const auto* link = tree.Parents.contains(child) ?
                world.try_get<Engine::HierarchyComponent>(child) : nullptr;
            if (!link || link->parent != entity || !unique.insert(child).second) tree.Invalid.insert(entity);
        }
    }
    // Break invalid edges in the view only. All entities remain visible; component data is never repaired here.
    for (const auto entity : entities)
    {
        std::unordered_set<entt::entity> chain;
        for (auto current = entity; current != entt::null; current = tree.Parents.at(current))
        {
            if (!chain.insert(current).second || chain.size() > 256)
            {
                tree.Parents[entity] = entt::null;
                tree.Invalid.insert(entity);
                break;
            }
        }
    }
    for (const auto entity : entities)
    {
        if (tree.Parents[entity] == entt::null) tree.Roots.push_back(entity);
        auto& children = tree.Children[entity];
        if (const auto* link = world.try_get<Engine::HierarchyComponent>(entity))
            for (const auto child : link->children)
                if (tree.Parents.contains(child) && tree.Parents[child] == entity) children.push_back(child);
    }
    return tree;
}

Editor::EntityHierarchyView::~EntityHierarchyView() { ResetDrag(); }

void Editor::EntityHierarchyView::ResetDrag()
{
    if (m_DragSource) m_DragSource->Release();
    m_DragSource = nullptr;
}

void Editor::EntityHierarchyView::ReleaseFinishedDrag()
{
    const auto* payload = ImGui::GetDragDropPayload();
    if (!payload || !payload->IsDataType(EntityPayload)) ResetDrag();
}

Editor::EntityHierarchyAction Editor::EntityHierarchyView::Draw(Engine::Scene& scene,
    std::span<const entt::entity> selected, entt::entity reveal, bool locked)
{
    ReleaseFinishedDrag();
    const auto tree = BuildEntityHierarchyTree(scene);
    const auto& world = scene.GetRegistry();
    EntityHierarchyAction action;
    std::unordered_set<entt::entity> ancestors;
    if (tree.Parents.contains(reveal))
        for (auto parent = tree.Parents.at(reveal); parent != entt::null; parent = tree.Parents.at(parent))
            ancestors.insert(parent);
    ImGui::PushID(&scene);
    const auto acceptMove = [&](entt::entity parent)
    {
        const auto* payload = ImGui::AcceptDragDropPayload(EntityPayload);
        if (!payload) return;
        EntityDragPayload source;
        if (payload->DataSize != static_cast<int>(sizeof(source))) { action.RejectedDrop = true; return; }
        std::memcpy(&source, payload->Data, sizeof(source));
        if (source.Scene != &scene || !world.valid(source.Entity)) { action.RejectedDrop = true; return; }
        const auto* id = world.try_get<Engine::IDComponent>(source.Entity);
        if (!scene.FindEntity(source.Entity) || !id || id->id != source.PersistentId)
            action.RejectedDrop = true;
        else action.Move = EntityHierarchyMove{ source.Entity, parent };
    };
    const auto draw = [&](auto&& self, entt::entity entity) -> void
    {
        const auto& children = tree.Children.at(entity);
        auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
            ImGuiTreeNodeFlags_SpanAvailWidth;
        if (std::ranges::find(selected, entity) != selected.end()) flags |= ImGuiTreeNodeFlags_Selected;
        if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        ImGui::PushID(std::to_string(entt::to_integral(entity)).c_str());
        if (ancestors.contains(entity)) ImGui::SetNextItemOpen(true);
        const auto label = EntityName(scene, entity);
        const bool opened = ImGui::TreeNodeEx("EntityNode", flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && !locked)
        {
            action.Clicked = entity;
            action.Toggle = ImGui::GetIO().KeyCtrl;
        }
        if (entity == reveal) ImGui::SetScrollHereY();
        if (tree.Invalid.contains(entity) && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s",
                Text("UI_HIERARCHY_INVALID", "Invalid hierarchy links; shown without repair.").c_str());
        const auto* id = world.try_get<Engine::IDComponent>(entity);
        if (!locked && id && id->id != 0 && ImGui::BeginDragDropSource())
        {
            if (m_DragSource != &scene)
            {
                ResetDrag();
                scene.AddRef();
                m_DragSource = &scene;
            }
            const EntityDragPayload payload{ &scene, entity, id->id };
            ImGui::SetDragDropPayload(EntityPayload, &payload, sizeof(payload));
            ImGui::TextUnformatted(label.c_str());
            ImGui::EndDragDropSource();
        }
        if (!locked && ImGui::BeginDragDropTarget())
        {
            acceptMove(entity);
            ImGui::EndDragDropTarget();
        }
        if (!locked && ImGui::BeginPopupContextItem("EntityActions"))
        {
            if (ImGui::MenuItem(Text("UI_DELETE_ENTITY", "Delete entity and children").c_str()))
                action.Delete = entity;
            ImGui::EndPopup();
        }
        if (opened && !children.empty())
        {
            for (const auto child : children) self(self, child);
            ImGui::TreePop();
        }
        ImGui::PopID();
    };
    if (!tree.Invalid.empty())
        ImGui::TextDisabled("%s",
            Text("UI_HIERARCHY_INVALID", "Invalid hierarchy links; shown without repair.").c_str());
    for (const auto root : tree.Roots) draw(draw, root);
    if (tree.Roots.empty()) ImGui::TextDisabled("%s", Text("UI_HIERARCHY_EMPTY", "No entities.").c_str());
    const auto* payload = ImGui::GetDragDropPayload();
    if (!locked && payload && payload->IsDataType(EntityPayload))
    {
        ImGui::Button(Text("UI_HIERARCHY_ROOT_DROP", "Drop here to move to root").c_str(), ImVec2(-1.0f, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            acceptMove(entt::null);
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::PopID();
    return action;
}
