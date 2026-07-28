#pragma once

#include "UIComponent.h"
#include "UIRenderComponent.h"
#include "EventInterface.h"

BEGIN(Engine)

ENUM()
enum class ESelectionState : uint8
{
	Normal,
	Hovered,
	Pressed,
	Disabled
};

CLASS()
class ENGINE_API UISelectable : public UIComponent,
								public IPointerEnterHandler,
								public IPointerExitHandler,
								public IPointerDownHandler,
								public IPointerUpHandler
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
protected:
	UISelectable() {}
	virtual ~UISelectable() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static UISelectable* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Interaction Interface
public:
	virtual void OnPointerEnter(const PointerEventData& eventData) override;
	virtual void OnPointerExit(const PointerEventData& eventData) override;
	virtual void OnPointerDown(const PointerEventData& eventData) override;
	virtual void OnPointerUp(const PointerEventData& eventData) override;
protected:
	virtual void UpdateTransition(ESelectionState newState);
#pragma endregion

#pragma region Delegate
public:
	MulticastDelegate<>& GetOnHoverEnter() { return m_OnHoverEnter; }
	MulticastDelegate<>& GetOnHoverExit() { return m_OnHoverExit; }
	MulticastDelegate<>& GetOnPointerDown() { return m_OnPointerDown; }
	MulticastDelegate<>& GetOnPointerUp() { return m_OnPointerUp; }
#pragma endregion

#pragma region Getter & Setter
public:
	ESelectionState GetCurrentState() const { return m_CurrentState; }
	bool IsInteractable() const { return m_Interactable; }
	void SetInteractable(bool interactable);
#pragma endregion


#pragma region Member Variables
protected:
	PROPERTY(EDITABLE)
	bool m_Interactable = { true };
	PROPERTY(EDITABLE, COLOR())
	vec4 m_NormalColor = { 1.f, 1.f, 1.f, 1.f };
	PROPERTY(EDITABLE, COLOR())
	vec4 m_HoveredColor = { 0.9f, 0.9f, 0.9f, 1.f };
	PROPERTY(EDITABLE, COLOR())
	vec4 m_PressedColor = { 0.7f, 0.7f, 0.7f, 1.f };
	PROPERTY(EDITABLE, COLOR())
	vec4 m_DisabledColor = { 0.5f, 0.5f, 0.5f, 0.5f };

	ESelectionState m_CurrentState = { ESelectionState::Normal };
	UIRenderComponent* m_TargetGraphic = { nullptr };

	MulticastDelegate<> m_OnHoverEnter;
	MulticastDelegate<> m_OnHoverExit;
	MulticastDelegate<> m_OnPointerDown;
	MulticastDelegate<> m_OnPointerUp;
#pragma endregion

};
END