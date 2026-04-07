#pragma once

#include "Handle.h"

BEGIN(Engine)
class Resource;
class ResourceManager;
template<typename T>
class ResourceHandle
{	
public:
	static_assert(std::is_base_of<Resource, T>::value, "T must be derived from Resource");
	ResourceHandle() = default;
	ResourceHandle(Handle handle) : m_Handle(handle) {}

	bool operator==(const ResourceHandle& other) const { return m_Handle == other.m_Handle; }
	bool operator!=(const ResourceHandle& other) const { return m_Handle != other.m_Handle; }

	bool IsValid() const;
	operator bool() const { return IsValid(); }

	T* Get() const;
	T* operator->() const { return Get(); }
	T& operator*() const { return *Get(); }

	Handle GetRawHandle() const { return m_Handle; }
private:
	void InternalAddRef();
	void InternalRelease();

private:
	Handle m_Handle;
};

END