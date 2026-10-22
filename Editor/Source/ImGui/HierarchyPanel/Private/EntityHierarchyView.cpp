#include "EntityHierarchyView.h"
#include "InputManager.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "UIComponents.h"
#include "LocalizationManager.h"
#include "Functions.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstring>
#include "IconsFontAwesome7.h"
#include <fmt/format.h>

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

    std::string Text(const char* key)
    {
        return Engine::LocalizationManager::Get().GetText(std::string(key));
    }

    struct CachedEntityText
    {
        std::wstring Source;
        std::string Name;
        std::string FallbackFormat;
        std::string Label;
        std::string Icon;
        std::uint64_t PersistentId = 0;
        std::size_t ChildCount = 0;
        bool NameReady = false;
        bool LabelReady = false;
    };

    struct HierarchyTextCache
    {
        std::unordered_map<entt::entity, CachedEntityText> Entries;
    };

    const std::string& EntityLabel(Engine::Scene& scene, entt::entity entity,
        std::string_view icon, std::size_t childCount)
    {
        const auto& name = Editor::GetEntityHierarchyName(scene, entity);
        auto& entry = scene.GetRegistry().ctx().emplace<HierarchyTextCache>().Entries.at(entity);
        if (!entry.LabelReady || entry.Icon != icon || entry.ChildCount != childCount)
        {
            std::string label(icon);
            label += " ";
            label += name;
            if (childCount != 0) label += " (" + std::to_string(childCount) + ")";

            entry.Label = std::move(label);
            entry.Icon.assign(icon);
            entry.ChildCount = childCount;
            entry.LabelReady = true;
        }
        return entry.Label;
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

const std::string& Editor::GetEntityHierarchyName(Engine::Scene& scene, entt::entity entity)
{
    static const std::string empty;
    auto& world = scene.GetRegistry();
    if (!world.valid(entity) || !scene.FindEntity(entity)) return empty;

    const auto* name = world.try_get<Engine::NameComponent>(entity);
    const std::wstring_view source = name ? std::wstring_view(name->name) : std::wstring_view{};
    const auto* id = world.try_get<Engine::IDComponent>(entity);
    const std::uint64_t persistentId = id ? id->id : entt::to_integral(entity);
    auto& entry = world.ctx().emplace<HierarchyTextCache>().Entries[entity];
    bool changed = !entry.NameReady || entry.PersistentId != persistentId || entry.Source != source;

    if (source.empty())
    {
        const auto format = Engine::LocalizationManager::Get().GetTextForDisplay("Hierarchy.EntityFallback");
        if (changed || entry.FallbackFormat != format.c_str())
        {
            entry.Name = fmt::format(fmt::runtime(format.c_str()), persistentId);
            entry.FallbackFormat = format.c_str();
            changed = true;
        }
    }
    else if (changed)
    {
        entry.Name = Engine::WStrToStr(name->name);
        entry.FallbackFormat.clear();
    }

    if (changed)
    {
        entry.Source.assign(source);
        entry.PersistentId = persistentId;
        entry.NameReady = true;
        entry.LabelReady = false;
    }
    return entry.Name;
}

Editor::EntityHierarchyTree Editor::BuildEntityHierarchyTree(Engine::Scene& scene)
{
    EntityHierarchyTree tree;
    const auto& world = scene.GetRegistry();
    std::vector<std::pair<std::uint64_t, entt::entity>> sortedEntities;
    if (const auto* storage = world.storage<entt::entity>())
    {
        sortedEntities.reserve(storage->size());
        for (auto [entity] : storage->each())
        {
            if (!scene.FindEntity(entity)) continue;
            const auto* id = world.try_get<Engine::IDComponent>(entity);
            const auto key = id ? id->id : static_cast<std::uint64_t>(entt::to_integral(entity));
            sortedEntities.emplace_back(key, entity);
        }
    }
    std::sort(sortedEntities.begin(), sortedEntities.end(), [](const auto& left, const auto& right)
        {
            if (left.first != right.first) return left.first < right.first;
            return entt::to_integral(left.second) < entt::to_integral(right.second);
        });

    std::vector<entt::entity> entities;
    entities.reserve(sortedEntities.size());
    tree.Parents.reserve(sortedEntities.size());
    tree.Children.reserve(sortedEntities.size());
    for (const auto& [key, entity] : sortedEntities)
    {
        entities.push_back(entity);
        tree.Parents.emplace(entity, entt::null);
    }

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
        const auto& childList = relation->children;
        for (std::size_t i = 0; i < childList.size(); ++i)
        {
            const auto child = childList[i];
            const auto* link = tree.Parents.contains(child) ?
                world.try_get<Engine::HierarchyComponent>(child) : nullptr;
            const bool duplicate = std::find(childList.begin(), childList.begin() + static_cast<std::ptrdiff_t>(i), child) !=
                childList.begin() + static_cast<std::ptrdiff_t>(i);
            if (!link || link->parent != entity || duplicate) tree.Invalid.insert(entity);
        }
    }
    // Break invalid edges in the view only. All entities remain visible; component data is never repaired here.
    for (const auto entity : entities)
    {
        std::array<entt::entity, 256> chain{};
        std::size_t depth = 0;
        for (auto current = entity; current != entt::null; current = tree.Parents.at(current))
        {
            const bool cycle = std::find(chain.begin(), chain.begin() + depth, current) != chain.begin() + depth;
            if (cycle || depth >= chain.size())
            {
                tree.Parents[entity] = entt::null;
                tree.Invalid.insert(entity);
                break;
            }
            chain[depth++] = current;
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

const Editor::EntityHierarchyTree& Editor::GetEntityHierarchyTree(Engine::Scene& scene)
{
    struct Source
    {
        entt::entity Entity = entt::null;
        std::uint64_t Order = 0;
        bool HasHierarchy = false;
        entt::entity Parent = entt::null;
        std::vector<entt::entity> Children;
    };
    struct Cache
    {
        EntityHierarchyTree Tree;
        std::vector<Source> Sources;
        bool Valid = false;
    };

    auto& world = scene.GetRegistry();
    auto& cache = world.ctx().emplace<Cache>();
    const auto* storage = std::as_const(world).storage<entt::entity>();

    const auto unchanged = [&]() -> bool
        {
            if (!cache.Valid) return false;
            std::size_t index = 0;
            if (storage)
            {
                for (auto [entity] : storage->each())
                {
                    if (!scene.FindEntity(entity)) continue;
                    if (index >= cache.Sources.size()) return false;

                    const auto& previous = cache.Sources[index++];
                    const auto* id = world.try_get<Engine::IDComponent>(entity);
                    const auto order = id ? id->id : static_cast<std::uint64_t>(entt::to_integral(entity));
                    const auto* link = world.try_get<Engine::HierarchyComponent>(entity);
                    if (previous.Entity != entity || previous.Order != order ||
                        previous.HasHierarchy != (link != nullptr))
                        return false;
                    if (link && (previous.Parent != link->parent || previous.Children != link->children))
                        return false;
                }
            }
            return index == cache.Sources.size();
        };

    if (unchanged()) return cache.Tree;

    std::vector<Source> sources;
    if (storage)
    {
        sources.reserve(storage->size());
        for (auto [entity] : storage->each())
        {
            if (!scene.FindEntity(entity)) continue;

            Source source;
            source.Entity = entity;
            const auto* id = world.try_get<Engine::IDComponent>(entity);
            source.Order = id ? id->id : static_cast<std::uint64_t>(entt::to_integral(entity));
            const auto* link = world.try_get<Engine::HierarchyComponent>(entity);
            source.HasHierarchy = link != nullptr;
            if (link)
            {
                source.Parent = link->parent;
                source.Children = link->children;
            }
            sources.push_back(std::move(source));
        }
    }

    auto tree = BuildEntityHierarchyTree(scene);
    cache.Tree = std::move(tree);
    cache.Sources = std::move(sources);
    cache.Valid = true;
    return cache.Tree;
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
    const auto& tree = GetEntityHierarchyTree(scene);
    auto& world = scene.GetRegistry();
    auto& textCache = world.ctx().emplace<HierarchyTextCache>();
    std::erase_if(textCache.Entries, [&](const auto& item) { return !tree.Parents.contains(item.first); });
    EntityHierarchyAction action;

    // --- 검색 필터링 및 조상 노드 수집 ---
    const bool isFiltering = !filter.empty();
    std::unordered_set<entt::entity> matchingEntities;
    std::unordered_set<entt::entity> visibleInSearch;
    if (isFiltering)
    {
        for (const auto& [entity, parent] : tree.Parents)
        {
            if (ContainsIgnoreCase(GetEntityHierarchyName(scene, entity), filter))
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
            action.VisibleEntities.push_back(entity);

            // [방안 2] 교차 줄무늬 배경 (홀수 행마다 은은한 음영 추가)
            if ((rowIndex++ % 2) == 1)
            {
                const ImVec2 rowMin = ImVec2(ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y);
                const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowWidth(), rowMin.y + ImGui::GetFrameHeight());
                ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 7)); // 2.7% 밝기
            }

            const auto& children = tree.Children.at(entity);
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding;
            if (std::ranges::find(selected, entity) != selected.end()) flags |= ImGuiTreeNodeFlags_Selected;
            if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            ImGui::PushID(static_cast<int>(entt::to_integral(entity)));

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
            const auto& label = EntityLabel(scene, entity, visual.Icon, children.size());

            ImGui::SetNextItemAllowOverlap();
            const bool opened = ImGui::TreeNodeEx("EntityNode", flags, "%s", label.c_str());
            ImGui::PopStyleColor(); // 색상 복원

            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() && !locked)
            {
                auto& input = Engine::InputManager::Get();
                action.Clicked = entity;
                action.Toggle = input.IsCtrlPressed();
                action.Range = input.IsShiftPressed();
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

            // --- 2. 우측 정렬 컨트롤: Active & Visible 토글 아이콘 버튼 ---
            if (!locked && flagComp)
            {
                const float itemSize = ImGui::GetFrameHeight();
                const float spacing = ImGui::GetStyle().ItemSpacing.x;
                const float rightPadding = ImGui::GetStyle().WindowPadding.x;
                const float controlsWidth = (itemSize * 2.0f) + spacing + rightPadding;
                const float alignX = ImGui::GetWindowContentRegionMax().x - controlsWidth;

                if (alignX > ImGui::GetCursorPosX())
                {
                    ImGui::SameLine(alignX);

                    // 1) 가시성(Visible) 눈알 아이콘 버튼
                    const bool localVisible = flagComp->IsVisible();
                    const char* eyeIcon = localVisible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
                    if (!effectiveVisible && localVisible) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                    else ImGui::PushStyleColor(ImGuiCol_Text, localVisible ? ImVec4(1, 1, 1, 1) : ImVec4(0.4f, 0.4f, 0.4f, 1));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));

                    if (ImGui::Button(eyeIcon, ImVec2(itemSize, itemSize)))
                    {
                        action.ToggleVisible = entity;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", Text(!effectiveVisible && localVisible ?
                            "Hierarchy.HiddenByParent" : "Hierarchy.ToggleVisibility").c_str());
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine(0.0f, spacing);

                    // 2) 활성화(Active) 체크 아이콘 버튼 (체크박스 대신 동일 규격 아이콘 버튼으로 통일)
                    const bool localActive = flagComp->IsActive();
                    const char* activeIcon = ICON_FA_CHECK;
                    if (!effectiveActive && localActive) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.5f));
                    else ImGui::PushStyleColor(ImGuiCol_Text, localActive ? ImGui::GetStyle().Colors[ImGuiCol_CheckMark] : ImVec4(0.35f, 0.35f, 0.35f, 1));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.15f));

                    if (ImGui::Button(activeIcon, ImVec2(itemSize, itemSize)))
                    {
                        action.ToggleActive = entity;
                    }
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", Text(!effectiveActive && localActive ?
                            "Hierarchy.DeactivatedByParent" : "Hierarchy.ToggleActive").c_str());
                    ImGui::PopStyleColor(3);
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    for (const auto root : tree.Roots) draw(draw, root);
    ImGui::PopStyleVar();

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
