#pragma once


#include "Base.h"

BEGIN(Engine)
class ENGINE_API PrototypeManager : public Base
{
	DECLARE_SINGLETON(PrototypeManager)
#pragma region Constructor&Destructor
private:
	PrototypeManager() {}
	virtual ~PrototypeManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion
#pragma region Prototype Management
public:
	EResult RegisterGameObjectPrototype(const wstring& prototypeTag, class GameObject* prototype);
	class GameObject* CreateGameObjectPrototype(const wstring& prototypeTag, void* arg = nullptr);
public:
	EResult RegisterComponentPrototype(const wstring& prototypeTag, class Component* prototype);
	class Component* CreateComponentPrototype(const wstring& prototypeTag, class GameObject* owner, void* arg = nullptr);
#pragma endregion
#pragma region Variable
private:
	unordered_map<wstring, class Component*> m_ComponentPrototypeMap;
	unordered_map<wstring, class GameObject*> m_GameObjectPrototypeMap;
#pragma endregion
};
END