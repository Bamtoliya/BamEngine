#include "AssetPathRelocation.h"
#include "Engine_Includes.h"
#include "Functions.h"
#include "AnimatorComponent.h"
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
        if (path.empty())
            return false;
        const auto raw = Engine::Utf8ToPath(path);
        const auto physical = Engine::PlatformPath::Normalize(Engine::PlatformPath::ResolveUri(raw));
        const auto mapped = Engine::PlatformPath::RemapFilePath(physical, m_Changes);
        if (physical == mapped)
            return false;
        auto text = Engine::WStrToStr(asUri || Engine::PlatformPath::IsVirtualUri(raw.wstring())
            ? Engine::PlatformPath::ToVirtualUri(mapped) : mapped.wstring());
        m_Updates.emplace_back([&path, text = std::move(text)]() mutable { path.swap(text); });
        return true;
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
            const auto& timeline = registry.get<Engine::ActionTimelineComponent>(entity);
            if (Affects(Engine::Utf8ToPath(timeline.timelinePath)) ||
                Affects(Engine::Utf8ToPath(timeline.loadedTimelinePath))) return false;
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
        for (auto& action : m_AfterCommit)
            action();
        m_Updates.clear();
        m_AfterCommit.clear();
    }
}
