#pragma once

#include "Types.h"
#include <stdexcept>
#include "Delegate.h"
#include <algorithm>
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
        std::string UndoLabel() const { return m_Cursor ? m_Commands[m_Cursor - 1]->Label() : ""; }
        std::string RedoLabel() const { return m_Cursor < m_Commands.size() ? m_Commands[m_Cursor]->Label() : ""; }
        MulticastDelegate<>& OnHistoryChanged() { return m_OnHistoryChanged; }
    private:
        void Append(std::unique_ptr<EditorCommand> command);
        std::vector<std::unique_ptr<EditorCommand>> m_Commands;
        std::size_t m_Cursor = 0;
        std::size_t m_Limit;
        bool m_IsExecuting = false;
        MulticastDelegate<> m_OnHistoryChanged;
    };
}
