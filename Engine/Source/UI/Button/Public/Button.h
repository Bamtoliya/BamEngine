#pragma once 

#include "UIComponent.h"

BEGIN(Engine)
class ENGINE_API Button final : public UIComponent
{
	REFLECT_CLASS(Button)
#pragma region Constructor&Destructor
private:
	Button() {}
	virtual ~Button() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Button* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion
};
END