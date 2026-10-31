#include "EntityClipboard.h"
#include "Scene.h"
#include "Entity.h"
#include "CoreComponents.h"
#include "SelectionManager.h"
#include "Application.h"
#include "SceneEditHistory.h"
#include "NotificationManager.h"
#include "PIEManager.h"
#include "LocalizationManager.h"
#include <archive/JsonArchive.h>

std::vector<CopiedEntityRecord> EntityClipboard::s_Clipboard;
void* EntityClipboard::s_SourceScene = nullptr;

bool EntityClipboard::HasData()
{
    return !s_Clipboard.empty();
}

void EntityClipboard::Clear()
{
    s_Clipboard.clear();
    s_SourceScene = nullptr;
}

void EntityClipboard::OnSceneChanged()
{
    Clear();
}

bool EntityClipboard::CopySelected(Engine::Scene* scene)
{
    if (!scene) return false;

    auto& selection = SelectionManager::Get();
    std::vector<Engine::Entity*> selected;
    std::unordered_set<entt::entity> selectedHandles;
    for (auto* entity : selection.GetSelectedEntities())
    {
        if (entity && entity->GetScene() == scene)
        {
            selected.push_back(entity);
            selectedHandles.insert(entity->GetEntityHandle());
        }
    }
    if (selected.empty()) return false;

    std::vector<Engine::Entity*> roots;
    auto& registry = scene->GetRegistry();
    for (auto* entity : selected)
    {
        entt::entity current = entity->GetEntityHandle();
        bool hasSelectedAncestor = false;
        while (true)
        {
            const auto* hier = registry.try_get<Engine::HierarchyComponent>(current);
            if (!hier || hier->parent == entt::null) break;
            if (selectedHandles.contains(hier->parent))
            {
                hasSelectedAncestor = true;
                break;
            }
            current = hier->parent;
        }
        if (!hasSelectedAncestor) roots.push_back(entity);
    }
    if (roots.empty()) return false;

    s_Clipboard.clear();
    for (auto* root : roots)
    {
        uint64 parentId = 0;
        const auto* hier = registry.try_get<Engine::HierarchyComponent>(root->GetEntityHandle());
        if (hier && hier->parent != entt::null)
        {
            const auto* idComp = registry.try_get<Engine::IDComponent>(hier->parent);
            if (idComp) parentId = idComp->id;
        }

        archive::JsonArchive ar;
        if (scene->WritePrefabArchive(ar, *root))
        {
            std::string json;
            if (ar.ToJson(json))
            {
                s_Clipboard.push_back({ parentId, std::move(json) });
            }
        }
    }
    s_SourceScene = scene;

    if (!s_Clipboard.empty())
    {
        NotificationManager::Get().AddNotification(LOCAL("UI_EDIT"), LOCAL_FMT("Notification.EntitiesCopied", s_Clipboard.size()), ELogLevel::Info, 2.0f);
        return true;
    }
    return false;
}

bool EntityClipboard::Paste(Engine::Scene* scene, Engine::Entity* targetParent)
{
    if (!scene || s_Clipboard.empty()) return false;
    if (!PIEManager::Get().IsEditing()) return false;

    auto& history = Application::Get().GetEditHistory();
    auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene) : SceneEditSnapshot{};

    std::vector<Engine::Entity*> spawnedRoots;
    for (const auto& record : s_Clipboard)
    {
        archive::JsonArchive ar;
        if (!ar.Parse(record.SerializedJson)) continue;

        Engine::Entity* effectiveParent = targetParent;
        if (!effectiveParent && record.OriginalParentId != 0)
        {
            effectiveParent = scene->FindEntityById(record.OriginalParentId);
        }

        Engine::Entity* spawned = nullptr;
        if (scene->InstantiatePrefabArchive(ar, spawned, effectiveParent) && spawned)
        {
            spawnedRoots.push_back(spawned);
        }
    }

    if (spawnedRoots.empty())
    {
        NotificationManager::Get().AddNotification(LOCAL("UI_EDIT"), LOCAL("Notification.EntitiesPasteFailed"), ELogLevel::Error, 3.0f);
        return false;
    }

    if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), "UI_HISTORY_PASTE");

    SelectionManager::Get().ClearSelection();
    for (auto* entity : spawnedRoots)
    {
        SelectionManager::Get().AddToSelection(entity);
    }

    NotificationManager::Get().AddNotification(LOCAL("UI_EDIT"), LOCAL_FMT("Notification.EntitiesPasted", spawnedRoots.size()), ELogLevel::Info, 2.0f);
    return true;
}