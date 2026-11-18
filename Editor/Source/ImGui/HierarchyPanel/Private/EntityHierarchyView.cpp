#include "EntityHierarchyView.h"
#include "InputManager.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "RenderComponents.h"
#include "UIComponents.h"
#include "LocalizationManager.h"
#include "Functions.h"
#include <imgui.h>
#include <imgui_internal.h>
#include "IconsFontAwesome7.h"
#include <fmt/format.h>
#include "EntityClipboard.h"
#include "ShortcutManager.h"
#include "PrefabInstanceComponent.h"
#include "PlatformPath.h"
#include "PrefabModeManager.h"
#include "SelectionManager.h"
#include "ReflectionInspectorSupport.h"
#include <cctype>
#include <utility>

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

    entt::entity FindPrefabRootHandle(const entt::registry& world, entt::entity entity)
    {
        entt::entity cur = entity;
        while (cur != entt::null)
        {
            if (world.all_of<Engine::PrefabInstanceComponent>(cur))
                return cur;
            const auto* link = world.try_get<Engine::HierarchyComponent>(cur);
            cur = link ? link->parent : entt::null;
        }
        return entt::null;
    }
    EntityTypeVisual GetEntityVisual(const entt::registry& world, entt::entity entity, bool hasChildren)
    {
        const entt::entity prefabRoot = FindPrefabRootHandle(world, entity);
        if (prefabRoot != entt::null)
        {
            const bool isRoot = (prefabRoot == entity);
            const char* icon = isRoot ? ICON_FA_CUBES : (hasChildren ? ICON_FA_FOLDER : ICON_FA_CUBE);
            return { icon, ImVec4(0.35f, 0.78f, 1.0f, 1.0f) }; // 유니티 스타일 청록색
        }
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

    void PushEntityId(const entt::registry& world, entt::entity entity)
    {
        const auto* id = world.try_get<Engine::IDComponent>(entity);
        const bool persistent = id && id->id != 0;
        const std::uint64_t value = persistent ?
            id->id : static_cast<std::uint64_t>(entt::to_integral(entity));

        std::array<char, 32> buffer{};
        buffer[0] = persistent ? 'P' : 'R';

        const auto result = std::to_chars(buffer.data() + 1, buffer.data() + buffer.size(), value);
        ImGui::PushID(buffer.data(), result.ptr);
    }

    struct HierarchyRow
    {
        entt::entity Entity = entt::null;
        int Depth = 0;
    };

    std::uint64_t GetExpandKey(const entt::registry& world, entt::entity entity)
    {
        const auto* id = world.try_get<Engine::IDComponent>(entity);
        return id && id->id != 0 ? id->id : (std::uint64_t{ 1 } << 63) | entt::to_integral(entity);
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

void EntityHierarchyView::StartRename(entt::entity entity, const std::string& currentName)
{
    m_RenamingEntity = entity;
    strncpy_s(m_RenameBuffer, currentName.c_str(), sizeof(m_RenameBuffer));
    m_FocusRenameInput = true;
}

void Editor::EntityHierarchyView::ResetDrag()
{
    if (m_DragSource) m_DragSource->Release();
    m_DragSource = nullptr;
    m_DragAnchor = entt::null;
    m_DragMembers.clear();
}

void Editor::EntityHierarchyView::ReleaseFinishedDrag()
{
    const auto* payload = ImGui::GetDragDropPayload();
    if (!payload || !payload->IsDataType(EntityPayload)) ResetDrag();
}

Editor::EntityHierarchyAction Editor::EntityHierarchyView::Draw(Engine::Scene& scene,
    std::span<const entt::entity> selected, entt::entity reveal, bool locked, std::string_view filter, bool multiSelect)
{
    ReleaseFinishedDrag();
    const auto& tree = GetEntityHierarchyTree(scene);
    auto& world = scene.GetRegistry();
    auto& textCache = world.ctx().emplace<HierarchyTextCache>();
    std::erase_if(textCache.Entries, [&](const auto& item) { return !tree.Parents.contains(item.first); });
    EntityHierarchyAction action;
    const bool selectableRows = multiSelect && !locked;
    std::vector<entt::entity> nextSelection(selected.begin(), selected.end());
    entt::entity selectDragged = entt::null;
    const auto selectionId = [](entt::entity entity)
    {
        return static_cast<ImGuiSelectionUserData>(entt::to_integral(entity));
    };

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
    for (const auto ancestor : ancestors)
        m_Expanded.insert(GetExpandKey(world, ancestor));
    ImGui::PushID(&scene);
    const auto acceptMove = [&](entt::entity target, EntityDropPosition position, const ImRect* edge = nullptr)
    {
        const auto flags = edge ? ImGuiDragDropFlags_AcceptPeekOnly : ImGuiDragDropFlags_None;
        const auto* payload = ImGui::AcceptDragDropPayload(EntityPayload, flags);
        if (!payload) return;
        if (!payload->IsDelivery())
        {
            if (edge && payload->IsPreview() && std::ranges::all_of(m_DragMembers, [&](const auto& member)
                {
                    return tree.Parents.contains(member.Entity) && !tree.Invalid.contains(member.Entity) &&
                        member.Entity != target && tree.Parents.at(member.Entity) == tree.Parents.at(target);
                }))
            {
                const float y = position == EntityDropPosition::Before ? edge->Min.y : edge->Max.y;
                ImGui::GetWindowDrawList()->AddLine(ImVec2(edge->Min.x, y), ImVec2(edge->Max.x, y),
                    ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
            }
            return;
        }
        EntityDragPayload source;
        if (payload->DataSize != static_cast<int>(sizeof(source))) { action.RejectedDrop = true; return; }
        std::memcpy(&source, payload->Data, sizeof(source));
        const auto isCurrent = [&](entt::entity entity, std::uint64_t persistentId)
        {
            const auto* id = world.valid(entity) ? world.try_get<Engine::IDComponent>(entity) : nullptr;
            return id && persistentId != 0 && id->id == persistentId && scene.FindEntity(entity);
        };
        if (source.Scene != &scene || !isCurrent(source.Entity, source.PersistentId))
        {
            action.RejectedDrop = true;
            return;
        }

        std::vector<entt::entity> children;
        if (m_DragSource == source.Scene && m_DragAnchor == source.Entity)
        {
            children.reserve(m_DragMembers.size());
            for (const auto& member : m_DragMembers)
            {
                if (!isCurrent(member.Entity, member.PersistentId))
                {
                    action.RejectedDrop = true;
                    return;
                }
                children.push_back(member.Entity);
            }
        }
        else
            children.push_back(source.Entity);

        action.Move = EntityHierarchyMove{ std::move(children), target, source.Entity, position };
    };
    const auto acceptPrefab = [&](entt::entity parent)
        {
            const auto* dropped = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM");
            if (!dropped) return;

            const auto decoded = Editor::DecodeInspectorResourcePath(dropped->Data, dropped->DataSize);
            if (!decoded)
            {
                action.RejectedDrop = true;
                return;
            }

            std::string extension = Engine::WStrToStr(std::filesystem::path(*decoded).extension().wstring());
            for (char& ch : extension) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

            if (extension != ".json" && extension != ".bamprefab" && extension != ".prefab")
            {
                action.RejectedDrop = true;
                return;
            }

            action.DropPrefabFile = *decoded;
            action.DropPrefabTarget = parent;
        };
    const float indentStep = ImGui::GetStyle().IndentSpacing;
    const auto drawRow = [&](const HierarchyRow& row, int rowIndex) -> void
        {
            const entt::entity entity = row.Entity;

            // [방안 2] 교차 줄무늬 배경 (홀수 행마다 은은한 음영 추가)
            if ((rowIndex % 2) == 1)
            {
                const ImVec2 rowMin = ImVec2(ImGui::GetWindowPos().x, ImGui::GetCursorScreenPos().y);
                const ImVec2 rowMax = ImVec2(rowMin.x + ImGui::GetWindowWidth(), rowMin.y + ImGui::GetFrameHeight());
                ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, IM_COL32(255, 255, 255, 7)); // 2.7% 밝기
            }

            const auto& children = tree.Children.at(entity);
            const bool isCanvas = world.all_of<Engine::CanvasComponent>(entity);
            const auto spanFlag = selectableRows ? ImGuiTreeNodeFlags_SpanAvailWidth : ImGuiTreeNodeFlags_SpanFullWidth;
            auto flags = ImGuiTreeNodeFlags_OpenOnArrow | spanFlag | ImGuiTreeNodeFlags_FramePadding;
            if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf; // 자식이 없으면 리프 노드로 처리하여 > 화살표 숨김
            if (!isCanvas) flags |= ImGuiTreeNodeFlags_OpenOnDoubleClick;
            if (std::ranges::find(nextSelection, entity) != nextSelection.end()) flags |= ImGuiTreeNodeFlags_Selected;
            flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen; // 자식은 평탄화된 줄 목록에서 따로 그린다
            PushEntityId(world, entity);
            if (row.Depth > 0) ImGui::Indent(indentStep * static_cast<float>(row.Depth));

            // 조상이거나, 검색 중 일치 자식을 가진 부모 노드는 자동으로 펼침
            const std::uint64_t expandKey = GetExpandKey(world, entity);
            const bool forceOpen = isFiltering && !matchingEntities.contains(entity);
            if (!children.empty())
                ImGui::SetNextItemOpen(forceOpen || m_Expanded.contains(expandKey));
            const auto syncOpen = [&](bool isOpen)
                {
                    if (children.empty() || forceOpen || !ImGui::IsItemToggledOpen()) return;
                    if (isOpen) m_Expanded.insert(expandKey);
                    else m_Expanded.erase(expandKey);
                };

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
            bool opened = false;
            const bool renaming = m_RenamingEntity == entity;
            if (renaming)
            {
                // 리네임 중일 때는 아이콘만 노드로 그리고 바로 옆에 인라인 텍스트 입력창 배치
                opened = ImGui::TreeNodeEx("EntityNode", flags, "%s", visual.Icon);
                syncOpen(opened);
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (m_FocusRenameInput)
                {
                    ImGui::SetKeyboardFocusHere();
                    m_FocusRenameInput = false;
                }
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 60.0f);
                if (ImGui::InputText("##EntityRenameInput", m_RenameBuffer, sizeof(m_RenameBuffer),
                    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                {
                    action.RenameEntity = entity;
                    action.NewName = Engine::StrToWStr(m_RenameBuffer);
                    m_RenamingEntity = entt::null;
                }
                else if (Engine::InputManager::Get().IsKeyButtonDown(Engine::EKeyCode::Escape))
                {
                    m_RenamingEntity = entt::null;
                }
                else if (ImGui::IsItemDeactivated() && !Engine::InputManager::Get().IsKeyButtonDown(Engine::EKeyCode::Escape))
                {
                    action.RenameEntity = entity;
                    action.NewName = Engine::StrToWStr(m_RenameBuffer);
                    m_RenamingEntity = entt::null;
                }
            }
            else
            {
                if (selectableRows)
                    ImGui::SetNextItemSelectionUserData(selectionId(entity));
                opened = ImGui::TreeNodeEx("EntityNode", flags, "%s", label.c_str());
                syncOpen(opened);
                ImGui::PopStyleColor();
            }

            if (!selectableRows && !locked && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                action.Clicked = entity;
            if (!locked && m_RenamingEntity != entity && isCanvas &&
                ImGui::IsItemHovered() && !ImGui::IsItemToggledOpen() &&
                Engine::InputManager::Get().IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
            {
                action.OpenUIEditor = entity;
            }
            if (entity == reveal) ImGui::SetScrollHereY();
            if (tree.Invalid.contains(entity) && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", Text("Hierarchy.InvalidLinks", "Invalid hierarchy links; shown without repair.").c_str());

            // --- 1. [핵심] TreeNodeEx에 Drag & Drop 및 우클릭 메뉴를 먼저 바인딩 ---
            const auto* id = world.try_get<Engine::IDComponent>(entity);
            if (!locked && m_RenamingEntity != entity && id && id->id != 0 && ImGui::BeginDragDropSource())
            {
                if (m_DragSource != &scene || m_DragAnchor != entity)
                {
                    ResetDrag();
                    scene.AddRef();
                    m_DragSource = &scene;
                    m_DragAnchor = entity;
                    if (selectableRows && std::ranges::find(selected, entity) != selected.end())
                    {
                        for (const auto handle : selected)
                        {
                            const auto* memberId = world.valid(handle) ? world.try_get<Engine::IDComponent>(handle) : nullptr;
                            m_DragMembers.push_back({ handle, memberId ? memberId->id : 0 });
                        }
                    }
                    else
                    {
                        m_DragMembers.push_back({ entity, id->id });
                        if (selectableRows)
                            selectDragged = entity;
                    }
                }
                // Single-entity drop targets still consume the original anchor payload.
                const EntityDragPayload payload{ &scene, entity, id->id };
                ImGui::SetDragDropPayload(EntityPayload, &payload, sizeof(payload));
                if (m_DragMembers.size() > 1)
                {
                    const auto dragLabel = fmt::format(fmt::runtime(Text("Hierarchy.DragSelection")), m_DragMembers.size());
                    ImGui::TextUnformatted(dragLabel.c_str());
                }
                else
                    ImGui::TextUnformatted(label.c_str());
                ImGui::EndDragDropSource();
            }
            const auto* drag = ImGui::GetDragDropPayload();
            const auto parent = tree.Parents.at(entity);
            const auto sourceParent = tree.Parents.find(m_DragAnchor);
            const bool siblingDrop = selectableRows && !renaming && drag && drag->IsDataType(EntityPayload) &&
                m_DragSource == &scene && parent != entt::null && entity != m_DragAnchor &&
                sourceParent != tree.Parents.end() && sourceParent->second == parent && !tree.Invalid.contains(entity);
            if (siblingDrop && ImGui::IsItemVisible())
            {
                const ImRect rowBounds(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                const float edgeHeight = rowBounds.GetHeight() * 0.25f;
                const float top = rowBounds.Min.y + edgeHeight;
                // Below an expanded parent the next visible rows belong to its subtree.
                const bool hasOpenChildren = opened && !children.empty();
                const float bottom = rowBounds.Max.y - (hasOpenChildren ? 0.0f : edgeHeight);
                const auto acceptZone = [&](float minY, float maxY, const char* id, EntityDropPosition position)
                {
                    const ImRect zone(ImVec2(rowBounds.Min.x, minY), ImVec2(rowBounds.Max.x, maxY));
                    if (ImGui::BeginDragDropTargetCustom(zone, ImGui::GetID(id)))
                    {
                        acceptMove(entity, position, position == EntityDropPosition::Into ? nullptr : &zone);
                        ImGui::EndDragDropTarget();
                    }
                };
                acceptZone(rowBounds.Min.y, top, "BeforeSibling", EntityDropPosition::Before);
                acceptZone(top, bottom, "IntoEntity", EntityDropPosition::Into);
                if (!hasOpenChildren)
                    acceptZone(bottom, rowBounds.Max.y, "AfterSibling", EntityDropPosition::After);
            }
            else if (!locked && ImGui::BeginDragDropTarget())
            {
                acceptMove(entity, EntityDropPosition::Into);
                acceptPrefab(entity);
                ImGui::EndDragDropTarget();
            }
            if (!locked && ImGui::BeginPopupContextItem("EntityActions"))
            {
                auto& sm = ShortcutManager::Get();
                const auto* link = world.try_get<Engine::HierarchyComponent>(entity);
                const bool hasParent = link && link->parent != entt::null;
                // 1. 씬 뷰포트에서 포커스 (단축키 힌트 동적 조회)
                if (ImGui::MenuItem((std::string(ICON_FA_CROSSHAIRS " ") + LOCAL("Hierarchy.FocusSelected")).c_str(),
                    sm.GetShortcutHint("Viewport.Focus")))
                    action.FocusEntity = entity;
                if (isCanvas)
                {
                    if (ImGui::MenuItem((std::string(ICON_FA_WINDOW_MAXIMIZE " ") + Text("Hierarchy.OpenInUIEditor", "Open in UI Editor")).c_str()))
                        action.OpenUIEditor = entity;
                }
                // 2. 이름 변경 (단축키 힌트 동적 조회)
                if (ImGui::MenuItem((std::string(ICON_FA_PEN " ") + LOCAL("EditorCommon.Rename")).c_str(),
                    sm.GetShortcutHint("Hierarchy.Rename")))
                    action.StartRename = entity;
                ImGui::Separator();
                // 3. 자식 엔티티 추가
                if (ImGui::MenuItem((std::string(ICON_FA_PLUS " ") + LOCAL("Hierarchy.CreateChild")).c_str()))
                    action.CreateChild = entity;
                ImGui::Separator();
                // 4. 복사 / 복제 / 붙여넣기
                if (ImGui::MenuItem((std::string(ICON_FA_COPY " ") + Text("EditorCommon.Copy", "Copy")).c_str(), sm.GetShortcutHint("Edit.Copy")))
                    action.Copy = true;
                if (ImGui::MenuItem((std::string(ICON_FA_CLONE " ") + LOCAL("Hierarchy.Duplicate")).c_str(), sm.GetShortcutHint("Edit.Duplicate")))
                    action.Duplicate = true;
                const bool hasClipboard = EntityClipboard::HasData();
                if (ImGui::MenuItem((std::string(ICON_FA_PASTE " ") + Text("EditorCommon.Paste", "Paste")).c_str(), sm.GetShortcutHint("Edit.Paste"), false, hasClipboard))
                    action.Paste = true;
                if (ImGui::MenuItem((std::string(ICON_FA_PASTE " ") + Text("EditorCommon.PasteAsChild", "Paste As Child")).c_str(), sm.GetShortcutHint("Edit.PasteAsChild"), false, hasClipboard))
                    action.PasteAsChild = entity;
                ImGui::Separator();
                // 4. 프리팹 관련
                if (ImGui::MenuItem((std::string(ICON_FA_CUBES " ") + Text("Prefab.SaveAs", "Save As Prefab...")).c_str()))
                    action.SaveAsPrefab = entity;
                const entt::entity prefabRoot = FindPrefabRootHandle(world, entity);
                if (prefabRoot != entt::null)
                {
                    const auto* prefabComp = world.try_get<Engine::PrefabInstanceComponent>(prefabRoot);
                    if (prefabComp)
                    {
                        ImGui::Separator();
                        if (ImGui::MenuItem((std::string(ICON_FA_FOLDER_OPEN " ") + LOCAL("Prefab.Open")).c_str()))
                            PrefabModeManager::Get().EnterPrefabMode(prefabComp->prefabAssetPath);
                        if (ImGui::MenuItem((std::string(ICON_FA_LOCATION_ARROW " ") + LOCAL("Prefab.Select")).c_str()))
                            SelectionManager::Get().SetSelectedAsset(Engine::PlatformPath::ResolveUri(prefabComp->prefabAssetPath));
                        if (ImGui::MenuItem((std::string(ICON_FA_LINK_SLASH " ") + LOCAL("Prefab.Unpack")).c_str()))
                            action.UnpackPrefab = prefabRoot;
                    }
                }
                // 5. 계층 순서 이동 및 루트로 이동 (부모 해제)
                if (hasParent)
                {
                    ImGui::Separator();
                    if (ImGui::MenuItem(Text("Hierarchy.MoveUp", "Move Up").c_str()))
                        action.MoveUp = entity;
                    if (ImGui::MenuItem(Text("Hierarchy.MoveDown", "Move Down").c_str()))
                        action.MoveDown = entity;
                    if (ImGui::MenuItem((std::string(ICON_FA_ARROW_UP_FROM_BRACKET " ") + LOCAL("Hierarchy.MoveToRoot")).c_str()))
                        action.Unparent = entity;
                }
                ImGui::Separator();
                // 6. 삭제
                const bool isEntitySelected = std::ranges::find(nextSelection, entity) != nextSelection.end();
                const size_t selectedCount = nextSelection.size();
                std::string deleteLabel;
                if (isEntitySelected && selectedCount > 1)
                    deleteLabel = Text("Hierarchy.DeleteSelectedEntities", "Delete selected entities") + " (" + std::to_string(selectedCount) + ")";
                else
                    deleteLabel = Text("Hierarchy.DeleteEntityAndChildren", "Delete entity and children");
                if (ImGui::MenuItem((std::string(ICON_FA_TRASH " ") + deleteLabel).c_str(), ShortcutManager::Get().GetShortcutHint("Edit.Delete")))
                {
                    if (isEntitySelected && selectedCount > 1)
                        action.DeleteEntities = nextSelection;
                    else
                        action.DeleteEntities.push_back(entity);
                }
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
            if (row.Depth > 0) ImGui::Unindent(indentStep * static_cast<float>(row.Depth));
            ImGui::PopID();
        };
    if (!tree.Invalid.empty())
        ImGui::TextDisabled("%s",
            Text("Hierarchy.InvalidLinks", "Invalid hierarchy links; shown without repair.").c_str());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
    // 1단계: 펼침·검색 상태를 반영한 보이는 줄 목록 생성 (ImGui 위젯 없음)
    std::vector<HierarchyRow> rows;
    rows.reserve(tree.Parents.size());
    const auto collect = [&](auto&& self, entt::entity entity, int depth) -> void
        {
            if (isFiltering && !visibleInSearch.contains(entity)) return;
            rows.push_back({ entity, depth });
            const auto& children = tree.Children.at(entity);
            const bool forceOpen = isFiltering && !matchingEntities.contains(entity);
            if (!children.empty() && (forceOpen || m_Expanded.contains(GetExpandKey(world, entity))))
                for (const auto child : children) self(self, child, depth + 1);
        };
    for (const auto root : tree.Roots) collect(collect, root, 0);

    const auto findRow = [&](ImGuiSelectionUserData id)
    {
        return std::ranges::find_if(rows, [&](const auto& row) { return selectionId(row.Entity) == id; });
    };

    const auto applyRequests = [&](const ImGuiMultiSelectIO& io)
    {
        for (const auto& request : io.Requests)
        {
            if (request.Type == ImGuiSelectionRequestType_SetAll)
            {
                nextSelection.clear();
                if (request.Selected)
                    for (const auto& row : rows) nextSelection.push_back(row.Entity);
            }
            else if (request.Type == ImGuiSelectionRequestType_SetRange)
            {
                auto first = findRow(request.RangeFirstItem);
                auto last = findRow(request.RangeLastItem);
                if (first == rows.end() || last == rows.end())
                    continue;

                if (request.RangeDirection < 0)
                    std::swap(first, last);

                const int step = first <= last ? 1 : -1;
                for (auto row = first;; row += step)
                {
                    const auto it = std::ranges::find(nextSelection, row->Entity);
                    if (request.Selected && it == nextSelection.end())
                        nextSelection.push_back(row->Entity);
                    else if (!request.Selected && it != nextSelection.end())
                        nextSelection.erase(it);

                    if (row == last)
                        break;
                }
            }
        }
    };

    int selectionAnchorIndex = -1;
    const float selectionGutter = ImGui::GetFontSize() * 0.75f;
    if (selectableRows)
    {
        const auto flags = ImGuiMultiSelectFlags_ScopeWindow | ImGuiMultiSelectFlags_BoxSelect2d |
            ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_SelectOnClickRelease |
            ImGuiMultiSelectFlags_NoSelectAll;
        const auto* io = ImGui::BeginMultiSelect(flags, static_cast<int>(nextSelection.size()), static_cast<int>(rows.size()));
        applyRequests(*io);
        const auto anchor = findRow(io->RangeSrcItem);
        if (anchor != rows.end())
            selectionAnchorIndex = static_cast<int>(anchor - rows.begin());
        ImGui::Indent(selectionGutter);
    }

    int revealIndex = -1, renameIndex = -1;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        if (rows[i].Entity == reveal) revealIndex = i;
        if (rows[i].Entity == m_RenamingEntity) renameIndex = i;
    }

    // 2단계: 화면에 보이는 줄만 그린다. 스크롤 대상과 이름 변경 중인 줄은 화면 밖이어도 포함한다.
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(rows.size()), ImGui::GetFrameHeight());
    if (revealIndex >= 0) clipper.IncludeItemByIndex(revealIndex);
    if (renameIndex >= 0) clipper.IncludeItemByIndex(renameIndex);
    if (selectionAnchorIndex >= 0) clipper.IncludeItemByIndex(selectionAnchorIndex);
    while (clipper.Step())
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            drawRow(rows[i], i);
    if (selectableRows)
    {
        ImGui::Unindent(selectionGutter);
        applyRequests(*ImGui::EndMultiSelect());
        if (selectDragged != entt::null)
            nextSelection.assign(1, selectDragged);
        if (!std::ranges::equal(selected, nextSelection))
            action.Selection = std::move(nextSelection);
    }
    ImGui::PopStyleVar();

    if (isFiltering && matchingEntities.empty())
        ImGui::TextDisabled("%s", Text("Hierarchy.NoMatchingEntities", "No matching entities found.").c_str());
    else if (tree.Roots.empty())
        ImGui::TextDisabled("%s", Text("Hierarchy.Empty", "No entities.").c_str());
    const auto* payload = ImGui::GetDragDropPayload();
    const bool entityDrag = payload && payload->IsDataType(EntityPayload);
    const bool assetDrag = payload && payload->IsDataType("CONTENT_BROWSER_ITEM");

    if (!locked && (entityDrag || assetDrag))
    {
        const char* labelKey = assetDrag ? "Hierarchy.DropPrefabRoot" : "Hierarchy.RootDropHint";
        const std::string label = Text(labelKey) + "###HierarchyRootDrop";
        const float height = std::max(ImGui::GetFrameHeight() * 1.5f, ImGui::GetContentRegionAvail().y);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 0.35f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 0.6f));
        ImGui::Button(label.c_str(), ImVec2(-1.0f, height));
        ImGui::PopStyleColor(2);
        if (ImGui::BeginDragDropTarget())
        {
            acceptMove(entt::null, EntityDropPosition::Into);
            acceptPrefab(entt::null);
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::PopID();
    return action;
}
