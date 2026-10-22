#include "SceneEditHistory.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include "TransformSystem.h"
#include "Components/UI/UIInteractionStateComponent.h"
#include "ReflectionEditState.h"
#include <unordered_set>
#include <exception>

struct Editor::SceneEditSnapshot::Data
{
    struct Row
    {
        struct Reference { entt::id_type Storage; std::string Property; uint64 Target; };
        uint64 Id = 0;
        entt::entity Value = entt::null;
        std::vector<entt::id_type> Components;
        bool HasHierarchy = false;
        uint64 Parent = 0;
        std::vector<uint64> Children;
        std::vector<Reference> References;
    };
    Engine::Scene* Owner = nullptr;
    bool Whole = false;
    std::wstring Name;
    entt::registry Values;
    std::vector<Row> Rows;
    ~Data() { if (Owner) Owner->Release(); }
};

Editor::SceneEditSnapshot Editor::SceneEditSnapshot::Capture(Engine::Scene& scene)
{
    std::vector<entt::entity> entities;
    auto& world = scene.GetRegistry();
    for (auto [entity] : world.storage<entt::entity>().each())
    {
        if (!scene.FindEntity(entity)) return {};
        entities.push_back(entity);
    }
    return CaptureImpl(scene, entities, true);
}

Editor::SceneEditSnapshot Editor::SceneEditSnapshot::Capture(Engine::Scene& scene,
    std::span<const entt::entity> entities)
{
    return CaptureImpl(scene, entities, false);
}

Editor::SceneEditSnapshot Editor::SceneEditSnapshot::CaptureImpl(Engine::Scene& scene,
    std::span<const entt::entity> entities, bool whole)
{
    if (!Engine::ReflectionRegistry::IsCreated()) return {};
    auto data = std::make_shared<Data>();
    scene.AddRef();
    data->Owner = &scene;
    data->Whole = whole;
    data->Name = scene.GetName();
    const auto& world = scene.GetRegistry();
    const auto& registry = Engine::ReflectionRegistry::Get();
    const auto idOf = [&](entt::entity entity) -> uint64
    {
        if (entity == entt::null) return 0;
        const auto* id = world.valid(entity) ? world.try_get<Engine::IDComponent>(entity) : nullptr;
        return id && scene.FindEntity(entity) ? id->id : 0;
    };
    std::unordered_set<uint64> ids;
    for (const auto entity : entities)
    {
        const auto id = idOf(entity);
        if (!id || !ids.insert(id).second) return {};
        Data::Row row;
        row.Id = id;
        row.Value = data->Values.create();
        for (auto&& [storageId, storage] : world.storage())
        {
            if (!storage.contains(entity)) continue;
            const auto* entry = registry.FindComponent(storageId);
            // Full document edits must not silently discard an unregistered component.
            if (!entry) { if (whole) return {}; else continue; }
            if (entry->Type == std::type_index(typeid(Engine::UIInteractionStateComponent))) continue;
            if (!entry->MatchesStorage(world) || !entry->Copy ||
                !entry->Copy(world, entity, data->Values, row.Value)) return {};
            row.Components.push_back(storageId);
            if (entry->ReflectionType)
                for (const auto& property : entry->ReflectionType->Properties)
                {
                    if (property.GetValueCppType() != std::type_index(typeid(entt::entity))) continue;
                    const auto* reference = property.ReadValue(entry->View(world, entity)).Get<entt::entity>();
                    if (!reference || !property.CanWrite()) return {};
                    const auto target = idOf(*reference);
                    if (*reference != entt::null && !target) return {};
                    row.References.push_back({ storageId, property.Name, target });
                }
        }
        if (const auto* link = world.try_get<Engine::HierarchyComponent>(entity))
        {
            row.HasHierarchy = true;
            row.Parent = idOf(link->parent);
            if (link->parent != entt::null && !row.Parent) return {};
            for (const auto child : link->children)
            {
                const auto childId = idOf(child);
                if (!childId) return {};
                row.Children.push_back(childId);
            }
        }
        data->Rows.push_back(std::move(row));
    }
    if (whole)
    {
        std::unordered_map<uint64, const Data::Row*> rows;
        for (const auto& row : data->Rows) rows.emplace(row.Id, &row);
        for (const auto& row : data->Rows)
        {
            if (row.Parent && (!rows.contains(row.Parent) ||
                std::count(rows.at(row.Parent)->Children.begin(), rows.at(row.Parent)->Children.end(), row.Id) != 1))
                return {};
            std::unordered_set<uint64> children, ancestors;
            for (const auto child : row.Children)
                if (!rows.contains(child) || rows.at(child)->Parent != row.Id || !children.insert(child).second)
                    return {};
            for (auto current = row.Id; current; current = rows.at(current)->Parent)
                if (!rows.contains(current) || !ancestors.insert(current).second || ancestors.size() > 256) return {};
        }
    }
    SceneEditSnapshot result;
    result.m_Data = std::move(data);
    return result;
}

Engine::Scene* Editor::SceneEditSnapshot::Scene() const { return m_Data ? m_Data->Owner : nullptr; }

bool Editor::SceneEditSnapshot::MatchesStructure() const
{
    if (!m_Data || !Engine::ReflectionRegistry::IsCreated()) return false;
    auto& scene = *m_Data->Owner;
    const auto& world = scene.GetRegistry();
    const auto& registry = Engine::ReflectionRegistry::Get();
    const auto idOf = [&](entt::entity entity) -> uint64
    {
        const auto* id = world.valid(entity) ? world.try_get<Engine::IDComponent>(entity) : nullptr;
        return id && scene.FindEntity(entity) ? id->id : 0;
    };
    if (m_Data->Whole)
    {
        std::unordered_set<uint64> ids;
        if (const auto* storage = world.storage<entt::entity>())
            for (auto [entity] : storage->each())
            {
                const auto id = idOf(entity);
                if (!id || !ids.insert(id).second) return false;
            }
        if (ids.size() != m_Data->Rows.size() || scene.GetName() != m_Data->Name) return false;
    }
    for (const auto& row : m_Data->Rows)
    {
        const auto* wrapper = scene.FindEntityById(row.Id);
        if (!wrapper) return false;
        const auto entity = wrapper->GetEntityHandle();
        for (const auto& entry : registry.GetComponentEntries())
        {
            if (!entry.MatchesStorage(world)) return false;
            const bool expected = std::ranges::find(row.Components, entry.StorageId) != row.Components.end();
            if (entry.Has(world, entity) != expected) return false;
        }
        if (row.HasHierarchy)
        {
            const auto& link = world.get<Engine::HierarchyComponent>(entity);
            if (link.parent != entt::null && !idOf(link.parent)) return false;
            if (idOf(link.parent) != row.Parent || link.children.size() != row.Children.size()) return false;
            for (std::size_t i = 0; i < row.Children.size(); ++i)
                if (idOf(link.children[i]) != row.Children[i]) return false;
        }
        for (const auto& reference : row.References)
        {
            const auto* entry = registry.FindComponent(reference.Storage);
            const auto* property = entry && entry->ReflectionType ?
                entry->ReflectionType->FindProperty(reference.Property) : nullptr;
            const auto* target = property ? property->ReadValue(entry->View(world, entity)).Get<entt::entity>() : nullptr;
            if (!target || (*target != entt::null && !idOf(*target)) || idOf(*target) != reference.Target) return false;
        }
    }
    return true;
}

bool Editor::SceneEditSnapshot::SameScope(const SceneEditSnapshot& other) const
{
    if (!m_Data || !other.m_Data || Scene() != other.Scene() || m_Data->Whole != other.m_Data->Whole ||
        m_Data->Rows.size() != other.m_Data->Rows.size()) return false;
    for (std::size_t i = 0; i < m_Data->Rows.size(); ++i)
        if (m_Data->Rows[i].Id != other.m_Data->Rows[i].Id) return false;
    return true;
}

bool Editor::SceneEditSnapshot::Restore() const
{
    if (!m_Data || !Engine::ReflectionRegistry::IsCreated()) return false;
    auto& scene = *m_Data->Owner;
    auto& world = scene.GetRegistry();
    const auto& registry = Engine::ReflectionRegistry::Get();
    std::unordered_map<uint64, entt::entity> live;
    for (auto [entity] : world.storage<entt::entity>().each())
    {
        const auto* id = world.try_get<Engine::IDComponent>(entity);
        if (!scene.FindEntity(entity) || !id || !id->id || !live.emplace(id->id, entity).second) return false;
        if (m_Data->Whole)
            for (auto&& [storageId, storage] : world.storage())
                if (storage.contains(entity) && !registry.FindComponent(storageId)) return false;
    }
    struct Links { uint64 Parent = 0; std::vector<uint64> Children; };
    std::unordered_map<uint64, Links> graph;
    if (!m_Data->Whole)
        for (const auto [id, entity] : live)
        {
            auto& link = graph[id];
            if (const auto* source = world.try_get<Engine::HierarchyComponent>(entity))
            {
                if (source->parent != entt::null)
                {
                    const auto* parent = world.valid(source->parent) ?
                        world.try_get<Engine::IDComponent>(source->parent) : nullptr;
                    if (!parent) return false;
                    link.Parent = parent->id;
                }
                for (const auto child : source->children)
                {
                    const auto* value = world.valid(child) ? world.try_get<Engine::IDComponent>(child) : nullptr;
                    if (!value) return false;
                    link.Children.push_back(value->id);
                }
            }
        }
    for (const auto& row : m_Data->Rows)
    {
        if (!m_Data->Whole && !live.contains(row.Id)) return false;
        graph[row.Id] = Links{ row.Parent, row.Children };
        for (const auto storageId : row.Components)
        {
            const auto* entry = registry.FindComponent(storageId);
            if (!entry || !entry->Copy || !entry->Remove || !entry->MatchesStorage(world) ||
                !entry->MatchesStorage(m_Data->Values)) return false;
        }
    }
    // Validate the complete resulting graph before any mutation or recursive transform update.
    for (const auto& [id, link] : graph)
    {
        if (link.Parent && (!graph.contains(link.Parent) ||
            std::count(graph.at(link.Parent).Children.begin(), graph.at(link.Parent).Children.end(), id) != 1))
            return false;
        std::unordered_set<uint64> children, ancestors;
        for (const auto child : link.Children)
            if (!graph.contains(child) || graph.at(child).Parent != id || !children.insert(child).second) return false;
        for (auto current = id; current; current = graph.at(current).Parent)
            if (!graph.contains(current) || !ancestors.insert(current).second || ancestors.size() > 256) return false;
    }
    for (const auto& entry : registry.GetComponentEntries())
        if (!entry.MatchesStorage(world)) return false;
    for (const auto& row : m_Data->Rows)
        for (const auto& reference : row.References)
        {
            const auto* entry = registry.FindComponent(reference.Storage);
            const auto* property = entry && entry->ReflectionType ?
                entry->ReflectionType->FindProperty(reference.Property) : nullptr;
            if (!property || !property->CanWrite() || (reference.Target && !graph.contains(reference.Target)))
                return false;
        }

    // Evaluate the resulting spatial graph before copying values into the live scene.
    entt::registry spatialCheck;
    std::unordered_map<uint64, entt::entity> spatialHandles;
    std::unordered_map<uint64, const Data::Row*> edits;
    for (const auto& row : m_Data->Rows) edits.emplace(row.Id, &row);
    for (const auto& [id, link] : graph) spatialHandles.emplace(id, spatialCheck.create());
    for (const auto& [id, link] : graph)
    {
        const auto entity = spatialHandles.at(id);
        const Engine::TransformComponent* transform = nullptr;
        if (const auto found = edits.find(id); found != edits.end())
            transform = m_Data->Values.try_get<Engine::TransformComponent>(found->second->Value);
        else if (live.contains(id)) transform = world.try_get<Engine::TransformComponent>(live.at(id));
        if (transform) spatialCheck.emplace<Engine::TransformComponent>(entity, *transform);
        auto& relation = spatialCheck.emplace<Engine::HierarchyComponent>(entity);
        relation.parent = link.Parent ? spatialHandles.at(link.Parent) : entt::null;
        for (const auto child : link.Children) relation.children.push_back(spatialHandles.at(child));
    }
    if (!Engine::TransformSystem::UpdateHierarchy(spatialCheck)) return false;

    if (m_Data->Whole)
    {
        // All hierarchy links are restored below using IDs, including recreated entities with new handles.
        for (auto [entity, link] : world.view<Engine::HierarchyComponent>().each()) link = {};
        for (auto it = live.begin(); it != live.end();)
        {
            if (graph.contains(it->first)) { ++it; continue; }
            auto* wrapper = scene.FindEntity(it->second);
            if (!wrapper || !scene.DestroyEntity(*wrapper)) return false;
            it = live.erase(it);
        }
        for (const auto& row : m_Data->Rows)
            if (!live.contains(row.Id))
            {
                auto& created = scene.CreateEntity();
                const auto handle = created.GetEntityHandle();
                world.get<Engine::IDComponent>(handle).id = row.Id;
                live.emplace(row.Id, handle);
            }
    }
    for (const auto& row : m_Data->Rows)
    {
        const auto entity = live.at(row.Id);
        for (const auto& entry : registry.GetComponentEntries())
            if (entry.Has(world, entity) &&
                std::ranges::find(row.Components, entry.StorageId) == row.Components.end()) entry.Remove(world, entity);
        for (const auto storageId : row.Components)
        {
            const auto* entry = registry.FindComponent(storageId);
            if (!entry->Copy(m_Data->Values, row.Value, world, entity)) return false;
        }
        if (row.HasHierarchy)
        {
            auto& link = world.get<Engine::HierarchyComponent>(entity);
            link.parent = row.Parent ? live.at(row.Parent) : entt::null;
            link.children.clear();
            for (const auto id : row.Children) link.children.push_back(live.at(id));
        }
        for (const auto& reference : row.References)
        {
            const auto* entry = registry.FindComponent(reference.Storage);
            const auto* property = entry->ReflectionType->FindProperty(reference.Property);
            const auto target = reference.Target ? live.at(reference.Target) : entt::entity(entt::null);
            if (!property->WriteValue(entry->Edit(world, entity), reflection::ValueView::From(target))) return false;
        }
        if (auto* value = world.try_get<Engine::CameraComponent>(entity)) value->InvalidateCache();
        if (auto* value = world.try_get<Engine::LightComponent>(entity)) value->InvalidateCache();
        if (auto* value = world.try_get<Engine::ColliderComponent>(entity)) value->MarkDirty();
        if (auto* value = world.try_get<Engine::Collider2DComponent>(entity)) value->MarkDirty();
    }
    if (auto* state = world.ctx().find<ReflectionSceneEditState>())
    {
        std::erase_if(state->Entities, [&](const auto& value) { return !world.valid(value.first); });
        for (auto& [entity, settings] : state->Entities)
            for (auto& [type, component] : settings.Components) component.TextBuffers.clear();
    }
    if (m_Data->Whole) scene.SetName(m_Data->Name);
    if (!Engine::TransformSystem::UpdateHierarchy(world)) return false;
    scene.UpdateHierarchyFlags();
    return true;
}

class Editor::SceneEditHistory::Change final : public EditorCommand
{
public:
    Change(SceneEditSnapshot before, SceneEditSnapshot after, std::string label)
        : EditorCommand(std::move(label)), Before(std::move(before)), After(std::move(after)) {}
    bool Execute() override { return Before.MatchesStructure() && After.Restore(); }
    bool Undo() override { return After.MatchesStructure() && Before.Restore(); }
    SceneEditSnapshot Before, After;
};

void Editor::SceneEditHistory::Record(SceneEditSnapshot before, SceneEditSnapshot after, std::string label, bool merge)
{
    if (!m_Enabled) return;
    if (!before || !after || before.Scene() != after.Scene())
    {
        Clear();
        m_Error = "UI_HISTORY_CAPTURE_FAILED";
        return;
    }
    if (merge && m_Pending && m_Pending->Label() == label && m_Pending->After.SameScope(before))
        m_Pending->After = std::move(after);
    else
    {
        Flush();
        m_Pending = std::make_shared<Change>(std::move(before), std::move(after), std::move(label));
    }
    if (!merge) Flush();
    m_Error.clear();
}

void Editor::SceneEditHistory::Flush()
{
    if (!m_Pending) return;
    m_Commands.Record(std::make_unique<Change>(*m_Pending));
    m_Pending.reset();
}

void Editor::SceneEditHistory::Clear()
{
    m_Pending.reset();
    m_Commands.Clear();
    ++m_Revision;
    m_Error.clear();
}

void Editor::SceneEditHistory::Synchronize(Engine::Scene* scene, bool editing)
{
    if (scene != m_CurrentScene || editing != m_Enabled)
    {
        Clear();
        m_CurrentScene = scene;
        m_Enabled = editing;
    }
}

bool Editor::SceneEditHistory::Undo()
{
    if (!CanUndo()) return false;
    Flush();
    if (BeforeReplay) BeforeReplay();
    ++m_Revision;
    const bool result = m_Commands.Undo();
    if (AfterReplay) AfterReplay();
    m_Error = result ? "" : "UI_HISTORY_REPLAY_FAILED";
    return result;
}

bool Editor::SceneEditHistory::Redo()
{
    if (!CanRedo()) return false;
    if (BeforeReplay) BeforeReplay();
    ++m_Revision;
    const bool result = m_Commands.Redo();
    if (AfterReplay) AfterReplay();
    m_Error = result ? "" : "UI_HISTORY_REPLAY_FAILED";
    return result;
}

Editor::SceneEditScope::SceneEditScope(SceneEditHistory& history, Engine::Scene& scene, std::string label)
    : m_History(history), m_Scene(scene), m_Label(std::move(label)), m_Uncaught(std::uncaught_exceptions())
{
    if (!history.Enabled()) return;
    m_Outer = history.m_ScopeDepth++ == 0;
    m_Entered = true;
    try { if (m_Outer) m_Before = SceneEditSnapshot::Capture(scene); }
    catch (...)
    {
        --history.m_ScopeDepth;
        m_Entered = false;
        throw;
    }
}

Editor::SceneEditScope::~SceneEditScope()
{
    if (!m_Entered) return;
    --m_History.m_ScopeDepth;
    if (!m_Outer) return;
    if (!m_History.Enabled()) return;
    try
    {
        if (std::uncaught_exceptions() != m_Uncaught) m_History.Clear();
        else m_History.Record(std::move(m_Before), SceneEditSnapshot::Capture(m_Scene), std::move(m_Label));
    }
    catch (...) { m_History.Clear(); }
}
