#pragma once

#include "Engine_Includes.h"

BEGIN(Engine)
class Resource;
class Archive;

struct ENGINE_API AssetRefBase
{
public:
	virtual ~AssetRefBase() = default;
	virtual void Serialize(Archive& ar);
protected:
	Resource* m_Ptr = nullptr;
};

template<typename T>
struct ENGINE_API AssetRef : public AssetRefBase
{
	static_assert(std::is_base_of_v<Resource, T>, "AssetRef<T> requires T to inherit from Resource!");
public:
	AssetRef() = default;
	AssetRef(T* ptr) { m_Ptr = reinterpret_cast<Resource*>(ptr); }
public:
	T* operator->() const { return static_cast<T*>(m_Ptr); }
	T& operator*() { return *static_cast<T*>(m_Ptr); }
	operator T* () const { return static_cast<T*>(m_Ptr); }

	bool operator!() const { return m_Ptr == nullptr; }
	explicit operator bool() const { return m_Ptr != nullptr; }
	bool operator==(const AssetRef<T>& other) const { return m_Ptr == other.m_Ptr; }
	bool operator!=(const AssetRef<T>& other) const { return m_Ptr != other.m_Ptr; }

	T* Get() const { return static_cast<T*>(m_Ptr); }
	T** GetAdressOf() { return reinterpret_cast<T**>(&m_Ptr); }

	AssetRef<T>& operator=(T* ptr)
	{
		m_Ptr = reinterpret_cast<class Resource*>(ptr);
		return *this;
	}
};

END