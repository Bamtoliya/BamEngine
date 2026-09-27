#include "CommandHistory.h"

CommandHistory::CommandHistory()
{
    m_Commands.reserve(MAX_COMMAND_COUNT+1);
}

bool CommandHistory::Execute(Command* command)
{
	if (!command || m_IsExecuting)
		return false;

    HistoryProcessingScope scope(m_IsExecuting);

	command->Execute();
	Append(command);

	m_OnHistoryChanged.Broadcast();
    return true;
}

bool CommandHistory::Record(Command* command)
{
    if (!command || m_IsExecuting)
        return false;

    HistoryProcessingScope scope(m_IsExecuting);
    Append(command);
    m_OnHistoryChanged.Broadcast();
    return true;
}

bool CommandHistory::Undo()
{
	if (m_IsExecuting || !CanUndo())
		return false;

    HistoryProcessingScope scope(m_IsExecuting);

	m_Commands[m_Cursor - 1]->Undo();
    --m_Cursor;

	m_OnHistoryChanged.Broadcast();
    return true;
}

bool CommandHistory::Redo()
{
    if (m_IsExecuting || !CanRedo())
        return false;

    HistoryProcessingScope scope(m_IsExecuting);

    m_Commands[m_Cursor]->Execute();
    ++m_Cursor;

    m_OnHistoryChanged.Broadcast();
    return true;
}

void CommandHistory::Clear()
{
    if (m_IsExecuting || m_Commands.empty())
        return;

    HistoryProcessingScope scope(m_IsExecuting);

    m_Commands.clear();
    m_Cursor = 0;

    m_OnHistoryChanged.Broadcast();
}

bool CommandHistory::CanUndo() const
{
    return m_Cursor > 0;
}

bool CommandHistory::CanRedo() const
{
    return m_Cursor < m_Commands.size();
}

uint32 CommandHistory::GetUndoCount() const
{
    return m_Cursor;
}

uint32 CommandHistory::GetRedoCount() const
{
    return uint32(m_Commands.size() - m_Cursor);
}

void CommandHistory::Append(Command* command)
{
    while (m_Commands.size() > m_Cursor)
    {
        m_Commands.pop_back();
    }

    m_Commands.push_back(std::move(command));

    if (m_Commands.size() > MAX_COMMAND_COUNT)
    {
        m_Commands.erase(m_Commands.begin());
    }

    m_Cursor = m_Commands.size();
}
