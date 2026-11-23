#pragma once

#include "PlatformPath.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <utility>
#include <entt/entity/fwd.hpp>

namespace Engine { struct TimelineNotifyTrack; }

namespace Editor
{
    // Why the editor refused to prepare an asset move. Subject names the blocking editor or document for the dialog.
    enum class EAssetMoveBlockReason : std::uint8_t
    {
        None,
        NotEditing,
        DocumentBusy,
        SceneLoading,
        PrefabMode,
        UnsavedResourceEditor,
        UnsavedDocument,
        DocumentRejected,
        EditorRetainsPath
    };

    struct AssetMoveBlock
    {
        EAssetMoveBlockReason Reason = EAssetMoveBlockReason::None;
        std::string Subject;

        explicit operator bool() const { return Reason != EAssetMoveBlockReason::None; }
    };

    // Prepared on the UI thread, applied synchronously only after the disk and ResourceManager commit.
    // Owners captured by the updates must remain alive until Apply returns; discard the plan on any prepare failure.
    class AssetPathRelocation
    {
    public:
        AssetPathRelocation() = default;
        AssetPathRelocation(std::vector<Engine::FilePathChange> changes, std::vector<std::filesystem::path> documents)
            : m_Changes(std::move(changes)), m_Documents(std::move(documents)) {}

        const std::vector<Engine::FilePathChange>& Changes() const { return m_Changes; }
        bool Affects(const std::filesystem::path& path) const;
        bool Rewrites(const std::filesystem::path& path) const;
        bool Add(std::filesystem::path& path);
        bool AddUtf8(std::string& path, bool asUri = false);
        bool AddTimelineNotifyPaths(std::vector<Engine::TimelineNotifyTrack>& tracks);
        bool PrepareScenePaths(entt::registry& registry);
        void AfterCommit(std::function<void()> action) { m_AfterCommit.push_back(std::move(action)); }
        void Apply();

    private:
        void AddWidePath(std::wstring& path, const std::filesystem::path& physical, bool asUri);
        std::vector<Engine::FilePathChange> m_Changes;
        std::vector<std::filesystem::path> m_Documents;
        std::vector<std::function<void()>> m_Updates;
        std::vector<Engine::Utf8FilePathUpdate> m_Utf8Updates;
        std::vector<std::function<void()>> m_AfterCommit;
    };
}
