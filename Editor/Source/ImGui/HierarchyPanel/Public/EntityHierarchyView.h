#pragma once

#include <entt/entity/entity.hpp>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <cstdint>

namespace Engine { class Scene; }

namespace Editor
{
    struct EntityHierarchyTree
    {
        std::vector<entt::entity> Roots;
        std::unordered_map<entt::entity, entt::entity> Parents;
        std::unordered_map<entt::entity, std::vector<entt::entity>> Children;
        std::unordered_set<entt::entity> Invalid;
    };

    struct EntityHierarchyMove
    {
        entt::entity Child = entt::null;
        entt::entity Parent = entt::null;
    };

    struct EntityHierarchyAction
    {
        entt::entity Clicked = entt::null;
        entt::entity Delete = entt::null;
        bool Toggle = false;
        bool RejectedDrop = false;
        std::optional<EntityHierarchyMove> Move;
    };

    // Frame-local presentation data; the Scene remains the owner of all relationships.
    EntityHierarchyTree BuildEntityHierarchyTree(Engine::Scene& scene);

    class EntityHierarchyView
    {
    public:
        EntityHierarchyView() = default;
        ~EntityHierarchyView();
        EntityHierarchyView(const EntityHierarchyView&) = delete;
        EntityHierarchyView& operator=(const EntityHierarchyView&) = delete;
        EntityHierarchyAction Draw(Engine::Scene& scene, std::span<const entt::entity> selected,
            entt::entity reveal = entt::null, bool locked = false);
        void ReleaseFinishedDrag();
        void ResetDrag();
    private:
        Engine::Scene* m_DragSource = nullptr;
    };
}
