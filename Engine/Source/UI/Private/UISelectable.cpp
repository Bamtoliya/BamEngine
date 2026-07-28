#pragma once

#include "UISelectable.h"
#include "ComponentRegistry.h"
#include "GameObject.h"

REGISTER_COMPONENT(UISelectable)

EResult UISelectable::Initialize(void* arg)
{
	__super::Initialize(arg);
	m_CurrentState = m_Interactable ? ESelectionState::Normal : ESelectionState::Disabled;
	return EResult::Success;
}

UISelectable* UISelectable::Create(void* arg)
{
	return nullptr;
}

Component* UISelectable::Clone(GameObject* owner, void* arg)
{
	return nullptr;
}

void UISelectable::Free()
{
	m_OnHoverEnter.Clear();
	m_OnHoverExit.Clear();
	m_OnPointerDown.Clear();
	m_OnPointerUp.Clear();
	m_TargetGraphic = nullptr;
	__super::Free();
}

void UISelectable::OnPointerEnter(const PointerEventData& eventData)
{
	if (!m_Interactable) return;
	UpdateTransition(ESelectionState::Hovered);
	m_OnHoverEnter.Broadcast();
}

void UISelectable::OnPointerExit(const PointerEventData& eventData)
{
	if (!m_Interactable) return;
	UpdateTransition(ESelectionState::Normal);
	m_OnHoverExit.Broadcast();
}

void UISelectable::OnPointerDown(const PointerEventData& eventData)
{
	if (!m_Interactable) return;
	UpdateTransition(ESelectionState::Pressed);
	m_OnPointerDown.Broadcast();
}

void UISelectable::OnPointerUp(const PointerEventData& eventData)
{
	if (!m_Interactable) return;
	UpdateTransition(ESelectionState::Hovered);
	m_OnPointerUp.Broadcast();
}

void UISelectable::UpdateTransition(ESelectionState newState)
{
	m_CurrentState = newState;
	if (!m_TargetGraphic && m_Owner)
	{
		m_TargetGraphic = m_Owner->GetComponent<UIRenderComponent>();
	}
	if (m_TargetGraphic)
	{
		vec4 targetColor = m_NormalColor;
		switch (m_CurrentState)
		{
		case ESelectionState::Hovered:  targetColor = m_HoveredColor;  break;
		case ESelectionState::Pressed:  targetColor = m_PressedColor;  break;
		case ESelectionState::Disabled: targetColor = m_DisabledColor; break;
		default: break;
		}
		m_TargetGraphic->SetColor(targetColor);
	}
}

void UISelectable::SetInteractable(bool interactable)
{
	if (m_Interactable == interactable) return;
	m_Interactable = interactable;
	UpdateTransition(m_Interactable ? ESelectionState::Normal : ESelectionState::Disabled);
}
