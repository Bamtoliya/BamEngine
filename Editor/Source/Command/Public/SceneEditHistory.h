#pragma once

#include "CommandHistory.h"
#include <functional>
#include <span>
#include <entt/entity/entity.hpp>

namespace Engine { class Scene; }

namespace Editor
{
    // In-memory values, not a file format. Component copy operations come from the common ECS registry.
    class SceneEditSnapshot
    {
    public:
        static SceneEditSnapshot Capture(Engine::Scene& scene);
        static SceneEditSnapshot Capture(Engine::Scene& scene, std::span<const entt::entity> entities);
        explicit operator bool() const { return m_Data != nullptr; }
        bool Restore() const;
        bool MatchesStructure() const;
        bool SameScope(const SceneEditSnapshot& other) const;
        Engine::Scene* Scene() const;
    private:
        struct Data;
        std::shared_ptr<Data> m_Data;
        static SceneEditSnapshot CaptureImpl(Engine::Scene& scene, std::span<const entt::entity> entities, bool whole);
    };

    class SceneEditHistory final
    {
    public:
        void Record(SceneEditSnapshot before, SceneEditSnapshot after, std::string label, bool merge = false);
        void Flush();
        void Clear();
        void Synchronize(Engine::Scene* scene, bool editing);
        bool Undo();
        bool Redo();
        bool CanUndo() const { return m_Enabled && (m_Pending || m_Commands.CanUndo()); }
        bool CanRedo() const { return m_Enabled && !m_Pending && m_Commands.CanRedo(); }
        uint64 Revision() const { return m_Revision; }
        bool Enabled() const { return m_Enabled; }
        CommandHistory& Commands() { return m_Commands; }
        const CommandHistory& Commands() const { return m_Commands; }
        std::function<void()> BeforeReplay;
        std::function<void()> AfterReplay;
        void BeginFrame() { m_ShortcutsAllowed = false; }
        void AllowShortcuts(bool focused) { m_ShortcutsAllowed |= focused; }
        bool ShortcutsAllowed() const { return m_ShortcutsAllowed; }
        const std::string& Error() const { return m_Error; }
    private:
        friend class SceneEditScope;
        class Change;
        // Declare commands first so they outlive the pending operation.
        CommandHistory m_Commands;
        std::shared_ptr<Change> m_Pending;
        Engine::Scene* m_CurrentScene = nullptr;
        bool m_Enabled = false;
        bool m_ShortcutsAllowed = false;
        std::size_t m_ScopeDepth = 0;
        uint64 m_Revision = 0;
        std::string m_Error;
    };

    // A discrete document operation. Continuous widgets use Record(..., true) instead.
    class SceneEditScope final
    {
    public:
        SceneEditScope(SceneEditHistory& history, Engine::Scene& scene, std::string label);
        ~SceneEditScope();
        SceneEditScope(const SceneEditScope&) = delete;
        SceneEditScope& operator=(const SceneEditScope&) = delete;
    private:
        SceneEditHistory& m_History;
        Engine::Scene& m_Scene;
        SceneEditSnapshot m_Before;
        std::string m_Label;
        int m_Uncaught = 0;
        bool m_Entered = false;
        bool m_Outer = false;
    };
}
