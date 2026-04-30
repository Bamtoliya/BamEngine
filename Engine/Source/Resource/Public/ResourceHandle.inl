#pragma once
#include "ResourceManager.h"

template<typename T>
inline ResourceHandle<T>::ResourceHandle(Handle handle)
	: m_Handle(handle)
{
	InternalAddRef();
}

template<typename T>
inline ResourceHandle<T>::ResourceHandle(const ResourceHandle& other)
	: m_Handle(other.m_Handle)
{
	InternalAddRef();
}

template<typename T>
inline ResourceHandle<T>::ResourceHandle(ResourceHandle&& other) noexcept
	: m_Handle(other.m_Handle)
{
	other.m_Handle = Handle();
}

template<typename T>
template <typename U, typename std::enable_if_t<std::is_convertible_v<U*, T*>, int>>
inline ResourceHandle<T>::ResourceHandle(const ResourceHandle<U>& other)
	: m_Handle(other.m_Handle)
{
	InternalAddRef();
}

template<typename T>
template <typename U, typename std::enable_if_t<std::is_convertible_v<U*, T*>, int>>
inline ResourceHandle<T>::ResourceHandle(ResourceHandle<U>&& other) noexcept
	: m_Handle(other.m_Handle)
{
	other.m_Handle = Handle();
}

template<typename T>
inline ResourceHandle<T>::~ResourceHandle()
{
	InternalRelease();
}

template<typename T>
inline ResourceHandle<T>& ResourceHandle<T>::operator=(const ResourceHandle& other)
{
	if (this == &other)
		return *this;

	InternalRelease();
	m_Handle = other.m_Handle;
	InternalAddRef();
	return *this;
}

template<typename T>
inline ResourceHandle<T>& ResourceHandle<T>::operator=(ResourceHandle&& other) noexcept
{
	if (this == &other)
		return *this;

	InternalRelease();
	m_Handle = other.m_Handle;
	other.m_Handle = Handle();
	return *this;
}

template<typename T>
template <typename U>
inline ResourceHandle<T>& ResourceHandle<T>::operator=(const ResourceHandle<U>& other)
{
	static_assert(std::is_convertible_v<U*, T*>, "U must be derived from T");
	if (m_Handle == other.m_Handle)
		return *this;
	InternalRelease();
	m_Handle = other.m_Handle;
	InternalAddRef();
	return *this;
}

template<typename T>
template <typename U>
inline ResourceHandle<T>& ResourceHandle<T>::operator=(ResourceHandle<U>&& other) noexcept
{
	static_assert(std::is_convertible_v<U*, T*>, "U must be derived from T");
	if (m_Handle == other.m_Handle)
		return *this;
	InternalRelease();
	m_Handle = other.m_Handle;
	other.m_Handle = Handle();
	return *this;
}

template<typename T>
inline void ResourceHandle<T>::Reset()
{
	InternalRelease();
}

template<typename T>
inline void ResourceHandle<T>::InternalAddRef()
{
	if (m_Handle.IsValid())
	{
		ResourceManager::Get().AddRefResource(m_Handle);
	}
}

template<typename T>
inline void ResourceHandle<T>::InternalRelease()
{
	if (m_Handle.IsValid())
	{
		ResourceManager::Get().ReleaseResource(m_Handle);
		m_Handle = Handle();
	}
}

template<typename T>
inline bool ResourceHandle<T>::IsValid() const
{
	if (!m_Handle.IsValid())
		return false;

	return ResourceManager::Get().IsValid(m_Handle);
}

template<typename T>
inline Resource* ResourceHandle<T>::GetUntyped() const
{
	if (!m_Handle.IsValid())
		return nullptr;

	return ResourceManager::Get().GetResource(m_Handle);
}