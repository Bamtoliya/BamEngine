#pragma once

#include "Editor_Includes.h"
#include "Command.h"

#define MAX_COMMAND_COUNT 100

BEGIN(Editor)

class HistoryProcessingScope final
{
public:
	explicit HistoryProcessingScope(bool& flag)
		: m_Flag(flag)
	{
		m_Flag = true;
	}

	~HistoryProcessingScope()
	{
		m_Flag = false;
	}

	HistoryProcessingScope(const HistoryProcessingScope&) = delete;
	HistoryProcessingScope& operator=(
		const HistoryProcessingScope&) = delete;

private:
	bool& m_Flag;
};

class CommandHistory final
{
public:
	CommandHistory();
	CommandHistory(CommandHistory&) = delete;
	CommandHistory& operator=(CommandHistory&) = delete;

	bool Execute(Command* command);
	bool Record(Command* command);

	bool Undo();
	bool Redo();

	void Clear();

	bool CanUndo() const;
	bool CanRedo() const;

	uint32 GetUndoCount() const;
	uint32 GetRedoCount() const;

	MulticastDelegate<>& OnHistoryChanged() { return m_OnHistoryChanged; }

private:
	void Append(Command* command);

private:
	vector<Command*> m_Commands;

	uint32 m_Cursor = 0;
	bool m_IsExecuting = false;
	MulticastDelegate<> m_OnHistoryChanged;
};
END