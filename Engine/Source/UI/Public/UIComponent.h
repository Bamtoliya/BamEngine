#pragma once

#include "Component.h"
#include "GameObject.h"
#include "RectTransform.h"

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
	static UIComponent* Create(void* arg = nullptr) { return nullptr; }
	static Component* Clone(GameObject* owner, void* arg = nullptr) { return nullptr; }
	virtual void Free() override { __super::Free(); }
#pragma endregion 

#pragma region RectTransform
public:
	RectTransform* GetRectTransform()
	{
		if (!m_RectTransform)
		{
			m_RectTransform = m_Owner->GetComponent<RectTransform>();
		}
		return m_RectTransform;
	}
#pragma endregion

#pragma region Member Variable
protected:
	RectTransform* m_RectTransform = { nullptr };
#pragma endregion

};
END