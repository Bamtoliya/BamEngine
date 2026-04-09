#pragma once

#include "Script.h"


#pragma region Constructor&Destructor
EResult Script::Initialize(void* arg) 
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	return EResult::Success;
}

Script* Script::Create(void* arg)
{
	Script* script = new Script();
	if (script->Initialize(arg) != EResult::Success)
	{
		delete script;
		return nullptr;
	}
	return script;
}

void Script::Free()
{
}

#pragma endregion


