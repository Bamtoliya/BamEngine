#pragma once

class Command
{
protected:

public:
	virtual ~Command() = default;
	virtual void Execute() = 0;
	virtual void Undo() = 0;

private:
};