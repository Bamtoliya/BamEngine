#pragma once

#include "Editor_Includes.h"
#include "Types.h"
#include <stdexcept>
#include "Delegate.h"
#include <algorithm>
#include <functional>
#include <memory>

namespace Editor
{
    using Engine::uint32;
    using Engine::uint64;
    using Engine::MulticastDelegate;
    // A failed operation must leave the document unchanged.
    class EditorCommand
    {
    public:
        explicit EditorCommand(std::string label) : m_Label(std::move(label)) {}
        virtual ~EditorCommand() = default;
        virtual bool Execute() = 0;
        virtual bool Undo() = 0;
        const std::string& Label() const { return m_Label; }
    private:
        std::string m_Label;
    };

    class CallbackEditorCommand final : public EditorCommand
    {
    public:
        CallbackEditorCommand(std::string label, std::function<bool()> execute, std::function<bool()> undo)
            : EditorCommand(std::move(label)), m_Execute(std::move(execute)), m_Undo(std::move(undo)) {}
        bool Execute() override { return m_Execute(); }
        bool Undo() override { return m_Undo(); }
    private:
        std::function<bool()> m_Execute;
        std::function<bool()> m_Undo;
    };

    class CommandHistory final
    {
    public:
        explicit CommandHistory(std::size_t limit = 100) : m_Limit(std::max(std::size_t(1), limit)) {}
        CommandHistory(const CommandHistory&) = delete;
        CommandHistory& operator=(const CommandHistory&) = delete;

        bool Execute(std::unique_ptr<EditorCommand> command);
        bool Record(std::unique_ptr<EditorCommand> command);
        bool Undo();
        bool Redo();
        void Clear();

        bool CanUndo() const { return !m_IsExecuting && m_Cursor != 0; }
        bool CanRedo() const { return !m_IsExecuting && m_Cursor < m_Commands.size(); }
        uint32 GetUndoCount() const { return static_cast<uint32>(m_Cursor); }
        uint32 GetRedoCount() const { return static_cast<uint32>(m_Commands.size() - m_Cursor); }
        uint64 GetCurrentStateId() const { return m_CurrentStateId; }

        struct CommandItemView
        {
            std::string Label;
            uint64 StateId = 0;
        };

        std::vector<CommandItemView> GetHistoryView() const
        {
            std::vector<CommandItemView> result;
            result.reserve(m_Commands.size());
            for (const auto& entry : m_Commands)
            {
                result.push_back({ entry.Command ? entry.Command->Label() : "Unnamed", entry.AfterStateId });
            }
            return result;
        }

        std::size_t GetCursor() const { return m_Cursor; }

        bool UndoTo(std::size_t targetCursor)
        {
            if (m_IsExecuting) return false;
            while (m_Cursor > targetCursor)
            {
                if (!Undo()) return false;
            }
            while (m_Cursor < targetCursor)
            {
                if (!Redo()) return false;
            }
            return true;
        }

        std::string UndoLabel() const
        {
            return m_Cursor ? m_Commands[m_Cursor - 1].Command->Label() : "";
        }

        std::string RedoLabel() const
        {
            return m_Cursor < m_Commands.size() ? m_Commands[m_Cursor].Command->Label() : "";
        }

        MulticastDelegate<>& OnHistoryChanged() { return m_OnHistoryChanged; }

    private:
        struct Entry
        {
            std::unique_ptr<EditorCommand> Command;
            uint64 BeforeStateId = 0;
            uint64 AfterStateId = 0;
        };

        void Append(std::unique_ptr<EditorCommand> command);

        std::vector<Entry> m_Commands;
        std::size_t m_Cursor = 0;
        std::size_t m_Limit;
        uint64 m_CurrentStateId = 0;
        uint64 m_NextStateId = 1;
        bool m_IsExecuting = false;
        MulticastDelegate<> m_OnHistoryChanged;
    };
}
