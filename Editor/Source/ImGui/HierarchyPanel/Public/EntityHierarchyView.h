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
        entt::entity FocusEntity = entt::null;
        entt::entity OpenUIEditor = entt::null;
        entt::entity CreateChild = entt::null;
        entt::entity Unparent = entt::null;
        entt::entity StartRename = entt::null;
        entt::entity RenameEntity = entt::null;
        std::wstring NewName;
        bool Duplicate = false;
        std::vector<entt::entity> DeleteEntities;
        entt::entity MoveUp = entt::null;
        entt::entity MoveDown = entt::null;
        entt::entity ToggleVisible = entt::null;
        entt::entity ToggleActive = entt::null;
        entt::entity SaveAsPrefab = entt::null;
        entt::entity UnpackPrefab = entt::null;
        bool Copy = false;
        bool Paste = false;
        entt::entity PasteAsChild = entt::null;
        std::wstring DropPrefabFile;
        entt::entity DropPrefabTarget = entt::null;
        std::optional<std::vector<entt::entity>> Selection;
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
        EntityHierarchyAction Draw(Engine::Scene& scene, std::span<const entt::entity> selected,
            entt::entity reveal = entt::null, bool locked = false, std::string_view filter = {}, bool multiSelect = false);
        void StartRename(entt::entity entity, const std::string& currentName);
        void ReleaseFinishedDrag();
        void ResetDrag();
    private:
        Engine::Scene* m_DragSource = nullptr;
        entt::entity m_RenamingEntity = entt::null;
        char m_RenameBuffer[128] = "";
        bool m_FocusRenameInput = false;
        // 펼친 노드 집합(영속 ID 기준). 클리퍼로 그리지 않은 줄의 열림 상태도 유지하기 위해 뷰가 직접 소유한다.
        std::unordered_set<std::uint64_t> m_Expanded;
    };
}
