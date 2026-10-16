#include "CommandHistory.h"

namespace
{
    struct ProcessingScope
    {
        bool& Flag;
        explicit ProcessingScope(bool& flag) : Flag(flag) { Flag = true; }
        ~ProcessingScope() { Flag = false; }
    };
}

bool Editor::CommandHistory::Execute(std::unique_ptr<EditorCommand> command)
{
    if (!command || m_IsExecuting) return false;
    ProcessingScope scope(m_IsExecuting);
    if (!command->Execute()) return false;
    Append(std::move(command));
    m_OnHistoryChanged.Broadcast();
    return true;
}

bool Editor::CommandHistory::Record(std::unique_ptr<EditorCommand> command)
{
    if (!command || m_IsExecuting) return false;
    ProcessingScope scope(m_IsExecuting);
    Append(std::move(command));
    m_OnHistoryChanged.Broadcast();
    return true;
}

bool Editor::CommandHistory::Undo()
{
    if (!CanUndo()) return false;
    ProcessingScope scope(m_IsExecuting);
    if (!m_Commands[m_Cursor - 1]->Undo()) return false;
    --m_Cursor;
    m_OnHistoryChanged.Broadcast();
    return true;
}

bool Editor::CommandHistory::Redo()
{
    if (!CanRedo()) return false;
    ProcessingScope scope(m_IsExecuting);
    if (!m_Commands[m_Cursor]->Execute()) return false;
    ++m_Cursor;
    m_OnHistoryChanged.Broadcast();
    return true;
}

void Editor::CommandHistory::Clear()
{
    if (m_IsExecuting) return;
    ProcessingScope scope(m_IsExecuting);
    m_Commands.clear();
    m_Cursor = 0;
    m_OnHistoryChanged.Broadcast();
}

void Editor::CommandHistory::Append(std::unique_ptr<EditorCommand> command)
{
    m_Commands.erase(m_Commands.begin() + m_Cursor, m_Commands.end());
    m_Commands.push_back(std::move(command));
    if (m_Commands.size() > m_Limit) m_Commands.erase(m_Commands.begin());
    m_Cursor = m_Commands.size();
}