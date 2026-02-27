#pragma once

#include "Button.h"
#include "ComponentRegistry.h"

REGISTER_COMPONENT(Button)

#pragma region Constrcutor&Destructor
EResult Button::Initialize(void* arg)
{
	return EResult::Success;
}

Button* Button::Create(void* arg)
{
	Button* instance = new Button();
	if(IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* Button::Clone(GameObject* owner, void* arg)
{
	Button* clone = Create(arg);
	if (clone == nullptr)
		return nullptr;
	clone->SetOwner(owner);
	return clone;
}

void Button::Free()
{
	__super::Free();
}
#pragma endregion
