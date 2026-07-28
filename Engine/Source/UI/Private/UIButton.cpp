#pragma once

#include "UIButton.h"
#include "ComponentRegistry.h"
#include "GameObject.h"

REGISTER_COMPONENT(UIButton)
#pragma region Constructor & Destructor
EResult UIButton::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;
	return EResult::Success;
}
UIButton* UIButton::Create(void* arg)
{
	UIButton* instance = new UIButton();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
Component* UIButton::Clone(GameObject* owner, void* arg)
{
	UIButton* clone = Create(arg);
	if (clone == nullptr)
		return nullptr;
	clone->SetOwner(owner);
	return clone;
}
void UIButton::Free()
{
	m_OnClick.Clear();
	__super::Free();
}
#pragma endregion

#pragma region Interaction Override
void UIButton::OnPointerUp(const PointerEventData& eventData)
{
	if (!IsInteractable())
		return;

	__super::OnPointerUp(eventData);

	m_OnClick.Broadcast();
}
#pragma endregion