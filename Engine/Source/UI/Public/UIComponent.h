#pragma once

#include "Component.h"

BEGIN(Engine)

CLASS()
class ENGINE_API UIComponent : public Component	
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
protected:
	UIComponent() {}
	virtual ~UIComponent() {}
	EResult Initialize(void* arg = nullptr) override { return EResult::NotImplemented; }
public:
	virtual void Free() override { __super::Free(); }
#pragma endregion 

#pragma region Member Variable
protected:

#pragma endregion

};
END