#pragma once

#include "UISelectable.h"

BEGIN(Engine)

CLASS()
class ENGINE_API UIButton final : public UISelectable
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
private:
	UIButton() {}
	virtual ~UIButton() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static UIButton* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion
#pragma region Interaction Override
public:
	virtual void OnPointerUp(const PointerEventData& eventData) override;
#pragma endregion
#pragma region Delegate
public:
	MulticastDelegate<>& GetOnClick() { return m_OnClick; }
#pragma endregion
#pragma region Member Variables
private:
	MulticastDelegate<> m_OnClick;
#pragma endregion
};
END