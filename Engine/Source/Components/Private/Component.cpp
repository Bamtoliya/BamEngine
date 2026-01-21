#pragma once
#include "Component.h"

#pragma region Constructor&Destructor
EResult Component::Initialize(void* arg)
{
	DESC* desc = static_cast<DESC*>(arg);
	m_Active = desc->Active;
	m_Tag = desc->Tag;
	m_Owner = desc->Owner;
	return EResult::Success;
}

void Component::Free()
{
	m_Owner = nullptr;
	Base::Free();
}
#pragma endregion


