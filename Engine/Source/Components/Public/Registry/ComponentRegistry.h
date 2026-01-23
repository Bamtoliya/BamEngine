#pragma once

#include "Component.h"

BEGIN(Engine)

class ENGINE_API ComponentRegistry : public Base
{
	DECLARE_SINGLETON(ComponentRegistry)

	using CreateFunc = function<Component* (void*)>;
	using ComponentDesc = tagComponentDesc;
#pragma region Constructor&Destructor
private:
	ComponentRegistry() {}
	virtual ~ComponentRegistry() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Component Management
public:
	template<typename T>
	EResult Register(const std::wstring& name)
	{
		type_index typeIndex = type_index(typeid(T));

		if (m_TypeMap.find(typeIndex) != m_TypeMap.end())
			return EResult::AlreadyInitialized;

		CreateFunc creator = [](void* arg = nullptr) -> Component*
		{
			return T::Create(arg);
		};

		m_TypeMap[typeIndex] = creator;
		m_NameMap[name] = creator;

		return EResult::Success;
	}

	template<typename T>
	T* Create(void* arg = nullptr)
	{
		type_index typeIndex = type_index(typeid(T));
		auto iter = m_TypeMap.find(typeIndex);

		if (iter != m_TypeMap.end())
		{
			return static_cast<T*>(iter->second(arg));
		}
		return nullptr;
	}

	Component* Create(const wstring& name, void* arg = nullptr);

#pragma endregion

#pragma region Variable
private:
	unordered_map<type_index, CreateFunc>	m_TypeMap;
	unordered_map<wstring, CreateFunc>		m_NameMap;

#pragma endregion

};

#pragma region Helper

template<typename T>
struct ComponentRegisterHelper
{
	ComponentRegisterHelper(const wstring& name)
	{
		ComponentRegistry::Get().Register<T>(name);
	}
};

#pragma endregion	

END