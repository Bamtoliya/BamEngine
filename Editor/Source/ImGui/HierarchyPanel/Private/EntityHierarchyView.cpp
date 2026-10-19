#include "EntityHierarchyView.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "UIComponents.h"
#include "LocalizationManager.h"
#include "Functions.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include "IconsFontAwesome7.h"

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

    bool ContainsIgnoreCase(std::string_view source, std::string_view target)
    {
        if (target.empty()) return true;
        if (source.size() < target.size()) return false;
        auto it = std::search(source.begin(), source.end(), target.begin(), target.end(),
            [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
        return it != source.end();
    }

    struct EntityTypeVisual { const char* Icon; ImVec4 Color; };

    EntityTypeVisual GetEntityVisual(const entt::registry& world, entt::entity entity, bool hasChildren)
    {
        if (world.all_of<Engine::CameraComponent>(entity))
            return { ICON_FA_CAMERA, ImVec4(0.45f, 0.75f, 1.0f, 1.0f) }; // 하늘색 (카메라)
        if (world.all_of<Engine::LightComponent>(entity))
            return { ICON_FA_LIGHTBULB, ImVec4(1.0f, 0.85f, 0.35f, 1.0f) }; // 밝은 노랑 (라이트)
        if (world.all_of<Engine::StaticMeshRendererComponent>(entity))
            return { ICON_FA_CUBE, ImVec4(0.35f, 0.85f, 0.85f, 1.0f) }; // 청록색 (3D 메시)
        if (world.all_of<Engine::SkinnedMeshRendererComponent>(entity))
            return { ICON_FA_USER, ImVec4(1.0f, 0.65f, 0.35f, 1.0f) }; // 주황색 (스킨드 메시)
        if (world.all_of<Engine::SpriteRendererComponent>(entity))
            return { ICON_FA_IMAGE, ImVec4(0.45f, 0.9f, 0.5f, 1.0f) }; // 녹색 (2D 스프라이트)
        if (world.all_of<Engine::CanvasComponent>(entity) || world.all_of<Engine::RectTransformComponent>(entity))
            return { ICON_FA_WINDOW_MAXIMIZE, ImVec4(0.85f, 0.55f, 1.0f, 1.0f) }; // 보라색 (UI 요소)
        if (world.all_of<Engine::TerrainComponent>(entity))
            return { ICON_FA_MOUNTAIN, ImVec4(0.35f, 0.85f, 0.45f, 1.0f) }; // 연두색 (터레인/지형)
        if (hasChildren)
            return { ICON_FA_FOLDER, ImVec4(0.92f, 0.82f, 0.55f, 1.0f) }; // 따뜻한 베이지 (폴더/그룹)

        return { ICON_FA_CIRCLE, ImVec4(0.65f, 0.65f, 0.65f, 0.8f) }; // 회색 (기본 빈 엔티티)
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
    std::span<const entt::entity> selected, entt::entity reveal, bool locked, std::string_view filter)
{
    ReleaseFinishedDrag();
    scene.UpdateHierarchyFlags();
    const auto tree = BuildEntityHierarchyTree(scene);
    auto& world = scene.GetRegistry();
    EntityHierarchyAction action;

    // --- 검색 필터링 및 조상 노드 수집 ---
    const bool isFiltering = !filter.empty();
    std::unordered_set<entt::entity> matchingEntities;
    std::unordered_set<entt::entity> visibleInSearch;
    if (isFiltering)
    {
        for (const auto& [entity, parent] : tree.Parents)
        {
            if (ContainsIgnoreCase(EntityName(scene, entity), filter))
            {
                matchingEntities.insert(entity);
                visibleInSearch.insert(entity);
                for (auto curr = parent; curr != entt::null; curr = tree.Parents.contains(curr) ? tree.Parents.at(curr) : entt::null)
                    visibleInSearch.insert(curr);
            }
        }
    }

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
    int rowIndex = 0; // 줄무늬 배경 카운터
    const auto draw = [&](auto&& self, entt::entity entity) -> void
        {
            // 검색 필터 적용: 검색 중일 때 일치 노드나 그 조상이 아니면 표시하지 않음
            if (isFiltering && !visibleInSearch.contains(entity)) return;

            // [방안 2] 교차 줄무늬 배경 (홀수 행마다 은은한 음영 추가)
            if ((rowIndex++ % 2) == 1)
            {
                const ImVec2 rowMin = ImVec2(ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y);
                const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowWidth(), rowMin.y + ImGui::GetFrameHeight());
                ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 7)); // 2.7% 밝기
            }

            const auto& children = tree.Children.at(entity);
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (std::ranges::find(selected, entity) != selected.end()) flags |= ImGuiTreeNodeFlags_Selected;
            if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::PushID(std::to_string(entt::to_integral(entity)).c_str());

            // 조상이거나, 검색 중 일치 자식을 가진 부모 노드는 자동으로 펼침
            if (ancestors.contains(entity) || (isFiltering && visibleInSearch.contains(entity) && !matchingEntities.contains(entity)))
                ImGui::SetNextItemOpen(true);

            auto* flagComp = world.try_get<Engine::FlagComponent>(entity);
            const bool effectiveActive = flagComp ? flagComp->IsActiveInHierarchy() : true;
            const bool effectiveVisible = flagComp ? flagComp->IsVisibleInHierarchy() : true;
            const bool dimNode = !effectiveActive || !effectiveVisible;
            const bool isMatch = matchingEntities.contains(entity);

            // [방안 1] 엔티티 타입별 시각 정보 조회 (아이콘 및 테마 색상)
            const auto visual = GetEntityVisual(world, entity, !children.empty());

            // 텍스트 색상 결정 (항상 정확히 1번 Push & 1번 Pop)
            if (dimNode) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            else if (isMatch) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.2f, 1.0f)); // 검색 일치 황금색
            else ImGui::PushStyleColor(ImGuiCol_Text, visual.Color); // 타입별 고유 색상

            // 레이블: [아이콘] [이름] (자식이 있으면 자식 개수 뱃지 추가)
            std::string label = std::string(visual.Icon) + " " + EntityName(scene, entity);
            if (!children.empty()) label += " (" + std::to_string(children.size()) + ")";

            ImGui::SetNextItemAllowOverlap();
            const bool opened = ImGui::TreeNodeEx("EntityNode", flags, "%s", label.c_str());
            ImGui::PopStyleColor(); // 색상 복원

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && !locked)
            {
                action.Clicked = entity;
                action.Toggle = ImGui::GetIO().KeyCtrl;
            }
            if (entity == reveal) ImGui::SetScrollHereY();
            if (tree.Invalid.contains(entity) && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", Text("UI_HIERARCHY_INVALID", "Invalid hierarchy links; shown without repair.").c_str());

            // --- 1. [핵심] TreeNodeEx에 Drag & Drop 및 우클릭 메뉴를 먼저 바인딩 ---
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
                const auto* link = world.try_get<Engine::HierarchyComponent>(entity);
                const bool hasParent = link && link->parent != entt::null;

                if(hasParent && ImGui::MenuItem(Text("UI_HIERARCHY_MOVE_UP", "Move Up").c_str()))
                    action.MoveUp = entity;
                if (hasParent && ImGui::MenuItem(Text("UI_HIERARCHY_MOVE_DOWN", "Move Down").c_str()))
                    action.MoveDown = entity;

                if (hasParent) ImGui::Separator();

                if (ImGui::MenuItem(Text("UI_DELETE_ENTITY", "Delete entity and children").c_str()))
                    action.Delete = entity;
                ImGui::EndPopup();
            }

            // --- 2. 우측 정렬 컨트롤: Active 체크박스 & Visible 눈알 아이콘 ---
            if (!locked && flagComp)
            {
                const float itemSize = ImGui::GetFrameHeight();
                const float rightPadding = 12.0f;
                const float controlsWidth = (itemSize * 2.0f) + ImGui::GetStyle().ItemSpacing.x + rightPadding;
                const float alignX = ImGui::GetWindowContentRegionMax().x - controlsWidth;

                if (alignX > ImGui::GetCursorPosX())
                {
                    ImGui::SameLine(alignX);

                    // 눈알 아이콘 버튼 (가시성 토글)
                    const bool localVisible = flagComp->IsVisible();
                    const char* eyeIcon = localVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
                    if (!effectiveVisible && localVisible) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                    else ImGui::PushStyleColor(ImGuiCol_Text, localVisible ? ImVec4(1, 1, 1, 1) : ImVec4(0.4f, 0.4f, 0.4f, 1));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));

                    // 눈알 아이콘 버튼 (가시성 토글)
                    if (ImGui::Button(eyeIcon, ImVec2(itemSize, itemSize)))
                    {
                        action.ToggleVisible = entity;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(!effectiveVisible && localVisible ? "Hidden by parent hierarchy." : "Toggle Visibility");
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine();

                    // 활성화 체크박스 (Active 토글)
                    bool localActive = flagComp->IsActive();
                    if (ImGui::Checkbox("##EntityActive", &localActive))
                    {
                        action.ToggleActive = entity;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip(!effectiveActive && localActive ? "Deactivated by parent hierarchy." : "Toggle Active");
                }
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

    if (isFiltering && matchingEntities.empty())
        ImGui::TextDisabled("%s", Text("UI_HIERARCHY_NO_MATCH", "No matching entities found.").c_str());
    else if (tree.Roots.empty())
        ImGui::TextDisabled("%s", Text("UI_HIERARCHY_EMPTY", "No entities.").c_str());
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
