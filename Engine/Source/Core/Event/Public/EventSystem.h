#pragma once

#include "Component.h"
#include "DynamicEvent.h"
#include "EventInterface.h"

BEGIN(Engine)
class BaseRaycaster;

CLASS()
class ENGINE_API EventSystem final : public Component
{
	REFLECT_CLASS()

#pragma region Constructor&Destructor
protected:
	EventSystem() {}
	public: virtual ~EventSystem() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static EventSystem* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion
public:
	static EventSystem* GetCurrent() { return s_Current; }

#pragma region Loop
public:
	virtual void FixedUpdate(f32 dt) override;
	virtual void Update(f32 dt) override;
	virtual void LateUpdate(f32 dt) override;
#pragma endregion

#pragma region Raycaster Management
public:
	void AddRaycaster(BaseRaycaster* raycaster);
	void RemoveRaycaster(BaseRaycaster* raycaster);
#pragma endregion



#pragma region Member Variables
protected:
	PROPERTY()
	vector<BaseRaycaster*> m_Raycasters;

	GameObject* m_CurrentHover = nullptr;
	GameObject* m_CurrentPress = nullptr;

	PointerEventData m_CurrentEventData;

	static EventSystem* s_Current;
#pragma endregion
};
END