#pragma once

#include "Handle.h"
#include <type_traits>

BEGIN(Engine)
class Resource;
class ResourceManager;

template<typename T>
class ResourceHandle
{
public:
	ResourceHandle() = default;
	explicit ResourceHandle(Handle handle);

	ResourceHandle(const ResourceHandle& other);
	ResourceHandle(ResourceHandle&& other) noexcept;
	~ResourceHandle();

	ResourceHandle& operator=(const ResourceHandle& other);
	ResourceHandle& operator=(ResourceHandle&& other) noexcept;

public:
	bool operator==(const ResourceHandle& other) const { return m_Handle == other.m_Handle; }
	bool operator!=(const ResourceHandle& other) const { return m_Handle != other.m_Handle; }

	bool IsValid() const;
	operator bool() const { return IsValid(); }

	T* Get() const
	{
		static_assert(std::is_base_of_v<Resource, T>, "T must be derived from Resource");
		return static_cast<T*>(GetUntyped());
	}

	T* operator->() const { return Get(); }
	T& operator*() const { return *Get(); }

	Handle GetRawHandle() const { return m_Handle; }

	void Reset();

private:
	Resource* GetUntyped() const;

	void InternalAddRef();
	void InternalRelease();

private:
	Handle m_Handle;
};
END