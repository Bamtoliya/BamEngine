#include "AssetPathRelocation.h"
#include "Engine_Includes.h"
#include "Functions.h"
#include "AnimatorComponent.h"
#include "ActionTimelineAsset.h"
#include "ActionTimelineComponent.h"
#include "PrefabInstanceComponent.h"
#include "GraphBindingComponent.h"
#include <algorithm>
#include <entt/entity/registry.hpp>

namespace Editor
{
    bool AssetPathRelocation::Rewrites(const std::filesystem::path& path) const
    {
        if (path.empty())
            return false;
        const auto physical = Engine::PlatformPath::ResolveUri(path);
        return std::ranges::any_of(m_Documents, [&physical](const auto& document)
        {
            return Engine::PlatformPath::IsSamePath(physical, document);
        });
    }

    bool AssetPathRelocation::Affects(const std::filesystem::path& path) const
    {
        if (path.empty())
            return false;
        const auto physical = Engine::PlatformPath::Normalize(Engine::PlatformPath::ResolveUri(path));
        return Engine::PlatformPath::RemapFilePath(physical, m_Changes) != physical || Rewrites(path);
    }

    bool AssetPathRelocation::Add(std::filesystem::path& path)
    {
        if (path.empty())
            return false;
        const auto physical = Engine::PlatformPath::Normalize(Engine::PlatformPath::ResolveUri(path));
        auto mapped = Engine::PlatformPath::RemapFilePath(physical, m_Changes);
        if (physical == mapped)
            return false;
        if (Engine::PlatformPath::IsVirtualUri(path.wstring()))
            mapped = Engine::PlatformPath::ToVirtualUri(mapped);
        m_Updates.emplace_back([&path, mapped = std::move(mapped)]() mutable { path.swap(mapped); });
        return true;
    }

    bool AssetPathRelocation::AddUtf8(std::string& path, bool asUri)
    {
        return Engine::PlatformPath::PrepareUtf8FilePathUpdate(path, m_Changes, m_Utf8Updates, asUri);
    }

    bool AssetPathRelocation::AddTimelineNotifyPaths(std::vector<Engine::TimelineNotifyTrack>& tracks)
    {
        const auto previousSize = m_Utf8Updates.size();
        Engine::ActionTimelineAsset::PrepareNotifyPathRelocation(tracks, m_Changes, m_Utf8Updates);
        return m_Utf8Updates.size() != previousSize;
    }

    bool AssetPathRelocation::PrepareScenePaths(entt::registry& registry)
    {
        // These strings are outside the saved-reference writer's supported fields.
        for (const auto entity : std::as_const(registry).view<Engine::PrefabInstanceComponent>())
            if (Affects(registry.get<Engine::PrefabInstanceComponent>(entity).prefabAssetPath)) return false;
        for (const auto entity : std::as_const(registry).view<Engine::GraphBindingComponent>())
            if (Affects(registry.get<Engine::GraphBindingComponent>(entity).graphFile)) return false;
        for (const auto entity : std::as_const(registry).view<Engine::ActionTimelineComponent>())
        {
            auto& timeline = registry.get<Engine::ActionTimelineComponent>(entity);
            AddUtf8(timeline.timelinePath, true);
            AddUtf8(timeline.loadedTimelinePath, true);
        }
        for (const auto entity : std::as_const(registry).view<Engine::AnimatorComponent>())
        {
            auto& animator = registry.get<Engine::AnimatorComponent>(entity);
            AddUtf8(animator.controllerPath, true);
            AddUtf8(animator.loadedControllerPath, true);
        }
        return true;
    }

    void AssetPathRelocation::Apply()
    {
        for (auto& update : m_Updates)
            update();
        for (auto& update : m_Utf8Updates)
            update.Apply();
        for (auto& action : m_AfterCommit)
            action();
        m_Updates.clear();
        m_Utf8Updates.clear();
        m_AfterCommit.clear();
    }
}
