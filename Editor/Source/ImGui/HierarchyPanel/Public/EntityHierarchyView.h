#pragma once

#include <entt/entity/entity.hpp>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <cstdint>
#include <string_view>
#include <string>

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
        entt::entity MoveUp = entt::null;
        entt::entity MoveDown = entt::null;
        entt::entity ToggleVisible = entt::null;
        entt::entity ToggleActive = entt::null;
        bool Toggle = false;
        bool Range = false;
        std::vector<entt::entity> VisibleEntities;
        bool RejectedDrop = false;
        std::optional<EntityHierarchyMove> Move;
    };

    // Frame-local presentation data; the Scene remains the owner of all relationships.
    EntityHierarchyTree BuildEntityHierarchyTree(Engine::Scene& scene);
    // 반환한 참조는 같은 registry의 캐시 재구축 또는 registry 종료 시까지 유효합니다.
    const EntityHierarchyTree& GetEntityHierarchyTree(Engine::Scene& scene);

    // 반환 참조는 해당 이름 갱신, 캐시 정리 또는 registry 종료 시까지 유효합니다.
    const std::string& GetEntityHierarchyName(Engine::Scene& scene, entt::entity entity);

    class EntityHierarchyView
    {
    public:
        EntityHierarchyView() = default;
        ~EntityHierarchyView();
        EntityHierarchyView(const EntityHierarchyView&) = delete;
        EntityHierarchyView& operator=(const EntityHierarchyView&) = delete;
        EntityHierarchyAction Draw(Engine::Scene& scene, std::span<const entt::entity> selected, entt::entity reveal = entt::null, bool locked = false, std::string_view filter = {});
        void ReleaseFinishedDrag();
        void ResetDrag();
    private:
        Engine::Scene* m_DragSource = nullptr;
    };
}
